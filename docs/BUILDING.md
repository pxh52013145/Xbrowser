# Building XBrowser (Windows)

## Prerequisites

- Windows 10/11 (x64)
- Visual Studio 2022 (Desktop development with C++)
- CMake 3.24+
- Qt 6.6+ (MSVC x64)
- WebView2 Runtime (Evergreen)

## Configure & Build (Visual Studio generator)

From a Developer PowerShell:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:\Qt\6.6.2\msvc2019_64"
cmake --build build --config Debug
```

Run the app (path depends on generator/config):

- Visual Studio generator: `build\Debug\xbrowser.exe` (or `Release`)

By default, the build runs `windeployqt` after linking `xbrowser`, so the exe should be runnable directly (double-clickable) without setting Qt on `PATH`.
You can disable this if you want faster builds: `-DXBROWSER_AUTO_DEPLOY_QT=OFF`.

If you see missing Qt DLL errors (e.g. `Qt6QuickControls2d.dll`), you need either:

- Run from a shell with Qt on `PATH` (dev workflow): `.\scripts\run.ps1 -Config Debug` (or double-click `scripts\run.cmd`)
- Or deploy Qt runtime next to the exe (double-clickable): `.\scripts\deploy.ps1 -Config Debug` (or double-click `scripts\deploy.cmd`)

## Packaging (portable folder)

To create a self-contained runnable folder (Qt DLLs + plugins + compiler runtime), run:

```powershell
.\scripts\package.ps1 -Config Release
```

Or double-click: `scripts\\package.cmd`

Output:

- `dist\\Release\\xbrowser.exe`

## WebView2 SDK notes

This repo uses the **Microsoft.Web.WebView2** NuGet package for headers/libs.

- Default: CMake downloads the pinned version automatically at configure time.
- Offline: extract the NuGet package and pass `-DXBROWSER_WEBVIEW2_SDK_DIR="...\\Microsoft.Web.WebView2.<version>"`.
- To bump SDK version: set `-DXBROWSER_WEBVIEW2_PACKAGE_VERSION=...` during configure.
