#pragma once

#include <cstddef>
#include <cstdint>

namespace self_involving {

constexpr std::uint32_t kVoiceMagic = 0x53495633;  // "SIV3"
constexpr std::size_t kPcmFrameBytes = 960;       // 20 ms, 24 kHz, s16 mono

enum class AudioKind : std::uint8_t { kMicrophone = 1, kSpeaker = 2 };

#pragma pack(push, 1)
struct AudioHeader {
    std::uint32_t magic;
    std::uint8_t version;
    AudioKind kind;
    std::uint16_t flags;
    std::uint32_t sequence;
    std::uint32_t sample_rate;
    std::uint16_t payload_bytes;
    std::uint16_t reserved;
};
#pragma pack(pop)

static_assert(sizeof(AudioHeader) == 20);

}  // namespace self_involving
