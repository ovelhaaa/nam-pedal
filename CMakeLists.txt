cmake_minimum_required(VERSION 3.21)

if(NOT CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/arm-none-eabi-toolchain.cmake"
      CACHE FILEPATH "ARM GNU toolchain" FORCE)
endif()

project(nam_pedal_platform LANGUAGES C ASM)

set(NAM_IMAGE "bringup" CACHE STRING "Firmware image: bootloader, bringup, or xip")
set_property(CACHE NAM_IMAGE PROPERTY STRINGS bootloader bringup xip)
set(CLOCK_PROFILE "PERFORMANCE_480MHZ" CACHE STRING "SAFE_400MHZ or PERFORMANCE_480MHZ")
set_property(CACHE CLOCK_PROFILE PROPERTY STRINGS SAFE_400MHZ PERFORMANCE_480MHZ)

if(NOT NAM_IMAGE MATCHES "^(bootloader|bringup|xip)$")
  message(FATAL_ERROR "NAM_IMAGE must be bootloader, bringup, or xip")
endif()
if(NOT CLOCK_PROFILE MATCHES "^(SAFE_400MHZ|PERFORMANCE_480MHZ)$")
  message(FATAL_ERROR "CLOCK_PROFILE must be SAFE_400MHZ or PERFORMANCE_480MHZ")
endif()

