#include "board_diagnostics.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>

#include "board_pins.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_app_desc.h"
#include "esp_check.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_psram.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace self_involving {
namespace {

constexpr char kTag[] = "m0_diag";
constexpr std::size_t kPsramTestBytes = 512 * 1024;
constexpr uint8_t kEs8311Addresses[] = {0x18, 0x19};

bool CheckPartition(const char* label, esp_partition_type_t type,
                    esp_partition_subtype_t subtype,
                    std::size_t minimum_size) {
    const esp_partition_t* partition =
        esp_partition_find_first(type, subtype, label);
    if (partition == nullptr) {
        ESP_LOGE(kTag, "Missing partition: %s", label);
        return false;
    }
    ESP_LOGI(kTag, "Partition %-8s offset=0x%08" PRIx32 " size=%" PRIu32,
             label, partition->address, partition->size);
    if (partition->size < minimum_size) {
        ESP_LOGE(kTag, "Partition %s is smaller than the resource budget", label);
        return false;
    }
    return true;
}

bool TestPsram() {
    auto* words = static_cast<uint32_t*>(
        heap_caps_malloc(kPsramTestBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (words == nullptr) {
        ESP_LOGE(kTag, "Unable to allocate %u bytes in PSRAM",
                 static_cast<unsigned>(kPsramTestBytes));
        return false;
    }

    const std::size_t word_count = kPsramTestBytes / sizeof(uint32_t);
    uint32_t state = 0x51F15EED;
    for (std::size_t i = 0; i < word_count; ++i) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        words[i] = state ^ static_cast<uint32_t>(i);
    }

    state = 0x51F15EED;
    bool ok = true;
    for (std::size_t i = 0; i < word_count; ++i) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        if (words[i] != (state ^ static_cast<uint32_t>(i))) {
            ESP_LOGE(kTag, "PSRAM mismatch at word %u", static_cast<unsigned>(i));
            ok = false;
            break;
        }
    }

    std::memset(words, 0, kPsramTestBytes);
    heap_caps_free(words);
    return ok;
}

bool IncrementBootCounter(uint32_t* boot_count) {
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open("m0_diag", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "nvs_open failed: %s", esp_err_to_name(err));
        return false;
    }

    uint32_t count = 0;
    err = nvs_get_u32(handle, "boot_count", &count);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(kTag, "nvs_get_u32 failed: %s", esp_err_to_name(err));
        nvs_close(handle);
        return false;
    }

    ++count;
    err = nvs_set_u32(handle, "boot_count", count);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(kTag, "NVS boot counter commit failed: %s", esp_err_to_name(err));
        return false;
    }
    *boot_count = count;
    return true;
}

bool ProbeEs8311() {
    i2c_master_bus_config_t bus_config = {};
    bus_config.i2c_port = I2C_NUM_0;
    bus_config.sda_io_num = board::kI2cSda;
    bus_config.scl_io_num = board::kI2cScl;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;

    i2c_master_bus_handle_t bus = nullptr;
    esp_err_t err = i2c_new_master_bus(&bus_config, &bus);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "I2C bus initialization failed: %s", esp_err_to_name(err));
        return false;
    }

    bool found = false;
    for (uint8_t address : kEs8311Addresses) {
        err = i2c_master_probe(bus, address, 100);
        if (err == ESP_OK) {
            ESP_LOGI(kTag, "ES8311-compatible I2C response at 0x%02x", address);
            found = true;
            break;
        }
    }
    if (!found) {
        ESP_LOGW(kTag, "No response at expected ES8311 addresses 0x18/0x19");
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_del_master_bus(bus));
    return found;
}

}  // namespace

