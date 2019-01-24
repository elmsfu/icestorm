#include "spidev_int.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

static int fd = -1;

static void spi_init(const void* params) {

        spidev_params_t* spidev_params = (spidev_params_t*)params;

        fd = open(spidev_params->name, O_RDWR);
        if (fd < 0) {
                perror("open");
                exit(1);
        }

        /* Output only, update data on negative clock edge. */
        __u8  lsb   = 0; // MSB first
        __u8  bits  = 8;
        __u32 mode  = SPI_MODE_3; //
        __u32 speed = 6000000;

        if (ioctl(fd, SPI_IOC_WR_MODE32, &mode) < 0) {
                perror("SPI wr_mode");
                return;
        }
        if (ioctl(fd, SPI_IOC_WR_LSB_FIRST, &lsb) < 0) {
                perror("SPI wr_lsb_fist");
                return;
        }
        if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
                perror("SPI bits_per_word");
                return;
        }
        if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
                perror("SPI max_speed_hz");
                return;
        }

        // read back
        if (ioctl(fd, SPI_IOC_RD_MODE32, &mode) < 0) {
                perror("SPI rd_mode");
                return;
        }
        if (ioctl(fd, SPI_IOC_RD_LSB_FIRST, &lsb) < 0) {
                perror("SPI rd_lsb_fist");
                return;
        }
        if (ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits) < 0) {
                perror("SPI bits_per_word");
                return;
        }
        if (ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed) < 0) {
                perror("SPI max_speed_hz");
                return;
        }

        printf("%s: spi mode 0x%x, %d bits %sper word, %d Hz max\n",
                spidev_params->name, mode, bits, lsb ? "(lsb first) " : "", speed);
}

static void spi_deinit() {
  close(fd);
}


static void error(int status)
{
        fprintf(stderr, "ABORT.\n");
        spi_deinit();
        exit(status);
}


static void send_spi(uint8_t *data, int n)
{
        if (n < 1)
                return;

        struct spi_ioc_transfer xfer[2];
        memset(xfer, 0, sizeof xfer);

        xfer[0].tx_buf = data;
        xfer[0].len = n;

        int status = ioctl(fd, SPI_IOC_MESSAGE(2), xfer);
        if (status < 0) {
                perror("SPI_IOC_MESSAGE");
                return;
        }

}

static void xfer_spi(uint8_t *data, int n)
{
        if (n < 1)
                return;

        struct spi_ioc_transfer xfer[2];
        memset(xfer, 0, sizeof xfer);

        xfer[0].tx_buf = data;
        xfer[0].len = n;
        xfer[1].rx_buf = data;
        xfer[1].len = n;

        int status = ioctl(fd, SPI_IOC_MESSAGE(2), xfer);
        if (status < 0) {
                perror("SPI_IOC_MESSAGE");
                return;
        }

        /* Input and output, update data on negative edge read on positive. */
}

static uint8_t xfer_spi_bits(uint8_t data, int n)
{
        if (n < 1)
                return 0;

        xfer_spi(&data, (n+7)/8);
        return data;
}

static void set_gpio(int slavesel_b, int creset_b)
{
        uint8_t gpio = 0;
        // set select and reset
}

static int get_cdone()
{
        uint8_t data;
        //read cdone gpio

        return 1;
}

static void send_49bits()
{
        size_t nb = 49;
        uint8_t dummy[(49+7)/8];
        xfer_spi(dummy, nb/8);
        xfer_spi_bits(0, nb - (nb/8)*8);

}

static void set_speed(bool slow_clock)
{
        // set 6 MHz clock
        uint32_t speed = 6000000;
        if (slow_clock) {
                // set 50 kHz clock
                speed = 50000;
        }

        if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
                perror("SPI max_speed_hz");
                return;
        }
}

const spi_interface_t spidev_int =
{
  .spi_init      = spi_init,
  .spi_deinit    = spi_deinit,
  .set_gpio      = set_gpio,
  .get_cdone     = get_cdone,
  .send_spi      = send_spi,
  .xfer_spi      = xfer_spi,
  .xfer_spi_bits = xfer_spi_bits,
  .set_speed     = set_speed,
  .send_49bits   = send_49bits,
  .error         = error,
};
