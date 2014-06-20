/*#include <stdio.h>*/
#include <ftdi.h>
#include <stdint.h>
#include <assert.h>
#include "wine/debug.h"

#define SPI_CLOCK_FREQ    100000
#define PIN_MOSI    (1 << 0)   /* FT232RL pin 1, signal TXD AKA D0, output */
#define PIN_MISO    (1 << 1)   /* FT232RL pin 5, signal RXD AKA D1, input */
#define PIN_CLK     (1 << 2)   /* FT232RL pin 3, signal RTS AKA D2, output */
#if 0
#define PIN_MUL     (1 << 3)   /* FT232RL pin 11, signal CTS AKA D3, output */
#endif
#define PIN_CS     (1 << 4)   /* FT232RL pin 2, signal DTR AKA D4, output */
#define PINS_OUTPUT (PIN_MOSI | PIN_CLK | /*PIN_MUL |*/ PIN_CS)

char *ftdi_device_descs[] = {
    "i:0x0403:0x6001",
    "i:0x0403:0x6010",
};

static struct ftdi_context *ftdicp = NULL;
static int spi_nrefs = 0;

static uint8_t ftdi_pin_state = 0;

WINE_DEFAULT_DEBUG_CHANNEL(spilpt);

int spi_set_pins(uint8_t byte)
{
    if (ftdi_write_data(ftdicp, &byte, sizeof(byte)) < 0) {
        WARN("FTDI: write data failed: %s\n", ftdi_get_error_string(ftdicp));
        return -1;
    }
    if (ftdi_read_data(ftdicp, &byte, sizeof(byte)) < 0) {
        WARN("FTDI: read data failed: %s\n", ftdi_get_error_string(ftdicp));
        return -1;
    }

    return 0;
}

int spi_init(void)
{
    /* Set initial pin state: CS high, MISO high as pullup, MOSI and CLK low */
    ftdi_pin_state = ~(PIN_MOSI | PIN_CLK) & (PIN_CS | PIN_MISO);
    return spi_set_pins(ftdi_pin_state);
}

int spi_xfer_begin(void)
{
    uint8_t pin_states[6];
    int i, rc;

    i=0;

    /* Reset sequence: set CS high, wait two clock cycles */

    /* CS should be already high from previous end of transfer or from initialization */
#if 0
    ftdi_pin_state |= PIN_CS;
    pin_states[i++] = ftdi_pin_state;
#endif

    ftdi_pin_state |= PIN_CLK;
    pin_states[i++] = ftdi_pin_state;

    ftdi_pin_state &= ~PIN_CLK;
    pin_states[i++] = ftdi_pin_state;

    ftdi_pin_state |= PIN_CLK;
    pin_states[i++] = ftdi_pin_state;

    ftdi_pin_state &= ~PIN_CLK;
    pin_states[i++] = ftdi_pin_state;

    /* Start transfer */

    ftdi_pin_state &= ~PIN_CS;
    pin_states[i++] = ftdi_pin_state;

    rc = ftdi_write_data(ftdicp, pin_states, i);
    if (rc < 0) {
        WARN("FTDI: write data failed: %s\n", ftdi_get_error_string(ftdicp));
        return -1;
    }
    if (rc != i) {
        WARN("FTDI: short write: %s\n", ftdi_get_error_string(ftdicp));
        return -1;
    }

    /* In FTDI sync bitbang mode every write is preceded by a read to
     * internal buffer. We need to slurp contents of that buffer and
     * discard it. */
    rc = ftdi_read_data(ftdicp, pin_states, i);
    if (rc < 0) {
        WARN("FTDI: read data failed: %s\n", ftdi_get_error_string(ftdicp));
        return -1;
    }
    if (rc != i) {
        WARN("FTDI: short read: %s\n", ftdi_get_error_string(ftdicp));
        return -1;
    }

    return 0;
}

int spi_xfer_end(void)
{
    ftdi_pin_state |= PIN_CS;
    return spi_set_pins(ftdi_pin_state);
}

#define MAX_IO  512

int spi_write(uint8_t *buf, int size)
{
    int i, n, bit, rc;
    uint8_t byte;
    uint8_t pin_states[MAX_IO * 3];

    assert(size < MAX_IO);

    i=0;
    for (n = 0; n < size; n++) {
        byte = buf[n];
        for (bit = (1 << 7); bit != 0; bit >>= 1) {  /* MSB first */
            /* Set output bit */
            if (byte & bit)
                ftdi_pin_state |= PIN_MOSI;
            else
                ftdi_pin_state &= ~PIN_MOSI;
            pin_states[i++] = ftdi_pin_state;

            /* Clock high */
            ftdi_pin_state |= PIN_CLK;
            pin_states[i++] = ftdi_pin_state;

            /* Clock low */
            ftdi_pin_state &= ~PIN_CLK;
            pin_states[i++] = ftdi_pin_state;
        }
    }

    rc = ftdi_write_data(ftdicp, pin_states, i);
    if (rc < 0) {
        WARN("FTDI: write data failed: %s\n", ftdi_get_error_string(ftdicp));
        return -1;
    }
    if (rc != i) {
        WARN("FTDI: short write: %s\n", ftdi_get_error_string(ftdicp));
        return -1;
    }

    /* In FTDI sync bitbang mode every write is preceded by a read to
     * internal buffer. We need to slurp contents of that buffer and
     * discard it. */
    rc = ftdi_read_data(ftdicp, pin_states, i);
    if (rc < 0) {
        WARN("FTDI: read data failed: %s\n", ftdi_get_error_string(ftdicp));
        return -1;
    }
    if (rc != i) {
        WARN("FTDI: short read: %s\n", ftdi_get_error_string(ftdicp));
        return -1;
    }

    return i;
}

