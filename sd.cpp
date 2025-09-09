#include "sd.h"
#include "spi.h"

#include <algorithm>

extern "C" {
    #include "petitfat/source/diskio.h"
}

namespace {
    uint8_t sectorCache[512];
    DWORD sdCachedSector = NO_SECTOR;
    DWORD sdRequestedSector = NO_SECTOR;
    bool sdMultiTransfer = false;
}


/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/
DSTATUS disk_initialize (void)
{
	if (SD::init()) {
        SPI::speed_mode(true); // fast SPI
        return RES_OK;
    }

    return RES_ERROR;
}

void make_empty_traffic() {
    // some cards apparently need it?
    // should be performed with deselected CS
    SPI::raw_byte_read();
    SPI::raw_byte_read();
}


/*-----------------------------------------------------------------------*/
/* Read Partial Sector                                                   */
/*-----------------------------------------------------------------------*/

DRESULT sd_request_sector(DWORD sector) {
    SPI::begin();
    SD::cs_set();
    if (SD::send_command(17, sector, 0x01) != 0x00) { // CMD17 to read a single block
        // command failed
        SD::cs_reset();
        SPI::end();
        return RES_ERROR;
    }

    sdRequestedSector = sector;

    return RES_OK;
}

DRESULT sd_start_sector_stream(DWORD starting_sector) {
    SPI::begin();
    SD::cs_set();
    if (SD::send_command(18, starting_sector, 0x01) != 0x00) { // CMD18 to read multiple blocks
        // command failed
        SD::cs_reset();
        SPI::end();
        return RES_ERROR;
    }

    sdRequestedSector = starting_sector;
    sdMultiTransfer = true;

    return RES_OK;
}

DRESULT sd_stop_sector_stream() {
    if (SD::send_command(12, 0, 0x01) != 0x00) { // CMD12 to stop transmission
        // command failed
        SD::cs_reset();
        make_empty_traffic();
        SPI::end();
        return RES_ERROR;
    }

    sdRequestedSector = NO_SECTOR;
    sdMultiTransfer = false;

    SD::cs_reset();
    make_empty_traffic();
    SPI::end();
    return RES_OK;
}

void sd_read_sector() {
    // Wait for data token (0xFE)
    uint8_t token;
    do {
        token = SPI::raw_byte_read();
    } while (token == 0xFF);

    // Read the entire sector into the cache
    SPI::raw_read(sectorCache, sizeof(sectorCache));

    // Skip CRC bytes
    SPI::raw_byte_read();
    SPI::raw_byte_read();

    if (sdMultiTransfer) {
        sdRequestedSector++;
    }
    else {
        SD::cs_reset();
        // for my testing it's not needed in all cards I have
        // make_empty_traffic();
        SPI::end();
        sdRequestedSector = NO_SECTOR;
    }
}

DRESULT disk_readp_ex (
    BYTE* buff,		/* Pointer to the destination object */
    DWORD sector,	/* Sector number (LBA) */
    DWORD next_sector, /* Next sector number (LBA) for pre-fetching */
    UINT offset,	/* Offset in the sector */
    UINT count		/* Byte count (bit15:destination) */
)
{
    if (next_sector == NO_SECTOR) {
        next_sector = sector + 1; // heuristics
    }

    DRESULT res = RES_ERROR;

    if (sdCachedSector == sector) {
        res = RES_OK;
    }
    else if (sector == sdRequestedSector) {
        // already requested sector from sd card, wait and read it
        sd_read_sector();
        sdCachedSector = sector;
        res = RES_OK;
    }

    if (sdRequestedSector != NO_SECTOR && next_sector != sdRequestedSector) {
        // requested sector earlier but now we will need different one, stop transfer
        if (sd_stop_sector_stream() != RES_OK) {
            return RES_ERROR;
        }
    }

    if (res != RES_OK) { // we don't have sector data yet

        // need to request the sector now and wait for it
        if (next_sector == sector + 1) { // transfer needed
            // request multi-sector read if we are going to read the next sector soon
            res = sd_start_sector_stream(sector);
        }
        else {
            res = sd_request_sector(sector);
        }

        if (res == RES_OK) {
            sd_read_sector();
            sdCachedSector = sector;
        }
        else {
            return res;
        }
    }

    // correct sector is in cache
    std::copy(sectorCache + offset, sectorCache + offset + count, buff);

    // at this point we have the correct sector, but we might want to pre-fetch the next one
    if (sdRequestedSector == NO_SECTOR && next_sector != NO_SECTOR) {
        sd_start_sector_stream(next_sector);
    }

    return res;
}

