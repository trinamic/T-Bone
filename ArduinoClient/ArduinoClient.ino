#include <SPI.h>
#include <TMC26XGenerator.h>
#include <Metro.h>
#include <CmdMessenger.h>

#include "constants.h"

#define DEBUG

//standards
int cs_squirrel = 7;
int reset_squirrel = 2;

#define TMC_26X_CONFIG 0x8440000a //SPI-Out: block/low/high_time=8/4/4 Takte; CoverLength=autom; TMC26x
#define TMC260_SENSE_RESISTOR_IN_MO 150
#define CLOCK_FREQUENCY 16000000ul

//config
unsigned char steps_per_revolution = 200;
unsigned int current_in_ma = 1;
long vmax = 5000;
long amax = vmax/100;
long dmax = amax;

//somebody must deal with our commands
CmdMessenger messenger = CmdMessenger(Serial);
//watchdog Metro
Metro watchDogMetro = Metro(1000);

void setup() {
  //initialize the serial port for commands
  Serial.begin(115200); //TODO we will use serial 1
  // Adds newline to every command
  messenger.printLfCr();   

  // Attach my application's user-defined callback methods
  attachCommandCallbacks();

  //initialize the 43x
  initialzeTMC43x();
  //start the tmc260 driver
  intializeTMC260();


  //finally signal that we are ready
  watchDogStart();
}

unsigned long tmc43xx_write;
unsigned long tmc43xx_read;

void loop() {
  // Process incoming serial data, and perform callbacks
  messenger.feedinSerialData();
  if (watchDogMetro.check()) {
    watchDogPing();
  }
}





