#include <SPI.h>
#include <TMC26XGenerator.h>
#include <Metro.h>

//#define DEBUG_SPI
#define DEBUG

//config
unsigned char steps_per_revolution = 200;
unsigned int current_in_ma = 500;
long vmax = 100000000ul;
long bow = 1000000;
long end_bow = bow;
long amax = vmax/100;
long dmax = amax;
//how far will we go 
unsigned long random_range=1000000ul;
#define HIT_RANGE 0.1
//just some virtual endstops
long es_left = random_range*HIT_RANGE;
long es_right = random_range*(1.0-HIT_RANGE);

//register
#define GENERAL_CONFIG_REGISTER 0x0
#define START_CONFIG_REGISTER 0x2
#define SPIOUT_CONF_REGISTER 0x04
#define STEP_CONF_REGISTER 0x0A
#define STATUS_REGISTER 0x0e
#define RAMP_MODE_REGISTER 0x20
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
#define TMC_26X_CONFIG 0x8440000a //SPI-Out: block/low/high_time=8/4/4 Takte; CoverLength=autom; TMC26x
#define TMC260_SENSE_RESISTOR_IN_MO 150
#define CLOCK_FREQUENCY 16000000ul

//we have a TMC260 at the end so we configure a configurer
TMC26XGenerator tmc260 = TMC26XGenerator(current_in_ma,TMC260_SENSE_RESISTOR_IN_MO);

//a metro to control the movement
Metro moveMetro = Metro(5000ul);
Metro checkMetro = Metro(1000ul);

int squirrel_a = 12;
int squirrel_b = 8;
int reset_squirrel = 4;

void setup() {
  //initialize the serial port for debugging
  Serial.begin(9600);

  //reset the quirrel
  pinMode(reset_squirrel,OUTPUT);
  digitalWrite(reset_squirrel, LOW);
  delay(1);
  digitalWrite(reset_squirrel, HIGH);
  pinMode(squirrel_a,OUTPUT);
  digitalWrite(squirrel_a,HIGH);
  pinMode(squirrel_b,OUTPUT);
  digitalWrite(squirrel_b,HIGH);
  //initialize SPI
  SPI.begin();
  //configure the TMC26x A
  write43x(squirrel_a, GENERAL_CONFIG_REGISTER,_BV(9)); //we use xtarget
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
  write43x(squirrel_a, SPIOUT_CONF_REGISTER,TMC_26X_CONFIG);
  set260Register(squirrel_a, tmc260.getDriverControlRegisterValue());
  set260Register(squirrel_a, tmc260.getChopperConfigRegisterValue());
  set260Register(squirrel_a, tmc260.getStallGuard2RegisterValue());
  set260Register(squirrel_a, tmc260.getDriverConfigurationRegisterValue() | 0x80);

  //configure the TMC26x B
  write43x(squirrel_b, GENERAL_CONFIG_REGISTER,_BV(9)); //we use xtarget
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
  write43x(squirrel_b, SPIOUT_CONF_REGISTER,TMC_26X_CONFIG);
  set260Register(squirrel_b, tmc260.getDriverControlRegisterValue());
  set260Register(squirrel_b, tmc260.getChopperConfigRegisterValue());
  set260Register(squirrel_b, tmc260.getStallGuard2RegisterValue());
  set260Register(squirrel_b, tmc260.getDriverConfigurationRegisterValue() | 0x80);

}

unsigned long tmc43xx_write;
unsigned long tmc43xx_read;

unsigned long target=0;

void loop() {
  if (target==0 | moveMetro.check()) {
    unsigned char motor;
    if (random(2)) {
      Serial.println("Moving Motor A");
      motor=squirrel_a;
    } 
    else {
      Serial.println("Moving Motor B");
      motor=squirrel_b;
    }
    target=random(random_range);
    Serial.print("Move to ");
    Serial.println(target);
    Serial.println();
    unsigned long this_v = vmax+random(100)*vmax;
    if (target<es_left) {
      Serial.print("Target hits left endstop at");
      Serial.println(es_left);
    } 
    else if (target>es_right) {
      Serial.print("Target hits right endstop at");
      Serial.println(es_right);
    }
    write43x(motor, V_MAX_REGISTER,this_v << 8); //set the velocity - TODO recalculate float numbers
    write43x(motor, A_MAX_REGISTER,amax); //set maximum acceleration
    write43x(motor, D_MAX_REGISTER,dmax); //set maximum deceleration
    write43x(motor, X_TARGET_REGISTER,target);
  }
  if (checkMetro.check()) {
    // put your main code here, to run repeatedly: 
    read43x(squirrel_a, 0x21,0);
    Serial.print("x actual: ");
    unsigned long x_acutal = read43x(squirrel_a, 0x21,0);
    Serial.println(x_acutal);
  }
}




