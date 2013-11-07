#include <QueueArray.h>

#include <SPI.h>
#include <TMC26XGenerator.h>
#include <Metro.h>
#include <CmdMessenger.h>

#include "constants.h"
#include "types.h"
#
#define DEBUG

#define COMMAND_QUEUE_LENGTH 2
//standards
const int reset_squirrel = 4;

#define TMC_26X_CONFIG 0x8440000a //SPI-Out: block/low/high_time=8/4/4 Takte; CoverLength=autom; TMC26x
#define TMC260_SENSE_RESISTOR_IN_MO 150
#define CLOCK_FREQUENCY 16000000ul
#define DEFAULT_CURRENT_IN_MA 1
#define DEFAULT_STEPS_PER_REVOLUTION 200
//#define DEFAULT_V_MAX 1
#define DEFAULT_ACCELERATION 1
#define DEFAULT_BOW 1

const char nr_of_motors = 2;
squirrel motors[2] = {
  {8,3,0, TMC26XGenerator(DEFAULT_CURRENT_IN_MA,TMC260_SENSE_RESISTOR_IN_MO),DEFAULT_STEPS_PER_REVOLUTION, DEFAULT_ACCELERATION, DEFAULT_ACCELERATION, DEFAULT_BOW, DEFAULT_BOW },
  {12,2,1, TMC26XGenerator(DEFAULT_CURRENT_IN_MA,TMC260_SENSE_RESISTOR_IN_MO), DEFAULT_STEPS_PER_REVOLUTION, DEFAULT_ACCELERATION, DEFAULT_ACCELERATION, DEFAULT_BOW, DEFAULT_BOW}
};

//config

QueueArray<movement> moveQueue = QueueArray<movement>(COMMAND_QUEUE_LENGTH);


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





