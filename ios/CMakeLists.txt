cmake_minimum_required(VERSION 3.24)
set(CMAKE_OSX_ARCHITECTURES arm64)
set(CMAKE_OSX_DEPLOYMENT_TARGET 15.0)
project(JMFlexModIOS LANGUAGES C CXX OBJCXX ASM)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    message(FATAL_ERROR "Configure with -DCMAKE_SYSTEM_NAME=iOS and an iPhoneOS SDK")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui_impl_metal.mm" OR
   NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui_impl_metal.h")
    message(FATAL_ERROR "The pinned Dear ImGui v1.90.4 Metal backend is missing")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/Dobby/CMakeLists.txt")
    message(FATAL_ERROR "The pinned Dobby source is missing")
endif()
set(Plugin.SymbolResolver OFF CACHE BOOL "" FORCE)
set(DOBBY_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(DOBBY_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(DOBBY_BUILD_SILICON ON CACHE BOOL "" FORCE)
set(DOBBY_LIBRARY_TYPE STATIC CACHE STRING "" FORCE)
add_subdirectory(vendor/Dobby dobby-build EXCLUDE_FROM_ALL)

add_library(jmflexmod_ios SHARED
    Main.mm Platform.mm Auth.mm Hooks.mm Cheat.cpp Menu.cpp
    ../jni/vendor/monocypher/monocypher.c
    ../jni/vendor/monocypher/monocypher-ed25519.c
    ../jni/imgui/imgui.cpp ../jni/imgui/imgui_draw.cpp
    ../jni/imgui/imgui_tables.cpp ../jni/imgui/imgui_widgets.cpp
    vendor/imgui_impl_metal.mm)
target_compile_definitions(jmflexmod_ios PRIVATE JM_IOS=1)
target_compile_options(jmflexmod_ios PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
target_include_directories(jmflexmod_ios PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor" "${CMAKE_CURRENT_SOURCE_DIR}/../jni"
    "${CMAKE_CURRENT_SOURCE_DIR}/../jni/imgui")
target_link_libraries(jmflexmod_ios PRIVATE dobby_static
    "-framework Foundation" "-framework UIKit" "-framework Metal"
    "-framework MetalKit" "-framework CoreGraphics" "-framework QuartzCore")
set_target_properties(jmflexmod_ios PROPERTIES
    OUTPUT_NAME "JMFlexMod" SUFFIX ".dylib"
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO)
