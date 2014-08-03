Theese variables define transport options for BlueSuite command line tools.
They can be specified in environment or using the "-trans" option. See also
this post
<http://pfalcon-oe.blogspot.ru/2012/01/enabling-spi-logging-in-bluesuite.html>.

* SPI_DELAY_MODE - signed integer;
* SPI_STOP_CHIP - valid values are "ON", "OFF" and "AUTO";
* SPI_FAST_RESET - valid values are "SLOW", "FAST", "FASTER";
* SPI_DUMMY_MODE - valid values are "FULL", "OVERRIDE"
* SPI_DUMMY_FILE - text file name of unknown format;
* SPIDEBUG_FULL - valid values are "ON", "OFF", "TRUE", "FALSE";
* SPIDEBUG_REPRODUCIBLE - valid values are "ON", "OFF", "TRUE", "FALSE";
* SPIDEBUG_WRITES - valid values are "ON", "OFF", "TRUE", "FALSE";
* SPIDEBUG_READS - valid values are "ON", "OFF", "TRUE", "FALSE";
* SPIDEBUG_WIRE - valid values are "ON", "OFF", "TRUE", "FALSE";
* SPIDEBUG_OPTIONS - integer bitfield;
* SPIDEBUG - valid values are "ON", "OFF", "TRUE", "FALSE";
* SPIDEBUG_FILE - output file name, can be "CON:" to output to a terminal;
* SPISLOW - perform at slowest clock speed, valid values are "ON", "OFF", not applicable to csr-spi-ftdi;
* SPICLOCK - integer, default 20, not applicable to csr-spi-ftdi;
* SPIMAXCLOCK - integer, not applicable to csr-spi-ftdi;
