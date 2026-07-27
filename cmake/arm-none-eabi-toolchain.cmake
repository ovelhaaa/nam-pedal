set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(_ARM_HINTS
    "$ENV{ARM_GCC_PATH}/bin"
    "C:/Program Files (x86)/GNU Arm Embedded Toolchain/10 2021.10/bin"
    "C:/Program Files/Arm GNU Toolchain arm-none-eabi/bin")

find_program(CMAKE_C_COMPILER arm-none-eabi-gcc HINTS ${_ARM_HINTS} REQUIRED)
find_program(CMAKE_ASM_COMPILER arm-none-eabi-gcc HINTS ${_ARM_HINTS} REQUIRED)
find_program(CMAKE_OBJCOPY arm-none-eabi-objcopy HINTS ${_ARM_HINTS} REQUIRED)
find_program(CMAKE_SIZE arm-none-eabi-size HINTS ${_ARM_HINTS} REQUIRED)
find_program(CMAKE_AR arm-none-eabi-ar HINTS ${_ARM_HINTS} REQUIRED)
find_program(CMAKE_RANLIB arm-none-eabi-ranlib HINTS ${_ARM_HINTS} REQUIRED)

set(CMAKE_EXECUTABLE_SUFFIX_C ".elf")