/*-----------------------------------------------------------------------*/
/* Write Partial Sector                                                  */
/*-----------------------------------------------------------------------*/

namespace {
    UINT sdWriteBytes = 0;
}

DRESULT disk_writep (
    const BYTE* buff,       /* Pointer to the data to be written, NULL:Initiate/Finalize write operation */
    DWORD sc                /* Sector number (LBA) when buff==NULL and sc>0; or number of bytes to send when buff!=NULL */
)
{
    // if read is pending stop it
    if (sdRequestedSector != NO_SECTOR) {
        if (sd_stop_sector_stream() != RES_OK) {
            return RES_ERROR;
        }
    }

    // Initiate write (buff == NULL, sc > 0): Send CMD24 and the data-start token (0xFE)
    if (!buff) {
        if (sc) {
            // Start single-block write to LBA = sc (SDHC uses LBA directly)
            SPI::begin();
            SD::cs_set();

            // CMD24 (WRITE_BLOCK)
            if (SD::send_command(24, sc, 0x01) != 0x00) {
                SD::cs_reset();
                make_empty_traffic();
                SPI::end();
                return RES_ERROR;
            }

            // Send start token 0xFE
            uint8_t token = 0xFE;
            SPI::raw_write(&token, 1);

            sdWriteBytes = 0;
            return RES_OK;
        } else {
            // Finalize write (buff == NULL, sc == 0): send CRC, check data-response, wait busy release
            while (sdWriteBytes < 512) {
                uint8_t pad = 0;
                SPI::raw_write(&pad, 1);
                sdWriteBytes++;
            }

            // Send dummy CRC
            uint8_t crc[2] = {0xFF, 0xFF};
            SPI::raw_write(crc, 2);

            // Get first non-0xFF data-response byte
            uint8_t resp;
            do { resp = SPI::raw_byte_read(); } while (resp == 0xFF);

            // Data accepted = 0bxxx00101 (mask low 5 bits == 0x05)
            if ((resp & 0x1F) != 0x05) {
                SD::cs_reset();
                make_empty_traffic();
                SPI::end();
                return RES_ERROR;
            }

            // Busy wait: card drives DO low (0x00) until the programming completes
            // Add a simple timeout if desired
            uint32_t guard = 0;
            while (SPI::raw_byte_read() == 0x00) {
                if (++guard > 10'000'000u) { // crude timeout guard
                    SD::cs_reset();
                    make_empty_traffic();
                    SPI::end();
                    return RES_ERROR;
                }
            }

            // Release bus
            SD::cs_reset();
            make_empty_traffic();
            SPI::end();
            return RES_OK;
        }
    }

    // Enforce sector-size limit
    if (sdWriteBytes + (UINT)sc > 512) {
        return RES_ERROR;
    }

    if (sc) {
        SPI::raw_write((uint8_t*)buff, (size_t)sc);
        sdWriteBytes += (UINT)sc;
    }

    return RES_OK;
}



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
    
    if (cmd == 12) {
        SPI::raw_byte_read(); // discard a stuff byte when sending CMD12
    }

    // Wait for a response (response starts with 0 in the most significant bit)
    uint8_t response;
    for (int i = 0; i < 8; i++) { // Try up to 8 bytes
        response = SPI::raw_byte_read();
        if ((response & 0x80) == 0) {
            break; // Response received
        }
    }

    return response;
}

bool SD::init() {
    sdCachedSector = NO_SECTOR;
    sdRequestedSector = NO_SECTOR;
    sdMultiTransfer = false;

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
        make_empty_traffic();

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
        make_empty_traffic();

        // Send CMD55 (APP_CMD) before ACMD41
        cs_set();
        response = send_command(55, 0, 0x01); // CMD55, no argument, CRC ignored in SPI mode
        cs_reset();

        make_empty_traffic();

        cs_set();
        response = send_command(41, 0x40000000, 0x01); // ACMD41, argument = HCS (Host Capacity Support)
        cs_reset();
    } while (response != 0x00);

    make_empty_traffic();

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