esp_err_t ConfigureSafeGpios() {
    gpio_config_t output_config = {};
    output_config.pin_bit_mask =
        (1ULL << board::kPowerAmplifierEnable) |
        (1ULL << board::kLcdBacklight) |
        (1ULL << board::kStatusLed);
    output_config.mode = GPIO_MODE_OUTPUT;
    output_config.pull_up_en = GPIO_PULLUP_DISABLE;
    output_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    output_config.intr_type = GPIO_INTR_DISABLE;
    ESP_RETURN_ON_ERROR(gpio_config(&output_config), kTag, "configure outputs");

    // Safety first: keep the power amplifier and backlight off until their tests run.
    gpio_set_level(board::kPowerAmplifierEnable, 0);
    gpio_set_level(board::kLcdBacklight, 0);
    gpio_set_level(board::kStatusLed, 0);

    gpio_config_t button_config = {};
    button_config.pin_bit_mask = 1ULL << board::kBootButton;
    button_config.mode = GPIO_MODE_INPUT;
    button_config.pull_up_en = GPIO_PULLUP_ENABLE;
    button_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    button_config.intr_type = GPIO_INTR_DISABLE;
    return gpio_config(&button_config);
}

DiagnosticResult RunBoardDiagnostics() {
    DiagnosticResult result;

    const esp_app_desc_t* app = esp_app_get_description();
    ESP_LOGI(kTag, "Firmware %s built %s %s", app->version, app->date, app->time);

    esp_chip_info_t chip_info = {};
    esp_chip_info(&chip_info);
    ESP_LOGI(kTag, "ESP32-S3 cores=%d revision=%d", chip_info.cores,
             chip_info.revision);

    uint32_t flash_size = 0;
    const esp_err_t flash_err = esp_flash_get_size(nullptr, &flash_size);
    result.flash_bytes = flash_size;
    result.flash_ok = flash_err == ESP_OK && flash_size == 16 * 1024 * 1024;
    ESP_LOGI(kTag, "Flash: %" PRIu32 " bytes (%s)", flash_size,
             result.flash_ok ? "ok" : "unexpected");

    result.psram_bytes = esp_psram_get_size();
    const bool psram_size_ok = result.psram_bytes == 8 * 1024 * 1024;
    result.psram_ok = psram_size_ok && TestPsram();
    ESP_LOGI(kTag, "PSRAM: %u bytes, pattern test=%s",
             static_cast<unsigned>(result.psram_bytes),
             result.psram_ok ? "ok" : "failed");

    bool partitions_ok = true;
    partitions_ok &= CheckPartition("ota_0", ESP_PARTITION_TYPE_APP,
                                    ESP_PARTITION_SUBTYPE_APP_OTA_0, 0x380000);
    partitions_ok &= CheckPartition("ota_1", ESP_PARTITION_TYPE_APP,
                                    ESP_PARTITION_SUBTYPE_APP_OTA_1, 0x380000);
    partitions_ok &= CheckPartition("assets", ESP_PARTITION_TYPE_DATA,
                                    ESP_PARTITION_SUBTYPE_DATA_SPIFFS, 0x400000);
    partitions_ok &= CheckPartition("storage", ESP_PARTITION_TYPE_DATA,
                                    ESP_PARTITION_SUBTYPE_DATA_SPIFFS, 0x100000);
    partitions_ok &= CheckPartition("coredump", ESP_PARTITION_TYPE_DATA,
                                    ESP_PARTITION_SUBTYPE_DATA_COREDUMP, 0x20000);
    result.partitions_ok = partitions_ok;

    result.nvs_ok = IncrementBootCounter(&result.boot_count);
    ESP_LOGI(kTag, "NVS boot counter=%" PRIu32 " (%s)", result.boot_count,
             result.nvs_ok ? "ok" : "failed");

    result.es8311_found = ProbeEs8311();
    LogMemoryWatermarks();
    return result;
}

void LogMemoryWatermarks() {
    ESP_LOGI(kTag,
             "Heap free internal=%u largest_internal=%u free_psram=%u largest_psram=%u",
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(
                 heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
             static_cast<unsigned>(
                 heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)));
}

}  // namespace self_involving
