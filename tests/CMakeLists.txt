function(xbrowser_add_test target_name)
  add_executable(${target_name}
    ${ARGN}

    ../src/core/AppPaths.cpp
    ../src/core/AppSettings.cpp
    ../src/core/BrowserController.cpp
    ../src/core/CommandBus.cpp
    ../src/core/ExtensionsStore.cpp
    ../src/core/NotificationCenter.cpp
    ../src/core/SessionStore.cpp
    ../src/core/SitePermissionsStore.cpp
    ../src/core/SplitViewController.cpp
    ../src/core/TabModel.cpp
    ../src/core/TabGroupModel.cpp
    ../src/core/ToastController.cpp
    ../src/core/WorkspaceModel.cpp
  )

  target_include_directories(${target_name} PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
  )

  target_link_libraries(${target_name} PRIVATE
    Qt6::Test
    Qt6::Core
    Qt6::Gui
    Qt6::Network
  )

  add_test(NAME ${target_name} COMMAND ${target_name})

  if(WIN32)
    # Ensure the test runner can locate Qt DLLs without requiring a deployment step.
    get_target_property(_qt_core_dll Qt6::Core IMPORTED_LOCATION_DEBUG)
    if(NOT _qt_core_dll)
      get_target_property(_qt_core_dll Qt6::Core IMPORTED_LOCATION_RELEASE)
    endif()
    if(NOT _qt_core_dll)
      get_target_property(_qt_core_dll Qt6::Core IMPORTED_LOCATION)
    endif()
    get_filename_component(_qt_bin_dir "${_qt_core_dll}" DIRECTORY)

    set_tests_properties(
      ${target_name}
      PROPERTIES
        ENVIRONMENT "PATH=${_qt_bin_dir};$ENV{PATH}"
    )
  endif()
endfunction()

xbrowser_add_test(xbrowser_test_tabmodel
  TestTabModel.cpp
)

xbrowser_add_test(xbrowser_test_tabswitcher
  TestTabSwitcherModel.cpp
  ../src/core/TabSwitcherModel.cpp
)

xbrowser_add_test(xbrowser_test_toast
  TestToastController.cpp
)

xbrowser_add_test(xbrowser_test_session
  TestSessionStore.cpp
)

xbrowser_add_test(xbrowser_test_notifications
  TestNotificationCenter.cpp
)

xbrowser_add_test(xbrowser_test_settings
  TestAppSettings.cpp
)

xbrowser_add_test(xbrowser_test_backclose
  TestBackClosePolicy.cpp
)

xbrowser_add_test(xbrowser_test_theme
  TestThemeSystem.cpp
  ../src/core/ThemeController.cpp
  ../src/core/ThemePackModel.cpp
)

xbrowser_add_test(xbrowser_test_downloads
  TestDownloadModel.cpp
  ../src/core/DownloadModel.cpp
)

xbrowser_add_test(xbrowser_test_workspaces
  TestWorkspaceModel.cpp
)

xbrowser_add_test(xbrowser_test_splitview
  TestSplitViewController.cpp
)

xbrowser_add_test(xbrowser_test_permissions
  TestSitePermissionsStore.cpp
)

xbrowser_add_test(xbrowser_test_extensions_store
  TestExtensionsStore.cpp
)

xbrowser_add_test(xbrowser_test_bookmarks
  TestBookmarksStore.cpp
  ../src/core/BookmarksStore.cpp
  ../src/core/BookmarksFilterModel.cpp
)

xbrowser_add_test(xbrowser_test_history
  TestHistoryStore.cpp
  ../src/core/HistoryStore.cpp
  ../src/core/HistoryFilterModel.cpp
)

xbrowser_add_test(xbrowser_test_shortcut_store
  TestShortcutStore.cpp
  ../src/core/ShortcutStore.cpp
)

xbrowser_add_test(xbrowser_test_webpanels
  TestWebPanelsStore.cpp
  ../src/core/WebPanelsStore.cpp
)
