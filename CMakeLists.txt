cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico_w)

# Copiá pico_sdk_import.cmake desde pico-sdk/external a esta carpeta
# (la extensión de VS Code lo genera sola al crear el proyecto)
include(pico_sdk_import.cmake)

project(pendulo_invertido C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(pendulo_invertido main.c mpu6050.c)

target_link_libraries(pendulo_invertido
    pico_stdlib
    hardware_i2c
    hardware_pwm
    hardware_clocks
)

# Salida por USB (monitor serie)
pico_enable_stdio_usb(pendulo_invertido 1)
pico_enable_stdio_uart(pendulo_invertido 0)

pico_add_extra_outputs(pendulo_invertido)
