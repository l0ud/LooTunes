#include "gpio.h"

void GPIO::init() {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // PA4 - SD NSS
    GPIOA->MODER &= ~GPIO_MODER_MODE4_Msk;
    GPIOA->MODER |= GPIO_MODER_MODE4_0; // output mode
    GPIOA->OTYPER &= ~GPIO_OTYPER_OT4_Msk; // push-pull
    GPIOA->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED4_Msk;
    GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEED4_1 | GPIO_OSPEEDR_OSPEED4_0; // very high speed

    GPIOA->BSRR = GPIO_BSRR_BS4;

    // PA5 - light sensor (ADC_IN5)
    GPIOA->MODER &= ~GPIO_MODER_MODE5_Msk; // clear mode
    GPIOA->MODER |= GPIO_MODER_MODE5_0 | GPIO_MODER_MODE5_1; // analog mode
    GPIOA->OTYPER &= ~GPIO_OTYPER_OT5_Msk; // push-pull
    GPIOA->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED5_Msk; // low speed (00)

    // PA7 - USB power pin (output)
    GPIOA->MODER &= ~GPIO_MODER_MODE7_Msk;
    GPIOA->MODER |= GPIO_MODER_MODE7_0; // output mode
    GPIOA->OTYPER &= ~GPIO_OTYPER_OT7_Msk; // push-pull
    GPIOA->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED7_Msk;
    GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEED7_0; // low speed (01)
    // turn off USB power
    GPIOA->BSRR = GPIO_BSRR_BR7; // reset PA7

    // PB2 - led pin, GPIO output
    GPIOB->MODER &= ~GPIO_MODER_MODE2_Msk;
    GPIOB->MODER |= GPIO_MODER_MODE2_0; // output mode
    GPIOB->OTYPER &= ~GPIO_OTYPER_OT2_Msk; // push-pull
    GPIOB->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED2_Msk;
    GPIOB->OSPEEDR |= GPIO_OSPEEDR_OSPEED2_0; // low speed (01)

    // map PB3 to TIM1_CH2
    GPIOB->MODER &= ~GPIO_MODER_MODE3_Msk;
    GPIOB->MODER |= GPIO_MODER_MODE3_1; // alternate mode
    GPIOB->AFR[0] |= (1 << GPIO_AFRL_AFSEL3_Pos); // AF1: TIM1_CH2

    // map PB6 to TIM1_CH3
    GPIOB->MODER &= ~GPIO_MODER_MODE6_Msk;
    GPIOB->MODER |= GPIO_MODER_MODE6_1; // alternate mode
    GPIOB->AFR[0] |= (1 << GPIO_AFRL_AFSEL6_Pos); // AF1: TIM1_CH3
}