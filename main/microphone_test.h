#pragma once

/**
 * 启动 INMP441 麦克风测试：初始化 NVS、I2S、Wi-Fi 和 HTTP 录音网页。
 * 在 app_main 中调用一次；STA 模式最多等待联网 30 秒。
 * 返回后，由 HTTP 服务处理录音请求；联网超时则不启动 HTTP 服务。
 * 此模块负责默认网络接口及事件循环的初始化。
 */
void microphone_test_start(void);
