cmake_minimum_required(VERSION 3.20)
project(AegisX CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(AegisX_ClientGuard WIN32
    src/client_main.cpp
    src/hook_detector.cpp
    src/hook_detector.h
    src/external_detector.cpp
    src/external_detector.h
    src/anti_debug.cpp
    src/anti_debug.h
    src/client_fow.cpp
    src/client_fow.h
    src/behavior_ac.cpp
    src/behavior_ac.h
    src/memory_guard.cpp
    src/memory_guard.h
    src/kernel_guard.cpp
    src/kernel_guard.h
    src/ml_behavior_engine.cpp
    src/ml_behavior_engine.h
    src/dma_shield.cpp
    src/dma_shield.h
    src/hypervisor_detector.cpp
    src/hypervisor_detector.h
    src/ai_cv_detector.cpp
    src/ai_cv_detector.h
    src/leetify_analyzer.cpp
    src/leetify_analyzer.h
    src/anti_tamper.cpp
    src/anti_tamper.h
    src/watchdog.cpp
    src/watchdog.h
    src/gui_window.cpp
    src/gui_window.h
    res/resource.rc
)

target_include_directories(AegisX_ClientGuard PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(AegisX_ClientGuard PRIVATE
    psapi
    advapi32
    user32
    gdi32
    gdiplus
    dwmapi
    shell32
    setupapi
)
