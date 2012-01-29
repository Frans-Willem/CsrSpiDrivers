/*
Attempt to talk to CSR Bluetooth chip SPI
 */
#include <inttypes.h>
#include <avr/interrupt.h>
#include <avr/io.h>

uint8_t pinChipSelect=_BV(2);
uint8_t pinMOSI=_BV(3);
uint8_t pinMISO=_BV(4);
uint8_t pinCLK=_BV(5);

uint8_t nOutMask=pinChipSelect|pinMOSI|pinCLK;
uint8_t nInMask=pinMISO;
uint8_t nOutput=pinChipSelect|pinMOSI|pinCLK;
uint8_t nDefault=pinChipSelect|pinCLK|pinMISO;

void setup()
{
  // start serial port at 9600 bps:
  Serial.begin(256000);
  DDRB=(DDRB&~(nInMask|nOutMask))|nOutput;
  PORTB=(PORTB&~(nInMask|nOutMask))|nDefault;
}

void loop()
{
  // if we get a valid byte, read analog ins:
  if (Serial.available() > 0) {
    uint8_t in=Serial.read();
    PORTB=(PORTB&~nOutMask)|(in&nOutMask);
    Serial.write(PINB&nInMask);
  }
}
