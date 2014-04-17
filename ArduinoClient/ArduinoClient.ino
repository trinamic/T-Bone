#include <avr/interrupt.h>
#include <digitalIOPerformance.h>
#include <SPI.h>
#include <QueueArray.h>
#include <TMC26XGenerator.h>
#include <CmdMessenger.h>

#include "constants.h"
#include "types.h"

//##################
//# Debug Settings #
//##################

//#define DEBUG_MOTOR_CONTFIG
//#define DEBUG_HOMING
//#define DEBUG_HOMING_STATUS
//#define DEBUG_HOMING_STATUS_5041
//#define DEBUG_ENDSTOPS
//#define DEBUG_ENDSTOPS_DETAIL

//#define DEBUG_MOTION
//#define DEBUG_MOTION_SHORT
//#define DEBUG_MOTION_TRACE
//#define DEBUG_MOTION_TRACE_SHORT
//#define DEBUG_MOTION_START
//#define DEBUG_MOTION_REGISTERS
//#define DEBUG_MOTOR_QUEUE
//#define DEBUG_MOTION_STATUS
//#define DEBUG_X_POS
//#define DEBUG_SET_POS

//#define DEBUG_SPI

//#define DEBUG_STATUS
//#define DEBUG_STATUS_SHORT
//#define DEBUG_TMC5041_STATUS
//#define DEBUG_TMC4361_STATUS

#define RX_TX_BLINKY

//how many motors do we know?
const char nr_of_coordinated_motors = 3;
const char nr_of_controlled_motors = 2;
const char nr_of_motors = nr_of_coordinated_motors + nr_of_controlled_motors;
const char homing_max_following_motors = nr_of_controlled_motors - 1;

//how much space do we have to store commands
#define COMMAND_QUEUE_LENGTH 50

//standards
#define TMC_260_CONFIG 0x8440000a //SPI-Out: block/low/high_time=8/4/4 Takte; CoverLength=autom; TMC26x
#define TMC260_SENSE_RESISTOR_IN_MO 100 // Lars: 100mOhm on electronic
#define CLOCK_FREQUENCY 16000000ul
#define DEFAULT_CURRENT_IN_MA 10
#define DEFAULT_STEPS_PER_REVOLUTION 200

#define DEFAULT_ACCELERATION 1
#define DEFAULT_BOW 1

//#define CALCULATE_OUTPUT TXLED
//the CS pins have to be defines for digitalWriteFast
#define CS_4361_1_PIN 4
#define CS_4361_2_PIN 12
#define CS_4361_3_PIN 6
#define START_SIGNAL_PIN A3
#define CS_5041_PIN 11

#define INT_5041_PIN 8
#define INT_4361_1_PIN 3
#define INT_4361_2_PIN 2
#define INT_4361_3_PIN 7

#define PWM1_PIN 9
#define PWM2_PIN 10

#define TEMP1_PIN A0
#define TEMP2_PIN A1
#define TEMP3_PIN A2

#define FB1_PIN A4
#define FB2_PIN A5

TMC4361_info motors[nr_of_coordinated_motors] = {
  {
    INT_4361_1_PIN,0, motor_1_target_reached, 
    TMC26XGenerator(DEFAULT_CURRENT_IN_MA,TMC260_SENSE_RESISTOR_IN_MO),
    DEFAULT_STEPS_PER_REVOLUTION                    }
  ,
  {
    INT_4361_2_PIN,1, motor_2_target_reached, 
    TMC26XGenerator(DEFAULT_CURRENT_IN_MA,TMC260_SENSE_RESISTOR_IN_MO), 
    DEFAULT_STEPS_PER_REVOLUTION                     }
  ,
  {
    INT_4361_3_PIN,4, motor_3_target_reached, 
    TMC26XGenerator(DEFAULT_CURRENT_IN_MA,TMC260_SENSE_RESISTOR_IN_MO), 
    DEFAULT_STEPS_PER_REVOLUTION                     }
};


#define DEFAULT_COMMAND_BUFFER_DEPTH (nr_of_motors)

//how many otors can be theoretically geared together
#define MAX_FOLLOWING_MOTORS (nr_of_motors-1)


motion_state current_motion_state = no_motion;
//this bitfiled store which motors are invesed - _BV(1) means the motor is inversed)
unsigned char inverted_motors;



volatile boolean next_move_prepared = false;
volatile boolean move_executing = false;
volatile unsigned int motor_status;
volatile unsigned char target_motor_status;
volatile unsigned char next_target_motor_status;
char direction[nr_of_coordinated_motors];
unsigned char min_buffer_depth = DEFAULT_COMMAND_BUFFER_DEPTH;


//config

QueueArray<movement> moveQueue = QueueArray<movement>(COMMAND_QUEUE_LENGTH);


//somebody must deal with our commands
CmdMessenger messenger = CmdMessenger(Serial1);

void setup() {
  //pwm is done in the bbb
  pinModeFast(PWM1_PIN,INPUT);
  digitalWriteFast(PWM1_PIN,LOW);
  pinModeFast(PWM2_PIN,INPUT);
  digitalWriteFast(PWM2_PIN,LOW);

  //switch the analog pins as innput to hand them over to the BBB
  pinModeFast(TEMP1_PIN,INPUT);
  digitalWriteFast(TEMP1_PIN,LOW);
  pinModeFast(TEMP2_PIN,INPUT);
  digitalWriteFast(TEMP2_PIN,LOW);
  pinModeFast(TEMP3_PIN,INPUT);
  digitalWriteFast(TEMP3_PIN,LOW);
  pinModeFast(FB1_PIN,INPUT);
  digitalWriteFast(FB1_PIN,LOW);
  pinModeFast(FB1_PIN,INPUT);
  digitalWriteFast(FB1_PIN,LOW);

  //initialize SPI
  SPI.begin();
  //SPI.setClockDivider(SPI_CLOCK_DIV2);

  // Use HWBE as Output
  DDRE |= _BV(2);                    // set HWBE pin as output (Fuse HWBE disabled, point to check..)

  // Analog reference AREF
  analogReference(EXTERNAL);

  //set the TMC4361 ipns correctly
  prepareTMC4361();
  //set the TMMC5041 pins correctly
  prepareTMC5041();

  //at least we should try deactivate the motion drivers
  resetTMC4361(true, true);


  //initialize the serial port for commands
  Serial1.begin(38400);
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

unsigned long last_millis=0;

void loop() {
  //move if neccessary
  checkMotion();
  // Process incoming serial data, and perform callbacks
  messenger.feedinSerialData();

  if (millis()-last_millis>1000) {
#ifdef RX_TX_BLINKY
    TXLED1;
#endif
    watchDogPing();
    last_millis=millis();
#ifdef RX_TX_BLINKY
    TXLED0;
#endif
  }
}
