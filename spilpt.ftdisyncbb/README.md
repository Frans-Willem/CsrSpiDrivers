## Pinouts

Pinout specified in spi.c file. This pinout is chosen so popular FT232R
adapters could be used. Change it at will. Beware, there are cables and
adapters providing 5V output, but CSR chips require 3V3 or 1V8.

| Signal | FT232RL pin | FTDI signal | SyncBB signal | CSR signal | HC-0x pin |
| --- | --- | --- | --- | --- | --- |
| MOSI | 1 | TXD | D0 | SPI_MOSI | 17 |
| MISO | 5 | RXD | D1 | SPI_MISO | 18 |
| CLK | 3 | RTS | D2 | SPI_CLK | 19 |
| CS# | 2 | DTR | D4 | SPI_CS# | 16 |

## Building on Debian/Ubuntu Linux for wine

    sudo apt-get install -y wine-dev libftdi-dev libusb-dev
    make -f Makefile.wine all

## TODO

* Honor *g_nSpiShiftPeriod*
