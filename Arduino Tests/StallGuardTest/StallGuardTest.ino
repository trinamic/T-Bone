#include <digitalIOPerformance.h>
#include <SPI.h>
#include <TMC26XGenerator.h>

#include "constants.h"
#include "types.h"

//#define DEBUG_SPI

#define CS_4361_1_PIN 4
#define CS_4361_2_PIN 12
#define CS_4361_3_PIN 6
#define START_SIGNAL_PIN A3
#define CS_5041_PIN 11

#define INT_5041_PIN 8
#define INT_4361_1_PIN 3
#define INT_4361_2_PIN 2
#define INT_4361_3_PIN 7

#define DEFAULT_CURRENT_IN_MA 10
#define TMC260_SENSE_RESISTOR_IN_MO 150
#define DEFAULT_STEPS_PER_REVOLUTION 200
#define CLOCK_FREQUENCY 16000000ul
#define TMC_260_CONFIG 0x8440000a //SPI-Out: block/low/high_time=8/4/4 Takte; CoverLength=autom; TMC26x

const int microsteps = 256;
const char nr_of_coordinated_motors = 3;
const unsigned long slow_run = 4*microsteps; //steps/s
const unsigned long fast_run = DEFAULT_STEPS_PER_REVOLUTION*microsteps; //steps/s
const long homed_too_far = microsteps * 200ul * 10000ul; //if we moved that much we have long crushed into the mechanic

const char motor_to_test = 0;
const int test_current_in_ma = 1000;

void setup() {
    //initialize SPI
  SPI.begin();
  Serial.begin(115200);

    // Use HWBE as Output
  DDRE |= _BV(2);                    // set HWBE pin as output (Fuse HWBE disabled, point to check..)

  Serial.println("starting");
  initTMC4361();
  intializeTMC260();
  setCurrentTMC260(motor_to_test, test_current_in_ma);
}

void loop() {
  home_on_sg();
  delay(1000);
  // put your main code here, to run repeatedly: 
}


TMC4361_info motors[nr_of_coordinated_motors] = {
  {
    INT_4361_1_PIN,0, NULL, 
    TMC26XGenerator(DEFAULT_CURRENT_IN_MA,TMC260_SENSE_RESISTOR_IN_MO),
    DEFAULT_STEPS_PER_REVOLUTION                  }
  ,
  {
    INT_4361_2_PIN,1, NULL, 
    TMC26XGenerator(DEFAULT_CURRENT_IN_MA,TMC260_SENSE_RESISTOR_IN_MO), 
    DEFAULT_STEPS_PER_REVOLUTION                   }
  ,
  {
    INT_4361_3_PIN,4, NULL, 
    TMC26XGenerator(DEFAULT_CURRENT_IN_MA,TMC260_SENSE_RESISTOR_IN_MO), 
    DEFAULT_STEPS_PER_REVOLUTION                   }
};

