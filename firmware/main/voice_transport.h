#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "device_config.h"
#include "esp_err.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "m3_state.h"

namespace self_involving {

using SpeakerFrameCallback = void (*)(const std::uint8_t*, std::size_t, void*);
using RemoteStateCallback = void (*)(DeviceState, void*);

class VoiceTransport {
public:
    esp_err_t Initialize(const DeviceConfig& config,
                         SpeakerFrameCallback speaker_callback,
                         RemoteStateCallback state_callback, void* context);
    esp_err_t Connect(int timeout_ms);
    esp_err_t StartSession();
    esp_err_t CancelSession();
    esp_err_t QueueMicrophoneFrame(const std::uint8_t* data, std::size_t size);
    bool connected() const { return connected_.load(); }

private:
    static void WebSocketEvent(void* handler_args, esp_event_base_t base,
                               int32_t event_id, void* event_data);
    static void SendTaskThunk(void* context);
    void HandleEvent(int32_t event_id, void* event_data);
    void SendTask();
    esp_err_t SendText(const char* text);

    void* client_ = nullptr;
    QueueHandle_t microphone_queue_ = nullptr;
    SpeakerFrameCallback speaker_callback_ = nullptr;
    RemoteStateCallback state_callback_ = nullptr;
    void* callback_context_ = nullptr;
    std::atomic<bool> connected_{false};
    std::atomic<bool> session_active_{false};
    std::uint32_t sequence_ = 0;
};

}  // namespace self_involving
