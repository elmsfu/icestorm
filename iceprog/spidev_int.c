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
const static char* gpio_value      = "/sys/class/gpio/gpio%d/value";
const static char* gpio_dir        = "/sys/class/gpio/gpio%d/direction";
const static char* gpio_active_low = "/sys/class/gpio/gpio%d/active_low";
const static char* gpio_edge       = "/sys/class/gpio/gpio%d/edge";

static uint32_t reset = -1;
static uint32_t done = -1;
static uint32_t ss = -1;

#define SLOW_SPEED 600000
#define BUF_LEN 256

static int gpio_help_write(const char* file, const char* value) {
        int fd = open(file, O_WRONLY);
        if (fd < 0) {
	  fprintf(stderr, "%s(%s, %s): ", __PRETTY_FUNCTION__, file, value);
	  perror("open");
                exit(1);
        }
	
        int ret = write(fd, value, strlen(value));
        if (ret < 0) {
                perror("write");
                exit(1);
        }
	
        ret = close(fd);
        if (ret < 0) {
                perror("close");
                exit(1);
        }

        return 0;
}

static int gpio_get(uint32_t pin, uint8_t* value) {
        char buf[BUF_LEN] = "";
        int retval = -1;

        snprintf(buf, BUF_LEN, gpio_value, pin);

        int fd = open(buf, O_RDONLY);
        if (fd < 0) {
                perror("open get_gpio");
                exit(1);
        }
	
        int ret = read(fd, buf, BUF_LEN);
	if (ret < 0) {
                perror("write");
                exit(1);
        }

        ret = close(fd);
        if (ret < 0) {
                perror("write");
                exit(1);
        }

        switch (buf[0]) {
        case '0':
                retval = 0;
		*value = 0;
                break;
        case '1':
                retval = 1;
		*value = 1;
                break;
        default:
                retval = -1;
		fprintf(stderr, "failed to read gpio pin(%d): %s\n", pin, buf);
                break;
        }

	fprintf(stderr, "read gpio pin(%d): %d %c\n", pin, *value, buf[0]);

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
		int ret = dprintf(fd, "%d\n", pin);
                if (ret < 0) {
		  perror("dprintf export");
		}
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

	fprintf(stderr, "r,s,d: %d, %d %d\n", reset, ss, done);
	
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
        __u32 mode  = SPI_MODE_3 | SPI_NO_CS;
        __u32 speed = SLOW_SPEED;

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

	speed = -1;
	mode = -1;
	bits = -1;
	lsb = -1;
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


static void xfer_spi(uint8_t *txdata, uint32_t ntx, uint8_t* rxdata, uint32_t nrx)
{
        if (ntx + nrx < 1)
                return;

        struct spi_ioc_transfer xfer[2];
        memset(xfer, 0, sizeof(xfer));

	xfer[0].speed_hz = SLOW_SPEED;
        xfer[0].tx_buf = txdata;
        xfer[0].len = ntx;
	//xfer[0].cs_change = 1;

	xfer[1].speed_hz = SLOW_SPEED;
        xfer[1].rx_buf = rxdata;
	xfer[1].len = nrx;

        int status = ioctl(spi_fd, SPI_IOC_MESSAGE(2), xfer);
        if (status < 0) {
                perror("SPI_IOC_MESSAGE");
                return;
        }
}

static uint8_t xfer_spi_bits(uint8_t data, int n)
{
        if (n < 1 || n >8)
                return 0;

	//TODO: use bits_per_word
        xfer_spi(&data, 1, &data, 1);
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
        int ret = gpio_get(done, &data);

        return data;
}

static void send_49bits()
{
        size_t nb = 49;
        uint8_t dummy[(49+7)/8];
        xfer_spi(dummy, nb/8, NULL, 0);
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
  .xfer_spi      = xfer_spi,
  .xfer_spi_bits = xfer_spi_bits,
  .set_speed     = set_speed,
  .send_49bits   = send_49bits,
  .error         = error,
};
