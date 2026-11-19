# AI Passport Demo

ESP32 + ILI9341（240×320 竖屏）的 LVGL 身份卡应用，使用 ESP-IDF 6.1 和固定版本 LVGL 9.3.0。

- 上电进入身份卡，显示姓名缩写头像、姓名、身份、编号和所在地；提供资料详情页。
- 首版使用英文演示资料，统一标记 DEMO。
- 触摸芯片尚未配置，板上点击暂不可用；界面显示 TOUCH NOT CONFIGURED。
- 原屏幕色条和麦克风测试保留，可单独切换入口运行。

请先阅读 [UI 架构与开发指南](docs/UI_ARCHITECTURE.md)，其中包含数据流、线程边界、触摸接入点、添加页面方式和上板验证清单。

常用修改位置：

| 需求 | 文件 |
| --- | --- |
| 更改默认身份资料 | `main/app/identity.c` |
| 更改身份卡布局 | `main/ui/pages/identity_page.c` |
| 更改详情布局 | `main/ui/pages/details_page.c` |
| 更改主题 | `main/ui/ui_theme.h` |
| 添加页面与导航 | `main/ui/ui.c` |
| 接入触摸芯片 | `main/board/touch.c` |
| 更改引脚 | `main/board/board_config.h` |

编译、烧录和硬件验证由用户执行；当前不宣称完整构建或上板验证通过。主机测试源码位于 `tests/`，执行方式见开发指南。
