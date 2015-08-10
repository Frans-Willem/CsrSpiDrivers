#include <ftdi.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#ifdef SPI_STATS
#include <math.h>
#endif
#include <sys/time.h>
#ifdef __WINE__
#include "wine/debug.h"
#endif

#include "spi.h"
#include "compat.h"
#include "logging.h"

/* FTDI clock frequency. At maximum I got 15KB/s reads at 4 MHz clock. */
#define FTDI_BASE_CLOCK    4000000
#define SPI_BASE_CLOCK     1000
#define SPI_READ_WAIT_INTVL_us   500    /* Microseconds */

/*
 * FT232R (as the lowest FTDI chip supporting sync bitbang mode) has 128 byte
 * receive buffer and 256 byte transmit buffer. It works like 384 byte buffer.
 * See:
 * http://developer.intra2net.com/mailarchive/html/libftdi/2011/msg00410.html
 * http://developer.intra2net.com/mailarchive/html/libftdi/2011/msg00413.html
 * http://jdelfes.blogspot.ru/2014/03/ft232r-bitbang-spi-part-2.html
 */
#define FTDI_MAX_XFER_SIZE      (128 + 256)

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
#define PIN_nCS     (1 << 4)    /* FT232RL pin 2 (DTR#/D4), output */
#define PIN_CLK     (1 << 2)    /* FT232RL pin 3 (RTS#/D2), output */
#define PIN_MOSI    (1 << 7)    /* FT232RL pin 6 (RI#/D7), output */
#define PIN_MISO    (1 << 5)    /* FT232RL pin 9 (DSR#/D5), input */
#define PIN_nLED_RD (1 << 6)    /* FT232RL pin 10 (DCD#/D6), output */
#define PIN_nLED_WR (1 << 3)    /* FT232RL pin 11 (CTS#/D3), output */
#define PINS_OUTPUT (PIN_MOSI | PIN_CLK | PIN_nCS | PIN_nLED_RD | PIN_nLED_WR)

static int spi_dev_open = 0;
static int spi_nrefs = 0;

static struct ftdi_context ftdic;
static uint8_t ftdi_pin_state = 0;

#define SPI_LED_FREQ  10   /* Hz */
static int spi_led_state = 0;
static struct timeval spi_led_start_tv;

#ifdef SPI_STATS
static struct spi_stats {
    long reads, writes;
    long read_bytes, write_bytes;
    struct timeval tv_xfer_begin, tv_xfer;
    struct timeval tv_open_begin, tv_open;
    unsigned long ftdi_clock_min;
    unsigned long spi_clock_max, spi_clock_min;
    unsigned long slowdowns;
} spi_stats;
#endif

struct ftdi_device_ids {
    uint16_t vid, pid;
};

#define SPI_MAX_PORTS   16
struct spi_port spi_ports[SPI_MAX_PORTS];
int spi_nports = 0;

unsigned long spi_clock = SPI_BASE_CLOCK, spi_max_clock = SPI_BASE_CLOCK;
unsigned long spi_ftdi_base_clock = FTDI_BASE_CLOCK;

static struct ftdi_device_ids ftdi_device_ids[] = {
    { 0x0403, 0x6001 }, /* FT232R */
    /* Chips below are not tested. */
    { 0x0403, 0x6010 }, /* FT2232H/C/D */
    { 0x0403, 0x6011 }, /* FT4232H */
    { 0x0403, 0x6014 }, /* FT232H */
};

static char *spi_err_buf = NULL;
static size_t spi_err_buf_sz = 0;

#ifdef __WINE__
WINE_DEFAULT_DEBUG_CHANNEL(spilpt);
#else
#define WINE_TRACE(args...)     do { } while(0)
#define WINE_WARN(args...)      do { } while(0)
#define WINE_ERR(args...)       do { } while(0)
#endif

void spi_set_err_buf(char *buf, size_t sz)
{
    if (buf && sz) {
        spi_err_buf = buf;
        spi_err_buf_sz = sz;
    } else {
        spi_err_buf = NULL;
        spi_err_buf_sz = 0;
    }
}

