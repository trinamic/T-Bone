#include <SPI.h>
#include <TMC26XGenerator.h>
#include <Metro.h>

//#define DEBUG

//simple FP math see https://ucexperiment.wordpress.com/2012/10/28/fixed-point-math-on-the-arduino-platform/
#define FIXED_8_24_MAKE(a)     (int32_t)((a*(1ul << 24ul)))

//config
unsigned char steps_per_revolution = 200;
unsigned int current_in_ma = 500;
long vmax = 10000000ul;
long bow = 1000000;
long end_bow = bow;
long amax = vmax/100;
long dmax = amax;

//register
#define GENERAL_CONFIG_REGISTER 0x0
#define START_CONFIG_REGISTER 0x2
#define SPIOUT_CONF_REGISTER 0x04
#define STEP_CONF_REGISTER 0x0A
#define STATUS_REGISTER 0x0e
#define GEAR_RATIO_REGISTER 0x12
#define RAMP_MODE_REGISTER 0x20
#define X_ACTUAL_REGISTER 0x21
#define V_MAX_REGISTER 0x24
#define A_MAX_REGISTER 0x28
#define D_MAX_REGISTER 0x29
#define BOW_1_REGISTER 0x2d
#define BOW_2_REGISTER 0x2e
#define BOW_3_REGISTER 0x2f
#define BOW_4_REGISTER 0x30
#define CLK_FREQ_REGISTER 0x31
#define X_TARGET_REGISTER 0x37
#define COVER_LOW_REGISTER 0x6c
#define COVER_HIGH_REGISTER 0x6d

//values
#define TMC_26X_CONFIG_SPI 0x8440000a //SPI-Out: block/low/high_time=8/4/4 Takte; CoverLength=autom; TMC26x
#define TMC_26X_CONFIG_SD 0x8440000b //SPI-Out: block/low/high_time=8/4/4 Takte; CoverLength=autom; TMC26x
#define TMC260_SENSE_RESISTOR_IN_MO 150
#define CLOCK_FREQUENCY 16000000ul

//we have a TMC260 at the end so we configure a configurer
TMC26XGenerator tmc260 = TMC26XGenerator(current_in_ma,TMC260_SENSE_RESISTOR_IN_MO);

//a metro to control the movement
Metro moveMetro = Metro(5000ul);
Metro checkMetro = Metro(1000ul);

int squirrel_a = 7;
int interrupt_a = 2;
int squirrel_b = 8;
int interrupt_b = 3;
int reset_squirrel = 4;

