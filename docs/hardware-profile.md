# 硬件档案

本项目面向第二台未安装应用固件的 movecall-moji-esp32s3-enterprise 设备。下列资料来自同型号已烧录设备的核实结果，首次烧录前仍需读取 Flash、确认晶振与电源行为，并保存原始镜像。

## 主控与存储

| 项目 | 规格 |
|---|---|
| 主控 | ESP32-S3，双核 240 MHz |
| 模组 | ESP32-S3-WROOM-1-N16R8 |
| Flash | 16 MB |
| PSRAM | 8 MB Octal PSRAM，80 MHz |
| 开发平台 | ESP-IDF 6.x，C/C++ |
| USB | 原生 USB Serial/JTAG，可供电、日志和烧录 |
| 无线 | 2.4 GHz Wi-Fi；芯片支持 BLE 5 |

## 显示

| 信号 | 配置 |
|---|---|
| 控制器 | GC9A01，240 × 240，16 位色 |
| SPI | 40 MHz |
| SCLK | GPIO16 |
| MOSI | GPIO17 |
| CS | GPIO15 |
| DC | GPIO7 |
| RST | GPIO18 |
| 背光 PWM | GPIO3 |

## 音频

| 信号 | 配置 |
|---|---|
| 编解码器 | ES8311 |
| 采样率目标 | 输入/输出 24 kHz |
| MCLK | GPIO6 |
| WS | GPIO12 |
| BCLK | GPIO14 |
| 麦克风数据 | GPIO13 |
| 扬声器数据 | GPIO11 |
| 功放使能 | GPIO9 |
| I²C | SDA GPIO5、SCL GPIO4 |

## 输入与状态

| 项目 | 配置 |
|---|---|
| BOOT 按键 | GPIO0；短按切换会话，长按进入静音/安全操作 |
| 单色 LED | GPIO21 |
| 触摸 | 初始版本不依赖触摸 |
| 供电 | 初始版本仅支持 USB 5 V，不假定存在可用电池管理 |

## 首次上电原则

1. 在写入前读取并保存完整 16 MB Flash 镜像、eFuse 摘要和 MAC 信息。
2. 不修改 eFuse、Secure Boot 或 Flash Encryption，直到恢复路径经过验证。
3. 首次固件仅初始化串口、屏幕、LED、按键和音频自检，不立即启用远端连接。
4. 引脚高低电平、背光和功放默认状态必须经过实机确认，避免上电爆音与过流。
