cmake_minimum_required(VERSION 3.15.3)

project(puya_playground)

enable_language(C CXX ASM)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)


set(PROJECT_FILES
        CMSIS/Device/PY32F0xx/Source/startup_py32f0xx.cpp
        CMSIS/Device/PY32F0xx/Source/system_py32f0xx.c
        CMSIS/Device/PY32F0xx/Source/syscalls.c
        HAL/src/py32f0xx_hal.c
        HAL/src/py32f0xx_hal_cortex.c
        HAL/src/py32f0xx_hal_gpio.c
        HAL/src/py32f0xx_hal_rcc.c
        HAL/src/py32f0xx_hal_tim.c
        HAL/src/py32f0xx_hal_tim_ex.c
        main.cpp
        lcd.cpp
        sd.cpp
        button.cpp
        CMSIS/Device/PY32F0xx/Include/py32f0xx.h
        CMSIS/Device/PY32F0xx/Include/system_py32f0xx.h
        )

set(EXECUTABLE ${PROJECT_NAME}.out)

add_executable(${EXECUTABLE} ${PROJECT_FILES})

target_compile_definitions(${EXECUTABLE} PRIVATE
        -DPY32F030x6
        )

target_include_directories(${EXECUTABLE} PRIVATE
        CMSIS/Include
        CMSIS/Device/PY32F0xx/Include
        HAL/include
        ./
        )

target_compile_options(${EXECUTABLE} PRIVATE
        -mcpu=cortex-m0plus
        -mthumb
        -mfloat-abi=soft
        -fdata-sections
        -ffunction-sections
        -Wall
        )

target_link_options(${EXECUTABLE} PRIVATE
        -T${CMAKE_SOURCE_DIR}/CMSIS/Device/PY32F0xx/Source/mapping_py32f0xx.ld
        -mcpu=cortex-m0plus
        -mthumb
        -mfloat-abi=soft
        -lc
        -lm
        -lnosys
        -Wl,-Map=${PROJECT_NAME}.map,--cref
        -Wl,--gc-sections
        )

# Print executable size
add_custom_command(TARGET ${EXECUTABLE}
        POST_BUILD
        COMMAND arm-none-eabi-size ${EXECUTABLE})

# Create hex file
add_custom_command(TARGET ${EXECUTABLE}
        POST_BUILD
        COMMAND arm-none-eabi-objcopy -O ihex ${EXECUTABLE} ${PROJECT_NAME}.hex
        COMMAND arm-none-eabi-objcopy -O binary ${EXECUTABLE} ${PROJECT_NAME}.bin)

