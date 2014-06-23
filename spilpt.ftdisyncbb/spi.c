/*#include <stdio.h>*/
#include <ftdi.h>
#include <stdint.h>
#include <assert.h>
#include "wine/debug.h"

#include "spi.h"
#include "hexdump.h"

#define SPI_CLOCK_FREQ    100000
/* This pinout is done so, that popular FT232R adapters could be used. Change
 * it at will. Beware, there are adapters providing 5V output, but CSR chips
 * require 3V3 or 1V8 */
#define PIN_MOSI    (1 << 0)    /* FT232RL pin 1, signal TXD AKA D0, output */
#define PIN_MISO    (1 << 1)    /* FT232RL pin 5, signal RXD AKA D1, input */
#define PIN_CLK     (1 << 2)    /* FT232RL pin 3, signal RTS AKA D2, output */
#if 0
/* Multiplexing is not implemented. */
#define PIN_MUL     (1 << 3)    /* FT232RL pin 11, signal CTS AKA D3, output */
#endif
#define PIN_nCS      (1 << 4)    /* FT232RL pin 2, signal DTR AKA D4, output */
#define PIN_nLED_WR  (1 << 7)    /* FT232RL pin 6, signal RI AKA D7, output */
#define PIN_nLED_RD  (1 << 5)    /* FT232RL pin 9, signal DSR AKA D5, output */
#define PINS_OUTPUT (PIN_MOSI | PIN_CLK | /*PIN_MUL |*/ PIN_nCS | PIN_nLED_WR | PIN_nLED_RD)

char *ftdi_device_descs[] = {
    "i:0x0403:0x6001",
    "i:0x0403:0x6010",
};

static struct ftdi_context *ftdicp = NULL;
static int spi_dev_open = 0;
static int spi_nrefs = 0;

static uint8_t ftdi_pin_state = 0;

#define SPI_LED_CLK_PERIOD  (SPI_CLOCK_FREQ / 30)
static long long spi_led_counter = 0;
static int spi_led_state = 0;

WINE_DEFAULT_DEBUG_CHANNEL(spilpt);

static int spi_ftdi_xfer(uint8_t *buf, int len)
{
    int rc;
    uint8_t *bufp;

    bufp = buf;

    if (ftdicp == NULL) {
        WINE_WARN("FTDI: no port open\n");
        return -1;
    }

    rc = ftdi_write_data(ftdicp, bufp, len);
    if (rc < 0) {
        WINE_WARN("FTDI: write data failed: %s\n", ftdi_get_error_string(ftdicp));
        return -1;
    }
    if (rc != len) {
        WINE_WARN("FTDI: short write: need %d, got %d\n", len, rc);
        return -1;
    }

    /* In FTDI sync bitbang mode every write is preceded by a read to internal
     * buffer. We need to slurp contents of that buffer and discard it. */
    while (len > 0) {
        rc = ftdi_read_data(ftdicp, bufp, len);

        if (rc < 0) {
            WINE_WARN("FTDI: read data failed: %s\n", ftdi_get_error_string(ftdicp));
            return -1;
        }
        if (rc == 0)
            usleep(5000);
        len -= rc;
        bufp += rc;
    }

    return 0;
}

