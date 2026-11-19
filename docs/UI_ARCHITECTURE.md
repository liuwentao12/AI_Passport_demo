# 身份卡 UI 开发指南

## 运行入口与职责

`main/main.c` 只调用 `app_start()`。启动后 UI 任务初始化硬件和 LVGL，创建身份卡与详情页，循环消费资料更新并调用 `lv_timer_handler()`。

| 模块 | 职责 | 后续通常改哪里 |
| --- | --- | --- |
| `main/board/board_config.h` | 引脚、分辨率、SPI 参数 | 更换接线或屏幕时 |
| `main/board/display.c` | SPI 和 ILI9341 初始化、同步等待 DMA | 显示驱动变化时 |
| `main/board/touch.c` | 触摸输入接口；目前未接入实际芯片 | 确认触摸型号后 |
| `main/ui/port/lvgl_port.c` | LVGL tick、RGB565 字节交换、刷屏和输入适配 | 换显示库移植配置时 |
| `main/app/identity.c` | 演示资料、字符串有效性检查 | 修改默认身份信息 |
| `main/app/app.c` | 单 UI 任务、跨任务资料更新队列 | 接入麦克风、网络等业务事件 |
| `main/ui/ui.c` | 公共顶栏、导航、页面可见性 | 新增页面和导航 |
| `main/ui/ui_theme.c/.h` | 色彩、字体、通用控件创建 | 统一调整视觉风格 |
| `main/ui/pages/` | 页面控件创建及资料显示 | 修改具体页面布局 |

首版以 240×320 竖屏为设计尺寸，英文演示资料，头像使用姓名缩写。两个页面仅创建一次，切换时隐藏/显示，避免反复分配对象。卡片省略超长字段，详情页完整显示姓名、身份、编号、地点、签发日期和简介，自动换行并可竖向滚动。DEMO 标记表示示例资料，不代表真实身份验证。

## 数据更新：业务任务不要操作 LVGL

启动完成后，普通 FreeRTOS 任务可以这样更新资料：

```c
#include <stdio.h>
#include "app/app.h"

identity_t profile = *identity_demo();
snprintf(profile.name, sizeof(profile.name), "%s", "Your Name");
snprintf(profile.initials, sizeof(profile.initials), "%s", "YN");
ESP_ERROR_CHECK(app_set_identity(&profile));
```

`app_set_identity()` 检查每个字符串均有终止符、姓名非空，然后把整个结构体复制到长度为 1 的队列。调用者随后可以释放或修改自己的结构体。连续更新会保留最新一份；不保存到 Flash，重启恢复演示资料。该函数不允许 ISR 调用，也不允许调用期间另一任务修改同一输入结构体。

执行链为：业务任务 → `app_set_identity()` → FreeRTOS 队列 → UI 任务 → `ui_set_identity()` → 两个页面的 label。所有 `lv_*` 调用由 UI 任务独占，因此首版不需要跨任务 LVGL 锁。

目前普通资料更新不等于录音控制。接入麦克风时应新增命令队列：按钮回调发命令，录音任务处理阻塞工作，再通过状态消息回传；不要在 LVGL 回调里等 I2S、Wi-Fi 或 HTTP。

## 刷屏和内存

- 沿用已验证的 SPI2 引脚、10 MHz 时钟和 RGB 顺序，画布为 240×320 竖屏。
- 用户反馈镜像显示后，按左右镜像处理：在面板初始化后调用 `esp_lcd_panel_mirror()`，配置集中于 `board_config.h`，当前 X=true、Y=false。横向色条左右对称，不能验证左右方向；本次修正仅静态检查，需上板确认文字阅读方向。后续触摸校准应以修正后的显示坐标为准。
- LVGL 使用 48 KiB 内存池，LCD DMA 缓冲为 `240 × 20 × 2 = 9600` 字节，不依赖全屏缓冲或 PSRAM。
- LVGL 生成局部 RGB565 → port 交换为 SPI 字节顺序 → `board_display_draw()` → 中断释放信号量 → UI 任务确认完成 → `lv_display_flush_ready()`。
- 一次只有一笔颜色传输，DMA 完成后才复用缓冲。等待超时会 fail-fast，不会继续重用仍可能被 DMA 访问的内存。
- 驱动和 UI 生命周期持续到重启。初始化失败由应用终止启动；尚未提供运行时卸载/重启 API。

