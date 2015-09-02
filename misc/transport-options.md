Theese variables define transport options for BlueSuite command line tools.
They can be specified in environment or using the `-TRANS` option. See also
this post
<http://pfalcon-oe.blogspot.ru/2012/01/enabling-spi-logging-in-bluesuite.html>.

* `SPITRANS` - SPI transport driver, valid values: `USB`, `LPT`, `REMOTE`,
  `XAPSIM`, `SDIOCEV5`, `KALSIM`, `SDIOCE`, `SDIO`, `TRB` or any custom
  transport name (e.g. using `sometrans` name will instruct PTTransport.dll to
  use `sometrans.dll` as a driver). Only `USB` and `LPT` drivers are available
  in BlueSuite, other drivers are available in other CSR packages.  Use `USB`
  transport for csr-spi-ftdi;
* `SPIPORT` - transport port identifier, usually an integer. Default: `1` for
  `USB`, `1` for `LPT`, `remote` for `REMOTE`. Use `1` for csr-spi-ftdi;
* `SPIREMOTEIP` - IP address of the remote SPI programmer for `REMOTE`
  transport, default: `127.0.0.1`. Remote programmer TCP port number is
  hardcoded to 10122;
* `SPI_DELAY_MODE` - signed integer;
* `SPI_STOP_CHIP` - valid values are `ON`, `OFF` and `AUTO`;
* `SPI_FAST_RESET` - valid values are `SLOW`, `FAST`, `FASTER`;
* `SPI_DUMMY_MODE` - valid values are `FULL`, `OVERRIDE`
* `SPI_DUMMY_FILE` - text file name of unknown format;
* `SPIDEBUG_FULL` - valid values are `ON`, `OFF`, `TRUE`, `FALSE`;
* `SPIDEBUG_REPRODUCIBLE` - valid values are `ON`, `OFF`, `TRUE`, `FALSE`;
* `SPIDEBUG_WRITES` - valid values are `ON`, `OFF`, `TRUE`, `FALSE`;
* `SPIDEBUG_READS` - valid values are `ON`, `OFF`, `TRUE`, `FALSE`;
* `SPIDEBUG_WIRE` - valid values are `ON`, `OFF`, `TRUE`, `FALSE`;
* `SPIDEBUG_OPTIONS` - integer bitfield;
* `SPIDEBUG` - valid values are `ON`, `OFF`, `TRUE`, `FALSE`;
* `SPIDEBUG_FILE` - output file name, can be `CON:` to output to a terminal;
* `SPISLOW` - perform at slowest clock speed, valid values are `ON`, `OFF`, not
  applicable to csr-spi-ftdi;
* `SPICLOCK` - SPI clock rate, in kHz, used internally to control clock rate in
  SPI driver.
* `SPIMAXCLOCK` - maximum SPI clock rate in kHz, default is 1000.
