# Running XBrowser (Windows)

## Recommended (development)

- Build + run: `dev.cmd` (or `scripts\\dev.cmd`, or `.\scripts\dev.ps1 -Config Debug`)
- Run existing build: `run.cmd` (or `scripts\\run.cmd`, or `.\scripts\run.ps1 -Config Debug`)
- Build + test: `powershell -NoProfile -ExecutionPolicy Bypass -File scripts\\check.ps1 -Config Debug`

If `build/` is not configured yet (missing `build\\CMakeCache.txt`), `dev.ps1`/`check.ps1` will auto-run `scripts\\configure.ps1`.
Qt is auto-detected from `Qt6_DIR` / `CMAKE_PREFIX_PATH` or common locations like `<drive>:\\Qt\\<version>\\msvc*_64` (you can also pass `-QtPrefix`).

By default, `run.ps1` launches a **staged copy** of the built exe under `build\\run\\<Config>\\...` so you can rebuild without hitting Windows file locks (e.g. `LNK1104` on `build\\<Config>\\xbrowser.exe`).
Use `.\scripts\run.ps1 -InPlace` if you specifically want to run `build\\<Config>\\xbrowser.exe` directly (this will lock the exe while it is running).

`run.ps1 -InPlace` auto-deploys the Qt runtime (via `windeployqt`) if the build output is missing Qt DLLs/plugins (checks `platforms\\qwindows*.dll` + QML folders).

If you double-click the `.cmd` launchers and something fails, the window will pause so you can read the error.

Note: when `XBROWSER_AUTO_DEPLOY_QT=ON`, the build runs `windeployqt` after linking `xbrowser`. If you run the build output exe in-place (`build\\Debug\\xbrowser.exe`) and then rebuild, the link step can fail with `LNK1104` (Windows locks running executables). Prefer `dev.cmd` / staged `run.ps1`, or close `xbrowser.exe` before rebuilding.

## Direct exe (double-clickable)

1. Build once: `cmake --build build --config Debug`
2. Ensure Qt runtime is deployed next to the exe:
   - Default: the build runs `windeployqt` automatically (`XBROWSER_AUTO_DEPLOY_QT=ON`)
   - Manual (one-time): `scripts\\deploy.cmd` (or `.\scripts\deploy.ps1 -Config Debug`)
3. Run: `build\\Debug\\xbrowser.exe`

## Profiles & incognito

- Run with a named profile (separate data dir): `.\scripts\run.ps1 -Config Debug -Args @('--profile','dev')`
- Run in incognito mode (temporary data dir, cleaned on exit): `.\scripts\run.ps1 -Config Debug -Args @('--incognito')`

In-app shortcuts:
- New window: `Ctrl+N` (starts a new process with a fresh profile id)
- New incognito window: `Ctrl+Shift+N`

## Visual Studio

- Open `build\\XBrowser.sln`
- Set `xbrowser` as the startup project, then F5

## In-app entry points

- Extensions: **Tools > Extensions** (or type `> open-extensions` in the address bar command mode)

## Logs & common runtime issues

- Log file: `%LOCALAPPDATA%\\XBrowser\\XBrowser\\xbrowser.log`
- Web engine unavailable: install **WebView2 Runtime (Evergreen)**, then click **Retry** in-app.
- Missing Qt6*.dll / platform plugin: run `scripts\\run.cmd` (auto-fix) or `scripts\\deploy.cmd` (one-time).
