#include <atomic>
#include <cinttypes>
#include <cstdint>

#include "audio_engine.h"
#include "board_diagnostics.h"
#include "board_pins.h"
#include "device_config.h"
#include "display_test.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "m3_state.h"
#include "nvs_flash.h"
#include "voice_transport.h"
#include "wifi_manager.h"

namespace {
constexpr char kTag[] = "self_involving_m3";
constexpr TickType_t kPoll = pdMS_TO_TICKS(20);
constexpr TickType_t kLongPress = pdMS_TO_TICKS(2000);
constexpr TickType_t kHealthPeriod = pdMS_TO_TICKS(10000);
std::atomic<self_involving::DeviceState> g_state{
    self_involving::DeviceState::kBooting};
self_involving::AudioEngine* g_audio = nullptr;
self_involving::VoiceTransport* g_transport = nullptr;

void MicrophoneFrame(const std::uint8_t* data, std::size_t size, void*) {
    if (g_transport != nullptr) g_transport->QueueMicrophoneFrame(data, size);
}
void SpeakerFrame(const std::uint8_t* data, std::size_t size, void*) {
    if (g_audio != nullptr) g_audio->QueueSpeakerAudio(data, size);
}
void RemoteState(self_involving::DeviceState state, void*) {
    g_state.store(state);
    if (state == self_involving::DeviceState::kIdleLocal && g_audio != nullptr) {
        g_audio->SetSessionActive(false);
    }
}
}  // namespace

extern "C" void app_main(void) {
    using self_involving::DeviceState;
    ESP_LOGI(kTag, "Self-involving M3 voice body starting");
    ESP_ERROR_CHECK(self_involving::ConfigureSafeGpios());
    const esp_err_t nvs_error = nvs_flash_init();
    if (nvs_error != ESP_OK) {
        ESP_LOGE(kTag, "NVS initialization failed without erase: %s",
                 esp_err_to_name(nvs_error));
    }
    const auto diagnostics = self_involving::RunBoardDiagnostics();
    const bool board_ok = diagnostics.flash_ok && diagnostics.psram_ok &&
                          diagnostics.partitions_ok && diagnostics.nvs_ok &&
                          diagnostics.es8311_found;
    self_involving::DisplayTest display;
    const esp_err_t display_error = display.Initialize();
    if (display_error == ESP_OK) display.ShowState(DeviceState::kBooting);

    self_involving::AudioEngine audio;
    g_audio = &audio;
    static self_involving::VoiceTransport transport;
    if (!board_ok || audio.Initialize() != ESP_OK ||
        audio.Start(MicrophoneFrame, nullptr) != ESP_OK) {
        g_state.store(DeviceState::kError);
    } else {
        const self_involving::DeviceConfig config =
            self_involving::LoadDeviceConfig();
        if (!config.complete) {
            ESP_LOGW(kTag, "M3 configuration missing; use menuconfig or NVS provisioning");
            g_state.store(DeviceState::kNeedsConfiguration);
        } else {
            g_state.store(DeviceState::kConnecting);
            self_involving::WifiManager wifi;
            if (wifi.Connect(config, 20000) != ESP_OK) {
                g_state.store(DeviceState::kOffline);
            } else {
                g_transport = &transport;
                if (transport.Initialize(config, SpeakerFrame, RemoteState,
                                         nullptr) != ESP_OK ||
                    transport.Connect(15000) != ESP_OK) {
                    g_state.store(DeviceState::kOffline);
                } else {
                    g_state.store(DeviceState::kIdleLocal);
                }
            }
        }
    }

    bool muted = false;
    bool was_pressed = false;
    TickType_t pressed_at = 0;
    TickType_t last_health = xTaskGetTickCount();
    DeviceState rendered = DeviceState::kBooting;
    while (true) {
        const TickType_t now = xTaskGetTickCount();
        const bool pressed =
            gpio_get_level(self_involving::board::kBootButton) == 0;
        if (pressed && !was_pressed) pressed_at = now;
        else if (!pressed && was_pressed) {
            const TickType_t duration = now - pressed_at;
            if (duration >= kLongPress) {
                muted = !muted;
                if (g_transport != nullptr) g_transport->CancelSession();
                audio.SetMuted(muted);
                g_state.store(muted ? DeviceState::kMuted
                                    : (g_transport != nullptr && g_transport->connected()
                                           ? DeviceState::kIdleLocal
                                           : DeviceState::kOffline));
            } else if (!muted && g_transport != nullptr &&
                       g_transport->connected()) {
                const DeviceState current = g_state.load();
                if (current == DeviceState::kListening ||
                    current == DeviceState::kThinking ||
                    current == DeviceState::kSpeaking) {
                    g_transport->CancelSession();
                    audio.SetSessionActive(false);
                    g_state.store(DeviceState::kIdleLocal);
                } else if (g_transport->StartSession() == ESP_OK) {
                    audio.SetSessionActive(true);
                    g_state.store(DeviceState::kListening);
                }
            }
        }
        was_pressed = pressed;
        const DeviceState state = g_state.load();
        if (state != rendered && display_error == ESP_OK) {
            display.ShowState(state);
            rendered = state;
            ESP_LOGI(kTag, "state=%s", self_involving::DeviceStateName(state));
        }
        gpio_set_level(self_involving::board::kStatusLed,
                       state == DeviceState::kListening ||
                       state == DeviceState::kSpeaking ||
                       state == DeviceState::kMuted);
        if (now - last_health >= kHealthPeriod) {
            self_involving::LogMemoryWatermarks();
            ESP_LOGI(kTag, "state=%s playback_drops=%" PRIu32,
                     self_involving::DeviceStateName(state),
                     audio.dropped_playback_frames());
            last_health = now;
        }
        vTaskDelay(kPoll);
    }
}
