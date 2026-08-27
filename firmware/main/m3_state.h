#pragma once

#include <cstdint>

namespace self_involving {

enum class DeviceState : std::uint8_t {
    kBooting,
    kNeedsConfiguration,
    kConnecting,
    kIdleLocal,
    kListening,
    kThinking,
    kSpeaking,
    kMuted,
    kOffline,
    kError,
};

const char* DeviceStateName(DeviceState state);

}  // namespace self_involving