set(CUBE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/STM32CubeH7")
set(HAL_ROOT "${CUBE_ROOT}/Drivers/STM32H7xx_HAL_Driver")
set(CMSIS_ROOT "${CUBE_ROOT}/Drivers/CMSIS")
set(DEVICE_ROOT "${CMSIS_ROOT}/Device/ST/STM32H7xx")
foreach(required
    "${HAL_ROOT}/Src/stm32h7xx_hal.c"
    "${DEVICE_ROOT}/Include/stm32h743xx.h"
    "${DEVICE_ROOT}/Source/Templates/gcc/startup_stm32h743xx.s")
  if(NOT EXISTS "${required}")
    message(FATAL_ERROR "STM32CubeH7 v1.12.1 is not initialized: missing ${required}. Run git submodule update --init --recursive third_party/STM32CubeH7.")
  endif()
endforeach()

set(PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/platform/stm32h743")
set(HAL_SOURCES
    "${HAL_ROOT}/Src/stm32h7xx_hal.c"
    "${HAL_ROOT}/Src/stm32h7xx_hal_cortex.c"
    "${HAL_ROOT}/Src/stm32h7xx_hal_dma.c"
    "${HAL_ROOT}/Src/stm32h7xx_hal_dma_ex.c"
    "${HAL_ROOT}/Src/stm32h7xx_hal_flash.c"
    "${HAL_ROOT}/Src/stm32h7xx_hal_flash_ex.c"
    "${HAL_ROOT}/Src/stm32h7xx_hal_gpio.c"
    "${HAL_ROOT}/Src/stm32h7xx_hal_mdma.c"
    "${HAL_ROOT}/Src/stm32h7xx_hal_pwr.c"
    "${HAL_ROOT}/Src/stm32h7xx_hal_pwr_ex.c"
    "${HAL_ROOT}/Src/stm32h7xx_hal_qspi.c"
    "${HAL_ROOT}/Src/stm32h7xx_hal_rcc.c"
    "${HAL_ROOT}/Src/stm32h7xx_hal_rcc_ex.c"
    "${HAL_ROOT}/Src/stm32h7xx_hal_uart.c"
    "${HAL_ROOT}/Src/stm32h7xx_hal_uart_ex.c")

set(PLATFORM_SOURCES
    "${PLATFORM_DIR}/src/startup_stm32h743.S"
    "${PLATFORM_DIR}/src/system_stm32h743.c"
    "${PLATFORM_DIR}/src/platform_clock.c"
    "${PLATFORM_DIR}/src/platform_uart.c"
    "${PLATFORM_DIR}/src/platform_dwt.c"
    "${PLATFORM_DIR}/src/platform_memory.c"
    "${PLATFORM_DIR}/src/platform_error.c"
    "${PLATFORM_DIR}/src/platform_qspi.c"
    "${PLATFORM_DIR}/src/platform_manifest.c"
    "${PLATFORM_DIR}/src/platform_diagnostics.c"
    "${PLATFORM_DIR}/src/platform_sections.c"
    "${PLATFORM_DIR}/src/platform_hal.c")

if(NAM_IMAGE STREQUAL "xip")
  list(APPEND HAL_SOURCES
      "${HAL_ROOT}/Src/stm32h7xx_hal_sai.c")
  list(APPEND PLATFORM_SOURCES
      "${CMAKE_CURRENT_SOURCE_DIR}/audio/audio.c")
endif()

if(NAM_IMAGE STREQUAL "bootloader")
  set(MAIN_SOURCE "${PLATFORM_DIR}/src/main_bootloader.c")
  set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker/bootloader.ld")
  set(IMAGE_DEFINE NAM_IMAGE_BOOTLOADER=1)
elseif(NAM_IMAGE STREQUAL "bringup")
  set(MAIN_SOURCE "${PLATFORM_DIR}/src/main_bringup.c")
  set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker/bringup_internal.ld")
  set(IMAGE_DEFINE NAM_IMAGE_BRINGUP=1)
else()
  set(MAIN_SOURCE "${PLATFORM_DIR}/src/main_xip.c")
  set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker/application_xip.ld")
  set(IMAGE_DEFINE NAM_IMAGE_XIP=1)
endif()

set(TARGET "nam_${NAM_IMAGE}_${CLOCK_PROFILE}")
add_executable(${TARGET} ${HAL_SOURCES} ${PLATFORM_SOURCES} "${MAIN_SOURCE}")
set_target_properties(${TARGET} PROPERTIES
    OUTPUT_NAME "${TARGET}"
    SUFFIX ".elf")
set_property(TARGET ${TARGET} APPEND PROPERTY LINK_DEPENDS "${LINKER_SCRIPT}")
if(NOT NAM_IMAGE STREQUAL "xip")
  set_property(TARGET ${TARGET} APPEND PROPERTY LINK_DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/linker/common_internal_sections.ld")
endif()

target_include_directories(${TARGET} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/audio"
    "${PLATFORM_DIR}/include"
    "${HAL_ROOT}/Inc"
    "${HAL_ROOT}/Inc/Legacy"
    "${CMSIS_ROOT}/Include"
    "${CMSIS_ROOT}/Core/Include"
    "${DEVICE_ROOT}/Include")

target_compile_definitions(${TARGET} PRIVATE
    STM32H743xx
    USE_HAL_DRIVER
    USE_PWR_LDO_SUPPLY
    NAM_SAMPLE_FLOAT
    ${IMAGE_DEFINE}
    CLOCK_PROFILE_${CLOCK_PROFILE}=1)

target_compile_options(${TARGET} PRIVATE
    -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard
    -ffunction-sections -fdata-sections
    -Wall -Wextra -Werror=return-type
    $<$<CONFIG:Debug>:-Og;-g3>
    $<$<CONFIG:Release>:-O2;-g0>)

target_link_options(${TARGET} PRIVATE
    -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard
    -T${LINKER_SCRIPT}
    -L${CMAKE_CURRENT_SOURCE_DIR}
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.map
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    --specs=nano.specs
    --specs=nosys.specs)

set(ELF "$<TARGET_FILE:${TARGET}>")
set(BIN "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.bin")
set(HEX "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.hex")
add_custom_command(TARGET ${TARGET} POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O binary "${ELF}" "${BIN}"
    COMMAND "${CMAKE_OBJCOPY}" -O ihex "${ELF}" "${HEX}"
    COMMAND "${CMAKE_SIZE}" "${ELF}"
    COMMENT "Generating BIN/HEX and reporting image size")

if(NAM_IMAGE STREQUAL "xip")
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  set(MANIFEST_BIN "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.manifest.bin")
  set(QSPI_BIN "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.qspi.bin")
  add_custom_command(TARGET ${TARGET} POST_BUILD
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/make_manifest.py"
              --image "${BIN}" --manifest "${MANIFEST_BIN}" --combined "${QSPI_BIN}"
              --firmware-version 1
      COMMENT "Generating the 64-byte manifest and 0x10000-offset QSPI image")
endif()
