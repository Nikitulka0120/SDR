cmake_minimum_required(VERSION 3.16)
project(PlutoSDR C)

set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Опция для включения или отключения установки зависимостей
option(INSTALL_DEPS "Установить зависимости" ON)
option(PLUTO_TIMESTAMP "Build pluto with timestamp" ON)

# RF frontend driver
find_package(SoapySDR REQUIRED)

# Подключение заголовочных файлов
include_directories(headers)

set(MAIN_SOURCE_FILES
    src/main.c
    src/configure_sdr.c
    src/transmitter.c
    src/receiver.c
)

# Добавляем исполняемый файл
add_executable(main ${MAIN_SOURCE_FILES})

target_include_directories(main PRIVATE headers)
target_link_libraries(main ${SoapySDR_LIBRARIES})
