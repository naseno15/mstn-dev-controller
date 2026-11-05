cmake_minimum_required(VERSION 3.21)
project(mstnDevController VERSION 1.1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Full static CRT (/MT) by default — no vcruntime/msvcp beside the exe.
option(MSTN_STATIC "Static MSVC CRT + WebView2LoaderStatic (no Loader DLL)" ON)
if(MSTN_STATIC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# WebView2 native root (contains include/WebView2.h). Prefer vendored third_party.
if(NOT DEFINED WEBVIEW2_ROOT OR WEBVIEW2_ROOT STREQUAL "")
  set(_candidates
    "${CMAKE_SOURCE_DIR}/third_party/webview2/pkg/build/native"
    "$ENV{USERPROFILE}/.nuget/packages/microsoft.web.webview2/1.0.2903.40/build/native"
  )
  file(GLOB _nuget_globs "$ENV{USERPROFILE}/.nuget/packages/microsoft.web.webview2/*/build/native")
  list(APPEND _candidates ${_nuget_globs})
  foreach(_c IN LISTS _candidates)
    if(EXISTS "${_c}/include/WebView2.h")
      set(WEBVIEW2_ROOT "${_c}")
      break()
    endif()
  endforeach()
endif()

if(NOT EXISTS "${WEBVIEW2_ROOT}/include/WebView2.h")
  message(FATAL_ERROR
    "WebView2 SDK not found.\n"
    "  powershell -File scripts/fetch-webview2.ps1\n"
    "then reconfigure, or pass -DWEBVIEW2_ROOT=...")
endif()

message(STATUS "WebView2: ${WEBVIEW2_ROOT}")
message(STATUS "MSTN_STATIC: ${MSTN_STATIC}")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(_wv2_arch x64)
  set(_wv2_rid win-x64)
else()
  set(_wv2_arch x86)
  set(_wv2_rid win-x86)
endif()

# Prefer static loader when MSTN_STATIC; else import lib + copy DLL.
set(_wv2_lib "")
set(_wv2_dll "")
if(MSTN_STATIC)
  foreach(_cand
      "${WEBVIEW2_ROOT}/${_wv2_arch}/WebView2LoaderStatic.lib"
      "${WEBVIEW2_ROOT}/lib/${_wv2_arch}/WebView2LoaderStatic.lib")
    if(EXISTS "${_cand}")
      set(_wv2_lib "${_cand}")
      break()
    endif()
  endforeach()
  if(_wv2_lib STREQUAL "")
    message(FATAL_ERROR "WebView2LoaderStatic.lib not found under ${WEBVIEW2_ROOT} (need MSTN_STATIC=OFF or fetch SDK)")
  endif()
else()
  foreach(_cand
      "${WEBVIEW2_ROOT}/${_wv2_arch}/WebView2Loader.dll.lib"
      "${WEBVIEW2_ROOT}/lib/${_wv2_arch}/WebView2Loader.dll.lib")
    if(EXISTS "${_cand}")
      set(_wv2_lib "${_cand}")
      break()
    endif()
  endforeach()
  if(_wv2_lib STREQUAL "")
    message(FATAL_ERROR "WebView2Loader.dll.lib not found under ${WEBVIEW2_ROOT}")
  endif()
  foreach(_cand
      "${WEBVIEW2_ROOT}/../../runtimes/${_wv2_rid}/native/WebView2Loader.dll"
      "${WEBVIEW2_ROOT}/../../../runtimes/${_wv2_rid}/native/WebView2Loader.dll"
      "${WEBVIEW2_ROOT}/${_wv2_arch}/WebView2Loader.dll")
    if(EXISTS "${_cand}")
      set(_wv2_dll "${_cand}")
      break()
    endif()
  endforeach()
endif()

add_executable(mstnDevController WIN32
  src/main.cpp
  src/app_window.cpp
  src/usb_type2.cpp
  src/decode_type2.cpp
  src/keyboard_out.cpp
  src/mmf_bridge.cpp
  src/profile.cpp
  src/bridge_js.cpp
  src/app_state.cpp
  src/app.rc
)

target_include_directories(mstnDevController PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  "${WEBVIEW2_ROOT}/include"
)

target_compile_definitions(mstnDevController PRIVATE
  UNICODE _UNICODE NOMINMAX
  MSTN_VERSION_STR="${PROJECT_VERSION}"
)
target_compile_options(mstnDevController PRIVATE /W3 /EHsc /utf-8)

target_link_libraries(mstnDevController PRIVATE
  winusb
  setupapi
  user32
  ole32
  oleaut32
  uuid
  shlwapi
  shell32
  comdlg32
  version
  "${_wv2_lib}"
)

add_custom_command(TARGET mstnDevController POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/ui" "$<TARGET_FILE_DIR:mstnDevController>/ui"
  COMMAND ${CMAKE_COMMAND}
    -DSRC=${CMAKE_SOURCE_DIR}/profiles
    -DDST=$<TARGET_FILE_DIR:mstnDevController>/profiles
    -P ${CMAKE_SOURCE_DIR}/scripts/seed_profiles.cmake
  COMMAND ${CMAKE_COMMAND} -E make_directory
    "$<TARGET_FILE_DIR:mstnDevController>/ui/fonts"
)

if(_wv2_dll)
  add_custom_command(TARGET mstnDevController POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${_wv2_dll}" "$<TARGET_FILE_DIR:mstnDevController>/WebView2Loader.dll"
  )
endif()

add_executable(check_decode
  checks/check_decode.cpp
  src/decode_type2.cpp
)
target_include_directories(check_decode PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(check_decode PRIVATE /utf-8)

add_executable(check_feedback_pkt checks/check_feedback_pkt.cpp)
target_compile_options(check_feedback_pkt PRIVATE /utf-8)
