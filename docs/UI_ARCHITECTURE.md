# 身份卡 UI 开发指南

## 运行入口与职责

`main/main.c` 只调用 `app_start()`。启动后 UI 任务初始化硬件和 LVGL，必要时完成触摸校准，再创建身份卡与详情页，循环消费资料更新并调用 `lv_timer_handler()`。

| 模块 | 职责 | 后续通常改哪里 |
| --- | --- | --- |
| `main/board/board_config.h` | 引脚、分辨率、SPI 参数 | 更换接线或屏幕时 |
| `main/board/display.c` | SPI 和 ILI9341 初始化、同步等待 DMA | 显示驱动变化时 |
| `main/board/touch.c` | XPT2046 SPI 采样、压力判断、校准参数保存 | 调整触摸采样时 |
| `main/board/touch_transform.c` | 三点仿射坐标变换，不依赖硬件 | 修改校准算法时 |
| `main/ui/port/touch_calibration.c` | 开机十字点校准及中心验证 | 修改校准交互时 |
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
- 显示方向由用户上板确认正常：`board_config.h` 中 X=true、Y=false。触摸直接按当前屏幕显示的十字点校准，不再额外套用 LCD 的 mirror 标志；修改方向或分辨率时，旧校准自动失效。
- LVGL 使用 48 KiB 内存池，LCD DMA 缓冲为 `240 × 20 × 2 = 9600` 字节，不依赖全屏缓冲或 PSRAM。
- LVGL 生成局部 RGB565 → port 交换为 SPI 字节顺序 → `board_display_draw()` → 中断释放信号量 → UI 任务确认完成 → `lv_display_flush_ready()`。
- 一次只有一笔颜色传输，DMA 完成后才复用缓冲。等待超时会 fail-fast，不会继续重用仍可能被 DMA 访问的内存。
- 驱动和 UI 生命周期持续到重启。初始化失败由应用终止启动；尚未提供运行时卸载/重启 API。

## 触摸接入点

已按用户确认的 XPT2046 接线接入：SCK=18、MOSI=23、MISO=19、CS2=17；与 LCD 共用 SPI2，LCD CS1=16。PEN 不接，使用 1 MHz、SPI mode 0 轮询 Z1/Z2 判断按压。驱动每次取 5 组坐标的中值，并在采样前后检查是否仍按下。

初始化顺序：`board_display_init()` 建立总线 → `board_touch_init()` 添加触摸设备 → LVGL display/input → `ui_touch_calibrate()` → 正式页面。触摸驱动不重复初始化总线；所有显示与触摸调用均在同一个 UI 任务中，LCD 传输完成后才继续采样。

首次启动会进入 Touch setup：依次按住左上、右上、左下的十字约 0.1 秒，看到 Release to continue 后松手；最后点按中心十字验证。请用触摸笔或指甲轻压电阻屏。三点仿射变换自动处理轴交换、正反方向和原始 ADC 范围，中心误差每轴不超过 15 像素才接受。错误点位会重新开始。

校准仅在成功松手后结束，避免触摸穿透到正式页面。每个点等待超过 60 秒会退出到身份卡并显示 TOUCH NOT CONFIGURED；检查接线后重启重试。串口输出校准原始坐标与错误原因。

参数保存在 NVS namespace `touch` 的 `cal_v1` 键中，包含版本、分辨率和显示镜像配置。下次启动直接复用。NVS 初始化或保存失败不会擦除整片 NVS，本次校准仍可在 RAM 中使用，下次重启需重做。需要手动重新校准时，将 `BOARD_TOUCH_FORCE_CALIBRATION` 改为 true，完成一次后改回 false。

`board_touch_is_available()` 表示驱动已初始化且已校准；`board_touch_read()` 返回已校准的竖屏坐标和按压状态。轻微超出屏幕边界时裁切到边缘，明显异常坐标/读取失败时释放按压。LVGL port 将其转换为 `lv_indev_data_t`。

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

主机用真实 LVGL 检查字符串边界、可见控件边界、100 次导航回调切换及内存稳定性；新增独立坐标变换测试覆盖轴交换、反向轴、中心映射及退化校准点。主机 UI 测试不链接 ESP-IDF 硬件驱动。测试成功会在 `build-host` 生成 `identity.ppm` 和 `details.ppm` 供查看；不能替代上板验证。

上板检查：
- 首次启动完成四次点按校准，再出现 AI PASSPORT 身份卡；重启后不重复校准。
- 连续运行稳定，无看门狗复位、DMA timeout 或内存分配失败。
- 点击 Identity/Details 切页，在详情页上下拖动并确认松手停止；检查四角没有镜像或偏移。
- 确认麦克风诊断仍可单独启动。

验证状态：用户已确认显示正常。XPT2046 驱动和校准仅完成静态审查，没有代为编译、执行新增测试或烧录；触摸采样、压力阈值、坐标精度和 NVS 重启恢复待用户上板验证。

参考：[LVGL 9.3 显示适配说明](https://lvgl.io/docs/open/9.3/details/main-modules/display/setup.html)、[组件版本](https://components.espressif.com/components/lvgl/lvgl/versions/9.3.0)。

触摸协议参考：[XPT2046 数据手册](https://files.waveshare.com/wiki/common/XPT2046_Datasheet.pdf)，按 24 时钟帧读取 12 位结果；PEN 未接时使用压力通道判定接触。
