# XBrowser 对齐 Zen Browser 评估（MVP 阶段）

本文聚焦当前已落地的 Zen 对齐项（窗口布局/侧栏、Compact Mode、Single Toolbar、弹层稳定性、omnibox 性能等），从“体验一致性、性能、可回归测试”三个维度给出评估与改进建议。

## 已实现（对齐度：高）

### 1) Window/Layout：侧栏右侧模式
- 已支持 `sidebarOnRight`，侧栏/分隔线方向与 Web Panel 布局可镜像。
- 具备持久化与单测覆盖（`TestAppSettings`）。

### 2) Core/UI：LayoutController 收敛布局状态
- `showTopBar/showSidebar/sidebarIconOnly` 由 `LayoutController` 统一计算。
- CompactMode 的 hover/focus/popupContext 对可见性影响已纳入，并有单测覆盖（`TestLayoutController`）。

### 3) Single Toolbar：地址栏移入侧栏（条件受控）
- `useSingleToolbar` 开启且侧栏展开、非 icon-only 时，顶部地址栏收敛到侧栏地址栏；`Ctrl+L` 能聚焦到“当前有效地址栏”。
- 与 CompactMode/侧栏开关的联动已有防护，避免无侧栏时无地址入口。

### 4) Layering/Popups：CompactMode 弹层锚点稳定
- 顶栏相关弹层（main menu/downloads/site panel/tab switcher/extensions panel/extension popup/permission doorhanger）打开后不会因 CompactMode hover 丢失导致顶栏瞬间折叠。
- 补齐了侧栏/工具栏右键菜单、站点权限二级面板、emoji picker、workspace 菜单/重命名等 context，降低锚点消失/跳动风险。
- Find in Page（Find Bar）打开时顶栏保持展开，避免工具窗锚点依赖顶栏高度导致的跳动。

## 已补齐（对齐度：高）

### 1) Motion/动效一致性
- TopBar 高度在 `showTopBar` 切换时增加短动画并尊重 `reduceMotion`，CompactMode 下展开/收起更接近 Zen 的主观手感。
- CompactMode 顶部/侧栏 hover 进入/退出时序对齐 UX_SPEC（进入 120ms，退出 300ms），降低误触与抖动。

### 2) PopupContext 规范化程度
现状：
- 已通过字符串 `popupManagerContext` 让 `LayoutController` 识别“保持展开”的场景，但 context 分散在多个点击点。

建议：
- 长期可考虑为 PopupManager 提供统一的 `openWithContext()`/`toggleWithContext()` 辅助，减少遗漏与重复逻辑（本轮先不重构到位，避免牵一发动全身）。

## 需要重点优化（对齐度：低，性能优先）

### 1) Omnibox 更新策略（性能）
已落地：
- QML 层增加短延迟防抖（debounce），减少每次按键重算。
- 将 tab/workspace 命中与排序逻辑迁移到 C++（`OmniboxUtils`）并纳入单测。
- 进一步减少分配：`matchRange` 使用 case-insensitive 搜索避免 `toLower()`；`tabSuggestions` 仅对可能入榜候选计算高亮范围并尽量延后 URL 字符串化。

后续可选：
- 若需要进一步压榨输入延迟，可评估更细粒度的增量匹配/缓存（例如 query 递增时复用上一次结果）。

## 测试覆盖评估

已覆盖：
- `AppSettings` 关键设置持久化（含 sidebarOnRight/useSingleToolbar）。
- `LayoutController` 在 compactMode + popup contexts 下的 `showTopBar/showSidebar` 逻辑。

缺口（本轮补齐）：
- Omnibox 匹配与排序逻辑已补齐回归测试（含 trim/URL-only/title-empty 场景），并已纳入 `ctest`。
- LayoutController 增补 addressBarVisible/singleToolbar fallback 等边界用例，降低组合状态回归风险。

## 结论（MVP 对齐建议）
- 体验层面：当前对齐的“结构与交互”已基本满足 MVP，下一步优先补齐 TopBar 动效一致性与 Omnibox 性能短板。
- 工程层面：将高频、可回归的逻辑（如 omnibox 匹配/排序）下沉到 C++ 并加单测，是对齐 Zen 的“稳定性/性能”标准的性价比最高路径。
