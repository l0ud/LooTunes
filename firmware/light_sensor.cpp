/*
 * Copyright (c) 2025 Przemysław Romaniak
 * 
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the root directory for details.
*/

#include "light_sensor.h"

void LIGHT::init() {
    __HAL_RCC_ADC_CLK_ENABLE();

    stop();

    ADC1->CHSELR = ADC_CHSELR_CHSEL5; // select channel 5 (PA5)

    ADC1->CR = ADC_CR_ADCAL;
    // wait for calibration to complete
    while (ADC1->CR & ADC_CR_ADCAL);


    NVIC_EnableIRQ(ADC_COMP_IRQn);

    // analog watchdog on ADC1, channel 5 (PA5)
    ADC1->CFGR1 = ADC_CFGR1_CONT | ADC_CFGR1_OVRMOD | ADC_CFGR1_AWDEN | ADC_CFGR1_AWDSGL | ADC_CFGR1_AWDCH_0 | ADC_CFGR1_AWDCH_2;

    ADC1->SMPR = ADC_SMPR_SMP_2 | ADC_SMPR_SMP_1 | ADC_SMPR_SMP_0; // set sampling time to 239.5 cycles (ADC_SMPR_SMP_2 | ADC_SMPR_SMP_1 | ADC_SMPR_SMP_0)

    ADC1->IER = ADC_IER_AWDIE; // enable ADC interrupt for analog watchdog

    set_thresholds(0x000, 0xFFF); // set initial thresholds for analog watchdog
    // start ADC conversion
    ADC1->CR |= ADC_CR_ADSTART;
}

void LIGHT::start() {
    if ((ADC1->CR & ADC_CR_ADEN) != 0) {
        // if ADC is already enabled, just return
        return;
    }

    ADC1->CR |= ADC_CR_ADEN; // enable ADC

    while (!(ADC1->CR & ADC_CR_ADEN));
    
    // wait for ADC to stabilize
    //HAL_Delay(1); // allow some time for the ADC to stabilize after enabling
    ADC1->CR |= ADC_CR_ADSTART; // start ADC conversion
}

void LIGHT::stop() {
    if ((ADC1->CR & ADC_CR_ADEN) != 0) {

        if (ADC1->CR & ADC_CR_ADSTART) {
            // stop any ongoing conversion
            ADC1->CR |= ADC_CR_ADSTP;
            // wait for the conversion to stop
            while (ADC1->CR & ADC_CR_ADSTART);
        }
        ADC1->CR &= ~ADC_CR_ADEN; // disable ADC if it is enabled
        
        while (ADC1->CR & ADC_CR_ADEN);
    }
}

void LIGHT::set_thresholds(uint16_t low, uint16_t high) {
    // check if thresholds currently set are the same
    if ((ADC1->TR & (ADC_TR_HT_Msk | ADC_TR_LT_Msk)) == ((high << ADC_TR_HT_Pos) | (low << ADC_TR_LT_Pos))) {
        return; // thresholds are already set, no need to change
    }

    bool started = (ADC1->CR & ADC_CR_ADSTART) != 0;
    
    if (started) {
        stop();
    }

    ADC1->TR = (high << ADC_TR_HT_Pos) | (low << ADC_TR_LT_Pos);
    if (started) { // restore ADC state
        start();
    }
}

// interrupt
void ADC_COMP_IRQHandler(void) {
    if (ADC1->ISR & ADC_ISR_AWD) {
        // Analog watchdog triggered
        const uint32_t adc_value = ADC1->DR; // read the ADC data register
        // callback is supposed to change thresholds so interrupt won't trigger immediately again
        LightSensorCallback(adc_value);
        ADC1->ISR |= ADC_ISR_AWD; // Clear the AWD flag
    }
}
