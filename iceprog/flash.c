#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "flash.h"
#include "spidev_int.h"

extern bool verbose;

// ---------------------------------------------------------
// FLASH definitions
// ---------------------------------------------------------

/* Transfer Command bits */

/* All byte based commands consist of:
 * - Command byte
 * - Length lsb
 * - Length msb
 *
 * If data out is enabled the data follows after the above command bytes,
 * otherwise no additional data is needed.
 * - Data * n
 *
 * All bit based commands consist of:
 * - Command byte
 * - Length
 *
 * If data out is enabled a byte containing bits to transfer follows.
 * Otherwise no additional data is needed. Only up to 8 bits can be transferred
 * per transaction when in bit mode.
 */

/* Flash command definitions */
/* This command list is based on the Winbond W25Q128JV Datasheet */
enum flash_cmd {
        FC_WE = 0x06, /* Write Enable */
        FC_SRWE = 0x50, /* Volatile SR Write Enable */
        FC_WD = 0x04, /* Write Disable */
        FC_RPD = 0xAB, /* Release Power-Down, returns Device ID */
        FC_MFGID = 0x90, /*  Read Manufacturer/Device ID */
        FC_JEDECID = 0x9F, /* Read JEDEC ID */
        FC_UID = 0x4B, /* Read Unique ID */
        FC_RD = 0x03, /* Read Data */
        FC_FR = 0x0B, /* Fast Read */
        FC_PP = 0x02, /* Page Program */
        FC_SE = 0x20, /* Sector Erase 4kb */
        FC_BE32 = 0x52, /* Block Erase 32kb */
        FC_BE64 = 0xD8, /* Block Erase 64kb */
        FC_CE = 0xC7, /* Chip Erase */
        FC_RSR1 = 0x05, /* Read Status Register 1 */
        FC_WSR1 = 0x01, /* Write Status Register 1 */
        FC_RSR2 = 0x35, /* Read Status Register 2 */
        FC_WSR2 = 0x31, /* Write Status Register 2 */
        FC_RSR3 = 0x15, /* Read Status Register 3 */
        FC_WSR3 = 0x11, /* Write Status Register 3 */
        FC_RSFDP = 0x5A, /* Read SFDP Register */
        FC_ESR = 0x44, /* Erase Security Register */
        FC_PSR = 0x42, /* Program Security Register */
        FC_RSR = 0x48, /* Read Security Register */
        FC_GBL = 0x7E, /* Global Block Lock */
        FC_GBU = 0x98, /* Global Block Unlock */
        FC_RBL = 0x3D, /* Read Block Lock */
        FC_RPR = 0x3C, /* Read Sector Protection Registers (adesto) */
        FC_IBL = 0x36, /* Individual Block Lock */
        FC_IBU = 0x39, /* Individual Block Unlock */
        FC_EPS = 0x75, /* Erase / Program Suspend */
        FC_EPR = 0x7A, /* Erase / Program Resume */
        FC_PD = 0xB9, /* Power-down */
        FC_QPI = 0x38, /* Enter QPI mode */
        FC_ERESET = 0x66, /* Enable Reset */
        FC_RESET = 0x99, /* Reset Device */
};


// ---------------------------------------------------------
// FLASH function implementations
// ---------------------------------------------------------

// the FPGA reset is released so also FLASH chip select should be deasserted
void flash_release_reset()
{
        spi_interface->set_gpio(1, 1);
}

// FLASH chip select assert
// should only happen while FPGA reset is asserted
void flash_chip_select()
{
        spi_interface->set_gpio(0, 0);
}

// FLASH chip select deassert
void flash_chip_deselect()
{
        spi_interface->set_gpio(1, 0);
}

// SRAM reset is the same as flash_chip_select()
// For ease of code reading we use this function instead
void sram_reset()
{
        // Asserting chip select and reset lines
        spi_interface->set_gpio(0, 0);
}

// SRAM chip select assert
// When accessing FPGA SRAM the reset should be released
void sram_chip_select()
{
        spi_interface->set_gpio(0, 1);
}

