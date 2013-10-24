#include <SPI.h>
#define DEBUG

//motor config
unsigned char steps_per_revolution = 200;

//register
#define SPIOUT_CONF 0x04
#define STEP_CONF 0x0A

//values
#define TMC_26X_CONFIG 0xA


int cs_squirrel = 7;

void setup() {
  //initialize SPI
  SPI.begin();
  pinMode(cs_squirrel,OUTPUT);
  digitalWrite(cs_squirrel,HIGH);
  //initialize the serial port for debugging
  Serial.begin(9600);
  //configure the TMC26x
  send43x(SPIOUT_CONF,TMC_26X_CONFIG);
  //configure the motor type
  unsigned long motorconfig = 0xff; //we want closed loop operation
  motorconfig |= steps_per_revolution<<4;
  send43x(STEP_CONF,motorconfig);
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
