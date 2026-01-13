# Zen Feature Notes (XBrowser)

This repo tracks a subset of Zen Browser UI ideas in a Qt/QML + WebView2 architecture.

## Drag & Drop (ZEN-151)

Implemented in XBrowser:

- In-app tab drag to reorder (QML `DragHandler` + `DropArea` -> `BrowserController::moveTabBefore`).
- In-app drag-to-split dropzones (QML dropzones + `SplitViewController`).

Not implemented (by design, would require native work):

- OS-level drag-out (drag a tab to desktop/another app) and rich drag payloads.
- Cross-window/tab tear-off and window reparenting.
- Global drag telemetry/hooks like Zen's native drag service.

If needed later, add a Windows-native drag bridge that can:

- Export tab metadata (URL/title/icon) as standard clipboard/drag formats.
- Import dropped URLs into the tab model.
- Coordinate with WebView2 HWND hosting (airspace constraints).

## Multi-window Sync (ZEN-161)

Current state:

- Single window only.
- Session persistence is file-based (`session.json`) with debounced writes.

If multi-window is needed later:

- Decide ownership model (one shared workspace store vs per-window stores).
- Add an explicit sync layer (e.g. file watcher + conflict resolution or IPC).
- Avoid tight coupling between windows by syncing through a versioned state model instead of direct object pointers.

## Native Share / Haptics (ZEN-200)

- Share is implemented as `share-url` via `mailto:` fallback to clipboard.
- Haptics are not implemented (Windows desktop doesn’t provide a universal haptics API comparable to mobile).

