## Pinouts

Pinout specified in spi.c file. This pinout is chosen so popular FT232R
adapters could be used. Change it at will. Beware, there are cables and
adapters providing 5V output, but CSR chips require 3V3 or 1V8.

| Signal | FT232RL pin | FTDI pin name | SyncBB signal | CSR pin name | HC-0x pin |
| ------ | ----------- | ----------- | ------------- | ---------- | --------- |
| MOSI | 1 | TXD | D0 | SPI_MOSI | 17 |
| MISO | 5 | RXD | D1 | SPI_MISO | 18 |
| CLK | 3 | RTS | D2 | SPI_CLK | 19 |
| CS# | 2 | DTR | D4 | SPI_CS# | 16 |
| LED_WR | 6 | RI | D7 | -- | -- |
| LED_RD | 9 | DSR | D5 | -- | -- |

## Building DLL on Debian/Ubuntu 64 bit Linux for wine

    # Install build tools
    sudo apt-get install -y build-essential gcc-multilib g++-multilib
    # Install 32 bit stuff
    # Note that wine:i386 replaces 64 bit wine
    sudo apt-get install -y wine:i386 wine-dev:i386 libc6-dev-i386 libstdc++-dev:i386 libftdi-dev:i386
    make -f Makefile.wine all

After building the DLL You can reinstall 64 bit wine:

    sudo apt-get install -y wine

## Installing

Install CSR BlueSuite 2.3 or older, or CSR BlueLab 4.1 or older in wine. Find all
instances of spilpt.dll installed and move them out of the way:

    find ~/.wine/drive_c -iname spilpt.dll -execdir mv {} {}.orig \;

Install wine dll:

    sudo make -f Makefile.wine install

## CSR SPI API versions

This DLL implements CSR SPI API version 1.3, example of CSR packages that use
that API version:

* CSR BlueSuite 2.3 or older
* CSR BlueLab 4.1

Newer packages, e.g. CSR BlueSuite 2.4 use the API version 1.4 and will not
work with this DLL.

You can check the API version of CSR package by inspecting original spilpt.dll
with the following command:

    winedump -j export spilpt.dll | grep spifns_stream_

If the output is not empty, then the original DLL implements newer version of
API (version 1.4) and the package using it will not work with this DLL.

Old versions of BlueSuite can be found at
https://www.csrsupport.com/PCSWArchive after registration.

## TODO

* Honor *g_nSpiShiftPeriod*
