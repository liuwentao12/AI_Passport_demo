# AI Passport Demo

ESP32 + ILI9341（240×320 竖屏）的 LVGL 身份卡应用，使用 ESP-IDF 6.1 和固定版本 LVGL 9.3.0。

- 上电进入身份卡，显示姓名缩写头像、姓名、身份、编号和所在地；提供资料详情页。
- 首版使用英文演示资料，统一标记 DEMO。
- 已接入 XPT2046 轮询驱动（CS=17，PEN 不接）；首次启动按屏幕十字完成三点校准和中心验证，参数保存后可点击导航及滑动详情，待上板验证。
- 原屏幕色条和麦克风测试保留，可单独切换入口运行。
- 按 K2 或点底部 Muyu 进入电子木鱼，轻点白色木鱼触发短鸣并加 1 功德。计数在切页时保留，重启后归零。蜂鸣器信号接 GPIO4、共地。默认按无源蜂鸣器输出 2 kHz PWM，有源模块将 `BOARD_BUZZER_ACTIVE` 设为 `true`，低电平触发模块将 `BOARD_BUZZER_ACTIVE_LOW` 设为 `true`。裸蜂鸣器若需较大电流，应通过驱动电路控制。

请先阅读 [UI 架构与开发指南](docs/UI_ARCHITECTURE.md)，其中包含数据流、线程边界、触摸接入点、添加页面方式和上板验证清单。

常用修改位置：

| 需求 | 文件 |
| --- | --- |
| 更改默认身份资料 | `main/app/identity.c` |
| 更改身份卡布局 | `main/ui/pages/identity_page.c` |
| 更改详情布局 | `main/ui/pages/details_page.c` |
| 更改主题 | `main/ui/ui_theme.h` |
| 添加页面与导航 | `main/ui/ui.c` |
| 调整触摸采样 | `main/board/touch.c` |
| 重新触摸校准 | `main/board/board_config.h` 中 `BOARD_TOUCH_FORCE_CALIBRATION` |
| 更改引脚 | `main/board/board_config.h` |

编译、烧录和硬件验证由用户执行；当前不宣称完整构建或上板验证通过。主机测试源码位于 `tests/`，执行方式见开发指南。

## AI 语音问答

1. 复制 `main/app/voice_config.example.h` 为 `main/app/voice_config.local.h`，填入 2.4 GHz 手机热点的 SSID、密码和硅基流动中国站 API Key。Git 已忽略本地文件，烧录后的固件包含这些值，请把固件也留在本地。
2. 初次烧录这一版本时写入完整固件和分区表。工程默认启用 1500 KiB 应用分区、系统 CA 证书包和思源黑体。ESP-IDF 构建会通过组件管理器拉取 cJSON 并更新依赖锁。
3. 按 KEY3（GPIO14）进入 AI Chat 并开始录音。说一句短问题，再按 KEY3 停止。录音满 2.5 秒自动停止，短于 0.25 秒会提示重试。
4. ESP32 连接热点并通过 NTP 校时，将 16 kHz、16 位单声道 WAV 直接发给 SenseVoiceSmall。识别文本发给默认免费聊天模型 `Qwen/Qwen2.5-7B-Instruct`。问题和回答可上下滑动查看。每次按键开启一轮独立问答。

聊天请求最多 120 个输出 token，屏幕回答最多 480 个 UTF-8 字节。遇到 `Model unavailable` 时，在硅基流动模型目录核对可用的免费模型，再修改本地 `VOICE_CHAT_MODEL`。

## KEY1 NFC 分享

断开 PN532 和 ESP32 的电源，把模块拨到 HSU / UART，再接线并上电。UART 为 115200、8N1。

| PN532 | ESP32 |
| --- | --- |
| RX | GPIO5（ESP32 TX） |
| TX | GPIO34（ESP32 RX） |
| GND | GND |
| VCC | 模块支持 3.3 V 供电时接 3V3，供电要求以板卡标注为准 |

UART 使用 3.3 V 逻辑电平。GPIO5 是启动配置脚，接模块的 RX 输入。拨码按模块上的 HSU 标注设置，IRQ 和复位线可留空。

按 KEY1 进入身份卡。串口日志 `UART online, PN532 firmware ...` 表示模块已应答。出现 `Tap phone near NFC` 后，将手机 NFC 区域对准模块天线。约 20 秒内，手机可以读取只读 NDEF 网址 `https://github.com/liuwentao12`。若提示 `Check PN532 and wiring`，检查 HSU 模式、供电和 TX/RX 是否交叉。
