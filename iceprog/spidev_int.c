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

static int spi_fd = -1;

const static char* gpio_export     = "/sys/class/gpio/export";
const static char* gpio_unexport   = "/sys/class/gpio/unexport";
const static char* gpio_value      = "/sys/class/gpio%d/value";
const static char* gpio_dir        = "/sys/class/gpio%d/direction";
const static char* gpio_active_low = "/sys/class/gpio%d/active_low";
const static char* gpio_edge       = "/sys/class/gpio%d/edge";

static uint32_t reset = -1;
static uint32_t done = -1;
static uint32_t ss = -1;

#define BUF_LEN 256

static int gpio_help_write(const char* file, const char* value) {
        int fd = open(file, O_WRONLY);
        write(fd, value, strlen(value));
        close(fd);

        return 0;
}

static int gpio_get(uint32_t pin, uint8_t* value) {
        char buf[BUF_LEN] = "";
        int retval = -1;

        snprintf(buf, BUF_LEN, gpio_value, pin);

        int fd = open(buf, O_RDONLY);
        read(fd, buf, BUF_LEN);
        close(fd);

        switch (buf[0]) {
        case '0':
                retval = 0;
                break;
        case '1':
                retval = 1;
                break;
        default:
                retval = -1;
                break;
        }

        return retval;
}

static int gpio_set(uint32_t pin, uint8_t value) {
        char buf[BUF_LEN] = "";

        snprintf(buf, BUF_LEN, gpio_value, pin);
        if (value == 0) {
                gpio_help_write(buf, "0");
        } else if (value == 1) {
                gpio_help_write(buf, "1");
        } else {
                return -1;
        }

        return 0;
}

static int gpio_init(uint32_t pin, bool out) {
        char buf[BUF_LEN] = "";
        // if not already exported, then export
        snprintf(buf, BUF_LEN, gpio_value, pin);
        if (0 != access(buf, F_OK)) {
                int fd = open(gpio_export, O_WRONLY);
                dprintf(fd, "%d\n", pin);
                close(fd);
        }

        // set up
        snprintf(buf, BUF_LEN, gpio_dir, pin);
        if (out) {
                gpio_help_write(buf, "out");
        } else {
                gpio_help_write(buf, "in");
        }
        snprintf(buf, BUF_LEN, gpio_active_low, pin);
        gpio_help_write(buf, "0");
        snprintf(buf, BUF_LEN, gpio_edge, pin);
        gpio_help_write(buf, "none");

        return 0;
}

static int gpio_deinit(int pin) {
        // unexport
        int fd = open(gpio_unexport, O_WRONLY);
        dprintf(fd, "%d\n", pin);
        close(fd);
}


static void spi_init(const void* params) {

        spidev_params_t* spidev_params = (spidev_params_t*)params;

        reset = spidev_params->reset;
        ss = spidev_params->ss;
        done = spidev_params->done;

        gpio_init(reset, 1);
        gpio_init(ss, 1);
        gpio_init(done, 0);

        spi_fd = open(spidev_params->name, O_RDWR);
        if (spi_fd < 0) {
                perror("open");
                exit(1);
        }

        /* Output only, update data on negative clock edge. */
        __u8  lsb   = 0; // MSB first
        __u8  bits  = 8;
        __u32 mode  = SPI_MODE_3; //
        __u32 speed = 6000000;

        if (ioctl(spi_fd, SPI_IOC_WR_MODE32, &mode) < 0) {
                perror("SPI wr_mode");
                return;
        }
        if (ioctl(spi_fd, SPI_IOC_WR_LSB_FIRST, &lsb) < 0) {
                perror("SPI wr_lsb_fist");
                return;
        }
        if (ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
                perror("SPI bits_per_word");
                return;
        }
        if (ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
                perror("SPI max_speed_hz");
                return;
        }

        // read back
        if (ioctl(spi_fd, SPI_IOC_RD_MODE32, &mode) < 0) {
                perror("SPI rd_mode");
                return;
        }
        if (ioctl(spi_fd, SPI_IOC_RD_LSB_FIRST, &lsb) < 0) {
                perror("SPI rd_lsb_fist");
                return;
        }
        if (ioctl(spi_fd, SPI_IOC_RD_BITS_PER_WORD, &bits) < 0) {
                perror("SPI bits_per_word");
                return;
        }
        if (ioctl(spi_fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed) < 0) {
                perror("SPI max_speed_hz");
                return;
        }

        printf("%s: spi mode 0x%x, %d bits %sper word, %d Hz max\n",
                spidev_params->name, mode, bits, lsb ? "(lsb first) " : "", speed);
}

static void spi_deinit() {
  close(spi_fd);
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

        int status = ioctl(spi_fd, SPI_IOC_MESSAGE(2), xfer);
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

        int status = ioctl(spi_fd, SPI_IOC_MESSAGE(2), xfer);
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
        // set select and reset
        gpio_set(ss, slavesel_b);
        gpio_set(reset, creset_b);
}

static int get_cdone()
{
        uint8_t data = 0;
        //read cdone gpio
        gpio_get(done, &data);

        return data;
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

        if (ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
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
