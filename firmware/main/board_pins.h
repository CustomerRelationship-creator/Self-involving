#pragma once

#include "driver/gpio.h"

namespace self_involving::board {

constexpr gpio_num_t kLcdSclk = GPIO_NUM_16;
constexpr gpio_num_t kLcdMosi = GPIO_NUM_17;
constexpr gpio_num_t kLcdCs = GPIO_NUM_15;
constexpr gpio_num_t kLcdDc = GPIO_NUM_7;
constexpr gpio_num_t kLcdReset = GPIO_NUM_18;
constexpr gpio_num_t kLcdBacklight = GPIO_NUM_3;

constexpr gpio_num_t kI2cSda = GPIO_NUM_5;
constexpr gpio_num_t kI2cScl = GPIO_NUM_4;

constexpr gpio_num_t kI2sMclk = GPIO_NUM_6;
constexpr gpio_num_t kI2sWs = GPIO_NUM_12;
constexpr gpio_num_t kI2sBclk = GPIO_NUM_14;
constexpr gpio_num_t kI2sMicData = GPIO_NUM_13;
constexpr gpio_num_t kI2sSpeakerData = GPIO_NUM_11;
constexpr gpio_num_t kPowerAmplifierEnable = GPIO_NUM_9;

constexpr gpio_num_t kBootButton = GPIO_NUM_0;
constexpr gpio_num_t kStatusLed = GPIO_NUM_21;

constexpr int kLcdWidth = 240;
constexpr int kLcdHeight = 240;
constexpr int kLcdPixelClockHz = 40 * 1000 * 1000;

}  // namespace self_involving::board
