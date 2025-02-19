#pragma once

extern "C" {
#include "py32f0xx.h"
#include "py32f0xx_hal.h"

#include "config.h"
}
//#define LCD_MOSI_PIN GPIO_PIN_12 // GPIOA
//#define LCD_DC_PIN GPIO_PIN_6 // GPIOA

static uint8_t DMADummy = 0xff;

class SPI {
public:
    static void init()
    {
        // pinout
        //PA0 = SPI1_MISO
        //PA1 = SPI1_MOSI
        //PA2 = SPI1_SCK


        // switch GPIO to alternate mode
        
        // MISO - alternate mode
        GPIOA->MODER &= ~GPIO_MODER_MODE0_Msk;  
        GPIOA->MODER |= GPIO_MODER_MODE0_1; // alternate mode
        GPIOA->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED0_Msk;
        GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEED0_1 | GPIO_OSPEEDR_OSPEED0_0; // very high speed
        GPIOA->AFR[0] |= (10 << GPIO_AFRL_AFSEL0_Pos); // SPI1_MISO
        // mosi - alternate mode
        GPIOA->MODER &= ~GPIO_MODER_MODE1_Msk;
        GPIOA->MODER |= GPIO_MODER_MODE1_1; // alternate mode
        GPIOA->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED1_Msk;
        GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEED1_1 | GPIO_OSPEEDR_OSPEED1_0; // very high speed
        GPIOA->AFR[0] |= (10 << GPIO_AFRL_AFSEL1_Pos); // SPI1_MOSI
        // sck - alternate mode
        GPIOA->MODER &= ~GPIO_MODER_MODE2_Msk;
        GPIOA->MODER |= GPIO_MODER_MODE2_1; // alternate mode
        GPIOA->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED2_Msk;
        GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEED2_1 | GPIO_OSPEEDR_OSPEED2_0; // very high speed
        GPIOA->AFR[0] |= (10 << GPIO_AFRL_AFSEL2_Pos); // SPI1_SCK


        // nss - alternate mode
        /*
        GPIOB->MODER &= ~GPIO_MODER_MODE0_Msk;
        GPIOB->MODER |= GPIO_MODER_MODE0_1; // alternate mode
        GPIOB->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED0_Msk;
        GPIOB->OSPEEDR |= GPIO_OSPEEDR_OSPEED0_1 | GPIO_OSPEEDR_OSPEED0_0; // very high speed
        GPIOB->AFR[0] = (0 << GPIO_AFRL_AFSEL0_Pos); // SPI1_NSS
        GPIOB->PUPDR |= GPIO_PUPDR_PUPD0_0; // pull-up
        */

        //GPIOA->MODER &= ~GPIO_MODER_MODE6_Msk;
        //GPIOA->MODER |= GPIO_MODER_MODE6_0; // alternate mode
        //GPIOA->MODER &= ~GPIO_MODER_MODE0_Msk;
        //GPIOA->MODER |= GPIO_MODER_MODE0_0; // alternate mode

        // prepare SPI peripheral, needs to be call after gpio init
        //CPOL=0
        __HAL_RCC_SPI1_CLK_ENABLE();
        __HAL_RCC_DMA_CLK_ENABLE();
        __HAL_RCC_SYSCFG_CLK_ENABLE();

        // polarity clock 0 when idle, capture first edge (low to high), CPOL=0 and CPHA=0 so not setting them
        SPI1->CR1 = 0;
        speed_mode(false);
        SPI1->CR1 |= SPI_CR1_MSTR; // spi enable master, baud rate 00: fPCLK/2, as fast as possible

        SPI1->CR2 = SPI_CR2_FRXTH | SPI_CR2_SSOE; // | ; // RXNE event is generated if the FIFO level is greater than or equal to 1/4 (8-bit), TODO: TXEIE, RXNEIE, ERRIE
        
    }    // SPI_CR1_BIDIOE to set to output/input

    static void speed_mode(bool fast) {
        SPI1->CR1 &= ~SPI_CR1_BR_Msk; // clear baud rate
        if (!fast) {
            // slow = fPCLK/256
            SPI1->CR1 |= SPI_CR1_BR_2 | SPI_CR1_BR_1 | SPI_CR1_BR_0;
        }
    }

    static void begin() {
        SPI1->CR1 |= SPI_CR1_SPE; // spi enabled
    }

    static void end() {
        SPI1->CR1 &= ~SPI_CR1_SPE; // disable SPI
    }

    static void dma_map() {
        SYSCFG->CFGR3 &= ~SYSCFG_CFGR3_DMA1_MAP_Msk; // unmap dma channel 1
        SYSCFG->CFGR3 |= SYSCFG_CFGR3_DMA1_MAP_0; // map channel 1 dma to spi tx signalling
        DMA1_Channel1->CPAR = (uint32_t)&SPI1->DR; // connect dma to tx fifo

        //SYSCFG->CFGR3 &= ~SYSCFG_CFGR3_DMA2_MAP_Msk; // unmap dma channel 2
        //SYSCFG->CFGR3 |= SYSCFG_CFGR3_DMA2_MAP_1; // map channel 2 dma to spi rx signalling
        //DMA1_Channel2->CPAR = (uint32_t)&SPI1->DR; // connect dma to rx fifo
    }

    static void dma_begin() {
        SPI1->CR2 |= SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN;
    }

