
void initialzeTMC43x() {
  //initialize SPI
  SPI.begin();
  //
  pinMode(cs_squirrel,OUTPUT);
  digitalWrite(cs_squirrel,HIGH);
  write43x(GENERAL_CONFIG_REGISTER,_BV(9) | _BV(1) | _BV(2)); //we use xtarget
  write43x(CLK_FREQ_REGISTER,CLOCK_FREQUENCY);
  write43x(START_CONFIG_REGISTER,_BV(10)); //start automatically
  write43x(RAMP_MODE_REGISTER,_BV(2) | 1); //we want to go to positions in nice S-Ramps
  write43x(V_MAX_REGISTER,vmax << 8); //set the velocity - TODO recalculate float numbers

  setStepsPerRevolution(steps_per_revolution);
}

const __FlashStringHelper* setStepsPerRevolution(unsigned int steps) {
  //configure the motor type
  unsigned long motorconfig = 0x00; //we want closed loop operation
  motorconfig |= steps_per_revolution<<4;
  write43x(STEP_CONF_REGISTER,motorconfig);
  steps_per_revolution = steps;
  return NULL;
}

