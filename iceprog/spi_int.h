#include <stdint.h>
#include <stdbool.h>

typedef struct {
  void    (*spi_init)(const char *devstr, enum ftdi_interface ifnum);
  void    (*spi_deinit)();
  void    (*set_gpio)(int slavesel_b, int creset_b);
  int     (*get_cdone)();
  void    (*send_spi)(uint8_t *data, int n);
  void    (*xfer_spi)(uint8_t *data, int n);
  uint8_t (*xfer_spi_bits)(uint8_t data, int n);
  void    (*send_49bits)();
  void    (*set_speed)(bool slow_clock);
  void    (*error)(int code);
} spi_interface_t;
