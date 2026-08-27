#include "display_test.h"

#include <algorithm>
#include <cstddef>

#include "board_pins.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"

namespace self_involving {
namespace {

constexpr char kTag[] = "m0_display";
constexpr spi_host_device_t kLcdHost = SPI2_HOST;
constexpr std::size_t kFramePixels = board::kLcdWidth * board::kLcdHeight;
constexpr std::size_t kFrameBytes = kFramePixels * sizeof(uint16_t);

constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kRed = 0xF800;
constexpr uint16_t kGreen = 0x07E0;
constexpr uint16_t kBlue = 0x001F;
constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kAmber = 0xFD20;

}  // namespace

esp_err_t DisplayTest::Initialize() {
    // Initialize every field explicitly through zero-initialization. The
    // component helper macro targets an older spi_bus_config_t and trips IDF
    // 6's missing-field warning when warnings are treated as errors.
    spi_bus_config_t bus_config = {};
    bus_config.mosi_io_num = board::kLcdMosi;
    bus_config.miso_io_num = GPIO_NUM_NC;
    bus_config.sclk_io_num = board::kLcdSclk;
    bus_config.quadwp_io_num = GPIO_NUM_NC;
    bus_config.quadhd_io_num = GPIO_NUM_NC;
    bus_config.max_transfer_sz = kFrameBytes;
    ESP_RETURN_ON_ERROR(spi_bus_initialize(kLcdHost, &bus_config, SPI_DMA_CH_AUTO),
                        kTag, "initialize LCD SPI bus");

    esp_lcd_panel_io_handle_t io_handle = nullptr;
    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = board::kLcdCs;
    io_config.dc_gpio_num = board::kLcdDc;
    io_config.spi_mode = 0;
    io_config.pclk_hz = board::kLcdPixelClockHz;
    io_config.trans_queue_depth = 4;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi(
            reinterpret_cast<esp_lcd_spi_bus_handle_t>(
                static_cast<intptr_t>(kLcdHost)),
            &io_config,
            &io_handle),
        kTag, "create LCD panel IO");

    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = board::kLcdReset;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel = 16;

    esp_lcd_panel_handle_t panel_handle = nullptr;
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle), kTag,
        "create GC9A01 panel");
    panel_ = panel_handle;

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel_handle), kTag, "reset panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel_handle), kTag, "initialize panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(panel_handle, true), kTag,
                        "enable panel inversion");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel_handle, true), kTag,
                        "enable panel");

    frame_buffer_ = static_cast<uint16_t*>(
        heap_caps_malloc(kFrameBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (frame_buffer_ == nullptr) {
        ESP_LOGE(kTag, "Unable to allocate %u-byte display buffer in PSRAM",
                 static_cast<unsigned>(kFrameBytes));
        return ESP_ERR_NO_MEM;
    }

    gpio_set_level(board::kLcdBacklight, 1);
    return Fill(kBlack);
}

esp_err_t DisplayTest::ShowDiagnosticPattern(bool overall_ok) {
    if (overall_ok) {
        const uint16_t colors[] = {kGreen, kWhite, kBlue};
        return ShowBands(colors, sizeof(colors) / sizeof(colors[0]));
    }
    const uint16_t colors[] = {kRed, kAmber, kBlack};
    return ShowBands(colors, sizeof(colors) / sizeof(colors[0]));
}

esp_err_t DisplayTest::ShowMuted() {
    return Fill(kBlack);
}

esp_err_t DisplayTest::CyclePattern() {
    static constexpr uint16_t kPatterns[][4] = {
        {kRed, kGreen, kBlue, kWhite},
        {kWhite, kAmber, kWhite, kAmber},
        {kBlue, kBlack, kBlue, kBlack},
    };
    pattern_index_ = (pattern_index_ + 1) %
                     (sizeof(kPatterns) / sizeof(kPatterns[0]));
    return ShowBands(kPatterns[pattern_index_], 4);
}

esp_err_t DisplayTest::Fill(uint16_t rgb565) {
    if (panel_ == nullptr || frame_buffer_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    std::fill_n(frame_buffer_, kFramePixels, rgb565);
    return esp_lcd_panel_draw_bitmap(
        static_cast<esp_lcd_panel_handle_t>(panel_), 0, 0, board::kLcdWidth,
        board::kLcdHeight, frame_buffer_);
}

esp_err_t DisplayTest::ShowBands(const uint16_t* colors,
                                 std::size_t color_count) {
    if (panel_ == nullptr || frame_buffer_ == nullptr || colors == nullptr ||
        color_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    for (int y = 0; y < board::kLcdHeight; ++y) {
        const std::size_t band = std::min(
            color_count - 1,
            static_cast<std::size_t>(y) * color_count / board::kLcdHeight);
        std::fill_n(frame_buffer_ + y * board::kLcdWidth, board::kLcdWidth,
                    colors[band]);
    }
    return esp_lcd_panel_draw_bitmap(
        static_cast<esp_lcd_panel_handle_t>(panel_), 0, 0, board::kLcdWidth,
        board::kLcdHeight, frame_buffer_);
}

}  // namespace self_involving
