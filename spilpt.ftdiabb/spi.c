/*#include <stdio.h>*/
#include <ftdi.h>
#include "wine/debug.h"

#define FREQ    100000
#define BV_MOSI 0x01//Pin 0 "TXD"
#define BV_MISO 0x02//Pin 1 "RXD" - INPUT
#define BV_CLK  0x04//Pin 2 "RTS"
#define BV_MUL  0x08//Pin 3 "CTS"
#define BV_CSB  0x10//Pin 4 "DTR"
#define BV_ALLOUT (BV_MOSI | BV_CLK | BV_MUL | BV_CSB)//These are all output pins

char ftdi_device_descs[][] = {
    "i:0x0403:0x6001",
    "i:0x0403:0x6010",
};

static struct ftdi_context *ftdicp = NULL;
static int spi_nrefs = 0;

WINE_DEFAULT_DEBUG_CHANNEL(spilpt);

int spi_open()
{
    int i;

    spi_nrefs++;
    if (ftbb_refs > 1) {
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
    
    if (ftdi_set_baudrate(ftdicp, FREQ / 16) < 0) {
        WARN("FTDI: purge buffers failed: %s\n", ftdi_get_error_string(ftdicp));
        goto init_err;
    }
    
    if (ftdi_set_bitmode(ftdicp, 0, BITMODE_RESET) < 0) {
        WARN("FTDI: reset bitmode failed: %s\n", ftdi_get_error_string(ftdicp));
        goto init_err;
    }
    
    if (ftdi_set_bitmode(ftdicp, BV_ALLOUT, BITMODE_BITBANG) < 0) {
        WARN("FTDI: set asynchronous bitbang mode failed: %s\n", ftdi_get_error_string(ftdicp)); 
        goto init_err;
    }
    
    set_bits(8, 0xb);
    set_div(100000);
        
    return 0;

init_err:
    if (ftdicp != NULL) {
        ftdi_close(ftdicp);
    }

    return -1; 
}       

int spi_close()
{       
    spi_nrefs--;
    if (ftbb_refs == 0) {
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
