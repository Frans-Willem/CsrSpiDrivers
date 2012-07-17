/*
 * Attempt to talk to CSR Bluetooth chip SPI
 */
#include <inttypes.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>

#define PIN_CS _BV(2)
#define PIN_MOSI _BV(3)
#define PIN_MISO _BV(4)
#define PIN_CLK _BV(5)

int nShiftPeriod=1;//5814;
unsigned short g_nCmdReadBits=0;
unsigned short g_nCmdWriteBits=0;

void setup()
{
  //Set MISO as input, CLK, CS, MOSI as output
  DDRB=(DDRB&~PIN_MISO)|PIN_CS|PIN_MOSI|PIN_CLK;
  //Set MOSI and CLK low, CS and MISO high (MISO as pull-up)
  PORTB=(PORTB&~(PIN_MOSI|PIN_CLK))|PIN_CS|PIN_MISO;
  // start serial port at 256000 bps:
  Serial.begin(256000);
  //Send init message
  Serial.print("CSRSPI1");
}

uint8_t ReadByte() {
  while (Serial.available()<1);
  return (uint8_t)Serial.read();
}

void CSRReset() {
  //Chip select high
  PORTB|=PIN_CS;_delay_us(nShiftPeriod);
  //Two clock cycles to reset the device
  PORTB|=PIN_CLK; _delay_us(nShiftPeriod);
  PORTB&=~PIN_CLK; _delay_us(nShiftPeriod);
  PORTB|=PIN_CLK; _delay_us(nShiftPeriod);
  PORTB&=~PIN_CLK; _delay_us(nShiftPeriod);
}

void CSRStart() {
  CSRReset();
  //Chip select low
  PORTB&=~PIN_CS;
  _delay_us(nShiftPeriod);
}

void CSRStop() {
  PORTB|=PIN_CS;
}

uint8_t CSRTransfer(uint8_t nInput) {
  uint8_t ret=0;
  for (uint8_t b=0; b<8; b++) {
    if (nInput & 0x80) PORTB|=PIN_MOSI;
    else PORTB&=~PIN_MOSI;
    nInput<<=1;
    if (PINB&PIN_MISO) nInput|=1;
    _delay_us(nShiftPeriod);
    PORTB|=PIN_CLK;
    _delay_us(nShiftPeriod);
    PORTB&=~PIN_CLK;
  }
  return nInput;
}

void command_read() {
  uint8_t nCmd,nAddressH,nAddressL,nControlH,nControlL;
  nCmd=3; //Command read
  nAddressH=ReadByte();
  nAddressL=ReadByte();
  
  CSRStart();
  CSRTransfer(nCmd); //Command read
  CSRTransfer(nAddressH); //Transfer in address
  CSRTransfer(nAddressL);
  //Get control data
  nControlH=CSRTransfer(0);
  nControlL=CSRTransfer(0);
  if (nControlH != nCmd || nControlL != nAddressH) {
    //Read failed
    Serial.write((uint8_t)1);
    return;
  }
  Serial.write((uint8_t)0);
  while (true) {
    uint8_t nRead=ReadByte();
    if (nRead==0)
      break;
    while (nRead--) {
      Serial.write(CSRTransfer(0));
      Serial.write(CSRTransfer(0));
    }
  }
  CSRStop();
}

void command_write() {
  uint8_t nAddressH,nAddressL;
  //Read data from serial
  nAddressH=ReadByte();
  nAddressL=ReadByte();
  
  CSRStart();
  CSRTransfer(2); //Command write
  CSRTransfer(nAddressH);
  CSRTransfer(nAddressL);
  while (1) {
    Serial.write(31);
    uint8_t nBlockLength=ReadByte();
    if (nBlockLength==0)
      break;
    while (nBlockLength--) {
      CSRTransfer(ReadByte());
      CSRTransfer(ReadByte());
    }
  }
  CSRStop();
}

void command_xap_stopped() {
  //Check to see if chip responds to a write (see if it's even connected properly)
  uint8_t nControlH,nControlL;
  CSRStart();
  uint8_t nCheck=CSRTransfer(3);
  CSRTransfer(0xFF);
  CSRTransfer(0x9A);
  nControlH=CSRTransfer(0);
  nControlL=CSRTransfer(0);
  CSRStop();
  if (nControlH!=3 || nControlL!=0xFF) {
    //No chip present or not responding correctly, no way to find out.
    Serial.write(0xFF);
    return;
  }
  //Reading works, if the chip is running it'll pull MISO low during the first command byte, else it'll leave it high.
  Serial.write((uint8_t)(nCheck?1:0));
  return;
}

void loop()
{
  switch (ReadByte()) {
    case 1: //Read
      command_read();
      break;
    case 2: //Write
      command_write();
      break;
    case 3: //XAP Stopped
      command_xap_stopped();
      break;
  }
} 
