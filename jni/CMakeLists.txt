# ============================================================
# CMakeLists.txt — NDK build
# ============================================================
cmake_minimum_required(VERSION 3.22)
project(ff_cheat)

set(CMAKE_CXX_STANDARD 17)
set(ANDROID_ABI arm64-v8a)
set(ANDROID_PLATFORM android-29)

add_library(ff_cheat SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/cheat_bridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/anticheat_bypass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cheat_main.cpp
)

target_link_libraries(ff_cheat
    log android GLESv2 EGL
)

target_compile_options(ff_cheat PRIVATE
    -O3 -fvisibility=hidden
    -fstack-protector-strong
    -fno-rtti
    -DANDROID
)