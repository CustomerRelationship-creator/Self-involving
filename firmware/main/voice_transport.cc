#include "voice_transport.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "freertos/event_groups.h"
#include "voice_protocol.h"

namespace self_involving {
namespace {
constexpr char kTag[] = "m3_transport";
constexpr EventBits_t kConnectedBit = BIT0;
constexpr std::size_t kQueueDepth = 8;

struct OutgoingFrame {
    std::uint16_t size;
    std::uint8_t data[kPcmFrameBytes];
};
EventGroupHandle_t s_connect_events = nullptr;
}  // namespace

esp_err_t VoiceTransport::Initialize(const DeviceConfig& config,
                                     SpeakerFrameCallback speaker_callback,
                                     RemoteStateCallback state_callback,
                                     void* context) {
    speaker_callback_ = speaker_callback;
    state_callback_ = state_callback;
    callback_context_ = context;
    microphone_queue_ = xQueueCreate(kQueueDepth, sizeof(OutgoingFrame));
    s_connect_events = xEventGroupCreate();
    if (microphone_queue_ == nullptr || s_connect_events == nullptr) {
        return ESP_ERR_NO_MEM;
    }
    static char authorization[192];
    std::snprintf(authorization, sizeof(authorization), "Authorization: Bearer %s\r\n",
                  config.device_token);
    esp_websocket_client_config_t websocket = {};
    websocket.uri = config.gateway_uri;
    websocket.headers = authorization;
    websocket.crt_bundle_attach = esp_crt_bundle_attach;
    websocket.disable_auto_reconnect = false;
    websocket.reconnect_timeout_ms = 3000;
    websocket.network_timeout_ms = 10000;
    websocket.ping_interval_sec = 15;
    websocket.buffer_size = 2048;
    client_ = esp_websocket_client_init(&websocket);
    if (client_ == nullptr) return ESP_ERR_NO_MEM;
    esp_websocket_register_events(
        static_cast<esp_websocket_client_handle_t>(client_), WEBSOCKET_EVENT_ANY,
        WebSocketEvent, this);
    if (xTaskCreatePinnedToCore(SendTaskThunk, "voice_send", 4096, this, 6,
                                nullptr, 1) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t VoiceTransport::Connect(int timeout_ms) {
    const esp_err_t err = esp_websocket_client_start(
        static_cast<esp_websocket_client_handle_t>(client_));
    if (err != ESP_OK) return err;
    const EventBits_t bits = xEventGroupWaitBits(
        s_connect_events, kConnectedBit, pdFALSE, pdTRUE,
        pdMS_TO_TICKS(timeout_ms));
    return (bits & kConnectedBit) ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t VoiceTransport::StartSession() {
    if (!connected_.load()) return ESP_ERR_INVALID_STATE;
    sequence_ = 0;
    session_active_.store(true);
    return SendText("{\"type\":\"session.start\",\"protocol\":3,"
                    "\"audio\":{\"encoding\":\"pcm_s16le\","
                    "\"sample_rate\":24000,\"channels\":1,"
                    "\"frame_ms\":20}}");
}

esp_err_t VoiceTransport::CancelSession() {
    session_active_.store(false);
    xQueueReset(microphone_queue_);
    return connected_.load()
               ? SendText("{\"type\":\"session.cancel\"}")
               : ESP_OK;
}

esp_err_t VoiceTransport::QueueMicrophoneFrame(const std::uint8_t* data,
                                               std::size_t size) {
    if (!session_active_.load() || data == nullptr || size > kPcmFrameBytes) {
        return ESP_ERR_INVALID_STATE;
    }
    OutgoingFrame frame = {};
    frame.size = static_cast<std::uint16_t>(size);
    std::memcpy(frame.data, data, size);
    return xQueueSend(microphone_queue_, &frame, 0) == pdPASS
               ? ESP_OK : ESP_ERR_TIMEOUT;
}

void VoiceTransport::WebSocketEvent(void* handler_args, esp_event_base_t,
                                    int32_t event_id, void* event_data) {
    static_cast<VoiceTransport*>(handler_args)->HandleEvent(event_id, event_data);
}

void VoiceTransport::HandleEvent(int32_t event_id, void* event_data) {
    auto* event = static_cast<esp_websocket_event_data_t*>(event_data);
    if (event_id == WEBSOCKET_EVENT_CONNECTED) {
        connected_.store(true);
        xEventGroupSetBits(s_connect_events, kConnectedBit);
        SendText("{\"type\":\"hello\",\"protocol\":3,"
                 "\"device\":\"movecall-moji-esp32s3-enterprise\","
                 "\"firmware\":\"self-involving-m3\"}");
    } else if (event_id == WEBSOCKET_EVENT_DISCONNECTED) {
        connected_.store(false);
        session_active_.store(false);
        xEventGroupClearBits(s_connect_events, kConnectedBit);
        if (state_callback_ != nullptr) {
            state_callback_(DeviceState::kOffline, callback_context_);
        }
    } else if (event_id == WEBSOCKET_EVENT_DATA && event != nullptr) {
        if (event->op_code == 0x2 && event->data_len >=
                                           static_cast<int>(sizeof(AudioHeader))) {
            AudioHeader header = {};
            std::memcpy(&header, event->data_ptr, sizeof(header));
            if (header.magic == kVoiceMagic && header.version == 3 &&
                header.kind == AudioKind::kSpeaker &&
                header.payload_bytes <= kPcmFrameBytes &&
                event->data_len == static_cast<int>(sizeof(header) +
                                                    header.payload_bytes) &&
                speaker_callback_ != nullptr) {
                speaker_callback_(
                    reinterpret_cast<const std::uint8_t*>(event->data_ptr) +
                        sizeof(header),
                    header.payload_bytes, callback_context_);
                if (state_callback_ != nullptr) {
                    state_callback_(DeviceState::kSpeaking, callback_context_);
                }
            }
        } else if (event->op_code == 0x1 && state_callback_ != nullptr) {
            char message[256] = {};
            const std::size_t copy_size = std::min(
                static_cast<std::size_t>(event->data_len), sizeof(message) - 1);
            std::memcpy(message, event->data_ptr, copy_size);
            if (std::strstr(message, "response.thinking") != nullptr) {
                state_callback_(DeviceState::kThinking, callback_context_);
            } else if (std::strstr(message, "session.ended") != nullptr) {
                session_active_.store(false);
                state_callback_(DeviceState::kIdleLocal, callback_context_);
            }
        }
    }
}

void VoiceTransport::SendTaskThunk(void* context) {
    static_cast<VoiceTransport*>(context)->SendTask();
}

void VoiceTransport::SendTask() {
    OutgoingFrame frame = {};
    std::uint8_t packet[sizeof(AudioHeader) + kPcmFrameBytes] = {};
    while (true) {
        if (xQueueReceive(microphone_queue_, &frame, portMAX_DELAY) != pdPASS ||
            !connected_.load() || !session_active_.load()) continue;
        AudioHeader header = {kVoiceMagic, 3, AudioKind::kMicrophone, 0,
                              sequence_++, 24000, frame.size, 0};
        std::memcpy(packet, &header, sizeof(header));
        std::memcpy(packet + sizeof(header), frame.data, frame.size);
        const int sent = esp_websocket_client_send_bin(
            static_cast<esp_websocket_client_handle_t>(client_),
            reinterpret_cast<const char*>(packet), sizeof(header) + frame.size,
            pdMS_TO_TICKS(100));
        if (sent < 0) ESP_LOGW(kTag, "microphone frame send failed");
    }
}

esp_err_t VoiceTransport::SendText(const char* text) {
    const int sent = esp_websocket_client_send_text(
        static_cast<esp_websocket_client_handle_t>(client_), text,
        std::strlen(text), pdMS_TO_TICKS(1000));
    return sent >= 0 ? ESP_OK : ESP_FAIL;
}

}  // namespace self_involving
