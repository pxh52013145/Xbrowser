qt_add_executable(xbrowser
  WIN32
  app/main.cpp
  core/AppPaths.cpp
  core/AppSettings.cpp
  core/BrowserController.cpp
  core/CommandBus.cpp
  core/DownloadModel.cpp
  core/NotificationCenter.cpp
  core/QuickLinksModel.cpp
  core/SessionStore.cpp
  core/SplitViewController.cpp
  core/TabFilterModel.cpp
  core/TabGroupModel.cpp
  core/TabModel.cpp
  core/ThemeController.cpp
  core/ThemePackModel.cpp
  core/ToastController.cpp
  core/WorkspaceModel.cpp
  engine/webview2/WebView2View.cpp
  ../ui/ui.qrc
)

if(WIN32)
  target_sources(xbrowser PRIVATE
    platform/windows/WindowChromeController.cpp
    platform/windows/WindowChromeController.h
  )
endif()

target_include_directories(xbrowser PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(xbrowser PRIVATE
  Qt6::Gui
  Qt6::Qml
  Qt6::Quick
  Qt6::QuickControls2
  Qt6::Network
)

if(WIN32)
  target_link_libraries(xbrowser PRIVATE ole32 user32 shlwapi)
endif()

target_compile_definitions(xbrowser PRIVATE
  XBROWSER_VERSION="${PROJECT_VERSION}"
  UNICODE
  _UNICODE
  NOMINMAX
  WIN32_LEAN_AND_MEAN
)

include("${CMAKE_SOURCE_DIR}/cmake/WebView2Sdk.cmake")
xbrowser_setup_webview2(xbrowser)
