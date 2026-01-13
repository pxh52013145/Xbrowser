# XBrowser

Windows-only 的桌面浏览器项目，目标是用 **Qt 6 + QML** 重写一套“类 Zen Browser”的交互与 UI，同时使用 **Chromium 内核能力**（通过 **Microsoft Edge WebView2**）来提供网页渲染与 **Chrome/Edge 扩展**支持。

## 文档

- 设计文档：`docs/DESIGN.md`
- UX 规格：`docs/UX_SPEC.md`
- 构建指南：`docs/BUILDING.md`

## 目标（MVP）

- 垂直标签栏（Zen 风格的可见价值）
- 工作区（Workspace）与会话恢复
- 基础浏览能力：地址栏、前进后退、刷新、下载、权限提示
- 扩展：支持“加载解压扩展（unpacked）”与启用/禁用（后续逐步补齐扩展工具栏与弹窗）

## 开发环境（建议）

- Windows 10/11（x64）
- Visual Studio 2022（MSVC）
- Qt 6.6+（Qt Quick / Qt Quick Controls 2 / Qt Shader Tools）
- CMake 3.24+
- WebView2 Runtime（Evergreen，系统通常已安装；否则按文档安装）
