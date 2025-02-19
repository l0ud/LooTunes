#include "button.h"
//#include "signal_queue.h"

#include "defines.h"

extern "C" {
#include "py32f0xx.h"
#include "py32f0xx_hal.h"
}

void Button::init()
{
    // button - PB1, pull-down
    GPIOB->MODER &= ~GPIO_MODER_MODE1_Msk; // input mode

    GPIOB->PUPDR &= ~GPIO_PUPDR_PUPD1_Msk;
    GPIOB->PUPDR |= GPIO_PUPDR_PUPD1_1; // pull-down
    
    EXTI->EXTICR[0] &= ~EXTI_EXTICR1_EXTI1_Msk;
    EXTI->EXTICR[0] |= EXTI_EXTICR1_EXTI1_0; // select port B
    EXTI->FTSR |= EXTI_FTSR_FT1; // falling edge
    EXTI->RTSR |= EXTI_RTSR_RT1; // rising edge too

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


    EXTI->IMR |= EXTI_IMR_IM1; // unmask interrupt in exti
    //todo: EXTI_EMR ?
    NVIC_EnableIRQ(EXTI0_1_IRQn);
}

void Button::onExtInterrupt() {

    // if timer 3 was not enabled
    if (!(TIM3->CR1 & TIM_CR1_CEN)) {
        // some event happened since a while
        TIM3->CNT = 0;
        TIM3->CR1 |= TIM_CR1_CEN;
        // release events only interesting after timer is started
        return;
    }

    if (TIM3->CNT < BUTTON_DEBOUNCE_PERIOD) {
        // ignore this interrupt, possible bounce
        return;
    }

    // we're in stable state here so we could check if button was released
    if (!(GPIOB->IDR & GPIO_IDR_ID1)) {
        // button was released, short press
        //const Signal signal = { Signal::Type::ButtonPress, true, false };
        //SignalQueue.enqueue(signal);
        // reset timer so it will reach short timer interrupt again
        ShortButtonPressCallback();
        TIM3->CNT = 0;
    }

    // long press will be handled by timer
    // so nothing to do here
}

void Button::onShortTimerInterrupt() {
    // if the button is not pressed after bouncing period, stop timer
    if (!(GPIOB->IDR & GPIO_IDR_ID1)) {
        TIM3->CR1 &= ~TIM_CR1_CEN;
    }
}

void Button::onTimerInterrupt() {
    // button was held for 1 second
    // check if button is still pressed reading port
    if (GPIOB->IDR & GPIO_IDR_ID1) {
        // button is still pressed, long press
        //const Signal signal = { Signal::Type::ButtonPress, true, true };
        //SignalQueue.enqueue(signal);
        LongButtonPressCallback();
    }
}

Button BTN;

void EXTI0_1_IRQHandler(void)
{
    BTN.onExtInterrupt();
    // button pressed
    EXTI->PR |= EXTI_PR_PR1; // clear pending interrupt
}

void TIM3_IRQHandler(void)
{
    if (TIM3->SR & TIM_SR_CC1IF) {
        BTN.onShortTimerInterrupt();
        TIM3->SR &= ~TIM_SR_CC1IF; // clear interrupt flag
    }

    if (TIM3->SR & TIM_SR_UIF) {
        BTN.onTimerInterrupt();
        TIM3->SR &= ~TIM_SR_UIF;
    }
}