static int spi_set_pins(uint8_t byte)
{
    if (spi_ftdi_xfer(&byte, 1) < 0)
        return -1;

    return 0;
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
            if (spi_set_pins(ftdi_pin_state) < 0)
                return -1;
        }
    } else {
        if (spi_led_counter > SPI_LED_CLK_PERIOD)
            spi_led_counter = 0;

        if (spi_led_counter == 0) {
            /* Toggle specified LED(s) */
            ftdi_pin_state ^= spi_led_state;
            /* Turn off the other LED */
            ftdi_pin_state |= ((PIN_nLED_RD | PIN_nLED_WR) & ~spi_led_state);
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

static int spi_init(void)
{
    /* Set initial pin state: CS high, MISO high as pullup, MOSI and CLK low, LEDs off */
    ftdi_pin_state = (~(PIN_MOSI | PIN_CLK) & (PIN_nCS | PIN_MISO)) | PIN_nLED_WR | PIN_nLED_RD;
    return spi_set_pins(ftdi_pin_state);
}

int spi_xfer_begin(void)
{
    uint8_t pin_states[6];
    int state_offset;

    WINE_TRACE("\n");

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

    return 0;
}

int spi_xfer_end(void)
{
    WINE_TRACE("\n");
    spi_led(SPI_LED_OFF);
    spi_led_tick(0);
    ftdi_pin_state |= PIN_nCS;
    return spi_set_pins(ftdi_pin_state);
}

int spi_xfer_8(uint8_t *buf, int size)
{
    int bytes_left, block_offset, block_size, state_offset;
    uint8_t bit, byte, *bufp;
    uint8_t pin_states[SPI_MAX_XFER_BYTES * 8 * 3];

    WINE_TRACE("(%p, %d). input buf dump:\n", buf, size);
    hexdump(buf, size);

    spi_led_tick(size * 8 * 3);

    bytes_left = size;
    bufp = buf;
    do {
        block_size = SPI_MAX_XFER_BYTES;
        if (block_size > bytes_left)
            block_size = bytes_left;

        state_offset = 0;
        for (block_offset = 0; block_offset < block_size; block_offset++) {
            byte = bufp[block_offset];
            for (bit = (1 << 7); bit != 0; bit >>= 1) {  /* MSB first */
                /* Set output bit */
                if (byte & bit)
                    ftdi_pin_state |= PIN_MOSI;
                else
                    ftdi_pin_state &= ~PIN_MOSI;
                pin_states[state_offset++] = ftdi_pin_state;

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

        state_offset = 0;
        for (block_offset = 0; block_offset < block_size; block_offset++) {
            byte = 0;
            for (bit = (1 << 7); bit != 0; bit >>= 1) {  /* MSB first */
                /* XXX: on which cycle data should be read from MISO? */
                /* Input bit */
                if (pin_states[state_offset] & PIN_MISO)
                    byte |= bit;
                state_offset++;
                state_offset++;
                state_offset++;
            }
            bufp[block_offset] = byte;
        }

        bytes_left -= block_size;
        bufp += block_size;
    } while (bytes_left > 0);

    WINE_TRACE("(buf, %d). output buf dump:\n", size);
    hexdump(buf, size);

    return size;
}

int spi_write_8(const uint8_t *buf, int size)
{
    int bytes_left, block_offset, block_size, state_offset;
    uint8_t byte, bit;
    const uint8_t *bufp;
    uint8_t pin_states[SPI_MAX_WRITE_BYTES * 8 * 3];

    WINE_TRACE("(%p, %d). input buf dump:\n", buf, size);
    hexdump(buf, size);

    spi_led_tick(size * 8 * 3);

    bytes_left = size;
    bufp = buf;
    do {
        block_size = SPI_MAX_WRITE_BYTES;
        if (block_size > bytes_left)
            block_size = bytes_left;

        state_offset = 0;
        for (block_offset = 0; block_offset < block_size; block_offset++) {
            byte = bufp[block_offset];
            for (bit = (1 << 7); bit != 0; bit >>= 1) {  /* MSB first */
                /* Set output bit */
                if (byte & bit)
                    ftdi_pin_state |= PIN_MOSI;
                else
                    ftdi_pin_state &= ~PIN_MOSI;
                pin_states[state_offset++] = ftdi_pin_state;

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

        bytes_left -= block_size;
        bufp += block_size;
    } while (bytes_left > 0);

    return size;
}

int spi_read_8(uint8_t *buf, int size)
{
    int bytes_left, block_offset, block_size, state_offset;
    uint8_t byte, bit, *bufp;
    uint8_t pin_states[SPI_MAX_READ_BYTES * 8 * 2];

    WINE_TRACE("(%p, %d)\n", buf, size);

    spi_led_tick(size * 8 * 2);

    /* Write 0 during a read */
    ftdi_pin_state &= ~PIN_MOSI;

    bytes_left = size;
    bufp = buf;
    do {
        block_size = SPI_MAX_READ_BYTES;
        if (block_size > bytes_left)
            block_size = bytes_left;

        /* In FTDI sync bitbang mode we need to write something to device
         * to toggle a read. */

        /* Output series of clock signals for reads. Data is read to internal
         * buffer. */

        for (state_offset = 0; state_offset < block_size * 8 * 2; ) {
            /* Clock high */
            ftdi_pin_state |= PIN_CLK;
            pin_states[state_offset++] = ftdi_pin_state;

            /* Clock low */
            ftdi_pin_state &= ~PIN_CLK;
            pin_states[state_offset++] = ftdi_pin_state;
        }

        if (spi_ftdi_xfer(pin_states, state_offset) < 0)
            return -1;

        state_offset = 0;
        for (block_offset = 0; block_offset < block_size; block_offset++) {
            byte = 0;
            for (bit = (1 << 7); bit != 0; bit >>= 1) {  /* MSB first */
                /* XXX: on which edge of the CLK data should be read? */
                /* Looks like we need to read MISO on next cycle after raising
                 * the clock */
                /* Input bit */
                if (pin_states[state_offset] & PIN_MISO)
                    byte |= bit;
                state_offset++;
                state_offset++;
            }
            bufp[block_offset] = byte;
        }

        bytes_left -= block_size;
        bufp += block_size;
    } while (bytes_left > 0);

    WINE_TRACE("(%p, %d). output buf dump:\n", buf, size);
    hexdump(buf, size);

    return size;
}

int spi_xfer_16(uint16_t *buf, int size)
{
    int words_left, block_offset, block_size, state_offset;
    uint16_t word, bit, *bufp;
    uint8_t pin_states[SPI_MAX_XFER_BYTES * 8 * 3];

    WINE_TRACE("(%p, %d). input buf dump:\n", buf, size);
    hexdump((void *)buf, size * 2);

    spi_led_tick(size * 16 * 3);

    words_left = size;
    bufp = buf;
    do {
        block_size = SPI_MAX_XFER_BYTES / 2;
        if (block_size > words_left)
            block_size = words_left;

        state_offset = 0;
        for (block_offset = 0; block_offset < block_size; block_offset++) {
            word = bufp[block_offset];
            for (bit = (1 << 15); bit != 0; bit >>= 1) {  /* MSB first */
                /* Set output bit */
                if (word & bit)
                    ftdi_pin_state |= PIN_MOSI;
                else
                    ftdi_pin_state &= ~PIN_MOSI;
                pin_states[state_offset++] = ftdi_pin_state;

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

        state_offset = 0;
        for (block_offset = 0; block_offset < block_size; block_offset++) {
            word = 0;
            for (bit = (1 << 15); bit != 0; bit >>= 1) {  /* MSB first */
                /* XXX: on which cycle data should be read from MISO? */
                /* Input bit */
                if (pin_states[state_offset] & PIN_MISO)
                    word |= bit;
                state_offset++;
                state_offset++;
                state_offset++;
            }
            bufp[block_offset] = word;
        }

        words_left -= block_size;
        bufp += block_size;
    } while (words_left > 0);

    WINE_TRACE("(%p, %d). output buf dump:\n", buf, size);
    hexdump((void *)buf, size * 2);

    return size;
}

int spi_write_16(const uint16_t *buf, int size)
{
    int words_left, block_offset, block_size, state_offset;
    uint16_t word, bit;
    const uint16_t *bufp;
    uint8_t pin_states[SPI_MAX_WRITE_BYTES * 8 * 3];

    WINE_TRACE("(%p, %d). input buf dump:\n", buf, size);
    hexdump((void *)buf, size * 2);

    spi_led_tick(size * 16 * 3);

    words_left = size;
    bufp = buf;
    do {
        block_size = SPI_MAX_WRITE_BYTES / 2;
        if (block_size > words_left)
            block_size = words_left;

        state_offset = 0;
        for (block_offset = 0; block_offset < block_size; block_offset++) {
            word = bufp[block_offset];
            for (bit = (1 << 15); bit != 0; bit >>= 1) {  /* MSB first */
                /* Set output bit */
                if (word & bit)
                    ftdi_pin_state |= PIN_MOSI;
                else
                    ftdi_pin_state &= ~PIN_MOSI;
                pin_states[state_offset++] = ftdi_pin_state;

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

        words_left -= block_size;
        bufp += block_size;
    } while (words_left > 0);

    return size;
}

int spi_read_16(uint16_t *buf, int size)
{
    int words_left, block_offset, block_size, state_offset;
    uint16_t word, bit, *bufp;
    uint8_t pin_states[SPI_MAX_READ_BYTES * 8 * 2];

    WINE_TRACE("(%p, %d)\n", buf, size);

    spi_led_tick(size * 16 * 2);

    /* Write 0 during a read */
    ftdi_pin_state &= ~PIN_MOSI;

    words_left = size;
    bufp = buf;
    do {
        block_size = SPI_MAX_READ_BYTES / 2;
        if (block_size > words_left)
            block_size = words_left;

        /* In FTDI sync bitbang mode we need to write something to device
         * to toggle a read. */

        /* Output series of clock signals for reads. Data is read to internal
         * buffer. */

        for (state_offset = 0; state_offset < block_size * 16 * 2; ) {
            /* Clock high */
            ftdi_pin_state |= PIN_CLK;
            pin_states[state_offset++] = ftdi_pin_state;

            /* Clock low */
            ftdi_pin_state &= ~PIN_CLK;
            pin_states[state_offset++] = ftdi_pin_state;
        }

        if (spi_ftdi_xfer(pin_states, state_offset) < 0)
            return -1;

        state_offset = 0;
        for (block_offset = 0; block_offset < block_size; block_offset++) {
            word = 0;
            for (bit = (1 << 15); bit != 0; bit >>= 1) {  /* MSB first */
                /* XXX: on which edge of the CLK data should be read? */
                /* Looks like we need to read MISO on next cycle after raising
                 * the clock */
                /* Input bit */
                if (pin_states[state_offset] & PIN_MISO)
                    word |= bit;
                state_offset++;
                state_offset++;
            }
            bufp[block_offset] = word;
        }

        words_left -= block_size;
        bufp += block_size;
    } while (words_left > 0);

    WINE_TRACE("(%p, %d). output buf dump:\n", buf, size);
    hexdump((void *)buf, size * 2);

    return size;
}

int spi_open(void)
{
    int i, rc;
    char *device_desc;

    WINE_TRACE("spi_open\n");

    spi_nrefs++;

    if (spi_dev_open > 0) {
        return 0;
    }

    if (ftdicp == NULL) {
        ftdicp = ftdi_new();
        if (ftdicp == NULL) {
            WINE_WARN("FTDI: init failed\n");
            goto init_err;
        }

        ftdi_set_interface(ftdicp, INTERFACE_A); /* XXX for multichannel chips */
    }

#if 0
    for (i = 0; i < sizeof(ftdi_device_descs); i++) {
        device_desc = ftdi_device_descs[i];
        if (ftdi_usb_open_string(ftdicp, device_desc) == 0) {
            WINE_TRACE("spi_open: found device: %s\n", device_desc);
            break;
        }
        if (i == sizeof(ftdi_device_descs)) {
            WINE_WARN("FTDI: can't find FTDI device\n");
            goto init_err;
        }
    }
#endif

    rc = ftdi_usb_open(ftdicp, 0x0403, 0x6001);
    if (rc < 0) {
        WINE_WARN("ftdi_usb_open(): can't open USB device: %d\n", rc);
        goto init_err;
    }
    WINE_TRACE("opened 0x0403:0x6001\n");

    spi_dev_open++;

    if (ftdi_usb_reset(ftdicp) < 0) {
        WINE_WARN("FTDI: reset failed: %s\n", ftdi_get_error_string(ftdicp));
        goto init_err;
    }

    if (ftdi_usb_purge_buffers(ftdicp) < 0) {
        WINE_WARN("FTDI: purge buffers failed: %s\n", ftdi_get_error_string(ftdicp));
        goto init_err;
    }

    if (ftdi_set_baudrate(ftdicp, SPI_CLOCK_FREQ / 16) < 0) {
        WINE_WARN("FTDI: purge buffers failed: %s\n", ftdi_get_error_string(ftdicp));
        goto init_err;
    }

    if (ftdi_set_bitmode(ftdicp, 0, BITMODE_RESET) < 0) {
        WINE_WARN("FTDI: reset bitmode failed: %s\n", ftdi_get_error_string(ftdicp));
        goto init_err;
    }

    if (ftdi_set_bitmode(ftdicp, PINS_OUTPUT, BITMODE_SYNCBB) < 0) {
        WINE_WARN("FTDI: set synchronous bitbang mode failed: %s\n", ftdi_get_error_string(ftdicp));
        goto init_err;
    }

    if (spi_init() < 0)
        goto init_err;

    return 0;

init_err:
    if (ftdicp != NULL) {
        if (spi_dev_open > 0)
            ftdi_usb_close(ftdicp);
        spi_dev_open = 0;

        ftdi_free(ftdicp);
        ftdicp = NULL;
    }

    return -1;
}

int spi_close(void)
{
    WINE_TRACE("close");
    spi_nrefs--;
    if (spi_nrefs == 0) {
        if (ftdicp != NULL) {
            spi_dev_open--;
            if (spi_dev_open == 0) {
                spi_led(SPI_LED_OFF);
                spi_led_tick(0);
                if (ftdi_usb_close(ftdicp) < 0) {
                    WINE_WARN("FTDI: close failed: %s\n",
                            ftdi_get_error_string(ftdicp));
                    return -1;
                }
                ftdi_free(ftdicp);
                ftdicp = NULL;
            }
        }
    }
    if (spi_nrefs < 0) {
        WINE_WARN("spi_close(): spi_nrefs < 0\n");
        spi_nrefs = 0;
    }
    return 0;
}

int spi_isopen(void)
{
    return spi_nrefs;
}
