/*
 * Project-owned startup boundary.
 *
 * The Reset_Handler and complete STM32H743 interrupt vector are taken from the
 * exact CMSIS device revision pinned by this repository. SystemInit,
 * ExitRun0Mode, memory layout, and fault handlers are project implementations
 * under platform/stm32h743 and linker/.
 */
#include "../../../third_party/STM32CubeH7/Drivers/CMSIS/Device/ST/STM32H7xx/Source/Templates/gcc/startup_stm32h743xx.s"
