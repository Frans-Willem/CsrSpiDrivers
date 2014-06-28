## CSR SPI API versions

This DLL implements CSR SPI API version 1.3 or 1.4, which is choosen at
compile time. By default it builds for API version 1.4. Example of CSR packages
that use API version 1.4:

* CSR BlueSuite 2.4, 2.5, 2.5.8

Example of CSR packages using API version 1.3:

* CSR BlueSuite 2.3
* CSR BlueLab 4.1

You can check the API version of CSR package by inspecting original spilpt.dll
with the following command:

    winedump -j export spilpt.dll | grep spifns_stream_

If the output is not empty, then the original DLL implements newer version of
API (version 1.4).

New versions of BlueSuite can be found at https://www.csrsupport.com/PCSW .
Old versions of BlueSuite can be found at
https://www.csrsupport.com/PCSWArchive . Access to these pages requires
registration.


## Building DLL on Debian/Ubuntu 64 bit Linux for wine

Install build tools

    sudo apt-get install -y build-essential gcc-multilib g++-multilib

Install 32 bit stuff.

    sudo apt-get install -y wine-dev:i386 libc6-dev-i386 libstdc++-dev:i386 libftdi-dev:i386

If You want to build for SPI API version 1.3 instead of 1.4, comment out this
line in Makefile.wine:

    CFLAGS+= -DSPIFNS_API_1_4

Build with command:

    make -f Makefile.wine all


## Installing

Install CSR BlueSuite in wine. Find all instances of spilpt.dll installed and
move them out of the way:

    find ~/.wine -iname spilpt.dll -execdir mv {} {}.orig \;

Install wine dll:

    sudo make -f Makefile.wine install


## Pinouts

Pinout specified in spi.c file. Change it at will. Beware that FTDI adapters
provide 5V or 3V3 I/O levels while CSR chips require 3V3 or 1V8 I/O level. Use
logic level converter if levels doesn't match.

| Signal | FT232RL pin | FTDI pin name | SyncBB signal | CSR pin name | HC-0x pin |
| ------ | ----------- | ----------- | ------------- | ---------- | --------- |
| MOSI | 1 | TXD | D0 | SPI_MOSI | 17 |
| MISO | 5 | RXD | D1 | SPI_MISO | 18 |
| CLK | 3 | RTS | D2 | SPI_CLK | 19 |
| CS# | 2 | DTR | D4 | SPI_CS# | 16 |
| LED_WR | 6 | RI | D7 | -- | -- |
| LED_RD | 9 | DSR | D5 | -- | -- |

LED connections are optional. Wire LEDs cathodes through the current limiting
resistors (220 Ohm works fine) to the appropriate FTDI pins. Wire LEDs anodes
to FTDI VIO pin.


## TODO

* Honor *g_nSpiShiftPeriod*
