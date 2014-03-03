#include <SPI.h>

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

// Define Chip select Output
#define CS_5041_PIN 11

// Define other signals
#define INT_5041_PIN 8

// TMC5041
unsigned int AMP5041 = 200;
unsigned int holdAMP5041 = 100;
unsigned long chopper_config;
unsigned long current_register;

#define TMC_5031_R_SENSE 0.27
#define I_HOLD_DELAY 2

// TMC4361
long vmax = 50000000ul;
long amax = vmax/100;
long dmax = amax;


unsigned char status;

void setup()
{
  // Use HWBE as Output
  DDRE |= _BV(2);                    // set HWBE pin as output (Fuse HWBE disabled, point to check..)
  
  // Analog reference AREF
  analogReference(EXTERNAL);
  
  // Config SPI (CPOL = 1 and CPHA = 1)
  //SPI.setBitOrder(MSBFIRST);              // MSB sent first (TMC)
  //SPI.setClockDivider(SPI_CLOCK_DIV4);    // At 16MHz CLK = 4MHz
  //SPI.setDataMode(SPI_MODE3);             // Set CPOL=1 and CPHA=1
  SPI.begin();
  Serial.begin(9600);
  
  // Use HWBE pin to reset motion controller TMC4361
  delay(100);
  PORTE &= ~(_BV(2));          //to check (reset for motion controller)
  delay(100);
  PORTE |= _BV(2);
  
  // Set CS IOs
  pinMode(CS_5041_PIN, OUTPUT);
  digitalWrite(CS_5041_PIN, LOW);
  
  
  // Set other IOs
  pinMode(INT_5041_PIN, INPUT);

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
  writeRegister(CS_5041_PIN, TMC5031_HOLD_RUN_CURRENT_REGISTER_1,current_register);
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
  writeRegister(CS_5041_PIN, TMC5031_CHOPPER_CONFIGURATION_REGISTER_1,chopper_config);
  writeRegister(CS_5041_PIN, TMC5031_CHOPPER_CONFIGURATION_REGISTER_2,chopper_config);
  //configure reference switches
  writeRegister(CS_5041_PIN, TMC5031_REFERENCE_SWITCH_CONFIG_REGISTER_1, 0x0);
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
  writeRegister(CS_5041_PIN, TMC5031_X_TARGET_REGISTER_2, 200000);
  
  writeRegister(CS_5041_PIN, TMC5031_X_ACTUAL_REGISTER_1, 0);
  writeRegister(CS_5041_PIN, TMC5031_V_MAX_REGISTER_1, 10000ul);
  writeRegister(CS_5041_PIN, TMC5031_X_TARGET_REGISTER_1, 200000);
  
}

unsigned long target=0;

void loop()
{
  Serial.println("5041: ");
  readRegister(CS_5041_PIN, TMC5031_X_ACTUAL_REGISTER_2, 0);
  readRegister(CS_5041_PIN, TMC5031_X_ACTUAL_REGISTER_1, 0);
  delay(100);
}
