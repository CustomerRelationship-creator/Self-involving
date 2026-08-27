# Firmware

这里将存放面向 movecall-moji-esp32s3-enterprise 的原创 ESP-IDF 固件，不复制或依赖已安装设备的软件架构。

## 计划模块

- board：引脚、电源、背光、按键和板级自检
- audio：ES8311、I²S、DMA、环形缓冲、VAD、唤醒与播放
- display：GC9A01 驱动、状态界面和动画调度
- connectivity：配网、TLS、会话协议和重连
- identity：设备绑定、凭证轮换与撤销
- ota：双槽升级、校验、回滚和恢复
- app：状态机、事件总线、日志和指标

## 构建约束

- 目标芯片 ESP32-S3，启用 8 MB Octal PSRAM 与 16 MB Flash。
- 使用固定 ESP-IDF 版本和可复现工具链。
- 初始阶段保留双 OTA 应用槽和独立资源分区。
- 禁止在源码、固件资源或串口日志中嵌入服务密钥。
- 每个硬件驱动先提供独立自检，再接入完整状态机。

第一份代码应是最小硬件自检固件，而不是直接实现云端对话。
