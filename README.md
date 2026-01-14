# XBrowser

Windows-only 的桌面浏览器项目，目标是用 **Qt 6 + QML** 重写一套“类 Zen Browser”的交互与 UI，同时使用 **Chromium 内核能力**（通过 **Microsoft Edge WebView2**）来提供网页渲染与 **Chrome/Edge 扩展**支持。

## 文档

- 设计文档：`docs/DESIGN.md`
- UX 规格：`docs/UX_SPEC.md`
- 构建指南：`docs/BUILDING.md`
- 运行指南：`docs/RUNNING.md`

## 快速运行（开发）

推荐（构建 + 运行一条命令）：

```powershell
.\scripts\dev.ps1 -Config Debug
```

也可以直接双击：`scripts\\dev.cmd`

如果你只想手动构建/运行：

```powershell
cmake --build build --config Debug
.\build\Debug\xbrowser.exe
```

如果遇到“缺少 Qt6*.dll / 平台插件”等系统错误（或你禁用了自动部署），使用：

- 双击 `scripts\\run.cmd`（等价于 `run.ps1`，带 `ExecutionPolicy Bypass`）
- 需要“可双击运行”的独立目录：先执行 `scripts\\deploy.cmd` / `.\scripts\deploy.ps1 -Config Debug`，再双击 `build\\Debug\\xbrowser.exe`
- Visual Studio：打开 `build\\XBrowser.sln`，把 `xbrowser` 设为启动项目运行/调试

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
