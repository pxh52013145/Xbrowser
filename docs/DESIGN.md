# XBrowser 设计文档（Windows-only / Qt QML / Chromium / Extensions）

## 相关文档

- UX 规格：`docs/UX_SPEC.md`

## 1. 背景与目标

### 1.1 背景

你希望做一个：

- Windows-only 的桌面浏览器；
- UI 由 Qt/QML 完全掌控（更现代、更可定制）；
- 内核尽可能贴近“最新 Chromium”；
- **必须支持扩展**（至少 Chrome/Edge 扩展的主流能力）；
- 交互与结构参考 Zen Browser（例如：垂直标签、工作区、侧边栏、极简但高效的浏览体验）。

### 1.2 核心策略（先做壳，再做深）

把项目分成两层并严格解耦：

- **产品壳（UI + 状态机 + 数据）**：你要复刻/改良 Zen 的“可见价值”，这是项目的差异化。
- **网页引擎适配层（Engine Adapter）**：负责把“一个 Tab 能加载网页”这件事提供成稳定 API；未来如有需要可以更换引擎实现。

这份设计默认的引擎选择是：**WebView2（Edge Chromium）**，原因：

- Windows-only 与“接近最新 Chromium”的诉求非常匹配（Evergreen Runtime 自动更新）。
- WebView2 提供了 **Browser Extensions API**（扩展安装/枚举/启用禁用等能力）。
- 不需要自编译 Chromium/CEF，工程成本显著低于 CEF + Qt 从零绑定。

## 2. 需求拆分

### 2.1 必选（MVP 必须具备）

- 多标签（Tab）与垂直标签栏
- 工作区（Workspace）与会话恢复
- 基础导航：地址栏、前进/后退/刷新、加载状态、标题/Favicon
- 下载管理（至少：开始/进度/完成/打开目录）
- 权限请求（至少：通知、地理位置、摄像头/麦克风、剪贴板等的提示与持久化决策）
- 扩展支持（最低目标）：
  - 加载“解压扩展（Load unpacked）”
  - 已安装扩展列表
  - 启用/禁用

### 2.2 重要但可后置（M1~M3）

- 扩展工具栏（显示扩展图标、固定/隐藏）
- 扩展弹窗（action popup）与扩展页面（options page）
- 分屏/拆分视图（Split view）
- 侧边栏（书签/历史/下载/阅读列表/网页面板）
- 命令面板（Command palette）
- 主题系统（配色 + 圆角 + 动效 + 自定义 CSS-like tokens）
- 广告拦截（优先考虑扩展路线；内置拦截属于高成本）

### 2.3 明确非目标（第一阶段不做）

- 自研 Chromium 分支 / 自编译 Chromium
- 完整对标 Chrome 的多进程管理与所有策略（尽量复用 WebView2 的能力）
- 账号同步（书签/扩展云同步等，先本地化）

## 3. 技术选型

### 3.1 语言与框架

- C++20（业务核心、WebView2 封装、数据层）
- Qt 6 + QML（UI、动画、主题、布局）
- CMake（构建系统）

### 3.2 网页内核：WebView2

- 使用 Microsoft WebView2 SDK（COM 接口）
- 运行时：默认依赖 Evergreen Runtime（用户系统安装的 Edge WebView2）
- 用户数据目录：由应用控制（支持多 Profile / InPrivate 的扩展空间）

### 3.3 数据层

- SQLite（历史、书签、会话、站点权限、扩展元数据）
- JSON（少量 UI/偏好设置也可用 JSON，但建议最终归一到 SQLite）

## 4. 总体架构

### 4.1 分层

1) **UI 层（QML）**
- 只负责显示与交互，不直接触碰 WebView2 COM
- 通过 `QObject` 暴露的 ViewModel/Model 绑定数据

2) **浏览器核心（Core）**
- Tab/Workspace/Session 状态机
- 命令系统（快捷键、命令面板）
- 设置/权限/下载/历史/书签的统一业务逻辑

3) **引擎适配层（Engine Adapter）**
- `IWebContents`（“一个 Tab 的网页实例”抽象）
- WebView2 实现：`WebView2WebContents`
- 事件：导航、标题、图标、权限、下载、证书错误、进程崩溃

