# Identity UI Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 上电显示 LVGL 身份卡，提供可扩展页面和硬件边界。
**Architecture:** board 驱动 → LVGL port → pages；app 独占 UI 线程，通过队列接收数据。
**Tech Stack:** ESP-IDF 6.1, LVGL 9.3.0, C, FreeRTOS.
**Spec:** ../specs/2026-11-19-identity-ui-design.md

## Global Constraints
- 保留 240×320、SPI2、原引脚和 10 MHz，不同时启动色条测试和正式 UI。
- 触摸未配置时不产生按下事件。不自动运行麦克风测试。
- 首版使用英文内置字体、明确 DEMO 状态；不烧录。

## Review Focus
- DMA buffer 生命周期：必须传输完成才复用；固件构建及静态检查。
- 未终止字符串：主机测试拒绝无 NUL 的字段。
- 超长但有效资料：LVGL 标签裁切/省略，主机布局测试。
- 页面反复切换：真实 LVGL 测试对象与内存稳定性。
- 未配置触摸：read 返回 released，硬件验收清单单列。

## Tasks
- [x] 1. 创建 identity 数据模型及先失败后通过的主机边界测试。
- [x] 2. 提取 board/display、添加 board/touch 接口与 ui/port；加入固定 LVGL 依赖和缓冲配置。
- [x] 3. 创建主题、身份卡/详情页、UI 管理器及真实 LVGL 主机测试源码。
- [x] 4. 接入 app 任务、覆盖队列和 main。
- [x] 5. 更新开发指南、上板验证清单和验证结果。
- [ ] 用户执行：完整固件编译、主机页面测试和预览、上板验证。

## Execution notes
- 当前目录不是 Git 仓库；直接在用户指定工作区修改，无 worktree/commit。
- 用户“接入 LVGL”授权执行前一轮具体分层方案，不重复请求实施许可。
- 用户随后明确“不要帮我编译”：已中止固件与主机构建，后续仅静态审查；编译和运行验证交由用户执行。
- 身份数据边界测试在该指令前通过；页面测试最初暴露枚举比较 signedness 警告，已修正，重编译在用户指令后中止，不能宣称通过。
- 独立静态审查发现详情页不能恢复被省略的完整资料；已改为自动换行、内容高度和纵向滚动，补齐 role/number，并补充对应测试断言。遵循用户要求未运行新增测试。
