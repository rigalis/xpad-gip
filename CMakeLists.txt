cmake_minimum_required(VERSION 3.20)
project(xpad-gip VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
pkg_check_modules(FMT REQUIRED fmt)
find_package(nlohmann_json REQUIRED)

add_executable(xpad-gip src/main.cpp src/decoder.cpp)

target_include_directories(xpad-gip PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${LIBUSB_INCLUDE_DIRS}
    ${FMT_INCLUDE_DIRS})

target_link_libraries(xpad-gip PRIVATE
    ${LIBUSB_LIBRARIES}
    ${FMT_LIBRARIES}
    nlohmann_json::nlohmann_json)