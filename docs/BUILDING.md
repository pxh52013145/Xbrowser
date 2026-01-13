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

- Visual Studio generator: `build\src\Debug\xbrowser.exe` (or `Release`)

## WebView2 SDK notes

This repo uses the **Microsoft.Web.WebView2** NuGet package for headers/libs.

- Default: CMake downloads the pinned version automatically at configure time.
- Offline: extract the NuGet package and pass `-DXBROWSER_WEBVIEW2_SDK_DIR="...\\Microsoft.Web.WebView2.<version>"`.
- To bump SDK version: set `-DXBROWSER_WEBVIEW2_PACKAGE_VERSION=...` during configure.

