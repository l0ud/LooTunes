/*
 * Copyright (c) 2025 Przemysław Romaniak
 * 
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the root directory for details.
*/

#include "button.h"

extern "C" {
#include "py32f0xx.h"
#include "py32f0xx_hal.h"
}

static uint32_t g_btn_pressed = 0;

void BTN::init()
{
    // buttons are connected to PB0 and PB1, pull-up

    GPIOB->MODER &= ~(GPIO_MODER_MODE0_Msk | GPIO_MODER_MODE1_Msk); // input mode

    GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD0_Msk | GPIO_PUPDR_PUPD1_Msk); 
    GPIOB->PUPDR |= GPIO_PUPDR_PUPD0_0 | GPIO_PUPDR_PUPD1_0; // pull-up

    EXTI->EXTICR[0] |= EXTI_EXTICR1_EXTI0_0 | EXTI_EXTICR1_EXTI1_0; // 0: PB[0] and PB[1] pin -> EXTI line 0 and 1
    EXTI->FTSR |= EXTI_FTSR_FT0 | EXTI_FTSR_FT1; // falling edge
    EXTI->RTSR |= EXTI_RTSR_RT0 | EXTI_RTSR_RT1; // rising edge too

    // we'll use TIM3 for debouncing and measuring hold time
    __HAL_RCC_TIM3_CLK_ENABLE();
    // set timer for milliseconds
    TIM3->PSC = INPUT_FREQUENCY / 1000;
    TIM3->ARR = BUTTON_HOLD_PERIOD;

    TIM3->CCR1 = BUTTON_DEBOUNCE_PERIOD;
    
    // one pulse mode
    TIM3->CR1 |= TIM_CR1_OPM | TIM_CR1_URS; // one pulse mode, only overflow generates interrupt
    TIM3->DIER |= TIM_DIER_CC1IE | TIM_DIER_UIE; // enable capture1 and update interrupts
    // enable interrupt for timer
    NVIC_EnableIRQ(TIM3_IRQn);

    EXTI->IMR |= EXTI_IMR_IM0 | EXTI_IMR_IM1; // unmask interrupts in exti
    //todo: EXTI_EMR ?
    NVIC_EnableIRQ(EXTI0_1_IRQn);
}

void BTN::on_ext_interrupt() {

    // if timer 3 was not enabled
    if (!(TIM3->CR1 & TIM_CR1_CEN)) {
        // some event happened since a while
        TIM3->CNT = 0;
        TIM3->CR1 |= TIM_CR1_CEN;
        
        // remember which button was pressed
        g_btn_pressed = GPIOB->IDR & (GPIO_IDR_ID0 | GPIO_IDR_ID1);

        // release events only interesting after timer is started
        return;
    }

    if (TIM3->CNT < BUTTON_DEBOUNCE_PERIOD) {
        // ignore this interrupt, possible bounce
        return;
    }

    // we're in stable state here so we could check if button was released
    if ((GPIOB->IDR & (GPIO_IDR_ID0 | GPIO_IDR_ID1)) == (GPIO_IDR_ID0 | GPIO_IDR_ID1)) {
        // button was released, short press
        ButtonPressCallback(g_btn_pressed & GPIO_IDR_ID0 ? BTN::ID::POWER : BTN::ID::NEXT);

        // reset timer so it will reach short timer interrupt again
        TIM3->CNT = 0;
    }

    // long press will be handled by timer
    // so nothing to do here
}

void BTN::on_short_timer_interrupt() {
    // if no button is not pressed after bouncing period, stop timer
    if ((GPIOB->IDR & (GPIO_IDR_ID0 | GPIO_IDR_ID1)) == (GPIO_IDR_ID0 | GPIO_IDR_ID1)) {
        TIM3->CR1 &= ~TIM_CR1_CEN;
    }
}

void BTN::on_timer_interrupt() {
    // button was held for 1 second
    // check if button is still pressed reading port
    if ((GPIOB->IDR & GPIO_IDR_ID0) == 0) {
        // button is still pressed, long press
        ButtonPressCallback(BTN::ID::PREV);
    }
    if ((GPIOB->IDR & GPIO_IDR_ID1) == 0) {
        // button 2 is still pressed, long press
        ButtonPressCallback(BTN::ID::NEXT_DIR);
    }
}

void EXTI0_1_IRQHandler(void)
{
    BTN::on_ext_interrupt();
    // button pressed
    EXTI->PR |= EXTI_PR_PR0 | EXTI_PR_PR1; // clear pending interrupt
}

void TIM3_IRQHandler(void)
{
    if (TIM3->SR & TIM_SR_CC1IF) {
        BTN::on_short_timer_interrupt();
        TIM3->SR &= ~TIM_SR_CC1IF; // clear interrupt flag
    }

    if (TIM3->SR & TIM_SR_UIF) {
        BTN::on_timer_interrupt();
        TIM3->SR &= ~TIM_SR_UIF;
    }
}