    static void dma_end() {
        SPI1->CR2 &= ~(SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN);
    }

    static void raw_read(uint8_t* output, size_t len)
    {
        size_t iter = 0;

        while (iter < len) {

            // send dummy byte
            *(__IO uint8_t *)&SPI1->DR = 0xff;


            while(!(SPI1->SR & SPI_SR_RXNE)) { // rx empty
                // wait to receive something
            }

            output[iter++] = *(__IO uint8_t *)&SPI1->DR; // read data
        }

        spi_wait_end();

    }

    static inline uint8_t raw_byte_read()
    {
        *(__IO uint8_t *)&SPI1->DR = 0xff;
        while(!(SPI1->SR & SPI_SR_RXNE)) { // rx empty
            // wait to receive something
        }

        spi_wait_end();

        return *(__IO uint8_t *)&SPI1->DR; // read data

    }

    static void raw_write(uint8_t* input, size_t len) {
        size_t iter = 0;
        while (iter < len) {
            while(!(SPI1->SR & SPI_SR_TXE)) {
                // wait for space in fifo
            }
            *(__IO uint8_t *)&SPI1->DR = input[iter++];
        }

        spi_wait_end();

        // clean stuff received
        while (SPI1->SR & SPI_SR_FRLVL) {
            *(__IO uint8_t *)&SPI1->DR;
        }
    }


    static void dma_write(const uint8_t *memory, size_t len)
    {
        DMA1->IFCR = 0xffffffff;

        // channel 2 - fake rx
        DMA1_Channel2->CCR = 0;
        DMA1_Channel2->CNDTR = len;
        DMA1_Channel2->CMAR = reinterpret_cast<uint32_t>(&DMADummy); // receive to dummy mem

        DMA1_Channel2->CCR = DMA_CCR_EN; // from periph, memory do not increment, transfer enabled

        // channel 1 - tx
        DMA1_Channel1->CCR = 0;
        DMA1_Channel1->CNDTR = len;
        DMA1_Channel1->CMAR = reinterpret_cast<uint32_t>(memory);
        DMA1_Channel1->CCR = DMA_CCR_DIR | DMA_CCR_EN | DMA_CCR_MINC; // from memory, memory increment, transfer enabled

        spi_wait_dma_end();
        //spi_wait_end();

        DMA1_Channel1->CCR = 0;
        DMA1_Channel2->CCR = 0;

    }

    static void dma_read(uint8_t* input, size_t len)
    {
        DMA1->IFCR = 0xffffffff;

        // channel 2 - rx
        DMA1_Channel2->CCR = 0;
        DMA1_Channel2->CNDTR = len;
        DMA1_Channel2->CMAR = reinterpret_cast<uint32_t>(input); // receive to real memory

        DMA1_Channel2->CCR = DMA_CCR_EN | DMA_CCR_MINC; // from periph, memory increment, transfer enabled

        // channel 1 - placeholder tx
        DMA1_Channel1->CCR = 0;
        DMA1_Channel1->CNDTR = len;
        DMA1_Channel1->CMAR = reinterpret_cast<uint32_t>(&DMADummy);
        DMA1_Channel1->CCR = DMA_CCR_DIR | DMA_CCR_EN; // from memory, memory not increment, transfer enabled

        spi_wait_dma_end();
        //spi_wait_end();

        DMA1_Channel1->CCR = 0;
        DMA1_Channel2->CCR = 0;

    }

    static void dma_clock_gen(size_t len)
    {
        DMA1->IFCR = 0xffffffff;

        // channel 2 - rx
        /*
        DMA1_Channel2->CCR = 0;
        DMA1_Channel2->CNDTR = len;
        DMA1_Channel2->CMAR = reinterpret_cast<uint32_t>(input); // receive to real memory

        DMA1_Channel2->CCR = DMA_CCR_EN; // from periph, memory NOT increment (don't care), transfer enabled
*/
        // channel 1 - placeholder tx
        DMA1_Channel1->CCR = 0;
        DMA1_Channel1->CNDTR = len;
        DMA1_Channel1->CMAR = reinterpret_cast<uint32_t>(&DMADummy);
        DMA1_Channel1->CCR = DMA_CCR_DIR | DMA_CCR_EN; // from memory, memory not increment, transfer enabled

        spi_wait_dma_end();
        //spi_wait_end();

        DMA1_Channel1->CCR = 0;
        //DMA1_Channel2->CCR = 0;

    }

private:
    static void spi_wait_dma_end() {
        //while ((DMA1->ISR & (DMA_ISR_TCIF1 | DMA_ISR_TCIF2)) != (DMA_ISR_TCIF1 | DMA_ISR_TCIF2)) {
        //}

        while ((DMA1->ISR & (DMA_ISR_TCIF1)) != DMA_ISR_TCIF1) {
        }

        spi_wait_end();
    }

    static void inline spi_wait_end()
    {
        /*
        while (SPI1->SR & SPI_SR_FTLVL) {
            // wait for fifo empty
        }

        while (SPI1->SR & SPI_SR_BSY) {
            // wait for last oper completion
        }*/


        while (SPI1->SR & (SPI_SR_FTLVL | SPI_SR_BSY)) {
            // wait for fifo empty
        }
    }
};

class SPITransaction {
public:
    SPITransaction() {
        SPI::begin();
    }

    ~SPITransaction() {
        SPI::end();
    }
};
