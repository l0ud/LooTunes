
extern "C" {
#include "defines.h"
#include "py32f0xx.h"
#include "py32f0xx_hal.h"
}

#include "gpio.h"
#include "sd.h"
#include "button.h"
#include "playback.h"
#include "watchdog.h"
#include "light_sensor.h"

void SysTick_Handler(void) { HAL_IncTick(); }

void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /*配置时钟源HSE/HSI/LSE/LSI*/
  RCC_OscInitStruct.OscillatorType =
      RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON; // 开启HSI
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1; // 不分频
  // RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_4MHz;
  // //配置HSI输出时钟为4MHz RCC_OscInitStruct.HSICalibrationValue =
  // RCC_HSICALIBRATION_8MHz;                          //配置HSI输出时钟为8MHz
  RCC_OscInitStruct.HSICalibrationValue =
      RCC_HSICALIBRATION_24MHz; // 配置HSI输出时钟为16MHz
  // RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_22p12MHz;
  // //配置HSI输出时钟为22.12MHz RCC_OscInitStruct.HSICalibrationValue =
  // RCC_HSICALIBRATION_24MHz;                         //配置HSI输出时钟为24MHz
  RCC_OscInitStruct.HSEState = RCC_HSE_OFF;     // 关闭HSE
  RCC_OscInitStruct.HSEFreq = RCC_HSE_16_32MHz; // HSE晶振工作频率16M~32M
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;     // 关闭LSI
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) // 初始化RCC振荡器
  {
    while (1) {
    }
  }

  // 初始化CPU,AHB,APB总线时钟
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1; // RCC系统时钟类型
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; // SYSCLK的源选择为HSI
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;     // APH时钟不分频
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;      // APB时钟不分频

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) !=
      HAL_OK) // 初始化RCC系统时钟(FLASH_LATENCY_0=24M以下;FLASH_LATENCY_1=48M)
  {
    while (1) {
    }
  }
}

int main() {
  HAL_Init();
  HAL_SuspendTick();
  SystemClock_Config();
  //WDT::init();

  GPIO::init();
  BTN::init();
  SPI::init();
  //SPI::dma_map();
  LIGHT::init();
  WDT::feed();
  Controller::init();
  
  while(1) {
  if (!Controller::main()) { // error caused by file system / sd card
    Controller::sd_init();
  }
  }

  return 0;
}