static void spi_err(const char *fmt, ...) {
    static char buf[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    LOG(ERR, buf);
    if (spi_err_buf) {
        strncpy(spi_err_buf, buf, spi_err_buf_sz);
        spi_err_buf[spi_err_buf_sz - 1] = '\0';
    }
    va_end(args);
}

static int spi_ftdi_xfer(uint8_t *buf, int len)
{
    int rc;
    uint8_t *bufp;

    bufp = buf;

    rc = ftdi_write_data(&ftdic, bufp, len);
    if (rc < 0) {
        spi_err("FTDI: write data failed: %s", ftdi_get_error_string(&ftdic));
        return -1;
    }
    if (rc != len) {
        spi_err("FTDI: short write: need %d, got %d", len, rc);
        return -1;
    }

    /* In FTDI sync bitbang mode every write is preceded by a read to internal
     * buffer. We need to issue read for every write.
     *
     * The data in the read buffer may not be immediately available. Wait for
     * it if needed. */

    while (len > 0) {
        rc = ftdi_read_data(&ftdic, bufp, len);

        if (rc < 0) {
            spi_err("FTDI: read data failed: %s", ftdi_get_error_string(&ftdic));
            return -1;
        }
        if (rc == 0) {
            usleep(SPI_READ_WAIT_INTVL_us);
        } else {
            len -= rc;
            bufp += rc;
        }
    }

    return 0;
}

static inline int spi_set_pins(uint8_t byte)
{
    return spi_ftdi_xfer(&byte, 1);
}

static int spi_led_tick(void)
{
    struct timeval tv;

    if (spi_led_state == 0) {   /* Asked to turn LEDs off */
        ftdi_pin_state |= PIN_nLED_WR | PIN_nLED_RD;
        if (spi_set_pins(ftdi_pin_state) < 0)
            return -1;
    } else {
        if (gettimeofday(&tv, NULL) < 0) {
            LOG(WARN, "gettimeofday failed: %s", strerror(errno));
            return -1;
        }
        timersub(&tv, &spi_led_start_tv, &tv);
        if (((tv.tv_sec * 1000 + tv.tv_usec / 1000) / (1000 / SPI_LED_FREQ / 2)) % 2 == 0) {
            /* Some LED(s) should be on */
            if ((ftdi_pin_state & (PIN_nLED_WR | PIN_nLED_RD)) == (PIN_nLED_WR | PIN_nLED_RD)) {
                ftdi_pin_state ^= spi_led_state;
                if (spi_set_pins(ftdi_pin_state) < 0)
                    return -1;
            }
        } else {
            /* All LEDs should be off */
            if ((ftdi_pin_state & (PIN_nLED_WR | PIN_nLED_RD)) != (PIN_nLED_WR | PIN_nLED_RD)) {
                ftdi_pin_state |= PIN_nLED_WR | PIN_nLED_RD;
                if (spi_set_pins(ftdi_pin_state) < 0)
                    return -1;
            }
        }
    }

    return 0;
}

void spi_led(int led) 
{
    spi_led_state = 0;
    if (led & SPI_LED_READ)
        spi_led_state |= PIN_nLED_RD;
    if (led & SPI_LED_WRITE)
        spi_led_state |= PIN_nLED_WR;

    if (spi_led_state)
        if (gettimeofday(&spi_led_start_tv, NULL) < 0)
            LOG(WARN, "gettimeofday failed: %s", strerror(errno));
    spi_led_tick();
}

int spi_xfer_begin(void)
{
    uint8_t pin_states[32];
    int state_offset;

    LOG(DEBUG, "");

#ifdef SPI_STATS
    if (gettimeofday(&spi_stats.tv_xfer_begin, NULL) < 0)
        LOG(WARN, "gettimeofday failed: %s", strerror(errno));
#endif

    state_offset = 0;

    /* BlueCore chip SPI port reset sequence: deassert CS, wait at least two
     * clock cycles */

    ftdi_pin_state |= PIN_nCS;
    pin_states[state_offset++] = ftdi_pin_state;

    ftdi_pin_state |= PIN_CLK;
    pin_states[state_offset++] = ftdi_pin_state;
    pin_states[state_offset++] = ftdi_pin_state;

    ftdi_pin_state &= ~PIN_CLK;
    pin_states[state_offset++] = ftdi_pin_state;
    pin_states[state_offset++] = ftdi_pin_state;

    ftdi_pin_state |= PIN_CLK;
    pin_states[state_offset++] = ftdi_pin_state;
    pin_states[state_offset++] = ftdi_pin_state;

    ftdi_pin_state &= ~PIN_CLK;
    pin_states[state_offset++] = ftdi_pin_state;
    pin_states[state_offset++] = ftdi_pin_state;

    /* Start transfer */

    ftdi_pin_state &= ~PIN_nCS;
    pin_states[state_offset++] = ftdi_pin_state;
    pin_states[state_offset++] = ftdi_pin_state;

    if (spi_ftdi_xfer(pin_states, state_offset) < 0)
        return -1;

    return 0;
}

int spi_xfer_end(void)
{
    LOG(DEBUG, "");
    ftdi_pin_state |= PIN_nCS;
    if (spi_set_pins(ftdi_pin_state) < 0)
        return -1;

    spi_led(SPI_LED_OFF);

#ifdef SPI_STATS
    {
        struct timeval tv;

        if (gettimeofday(&tv, NULL) < 0)
            LOG(WARN, "gettimeofday failed: %s", strerror(errno));
        timersub(&tv, &spi_stats.tv_xfer_begin, &tv);
        timeradd(&spi_stats.tv_xfer, &tv, &spi_stats.tv_xfer);
    }
#endif

    return 0;
}

/*
 * There is a write speed optimization, that is made in assumption that reading
 * from MOSI on the slave side occurs just a bit after slave sees positive CLK
 * edge, so we can drive MOSI and CLK lines together in one operation, which
 * requires less communication with FTDI chip and hence less time.  However,
 * Vladimir Zidar reported that this doesn't work in his case (BC57F687A via
 * TIAO TUMPA), so it was disabled by default. You can enable such an
 * optimization by defining WRITE_OPTIMIZATION at compile time.
 */
int spi_xfer_8(int cmd, uint8_t *buf, int size)
{
    int bytes_left, block_offset, block_size, state_offset;
    uint8_t bit, byte, *bufp;
    uint8_t pin_states[FTDI_MAX_XFER_SIZE];

    LOG(DEBUG, "(%d, %p, %d)", cmd, buf, size);

    spi_led_tick();

    bytes_left = size;
    bufp = buf;
    do {
#ifdef WRITE_OPTIMIZATION
        block_size = FTDI_MAX_XFER_SIZE / 8 / 2;
#else
        if (cmd & SPI_XFER_WRITE)
            block_size = FTDI_MAX_XFER_SIZE / 8 / 3;
        else
            block_size = FTDI_MAX_XFER_SIZE / 8 / 2;
#endif
        if (block_size > bytes_left)
            block_size = bytes_left;

        /* In FTDI sync bitbang mode we need to write something to device to
         * toggle a read. */

        state_offset = 0;
        for (block_offset = 0; block_offset < block_size; block_offset++) {
            byte = bufp[block_offset];
            for (bit = (1 << 7); bit != 0; bit >>= 1) {
                if (cmd & SPI_XFER_WRITE) {
                    /* Set output bit */
                    if (byte & bit)
                        ftdi_pin_state |= PIN_MOSI;
                    else
                        ftdi_pin_state &= ~PIN_MOSI;
#ifndef WRITE_OPTIMIZATION
                    pin_states[state_offset++] = ftdi_pin_state;
#endif
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
            spi_stats.writes++;
            spi_stats.write_bytes += block_size;
        } else {
            spi_stats.reads++;
            spi_stats.read_bytes += block_size;
        }
#endif

        if (cmd & SPI_XFER_READ) {
            state_offset = 0;
            for (block_offset = 0; block_offset < block_size; block_offset++) {
                byte = 0;
                for (bit = (1 << 7); bit != 0; bit >>= 1) {
                    /* Input bit */
                    if (pin_states[state_offset] & PIN_MISO)
                        byte |= bit;
                    state_offset++;
                    state_offset++;
#ifndef WRITE_OPTIMIZATION
                    if (cmd & SPI_XFER_WRITE)
                        state_offset++;
#endif
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

    LOG(DEBUG, "(%d, %p, %d)", cmd, buf, size);

    spi_led_tick();

    words_left = size;
    bufp = buf;
    do {
#ifdef WRITE_OPTIMIZATION
        block_size = FTDI_MAX_XFER_SIZE / 16 / 2;
#else
        if (cmd & SPI_XFER_WRITE)
            block_size = FTDI_MAX_XFER_SIZE / 16 / 3;
        else
            block_size = FTDI_MAX_XFER_SIZE / 16 / 2;
#endif
        if (block_size > words_left)
            block_size = words_left;

        state_offset = 0;
        for (block_offset = 0; block_offset < block_size; block_offset++) {
            word = bufp[block_offset];
            for (bit = (1 << 15); bit != 0; bit >>= 1) {
                if (cmd & SPI_XFER_WRITE) {
                    /* Set output bit */
                    if (word & bit)
                        ftdi_pin_state |= PIN_MOSI;
                    else
                        ftdi_pin_state &= ~PIN_MOSI;
#ifndef WRITE_OPTIMIZATION
                    pin_states[state_offset++] = ftdi_pin_state;
#endif
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
            spi_stats.writes++;
            spi_stats.write_bytes += block_size * 2;
        } else {
            spi_stats.reads++;
            spi_stats.read_bytes += block_size * 2;
        }
#endif

        if (cmd & SPI_XFER_READ) {
            state_offset = 0;
            for (block_offset = 0; block_offset < block_size; block_offset++) {
                word = 0;
                for (bit = (1 << 15); bit != 0; bit >>= 1) {
                    /* Input bit */
                    if (pin_states[state_offset] & PIN_MISO)
                        word |= bit;
                    state_offset++;
                    state_offset++;
#ifndef WRITE_OPTIMIZATION
                    if (cmd & SPI_XFER_WRITE)
                        state_offset++;
#endif
                }
                bufp[block_offset] = word;
            }
        }

        words_left -= block_size;
        bufp += block_size;
    } while (words_left > 0);

    return size;
}

static int spi_enumerate_ports(void)
{
    int id, rc;
    struct ftdi_device_list *ftdevlist, *ftdev;

    spi_nports = 0;

    for (id = 0; id < sizeof(ftdi_device_ids) / sizeof(ftdi_device_ids[0]); id++) {
        LOG(DEBUG, "find all: 0x%04x:0x%04x", ftdi_device_ids[id].vid, ftdi_device_ids[id].pid);
        rc = ftdi_usb_find_all(&ftdic, &ftdevlist, ftdi_device_ids[id].vid, ftdi_device_ids[id].pid);
        if (rc < 0) {
            spi_err("FTDI: ftdi_usb_find_all() failed: %s", ftdi_get_error_string(&ftdic));
            return -1;
        }
        if (rc == 0)
            continue;

        for (ftdev = ftdevlist; ftdev; ftdev = ftdev->next) {
            spi_ports[spi_nports].vid = ftdi_device_ids[id].vid;
            spi_ports[spi_nports].pid = ftdi_device_ids[id].pid;

            if (ftdi_usb_get_strings(&ftdic, ftdev->dev,
                        spi_ports[spi_nports].manuf, sizeof(spi_ports[spi_nports].manuf),
                        spi_ports[spi_nports].desc, sizeof(spi_ports[spi_nports].desc),
                        spi_ports[spi_nports].serial, sizeof(spi_ports[spi_nports].serial)) < 0)
            {
                spi_err("FTDI: ftdi_usb_get_strings() failed: %s", ftdi_get_error_string(&ftdic));
                return -1;
            }
            LOG(INFO, "Found device: dev=%p, manuf=\"%s\", desc=\"%s\", serial=\"%s\", vid=0x%04x, pid=0x%04x",
                    ftdev, spi_ports[spi_nports].manuf,
                    spi_ports[spi_nports].desc, spi_ports[spi_nports].serial,
                    ftdi_device_ids[id].vid, ftdi_device_ids[id].pid);

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
    LOG(DEBUG, "spi_nrefs=%d, spi_dev_open=%d", spi_nrefs, spi_dev_open);

    if (ftdi_init(&ftdic) < 0) {
        spi_err("FTDI: init failed");
        return -1;
    }

    if (spi_enumerate_ports() < 0)
        return -1;

    spi_nrefs++;

    return 0;
}

int spi_deinit(void)
{
    LOG(DEBUG, "spi_nrefs=%d, spi_dev_open=%d", spi_nrefs, spi_dev_open);

    if (spi_nrefs == 0)
        return 0;

    spi_nrefs--;

    if (spi_nrefs == 0) {
        if (spi_dev_open)
            if (spi_close() < 0)
                return -1;
    }
    if (spi_nrefs < 0) {
        LOG(WARN, "spi_nrefs < 0");
        spi_nrefs = 0;
    }
    return 0;
}

void spi_set_ftdi_base_clock(unsigned long ftdi_clk)
{
    LOG(INFO, "FTDI: setting FTDI_BASE_CLOCK: %lu", ftdi_clk);
    spi_ftdi_base_clock = ftdi_clk;
}

static int spi_set_ftdi_clock(unsigned long ftdi_clk)
{
    LOG(DEBUG, "FTDI: FTDI clock: %lu", ftdi_clk);
    if (!spi_isopen()) {
        LOG(ERR, "FTDI: setting FTDI clock failed: SPI device is not open");
        return -1;
    }

    /*
     * See FT232R datasheet, section "Baud Rate Generator" and AppNote
     * AN_232R-01, section "Synchronous Bit Bang Mode". Also see this thread on
     * bitbang baud rate hardware bug in FTDI chips (XXX is this related to
     * syncbb mode?):
     * http://developer.intra2net.com/mailarchive/html/libftdi/2010/msg00240.html
     */
    LOG(INFO, "FTDI: clock: %lu, baudrate: %lu, FTDI_BASE_CLOCK: %lu",
            ftdi_clk, ftdi_clk / 16, spi_ftdi_base_clock);
    if (ftdi_set_baudrate(&ftdic, ftdi_clk / 16) < 0) {
        spi_err("FTDI: set baudrate %lu failed: %s",
                ftdi_clk / 16, ftdi_get_error_string(&ftdic));
        return -1;
    }

    return 0;
}

int spi_set_clock(unsigned long spi_clk) {
    unsigned long ftdi_clk;
    int rv;

    if (spi_clk > spi_max_clock)
        spi_clk = spi_max_clock;
    LOG(INFO, "FTDI: setting SPI clock: %lu", spi_clk);
    spi_clock = spi_clk;
    ftdi_clk = (spi_clock * spi_ftdi_base_clock) / SPI_BASE_CLOCK;
    rv = spi_set_ftdi_clock(ftdi_clk);
#ifdef SPI_STATS
    if (rv >= 0) {
        /* Don't account for slow cmds, that are executing at SPI clock 20,
         * they are short and not representative */
        if (spi_clock > 20) {
            if (spi_stats.ftdi_clock_min == 0 ||
                        ftdi_clk < spi_stats.ftdi_clock_min)
                spi_stats.ftdi_clock_min = ftdi_clk;
        }
    }
#endif
    return rv;
}

void spi_set_max_clock(unsigned long clk) {
    LOG(INFO, "FTDI: setting SPI max clock: %lu", clk);
    spi_max_clock = clk;
}

int spi_clock_slowdown() {
    unsigned long clk = spi_clock;

    /* Slow SPI clock down by 1.5 */
    clk = (clk * 2) / 3;
    if (clk < 25)
        clk = 25;

#ifdef SPI_STATS
    spi_stats.slowdowns++;
    if(spi_stats.spi_clock_max == 0 || clk > spi_stats.spi_clock_max)
        spi_stats.spi_clock_max = clk;
    if(spi_stats.spi_clock_min == 0 || clk < spi_stats.spi_clock_min)
        spi_stats.spi_clock_min = clk;
#endif

    LOG(INFO, "FTDI: SPI clock slowdown, set SPI clock to %lu", clk);
    return spi_set_clock(clk);
}

unsigned long spi_get_max_clock(void) {
    return spi_max_clock;
}

unsigned long spi_get_clock(void) {
    return spi_clock;
}

int spi_open(int nport)
{
    LOG(DEBUG, "(%d)", nport);

    if (spi_dev_open > 0) {
        return 0;
    }

    if (spi_nports == 0 || nport < spi_nports - 1) {
        spi_err("No FTDI device found");
        goto open_err;
    }

#ifdef SPI_STATS
    memset(&spi_stats, 0, sizeof(spi_stats));
    if (gettimeofday(&spi_stats.tv_open_begin, NULL) < 0)
        LOG(WARN, "gettimeofday failed: %s", strerror(errno));
    spi_stats.spi_clock_max = SPI_BASE_CLOCK;
    spi_stats.spi_clock_min = SPI_BASE_CLOCK;
    spi_stats.ftdi_clock_min = spi_ftdi_base_clock;
#endif

    /*ftdi_set_interface(&ftdic, INTERFACE_A);*/ /* XXX for multichannel chips */

    if (ftdi_usb_open_desc(&ftdic, spi_ports[nport].vid, spi_ports[nport].pid,
                NULL, spi_ports[nport].serial) < 0)
    {
        spi_err("FTDI: ftdi_usb_open_desc() failed: %s", ftdi_get_error_string(&ftdic));
        goto open_err;
    }

    LOG(INFO, "FTDI: using FTDI device: \"%s:%s:%s\"", spi_ports[nport].manuf,
            spi_ports[nport].desc, spi_ports[nport].serial);

    spi_dev_open++;

    if (ftdi_usb_reset(&ftdic) < 0) {
        spi_err("FTDI: reset failed: %s", ftdi_get_error_string(&ftdic));
        goto open_err;
    }

    if (ftdi_usb_purge_buffers(&ftdic) < 0) {
        spi_err("FTDI: purge buffers failed: %s", ftdi_get_error_string(&ftdic));
        goto open_err;
    }

    /* Set 1 ms latency timer, see FTDI AN232B-04 */
    if (ftdi_set_latency_timer(&ftdic, 1) < 0) {
        spi_err("FTDI: setting latency timer failed: %s", ftdi_get_error_string(&ftdic));
        goto open_err;
    }

    if (ftdi_set_bitmode(&ftdic, 0, BITMODE_RESET) < 0) {
        spi_err("FTDI: reset bitmode failed: %s", ftdi_get_error_string(&ftdic));
        goto open_err;
    }

    if (ftdi_set_bitmode(&ftdic, PINS_OUTPUT, BITMODE_SYNCBB) < 0) {
        spi_err("FTDI: set synchronous bitbang mode failed: %s", ftdi_get_error_string(&ftdic));
        goto open_err;
    }

    if (spi_set_ftdi_clock(spi_ftdi_base_clock) < 0)
        goto open_err;

    /* Set initial pin state: CS high, MISO high as pullup, MOSI and CLK low, LEDs off */
    ftdi_pin_state = (~(PIN_MOSI | PIN_CLK) & (PIN_nCS | PIN_MISO)) | PIN_nLED_WR | PIN_nLED_RD;
    if (spi_set_pins(ftdi_pin_state) < 0)
        goto open_err;

    return 0;

open_err:
    if (spi_dev_open > 0)
        ftdi_usb_close(&ftdic);
    spi_dev_open = 0;

    return -1;
}

int spi_isopen(void)
{
    return spi_dev_open ? 1 : 0;
}

#ifdef SPI_STATS
void spi_output_stats(void)
{
    double xfer_pct, avg_read, avg_write, rate;

    xfer_pct = avg_read = avg_write = rate = NAN;

    if (spi_stats.tv_open.tv_sec || spi_stats.tv_open.tv_usec) {
        xfer_pct = (spi_stats.tv_xfer.tv_sec * 1000 + spi_stats.tv_xfer.tv_usec / 1000);
        xfer_pct *= 100;
        xfer_pct /= (spi_stats.tv_open.tv_sec * 1000 + spi_stats.tv_open.tv_usec / 1000);
    }

    if (spi_stats.reads) {
        avg_read = spi_stats.read_bytes;
        avg_read /= spi_stats.reads;
    }

    if (spi_stats.writes) {
        avg_write = spi_stats.write_bytes;
        avg_write /= spi_stats.writes;
    }

    if (spi_stats.tv_xfer.tv_sec || spi_stats.tv_xfer.tv_usec) {
        rate = ((spi_stats.read_bytes + spi_stats.write_bytes) * 1000) /
            (spi_stats.tv_xfer.tv_sec * 1000 + spi_stats.tv_xfer.tv_usec / 1000);
        rate /= 1024;
    }

    if (stderr) {
        fprintf(stderr,
                "*** FTDI Statistics **************************************\n"
                "csr-spi-ftdi version: %s\n"
                "Time open: %ld.%03ld s\n"
                "Time in xfer: %ld.%03ld s (%.2f%% of open time)\n"
                "Reads: %ld (%ld bytes, %.2f bytes avg read size)\n"
                "Writes: %ld (%ld bytes, %.2f bytes avg write size)\n"
                "Xfer data rate: %.2f KB/s (%ld bytes in %ld.%03ld s)\n"
                "FTDI base clock: %lu Hz, min clock: %lu Hz\n"
                "SPI max clock: %lu, min clock: %lu, slowdowns: %lu\n"
                "**********************************************************\n",
                VERSION,
                spi_stats.tv_open.tv_sec, spi_stats.tv_open.tv_usec / 1000,
                spi_stats.tv_xfer.tv_sec, spi_stats.tv_xfer.tv_usec / 1000, xfer_pct,
                spi_stats.reads, spi_stats.read_bytes, avg_read,
                spi_stats.writes, spi_stats.write_bytes, avg_write,
                rate, spi_stats.read_bytes + spi_stats.write_bytes,
                    spi_stats.tv_xfer.tv_sec, spi_stats.tv_xfer.tv_usec / 1000,
                spi_ftdi_base_clock, spi_stats.ftdi_clock_min,
                spi_stats.spi_clock_max, spi_stats.spi_clock_min, spi_stats.slowdowns
        );
    }
}
#endif

int spi_close(void)
{
    LOG(DEBUG, "spi_nrefs=%d, spi_dev_open=%d", spi_nrefs, spi_dev_open);

    if (spi_dev_open == 0)
        return 0;

    spi_dev_open--;
    if (spi_dev_open == 0) {
        spi_led(SPI_LED_OFF);

        if (ftdi_set_bitmode(&ftdic, 0, BITMODE_RESET) < 0) {
            spi_err("FTDI: reset bitmode failed: %s",
                    ftdi_get_error_string(&ftdic));
            return -1;
        }

        if (ftdi_usb_close(&ftdic) < 0) {
            spi_err("FTDI: close failed: %s",
                    ftdi_get_error_string(&ftdic));
            return -1;
        }
#ifdef SPI_STATS
        {
            struct timeval tv;

            if (gettimeofday(&tv, NULL) < 0)
                LOG(WARN, "gettimeofday failed: %s", strerror(errno));
            timersub(&tv, &spi_stats.tv_open_begin, &tv);
            timeradd(&spi_stats.tv_open, &tv, &spi_stats.tv_open);
        }

        spi_output_stats();
#endif
    }
    return 0;
}