## 触摸接入点

当前 `board_touch_is_available()` 返回 false；`board_touch_read()` 清零输出并返回 false。界面显示 `TOUCH NOT CONFIGURED`，板上暂时不能通过点击切页。LVGL 的导航回调已连接，待接入真实输入后使用。

确认芯片型号后，在 board 层增加对应初始化和采样逻辑；初始化应在 LCD 初始化共享 SPI 总线之后、开始 LVGL 输入轮询之前完成。不要再次 `spi_bus_initialize()`。

`board_touch_read()` 应返回已校准的竖屏坐标（x=0..239、y=0..319）和按压状态。驱动负责原始数据过滤、轴交换和方向转换；port 负责转为 `lv_indev_data_t`。读取失败或越界会释放按压状态，避免误触。

点击 Details 的执行链：触摸采样 → LVGL input → `LV_EVENT_CLICKED` → `navigation()` → `ui_show_page(UI_PAGE_DETAILS)`。未来第三个页面可以沿用相同方式，业务逻辑无需知道 LCD 引脚。

## 添加页面与中文

1. 在 `ui/pages/` 新增页面的 create/update 接口，页面持有自己的对象指针。
2. 在 `ui.h` 的 `ui_page_t` 注册页面，在 `ui.c` 创建并加入导航和显示切换。当前底部为两按钮布局，新增页面时一并调整宽度/位置。
3. 通过应用层数据/事件传入业务状态，不在页面内读取硬件。

当前内置 Montserrat 12/14/16/20 字号不支持汉字。中文需添加具有适当授权的 LVGL 中文字库（可按实际字符裁剪），再修改主题和资料。字体配置同时写在 `sdkconfig.defaults` 和当前 `sdkconfig`；现有配置不会被 defaults 自动覆盖。

## 编译和验证（由用户执行）

在已激活 ESP-IDF 6.1 的终端执行 `idf.py build`，component manager 会按 `main/idf_component.yml` 和 `dependencies.lock` 安装固定版本 LVGL 9.3.0。当前没有自动烧录步骤。

硬件测试入口仍保留在 `microphone_test.c` 和 `touchscreen_test.c`；诊断时替换 `app_main()` 的启动函数。色条测试与正式 UI 会初始化同一 SPI 总线，不能同时启动。

可选主机检查（安装 CMake/Ninja 和本机 C/C++ 编译器后）：

```sh
cmake -S tests -B build-host -G Ninja
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

主机用真实 LVGL 检查字符串边界、无触摸时无按下状态、可见控件边界、100 次导航回调切换及内存稳定性。测试成功会在 `build-host` 生成 `identity.ppm` 和 `details.ppm` 供查看；不能替代上板验证。

上板检查：
- 上电出现 AI PASSPORT 身份卡，颜色/方向正确、文字没有被截到屏幕外。
- 连续运行稳定，无看门狗复位、DMA timeout 或内存分配失败。
- 修改默认资料后重编译，验证两页数据；实际触摸接入后检查四角坐标与按钮切换。
- 确认麦克风诊断仍可单独启动。

本次状态：用户要求不要代为编译，已停止此前启动的固件与主机构建，之后只做静态审查。身份数据独立边界测试在该要求前已通过；完整固件、页面测试和硬件验证尚未完成。

参考：[LVGL 9.3 显示适配说明](https://lvgl.io/docs/open/9.3/details/main-modules/display/setup.html)、[组件版本](https://components.espressif.com/components/lvgl/lvgl/versions/9.3.0)。
