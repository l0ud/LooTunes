#include "sd.h"
#include "spi.h"

#include "lcd.h"

#include <algorithm>

void SD::cs_set() {
    GPIOA->BSRR = GPIO_BSRR_BR4; // Set NSS low
}

void SD::cs_reset() {
    GPIOA->BSRR = GPIO_BSRR_BS4; // Set NSS high
}

uint8_t SD::send_command(uint8_t cmd, uint32_t arg, uint8_t crc) {
    uint8_t command[6];
    command[0] = cmd | 0x40; // Add the start bit (0b01xxxxxx)
    command[1] = (arg >> 24) & 0xFF;
    command[2] = (arg >> 16) & 0xFF;
    command[3] = (arg >> 8) & 0xFF;
    command[4] = arg & 0xFF;
    command[5] = crc; // CRC for CMD0 or CMD8; otherwise, it's ignored in SPI mode
    
    SPI::raw_write(command, sizeof(command));
    
    // Wait for a response (response starts with 0 in the most significant bit)
    uint8_t response;
    for (int i = 0; i < 8; i++) { // Try up to 8 bytes
        SPI::raw_read(&response, 1);
        if ((response & 0x80) == 0) {
            break; // Response received
        }
    }
    return response;
}

bool SD::init() {
    volatile uint8_t response;
    SPI::speed_mode(false); // slow SPI
    SPITransaction transaction; // enable SPI during this function

    // Step 1: Send at least 74 clock cycles with CS high
    cs_reset();
    for (int i = 0; i < 10; i++) {
        SPI::raw_byte_read();
    }

    // Step 2: Send CMD0 (GO_IDLE_STATE) with CS low
    uint8_t retries = 4;
    do {
        cs_set();
        response = send_command(0, 0, 0x95); // CMD0, no argument, CRC = 0x95
        cs_reset();

        if (response == 0x01) {
            break;
        }
        
    } while(--retries);


    if (retries == 0) {
        return false; // Card did not respond to CMD0
    }

    retries = 4; // todo: consider if this retry mechanism is necessary, might just fail
    do {
        SPI::raw_byte_read(); // some cards apparently need it?
        SPI::raw_byte_read(); // some cards apparently need it?

        // Step 3: Send CMD8 (SEND_IF_COND) to check card version
        cs_set();
        response = send_command(8, 0x000001AA, 0x87); // CMD8, argument = 0x1AA, CRC = 0x87
        uint8_t cmd8_response[4];
        if (response == 0x01) {
            // Read CMD8 response (4 bytes)
            SPI::raw_read(cmd8_response, sizeof(cmd8_response));
            if (cmd8_response[2] != 0x01 || cmd8_response[3] != 0xAA) {
                cs_reset();
                return false; // Card does not support required voltage range
            }

            break;
        }
        cs_reset();
    } while(--retries);

    if (retries == 0) {
        return false; // Card did not respond to CMD8 correctly, we don't support non-sdhc cards anyway
    }

    // Step 4: Send ACMD41 repeatedly until the card exits idle state
    do {
        SPI::raw_byte_read(); // some cards apparently need it?
        SPI::raw_byte_read(); // some cards apparently need it?

        // Send CMD55 (APP_CMD) before ACMD41
        cs_set();
        response = send_command(55, 0, 0x01); // CMD55, no argument, CRC ignored in SPI mode
        cs_reset();

        SPI::raw_byte_read(); // some cards apparently need it?
        SPI::raw_byte_read(); // some cards apparently need it?


        cs_set();
        response = send_command(41, 0x40000000, 0x01); // ACMD41, argument = HCS (Host Capacity Support)
        cs_reset();
    } while (response != 0x00);

    SPI::raw_byte_read(); // some cards apparently need it?
    SPI::raw_byte_read(); // some cards apparently need it?


    // Step 5: Send CMD58 (READ_OCR) to read OCR register
    cs_set();
    response = send_command(58, 0, 0x01); // CMD58, no argument, CRC ignored
    if (response != 0x00) {
        cs_reset();
        return false; // Failed to read OCR register
    }
    uint8_t ocr[4];
    SPI::raw_read(ocr, sizeof(ocr));

    // try to release bus
    SPI::raw_byte_read();
    cs_reset();
    SPI::raw_byte_read();

    // Initialization complete
    return true;
}

void SD::stream_sectors(uint32_t starting, uint32_t ending) {
    SPI::dma_begin();
    SPI::begin();
    cs_set();
    SPI::raw_byte_read(); // some cards apparently need it?

    send_command(18, starting, 0x01); // CMD18, argument = starting sector, CRC ignored

    //uint32_t full_frame = 170 * 320 * 2;

    while(starting != ending) {

        //uint32_t current_read = std::min((uint32_t)512, full_frame);
        //full_frame -= current_read;

        // read until start token
        *(__IO uint8_t *)&SPI1->DR; // read data to empty recv fifo
        *(__IO uint8_t *)&SPI1->DR; // read data to empty recv fifo
        *(__IO uint8_t *)&SPI1->DR; // read data to empty recv fifo
        *(__IO uint8_t *)&SPI1->DR; // read data to empty recv fifo

        uint8_t token;
        do {
            //SPI::raw_read(&token, 1);
            token = SPI::raw_byte_read();
        } while (token != 0xFE);

        LCD::listen();

        /*
        // read 512 bytes
        uint8_t buffer[512];
        SPI::raw_read(buffer, 512);
        */

        SPI::dma_clock_gen(512);
        //SPI::raw_read(buffer, 512);
        LCD::stop_listening();
        // skip crc and wait for next start token
        uint16_t crc;
        SPI::raw_read(reinterpret_cast<uint8_t*>(&crc), sizeof(crc));
        ++starting;
    }

    // send CMD12 to stop transmission
    send_command(12, 0, 0x01);

    cs_reset();

    SPI::end();
    SPI::dma_end();
}
