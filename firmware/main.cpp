/*
 * Copyright (c) 2025 Przemysław Romaniak
 * 
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the root directory for details.
*/

extern "C" {
#include "py32f0xx.h"
#include "py32f0xx_hal.h"
}

#include "gpio.h"
#include "sd.h"
#include "button.h"
#include "controller.h"
#include "watchdog.h"
#include "light_sensor.h"
#include "random.h"

void SysTick_Handler(void) { HAL_IncTick(); }

void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  // Configure clock sources: HSE/HSI/LSE/LSI
  RCC_OscInitStruct.OscillatorType =
      RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON; // Enable HSI
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1; // No division
  // RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_4MHz;
  // // Set HSI output clock to 4MHz
  // RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_8MHz;
  // // Set HSI output clock to 8MHz
  RCC_OscInitStruct.HSICalibrationValue =
      RCC_HSICALIBRATION_24MHz; // Set HSI output clock to 24MHz
  // RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_22p12MHz;
  // // Set HSI output clock to 22.12MHz
  // RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_24MHz;
  // // Set HSI output clock to 24MHz
  RCC_OscInitStruct.HSEState = RCC_HSE_OFF;     // Disable HSE
  RCC_OscInitStruct.HSEFreq = RCC_HSE_16_32MHz; // HSE crystal frequency 16M~32M
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;      // Enable LSI
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;

  // Initialize RCC oscillator
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    while (1) {
    }
  }

  // Initialize CPU, AHB, APB bus clocks
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1; // RCC system clock types
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; // SYSCLK source: PLL
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;        // AHB clock: no division
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;         // APB clock: no division

  // Initialize RCC system clock (FLASH_LATENCY_0 = below 24M; FLASH_LATENCY_1 = up to 48M)
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
    while (1) {
    }
  }
}

int main() {
  HAL_Init();
  HAL_SuspendTick();
  RAND::init();
  SystemClock_Config();
  //WDT::init();

  GPIO::init();
  BTN::init();
  SPI::init();
  LIGHT::init();
  WDT::feed();
  Controller::init();
  
  while(1) {
  if (!Controller::main()) { // false = error caused by file system / sd card
    Controller::init_sd();
  }
  }

  return 0;
}
