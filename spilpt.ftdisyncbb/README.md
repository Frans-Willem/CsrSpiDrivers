## Pinouts

Pinout specified in spi.c file. This pinout is chosen so popular FT232R
adapters could be used. Change it at will. Beware, there are cables and
adapters providing 5V output, but CSR chips require 3V3 or 1V8.

| Signal | FT232RL pin | FTDI signal | SyncBB signal | CSR signal | HC-0x pin |
| ------ | ----------- | ----------- | ------------- | ---------- | --------- |
| MOSI | 1 | TXD | D0 | SPI_MOSI | 17 |
| MISO | 5 | RXD | D1 | SPI_MISO | 18 |
| CLK | 3 | RTS | D2 | SPI_CLK | 19 |
| CS# | 2 | DTR | D4 | SPI_CS# | 16 |
| LED_WR | 6 | RI | D7 | -- | -- |
| LED_RD | 9 | DSR | D5 | -- | -- |

## Building on Debian/Ubuntu 64 bit Linux for wine

    # Install build tools
    sudo apt-get install -y build-essential gcc-multilib g++-multilib
    # Install 32 bit stuff
    # Note that wine:i386 replaces 64 bit wine
    sudo apt-get install -y wine:i386 wine-dev:i386 libc6-dev-i386 libstdc++-dev:i386 libftdi-dev:i386
    make -f Makefile.wine all

After building the dll You can reinstall 64 bit wine:

    sudo apt-get install -y wine

## TODO

* Honor *g_nSpiShiftPeriod*
