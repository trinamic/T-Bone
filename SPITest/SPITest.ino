#include <SPI.h>
#include <TMC26XGenerator.h>

#define DEBUG

//motor config
unsigned char steps_per_revolution = 200;

//register
#define SPIOUT_CONF 0x04
#define STEP_CONF 0x0A

//values
#define TMC_26X_CONFIG 0xA

//we have a TMC260 at the end so we configure a configurer
TMC26XGenerator tmc260 = TMC26XGenerator(700,25);


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
  write43x(SPIOUT_CONF,TMC_26X_CONFIG);
  //configure the motor type
  unsigned long motorconfig = 0xff; //we want closed loop operation
  motorconfig |= steps_per_revolution<<4;
  write43x(STEP_CONF,motorconfig);
  
  
  //TODO untested
  /*
  send43x(0x84,0x8440000a,false); //SPI-Out: block/low/high_time=8/4/4 Takte; CoverLength=autom
  send43x(0xEC,0x00000000,false); //Cover-Register: Einstellung des DRVCTRL mit uS=256/FS
  
  send43x(0x0e,0x0,false); //Abfrage Status, um SPI-Transfer zu beenden
  send43x(0x0e,0x0,false); //Abfrage Status, um SPI-Transfer zu beenden
  send43x(0x0e,0x0,false); //Abfrage Status, um SPI-Transfer zu beenden

  send43x(0xEC,0x9C7D7,false); // Cover-Register: Einstellung des CHOPCONF mit hysterese
  
  send43x(0x0e,0x0,false); //Abfrage Status, um SPI-Transfer zu beenden
  send43x(0x0e,0x0,false); //Abfrage Status, um SPI-Transfer zu beenden
  send43x(0x0e,0x0,false); //Abfrage Status, um SPI-Transfer zu beenden

  send43x(0xEC,0x000a0000, false);  //Cover-Register: Einstellung des SMARTEN=aus
  
  
  send43x(0x0e,0x0,false); //Abfrage Status, um SPI-Transfer zu beenden
  send43x(0x0e,0x0,false); //Abfrage Status, um SPI-Transfer zu beenden
  send43x(0x0e,0x0,false); //Abfrage Status, um SPI-Transfer zu beenden
  
  send43x(0xEC,0x000c001f, false); //Cover-Register: Einstellung des SGCSCONF mit CS=31
  
  send43x(0x0e,0x0,false); //Abfrage Status, um SPI-Transfer zu beenden
  send43x(0x0e,0x0,false); //Abfrage Status, um SPI-Transfer zu beenden
  send43x(0x0e,0x0,false); //Abfrage Status, um SPI-Transfer zu beenden

  send43x(0xEC,0x000ef080, false); //Cover-Register: Einstellung des DRVCONF mit S/D aus, VSENSE=0
  */
}

unsigned long tmc43xx_write;
unsigned long tmc43xx_read;

void loop() {
  // put your main code here, to run repeatedly: 
  send43x(0x21,0,false);
  Serial.print("x actual:");
  send43x(0x21,0,false);
  send43x(0xA2,0x00ABCDEFul,false);
  send43x(0xA2,0x00123456ul,false);
  
  Serial.print("control:");
  Serial.println(tmc260.getDriverControlRegisterValue(),HEX);

  Serial.print("chopper config:");
  Serial.println(tmc260.getChopperConfigRegisterValue(),HEX);

  Serial.print("stallguard config:");
  Serial.println(tmc260.getStallGuard2RegisterValue(),HEX);
  
  Serial.print("driver config:");
  Serial.println(tmc260.getDriverConfigurationRegisterValue(),HEX);
  
  Serial.println();
  delay(1000);
}
