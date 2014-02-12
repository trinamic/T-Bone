#include <SPI.h>
#include <TMC26XGenerator.h>

// TMC4361
#define GENERAL_CONFIG_REGISTER 0x0
#define START_CONFIG_REGISTER 0x2
#define SPIOUT_CONF_REGISTER 0x04
#define STEP_CONF_REGISTER 0x0A
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

#define REFERENCE_CONFIG_REGISTER 0x01
#define VIRTUAL_STOP_LEFT_REGISTER 0x33
#define VIRTUAL_STOP_RIGHT_REGISTER 0x34
#define EVENTS_REGISTER 0x0e
#define STATUS_REGISTER 0x0f
#define X_ACTUAL_REGISTER 0x21
#define X_LATCH_REGISTER 0x36

// TMC2660
#define TMC_26X_CONFIG 0x8440000a //SPI-Out: block/low/high_time=8/4/4 Takte; CoverLength=autom; TMC26x
#define TMC260_SENSE_RESISTOR_IN_MO 100
#define CLOCK_FREQUENCY 16000000ul

// TMC5031
#define TMC5031_GENERAL_CONFIG_REGISTER 0x00
#define TMC5031_GENERAL_STATUS_REGISTER 0x01
#define TMC5031_INPUT_REGISTER 0x04

#define TMC5031_RAMP_MODE_REGISTER_1 0x20
#define TMC5031_X_ACTUAL_REGISTER_1 0x21
#define TMC5031_V_ACTUAL_REGISTER_1 0x22
#define TMC5031_A_1_REGISTER_1 0x24
#define TMC5031_V_1_REGISTER_1 0x25
#define TMC5031_A_MAX_REGISTER_1 0x26
#define TMC5031_V_MAX_REGISTER_1 0x27
#define TMC5031_D_MAX_REGISTER_1 0x28
#define TMC5031_D_1_REGISTER_1 0x29a
#define TMC5031_V_STOP_REGISTER_1 0x29b
#define TMC5031_X_TARGET_REGISTER_1 0x2d
#define TMC5031_HOLD_RUN_CURRENT_REGISTER_1 0x30
#define TMC5031_REFERENCE_SWITCH_CONFIG_REGISTER_1 0x34
#define TMC5031_RAMP_STATUS_REGISTER_2 0x35
#define TMC5031_CHOPPER_CONFIGURATION_REGISTER_1 0x6c
#define TMC5031_DRIVER_STATUS_REGISTER_1 0x6f

#define TMC5031_RAMP_MODE_REGISTER_2 0x40
#define TMC5031_X_ACTUAL_REGISTER_2 0x41
#define TMC5031_V_ACTUAL_REGISTER_2 0x42
#define TMC5031_A_1_REGISTER_2 0x44
#define TMC5031_V_1_REGISTER_2 0x45
#define TMC5031_A_MAX_REGISTER_2 0x46
#define TMC5031_V_MAX_REGISTER_2 0x47
#define TMC5031_D_MAX_REGISTER_2 0x48
#define TMC5031_D_1_REGISTER_2 0x4a
#define TMC5031_V_STOP_REGISTER_2 0x49b
#define TMC5031_X_TARGET_REGISTER_2 0x4d
#define TMC5031_HOLD_RUN_CURRENT_REGISTER_2 0x50
#define TMC5031_REFERENCE_SWITCH_CONFIG_REGISTER_2 0x54
#define TMC5031_RAMP_STATUS_REGISTER_2 0x55
#define TMC5031_CHOPPER_CONFIGURATION_REGISTER_2 0x7c
#define TMC5031_DRIVER_STATUS_REGISTER_2 0x7f

#define DEBUG
#define CLK_FREQ 16000000ul

// Define Chip select Output
#define CS_5041_PIN 11
#define CS_4361_1_PIN 4
#define CS_4361_2_PIN 12
#define CS_4361_3_PIN 6

// Define PWM Output
#define PWM1_PIN 9
#define PWM2_PIN 10
#define PWM3_PIN 5
int pwmTime = 0;
int pwmStep = 5;

