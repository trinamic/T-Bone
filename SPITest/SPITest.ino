#include <SPI.h>
#define DEBUG

int cs_squirrel = 7;

void setup() {
  //initialize SPI
  SPI.begin();
  pinMode(cs_squirrel,OUTPUT);
  digitalWrite(cs_squirrel,HIGH);
  //initialize the serial port for debugging
  Serial.begin(9600);
}

unsigned long tmc43xx_write;
unsigned long tmc43xx_read;

void loop() {
  // put your main code here, to run repeatedly: 
  send43x(0x21,0);
  Serial.print("x actual:");
  send43x(0x21,0);
  Serial.println();
  send43x(0xA2,0x00ABCDEFul);
  send43x(0xA2,0x00123456ul);
  delay(1000);
}
