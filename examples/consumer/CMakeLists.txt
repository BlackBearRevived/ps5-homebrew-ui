# ps5-homebrew-ui - External sample built against the installed package.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.20)
project(HUIConsumer LANGUAGES CXX)
find_package(PS5HomebrewUI 1.0 CONFIG REQUIRED)
find_package(OpenGL REQUIRED COMPONENTS OpenGL EGL)
add_executable(hui_consumer main.cpp)
target_compile_definitions(hui_consumer PRIVATE GL_GLEXT_PROTOTYPES=1)
target_link_libraries(hui_consumer PRIVATE hui::ui OpenGL::EGL)
enable_testing()
add_test(NAME consumer_flow COMMAND hui_consumer "${PS5HomebrewUI_ASSET_DIR}")
set_tests_properties(consumer_flow PROPERTIES
    ENVIRONMENT "EGL_PLATFORM=surfaceless;LIBGL_ALWAYS_SOFTWARE=1")