void flash_read_id()
{
        /* JEDEC ID structure:
         * Byte No. | Data Type
         * ---------+----------
         *        0 | FC_JEDECID Request Command
         *        1 | MFG ID
         *        2 | Dev ID 1
         *        3 | Dev ID 2
         *        4 | Ext Dev Str Len
         */

        uint8_t data[260] = { FC_JEDECID };
        int len = 5; // command + 4 response bytes

        if (verbose)
                fprintf(stderr, "read flash ID..\n");

        flash_chip_select();

        // Write command and read first 4 bytes
        spi_interface->xfer_spi(data, 1, data+1, 4);
	
        if (data[4] == 0xFF)
                fprintf(stderr, "Extended Device String Length is 0xFF, "
                                "this is likely a read error. Ignorig...\n");
        else {
                // Read extended JEDEC ID bytes
                if (data[4] != 0) {
                        len += data[4];
                        spi_interface->xfer_spi(NULL, 0, data + 5, len - 5);
                }
        }

        flash_chip_deselect();

        // TODO: Add full decode of the JEDEC ID.
        fprintf(stderr, "flash ID:");
        for (int i = 1; i < len; i++)
                fprintf(stderr, " 0x%02X", data[i]);
        fprintf(stderr, "\n");
}

void flash_reset()
{
        flash_chip_select();
        spi_interface->xfer_spi_bits(0xFF, 8);
        flash_chip_deselect();

        flash_chip_select();
        spi_interface->xfer_spi_bits(0xFF, 2);
        flash_chip_deselect();
}

void flash_power_up()
{
        uint8_t data_rpd[1] = { FC_RPD };
        flash_chip_select();
        spi_interface->xfer_spi(data_rpd, 1, NULL, 0);
        flash_chip_deselect();
}

void flash_power_down()
{
        uint8_t data[1] = { FC_PD };
        flash_chip_select();
        spi_interface->xfer_spi(data, 1, NULL, 0);
        flash_chip_deselect();
}

uint8_t flash_read_status()
{
        uint8_t data[2] = { FC_RSR1 };

        flash_chip_select();
        spi_interface->xfer_spi(data, 1, data+1, 1);
        flash_chip_deselect();

        if (verbose) {
                fprintf(stderr, "SR1: 0x%02X\n", data[1]);
                fprintf(stderr, " - SPRL: %s\n",
                        ((data[1] & (1 << 7)) == 0) ?
                                "unlocked" :
                                "locked");
                fprintf(stderr, " -  SPM: %s\n",
                        ((data[1] & (1 << 6)) == 0) ?
                                "Byte/Page Prog Mode" :
                                "Sequential Prog Mode");
                fprintf(stderr, " -  EPE: %s\n",
                        ((data[1] & (1 << 5)) == 0) ?
                                "Erase/Prog success" :
                                "Erase/Prog error");
                fprintf(stderr, "-  SPM: %s\n",
                        ((data[1] & (1 << 4)) == 0) ?
                                "~WP asserted" :
                                "~WP deasserted");
                fprintf(stderr, " -  SWP: ");
                switch((data[1] >> 2) & 0x3) {
                        case 0:
                                fprintf(stderr, "All sectors unprotected\n");
                                break;
                        case 1:
                                fprintf(stderr, "Some sectors protected\n");
                                break;
                        case 2:
                                fprintf(stderr, "Reserved (xxxx 10xx)\n");
                                break;
                        case 3:
                                fprintf(stderr, "All sectors protected\n");
                                break;
                }
                fprintf(stderr, " -  WEL: %s\n",
                        ((data[1] & (1 << 1)) == 0) ?
                                "Not write enabled" :
                                "Write enabled");
                fprintf(stderr, " - ~RDY: %s\n",
                        ((data[1] & (1 << 0)) == 0) ?
                                "Ready" :
                                "Busy");
        }

        usleep(1000);

        return data[1];
}

