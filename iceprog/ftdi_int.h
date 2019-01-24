#include <ftdi.h>
#include "spi_int.h"

extern const spi_interface_t ftdi_int;

typedef struct {
  char *devstr;
  enum ftdi_interface ifnum;
} ftdi_params_t;
