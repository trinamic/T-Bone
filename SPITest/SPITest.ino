#include <SPI.h>
#include <TMC26XGenerator.h>

#define DEBUG

//motor config
unsigned char steps_per_revolution = 200;

//register
#define SPIOUT_CONF_REGISTER 0x04
#define STEP_CONF_REGISTER 0x0A
#define COVER_LOW_REGISTER 0x6c
#define COVER_HIGH_REGISTER 0x6d
#define STATUS_REGISTER 0x0e

//values
#define TMC_26X_CONFIG 0x8440000a //SPI-Out: block/low/high_time=8/4/4 Takte; CoverLength=autom; TMC26x

//we have a TMC260 at the end so we configure a configurer
TMC26XGenerator tmc260 = TMC26XGenerator(500,150);


int cs_squirrel = 7;

void setup() {
  //initialize SPI
  SPI.begin();
  pinMode(cs_squirrel,OUTPUT);
  digitalWrite(cs_squirrel,HIGH);
  //initialize the serial port for debugging
  Serial.begin(9600);
  //configure the TMC26x
  tmc260.setMicrosteps(256);
  write43x(SPIOUT_CONF_REGISTER,TMC_26X_CONFIG);
  set260Register(tmc260.getDriverControlRegisterValue());
  set260Register(tmc260.getChopperConfigRegisterValue());
  set260Register(tmc260.getStallGuard2RegisterValue());
  set260Register(tmc260.getDriverConfigurationRegisterValue() | 0x80);

  //configure the motor type
  unsigned long motorconfig = 0xff; //we want closed loop operation
  motorconfig |= steps_per_revolution<<4;
  write43x(STEP_CONF_REGISTER,motorconfig);
  
}

unsigned long tmc43xx_write;
unsigned long tmc43xx_read;

void loop() {
  // put your main code here, to run repeatedly: 
  read43x(0x21,0);
  Serial.print("x actual:");
  read43x(0x21,0);
  read43x(0xA2,0x00ABCDEFul);
  read43x(0xA2,0x00123456ul);
  
  Serial.print("control:");
  Serial.println(tmc260.getDriverControlRegisterValue(),HEX);

  Serial.print("chopper config:");
  Serial.println(tmc260.getChopperConfigRegisterValue(),HEX);

  Serial.print("stallguard config:");
  Serial.println(tmc260.getStallGuard2RegisterValue(),HEX);
  
  Serial.print("driver config:");
  Serial.println(tmc260.getDriverConfigurationRegisterValue() | 0x80,HEX);
  
  Serial.println();
  delay(1000);
}
