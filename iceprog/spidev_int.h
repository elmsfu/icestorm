#include <linux/types.h>
#include <linux/spi/spidev.h>

#include "spi_int.h"

extern const spi_interface_t spidev_int;

typedef struct {
  char* name;
} spidev_params_t;
