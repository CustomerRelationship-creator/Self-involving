#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"

namespace self_involving {

class DisplayTest {
public:
    esp_err_t Initialize();
    esp_err_t ShowDiagnosticPattern(bool overall_ok);
    esp_err_t ShowMuted();
    esp_err_t CyclePattern();

private:
    esp_err_t Fill(uint16_t rgb565);
    esp_err_t ShowBands(const uint16_t* colors, std::size_t color_count);

    void* panel_ = nullptr;
    uint16_t* frame_buffer_ = nullptr;
    uint8_t pattern_index_ = 0;
};

}  // namespace self_involving
