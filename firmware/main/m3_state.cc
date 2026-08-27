#include "m3_state.h"

namespace self_involving {

const char* DeviceStateName(DeviceState state) {
    switch (state) {
        case DeviceState::kBooting: return "BOOTING";
        case DeviceState::kNeedsConfiguration: return "NEEDS_CONFIGURATION";
        case DeviceState::kConnecting: return "CONNECTING";
        case DeviceState::kIdleLocal: return "IDLE_LOCAL";
        case DeviceState::kListening: return "LISTENING";
        case DeviceState::kThinking: return "THINKING";
        case DeviceState::kSpeaking: return "SPEAKING";
        case DeviceState::kMuted: return "MUTED";
        case DeviceState::kOffline: return "OFFLINE";
        case DeviceState::kError: return "ERROR";
    }
    return "UNKNOWN";
}

}  // namespace self_involving
