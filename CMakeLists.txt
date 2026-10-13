cmake_minimum_required(VERSION 3.25)
project(NativeIsland VERSION 0.8.7 LANGUAGES CXX RC)

if(NOT WIN32)
  message(FATAL_ERROR "NativeIsland only supports Windows 10/11")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Public installers must not depend on a separately installed Visual C++ runtime.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(NativeIsland WIN32
  src/app/NativeIsland.manifest
  src/app/version.rc
  src/app/main.cpp
  src/core/config.cpp
  src/rendering/dcomp_renderer.cpp
  src/rendering/pages.cpp
  src/telemetry/system_monitor.cpp
  src/media/media_service.cpp
  src/ui/island_window.cpp
  src/ui/settings_window.cpp
  src/services/app_launcher.cpp
  src/services/audio_service.cpp
  src/services/reminders.cpp
  src/services/windows_notifications.cpp
  src/services/google_calendar.cpp
  src/ui/reminder_window.cpp
)

target_include_directories(NativeIsland PRIVATE src)
target_compile_definitions(NativeIsland PRIVATE
  UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
  WINVER=0x0A00 _WIN32_WINNT=0x0A00
)
target_compile_options(NativeIsland PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(NativeIsland PRIVATE
  d3d11 dxgi d2d1 dwrite dcomp dwmapi shell32 shlwapi iphlpapi windowsapp windowscodecs comdlg32 comctl32 pdh setupapi oleaut32 winhttp ws2_32 crypt32 bcrypt userenv
)

include(CTest)
if(BUILD_TESTING)
  add_executable(NativeIslandTests tests/test_core.cpp src/core/config.cpp src/services/app_launcher.cpp src/services/reminders.cpp src/services/google_calendar.cpp src/services/windows_notifications.cpp)
  target_include_directories(NativeIslandTests PRIVATE src)
  target_compile_definitions(NativeIslandTests PRIVATE UNICODE _UNICODE NOMINMAX)
  target_compile_definitions(NativeIslandTests PRIVATE NI_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(NativeIslandTests PRIVATE shell32 shlwapi windowscodecs windowsapp winhttp ws2_32 crypt32 bcrypt userenv oleaut32)
  target_compile_options(NativeIslandTests PRIVATE /W4 /permissive- /EHsc /utf-8)
  add_test(NAME core COMMAND NativeIslandTests)
endif()

install(TARGETS NativeIsland RUNTIME DESTINATION .)
install(FILES LICENSE DESTINATION .)
install(FILES assets/LUCIDE-LICENSE.txt DESTINATION .)
install(FILES docs/google-calendar-setup.md DESTINATION .)
install(FILES docs/tasks-and-sound.md DESTINATION .)

