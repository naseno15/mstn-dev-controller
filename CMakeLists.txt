cmake_minimum_required(VERSION 3.21)
set(CMAKE_OSX_DEPLOYMENT_TARGET 12.0 CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "") # universal .app (Apple silicon + Intel)
project(mstnDevController VERSION 1.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Full static CRT (/MT) by default — no vcruntime/msvcp beside the exe.
option(MSTN_STATIC "Static MSVC CRT + WebView2LoaderStatic (no Loader DLL)" ON)
if(MSTN_STATIC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

set(MSTN_COMMON_SRC
  src/usb_type2.cpp
  src/decode_type2.cpp
  src/keyboard_out.cpp
  src/mmf_bridge.cpp
  src/profile.cpp
  src/bridge_js.cpp
  src/app_state.cpp
)

add_executable(check_decode
  checks/check_decode.cpp
  src/decode_type2.cpp
)
target_include_directories(check_decode PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(check_feedback_pkt checks/check_feedback_pkt.cpp)

if(MSVC)
  target_compile_options(check_decode PRIVATE /utf-8)
  target_compile_options(check_feedback_pkt PRIVATE /utf-8)
endif()

if(APPLE)
  enable_language(OBJCXX)
  set(MSTN_CODESIGN_ID "-" CACHE STRING "codesign identity for mstnCon.app (- = ad-hoc)")

  # Shared sources reach AppKit / IOUSBHost behind __APPLE__.
  set_source_files_properties(src/usb_type2.cpp src/keyboard_out.cpp src/bridge_js.cpp
    PROPERTIES LANGUAGE OBJCXX)

  set(_icns "${CMAKE_BINARY_DIR}/mstnCon.icns")
  add_custom_command(OUTPUT "${_icns}"
    COMMAND sips -s format icns "${CMAKE_SOURCE_DIR}/assets/mstnCon-icon.png" --out "${_icns}"
    DEPENDS assets/mstnCon-icon.png)
  set_source_files_properties("${_icns}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

  add_executable(mstnDevController MACOSX_BUNDLE src/main_mac.mm ${MSTN_COMMON_SRC} "${_icns}")
  target_include_directories(mstnDevController PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_compile_definitions(mstnDevController PRIVATE MSTN_VERSION_STR="${PROJECT_VERSION}")
  target_compile_options(mstnDevController PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
  set_target_properties(mstnDevController PROPERTIES
    OUTPUT_NAME mstnCon
    MACOSX_BUNDLE_BUNDLE_NAME mstnCon
    MACOSX_BUNDLE_GUI_IDENTIFIER com.mstn.devcontroller
    MACOSX_BUNDLE_ICON_FILE mstnCon
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
  )
  target_link_libraries(mstnDevController PRIVATE
    "-framework Cocoa"
    "-framework WebKit"
    "-framework IOKit"
    "-framework IOUSBHost"
    "-framework UniformTypeIdentifiers"
  )
  # Profiles are seeded from Resources into ~/Library/Application Support/mstnCon at first run.
  add_custom_command(TARGET mstnDevController POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_SOURCE_DIR}/ui" "$<TARGET_BUNDLE_CONTENT_DIR:mstnDevController>/Resources/ui"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_SOURCE_DIR}/profiles" "$<TARGET_BUNDLE_CONTENT_DIR:mstnDevController>/Resources/profiles"
    COMMAND codesign --force --sign "${MSTN_CODESIGN_ID}" "$<TARGET_BUNDLE_DIR:mstnDevController>"
    VERBATIM
  )

  add_executable(check_mac_keys checks/check_mac_keys.cpp src/profile.cpp src/decode_type2.cpp)
  target_include_directories(check_mac_keys PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  add_executable(check_mmf_telemetry checks/check_mmf_telemetry.cpp src/mmf_bridge.cpp)
  target_include_directories(check_mmf_telemetry PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  return()
endif()

enable_language(RC)
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
  ${MSTN_COMMON_SRC}
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
