## from https://github.com/raplin/CsrUsbSpiDeviceRE/blob/master/csrspi.c#L107
 * write
  * set CLK low
  * delay
  * set MOSI as needed
  * set CLK high
  * delay
 * read
  * set CLK low
  * delay
  * set CLK high
  * delay
  * read MISO
## from https://github.com/Frans-Willem/CsrSpiDrivers/blob/master/spilpt.fixed/basics.cpp
 * transfer
  * read MISO, set MOSI as needed, set CLK low
  * delay
  * set CLK high
  * delay
## https://github.com/Frans-Willem/CsrSpiDrivers/blob/master/spilpt.arduino/CsrSpiDriver/CsrSpiDriver.ino#L56
 * transfer
  * set MOSI as needed, read MISO
  * delay
  * set CLK high
  * delay
  * set CLK low
## https://gitlab.com/frol/csr-spilpt-mpsse/blob/master/a.c#L189
