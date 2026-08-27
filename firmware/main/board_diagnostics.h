#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"

namespace self_involving {

struct DiagnosticResult {
    bool flash_ok = false;
    bool psram_ok = false;
    bool partitions_ok = false;
    bool nvs_ok = false;
    bool es8311_found = false;
    uint32_t boot_count = 0;
    std::size_t flash_bytes = 0;
    std::size_t psram_bytes = 0;
};

esp_err_t ConfigureSafeGpios();
DiagnosticResult RunBoardDiagnostics();
void LogMemoryWatermarks();

}  // namespace self_involving