int spi_read(uint8_t *buf, int size)
{
    int i, n, bit, rc;
    uint8_t byte;
    uint8_t pin_states[MAX_IO * 2];

    assert(size < MAX_IO);

    /* In FTDI sync bitbang mode we need to write something to device
     * to toggle a read. */

    /* Output series of clock signals for reads. Data is read to internal
     * buffer. */

    ftdi_pin_state &= ~PIN_MOSI;

    for (i = 0; i < size * 8; ) {
        /* Clock high */
        ftdi_pin_state |= PIN_CLK;
        pin_states[i++] = ftdi_pin_state;

        /* Clock low */
        ftdi_pin_state &= ~PIN_CLK;
        pin_states[i++] = ftdi_pin_state;
    }

    rc = ftdi_write_data(ftdicp, pin_states, i);
    if (rc < 0) {
        WARN("FTDI: write data failed: %s\n", ftdi_get_error_string(ftdicp));
        return -1;
    }
    if (rc != i) {
        WARN("FTDI: short write: %s\n", ftdi_get_error_string(ftdicp));
        return -1;
    }

    /* Get data from read buffer */
    rc = ftdi_read_data(ftdicp, pin_states, i);
    if (rc < 0) {
        WARN("FTDI: read data failed: %s\n", ftdi_get_error_string(ftdicp));
        return -1;
    }
    if (rc != i) {
        WARN("FTDI: short read: %s\n", ftdi_get_error_string(ftdicp));
        return -1;
    }

    i=0;
    for (n = 0; n < size; n++) {
        byte = 0;
        for (bit = (1 << 7); bit != 0; bit >>= 1) {  /* MSB first */
            byte <<= 1;
            /* Input bit */
            /* XXX: on which edge of the CLK data should be read? */
            i++;
            if (pin_states[i] & PIN_MISO)
                byte |= bit;
            i++;
        }
        buf[n] = byte;
    }

    return i;
}

int spi_open(void)
{
    int i;
    char *device_desc;

    spi_nrefs++;
    if (spi_nrefs > 1) {
        return 0;
    }

    ftdicp = ftdi_new();
    if (ftdicp == NULL) {
        WARN("FTDI: init failed\n");
        goto init_err;
    }

    ftdi_set_interface(ftdicp, INTERFACE_A); /* XXX for multichannel chips */

    for (i = 0; i < sizeof(ftdi_device_descs); i++) {
        device_desc = ftdi_device_descs[i];
        if (ftdi_usb_open_string(ftdicp, device_desc)) {
            break;
        }
        if (i == sizeof(ftdi_device_descs)) {
            WARN("FTDI: can't find FTDI device\n");
            goto init_err;
        }
    }

    if (ftdi_usb_reset(ftdicp) < 0) {
        WARN("FTDI: reset failed: %s\n", ftdi_get_error_string(ftdicp));
        goto init_err;
    }

    if (ftdi_usb_purge_buffers(ftdicp) < 0) {
        WARN("FTDI: purge buffers failed: %s\n", ftdi_get_error_string(ftdicp));
        goto init_err;
    }

    if (ftdi_set_baudrate(ftdicp, SPI_CLOCK_FREQ / 16) < 0) {
        WARN("FTDI: purge buffers failed: %s\n", ftdi_get_error_string(ftdicp));
        goto init_err;
    }

    if (ftdi_set_bitmode(ftdicp, 0, BITMODE_RESET) < 0) {
        WARN("FTDI: reset bitmode failed: %s\n", ftdi_get_error_string(ftdicp));
        goto init_err;
    }

    if (ftdi_set_bitmode(ftdicp, PINS_OUTPUT, BITMODE_SYNCBB) < 0) {
        WARN("FTDI: set asynchronous bitbang mode failed: %s\n", ftdi_get_error_string(ftdicp));
        goto init_err;
    }

    return 0;

init_err:
    /* XXX close */
    if (ftdicp != NULL) {
        ftdi_free(ftdicp);
    }

    return -1;
}

int spi_close(void)
{
    spi_nrefs--;
    if (spi_nrefs == 0) {
        if (ftdi_usb_close(ftdicp) < 0) {
            WARN("FTDI: close failed: %s\n",
                    ftdi_get_error_string(ftdicp));
            return -1;
        }
        ftdi_free(ftdicp);
        ftdicp = NULL;
    }
    return 0;
}
