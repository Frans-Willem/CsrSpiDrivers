## CSR SPI API versions

This DLL implements CSR SPI API version 1.3 or 1.4, which is choosen at
compile time. By default it builds for API version 1.4. Example of CSR packages
that use API version 1.4:

* CSR BlueSuite 2.4, 2.5, 2.5.8

Example of CSR packages using API version 1.3:

* CSR BlueSuite 2.1, 2.3
* CSR BlueLab 4.1

You can check the API version of CSR package by inspecting original spilpt.dll
with the following command on Linux:

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

Build with command:

    make -f Makefile.wine all


### Installing

Install CSR BlueSuite in wine. Find all instances of spilpt.dll installed and
move them out of the way:

    find ~/.wine -iname spilpt.dll -execdir mv {} {}.orig \;

Install wine dll into the wine directory:

    sudo make -f Makefile.wine SPIAPI=<SPI_API_version> install

where *SPI_API_version* is one of "1.3" or "1.4".


## Building DLL on Debian/Ubuntu 64 bit for Windows using MinGW

Install MinGW cross-development environment:

    sudo apt-get install -y gcc-mingw32

Download precompiled libusb for windows from
http://sourceforge.net/projects/libusb/files/ and extract it to the libusb
directory:

    wget http://sourceforge.net/projects/libusb/files/libusb-1.0/libusb-1.0.19/libusb-1.0.19-rc1-win.7z
    7z x -olibusb libusb-1.0.19-rc1-win.7z

Download precompiled liftdi for windows from
http://sourceforge.net/projects/picusb/files/ and extract it:

    wget http://sourceforge.net/projects/picusb/files/libftdi1-1.1_devkit_x86_x64_21Feb2014.zip
    unzip libftdi1-1.1_devkit_x86_x64_21Feb2014.zip
    ln -s libftdi1-1.1_devkit_x86_x64_21Feb2014 libftdi1

Build with command:

    make -f Makefile.mingw all

### Install

1. Make a backup of spilpt.dll in your application directory (e.g. in
   *C:\Program Files\CSR\BlueSuite\*);
2. Copy appropriate version of spilpt.dll from spilpt-1.4-win32 or
   spilpt-1.3-win32 directory to your application directory;
3. Download and run Zadig from http://zadig.akeo.ie/ . In Options menu choose
   "List all devices", choose Your FTDI device ("FT232R USB UART" or similar),
   choose libusbK driver, press "Replace driver" or "Install driver".  This
   will install generic libusb-compatible driver for your FTDI chip. There is a
   nice tutorial on running libftdi programs on Windows here:
   http://embedded-funk.net/running-libftdi-under-windows/ ;
4. Run your CSR apps.

## Pinouts

Pinout specified in spi.c file. Change it at will. Beware that popular FTDI
adapters provide 5V or 3V3 I/O levels while CSR chips require 3V3 or 1V8 I/O
level. You may supply appropriate VCCIO to FTDI chip or use logic level
converter if levels don't match. See description of VCCIO pin in FTDI chip
datasheet for details.

| Signal | FT232RL pin | FTDI pin name | FTDI GPIO pin | CSR pin | HC-0x pin |
| ------ | ----------- | ------------- | ------------- | ------- | --------- |
| MOSI | 1 | TXD | D0 | SPI_MOSI | 17 |
| MISO | 5 | RXD | D1 | SPI_MISO | 18 |
| CLK | 3 | RTS# | D2 | SPI_CLK | 19 |
| CS# | 2 | DTR# | D4 | SPI_CS# | 16 |
| LED_WR | 6 | RI# | D7 | -- | -- |
| LED_RD | 9 | DSR# | D5 | -- | -- |

LED connections are optional. Wire LEDs cathodes through the current limiting
resistors (220 Ohm works fine) to the appropriate FTDI
pins. Wire LEDs anodes to FTDI VCCIO or 3V3OUT pin.


## BUGS

* Driver sometimes fails with error "Unable to start read (invalid control
  data)". The problem is clearly in SPI communication, but I still can not
  figure out the cause. Anyway, restarting operation helps.
* Driver for API 1.4 does not support more than one FTDI device connected to
  the computer at the same time. This is due to a limited stream API
  implementation.
