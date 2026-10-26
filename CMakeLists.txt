cmake_minimum_required(VERSION 3.16)
project(PlutoSDR CXX)

set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Опция для включения или отключения установки зависимостей
option(INSTALL_DEPS "Установить зависимости" ON)
option(PLUTO_TIMESTAMP "Build pluto with timestamp" ON)

# RF frontend driver
find_package(SoapySDR REQUIRED)


set(MAIN_SOURCE_FILES
    src/main.cpp
    src/configure_sdr.cpp
    src/transmitter.cpp
)

# Добавляем исполняемый файл
add_executable(main ${MAIN_SOURCE_FILES})

target_include_directories(main PRIVATE headers)
target_link_libraries(main ${SoapySDR_LIBRARIES})
