qt_add_executable(xbrowser
  WIN32
  app/main.cpp
  core/AppPaths.cpp
  core/AppSettings.cpp
  core/BrowserController.cpp
  core/BookmarksFilterModel.cpp
  core/BookmarksStore.cpp
  core/CommandBus.cpp
  core/DiagnosticsController.cpp
  core/DownloadModel.cpp
  core/ExtensionsStore.cpp
  core/HistoryFilterModel.cpp
  core/HistoryStore.cpp
  core/LayoutController.cpp
  core/ModsModel.cpp
  core/NotificationCenter.cpp
  core/OmniboxUtils.cpp
  core/QuickLinksModel.cpp
  core/SessionStore.cpp
  core/SitePermissionsStore.cpp
  core/SplitViewController.cpp
  core/TabFilterModel.cpp
  core/TabGroupModel.cpp
  core/TabModel.cpp
  core/TabSwitcherModel.cpp
  core/ShortcutStore.cpp
  core/SourceViewerHelper.cpp
  core/ThemeController.cpp
  core/ThemePackModel.cpp
  core/ToastController.cpp
  core/WebPanelsStore.cpp
  core/WorkspaceModel.cpp
  engine/webview2/WebView2View.cpp
  engine/webview2/WebView2CookieModel.cpp
  engine/webview2/BrowserExtensionsModel.cpp
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

if(WIN32)
  option(XBROWSER_AUTO_DEPLOY_QT "Run windeployqt after building xbrowser so the exe is runnable without a Qt PATH." ON)

  if(XBROWSER_AUTO_DEPLOY_QT)
    set(_qt_bin "")
    if(Qt6_DIR)
      get_filename_component(_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
      set(_qt_bin "${_qt_prefix}/bin")
    endif()

    find_program(XBROWSER_WINDEPLOYQT_EXECUTABLE NAMES windeployqt.exe windeployqt HINTS "${_qt_bin}")

    if(XBROWSER_WINDEPLOYQT_EXECUTABLE)
      add_custom_command(TARGET xbrowser POST_BUILD
        COMMAND "${XBROWSER_WINDEPLOYQT_EXECUTABLE}"
          $<$<CONFIG:Debug>:--debug>
          --force
          --no-translations
          --compiler-runtime
          --qmldir "${CMAKE_SOURCE_DIR}/ui/qml"
          "$<TARGET_FILE:xbrowser>"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${CMAKE_SOURCE_DIR}/cmake/qt.conf"
          "$<TARGET_FILE_DIR:xbrowser>/qt.conf"
        VERBATIM
      )
    else()
      message(WARNING "XBROWSER_AUTO_DEPLOY_QT is ON but windeployqt.exe was not found (Qt6_DIR='${Qt6_DIR}'). Use scripts/run.ps1 or disable XBROWSER_AUTO_DEPLOY_QT.")
    endif()
  endif()
endif()
