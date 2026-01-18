if(NOT DEFINED WINDEPLOYQT OR WINDEPLOYQT STREQUAL "")
  message(FATAL_ERROR "WINDEPLOYQT is not set.")
endif()

if(NOT DEFINED EXE OR EXE STREQUAL "")
  message(FATAL_ERROR "EXE is not set.")
endif()

if(NOT DEFINED CONFIG)
  set(CONFIG "")
endif()

if(NOT DEFINED QMLDIR)
  set(QMLDIR "")
endif()

if(NOT DEFINED TARGET_DIR)
  set(TARGET_DIR "")
endif()

if(NOT DEFINED QTCONF_SOURCE)
  set(QTCONF_SOURCE "")
endif()

set(_args "")
if(CONFIG STREQUAL "Debug")
  list(APPEND _args "--debug")
elseif(CONFIG STREQUAL "Release")
  list(APPEND _args "--release")
endif()

list(APPEND _args
  "--force"
  "--no-translations"
  "--compiler-runtime"
)

if(NOT QMLDIR STREQUAL "")
  list(APPEND _args "--qmldir" "${QMLDIR}")
endif()

list(APPEND _args "${EXE}")

execute_process(
  COMMAND "${WINDEPLOYQT}" ${_args}
  RESULT_VARIABLE _res
  OUTPUT_VARIABLE _out
  ERROR_VARIABLE _err
)

if(NOT _res EQUAL 0)
  set(_combined "")
  if(NOT _out STREQUAL "")
    string(APPEND _combined "${_out}\n")
  endif()
  if(NOT _err STREQUAL "")
    string(APPEND _combined "${_err}\n")
  endif()

  string(LENGTH "${_combined}" _combined_len)
  if(_combined_len GREATER 4000)
    string(SUBSTRING "${_combined}" 0 4000 _combined_short)
    set(_combined "${_combined_short}\n... (truncated)")
  endif()

  if(CONFIG STREQUAL "Debug")
    message(WARNING "windeployqt failed (ignored for Debug; code=${_res}).\n${_combined}")
  else()
    message(FATAL_ERROR "windeployqt failed (code=${_res}).\n${_combined}")
  endif()
endif()

if(NOT TARGET_DIR STREQUAL "" AND NOT QTCONF_SOURCE STREQUAL "" AND EXISTS "${QTCONF_SOURCE}")
  file(COPY_FILE "${QTCONF_SOURCE}" "${TARGET_DIR}/qt.conf" ONLY_IF_DIFFERENT)
endif()