// Define analog TEMP inputs
#define TEMP1_PIN A0                    // Analog 0
#define TEMP2_PIN A1                    // Analog 1
#define TEMP3_PIN A2                    // Analog 2
#define FEEDBACK1_PIN A4                // Analog 4
#define FEEDBACK2_PIN A5                // Analog 5

// Define other signals
#define INT_5041_PIN 8
#define INT_4361_1_PIN 3
#define INT_4361_2_PIN 2
#define INT_4361_3_PIN 7
#define START_PIN A3

// TMC5041
unsigned int AMP5041 = 200;
unsigned int holdAMP5041 = 100;
unsigned long chopper_config;
unsigned long current_register;

#define TMC_5031_R_SENSE 0.27
#define I_HOLD_DELAY 2

// TMC4361
unsigned char steps_per_revolution = 200;
unsigned int AMP4361 = 500;
long vmax = 100000000ul;
long bow = 1000000;
long end_bow = bow;
long amax = vmax/100;
long dmax = amax;

// TMC2660
TMC26XGenerator tmc260 = TMC26XGenerator(AMP4361, TMC260_SENSE_RESISTOR_IN_MO);

unsigned char status;

void setup()
{
  // Analog reference AREF
  analogReference(EXTERNAL);
  
  // Config SPI (CPOL = 1 and CPHA = 1)
  //SPI.setBitOrder(MSBFIRST);              // MSB sent first (TMC)
  //SPI.setClockDivider(SPI_CLOCK_DIV4);    // At 16MHz CLK = 4MHz
  //SPI.setDataMode(SPI_MODE3);             // Set CPOL=1 and CPHA=1
  SPI.begin();
  Serial.begin(9600);
  
  // Set CS IOs
  pinMode(CS_5041_PIN, OUTPUT);
  digitalWrite(CS_5041_PIN, LOW);
  pinMode(CS_4361_1_PIN, OUTPUT);
  digitalWrite(CS_4361_1_PIN, LOW);
  pinMode(CS_4361_2_PIN, OUTPUT);
  digitalWrite(CS_4361_2_PIN, LOW);
  pinMode(CS_4361_3_PIN, OUTPUT);
  digitalWrite(CS_4361_3_PIN, LOW);
  
  // Set PWM IOs
  pinMode(PWM1_PIN, OUTPUT);
  digitalWrite(PWM1_PIN, LOW);
  pinMode(PWM2_PIN, OUTPUT);
  digitalWrite(PWM2_PIN, LOW);
  pinMode(PWM3_PIN, OUTPUT);
  digitalWrite(PWM3_PIN, LOW);
  
  // Set other IOs
  pinMode(INT_5041_PIN, INPUT);
  pinMode(INT_4361_1_PIN, INPUT);
  pinMode(INT_4361_2_PIN, INPUT);
  pinMode(INT_4361_3_PIN, INPUT);
  pinMode(START_PIN, OUTPUT);

  // motor config
  unsigned long motorconfig = 0x00; //we want 256 microsteps
  motorconfig |= steps_per_revolution << 4;

  // 4361_1
  writeRegister(CS_4361_1_PIN, GENERAL_CONFIG_REGISTER, _BV(9));       // xtarget
  writeRegister(CS_4361_1_PIN, CLK_FREQ_REGISTER, CLOCK_FREQUENCY);
  writeRegister(CS_4361_1_PIN, START_CONFIG_REGISTER, _BV(10));         // start automatically
  writeRegister(CS_4361_1_PIN, RAMP_MODE_REGISTER, _BV(2) | 2);         // we want to go to positions in nice S-Ramps ()TDODO does not work)
  writeRegister(CS_4361_1_PIN, BOW_1_REGISTER, bow);
  writeRegister(CS_4361_1_PIN, BOW_2_REGISTER, end_bow);
  writeRegister(CS_4361_1_PIN, BOW_3_REGISTER, end_bow);
  writeRegister(CS_4361_1_PIN, BOW_4_REGISTER, bow);
  
  writeRegister(CS_4361_1_PIN, STEP_CONF_REGISTER, motorconfig);
  tmc260.setMicrosteps(256);
  writeRegister(CS_4361_1_PIN, SPIOUT_CONF_REGISTER, TMC_26X_CONFIG);
  set260Register(CS_4361_1_PIN, tmc260.getDriverControlRegisterValue());
  set260Register(CS_4361_1_PIN, tmc260.getChopperConfigRegisterValue());
  set260Register(CS_4361_1_PIN, tmc260.getStallGuard2RegisterValue());
  set260Register(CS_4361_1_PIN, tmc260.getDriverConfigurationRegisterValue() | 0x80);
  
  writeRegister(CS_4361_1_PIN, REFERENCE_CONFIG_REGISTER, 0x0);
  
  writeRegister(CS_4361_1_PIN, V_MAX_REGISTER, vmax << 8); //set the velocity - TODO recalculate float numbers
  writeRegister(CS_4361_1_PIN, A_MAX_REGISTER, amax); //set maximum acceleration
  writeRegister(CS_4361_1_PIN, D_MAX_REGISTER, dmax); //set maximum deceleration
  
  // 4361_2
  writeRegister(CS_4361_2_PIN, GENERAL_CONFIG_REGISTER, _BV(9));       // xtarget
  writeRegister(CS_4361_2_PIN, CLK_FREQ_REGISTER, CLOCK_FREQUENCY);
  writeRegister(CS_4361_2_PIN, START_CONFIG_REGISTER, _BV(10));         // start automatically
  writeRegister(CS_4361_2_PIN, RAMP_MODE_REGISTER, _BV(2) | 2);         // we want to go to positions in nice S-Ramps ()TDODO does not work)
  writeRegister(CS_4361_2_PIN, BOW_1_REGISTER, bow);
  writeRegister(CS_4361_2_PIN, BOW_2_REGISTER, end_bow);
  writeRegister(CS_4361_2_PIN, BOW_3_REGISTER, end_bow);
  writeRegister(CS_4361_2_PIN, BOW_4_REGISTER, bow);
  
  writeRegister(CS_4361_2_PIN, STEP_CONF_REGISTER, motorconfig);
  tmc260.setMicrosteps(256);
  writeRegister(CS_4361_2_PIN, SPIOUT_CONF_REGISTER, TMC_26X_CONFIG);
  set260Register(CS_4361_2_PIN, tmc260.getDriverControlRegisterValue());
  set260Register(CS_4361_2_PIN, tmc260.getChopperConfigRegisterValue());
  set260Register(CS_4361_2_PIN, tmc260.getStallGuard2RegisterValue());
  set260Register(CS_4361_2_PIN, tmc260.getDriverConfigurationRegisterValue() | 0x80);
  
  writeRegister(CS_4361_2_PIN, REFERENCE_CONFIG_REGISTER, 0x0);
  
  writeRegister(CS_4361_2_PIN, V_MAX_REGISTER, vmax << 8); //set the velocity - TODO recalculate float numbers
  writeRegister(CS_4361_2_PIN, A_MAX_REGISTER, amax); //set maximum acceleration
  writeRegister(CS_4361_2_PIN, D_MAX_REGISTER, dmax); //set maximum deceleration
  
  // 4361_3
  writeRegister(CS_4361_3_PIN, GENERAL_CONFIG_REGISTER, _BV(9));       // xtarget
  writeRegister(CS_4361_3_PIN, CLK_FREQ_REGISTER, CLOCK_FREQUENCY);
  writeRegister(CS_4361_3_PIN, START_CONFIG_REGISTER, _BV(10));         // start automatically
  writeRegister(CS_4361_3_PIN, RAMP_MODE_REGISTER, _BV(2) | 2);         // we want to go to positions in nice S-Ramps ()TDODO does not work)
  writeRegister(CS_4361_3_PIN, BOW_1_REGISTER, bow);
  writeRegister(CS_4361_3_PIN, BOW_2_REGISTER, end_bow);
  writeRegister(CS_4361_3_PIN, BOW_3_REGISTER, end_bow);
  writeRegister(CS_4361_3_PIN, BOW_4_REGISTER, bow);
  
  writeRegister(CS_4361_3_PIN, STEP_CONF_REGISTER, motorconfig);
  tmc260.setMicrosteps(256);
  writeRegister(CS_4361_3_PIN, SPIOUT_CONF_REGISTER, TMC_26X_CONFIG);
  set260Register(CS_4361_3_PIN, tmc260.getDriverControlRegisterValue());
  set260Register(CS_4361_3_PIN, tmc260.getChopperConfigRegisterValue());
  set260Register(CS_4361_3_PIN, tmc260.getStallGuard2RegisterValue());
  set260Register(CS_4361_3_PIN, tmc260.getDriverConfigurationRegisterValue() | 0x80);
  
  writeRegister(CS_4361_3_PIN, REFERENCE_CONFIG_REGISTER, 0x0);
  
  writeRegister(CS_4361_3_PIN, V_MAX_REGISTER, vmax << 8); //set the velocity - TODO recalculate float numbers
  writeRegister(CS_4361_3_PIN, A_MAX_REGISTER, amax); //set maximum acceleration
  writeRegister(CS_4361_3_PIN, D_MAX_REGISTER, dmax); //set maximum deceleration
  
  // --------------------------------------------------------------
  // ---------------------TMC5041---ANFANG-------------------------
  // --------------------------------------------------------------
  //initialize the genereal configuration of the tmc 5031
  writeRegister(CS_5041_PIN, TMC5031_GENERAL_CONFIG_REGISTER, _BV(3)); //int/PP are outputs
  writeRegister(CS_5041_PIN, TMC5031_RAMP_MODE_REGISTER_1,0); //enforce positioing mode
  writeRegister(CS_5041_PIN, TMC5031_RAMP_MODE_REGISTER_2,0); //enforce positioing mode
  unsigned char run_current = calculateCurrent(AMP5041);
  boolean low_sense = run_current & 0x80;
  run_current = run_current & 0x7F;
  unsigned char hold_current;
  if (low_sense) {
    hold_current=calculateCurrentLowSense(holdAMP5041);
  } 
  else {
    hold_current=calculateCurrentHighSense(holdAMP5041);
  }
  current_register=0;
  //se89t the holding delay
  current_register |= I_HOLD_DELAY << 16;
  current_register |= run_current << 8;
  current_register |= hold_current;
  writeRegister(CS_5041_PIN, TMC5031_HOLD_RUN_CURRENT_REGISTER_2,current_register);
  chopper_config = 0
    | (2<<15) // comparator blank time 2=34
    | _BV(13) //random t_off
      | (3 << 7) //hysteresis end time
        | (5 << 4) // hysteresis start time
          | 5 //t OFF
          ;
  if (!low_sense) {
    chopper_config|= _BV(17); //lower v_sense
  } 
  writeRegister(CS_5041_PIN, TMC5031_CHOPPER_CONFIGURATION_REGISTER_2,chopper_config);
  //configure reference switches
  writeRegister(CS_5041_PIN, TMC5031_REFERENCE_SWITCH_CONFIG_REGISTER_2, 0x0);
  //Set the basic config parameters 
  unsigned long acceleration = vmax/10;
  writeRegister(CS_5041_PIN, TMC5031_A_MAX_REGISTER_2,acceleration);
  writeRegister(CS_5041_PIN, TMC5031_D_MAX_REGISTER_2,acceleration);
  writeRegister(CS_5041_PIN, TMC5031_A_1_REGISTER_2,acceleration);
  writeRegister(CS_5041_PIN, TMC5031_V_1_REGISTER_2,0);
  writeRegister(CS_5041_PIN, TMC5031_D_1_REGISTER_2,acceleration); //the datahseet says it is needed
  writeRegister(CS_5041_PIN, TMC5031_V_STOP_REGISTER_2,vmax); //the datahseet says it is needed
  //get rid of the 'something happened after reboot' warning
  readRegister(CS_5041_PIN, TMC5031_GENERAL_STATUS_REGISTER,0);
  // --------------------------------------------------------------
  // ---------------------TMC5041---ENDE---------------------------
  // --------------------------------------------------------------

  // --------------------------------------------------------------
  // ---------------------TMC5041---ANFANG-------------------------
  // --------------------------------------------------------------
  writeRegister(CS_5041_PIN, TMC5031_CHOPPER_CONFIGURATION_REGISTER_1,chopper_config);
  //configure reference switches
  writeRegister(CS_5041_PIN, TMC5031_REFERENCE_SWITCH_CONFIG_REGISTER_1, 0x0);
  //Set the basic config parameters 
  writeRegister(CS_5041_PIN, TMC5031_A_MAX_REGISTER_1,acceleration);
  writeRegister(CS_5041_PIN, TMC5031_D_MAX_REGISTER_1,acceleration);
  writeRegister(CS_5041_PIN, TMC5031_A_1_REGISTER_1,acceleration);
  writeRegister(CS_5041_PIN, TMC5031_V_1_REGISTER_1,0);
  writeRegister(CS_5041_PIN, TMC5031_D_1_REGISTER_1,acceleration); //the datahseet says it is needed
  writeRegister(CS_5041_PIN, TMC5031_V_STOP_REGISTER_1,vmax); //the datahseet says it is needed
  // --------------------------------------------------------------
  // ---------------------TMC5041---ENDE---------------------------
  // --------------------------------------------------------------

  writeRegister(CS_5041_PIN, TMC5031_X_ACTUAL_REGISTER_2, 0);
  writeRegister(CS_5041_PIN, TMC5031_V_MAX_REGISTER_2, 10000ul);
  writeRegister(CS_5041_PIN, TMC5031_X_TARGET_REGISTER_2, 9999999);
  
  writeRegister(CS_5041_PIN, TMC5031_X_ACTUAL_REGISTER_1, 0);
  writeRegister(CS_5041_PIN, TMC5031_V_MAX_REGISTER_1, 10000ul);
  writeRegister(CS_5041_PIN, TMC5031_X_TARGET_REGISTER_1, 9999999);
  
  // Fahrbefehele
  writeRegister(CS_4361_1_PIN, X_ACTUAL_REGISTER, 0);
  writeRegister(CS_4361_1_PIN, X_TARGET_REGISTER, 9999999);
  writeRegister(CS_4361_2_PIN, X_ACTUAL_REGISTER, 0);
  writeRegister(CS_4361_2_PIN, X_TARGET_REGISTER, 9999999);
  writeRegister(CS_4361_3_PIN, X_ACTUAL_REGISTER, 0);
  writeRegister(CS_4361_3_PIN, X_TARGET_REGISTER, 9999999);
  //writeRegister(CS_4361_1_PIN, X_TARGET_REGISTER, 0);
  //writeRegister(CS_4361_2_PIN, X_TARGET_REGISTER, 0);
  //writeRegister(CS_4361_3_PIN, X_TARGET_REGISTER, 0);
}

unsigned long target=0;

void loop()
{
  Serial.println("4361_1: ");
  readRegister(CS_4361_1_PIN, X_ACTUAL_REGISTER, 0);
  Serial.println("4361_2: ");
  readRegister(CS_4361_2_PIN, X_ACTUAL_REGISTER, 0);
  Serial.println("4361_3: ");
  readRegister(CS_4361_3_PIN, X_ACTUAL_REGISTER, 0);
  Serial.println("5041: ");
  readRegister(CS_5041_PIN, TMC5031_X_ACTUAL_REGISTER_1, 0);
  delay(100);
  
  // PWM Test
  pwmTime = pwmTime + pwmStep;
  analogWrite(PWM1_PIN, pwmTime);
  analogWrite(PWM2_PIN, pwmTime);
  analogWrite(PWM3_PIN, pwmTime);
  
  if(pwmTime == 0 || pwmTime == 255)
  {
    pwmStep = -pwmStep;
  }
}







