# CsrSpiDrivers #
## Goals ##
The goals of this project are to write a set of replacement SPI drivers for BlueLab4.1 for those without functional LPT ports and without the budget for a Csr approved USB-SPI adapter.

## Projects overview ##
### spilpt ###
This was the first step towards writing a new SPI driver: decompilation of the current LPT driver, as to be a starting point for anyone wanting to write his or her own driver.
The goal is to be as close to the original as possible, aiming for identical assembly code.
Most of the functions are already identical, although in some cases this is impossible because of either a different compiler being used, or it being more difficult than I can manage.

Mind you that the original spilpt.dll contains a few bugs (namely not properly closing all ports), and should not be used as a starting point for your own driver. See spilpt.fixed for that instead.

### spilpt.fixed ###
This is the second step, bug-fixing the spilpt.dll driver.
Basically this contains any bugs and optimizations (speed-wise or code-wise to increase readability) that do not impact the general functionality.
The main difference between spilpt and spilpt.fixed is that the former is meant to be code-identical and the latter is meant to be functionally identical.

### spilpt.arduino.bitbang ###
This is the first attempt at getting the thing actually working without an LPT port. This is a minimal change of spilpt.fixed to communicate with an arduino instead of an LPT port.

### spitlpt.arduino.offload ###
This should be the final product, a dll that offloads most of the work to the arduino to speed things up.