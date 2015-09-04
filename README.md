**Table of Contents**

* [CSR BlueCore USB SPI programmer](#csr-bluecore-usb-spi-programmer)
  * [CSR chips supported by programmer](#csr-chips-supported-by-programmer)
    * [Chip notes](#chip-notes)
  * [Programmer hardware](#programmer-hardware)
    * [Using FT232RL breakout board as a programmer](#using-ft232rl-breakout-board-as-a-programmer)
    * [Dedicated programmer](#dedicated-programmer)
    * [Counterfeit FT232RL chips](#counterfeit-ft232rl-chips)
  * [Software](#software)
    * [CSR software](#csr-software)
    * [Installing prebuilt drivers](#installing-prebuilt-drivers)
      * [Installing on Ubuntu/Debian Linux](#installing-on-ubuntudebian-linux)
      * [Installing on Windows](#installing-on-windows)
    * [Using the driver](#using-the-driver)
      * [Choosing USB transport](#choosing-usb-transport)
      * [Options](#options)
      * [Communication speed](#communication-speed)
      * [Useful commands](#useful-commands)
      * [Troubleshooting](#troubleshooting)
    * [Building for Wine](#building-for-wine)
      * [Building Wine DLL on 32-bit Debian/Ubuntu Linux](#building-wine-dll-on-32-bit-debianubuntu-linux)
      * [Building Wine DLL on 64-bit Debian/Ubuntu Linux](#building-wine-dll-on-64-bit-debianubuntu-linux)
      * [Installing](#installing)
    * [Building DLL for Windows](#building-dll-for-windows)
      * [Cross-compiling DLL for Windows on Debian/Ubuntu using MinGW](#cross-compiling-dll-for-windows-on-debianubuntu-using-mingw)
    * [Bugs](#bugs)
  * [Thanks](#thanks)
  * [Related projects](#related-projects)
  * [Other sources of information](#other-sources-of-information)

# CSR BlueCore USB SPI programmer

This is USB SPI programmer for CSR BlueCore chips, based on FTDI FT232R USB to
UART converter chip. Programmer hardware can be made using simple FT232RL
breakout board, or built as a dedicated programmer using included schematic.
Programmer driver works by replacing USB SPI programmer driver, usbspi.dll, in
CSR applications and is currently ported to Linux/Wine and Windows.

Project home page: <https://github.com/lorf/csr-spi-ftdi>.

## CSR chips supported by programmer

Generally, all CSR BlueCore chips starting with BlueCore 2 should be supported.
Programmer was tested with the following chips:

* BC417143 (on HC-05 module)
* BC57F687A
* CSR8645
* BC212015 (reported by Alex Nuc, see [Chip notes](#chip-notes))

### Chip notes

* BlueCore chips require either 3.3V or 1.8V I/O voltage level. Check the
  datasheet.
* Some chips (like CSR8645) share SPI pins with PCM function. For such chips to
  be accessible via SPI, `SPI_PCM#` pin should be pulled up to I/O voltage
  supply through a 10K resistor.
* On some chips `SPI_DEBUG_EN` (on BC6140) or `SPI_PIO#` (on CSR1010) pin
  should be pulled up directly to I/O voltage supply to enable SPI port.
* Some bluetooth modules based on BlueCore chips with builtin battery chargers
  may be shipped with battery configuration enabled. Such modules will shutdown
  shortly after power on if You don't connect charged battery. Battery charger
  configuration is defined in `PSKEY_USR0` and can be changed using appropriate
  Configuration Tool or PSTool. See sample PSR files for disabling charger in
  [misc/](misc/).
* BlueCore 2 chips (such as BC212015) are not supported in BlueSuite 2.4 and
  above. It's also reported that to flash/dump these chips it's required to
  lower SPI speed. So for BC2 chips it's recommended to use BlueSuite 2.3 and
  set `SPIMAXCLOCK=200` [option](#options).

## Programmer hardware

Programmer hardware is based on FT232R chip. It is also possible that later
generation FTDI chips, such as FT2232C/D/H or FT232H, will also work, but this
was not tested.

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

LED connections are optional. Wire LED cathodes through the current limiting
resistors (330 Ohm works fine) to the appropriate FTDI
pins. Wire LED anodes to FTDI 3V3 pin.

Don't power BlueCore chip from FT232R internal 3.3V regulator! It's current
draw may exceed FT232R 50mA limit, which may cause communication errors.

Also see notes on [Counterfeit FT232RL chips](#counterfeit-ft232rl-chips).

### Dedicated programmer

KiCad schematic for a dedicated programmer can be found in
[hardware/](hardware/) subdirectory.

### Counterfeit FT232RL chips

When You buy cheap (< $5) FT232RL IC or module from Ebay or Aliexpress, You
usually get counterfeit FT232RL chip
([1](http://zeptobars.ru/en/read/FTDI-FT232RL-real-vs-fake-supereal),
[2](http://www.eevblog.com/forum/reviews/ftdi-driver-kills-fake-ftdi-ft232/msg536891/#msg536891),
[3](http://dangerousprototypes.com/forum/viewtopic.php?p=56343#p56343)).

I've got such a chip (this one has read-only EEPROM, S/N A50285BI, probably
it's a [generation 2 counterfeit](https://blog.cesanta.com/FTDI-adventures)),
and verified csr-spi-ftdi to work with it.

There is a known data loss issue with counterfeit FT232RL chip plugged in USB
3.0 socket. If You encounter it (You'll get a warning on stderr), try to replug
the programmer to USB 2.0 socket. It also may produce some communication
errors, but they are automatically retried, so in general it works.

Some versions of Windows FTDI driver tend to intentionally
[brick](https://hackaday.com/2014/10/22/watch-that-windows-update-ftdi-drivers-are-killing-fake-chips/)
generation 1 counterfeit chips by setting their PID to 0x0000. Although there
is a
[unbrick](https://www.reddit.com/r/arduino/comments/2k0i7x/watch_that_windows_update_ftdi_drivers_are/clgviyl)
([2](http://www.youtube.com/watch?v=RZH_qGautqM)) and
[protect](http://www.eevblog.com/forum/reviews/ftdi-driver-kills-fake-ftdi-ft232/msg537012/#msg537012)
procedures, csr-spi-ftdi will work on the bricked chips too.


## Software

### CSR software

This driver is tested with CSR BlueSuite 2.1 - 2.6.0 and with CSR BlueLab 4.1,
but should work with other CSR software too. Newer versions of BlueSuite can be
found at `https://www.csrsupport.com/PCSW`. Older versions of BlueSuite can be
found at `https://www.csrsupport.com/PCSWArchive`. Access to these pages
requires registration.

### Installing prebuilt drivers

Prebuilt drivers for Linux and Windows can be downloaded from
<https://github.com/lorf/csr-spi-ftdi/releases>.

#### Installing on Ubuntu/Debian Linux

Install Wine:

    sudo apt-get install wine

Install CSR BlueSuite in Wine. Find all instances of usbspi.dll installed and
move them out of the way:

    find ~/.wine -iname usbspi.dll -exec mv {} {}.orig \;

Copy usbspi.dll.so to Wine system directory:

    sudo cp -p lib-wine-linux/usbspi.dll.so /usr/lib/i386-linux-gnu/wine/

Alternately You can specify location of the .dll.so file in WINEDLLPATH
environment variable, see wine(1) man page for details.

Allow yourself access to FTDI device

    cat <<_EOT_ | sudo tee -a /etc/udev/rules.d/99-ftdi.rules
    # FT232R
    SUBSYSTEM=="usb", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6001", MODE="0660", GROUP="plugdev"
    _EOT_

After that You'll need to add yourself to `plugdev` group and relogin.


#### Installing on Windows

1. Install CSR package such as BlueSuite;
2. Make a backup of usbspi.dll in your application directory (e.g. in
   `C:\Program Files (x86)\CSR\BlueSuite 2.6.0\`);
3. Copy lib-win32/usbspi.dll to your application directory;
4. Connect Your FTDI device to computer;
5. Download and run Zadig from <http://zadig.akeo.ie/>. In Options menu choose
   "List all devices", choose Your FTDI device ("FT232R USB UART" or similar),
   choose libusbK driver, press "Replace driver" or "Install driver".  This
   will install generic libusb-compatible driver for your FTDI chip. There is a
   tutorial on running libftdi programs on Windows here:
   <http://embedded-funk.net/running-libftdi-under-windows/>;
6. Run your CSR apps.

### Using the driver

#### Choosing USB transport

Older BlueSuite defaults to using CSR SPI LPT programmer, to use csr-spi-ftdi
as programmer You need to select USB SPI transport. Use `-TRANS "SPITRANS=USB
SPIPORT=1"` option for command line tools. Import
`misc/spi-set-usb-transport.reg` to change the default.

#### Options

Csr-spi-ftdi driver supports several options, that can be set as environment
variables or using the -TRANS option to most CSR commandline apps.

* `SPIMAXCLOCK` - Maximum SPI clock frequency in kHz, default is 1000.
  Practical values range from 20 to 2000. CSR app may automatically slow SPI
  clock down when read or write verification fails. Some commands are always
  executed at 20 kHz.
* `FTDI_LOG_LEVEL` - sets csr-spi-ftdi log level, available log levels:
  `quiet`, `err`, `warn`, `info`, `debug`. Adding a `,dump` option provides hex
  dumps of transferred data. Example: `FTDI_LOG_LEVEL=info,dump`. Default:
  `warn`.
* `FTDI_LOG_FILE` - specify log file name. Can be set to `stdout` to log to
  standard output, or to `stderr` to log to standard error stream. Default:
  `stderr`.

For other options see [misc/transport-options.md](misc/transport-options.md).

#### Communication speed

Reading or writing a 1 MB flash on HC-05 module takes about 60 seconds. Dumping
HC-05 PS keys takes about 40 seconds.

Running csr-spi-ftdi in a virtual machine slows things down presumably due to
latency added by USB virtualization. E.g. running csr-spi-ftdi under VirtualBox
slows transactions down about 4x times.

#### Useful commands

These commands should be executed from directory where BlueSuite is installed
or this directory should be in your PATH.

* Display chip ID, firmware version and flash size:

        blueflashcmd -trans "SPITRANS=USB SPIPORT=1" -identify

* Save firmware backup (only for chips with flash, backup will include PS
  keys):

        blueflashcmd -trans "SPITRANS=USB SPIPORT=1" -dump csr-fw-backup

  This creates two files, `csr-fw-backup.xpv` and `csr-fw-backup.xdv`.

* Flash firmware from files `csr-fw-backup.xpv` and `csr-fw-backup.xdv`:

        blueflashcmd -trans "SPITRANS=USB SPIPORT=1" csr-fw-backup

* Collect debug logs:

        blueflashcmd -trans "SPITRANS=USB SPIPORT=1 SPIDEBUG=ON \
            SPIDEBUG_FILE=C:\csr-debug.log FTDI_LOG_LEVEL=debug,dump \
            FTDI_LOG_FILE=C:\csr-spi-ftdi-debug.log" -identify

* Lower SPI speed 5 times:

        blueflashcmd -trans "SPITRANS=USB SPIPORT=1 SPIMAXCLOCK=200" \
            -dump csr-fw-backup

* Save chip settings (PS Keys) backup into `csr-pskeys.psr`:

        pscli -trans "SPITRANS=USB SPIPORT=1" dump csr-pskeys.psr

* Merge some settings from `pskeys.psr` to the chip:

        pscli -trans "SPITRANS=USB SPIPORT=1" merge pskeys.psr

#### Troubleshooting

* Decreasing SPI speed using `SPIMAXCLOCK` [option](#options) may help in case
  of communication failures.
* `Unable to start read (invalid control data)` errors are usually harmless,
  since read attempts are retried. If You've got a pile of theese errors and
  programmer doesn't work - check connections, voltage levels, try to lower SPI
  connection resistor values. Decreasing SPI speed using `SPIMAXCLOCK`
  [option](#options) may also help.
* `WARNING: Attempt %d to read sector %d` warnings are also harmless if they are
  not result in error.
* `Couldn't find LPT port` error means You are using spilpt.dll driver instead
  of usbspi.dll. Try importing
  [misc/spi-set-usb-transport.reg](misc/spi-set-usb-transport.reg) or adding
  `-trans "SPITRANS=USB SPIPORT=1"` option on command line.
* `Lost %d of %d bytes of data in transit` or `ERROR: Download Failed, Sector
  %d` - probably an indication of counterfeit FT232RL plugged into USB3.0
  socket. Try to replug into USB 2.0 socket. See [Counterfeit FT232RL
  chips](#counterfeit-ft232rl-chips).

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

Install CSR BlueSuite in Wine. Find all instances of usbspi.dll installed and
move them out of the way:

    find ~/.wine -iname usbspi.dll -exec mv {} {}.orig \;

Install Wine dll into the Wine libraries directory:

    sudo make -f Makefile.wine install

Alternately You can specify location of the .dll.so file in WINEDLLPATH
environment variable, see wine(1) man page for details.


### Building DLL for Windows

#### Cross-compiling DLL for Windows on Debian/Ubuntu using MinGW

Install MinGW cross-development environment:

    sudo apt-get install -y mingw-w64 cmake

Download [precompiled libusb for
windows](http://sourceforge.net/projects/libusb/files/) and extract it to the
libusb directory:

    wget http://sourceforge.net/projects/libusb/files/libusb-1.0/libusb-1.0.19/libusb-1.0.19.7z
    7z x -olibusb libusb-1.0.19.7z

Build [libftdi](http://www.intra2net.com/en/developer/libftdi/) from source:

    wget http://www.intra2net.com/en/developer/libftdi/download/libftdi1-1.2.tar.bz2
    tar xjvf libftdi1-1.2.tar.bz2
    cd libftdi1-1.2
    cmake -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-i686-w64-mingw32.cmake \
        -DLIBUSB_INCLUDE_DIR=../libusb/include/libusb-1.0 \
        -DLIBUSB_LIBRARIES="-L../../libusb/MinGW32/static -lusb-1.0" \
        -DCMAKE_INSTALL_PREFIX=../libftdi1 .
    make all install
    cd ..

Build with command:

    make -f Makefile.mingw all


### Bugs

* See [Issues on github](https://github.com/lorf/csr-spi-ftdi/issues) to list
  current bug reports or to report a bug.
* Current implementation of 1.4 SPI API (used in BlueSuite starting from 2.4)
  is based on a wild guess and is just a wrapper around 1.3 functions. It
  doesn't support multiple programmers connected at the same time and may
  contain other bugs.


## Thanks
* This project is a derivative of Frans-Willem Hardijzer's [reverse-engineered
  usbspi.dll drivers](https://github.com/Frans-Willem/CsrSpiDrivers);
* Thanks to **unicorn** from <http://www.nebo-forum.kiev.ua/> for the idea of a
  DLL for Wine.


## Related projects
* [LPT programmer and general info](http://byron76.blogspot.com/) by Robin
  Gross;
* [Reverse-engineered SPILPT driver + Arduino SPILPT
  driver](https://github.com/Frans-Willem/CsrSpiDrivers) by Frans-Willem
  Hardijzer, for Windows;
* [SPILPT driver for
  Wine](http://www.nebo-forum.kiev.ua/viewtopic.php?p=58291#p58291) under Linux
  by **unicorn** using FTDI MPSSE;
* [Software](http://members.efn.org/~rick/work/rpi.csr.html) to read/write BC4
  flash over SPI using Raspberry PI GPIO;
* [USBSPI programmer](http://jernej87.blogspot.com/) based on CSR BC3 chip
  using original firmware by Jernej Škrabec;
* [USBSPI programmer software for Linux](https://gitorious.org/csrprogrammer)
  by Jernej Škrabec;
   * [USBSPI protocol
     analysis](http://jernej87.blogspot.com/2012/10/csrs-usb-programmer-protocol-analysis.html);
   * [Using USBSPI on
     Linux](http://jernej87.blogspot.com/2012/10/dumping-bluecore4-firmware-on-linux.html);
* [USBSPI programmer based on Stellaris
  Launchpad](https://github.com/Frans-Willem/CsrUsbSpiDeviceRE) by Frans-Willem
  Hardijzer, for Windows;
* [pypickit](https://code.google.com/p/pypickit/) contains code to flash CSR
  BC2 and BC3 chips using PicKit2.

## Other sources of information
* ~~BlueSuite 2.5.0 "source code"
  `https://www.csrsupport.com/document.php?did=38692` - it doesn't contain
  source code for SPI drivers but at least development header files in
  CSRSource/result/include/ are of some help.~~ It seems CSR removed it from
  download.
