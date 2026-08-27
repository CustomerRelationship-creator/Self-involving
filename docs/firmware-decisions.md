# 原创固件决策

## 技术基线

- ESP-IDF 6.0.2 作为首个可复现基线；升级 SDK 必须通过同一套硬件和资源测试。
- ESP32-S3 双核，16 MB Flash、8 MB Octal PSRAM。
- ESP-SR AFE 用于 AEC、VAD、WakeNet 和基础音频增强。
- ES8311 维持硬件 24 kHz I²S；AFE 路径重采样到其要求的 16 kHz，播放参考同时进入 AEC。
- 首版网络只实现 TLS WebSocket；不同时维护 MQTT/UDP 和 BLE 数据通道。
- 远端认知服务使用供应商适配器，设备协议不绑定某个模型 API。

## 任务划分

| 任务 | 职责 | 内存原则 |
|---|---|---|
| audio_rx | I²S DMA、时间戳和输入检查 | DMA 位于内部 RAM，不动态增长 |
| afe | AEC、VAD、WakeNet、上行 PCM | 单个 AFE 实例，工作区放 PSRAM |
| audio_codec | Opus 编解码与重采样 | 固定帧池，不按帧分配堆内存 |
| transport | TLS、会话和音频帧 | 固定发送/接收窗口和背压 |
| audio_tx | 播放队列、功放与 AEC 参考 | 过期帧丢弃，禁止积压 |
| ui | 圆屏、LED、字幕与意图状态 | 区域刷新，避免双全屏常驻 |
| supervisor | 状态机、看门狗、指标和恢复 | 最高权限，但不处理大缓冲 |

跨任务通信使用固定大小队列、事件组和对象池。任何队列都必须定义满载策略，不能把“稍后处理”实现成无界堆积。

## 音频链路

1. ES8311 采集 24 kHz 单声道 PCM。
2. 固定环形缓冲保留最近音频；待机数据只在 RAM 中覆盖。
3. 重采样并输入 ESP-SR AFE。
4. 本地 WakeNet/VAD 决定是否打开远端会话。
5. 上行目标为 20 ms Opus 帧；复杂度、FEC、DTX 和码率由实机测试确定。
6. 下行音频按时间戳播放，并把播放参考同步送入 AEC。
7. 在 AEC 指标通过之前，播放期间不开放完整语音打断。

小智默认使用 60 ms Opus 帧。我们的 20 ms 目标会降低单帧等待，但增加包率与 CPU 开销，因此必须以首包延迟、丢包、功耗和看门狗数据决定是否保留。

## 协议

首版消息分为：

- hello/capabilities：固件、硬件、协议、音频和资源能力。
- session：开始、取消、结束、超时。
- audio：序号、单调时间戳、编解码参数和负载。
- state：设备状态、认知状态和可见说明。
- consent：权限请求、批准、拒绝和过期。
- body.action：屏幕、LED、音量与播放等枚举动作。
- health：内存最低水位、队列峰值、网络、温度和重启原因。
- update：检查、下载、验证、自检、确认或回滚。

设备不在第一版实现通用 MCP 服务器。复杂工具属于远端认知层；设备端只暴露有限身体能力，降低内存占用和误调用面。

## 状态机

核心状态为 BOOTING、PROVISIONING、IDLE_LOCAL、CONNECTING、LISTENING、THINKING、SPEAKING、MUTED、DEGRADED_OFFLINE、UPDATING、ERROR 和 RECOVERY。网络断开不得影响本地静音、按键、显示、自检或 USB 恢复。

## OTA

保留双 OTA 槽并启用回滚。新镜像首次启动后依次验证 NVS、PSRAM、显示、音频输入、音频输出、网络和资源挂载；只有自检与服务握手均成功才标记有效。ESP-IDF 官方 [OTA 回滚机制](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/ota.html) 支持未确认镜像自动回到上一版本。

首次开发不烧写不可逆安全 eFuse。签名、Flash Encryption 和 Secure Boot 在恢复流程成熟后进入单独安全里程碑。

## 首批代码顺序

1. 可恢复的 boot/board 自检。
2. GC9A01、LED 和按键。
3. ES8311 录放音环回与指标。
4. PSRAM 环形缓冲和有界队列。
5. ESP-SR AFE、VAD、WakeNet。
6. TLS WebSocket 与模拟服务器。
7. 端到端远端语音。
8. OTA 自检与回滚。
9. 圆屏人格和主动事件。
