#include <QueueArray.h>

#include <SPI.h>
#include <TMC26XGenerator.h>
#include <Metro.h>
#include <CmdMessenger.h>

#include "constants.h"
#include "types.h"

#define DEBUG_MOTOR_CONTFIG
#define DEBUG_STATUS
#define DEBUG_MOTION
#define DEBUG_MOTION_TRACE

#define COMMAND_QUEUE_LENGTH 20
//how many otors can be theoretically geared together
#define MAX_FOLLOWING_MOTORS 6

//standards
#define TMC_26X_CONFIG 0x8440000a //SPI-Out: block/low/high_time=8/4/4 Takte; CoverLength=autom; TMC26x
#define TMC260_SENSE_RESISTOR_IN_MO 150
#define CLOCK_FREQUENCY 16000000ul
#define DEFAULT_CURRENT_IN_MA 1
#define DEFAULT_STEPS_PER_REVOLUTION 200

#define DEFAULT_ACCELERATION 1
#define DEFAULT_BOW 1

const char nr_of_motors = 2;
squirrel motors[2] = {
  {8,3,0, motor_1_target_reached, 
    TMC26XGenerator(DEFAULT_CURRENT_IN_MA,TMC260_SENSE_RESISTOR_IN_MO),
    DEFAULT_STEPS_PER_REVOLUTION, DEFAULT_ACCELERATION, DEFAULT_ACCELERATION, DEFAULT_BOW, DEFAULT_BOW },
  {12,2,1, motor_2_target_reached, 
    TMC26XGenerator(DEFAULT_CURRENT_IN_MA,TMC260_SENSE_RESISTOR_IN_MO), 
    DEFAULT_STEPS_PER_REVOLUTION, DEFAULT_ACCELERATION, DEFAULT_ACCELERATION, DEFAULT_BOW, DEFAULT_BOW }
};
const char reset_squirrel = 4;
const char start_signal_pin = 7;

boolean in_motion = false;

//config

QueueArray<movement> moveQueue = QueueArray<movement>(COMMAND_QUEUE_LENGTH);


//somebody must deal with our commands
CmdMessenger messenger = CmdMessenger(Serial1);
//watchdog Metro
Metro watchDogMetro = Metro(1000); 

void setup() {
  //at least we should try deactivate the squirrels
  pinMode(reset_squirrel,OUTPUT);
  digitalWrite(reset_squirrel, LOW);

  //initialize the serial port for commands
  Serial1.begin(115200);
  Serial.begin(115200); 
  // Adds newline to every command
  messenger.printLfCr();   

  // Attach my application's user-defined callback methods
  attachCommandCallbacks();


  //finally signal that we are ready
  watchDogStart();
}

unsigned long tmc43xx_write;
unsigned long tmc43xx_read;

void loop() {
  //move if neccessary
  checkMotion();
  // Process incoming serial data, and perform callbacks
  messenger.feedinSerialData();
  if (watchDogMetro.check()) {
    watchDogPing();
  }
}