4) **平台层（Windows Integration）**
- 任务栏/跳转列表（可后置）
- 注册默认浏览器/协议（可后置）
- 崩溃收集（可后置）

### 4.2 进程模型（由 WebView2 提供）

- XBrowser 主进程：Qt UI + 业务逻辑
- WebView2 子进程：浏览器进程/渲染进程/网络等（由 Runtime 管理）
- 你需要关心的是：为每个 Tab 创建/销毁对应的 controller，并处理崩溃与重建。

## 5. UI（QML）设计要点（Zen 风格）

### 5.1 主窗口布局

- 左侧：垂直标签栏（可折叠，支持 pinned）
- 顶部：极简导航栏（地址栏 + 常用按钮 + 扩展入口）
- 中间：网页内容区（WebView host）
- 右侧（可选）：侧边栏（书签/历史/网页面板）

### 5.2 Model 驱动（推荐）

- Tabs：`QAbstractListModel`（每个 Tab 的 title/url/loading/favIcon/isPinned/workspaceId 等）
- Workspaces：`QAbstractListModel`
- Downloads：`QAbstractListModel`
- Extensions：`QAbstractListModel`

QML 只绑定 model，所有复杂行为放在 C++ 的 `Controller/ViewModel` 里（避免 QML 逻辑失控）。

### 5.3 WebView 嵌入策略（关键风险点）

WebView2 的 HWND-hosting 模式可能带来“空域问题”：原生渲染层级高于 Qt Quick SceneGraph，导致 QML Popup/阴影/菜单无法覆盖网页区域。

设计决策（推荐分两步）：

- **MVP：先用 HWND-hosting**（最省心、最快跑通）
  - 约束 UI：所有需要覆盖网页的控件（右键菜单、扩展弹窗、权限弹窗）用 **独立顶层窗口**实现（frameless / always-on-top / 自己定位），避免被网页层遮挡。
- **M2+：评估 WebView2 CompositionController**（更“原生融合”的方案）
  - 目标：允许 QML 层真正覆盖网页区域（更丝滑的 Zen 风格动效与叠层）
  - 成本：需要更深入的 Windows DComp/渲染链路集成

## 6. Tab / Workspace / Session

### 6.1 Tab 生命周期

- 创建：分配 TabId -> 创建 `IWebContents` -> 绑定事件 -> 导航初始 URL
- 激活/切换：只改变“可见 Tab”，并暂停/恢复某些 UI 更新（性能优化）
- 关闭：优雅释放 controller；保存会话
- 崩溃恢复：检测到 WebView2 进程/renderer 崩溃 -> 重建 controller -> 尝试 reload

### 6.2 Workspace

- 一个 Workspace 是一组 Tab + 局部 UI 状态（例如侧边栏是否打开、固定 Tab 等）
- 允许按 Workspace 保存/恢复会话

### 6.3 会话恢复

最低目标：

- 崩溃/正常退出后恢复：workspaces + tabs（URL、标题可延迟加载）
- 恢复策略：先恢复结构，再懒加载页面（避免启动卡死）

## 7. 扩展系统（重点）

### 7.1 能力边界

WebView2 的扩展能力大体跟随 Edge（Chromium），但并不保证 100% 等同 Chrome。

目标定义：

- **MVP：能装、能列、能开关**
- **M1：能固定图标、能弹 popup（如果 manifest 提供）**
- **M2：扩展权限 UI、扩展更新/卸载、options page**

### 7.2 扩展安装方式

MVP 支持两条路径：

1) **Load unpacked**
- 用户选择一个包含 `manifest.json` 的目录
- 调用 WebView2 的扩展安装 API，将其安装到当前 Profile

2) **Install from local package（可后置到 M1）**
- 支持 `.zip` 或 `.crx`（需要实现 CRX 解包）
- 解包到应用管理的扩展目录，再走 “Load unpacked”

不建议在早期做 “一键从 Chrome Web Store 安装”（稳定性、合规性与实现成本都不友好）。

### 7.3 扩展元数据与 UI

- 用 ExtensionId 做主键（来自 WebView2 API）
- 本地缓存 manifest 里的：
  - name、version、permissions、action.default_popup、icons
