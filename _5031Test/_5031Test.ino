#include <SPI.h>
#include <TMC26XGenerator.h>
#include <Metro.h>

#define DEBUG

//config
unsigned int run_current_in_ma = 500;
unsigned int hold_current_in_ma = 100;
long vmax = 100000000ul;

#define TMC_5031_R_SENSE 0.27
#define I_HOLD_DELAY 2

//register
#define TMC5031_GENERAL_CONFIG_REGISTER 0x00
#define TMC5031_RAMP_MODE_REGISTER_1 0x20
#define TMC5031_X_ACTUAL_REGISTER_1 0x21
#define TMC5031_X_TARGET_REGISTER_1 0x2d
#define TMC5031_V_MAX_REGISTER_2 0x27
#define TMC5031_HOLD_RUN_CURRENT_REGISTER_1 0x30
#define TMC5031_CHOPPER_CONFIGURATION_REGISTER_1 0x6c

#define TMC5031_RAMP_MODE_REGISTER_2 0x40
#define TMC5031_X_ACTUAL_REGISTER_2 0x41
#define TMC5031_V_MAX_REGISTER_2 0x47
#define TMC5031_X_TARGET_REGISTER_2 0x4d
#define TMC5031_HOLD_RUN_CURRENT_REGISTER_2 0x50
#define TMC5031_CHOPPER_CONFIGURATION_REGISTER_2 0x7c

//values

//we have a TMC260 at the end so we configure a configurer

//a metro to control the movement
Metro moveMetro = Metro(5000ul);
Metro checkMetro = Metro(1000ul);

int squirrel_a = 12;
int squirrel_b = 8;
int tmc_5031 = 11;
int reset_squirrel = 4;
unsigned long chopper_config;

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
  pinMode(tmc_5031,OUTPUT);
  digitalWrite(squirrel_b,HIGH);
  //initialize SPI
  SPI.begin();
  //initialize the genereal configuration of the tmc 5031
  write5031(TMC5031_GENERAL_CONFIG_REGISTER, _BV(3)); //int/PP are outputs
  write5031(TMC5031_RAMP_MODE_REGISTER_2,0); //enforce positioing mode
  unsigned char run_current = calculateCurrent(run_current_in_ma);
  boolean low_sense = run_current & 0x80;
  run_current = run_current & 0x7F;
  unsigned char hold_current;
  if (low_sense) {
    hold_current=calculateCurrentLowSense(hold_current_in_ma);
  } else {
    hold_current=calculateCurrentHighSense(hold_current_in_ma);
  }
  unsigned long current_register=0;
  //se89t the holding delay
  current_register |= I_HOLD_DELAY << 16;
  current_register |= run_current << 8;
  current_register |= hold_current <<4;
  write5031(TMC5031_HOLD_RUN_CURRENT_REGISTER_2,current_register);
  chopper_config = 0
    | (2<<15) // comparator blank time
    | _BV(13) //random t_off
    | (4<<7) //hysteresis end time
    | (3<< 4) // hysteresis start time
    | 5 //t OFF
    ;
   if (!low_sense) {
    chopper_config|= _BV(17); //lower v_sense
   } 
   write5031(TMC5031_CHOPPER_CONFIGURATION_REGISTER_2,chopper_config);
}

unsigned long target=0;

void loop() {
  if (target==0 | moveMetro.check()) {
    target=random(1000000ul);
    unsigned long this_v = vmax+random(100)*vmax;
    Serial.print("Move to ");
    Serial.println(target);
    Serial.println();
    write5031(TMC5031_X_TARGET_REGISTER_2, target);
    write5031(TMC5031_V_MAX_REGISTER_2, this_v);
  }
  if (checkMetro.check()) {
    unsigned long x_actual = read5031(TMC5031_X_ACTUAL_REGISTER_2,0);
    Serial.print("X: ");
    Serial.println(x_actual);
    // put your main code here, to run repeatedly: 
  }
}




