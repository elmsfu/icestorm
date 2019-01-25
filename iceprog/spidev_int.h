#include <linux/types.h>
#include <linux/spi/spidev.h>

#include "spi_int.h"

extern const spi_interface_t spidev_int;

typedef struct {
  char* name;
  uint32_t reset;
  uint32_t ss;
  uint32_t done;
} spidev_params_t;
