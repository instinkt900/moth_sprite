cmake_minimum_required(VERSION 3.27)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/version.txt" MOTH_SPRITE_VERSION_FULL)
string(STRIP "${MOTH_SPRITE_VERSION_FULL}" MOTH_SPRITE_VERSION_FULL)
string(REGEX REPLACE "[-+].*$" "" MOTH_SPRITE_VERSION "${MOTH_SPRITE_VERSION_FULL}")

project(moth_sprite VERSION ${MOTH_SPRITE_VERSION} DESCRIPTION "A sprite sheet and animation clip editor for moth")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(external/nativefiledialog nativefiledialog)
# nfd_gtk.c disables a GCC-only warning that clang does not recognise.
target_compile_options(nativefiledialog PRIVATE $<$<C_COMPILER_ID:Clang>:-Wno-unknown-warning-option>)

set(SOURCES
	src/main.cpp
	src/sprite_application.cpp
	src/sprite_editor/sprite_editor.cpp
	src/sprite_editor/sprite_editor_clips.cpp
	src/sprite_editor/sprite_editor_frames.cpp
	src/sprite_editor/sprite_editor_io.cpp
	src/sprite_editor/sprite_editor_preview.cpp
	src/sprite_editor/sprite_editor_tools.cpp
	src/sprite_editor/sprite_editor_undo.cpp
)

find_package(moth_bridge REQUIRED)

add_executable(${PROJECT_NAME} ${SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} moth::bridge)
target_link_libraries(${PROJECT_NAME} nativefiledialog)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/common.h)

if(MSVC)
	set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
	set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "/W3 /WX")
	target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
	set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-Wall -Werror")
endif()

option(DISABLE_CLANG_TIDY "Disable clang-tidy even in Debug/CI builds" OFF)
if(NOT DISABLE_CLANG_TIDY AND (CMAKE_BUILD_TYPE STREQUAL "Debug" OR DEFINED ENV{CI}))
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        set_target_properties(${PROJECT_NAME} PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE};--extra-arg=-Wno-ignored-gch")
    else()
        message(STATUS "clang-tidy not found, skipping")
    endif()
endif()

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
