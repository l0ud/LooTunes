
extern "C" {
#include "config.h"
#include "py32f0xx.h"
#include "py32f0xx_hal.h"
}

#include "sd.h"
#include "lcd.h"
#include "button.h"

TIM_HandleTypeDef TimHandle1;
TIM_HandleTypeDef TimHandle16;

void gpio_init() {
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  // PA4 - SD NSS
  // PA5 - LCD NSS

  GPIOA->MODER &= ~GPIO_MODER_MODE4_Msk;
  GPIOA->MODER |= GPIO_MODER_MODE4_0; // output mode
  GPIOA->OTYPER &= ~GPIO_OTYPER_OT4_Msk; // push-pull
  GPIOA->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED4_Msk;
  GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEED4_1 | GPIO_OSPEEDR_OSPEED4_0; // very high speed

  GPIOA->BSRR = GPIO_BSRR_BS4;

  GPIOA->MODER &= ~GPIO_MODER_MODE5_Msk;
  GPIOA->MODER |= GPIO_MODER_MODE5_0; // output mode
  GPIOA->OTYPER &= ~GPIO_OTYPER_OT5_Msk; // push-pull
  GPIOA->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED5_Msk;
  GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEED5_1 | GPIO_OSPEEDR_OSPEED5_0; // very high speed
  GPIOA->BSRR = GPIO_BSRR_BS5;

  // PA6 - LCD DC
  GPIOA->MODER &= ~GPIO_MODER_MODE6_Msk;
  GPIOA->MODER |= GPIO_MODER_MODE6_0; // output mode
  GPIOA->OTYPER &= ~GPIO_OTYPER_OT6_Msk; // push-pull
  GPIOA->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED6_Msk;
  GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEED6_1 | GPIO_OSPEEDR_OSPEED6_0; // very high speed

}

void SysTick_Handler(void) { HAL_IncTick(); }

void TIM16_IRQHandler(void) { HAL_TIM_IRQHandler(&TimHandle16); }

void TIM1_BRK_UP_TRG_COM_IRQHandler(void) { HAL_TIM_IRQHandler(&TimHandle1); }

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
  RCC_OscInitStruct.LSIState = RCC_LSI_OFF;     // 关闭LSI
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
#define PORTB_LOW(x) GPIOB->BRR = (x)
#define PORTB_HIGH(x) GPIOB->BSRR = (x)

int main() {
  HAL_Init();
  HAL_SuspendTick();

  SystemClock_Config();

  gpio_init();

  BTN.init();

  // GPIOA->ODR |= LCD_BLK_PIN;
  SPI::init();

  // first initialize SD card
  bool result = SD::init();


  LCD::init();
  LCD::frame_command();

  SPI::speed_mode(true);


  if (result) {
    LCD::fillBlack();
  }
  else {
    LCD::fillRed();
  }


  LCD::on();
  LCD::frame_command();


  if (!result) {
    // restart microcontroller
    HAL_Delay(1000);
    NVIC_SystemReset();
  }

  SPI::dma_map();
  while(1) {
    SD::stream_sectors(425*0, 425*30); //10000
  }
  return 0;
}
