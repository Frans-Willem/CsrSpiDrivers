#ifndef _SPI_H
#define _SPI_H

/*
 * FT232R (as the lowest FTDI chip supporting sync bitbang mode) has 128 byte
 * receive buffer and 256 byte transmit buffer. It works like 384 byte buffer
 * while writing and 128 byte buffer while reading. See:
 * http://developer.intra2net.com/mailarchive/html/libftdi/2011/msg00410.html
 * http://developer.intra2net.com/mailarchive/html/libftdi/2011/msg00413.html
 * http://jdelfes.blogspot.ru/2014/03/ft232r-bitbang-spi-part-2.html
 */

#define FTDI_READ_BUF_SIZE      128
#define FTDI_WRITE_BUF_SIZE     256

/* One bit SPI read requires 2 bytes of buffer space. */
#define SPI_MAX_READ_BYTES      (FTDI_READ_BUF_SIZE / 8 / 2)
/* One bit SPI write requires 3 bytes of buffer space. */
#define SPI_MAX_WRITE_BYTES     ((FTDI_READ_BUF_SIZE + FTDI_WRITE_BUF_SIZE) / 8 / 3)
/* One bit SPI simultaneous read/write requires 3 bytes of buffer space. */
#define SPI_MAX_XFER_BYTES      (FTDI_READ_BUF_SIZE / 8 / 3)

#define SPI_LED_OFF     (0)
#define SPI_LED_READ    (1 << 0)
#define SPI_LED_WRITE   (1 << 1)

#ifdef __cplusplus
extern "C" {
#endif

int spi_open(void);
int spi_isopen(void);
int spi_close(void);

int spi_xfer_begin(void);
int spi_xfer_8(uint8_t *buf, int size);
int spi_write_8(const uint8_t *buf, int size);
int spi_read_8(uint8_t *buf, int size);
int spi_xfer_16(uint16_t *buf, int size);
int spi_write_16(const uint16_t *buf, int size);
int spi_read_16(uint16_t *buf, int size);
int spi_xfer_end(void);

void spi_led(int led);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