void setup() {
  //reset the quirrel
  pinMode(reset_squirrel,OUTPUT);
  digitalWrite(reset_squirrel, LOW);
  delay(10);
  digitalWrite(reset_squirrel, HIGH);
  pinMode(squirrel_a,OUTPUT);
  digitalWrite(squirrel_a,HIGH);
  pinMode(squirrel_b,OUTPUT);
  digitalWrite(squirrel_b,HIGH);
  //configure the interrupt routines
  pinMode(interrupt_a,INPUT);
  digitalWrite(interrupt_a,LOW);
  pinMode(interrupt_b,INPUT);
  digitalWrite(interrupt_b,LOW);
  //initialize the serial port for debugging
  Serial.begin(9600);
  //initialize SPI
  SPI.begin();
  //configure the TMC26x A
  write43x(squirrel_a, GENERAL_CONFIG_REGISTER, 0); //we use xtarget
  write43x(squirrel_a, CLK_FREQ_REGISTER,CLOCK_FREQUENCY);
  write43x(squirrel_a, START_CONFIG_REGISTER,_BV(10)); //start automatically
  write43x(squirrel_a, RAMP_MODE_REGISTER,_BV(2) | 2); //we want to go to positions in nice S-Ramps ()TDODO does not work)
  write43x(squirrel_a, BOW_1_REGISTER,bow);
  write43x(squirrel_a, BOW_2_REGISTER,end_bow);
  write43x(squirrel_a, BOW_3_REGISTER,end_bow);
  write43x(squirrel_a, BOW_4_REGISTER,bow);
  //configure the motor type
  unsigned long motorconfig = 0x00; //we want 256 microsteps
  motorconfig |= steps_per_revolution<<4;

  write43x(squirrel_a, STEP_CONF_REGISTER,motorconfig);
  tmc260.setMicrosteps(256);
  write43x(squirrel_a, SPIOUT_CONF_REGISTER,TMC_26X_CONFIG_SD);
  set260Register(squirrel_a, tmc260.getDriverControlRegisterValue());
  set260Register(squirrel_a, tmc260.getChopperConfigRegisterValue());
  set260Register(squirrel_a, tmc260.getStallGuard2RegisterValue());
  set260Register(squirrel_a, tmc260.getDriverConfigurationRegisterValue());

  //configure the TMC26x B
  write43x(squirrel_b, GENERAL_CONFIG_REGISTER, _BV(6)); //we use xtarget
  write43x(squirrel_b, CLK_FREQ_REGISTER,CLOCK_FREQUENCY);
  write43x(squirrel_b, START_CONFIG_REGISTER,_BV(10)); //start automatically
  write43x(squirrel_b, RAMP_MODE_REGISTER,_BV(2) | 2); //we want to go to positions in nice S-Ramps ()TDODO does not work)
  write43x(squirrel_b, BOW_1_REGISTER,bow);
  write43x(squirrel_b, BOW_2_REGISTER,end_bow);
  write43x(squirrel_b, BOW_3_REGISTER,end_bow);
  write43x(squirrel_b, BOW_4_REGISTER,bow);
  //configure the motor type
  motorconfig = 0x00; //we want 256 microsteps
  motorconfig |= steps_per_revolution<<4;

  write43x(squirrel_b, STEP_CONF_REGISTER,motorconfig);
  tmc260.setMicrosteps(256);
  write43x(squirrel_b, SPIOUT_CONF_REGISTER,TMC_26X_CONFIG_SD);
  set260Register(squirrel_b, tmc260.getDriverControlRegisterValue());
  set260Register(squirrel_b, tmc260.getChopperConfigRegisterValue());
  set260Register(squirrel_b, tmc260.getStallGuard2RegisterValue());
  set260Register(squirrel_b, tmc260.getDriverConfigurationRegisterValue());

}

unsigned long tmc43xx_write;
unsigned long tmc43xx_read;

unsigned long target=0;

void loop() {
  if (target==0 | moveMetro.check()) {
    target=random(100000ul);
    unsigned long this_v = vmax+random(100)*vmax;
    write43x(squirrel_a, V_MAX_REGISTER,this_v << 8); //set the velocity - TODO recalculate float numbers
    write43x(squirrel_a, A_MAX_REGISTER,amax); //set maximum acceleration
    write43x(squirrel_a, D_MAX_REGISTER,dmax); //set maximum deceleration
    write43x(squirrel_a, X_TARGET_REGISTER,target);
    float gear_ratio = (float)random(101)/100.0;
    Serial.print("Gaer ration: ");
    Serial.println(gear_ratio);
    unsigned long digital_ratio = FIXED_8_24_MAKE(gear_ratio);
    Serial.print(" ");
    Serial.println(digital_ratio, HEX);  
    write43x(squirrel_b,GEAR_RATIO_REGISTER,digital_ratio);
    Serial.print("Move to ");
    Serial.println(target);
    Serial.println();
  }
  if (checkMetro.check()) {
    unsigned long position;
    Serial.print("XPOS A: ");
    position = read43x(squirrel_a,X_ACTUAL_REGISTER,0);
    Serial.print(position);
    Serial.print(" -> ");
    position  = read43x(squirrel_a,X_TARGET_REGISTER,0);
    Serial.println(position);

    Serial.print("XPOS B: ");
    position = read43x(squirrel_b,X_ACTUAL_REGISTER,0);
    Serial.print(position);
    Serial.print(" -> ");
    position  = read43x(squirrel_b,X_TARGET_REGISTER,0);
    Serial.println(position);
    Serial.println();
  }
}






