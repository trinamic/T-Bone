#include <digitalIOPerformance.h>
#include <SPI.h>
#include <TMC26XGenerator.h>

#include "constants.h"
#include "types.h"

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

const char nr_of_coordinated_motors = 3;
const unsigned long slow_run = 4; //steps/s
const unsigned long fast_run = DEFAULT_STEPS_PER_REVOLUTION; //steps/s

void setup() {
  prepareTMC4361();
}

void loop() {
  // put your main code here, to run repeatedly: 
  
}
