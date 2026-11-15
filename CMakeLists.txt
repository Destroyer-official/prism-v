cmake_minimum_required(VERSION 3.20)
project(PrismV LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for language servers
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Android 15/16/17 Mandated 16 KB Page-Size Linker Flags
if (ANDROID OR (CMAKE_SYSTEM_NAME STREQUAL "Linux"))
    add_link_options("-Wl,-z,max-page-size=16384" "-Wl,-z,common-page-size=16384")
endif()

# Compiler Warning Flags
if (MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# Core include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/core/include)

# Core library sources
set(PRISM_CORE_SOURCES
    core/src/detint.cpp
    core/src/rans8.cpp
    core/src/bbr.cpp
    core/src/psc.cpp
    core/src/tile_pool.cpp
    core/src/coolchic.cpp
    core/src/afgs1.cpp
    core/src/container_pvloss.cpp
    core/src/isobmff.cpp
    core/src/vfs_proxy.cpp
    core/src/transaction.cpp
    core/src/dedup_rabin.cpp
    core/src/remux.cpp
    core/src/router.cpp
)

# Core library target
add_library(prism_core STATIC ${PRISM_CORE_SOURCES})
target_compile_features(prism_core PUBLIC cxx_std_20)
target_include_directories(prism_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
    $<INSTALL_INTERFACE:include>
)

set_target_properties(prism_core PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
)

# JNI Shared Library for Android
find_package(JNI QUIET)
if (ANDROID OR JNI_FOUND)
    add_library(prism_jni SHARED core/src/prism_jni.cpp ${PRISM_CORE_SOURCES})
    target_compile_features(prism_jni PUBLIC cxx_std_20)
    target_compile_definitions(prism_jni PRIVATE PRISM_ENABLE_JNI)
    target_include_directories(prism_jni PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
        $<INSTALL_INTERFACE:include>
    )
    if (JNI_FOUND)
        target_include_directories(prism_jni PRIVATE ${JNI_INCLUDE_DIRS})
    endif()
    if (ANDROID)
        target_link_libraries(prism_jni PRIVATE log android mediandk)
    endif()
endif()

# Test suite executable
set(PRISM_TEST_SOURCES
    tests/test_main.cpp
    tests/test_detint.cpp
    tests/test_rans8.cpp
    tests/test_bbr.cpp
    tests/test_psc.cpp
    tests/test_coolchic.cpp
    tests/test_afgs1.cpp
    tests/test_container.cpp
    tests/test_vfs.cpp
    tests/test_transaction.cpp
    tests/test_dedup.cpp
    tests/test_remux.cpp
    tests/test_router.cpp
)

add_executable(prism_tests ${PRISM_TEST_SOURCES})
target_link_libraries(prism_tests PRIVATE prism_core)
target_include_directories(prism_tests PRIVATE tests)

# Command-Line Application
add_executable(prism tools/prism_cli.cpp)
target_link_libraries(prism PRIVATE prism_core)

# Day-One Minimal Falsifiable Spikes
add_executable(exp_1a_bbr tools/exp_1a_bbr.cpp)
target_link_libraries(exp_1a_bbr PRIVATE prism_core)

add_executable(exp_1b_lossy tools/exp_1b_lossy.cpp)
target_link_libraries(exp_1b_lossy PRIVATE prism_core)

add_executable(verify_real_video tools/verify_real_video.cpp)
target_link_libraries(verify_real_video PRIVATE prism_core)

enable_testing()
add_test(NAME AllPrismTests COMMAND prism_tests)
add_test(NAME Exp1A_BBR COMMAND exp_1a_bbr)
add_test(NAME Exp1B_Lossy COMMAND exp_1b_lossy)

# Installation targets
include(GNUInstallDirs)
install(TARGETS prism_core prism
    EXPORT PrismTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY core/include/prism
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
