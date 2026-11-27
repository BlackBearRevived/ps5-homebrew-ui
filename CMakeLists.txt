# ps5-homebrew-ui - Installable C++20 UI library.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.20)
project(PS5HomebrewUI VERSION 1.0.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
find_package(Threads REQUIRED)
option(HUI_USE_PS5_OPENGL "Use PS5OpenGL from the PS5 SDK instead of desktop OpenGL" OFF)
if(HUI_USE_PS5_OPENGL)
    find_package(PS5OpenGL CONFIG REQUIRED)
    set(hui_gl PS5OpenGL::OpenGL)
else()
    find_package(OpenGL REQUIRED)
    set(hui_gl OpenGL::GL)
endif()

file(GLOB hui_sources CONFIGURE_DEPENDS
    src/audio/*.cpp src/core/*.cpp src/gfx/*.cpp src/ui/*.cpp src/ui/components/*.cpp)
add_library(hui_ui STATIC ${hui_sources}
    src/third_party/stb/vorbis.c src/third_party/stb/image.c
    src/third_party/utf8proc/utf8proc.c)
add_library(hui::ui ALIAS hui_ui)
set_target_properties(hui_ui PROPERTIES OUTPUT_NAME hui-ui EXPORT_NAME ui)
target_compile_features(hui_ui PUBLIC cxx_std_20 PRIVATE c_std_11)
target_include_directories(hui_ui PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_compile_definitions(hui_ui PRIVATE GL_GLEXT_PROTOTYPES=1)
if(HUI_USE_PS5_OPENGL)
    target_compile_options(hui_ui PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti>")
endif()
target_link_libraries(hui_ui PUBLIC ${hui_gl} Threads::Threads)

install(TARGETS hui_ui EXPORT PS5HomebrewUITargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY src/audio src/core src/gfx src/ui DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp")
install(FILES src/platform/ps5/system.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/platform/ps5)
install(DIRECTORY assets/fonts assets/audio DESTINATION ${CMAKE_INSTALL_DATADIR}/ps5-homebrew-ui)
install(FILES LICENSE THIRD_PARTY_NOTICES.md DESTINATION ${CMAKE_INSTALL_DATADIR}/ps5-homebrew-ui)
install(FILES src/third_party/stb/LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/ps5-homebrew-ui/licenses RENAME stb.txt)
install(FILES src/third_party/utf8proc/LICENSE.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/ps5-homebrew-ui/licenses RENAME utf8proc.md)

set(package_dir ${CMAKE_INSTALL_LIBDIR}/cmake/PS5HomebrewUI)
install(EXPORT PS5HomebrewUITargets NAMESPACE hui:: DESTINATION ${package_dir})
configure_package_config_file(cmake/PS5HomebrewUIConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/PS5HomebrewUIConfig.cmake
    INSTALL_DESTINATION ${package_dir} PATH_VARS CMAKE_INSTALL_DATADIR)
write_basic_package_version_file(PS5HomebrewUIConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/PS5HomebrewUIConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/PS5HomebrewUIConfigVersion.cmake
    DESTINATION ${package_dir})
