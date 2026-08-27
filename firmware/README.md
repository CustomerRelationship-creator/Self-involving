# Firmware

这里将存放面向 movecall-moji-esp32s3-enterprise 的原创 ESP-IDF 固件，不复制或依赖已安装设备的软件架构。具体技术取舍见 [原创固件决策](../docs/firmware-decisions.md)。

## 计划模块

- board：引脚、电源、背光、按键和板级自检
- audio：ES8311、I²S、DMA、环形缓冲、ESP-SR AFE、VAD、WakeNet、AEC 与播放
- display：GC9A01 驱动、真实状态界面和动画调度
- transport：TLS WebSocket、会话协议、背压和重连
- identity：设备绑定、凭证轮换与撤销
- ota：双槽升级、自检、校验、回滚和恢复
- app：状态机、事件总线、权限状态、日志和指标

## 构建约束

- 目标芯片 ESP32-S3，启用 8 MB Octal PSRAM 与 16 MB Flash。
- 首个基线使用 ESP-IDF 6.0.2 和固定、可复现的工具链。
- 保留双 OTA 应用槽、独立资源分区、本地诊断空间和迁移余量。
- 单个应用镜像目标不超过 3.2 MB；持续空闲 PSRAM 不低于 1.5 MB。
- 禁止在源码、固件资源或串口日志中嵌入服务密钥。
- 每个硬件驱动先提供独立自检，再接入完整状态机。
- 所有队列、缓存和日志都必须有硬上限与满载策略。

第一份代码应是最小硬件自检固件，而不是直接实现云端对话。
