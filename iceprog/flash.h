#include "spi_int.h"

extern spi_interface_t* spi_interface;

void flash_release_reset();
void flash_chip_deselect();
void sram_reset();
void sram_chip_select();

void flash_read_id();
void flash_reset();
void flash_power_up();
void flash_power_down();
uint8_t flash_read_status();
void flash_write_enable();
void flash_bulk_erase();
void flash_64kB_sector_erase(int addr);
void flash_prog(int addr, uint8_t *data, uint32_t n);
void flash_read(int addr, uint8_t *data, uint32_t n);
void flash_wait();
void flash_disable_protection();
