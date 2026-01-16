# XBrowser 项目规则（给 Codex/Agent）

本项目为 Windows-only：Qt 6 + QML + WebView2。

## 工作流（必须遵守）

### 1) 开发前先给计划
- 开始任何新功能/修复前，先在对话里输出一个清晰的 `plan`（按模块拆分；每个模块都要有测试方式）。
- 同步把计划条目写入 `reference/developing/*.csv`（`Status=todo` 或 `in_progress`）。

### 2) 开发完成后更新状态并再生成汇总 CSV
- 完成某条任务后，把其 `Status` 更新为 `done`。
- 运行合并脚本刷新汇总（不要手改汇总文件）：
  - `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/merge_plans.ps1 -IncludeLayout`

脚本会生成/刷新：
- `reference/zen_browser_merged_all.csv`
- `reference/zen_browser_merged_developed.csv`
- `reference/zen_browser_merged_developing.csv`
- `reference/zen_browser_merged_counts_by_area.csv`
- `reference/zen_browser_merged_areas.csv`

### 3) CSV 输入/输出约定（不要混用）
- **输入（Source of Truth）**
  - `reference/logs/*.csv`：历史归档（一般不改）
  - `reference/developing/*.csv`：当前迭代计划（要写 todo/in_progress/done）
- **输出（Generated，禁止手改）**
  - `reference/zen_browser_merged_*.csv`：均由 `scripts/merge_plans.ps1` 生成

### 4) Area 规范
- `Area` 必填；优先复用既有 Area 名称，避免拼写不一致。
- 若确实需要新增 Area：在写入 `reference/developing/*.csv` 后运行合并脚本，并检查 `reference/zen_browser_merged_areas.csv` 中已出现该 Area。

### 5) Status 规范
仅使用：
- `todo`
- `in_progress`
- `done`

## 测试与提交
- 代码变更后至少跑：`cmake --build build --config Debug` + `ctest -C Debug --output-on-failure`。
- UI/交互类改动必须在 CSV 的 `Test_Steps` 写清楚手动验证步骤，并实际自测。
- 每个模块完成且通过测试后再提交一次（避免把无关修改混进同一提交）。

