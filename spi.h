#ifndef _SPI_H
#define _SPI_H

/*
 * FT232R (as the lowest FTDI chip supporting sync bitbang mode) has 128 byte
 * receive buffer and 256 byte transmit buffer. It works like 384 byte buffer.
 * See:
 * http://developer.intra2net.com/mailarchive/html/libftdi/2011/msg00410.html
 * http://developer.intra2net.com/mailarchive/html/libftdi/2011/msg00413.html
 * http://jdelfes.blogspot.ru/2014/03/ft232r-bitbang-spi-part-2.html
 */
#define FTDI_MAX_XFER_SIZE      (128 + 256)

#define SPI_LED_OFF     (0)
#define SPI_LED_READ    (1 << 0)
#define SPI_LED_WRITE   (1 << 1)

#define SPI_XFER_READ   (1 << 0)
#define SPI_XFER_WRITE  (1 << 1)

struct spi_port {
    uint16_t vid, pid;
    char manuf[128], desc[128], serial[128];
};

typedef void (*spi_error_cb)(const char *errstr);

#ifdef __cplusplus
extern "C" {
#endif

extern struct spi_port spi_ports[16];
extern int spi_nports;

int spi_init(void);
int spi_deinit(void);

int spi_open(int nport);
int spi_isopen(void);
int spi_close(void);

int spi_xfer_begin(void);
int spi_xfer_8(int cmd, uint8_t *buf, int size);
int spi_xfer_16(int cmd, uint16_t *buf, int size);
int spi_xfer_end(void);

void spi_led(int led);
void spi_set_error_cb(spi_error_cb errcb);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
