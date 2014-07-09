#include <ftdi.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#ifdef SPI_STATS
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#endif
#ifdef __WINE__
#include "wine/debug.h"
#endif

#include "spi.h"
#include "hexdump.h"
#include "compat.h"

/* SPI clock frequency. At maximum I got 15KB/s reads at 8 MHz SPI clock. At
 * 12MHz SPI clock it doesn't work. */
#define SPI_CLOCK_FREQ    8000000
#define SPI_READ_WAIT_INTVL_us   500    /* Microseconds */

/*
 * FTDI bitbang pins:
 * D0 - TXD
 * D1 - RXD
 * D2 - RTS#
 * D3 - CTS#
 * D4 - DTR#
 * D5 - DSR#
 * D6 - DCD#
 * D7 - RI#
 */

/* Pinout. Change it at will. Beware that FTDI adapters provide 5V or 3V3 I/O
 * levels, but CSR chips require 3V3 or 1V8 I/O level. */
#define PIN_nCS     (1 << 0)    /* FT232RL pin 1, signal TXD AKA D0, output */
#define PIN_CLK     (1 << 4)    /* FT232RL pin 2, signal DTR# AKA D4, output */
#define PIN_MOSI    (1 << 1)    /* FT232RL pin 5, signal RXD AKA D1, output */
#define PIN_MISO    (1 << 3)    /* FT232RL pin 11, signal CTS# AKA D3, input */
#define PIN_nLED_RD (1 << 5)    /* FT232RL pin 9, signal DSR# AKA D5, output */
#define PIN_nLED_WR (1 << 6)    /* FT232RL pin 10, signal DCD# AKA D6, output */
#define PINS_OUTPUT (PIN_MOSI | PIN_CLK | PIN_nCS | PIN_nLED_RD | PIN_nLED_WR)

static int spi_dev_open = 0;
static int spi_nrefs = 0;
static spi_error_cb spi_err_cb = NULL;

static struct ftdi_context *ftdicp = NULL;
static uint8_t ftdi_pin_state = 0;

#define SPI_LED_FREQ  4   /* Hz */
static long long spi_led_counter = 0;
static int spi_led_state = 0;

#ifdef SPI_STATS
static struct spi_stats {
    long long trans_usb;
    long long reads, writes;
    long long read_bytes, write_bytes;
    long long read_ticks, write_ticks, misc_ticks;
    long long read_waits;
    struct timeval tv_xfer_begin, tv_xfer;
    struct timeval tv_open_begin, tv_open;
    struct timeval tv_wait_read;
} spi_stats;
#endif

struct ftdi_device_ids {
    uint16_t vid, pid;
};

#define SPI_MAX_PORTS   16
struct spi_port spi_ports[SPI_MAX_PORTS];
int spi_nports = 0;

static struct ftdi_device_ids ftdi_device_ids[] = {
    { 0x0403, 0x6001 }, /* FT232R */
    { 0x0403, 0x6010 }, /* FT2232H/C/D */
    { 0x0403, 0x6011 }, /* FT4232H */
    { 0x0403, 0x6014 }, /* FT232H */
};


#ifdef __WINE__
WINE_DEFAULT_DEBUG_CHANNEL(spilpt);
#else
#define WINE_TRACE(args...)     do { } while(0)
#define WINE_WARN(args...)      do { } while(0)
#define WINE_ERR(args...)       do { } while(0)
#endif

void spi_set_error_cb(spi_error_cb err_cb)
{
    spi_err_cb = err_cb;
}

