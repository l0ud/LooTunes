/**
 * @file      startup_stm32g071k8tx.c
 * @brief     STM32G071 Devices vector table for GCC toolchain.
 *            This module performs:
 *                - Set the initial SP
 *                - Set the initial PC == Reset_Handler,
 *                - Set the vector table entries with the exceptions ISR address,
 *                - Configure external SRAM mounted on STM3210E-EVAL board
 *                  to be used as data memory (optional, to be enabled by user)
 *                - Branches to main in the C library (which eventually
 *                  calls main()).
 *            After Reset the Cortex-M0+ processor is in Thread mode,
 *            priority is Privileged, and the Stack is set to Main.
 *******************************************************************************
  *
 */

/* Library includes. */

extern "C" {
  #include "py32f0xx.h"
  #include "core_cm0plus.h"
}

#define WEAK __attribute__ ((weak))

/* Includes ----------------------------------------------------------------------*/
extern "C" {
  void WEAK Reset_Handler(void);            // Address = 0x0000_0004
}
void WEAK NMI_Handler(void);              // Address = 0x0000_0008
void WEAK HardFault_Handler(void);        // Address = 0x0000_000C
void WEAK Reserved_Handler(void);         // Address = 0x0000_0010 to 0x0000_0028
void WEAK SVC_Handler(void);              // Address = 0x0000_002C
void WEAK Reserved_Handler(void);         // Address = 0x0000_0030
void WEAK Reserved_Handler(void);         // Address = 0x0000_0034
void WEAK PendSV_Handler(void);           // Address = 0x0000_0038
void WEAK SysTick_Handler(void);          // Address = 0x0000_003C

/* External Interrupts */
void WEAK WWDG_IRQHandler(void);                   // Address = 0x0000_0040 -> Window WatchDog
void WEAK PVD_IRQHandler(void);                    // Address = 0x0000_0044 -> PVD through EXTI Line detect
void WEAK RTC_IRQHandler(void);                    // Address = 0x0000_0048 -> RTC through the EXTI line
void WEAK FLASH_IRQHandler(void);                  // Address = 0x0000_004C -> FLASH
void WEAK RCC_IRQHandler(void);                    // Address = 0x0000_0050 -> RCC
void WEAK EXTI0_1_IRQHandler(void);                // Address = 0x0000_0054 -> EXTI Line 0 and 1
void WEAK EXTI2_3_IRQHandler(void);                // Address = 0x0000_0058 -> EXTI Line 2 and 3
void WEAK EXTI4_15_IRQHandler(void);               // Address = 0x0000_005C -> EXTI Line 4 to 15
void WEAK DMA1_Channel1_IRQHandler(void);          // Address = 0x0000_0064 -> DMA1 Channel 1
void WEAK DMA1_Channel2_3_IRQHandler(void);        // Address = 0x0000_0068 -> DMA1 Channel 2 and Channel 3
void WEAK ADC_COMP_IRQHandler(void);               // Address = 0x0000_0070 -> ADC1, COMP1 and COMP2
void WEAK TIM1_BRK_UP_TRG_COM_IRQHandler(void);    // Address = 0x0000_0074 -> TIM1 Break, Update, Trigger and Commutation
void WEAK TIM1_CC_IRQHandler(void);                // Address = 0x0000_0078 -> TIM1 Capture Compare
void WEAK TIM3_IRQHandler(void);                   // Address = 0x0000_0080 -> TIM3
void WEAK LPTIM1_IRQHandler(void);                 // Address = 0x0000_0084 -> TIM6, DAC and LPTIM1
void WEAK TIM14_IRQHandler(void);                  // Address = 0x0000_008C -> TIM14
void WEAK TIM16_IRQHandler(void);                  // Address = 0x0000_0094 -> TIM16
void WEAK TIM17_IRQHandler(void);                  // Address = 0x0000_0098 -> TIM17
void WEAK I2C1_IRQHandler(void);                   // Address = 0x0000_009C -> I2C1 combinated with EXTI23
void WEAK SPI1_IRQHandler(void);                   // Address = 0x0000_00A4 -> SPI1
void WEAK USART1_IRQHandler(void);                 // Address = 0x0000_00AC -> USART1
void WEAK USART2_IRQHandler(void);                 // Address = 0x0000_00B0 -> USART2



