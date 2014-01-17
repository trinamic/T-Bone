#include <digitalIOPerformance.h>
#include <SPI.h>
#include <QueueArray.h>
#include <TMC26XGenerator.h>
#include <Metro.h>
#include <CmdMessenger.h>

#include "constants.h"
#include "types.h"

//##################
//# Debug Settings #
//##################

//#define DEBUG_MOTOR_CONTFIG
#define DEBUG_HOMING
//#define DEBUG_ENDSTOPS

//#define DEBUG_MOTION
//#define DEBUG_MOTION_TRACE
//#define DEBUG_MOTION_TRACE_SHORT
//#define DEBUG_MOTOR_QUEUE
//#define DEBUG_MOTION_STATUS
//#define DEBUG_X_POS

//#define DEBUG_STATUS

//how much space do we have to store commands
#define COMMAND_QUEUE_LENGTH 40

#define CALCULATE_OUTPUT 13
//the CS pins have to be defines for digitalWriteFast 
#define SQUIRREL_0_PIN 8
#define SQUIRREL_1_PIN 12
#define TMC5041_PIN 11

//standards
#define TMC_26X_CONFIG 0x8440000a //SPI-Out: block/low/high_time=8/4/4 Takte; CoverLength=autom; TMC26x
#define TMC260_SENSE_RESISTOR_IN_MO 150
#define CLOCK_FREQUENCY 16000000ul
#define DEFAULT_CURRENT_IN_MA 1
#define DEFAULT_STEPS_PER_REVOLUTION 200

#define DEFAULT_ACCELERATION 1
#define DEFAULT_BOW 1

#define DEFAULT_COMMAND_BUFFER_DEPTH (nr_of_motors)

//how many motors do we know?
const char nr_of_motors = 2;

//how many otors can be theoretically geared together
#define MAX_FOLLOWING_MOTORS (nr_of_motors-1)

squirrel motors[nr_of_motors] = {
  {
    3,0, motor_1_target_reached, 
    TMC26XGenerator(DEFAULT_CURRENT_IN_MA,TMC260_SENSE_RESISTOR_IN_MO),
    DEFAULT_STEPS_PER_REVOLUTION      }
  ,
  {
    2,1, motor_2_target_reached, 
    TMC26XGenerator(DEFAULT_CURRENT_IN_MA,TMC260_SENSE_RESISTOR_IN_MO), 
    DEFAULT_STEPS_PER_REVOLUTION       }
};
const char reset_squirrel = 4;
const char start_signal_pin = 7;

motion_state current_motion_state = no_motion;

//config

QueueArray<movement> moveQueue = QueueArray<movement>(COMMAND_QUEUE_LENGTH);


//somebody must deal with our commands
CmdMessenger messenger = CmdMessenger(Serial1);
//watchdog Metro
Metro watchDogMetro = Metro(1000); 

void setup() {
  //at least we should try deactivate the squirrels
  pinModeFast(reset_squirrel,OUTPUT);
  digitalWriteFast(reset_squirrel, LOW);

  pinModeFast(TMC5041_PIN,OUTPUT);
  digitalWriteFast(TMC5041_PIN,HIGH);


  //initialize the serial port for commands
  Serial1.begin(115200);
  Serial.begin(115200); 
  //set the serial as debug output 
  moveQueue.setStream(Serial);

#ifdef CALCULATE_OUTPUT
  pinModeFast(CALCULATE_OUTPUT, OUTPUT);
  digitalWriteFast(CALCULATE_OUTPUT,LOW);
#endif

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








