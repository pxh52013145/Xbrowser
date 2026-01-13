include_guard(GLOBAL)

function(xbrowser_setup_webview2 target_name)
  set(XBROWSER_WEBVIEW2_PACKAGE_VERSION "1.0.3650.58" CACHE STRING "Microsoft.Web.WebView2 NuGet package version")
  set(XBROWSER_WEBVIEW2_SDK_DIR "" CACHE PATH "Optional path to an extracted Microsoft.Web.WebView2 NuGet package root")

  if(XBROWSER_WEBVIEW2_SDK_DIR)
    set(_sdk_root "${XBROWSER_WEBVIEW2_SDK_DIR}")
  else()
    set(_download_dir "${CMAKE_BINARY_DIR}/_deps/webview2sdk")
    set(_sdk_root "${_download_dir}/Microsoft.Web.WebView2.${XBROWSER_WEBVIEW2_PACKAGE_VERSION}")

    if(NOT EXISTS "${_sdk_root}/build/native/include/WebView2.h")
      file(MAKE_DIRECTORY "${_download_dir}")
      set(_nupkg "${_download_dir}/Microsoft.Web.WebView2.${XBROWSER_WEBVIEW2_PACKAGE_VERSION}.nupkg")
      set(_url "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${XBROWSER_WEBVIEW2_PACKAGE_VERSION}")
      message(STATUS "Downloading WebView2 SDK ${XBROWSER_WEBVIEW2_PACKAGE_VERSION}...")
      file(DOWNLOAD "${_url}" "${_nupkg}" SHOW_PROGRESS TLS_VERIFY ON)
      file(MAKE_DIRECTORY "${_sdk_root}")
      file(ARCHIVE_EXTRACT INPUT "${_nupkg}" DESTINATION "${_sdk_root}")
    endif()
  endif()

  if(NOT EXISTS "${_sdk_root}/build/native/include/WebView2.h")
    message(FATAL_ERROR "WebView2 SDK not found (expected ${_sdk_root}/build/native/include/WebView2.h). Set XBROWSER_WEBVIEW2_SDK_DIR or check network access.")
  endif()

  set(_inc "${_sdk_root}/build/native/include")
  set(_bin "${_sdk_root}/build/native/x64")

  if(EXISTS "${_bin}/WebView2LoaderStatic.lib")
    add_library(WebView2Loader STATIC IMPORTED GLOBAL)
    set_target_properties(WebView2Loader PROPERTIES
      IMPORTED_LOCATION "${_bin}/WebView2LoaderStatic.lib"
      INTERFACE_INCLUDE_DIRECTORIES "${_inc}"
    )
  else()
    add_library(WebView2Loader SHARED IMPORTED GLOBAL)
    set_target_properties(WebView2Loader PROPERTIES
      IMPORTED_IMPLIB "${_bin}/WebView2Loader.lib"
      IMPORTED_LOCATION "${_bin}/WebView2Loader.dll"
      INTERFACE_INCLUDE_DIRECTORIES "${_inc}"
    )

    add_custom_command(TARGET "${target_name}" POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${_bin}/WebView2Loader.dll"
        "$<TARGET_FILE_DIR:${target_name}>/WebView2Loader.dll"
      VERBATIM
    )
  endif()

  add_library(WebView2::Loader ALIAS WebView2Loader)
  target_link_libraries("${target_name}" PRIVATE WebView2::Loader)
endfunction()

