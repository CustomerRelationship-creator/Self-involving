#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace self_involving {

using MicrophoneFrameCallback = void (*)(const std::uint8_t*, std::size_t, void*);

class AudioEngine {
public:
    esp_err_t Initialize();
    esp_err_t Start(MicrophoneFrameCallback callback, void* context);
    void SetSessionActive(bool active) { session_active_.store(active); }
    void SetMuted(bool muted);
    esp_err_t QueueSpeakerAudio(const std::uint8_t* data, std::size_t size);
    std::uint32_t dropped_playback_frames() const { return dropped_playback_; }

private:
    static void CaptureTaskThunk(void* context);
    static void PlaybackTaskThunk(void* context);
    void CaptureTask();
    void PlaybackTask();
    void StoreInRing(const std::uint8_t* data, std::size_t size);

    void* codec_ = nullptr;
    QueueHandle_t playback_queue_ = nullptr;
    std::uint8_t* ring_ = nullptr;
    std::size_t ring_write_ = 0;
    MicrophoneFrameCallback callback_ = nullptr;
    void* callback_context_ = nullptr;
    std::atomic<bool> session_active_{false};
    std::atomic<bool> muted_{false};
    std::uint32_t dropped_playback_ = 0;
};

}  // namespace self_involving
