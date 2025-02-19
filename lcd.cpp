/* Tiny TFT Graphics Library v5 - see http://www.technoblogy.com/show?3WAI

   David Johnson-Davies - www.technoblogy.com - 26th October 2022

   CC BY 4.0
   Licensed under a Creative Commons Attribution 4.0 International license:
   http://creativecommons.org/licenses/by/4.0/
*/

#include "defines.h"

extern "C" {
    #include "py32f0xx.h"
    #include "py32f0xx_hal.h"
    #include "spi.h"
}

#include "lcd.h"

namespace {

int const xoff = 0, yoff = 35, invert = 1, rotate = 0,
          bgr = 0;

bool brighter = true;

// TFT color display **********************************************

int const CASET = 0x2A; // Define column address
int const RASET = 0x2B; // Define row address
int const RAMWR = 0x2C; // Write to display RAM

}


void LCD::cs_set() {
    GPIOA->BSRR = GPIO_BSRR_BR5; // Set NSS low
}

void LCD::cs_reset() {
    GPIOA->BSRR = GPIO_BSRR_BS5; // Set NSS high
}

void LCD::dc_set() {
    GPIOA->BSRR = GPIO_BSRR_BS6; // Set DC high
}

void LCD::dc_reset() {
    GPIOA->BSRR = GPIO_BSRR_BR6; // Set DC low
}


void LCD::pwm_init() {
    // enable timer14 used to lcd backlight control
    __HAL_RCC_TIM14_CLK_ENABLE();
    TIM14->ARR = 200; // period
    TIM14->CCR1 = 201; // duty cycle

    TIM14->PSC = INPUT_FREQUENCY / 100000;
    TIM14->CNT = 0;

    // make pwm on channel 1
    TIM14->CCMR1 = TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2; // PWM mode 1
    TIM14->CCER = TIM_CCER_CC1E;            // Enable output on Channel 1
    TIM14->BDTR = TIM_BDTR_MOE;             // Enable main output (MOE)

    TIM14->CR1 = TIM_CR1_CEN | TIM_CR1_ARPE; // enable, auto reload preload


    // map PA7 to TIM14_CH1
    GPIOA->AFR[0] &= ~GPIO_AFRL_AFSEL7_Msk;
    GPIOA->AFR[0] |= (4 << GPIO_AFRL_AFSEL7_Pos); // TIM14_CH1

    GPIOA->MODER &= ~GPIO_MODER_MODE7_Msk;
    GPIOA->MODER |= GPIO_MODER_MODE7_1; // alternate mode
    GPIOA->OTYPER &= ~GPIO_OTYPER_OT7_Msk; // push-pull

    GPIOA->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED7_Msk;
    GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEED7_1; // high speed
}

// Send a byte to the display
void LCD::byte_write(uint8_t d)
{
    //spi_blocking_write(&d, sizeof(d));
    SPI::raw_write(&d, sizeof(d));
}

// Send a command to the display
void LCD::command(uint8_t c)
{
    dc_reset();
    byte_write(c);
    //spi_wait_end();
    dc_set();
}

// Send a command followed by two byte_write words
void LCD::command2(uint8_t c, uint16_t d1, uint16_t d2)
{
    dc_reset();
    byte_write(c);
    //spi_wait_end();
    dc_set();
    byte_write(d1 >> 8);
    byte_write(d1);
    byte_write(d2 >> 8);
    byte_write(d2);
    //spi_wait_end();
}

void LCD::init()
{
    SPI::begin();
    dc_set(); // make byte_write default
    cs_set();

    command(0x01);  // Software reset

    
    HAL_Delay(250); // HAL_Delay 250 ms
    cs_reset(); // interestingly, only software reset command needs this (on ST7739, ST7735 is actually fine without it)
    cs_set();

    command(0x36);
    byte_write(rotate << 5 | bgr << 3); // Set orientation and rgb/bgr

    command(0x3A);
    byte_write(0x55); // Set color mode - 16-bit color

    // set gamma
    //command(0x26);
    //byte_write(0x02);

    //enchancements - WRCACE
    //command(0x55);
    // Moving Image high enhancement
    //byte_write(0xB3);

     //command(0xC0); //  8: Power control, 2 args + delay:
     //byte_write(0x00);                         //     GVDD = 4.7V
     //byte_write(0x70);
    //HAL_Delay(10); // HAL_Delay 10 ms

    //      1.0uA
    //command(0xC1); //  8: Power control, 2 args + delay:
    //byte_write(0b111); // needed for some poor screens
    HAL_Delay(10); // HAL_Delay 10 ms

    command(0x20 + invert); // Invert

    command(0x11);          // Out of sleep mode
    SPI::end();
    HAL_Delay(150);    
    cs_reset();
}

void LCD::ConfigChanged()
{
    TIM14->CCR1 = 201;
}

void ShortButtonPressCallback() {
    uint16_t values[] = {1, 50, 100, 150, 201};
    static uint8_t index = 0;

    TIM14->CCR1 = values[index];
    index = (index + 1) % 5;
}

void LongButtonPressCallback() {

}

void LCD::LightIntensityChanged(bool bright) {
    brighter = bright;
    LCD::ConfigChanged();
}

void LCD::on()
{
    SPITransaction spi;
    cs_set();
    command(0x29); // Display on
    HAL_Delay(150);
    cs_reset();
    pwm_init();
}

void LCD::fillBlack()
{
    SPI::begin();
    cs_set();
    for (int i = 0; i < Width; i++) {
        for (int j = 0; j < Height; j++) {
            byte_write(0x00);
            byte_write(0x00);
        }
    }
    cs_reset();
    SPI::end();
}

void LCD::fillRed()
{
    SPI::begin();
    cs_set();
    for (int i = 0; i < Width; i++) {
        for (int j = 0; j < Height; j++) {
            byte_write(0xF8);
            byte_write(0x00);
        }
    }
    cs_reset();
    SPI::end();
}

void LCD::frame_command() {
    SPITransaction spi;
    cs_set();
    command2(CASET, yoff, yoff + Height - 1);
    command2(RASET, xoff, xoff + Width - 1);
    command(RAMWR); // Leaves mosi low
/*
    for (int i = 0; i < Width; i++) {
        for (int j = 0; j < Height; j++) {
            uint8_t byte = i%2 == 0 ? 0xFF : 0x00;
            byte_write(byte);
            byte_write(byte);
        }
    }
*/
    cs_reset();

}
void LCD::EndFrame() {
    //spi_wait_end();
    //cs_reset();
}

void LCD::DrawLine(uint16_t *line) {
    //spi_dma_write(Height, line);
}
