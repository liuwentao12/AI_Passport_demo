#pragma once

/**
 * 初始化 ILI9341 屏幕并显示八色色条，在 app_main 中调用一次。
 * 当前仅测试显示，触摸芯片保持禁用，尚未实现触摸坐标读取。
 * 色条绘制完成后返回，保留屏幕及 SPI 资源以维持显示。
 */
void touchscreen_test_start(void);
