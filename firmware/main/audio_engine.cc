#include "audio_engine.h"

#include <algorithm>
#include <cstring>

#include "board_pins.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "voice_protocol.h"

namespace self_involving {
namespace {
constexpr char kTag[] = "m3_audio";
constexpr std::size_t kRingBytes = 512 * 1024;
constexpr std::size_t kPlaybackDepth = 8;

struct PcmFrame {
    std::uint16_t size;
    std::uint8_t data[kPcmFrameBytes];
};
}  // namespace

esp_err_t AudioEngine::Initialize() {
    i2c_master_bus_config_t i2c = {};
    i2c.i2c_port = I2C_NUM_0;
    i2c.sda_io_num = board::kI2cSda;
    i2c.scl_io_num = board::kI2cScl;
    i2c.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c.glitch_ignore_cnt = 7;
    i2c.flags.enable_internal_pullup = true;
    i2c_master_bus_handle_t i2c_bus = nullptr;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&i2c, &i2c_bus), kTag,
                        "create codec I2C bus");

    i2s_chan_config_t channel_config =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    channel_config.auto_clear = true;
    i2s_chan_handle_t tx = nullptr;
    i2s_chan_handle_t rx = nullptr;
    ESP_RETURN_ON_ERROR(i2s_new_channel(&channel_config, &tx, &rx), kTag,
                        "create I2S channels");
    i2s_std_config_t standard = {};
    standard.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(24000);
    standard.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    standard.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
    standard.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    standard.gpio_cfg.mclk = board::kI2sMclk;
    standard.gpio_cfg.bclk = board::kI2sBclk;
    standard.gpio_cfg.ws = board::kI2sWs;
    standard.gpio_cfg.dout = board::kI2sSpeakerData;
    standard.gpio_cfg.din = board::kI2sMicData;
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(tx, &standard), kTag,
                        "initialize I2S transmit");
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(rx, &standard), kTag,
                        "initialize I2S receive");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(tx), kTag, "enable I2S transmit");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(rx), kTag, "enable I2S receive");

    audio_codec_i2s_cfg_t data_config = {};
    data_config.port = I2S_NUM_0;
    data_config.rx_handle = rx;
    data_config.tx_handle = tx;
    const audio_codec_data_if_t* data_if = audio_codec_new_i2s_data(&data_config);
    if (data_if == nullptr) return ESP_ERR_NO_MEM;
    audio_codec_i2c_cfg_t control_config = {};
    control_config.port = I2C_NUM_0;
    control_config.addr = ES8311_CODEC_DEFAULT_ADDR;
    control_config.bus_handle = i2c_bus;
    const audio_codec_ctrl_if_t* control_if =
        audio_codec_new_i2c_ctrl(&control_config);
    const audio_codec_gpio_if_t* gpio_if = audio_codec_new_gpio();
    if (control_if == nullptr || gpio_if == nullptr) return ESP_ERR_NO_MEM;
    es8311_codec_cfg_t codec_config = {};
    codec_config.ctrl_if = control_if;
    codec_config.gpio_if = gpio_if;
    codec_config.codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH;
    codec_config.pa_pin = board::kPowerAmplifierEnable;
    codec_config.pa_reverted = false;
    codec_config.master_mode = false;
    const audio_codec_if_t* codec_if = es8311_codec_new(&codec_config);
    if (codec_if == nullptr) return ESP_ERR_NOT_FOUND;
    esp_codec_dev_cfg_t device_config = {};
    device_config.dev_type = ESP_CODEC_DEV_TYPE_IN_OUT;
    device_config.codec_if = codec_if;
    device_config.data_if = data_if;
    codec_ = esp_codec_dev_new(&device_config);
    if (codec_ == nullptr) return ESP_ERR_NO_MEM;
    esp_codec_dev_sample_info_t sample = {};
    sample.sample_rate = 24000;
    sample.channel = 1;
    sample.bits_per_sample = 16;
    sample.mclk_multiple = 256;
    if (esp_codec_dev_open(codec_, &sample) != ESP_CODEC_DEV_OK) return ESP_FAIL;
    esp_codec_dev_set_in_gain(codec_, 24.0f);
    esp_codec_dev_set_out_vol(codec_, CONFIG_M3_OUTPUT_VOLUME);
    esp_codec_dev_set_out_mute(codec_, true);

    ring_ = static_cast<std::uint8_t*>(
        heap_caps_malloc(kRingBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (ring_ == nullptr) return ESP_ERR_NO_MEM;
    std::memset(ring_, 0, kRingBytes);
    playback_queue_ = xQueueCreate(kPlaybackDepth, sizeof(PcmFrame));
    return playback_queue_ == nullptr ? ESP_ERR_NO_MEM : ESP_OK;
}

esp_err_t AudioEngine::Start(MicrophoneFrameCallback callback, void* context) {
    callback_ = callback;
    callback_context_ = context;
    if (xTaskCreatePinnedToCore(CaptureTaskThunk, "audio_rx", 4096, this, 7,
                                nullptr, 0) != pdPASS ||
        xTaskCreatePinnedToCore(PlaybackTaskThunk, "audio_tx", 4096, this, 7,
                                nullptr, 1) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void AudioEngine::SetMuted(bool muted) {
    muted_.store(muted);
    session_active_.store(false);
    esp_codec_dev_set_in_mute(codec_, muted);
    esp_codec_dev_set_out_mute(codec_, true);
}

esp_err_t AudioEngine::QueueSpeakerAudio(const std::uint8_t* data,
                                         std::size_t size) {
    if (data == nullptr || size == 0 || size > kPcmFrameBytes) {
        return ESP_ERR_INVALID_ARG;
    }
    PcmFrame frame = {};
    frame.size = static_cast<std::uint16_t>(size);
    std::memcpy(frame.data, data, size);
    if (xQueueSend(playback_queue_, &frame, 0) != pdPASS) {
        ++dropped_playback_;
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

void AudioEngine::CaptureTaskThunk(void* context) {
    static_cast<AudioEngine*>(context)->CaptureTask();
}
void AudioEngine::PlaybackTaskThunk(void* context) {
    static_cast<AudioEngine*>(context)->PlaybackTask();
}

void AudioEngine::CaptureTask() {
    std::uint8_t frame[kPcmFrameBytes] = {};
    while (true) {
        if (esp_codec_dev_read(codec_, frame, sizeof(frame)) != ESP_CODEC_DEV_OK) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        StoreInRing(frame, sizeof(frame));
        if (!muted_.load() && session_active_.load() && callback_ != nullptr) {
            callback_(frame, sizeof(frame), callback_context_);
        }
    }
}

void AudioEngine::PlaybackTask() {
    PcmFrame frame = {};
    while (true) {
        if (xQueueReceive(playback_queue_, &frame, portMAX_DELAY) == pdPASS &&
            !muted_.load()) {
            esp_codec_dev_set_out_mute(codec_, false);
            esp_codec_dev_write(codec_, frame.data, frame.size);
        }
    }
}

void AudioEngine::StoreInRing(const std::uint8_t* data, std::size_t size) {
    const std::size_t first = std::min(size, kRingBytes - ring_write_);
    std::memcpy(ring_ + ring_write_, data, first);
    if (size > first) std::memcpy(ring_, data + first, size - first);
    ring_write_ = (ring_write_ + size) % kRingBytes;
}

}  // namespace self_involving