- QML 中显示扩展列表与启用开关
- 扩展工具栏（M1）：
  - 支持 pinned 列表
  - 点击图标：打开扩展 popup（一个独立顶层窗口，内部是一个新的 WebView2 实例，导航到 `chrome-extension://{id}/{popupPath}`）

### 7.4 Profile 与扩展隔离

建议最小化复杂度：

- 默认单 Profile（一个 userDataFolder）
- 后续再加：InPrivate Profile（不持久化历史/扩展状态）
- 如果要多 Profile：扩展安装是 Profile 级别，要设计好“不同 Profile 的扩展集”与 UI 切换逻辑

## 8. 权限、下载与安全

### 8.1 权限模型

- WebView2 触发权限请求事件 -> Core 查询站点权限表 -> 弹窗询问 -> 记录决策
- 支持 “仅本次 / 永久允许 / 永久拒绝”（M1）

### 8.2 下载

- WebView2 下载事件 -> DownloadsModel
- 默认下载目录可配置
- 提供最小的 UI：进度、暂停/继续、打开文件/文件夹

### 8.3 安全与隐私

- 依赖 WebView2 的沙箱/站点隔离基础能力
- XBrowser 侧：
  - 默认禁用一些高风险开关（例如不必要的远程调试端口）
  - 对外暴露的自定义协议/JS Bridge 必须做白名单与权限控制（早期先不做或极简）

## 9. 工程结构（建议）

建议从第一天就把“UI/核心/引擎适配”拆开，避免后期换引擎或重构失控：

```
XBrowser/
  docs/
    DESIGN.md
  src/
    app/              # main() / 启动 / 单实例 / 崩溃恢复入口
    core/             # tab/workspace/session/settings/downloads/history/bookmarks
    engine/           # IWebContents 抽象 + WebView2 实现
    extensions/       # 扩展管理（安装/枚举/启用/禁用/元数据缓存）
    platform/win/     # Windows-specific glue（可后置）
  ui/
    qml/              # QML 模块：BrowserWindow、VerticalTabs、Sidebar、Dialogs 等
    assets/           # 图标、主题资源
```

## 10. 构建、打包与更新

### 10.1 构建

- CMake + MSVC
- CI：拉取 Qt（或使用缓存），拉取 WebView2 SDK（NuGet），编译产物

### 10.2 WebView2 Runtime 策略

推荐默认 Evergreen：

- 优点：内核更新几乎“自动跟进”
- 缺点：在极少数企业环境可能被策略限制

备用方案（后置）：Fixed Version Runtime（随安装包带一份 Runtime，体积更大，但版本可控）。

### 10.3 打包

MVP 建议先解决“可交付”：

- 先用一个轻量 installer（MSIX 或 NSIS 二选一）
- 后续再做自动更新与签名

## 11. 里程碑与交付物

### M0（1~2 周：跑通骨架）

- Qt/QML 主窗口 + WebView2 显示网页
- 地址栏/前进后退/刷新
- TabModel + 垂直标签栏 UI（最小可用）

### M1（2~4 周：可日用）

- 会话恢复、下载、权限弹窗
- 扩展：Load unpacked + 列表 + 启用/禁用
- 设置页（基础）

### M2（4~8 周：Zen 化体验）

- 工作区完善、分屏、侧边栏
- 扩展工具栏与 popup
- 主题系统与动效

## 12. 风险清单（提前规避）

- **WebView2 与 Qt Quick 叠层（空域问题）**：MVP 用顶层窗口规避，后续评估 CompositionController。
- **扩展兼容性差异**：明确“支持范围”，优先确保主流扩展（密码管理/广告拦截/脚本管理）可用。
- **启动与会话恢复性能**：坚持懒加载与分阶段恢复，避免一次性恢复 N 个 Tab 卡死。
- **调试复杂度**：引擎事件、下载、权限、扩展都要做日志与诊断开关（M1 必做）。

## 13. 未决问题（你需要确认）

1) 扩展目标：优先“能装能用”还是优先“工具栏 + popup 完整体验”？
2) 要不要做“多 Profile”（类似 Chrome/Edge）？还是先单 Profile + InPrivate？
3) Zen 的哪些特性是你最想复刻的 Top 3（决定 M2 的资源投入）？
