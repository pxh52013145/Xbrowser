# Zen Build/Packaging Notes (XBrowser)

This file closes the “build/packaging” reference items in `reference/logs/zen_browser_feature_impl_plan.csv` that are **Zen-specific** (Firefox build system) but still useful when porting ideas to XBrowser.

## ZEN-001 — Build integration via `moz.build`

Zen is integrated at **build time** (not a runtime-only injection):

- `reference/zen-browser-src/desktop-dev/src/zen/moz.build`
  - Adds `EXTRA_PP_COMPONENTS += ["ZenComponents.manifest"]`
  - Adds `DIRS += [...]` to include Zen submodules (e.g. `common`, `glance`, `mods`, `sessionstore`, etc.)

XBrowser equivalent:

- Build-time integration is handled by **CMake** + **Qt resources** (`ui/ui.qrc`) + QML imports.

## ZEN-002 — Resource packaging into `browser.jar`

Zen maps resources to stable `chrome://browser/...` URLs via jar manifests:

- `reference/zen-browser-src/desktop-dev/src/browser/base/content/zen-assets.jar.inc.mn`
  - Includes many Zen module `jar.inc.mn` files (e.g. `zen/common/jar.inc.mn`, `zen/tabs/jar.inc.mn`, `zen/workspaces/jar.inc.mn`, etc.)
- `reference/zen-browser-src/desktop-dev/src/browser/base/content/zen-assets.inc.xhtml`
  - Loads global Zen styles/scripts via `chrome://browser/content/zen-*` URLs (theme/sidebar/tabs/omnibox/workspaces/split-view, etc.)

XBrowser equivalent:

- UI assets live in QML/QRC (`ui/qml/**`, `ui/ui.qrc`).
- “Mods/Themes” are applied via runtime theme tokens (`src/core/ThemePackModel.*`, `src/core/ThemeController.*`), not via `browser.jar`.
