#ifndef _SPI_H
#define _SPI_H

#include <stdint.h>

#ifdef ENABLE_LEDS
/* Bit field */
#define SPI_LED_OFF     (0)
#define SPI_LED_READ    (1 << 0)
#define SPI_LED_WRITE   (1 << 1)
#endif

/* Bit field */
#define SPI_XFER_READ   (1 << 0)
#define SPI_XFER_WRITE  (1 << 1)

/* XAP CPU running status */
#define SPI_CPU_STOPPED 1
#define SPI_CPU_RUNNING 0

struct spi_port {
    uint16_t vid, pid;
    char manuf[128], desc[128], serial[128];
    char name[32];
};

#ifdef __cplusplus
extern "C" {
#endif

void spi_set_err_buf(char *buf, size_t sz);

int spi_init(void);
int spi_deinit(void);
int spi_get_port_list(struct spi_port **pportlist, int *pnports);

int spi_open(int nport);
int spi_isopen(void);
int spi_close(void);

int spi_set_clock(unsigned long spi_clk);
void spi_set_max_clock(unsigned long clk);
int spi_clock_slowdown(void);
unsigned long spi_get_max_clock(void);
unsigned long spi_get_clock(void);

int spi_xfer_begin(int get_status);
int spi_xfer(int cmd, int iosize, void *buf, int size);
int spi_xfer_end(void);

#ifdef ENABLE_LEDS
void spi_led(int led);
#endif

#ifdef SPI_STATS
void spi_output_stats(void);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