/* Exported types --------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
extern unsigned long _sidata; // FLASH start address for the .data section

extern unsigned long _rodata;	// FLASH start address for the .rodata section. defined in linker script

extern unsigned long _sdata;			/* RAM start address for the .data section. defined in linker script */
extern unsigned long _edata;		/* RAM end address for the .data section. defined in linker script */

extern unsigned long _sbss;			/* RAM start address for the .bss section. defined in linker script */
extern unsigned long _ebss;			/* RAM end address for the .bss section. defined in linker script */

extern unsigned long _stack; 		/* RAM start address for the .stack section. defined in linker script */
extern unsigned long _estack; 		/* RAM end address for the .stack section. defined in linker script */

extern unsigned long _user_heap_stack; 		/* start address for the .heap section. defined in linker script */
extern unsigned long _euser_heap_stack; 	/* end address for the .heap section. defined in linker script */

/* Private typedef -----------------------------------------------------------*/
/* function prototypes ------------------------------------------------------*/

extern "C" void Default_Handler(void)
{
  /* Go into an infinite loop. */
  while (1)
  {
  }
}

extern int main(void);

/******************************************************************************
*
* The minimal vector table for a Cortex M0+.  Note that the proper constructs
* must be placed on this to ensure that it ends up at physical address
* 0x0000.0000.
*
******************************************************************************/


__attribute__ ((section(".isr_vector")))
void (* const g_pfnVectors[])(void) __attribute__((used)) =
{
  (void(*)(void))&_estack,    /* Top of stack address */
  Reset_Handler,              /* Reset Handler */
  NMI_Handler,                /* NMI Handler */
  HardFault_Handler,          /* Hard Fault Handler */
  0,                          /* Reserved */
  0,                          /* Reserved */
  0,                          /* Reserved */
  0,                          /* Reserved */
  0,                          /* Reserved */
  0,                          /* Reserved */
  0,                          /* Reserved */
  SVC_Handler,                /* SVCall Handler */
  0,                          /* Reserved */
  0,                          /* Reserved */
  PendSV_Handler,             /* PendSV Handler */
  SysTick_Handler,            /* SysTick Handler */
  /* External Interrupts */
  WWDG_IRQHandler,              // 0Window Watchdog
  PVD_IRQHandler,               // 1PVD through EXTI Line detect
  RTC_IRQHandler,               // 2RTC through EXTI Line
  FLASH_IRQHandler,             // 3FLASH
  RCC_IRQHandler,               // 4RCC
  EXTI0_1_IRQHandler,           // 5EXTI Line 0 and 1
  EXTI2_3_IRQHandler,           // 6EXTI Line 2 and 3
  EXTI4_15_IRQHandler,          // 7EXTI Line 4 to 15
  0,                            // 8Reserved 
  DMA1_Channel1_IRQHandler,     // 9DMA1 Channel 1
  DMA1_Channel2_3_IRQHandler,   // 10DMA1 Channel 2 and Channel 3
  0,                            // 11Reserved 
  ADC_COMP_IRQHandler,          // 12ADC&COMP1 
  TIM1_BRK_UP_TRG_COM_IRQHandler, // 13TIM1 Break, Update, Trigger and Commutation
  TIM1_CC_IRQHandler,           // 14TIM1 Capture Compare
  0,                            // 15Reserved 
  TIM3_IRQHandler,              // 16TIM3
  LPTIM1_IRQHandler,            // 17LPTIM1
  0,                            // 18Reserved 
  TIM14_IRQHandler,             // 19TIM14
  0,                            // 20Reserved 
  TIM16_IRQHandler,             // 21TIM16
  TIM17_IRQHandler,             // 22TIM17
  I2C1_IRQHandler,              // 23I2C1
  0,                            // 24Reserved 
  SPI1_IRQHandler,              // 25SPI1
  0,                            // 26Reserved
  USART1_IRQHandler,            // 27USART1
  USART2_IRQHandler,            // 28USART2
  0,                            // 29Reserved
  0,                            // 30Reserved
  0                             // 31Reserved
};

