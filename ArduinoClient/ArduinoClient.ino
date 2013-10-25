#include <SPI.h>
#include <TMC26XGenerator.h>
#include <Metro.h>

#include "constants.h"

#define DEBUG

//standards
int cs_squirrel = 7;
#define TMC_26X_CONFIG 0x8440000a //SPI-Out: block/low/high_time=8/4/4 Takte; CoverLength=autom; TMC26x
#define TMC260_SENSE_RESISTOR_IN_MO 150
#define CLOCK_FREQUENCY 16000000ul

//config
unsigned char steps_per_revolution = 200;
unsigned int current_in_ma = 500;
long vmax = 5000;
long amax = vmax/100;
long dmax = amax;

void setup() {
  //initialize the serial port for debugging
  Serial.begin(9600);


}

unsigned long tmc43xx_write;
unsigned long tmc43xx_read;

unsigned long target=0;

void loop() {
}