static void spi_err(const char *fmt, ...) {
    static char buf[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    WINE_ERR("%s\n", buf);
	if (spi_err_cb)
        spi_err_cb(buf);
    va_end(args);
}

static int spi_ftdi_xfer(uint8_t *buf, int len)
{
    int rc;
    uint8_t *bufp;
#ifdef SPI_STATS
    struct timeval tv;
    int num_waits = 0;
#endif

    bufp = buf;

    if (ftdicp == NULL) {
        spi_err("FTDI: no port open");
        return -1;
    }

    rc = ftdi_write_data(ftdicp, bufp, len);
    if (rc < 0) {
        spi_err("FTDI: write data failed: %s", ftdi_get_error_string(ftdicp));
        return -1;
    }
    if (rc != len) {
        spi_err("FTDI: short write: need %d, got %d", len, rc);
        return -1;
    }

#ifdef SPI_STATS
    spi_stats.trans_usb++;
#endif

    /* In FTDI sync bitbang mode every write is preceded by a read to internal
     * buffer. We need to issue read for every write.
     *
     * The data in the read buffer may not be immediately available. Wait for
     * it if needed. */

    while (len > 0) {
        rc = ftdi_read_data(ftdicp, bufp, len);

        if (rc < 0) {
            spi_err("FTDI: read data failed: %s", ftdi_get_error_string(ftdicp));
            return -1;
        }
        if (rc == 0) {
            usleep(SPI_READ_WAIT_INTVL_us);
#ifdef SPI_STATS
            num_waits++;
#endif
        } else {
            len -= rc;
            bufp += rc;

#ifdef SPI_STATS
            spi_stats.trans_usb++;

            if (num_waits) {
                tv.tv_sec = (num_waits * SPI_READ_WAIT_INTVL_us) / 1000000;
                tv.tv_usec = (num_waits * SPI_READ_WAIT_INTVL_us) % 1000000;
                timeradd(&spi_stats.tv_wait_read, &tv, &spi_stats.tv_wait_read);

                spi_stats.read_waits++;

                num_waits = 0;
            }
#endif
        }
    }

    return 0;
}

static inline int spi_set_pins(uint8_t byte)
{
    return spi_ftdi_xfer(&byte, 1);
}

void spi_led(int led) 
{
    spi_led_state = 0;
    if (led & SPI_LED_READ)
        spi_led_state |= PIN_nLED_RD;
    if (led & SPI_LED_WRITE)
        spi_led_state |= PIN_nLED_WR;
}

static int spi_led_tick(int ticks)
{
    if (spi_led_state == 0) {
        if (spi_led_counter != 0) {
            /* LEDs off */
            ftdi_pin_state |= PIN_nLED_WR | PIN_nLED_RD;
            spi_led_counter = 0;
#ifdef SPI_STATS
            spi_stats.misc_ticks++;
#endif
            if (spi_set_pins(ftdi_pin_state) < 0)
                return -1;
        }
    } else {
        if (spi_led_counter > ((SPI_CLOCK_FREQ * 2) / SPI_LED_FREQ))
            spi_led_counter = 0;

        if (spi_led_counter == 0) {
            /* Toggle specified LED(s) */
            ftdi_pin_state ^= spi_led_state;
            /* Turn off the other LED */
            ftdi_pin_state |= ((PIN_nLED_RD | PIN_nLED_WR) & ~spi_led_state);
#ifdef SPI_STATS
            spi_stats.misc_ticks++;
#endif
            if (spi_set_pins(ftdi_pin_state) < 0) {
                ftdi_pin_state |= PIN_nLED_WR | PIN_nLED_RD;
                spi_led_counter = 0;
                return -1;
            }
        }

        spi_led_counter += ticks;
    }

#if 0
    WINE_TRACE("(%08d) state=0x%02x, counter=%08lld, rd=%s, wr=%s\n",
            ticks, spi_led_state, spi_led_counter,
            (ftdi_pin_state & PIN_nLED_RD ? "OFF": "ON "),
            (ftdi_pin_state & PIN_nLED_WR ? "OFF": "ON "));
#endif

    return 0;
}

int spi_xfer_begin(void)
{
    uint8_t pin_states[6];
    int state_offset;

    WINE_TRACE("\n");

#ifdef SPI_STATS
    if (gettimeofday(&spi_stats.tv_xfer_begin, NULL) < 0)
        WINE_WARN("gettimeofday failed: %s\n", strerror(errno));
#endif

    state_offset = 0;

    /* Reset sequence: deassert CS, wait two clock cycles */

    ftdi_pin_state |= PIN_nCS;
    pin_states[state_offset++] = ftdi_pin_state;

    ftdi_pin_state |= PIN_CLK;
    pin_states[state_offset++] = ftdi_pin_state;

    ftdi_pin_state &= ~PIN_CLK;
    pin_states[state_offset++] = ftdi_pin_state;

    ftdi_pin_state |= PIN_CLK;
    pin_states[state_offset++] = ftdi_pin_state;

    ftdi_pin_state &= ~PIN_CLK;
    pin_states[state_offset++] = ftdi_pin_state;

    /* Start transfer */

    ftdi_pin_state &= ~PIN_nCS;
    pin_states[state_offset++] = ftdi_pin_state;

    if (spi_ftdi_xfer(pin_states, state_offset) < 0)
        return -1;

#ifdef SPI_STATS
    spi_stats.misc_ticks += state_offset;
#endif

    return 0;
}

int spi_xfer_end(void)
{
    WINE_TRACE("\n");
    spi_led(SPI_LED_OFF);
    spi_led_tick(0);
    ftdi_pin_state |= PIN_nCS;
    if (spi_set_pins(ftdi_pin_state) < 0)
        return -1;

#ifdef SPI_STATS
    spi_stats.misc_ticks++;

    {
        struct timeval tv;

        if (gettimeofday(&tv, NULL) < 0)
            WINE_WARN("gettimeofday failed: %s\n", strerror(errno));
        timersub(&tv, &spi_stats.tv_xfer_begin, &tv);
        timeradd(&spi_stats.tv_xfer, &tv, &spi_stats.tv_xfer);
    }
#endif

    return 0;
}

int spi_xfer_8(int cmd, uint8_t *buf, int size)
{
    int bytes_left, block_offset, block_size, state_offset;
    uint8_t bit, byte, *bufp;
    uint8_t pin_states[FTDI_MAX_XFER_SIZE];

    WINE_TRACE("(%d, %p, %d)\n", cmd, buf, size);

    spi_led_tick(size * 8 * 2);

    bytes_left = size;
    bufp = buf;
    do {
        block_size = FTDI_MAX_XFER_SIZE / 8 / 2;
        if (block_size > bytes_left)
            block_size = bytes_left;

        /* In FTDI sync bitbang mode we need to write something to device to
         * toggle a read. */

        state_offset = 0;
        for (block_offset = 0; block_offset < block_size; block_offset++) {
            byte = bufp[block_offset];
            for (bit = (1 << 7); bit != 0; bit >>= 1) {  /* MSB first */
                if (cmd & SPI_XFER_WRITE) {
                    /* Set output bit */
                    if (byte & bit)
                        ftdi_pin_state |= PIN_MOSI;
                    else
                        ftdi_pin_state &= ~PIN_MOSI;
                    /* Speed optimization: skip this cycle during write. */
                    /*pin_states[state_offset++] = ftdi_pin_state;*/
                } else {
                    /* Write 0 during a read */
                    ftdi_pin_state &= ~PIN_MOSI;
                }

                /* Clock high */
                ftdi_pin_state |= PIN_CLK;
                pin_states[state_offset++] = ftdi_pin_state;

                /* Clock low */
                ftdi_pin_state &= ~PIN_CLK;
                pin_states[state_offset++] = ftdi_pin_state;
            }
        }

        if (spi_ftdi_xfer(pin_states, state_offset) < 0)
            return -1;

#ifdef SPI_STATS
        if (cmd & SPI_XFER_WRITE) {
            spi_stats.write_ticks += state_offset;
            spi_stats.writes++;
            spi_stats.write_bytes += block_size;
        } else {
            spi_stats.read_ticks += state_offset;
            spi_stats.reads++;
            spi_stats.read_bytes += block_size;
        }
#endif

        if (cmd & SPI_XFER_READ) {
            state_offset = 0;
            for (block_offset = 0; block_offset < block_size; block_offset++) {
                byte = 0;
                for (bit = (1 << 7); bit != 0; bit >>= 1) {  /* MSB first */
                    /* Input bit */
                    if (pin_states[state_offset] & PIN_MISO)
                        byte |= bit;
                    state_offset++;
                    state_offset++;
                    /*state_offset++;*/
                }
                bufp[block_offset] = byte;
            }
        }

        bytes_left -= block_size;
        bufp += block_size;
    } while (bytes_left > 0);

    return size;
}

int spi_xfer_16(int cmd, uint16_t *buf, int size)
{
    int words_left, block_offset, block_size, state_offset;
    uint16_t word, bit, *bufp;
    uint8_t pin_states[FTDI_MAX_XFER_SIZE];

    WINE_TRACE("(%d, %p, %d)\n", cmd, buf, size);

    spi_led_tick(size * 16 * 2);

    words_left = size;
    bufp = buf;
    do {
        block_size = FTDI_MAX_XFER_SIZE / 16 / 2;
        if (block_size > words_left)
            block_size = words_left;

        state_offset = 0;
        for (block_offset = 0; block_offset < block_size; block_offset++) {
            word = bufp[block_offset];
            for (bit = (1 << 15); bit != 0; bit >>= 1) {  /* MSB first */
                if (cmd & SPI_XFER_WRITE) {
                    /* Set output bit */
                    if (word & bit)
                        ftdi_pin_state |= PIN_MOSI;
                    else
                        ftdi_pin_state &= ~PIN_MOSI;
                    /*pin_states[state_offset++] = ftdi_pin_state;*/
                } else {
                    ftdi_pin_state &= ~PIN_MOSI;
                }

                /* Clock high */
                ftdi_pin_state |= PIN_CLK;
                pin_states[state_offset++] = ftdi_pin_state;

                /* Clock low */
                ftdi_pin_state &= ~PIN_CLK;
                pin_states[state_offset++] = ftdi_pin_state;
            }
        }

        if (spi_ftdi_xfer(pin_states, state_offset) < 0)
            return -1;

#ifdef SPI_STATS
        if (cmd & SPI_XFER_WRITE) {
            spi_stats.write_ticks += state_offset;
            spi_stats.writes++;
            spi_stats.write_bytes += block_size * 2;
        } else {
            spi_stats.read_ticks += state_offset;
            spi_stats.reads++;
            spi_stats.read_bytes += block_size * 2;
        }
#endif

        if (cmd & SPI_XFER_READ) {
            state_offset = 0;
            for (block_offset = 0; block_offset < block_size; block_offset++) {
                word = 0;
                for (bit = (1 << 15); bit != 0; bit >>= 1) {  /* MSB first */
                    /* Input bit */
                    if (pin_states[state_offset] & PIN_MISO)
                        word |= bit;
                    state_offset++;
                    state_offset++;
                    /*state_offset++;*/
                }
                bufp[block_offset] = word;
            }
        }

        words_left -= block_size;
        bufp += block_size;
    } while (words_left > 0);

    return size;
}

int spi_enumerate_ports(void)
{
    int id, rc;
    struct ftdi_device_list *ftdevlist, *ftdev;

    spi_nports = 0;

    for (id = 0; id < sizeof(ftdi_device_ids) / sizeof(ftdi_device_ids[0]); id++) {
        WINE_TRACE("find all: 0x%04x:0x%04x\n", ftdi_device_ids[id].vid, ftdi_device_ids[id].pid);
        rc = ftdi_usb_find_all(ftdicp, &ftdevlist, ftdi_device_ids[id].vid, ftdi_device_ids[id].pid);
        if (rc < 0) {
            spi_err("FTDI: ftdi_usb_find_all() failed: %s", ftdi_get_error_string(ftdicp));
            return -1;
        }
        if (rc == 0)
            continue;

        for (ftdev = ftdevlist; ftdev; ftdev = ftdev->next) {
            spi_ports[spi_nports].vid = ftdi_device_ids[id].vid;
            spi_ports[spi_nports].pid = ftdi_device_ids[id].pid;

            if (ftdi_usb_get_strings(ftdicp, ftdev->dev,
                        spi_ports[spi_nports].manuf, sizeof(spi_ports[spi_nports].manuf),
                        spi_ports[spi_nports].desc, sizeof(spi_ports[spi_nports].desc),
                        spi_ports[spi_nports].serial, sizeof(spi_ports[spi_nports].serial)) < 0)
            {
                spi_err("FTDI: ftdi_usb_get_strings() failed: %s", ftdi_get_error_string(ftdicp));
                return -1;
            }
            WINE_TRACE("dev=%p, manuf=\"%s\", desc=\"%s\", serial=\"%s\"\n",
                    ftdev, spi_ports[spi_nports].manuf,
                    spi_ports[spi_nports].desc, spi_ports[spi_nports].serial);

            spi_nports++;
            if (spi_nports >= SPI_MAX_PORTS)
                return 0;
        }
        ftdi_list_free(&ftdevlist);
    }

    return 0;
}

int spi_init(void)
{
    if (ftdicp == NULL) {
        ftdicp = ftdi_new();
        if (ftdicp == NULL) {
            spi_err("FTDI: init failed");
            return -1;
        }
    }

    ftdi_set_interface(ftdicp, INTERFACE_A); /* XXX for multichannel chips */

    if (spi_enumerate_ports() < 0) {
        spi_err("FTDI: port enumeration failed");
        return -1;
    }

    return 0;
}

void spi_deinit(void)
{
    if (ftdicp != NULL) {
        ftdi_free(ftdicp);
        ftdicp = NULL;
    }
}

int spi_open(int nport)
{
    WINE_TRACE("(%d)\n", nport);

    spi_nrefs++;

    if (spi_dev_open > 0) {
        return 0;
    }

    if (ftdicp == NULL) {
        if (spi_init() < 0)
            return -1;
    }

    if (spi_nports == 0 || nport < spi_nports - 1) {
        spi_err("No FTDI device found");
        goto open_err;
    }

#ifdef SPI_STATS
    memset(&spi_stats, 0, sizeof(spi_stats));
    if (gettimeofday(&spi_stats.tv_open_begin, NULL) < 0)
        WINE_WARN("gettimeofday failed: %s\n", strerror(errno));
#endif

    if (ftdi_usb_open_desc(ftdicp, spi_ports[nport].vid, spi_ports[nport].pid,
                NULL, spi_ports[nport].serial) < 0)
    {
        spi_err("FTDI: ftdi_usb_open_desc() failed: %s", ftdi_get_error_string(ftdicp));
        goto open_err;
    }

    WINE_TRACE("FTDI: using FTDI device: \"%s:%s:%s\"\n", spi_ports[nport].manuf,
            spi_ports[nport].desc, spi_ports[nport].serial);

    spi_dev_open++;

    if (ftdi_usb_reset(ftdicp) < 0) {
        spi_err("FTDI: reset failed: %s", ftdi_get_error_string(ftdicp));
        goto open_err;
    }

    if (ftdi_usb_purge_buffers(ftdicp) < 0) {
        spi_err("FTDI: purge buffers failed: %s", ftdi_get_error_string(ftdicp));
        goto open_err;
    }

    /* See FT232R datasheet, section "Baud Rate Generator" and AppNote
     * AN_232R-01, section "Synchronous Bit Bang Mode" */
    if (ftdi_set_baudrate(ftdicp, (SPI_CLOCK_FREQ * 2) / 16) < 0) {
        spi_err("FTDI: set baudrate failed: %s", ftdi_get_error_string(ftdicp));
        goto open_err;
    }

    if (ftdi_set_bitmode(ftdicp, 0, BITMODE_RESET) < 0) {
        spi_err("FTDI: reset bitmode failed: %s", ftdi_get_error_string(ftdicp));
        goto open_err;
    }

    if (ftdi_set_bitmode(ftdicp, PINS_OUTPUT, BITMODE_SYNCBB) < 0) {
        spi_err("FTDI: set synchronous bitbang mode failed: %s", ftdi_get_error_string(ftdicp));
        goto open_err;
    }

    /* Set initial pin state: CS high, MISO high as pullup, MOSI and CLK low, LEDs off */
    ftdi_pin_state = (~(PIN_MOSI | PIN_CLK) & (PIN_nCS | PIN_MISO)) | PIN_nLED_WR | PIN_nLED_RD;
#ifdef SPI_STATS
    spi_stats.misc_ticks++;
#endif
    if (spi_set_pins(ftdi_pin_state) < 0)
        goto open_err;

    return 0;

open_err:
    if (spi_dev_open > 0)
        ftdi_usb_close(ftdicp);
    spi_dev_open = 0;

    return -1;
}

#ifdef SPI_STATS
void spi_output_stats(void)
{
    double xfer_pct, wait_pct;
    double avg_read, avg_write, avg_wait;

    xfer_pct = (spi_stats.tv_xfer.tv_sec * 1000 + spi_stats.tv_xfer.tv_usec / 1000);
    xfer_pct *= 100;
    xfer_pct /= (spi_stats.tv_open.tv_sec * 1000 + spi_stats.tv_open.tv_usec / 1000);

    wait_pct = (spi_stats.tv_wait_read.tv_sec * 1000 + spi_stats.tv_wait_read.tv_usec / 1000);
    wait_pct *= 100;
    wait_pct /= (spi_stats.tv_xfer.tv_sec * 1000 + spi_stats.tv_xfer.tv_usec / 1000);

    avg_wait = (spi_stats.tv_wait_read.tv_sec * 1000000 + spi_stats.tv_wait_read.tv_usec);
    avg_wait /= spi_stats.read_waits;

    avg_read = spi_stats.read_bytes;
    avg_read /= spi_stats.reads;

    avg_write = spi_stats.write_bytes;
    avg_write /= spi_stats.writes;

    if (stderr) {
        fprintf(stderr,
                "Statistics:\n"
                "Time open: %ld.%03ld s\n"
                "Time in xfer: %ld.%03ld s (%.2f%% of open time)\n"
                "Time waiting for data: %ld.%03ld s (%.2f%% of xfer time, %lld waits, %.0f us avg wait time)\n"
                "USB transactions: %lld\n"
                "Reads: %lld (%lld bytes, %.2f bytes avg read size, %lld ticks)\n"
                "Writes: %lld (%lld bytes, %.2f bytes avg write size,  %lld ticks)\n"
                "Misc ticks: %lld\n",
                spi_stats.tv_open.tv_sec, spi_stats.tv_open.tv_usec / 1000,
                spi_stats.tv_xfer.tv_sec, spi_stats.tv_xfer.tv_usec / 1000, xfer_pct,
                spi_stats.tv_wait_read.tv_sec, spi_stats.tv_wait_read.tv_usec / 1000,
                    wait_pct, spi_stats.read_waits, avg_wait,
                spi_stats.trans_usb,
                spi_stats.reads, spi_stats.read_bytes, avg_read, spi_stats.read_ticks,
                spi_stats.writes, spi_stats.write_bytes, avg_write, spi_stats.write_ticks,
                spi_stats.misc_ticks
        );
    }
}
#endif

int spi_close(void)
{
    WINE_TRACE("spi_nrefs=%d, spi_dev_open=%d\n", spi_nrefs, spi_dev_open);
    spi_nrefs--;
    /* pttransport opens us twice, but closes only once. */
    /*if (spi_nrefs == 0) {*/
    if (1) {
        if (ftdicp != NULL) {
            spi_dev_open--;
            if (spi_dev_open == 0) {
                spi_led(SPI_LED_OFF);
                spi_led_tick(0);

                if (ftdi_set_bitmode(ftdicp, 0, BITMODE_RESET) < 0) {
                    spi_err("FTDI: reset bitmode failed: %s",
                            ftdi_get_error_string(ftdicp));
                    return -1;
                }

                if (ftdi_usb_close(ftdicp) < 0) {
                    spi_err("FTDI: close failed: %s",
                            ftdi_get_error_string(ftdicp));
                    return -1;
                }
#ifdef SPI_STATS
                {
                    struct timeval tv;

                    if (gettimeofday(&tv, NULL) < 0)
                        WINE_WARN("gettimeofday failed: %s\n", strerror(errno));
                    timersub(&tv, &spi_stats.tv_open_begin, &tv);
                    timeradd(&spi_stats.tv_open, &tv, &spi_stats.tv_open);
                }

                spi_output_stats();
#endif
                ftdi_free(ftdicp);
                ftdicp = NULL;
            }
        }
    }
    if (spi_nrefs < 0) {
        WINE_WARN("spi_nrefs < 0\n");
        spi_nrefs = 0;
    }
    return 0;
}

int spi_isopen(void)
{
    return spi_nrefs;
}