/**
 * @brief  This is the code that gets called when the processor first
 *          starts execution following a reset event. Only the absolutely
 *          necessary set is performed, after which the application
 *          supplied main() routine is called.
 * @param  None
 * @retval : None
*/
#define VECT_TAB_OFFSET  0x0U /*!< Vector Table base offset field.
                                   This value must be a multiple of 0x100. */

/**
 * @brief  This is the code that gets called when the processor receives an
 *         unexpected interrupt.  This simply enters an infinite loop, preserving
 *         the system state for examination by a debugger.
 *
 * @param  None
 * @retval : None
*/

void Reset_Handler(void)
{
    register unsigned long *pulSrc, *pulDest;

    // Configure HSI default clock to 8MHz
    RCC->ICSCR = (RCC->ICSCR & 0xFFFF0000) | (0x1<<13) | *(uint32_t *)(0x1fff0f04); 

    // Fill the stack with a known value.
    for(pulDest = &_stack; pulDest < &_estack; )
    {
    *pulDest++ = 0xA5A5A5A5;
    }

     // Fill the heap with a known value.
   for (pulDest = &_user_heap_stack; pulDest < &_euser_heap_stack; ++pulDest)
    {
    *pulDest = 0xEAEAEAEA;
    }

    // Copy the data segment initializers from flash to SRAM.
    pulSrc = &_sidata;
    for(pulDest = &_sdata; pulDest < &_edata; )
    {
        *(pulDest++) = *(pulSrc++);
    }

    // Zero fill the bss segment.
    for(pulDest = &_sbss; pulDest < &_ebss; )
    {
        *(pulDest++) = 0;
    }

    // Call the application's entry point.
    main();
}

/*******************************************************************************
*
* Provide weak aliases for each Exception handler to the Default_Handler.
* As they are weak aliases, any function with the same name will override
* this definition.
*
*******************************************************************************/
#pragma weak NMI_Handler = Default_Handler
#pragma weak HardFault_Handler = Default_Handler
#pragma weak SVC_Handler = Default_Handler
#pragma weak PendSV_Handler = Default_Handler
#pragma weak SysTick_Handler = Default_Handler

#pragma weak WWDG_IRQHandler = Default_Handler
#pragma weak PVD_IRQHandler = Default_Handler
#pragma weak RTC_IRQHandler = Default_Handler
#pragma weak FLASH_IRQHandler = Default_Handler
#pragma weak RCC_IRQHandler = Default_Handler
#pragma weak EXTI0_1_IRQHandler = Default_Handler
#pragma weak EXTI2_3_IRQHandler = Default_Handler
#pragma weak EXTI4_15_IRQHandler = Default_Handler
#pragma weak DMA1_Channel1_IRQHandler = Default_Handler
#pragma weak DMA1_Channel2_3_IRQHandler = Default_Handler
#pragma weak ADC1_COMP_IRQHandler = Default_Handler
#pragma weak TIM1_BRK_UP_TRG_COM_IRQHandler = Default_Handler
#pragma weak TIM1_CC_IRQHandler = Default_Handler
#pragma weak TIM3_IRQHandler = Default_Handler
#pragma weak LPTIM1_IRQHandler = Default_Handler
#pragma weak TIM14_IRQHandler = Default_Handler
#pragma weak TIM16_IRQHandler = Default_Handler
#pragma weak TIM17_IRQHandler = Default_Handler
#pragma weak I2C1_IRQHandler = Default_Handler
#pragma weak SPI1_IRQHandler = Default_Handler
#pragma weak USART1_IRQHandler = Default_Handler
#pragma weak USART2_IRQHandler = Default_Handler

