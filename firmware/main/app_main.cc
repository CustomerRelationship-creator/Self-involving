#include <cstdint>

#include "board_diagnostics.h"
#include "board_pins.h"
#include "display_test.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

namespace {

constexpr char kTag[] = "self_involving";
constexpr TickType_t kPollInterval = pdMS_TO_TICKS(20);
constexpr TickType_t kLongPressDuration = pdMS_TO_TICKS(2000);
constexpr TickType_t kHealthLogPeriod = pdMS_TO_TICKS(10000);

bool AllCriticalChecksPassed(const self_involving::DiagnosticResult& result) {
    return result.flash_ok && result.psram_ok && result.partitions_ok &&
           result.nvs_ok && result.es8311_found;
}

}  // namespace

extern "C" void app_main(void) {
    ESP_LOGI(kTag, "Self-involving M0 hardware self-test starting");

    ESP_ERROR_CHECK(self_involving::ConfigureSafeGpios());

    const esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err != ESP_OK) {
        // Never erase NVS automatically in a diagnostic firmware.
        ESP_LOGE(kTag, "NVS initialization failed without erase: %s",
                 esp_err_to_name(nvs_err));
    }

    const self_involving::DiagnosticResult diagnostics =
        self_involving::RunBoardDiagnostics();
    const bool overall_ok = AllCriticalChecksPassed(diagnostics);

    self_involving::DisplayTest display;
    const esp_err_t display_err = display.Initialize();
    if (display_err == ESP_OK) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(display.ShowDiagnosticPattern(overall_ok));
    } else {
        ESP_LOGE(kTag, "Display test unavailable: %s",
                 esp_err_to_name(display_err));
    }

    const bool final_ok = overall_ok && display_err == ESP_OK;
    gpio_set_level(self_involving::board::kStatusLed, final_ok ? 1 : 0);
    ESP_LOGI(kTag,
             "M0 result flash=%d psram=%d partitions=%d nvs=%d es8311=%d display=%d",
             diagnostics.flash_ok, diagnostics.psram_ok,
             diagnostics.partitions_ok, diagnostics.nvs_ok,
             diagnostics.es8311_found, display_err == ESP_OK);
    ESP_LOGW(kTag, "Power amplifier remains disabled; no audio is played in M0");

    bool muted = false;
    bool was_pressed = false;
    TickType_t pressed_at = 0;
    TickType_t last_health_log = xTaskGetTickCount();

    while (true) {
        const TickType_t now = xTaskGetTickCount();
        const bool pressed =
            gpio_get_level(self_involving::board::kBootButton) == 0;

        if (pressed && !was_pressed) {
            pressed_at = now;
        } else if (!pressed && was_pressed) {
            const TickType_t duration = now - pressed_at;
            if (duration >= kLongPressDuration) {
                muted = !muted;
                gpio_set_level(self_involving::board::kPowerAmplifierEnable, 0);
                gpio_set_level(self_involving::board::kStatusLed,
                               muted ? 1 : final_ok);
                if (display_err == ESP_OK) {
                    ESP_ERROR_CHECK_WITHOUT_ABORT(
                        muted ? display.ShowMuted()
                              : display.ShowDiagnosticPattern(overall_ok));
                }
                ESP_LOGI(kTag, "Software mute state=%s", muted ? "on" : "off");
            } else if (!muted && display_err == ESP_OK) {
                ESP_ERROR_CHECK_WITHOUT_ABORT(display.CyclePattern());
                ESP_LOGI(kTag, "Display pattern advanced");
            }
        }
        was_pressed = pressed;

        if (now - last_health_log >= kHealthLogPeriod) {
            self_involving::LogMemoryWatermarks();
            last_health_log = now;
        }
        vTaskDelay(kPollInterval);
    }
}