void flash_write_enable()
{
        if (verbose) {
                fprintf(stderr, "status before enable:\n");
                flash_read_status();
        }

        if (verbose)
                fprintf(stderr, "write enable..\n");

        uint8_t data[1] = { FC_WE };
        flash_chip_select();
        spi_interface->xfer_spi(data, 1, NULL, 0);
        flash_chip_deselect();

        if (verbose) {
                fprintf(stderr, "status after enable:\n");
                flash_read_status();
        }
}

void flash_bulk_erase()
{
        fprintf(stderr, "bulk erase..\n");

        uint8_t data[1] = { FC_CE };
        flash_chip_select();
        spi_interface->xfer_spi(data, 1, NULL, 0);
        flash_chip_deselect();
}

void flash_64kB_sector_erase(int addr)
{
        fprintf(stderr, "erase 64kB sector at 0x%06X..\n", addr);

        uint8_t command[4] = { FC_BE64, (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr };

        flash_chip_select();
        spi_interface->xfer_spi(command, 4, NULL, 0);
        flash_chip_deselect();
}

void flash_prog(int addr, uint8_t *data, uint32_t n)
{
        if (verbose)
                fprintf(stderr, "prog 0x%06X +0x%03X..\n", addr, n);

        uint8_t command[4] = { FC_PP, (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr };

	uint8_t* tx_data = malloc(n + 4);
	if (NULL == tx_data) {
		return;
	}
	memcpy(tx_data, command, 4);
	memcpy(tx_data+4, data, n);
	
        flash_chip_select();
        spi_interface->xfer_spi(tx_data, n+4, NULL, 0);
        flash_chip_deselect();

        if (verbose)
                for (int i = 0; i < n; i++)
                        fprintf(stderr, "%02x%c", data[i], i == n - 1 || i % 32 == 31 ? '\n' : ' ');
}

void flash_read(int addr, uint8_t *data, uint32_t n)
{
	if (verbose) {
                fprintf(stderr, "read 0x%06X +0x%03X..\n", addr, n);
	}

        uint8_t command[4] = { FC_RD, (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr };

        flash_chip_select();
        memset(data, 0, n);
        spi_interface->xfer_spi(command, 4, data, n);
        flash_chip_deselect();

        if (verbose)
                for (int i = 0; i < n; i++)
                        fprintf(stderr, "%02x%c", data[i], i == n - 1 || i % 32 == 31 ? '\n' : ' ');
}

void flash_wait()
{
        if (verbose)
                fprintf(stderr, "waiting..");

        int count = 0;
        while (1)
        {
                uint8_t data[2] = { FC_RSR1 };

                flash_chip_select();
                spi_interface->xfer_spi(data, 1, data+1, 1);
                flash_chip_deselect();
		
                if ((data[1] & 0x01) == 0) {
                        if (count < 2) {
                                count++;
                                if (verbose) {
                                        fprintf(stderr, "r");
                                        fflush(stderr);
                                }
                        } else {
                                if (verbose) {
                                        fprintf(stderr, "R");
                                        fflush(stderr);
                                }
                                break;
                        }
                } else {
                        if (verbose) {
                                fprintf(stderr, ".");
                                fflush(stderr);
                        }
                        count = 0;
                }

                usleep(1000);
        }

        if (verbose)
                fprintf(stderr, "\n");

}

void flash_disable_protection()
{
        fprintf(stderr, "disable flash protection...\n");

        // Write Status Register 1 <- 0x00
        uint8_t data[2] = { FC_WSR1, 0x00 };
        flash_chip_select();
        spi_interface->xfer_spi(data, 2, NULL, 0);
        flash_chip_deselect();

        flash_wait();

        // Read Status Register 1
        data[0] = FC_RSR1;

        flash_chip_select();
        spi_interface->xfer_spi(data, 1, data, 1);
        flash_chip_deselect();

        if (data[1] != 0x00)
                fprintf(stderr, "failed to disable protection, SR now equal to 0x%02x (expected 0x00)\n", data[1]);

}
