cmake_minimum_required(VERSION 3.16)
project(dashcam LANGUAGES CXX)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET
    gstreamer-1.0
    gstreamer-app-1.0
    gstreamer-video-1.0
)

add_executable(dashcam src/main.cpp)

target_compile_features(dashcam PRIVATE cxx_std_17)
set_target_properties(dashcam PROPERTIES CXX_EXTENSIONS OFF)

target_compile_options(dashcam PRIVATE
    -Wall -Wextra -Wpedantic
)

target_link_libraries(dashcam PRIVATE
    PkgConfig::GSTREAMER
)
