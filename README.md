**Table of Contents**

- [CSR BlueCore USB SPI programmer](#csr-bluecore-usb-spi-programmer)
    - [CSR chips supported by programmer](#csr-chips-supported-by-programmer)
        - [Notes](#notes)
    - [Programmer hardware](#programmer-hardware)
        - [Using FT232RL breakout board as a programmer](#using-ft232rl-breakout-board-as-a-programmer)
        - [Dedicated programmer](#dedicated-programmer)
    - [Software](#software)
        - [CSR SPI API versions](#csr-spi-api-versions)
        - [Installing prebuilt drivers](#installing-prebuilt-drivers)
            - [Installing on Ubuntu/Debian Linux](#installing-on-ubuntudebian-linux)
            - [Installing on Windows](#installing-on-windows)
        - [Running](#running)
            - [SPI clock](#spi-clock)
            - [Options](#options)
        - [Building for Wine](#building-for-wine)
            - [Building Wine DLL on 32-bit Debian/Ubuntu Linux](#building-wine-dll-on-32-bit-debianubuntu-linux)
            - [Building Wine DLL on 64-bit Debian/Ubuntu Linux](#building-wine-dll-on-64-bit-debianubuntu-linux)
            - [Installing](#installing)
        - [Building DLL for Windows](#building-dll-for-windows)
            - [Cross-compiling DLL for Windows on Debian/Ubuntu using MinGW](#cross-compiling-dll-for-windows-on-debianubuntu-using-mingw)
        - [BUGS](#bugs)
    - [Thanks](#thanks)
    - [Related projects](#related-projects)
    - [Other sources of information](#other-sources-of-information)

# CSR BlueCore USB SPI programmer

This is USB SPI programmer for CSR BlueCore chips, based on FTDI FT232R USB to
UART converter chip. Software is written for use with CSR tools (such as
BlueLab or BlueSuite) under Linux with Wine or under Windows. It works by
replacing SPI LPT programmer driver, spilpt.dll, in CSR applications.

Programmer hardware can be made using simple FTDI breakout board. Alternately
You can build dedicated programmer using the included schematic.

Project home page: <https://github.com/lorf/csr-spi-ftdi>.

## CSR chips supported by programmer

Generally, all CSR BlueCore chips starting with BlueCore 2 should be supported.
Programmer was tested with the following chips:

* BC417 (on HC-05 module)
* BC57F687A
* CSR8645

### Notes

* BlueCore chips require 3.3V or 1.8V I/O voltage level. Check the datasheet.
* Some chips (like CSR8645) share SPI pins with PCM function. For such chips to
  be accessible via SPI, `SPI_PCM#` pin should be pulled up to I/O voltage
  supply through a 10K resistor.
* Some bluetooth modules based on BlueCore chips with builtin battery chargers
  may be shipped with battery configuration enabled. Such modules will shutdown
  shortly after power on if you don't connect charged battery. This can be
  disabled using appropriate Configuration Tool.

## Programmer hardware

Programmer hardware is based on FT232R chip. It is also possible to use later
generation FTDI chips, such as FT2232C/D/H or FT232H, with minor code
modifications.

### Using FT232RL breakout board as a programmer

You can build a simple programmer using popular FT232RL breakout boards (search
Ebay for "FT232RL module 3.3" for example). Pinout specified in spi.c file.
Change it at will. Note that FTDI boards usually provide 5V or 3.3V I/O levels
while CSR chips require 3.3V or 1.8V I/O level. You may supply appropriate
VCCIO to FTDI chip or use logic level converter if levels don't match. See
description of VCCIO pin in FTDI chip datasheet for details.

| Signal | FT232RL pin | FTDI pin name | FTDI GPIO bit | CSR pin  |
|--------|-------------|---------------|---------------|----------|
| CS#    | 2           | DTR#          | D4            | SPI_CS#  |
| CLK    | 3           | RTS#          | D2            | SPI_CLK  |
| MOSI   | 6           | RI#           | D7            | SPI_MOSI |
| MISO   | 9           | DSR#          | D5            | SPI_MISO |
| TX     | 1           | TXD           | Not used      | UART_RX  |
| RX     | 5           | RXD           | Not used      | UART_TX  |
| LED_RD | 10          | DCD#          | D6            | --       |
| LED_WR | 11          | CTS#          | D3            | --       |

SPI and UART BlueCore pins could be connected directly to FTDI pins, but I'd
recommend to wire them through the 220 Ohm (or so) resistors.

TX and RX connections are optional and provide connectivity to BlueCore UART.

LED connections are optional. Wire LEDs cathodes through the current limiting
resistors (330 Ohm works fine) to the appropriate FTDI
pins. Wire LEDs anodes to FTDI 3V3 pin.

### Dedicated programmer

KiCad schematic for a dedicated programmer can be found in `hardware`
subdirectory.

## Software

### CSR SPI API versions

This driver implements CSR SPI API version 1.3 (used in CSR BlueSuite 2.1, 2.3,
CSR BlueLab 4.1) and 1.4 (CSR BlueSuite 2.4, 2.5, 2.5.8). DLL is built for each
API version during a compile time.

You can check the API version of CSR package by inspecting original spilpt.dll
with the following command on Linux:

    winedump -j export spilpt.dll | grep spifns_stream_

If the output is not empty, then the original DLL implements newer version of
API (version 1.4).

New versions of BlueSuite can be found at <https://www.csrsupport.com/PCSW>.
Old versions of BlueSuite can be found at
<https://www.csrsupport.com/PCSWArchive>. Access to these pages requires
registration.


### Installing prebuilt drivers

Prebuilt drivers for Linux and Windows can be downloaded from
<https://github.com/lorf/csr-spi-ftdi/releases>.

#### Installing on Ubuntu/Debian Linux

Install Wine:

    sudo apt-get install wine

Install CSR BlueSuite in Wine. Find all instances of spilpt.dll installed and
move them out of the way:

    find ~/.wine -iname spilpt.dll -execdir mv {} {}.orig \;

Copy approproate version of the .dll.so file to Wine system directory:

    sudo cp -p spilpt-wine-api<SPI_API_version>/spilpt.dll.so /usr/lib/i386-linux-gnu/wine/

where `<SPI_API_version>` is one of `1.3` or `1.4`. Alternately You can specify
location of the .dll.so file in WINEDLLPATH environment variable, see wine(1)
man page for details.

Run CSR apps.


#### Installing on Windows

1. Install CSR package such as BlueSuite;
2. Make a backup of spilpt.dll in your application directory (e.g. in
   `C:\Program Files (x86)\CSR\BlueSuite 2.5.8\`);
3. Copy appropriate version of spilpt.dll from spilpt-win32-api1.4 or
   spilpt-win32-api1.3 directory (see "CSR SPI API versions") to your
   application directory;
4. Connect Your FTDI device to computer;
5. Download and run Zadig from <http://zadig.akeo.ie/>. In Options menu choose
   "List all devices", choose Your FTDI device ("FT232R USB UART" or similar),
   choose libusbK driver, press "Replace driver" or "Install driver".  This
   will install generic libusb-compatible driver for your FTDI chip. There is a
   tutorial on running libftdi programs on Windows here:
   <http://embedded-funk.net/running-libftdi-under-windows/>;
6. Run your CSR apps.

### Running

Csr-spi-ftdi driver supports several options, that can be set as environment
variables or as a -TRANS option to most CSR commandline apps.

#### SPI clock

SPI clock run at 1/2 (when reading) or 1/3 (when writing) of FTDI clock rate.
CSR app may automatically slow SPI clock down when read or write verification
fails. Some commands are executed at the 1/50 of the clock rate. FTDI clock
rate can be contolled with FTDI_BASE_CLOCK option.

#### Options

* FTDI_BASE_CLOCK - Base clock frequency in Hz, default is 4 MHz. Changing this
  value proportionally changes all SPI clock rates.
* FTDI_LOG_LEVEL - sets csr-spi-ftdi log level, available log levels: "quiet",
  "err", "warn", "info", "debug". Adding a ",dump" option provides hex dumps of
  transferred data. Example: "FTDI_LOG_LEVEL=info,dump". Default: "warn".
* FTDI_LOG_FILE - specify log file name. Can be set to "stdout" to log to
  standard output, or to "stderr" to log to standard error stream. Default:
  "stderr".

For other options see `misc/transport-options.md`.

### Building for Wine

#### Building Wine DLL on 32-bit Debian/Ubuntu Linux

Install build tools:

    sudo apt-get install -y build-essential

Install development libraries:

    sudo apt-get install -y wine-dev libc6-dev libstdc++-dev libftdi-dev

Build with command:

    make -f Makefile.wine all


#### Building Wine DLL on 64-bit Debian/Ubuntu Linux

Install build tools:

    sudo apt-get install -y build-essential gcc-multilib g++-multilib

Install 32 bit stuff:

    sudo apt-get install -y wine-dev:i386 libc6-dev-i386 libstdc++-dev:i386 libftdi-dev:i386

Build with command:

    make -f Makefile.wine all

#### Installing

Install CSR BlueSuite in Wine. Find all instances of spilpt.dll installed and
move them out of the way:

    find ~/.wine -iname spilpt.dll -execdir mv {} {}.orig \;

Install Wine dll into the Wine libraries directory:

    sudo make -f Makefile.wine SPIAPI=<SPI_API_version> install

where `<SPI_API_version>` is one of `1.3` or `1.4` (see "CSR SPI API
versions"). Alternately You can specify location of the .dll.so file in
WINEDLLPATH environment variable, see wine(1) man page for details.


### Building DLL for Windows

#### Cross-compiling DLL for Windows on Debian/Ubuntu using MinGW

Install MinGW cross-development environment:

    sudo apt-get install -y mingw-w64

Download precompiled libusb for windows from
<http://sourceforge.net/projects/libusb/files/> and extract it to the libusb
directory:

    wget http://sourceforge.net/projects/libusb/files/libusb-1.0/libusb-1.0.19/libusb-1.0.19.7z
    7z x -olibusb libusb-1.0.19.7z

Download precompiled liftdi for windows from
<http://sourceforge.net/projects/picusb/files/> and extract it:

    wget http://sourceforge.net/projects/picusb/files/libftdi1-1.1_devkit_x86_x64_21Feb2014.zip
    unzip libftdi1-1.1_devkit_x86_x64_21Feb2014.zip
    ln -s libftdi1-1.1_devkit_x86_x64_21Feb2014 libftdi1

Build with command:

    make -f Makefile.mingw all


### BUGS

* Driver sometimes fails with error "Unable to start read (invalid control
  data)". The problem is clearly in SPI communication, but I still can not
  figure out the cause. Anyway, restarting operation helps.
* Current implementation of 1.4 API is based on a wild guess and is just a
  wrapper around 1.3 functions. It doesn't support multiple programmers
  connected at the same time and may contain other bugs.


## Thanks
* This project is a derivative of Frans-Willem Hardijzer's reverse-engineered
  spilpt.dll drivers <https://github.com/Frans-Willem/CsrSpiDrivers>;
* Thanks to **unicorn** from <http://www.nebo-forum.kiev.ua/> for the idea of a DLL
  for Wine.


## Related projects
* LPT programmer and general info by Robin Gross
  <http://byron76.blogspot.com/>;
* Reverse-engineered SPILPT driver + Arduino SPILPT driver by Frans-Willem
  Hardijzer, for Windows <https://github.com/Frans-Willem/CsrSpiDrivers>;
* SPILPT driver for Wine under Linux by **unicorn** using FTDI MPSSE
  <http://www.nebo-forum.kiev.ua/viewtopic.php?p=58291#p58291>;
* Software to read/write BC4 flash over SPI using Raspberry PI GPIO
  <http://members.efn.org/~rick/work/rpi.csr.html>;
* USBSPI programmer based on CSR BC3 chip using original firmware by Jernej
  Škrabec <http://jernej87.blogspot.com/>;
* USBSPI programmer software for Linux by Jernej Škrabec
  <https://gitorious.org/csrprogrammer>:
   * USBSPI protocol analysis
     <http://jernej87.blogspot.com/2012/10/csrs-usb-programmer-protocol-analysis.html>;
   * Using USBSPI on Linux
     <http://jernej87.blogspot.com/2012/10/dumping-bluecore4-firmware-on-linux.html>;
* USBSPI programmer based on Stellaris Launchpad by Frans-Willem Hardijzer, for
  Windows <https://github.com/Frans-Willem/CsrUsbSpiDeviceRE>;

## Other sources of information
* BlueSuite 2.5.0 "source code"
  <https://www.csrsupport.com/document.php?did=38692> - it doesn't contain
  source code for SPI drivers but at least development header files in
  CSRSource/result/include/ are of some help.
