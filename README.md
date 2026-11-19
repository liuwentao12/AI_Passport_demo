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
