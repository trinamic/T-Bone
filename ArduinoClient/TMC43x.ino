
void initialzeTMC43x() {
  //initialize CS pin
  pinMode(cs_squirrel,OUTPUT);
  digitalWrite(cs_squirrel,HIGH);
  //reset the quirrel
  pinMode(reset_squirrel,OUTPUT);
  digitalWrite(reset_squirrel, LOW);
  delay(1);
  digitalWrite(reset_squirrel, HIGH);
  delay(10);
  //initialize SPI
  SPI.begin();
  //preconfigure the TMC43x
  write43x(GENERAL_CONFIG_REGISTER,_BV(9) | _BV(1) | _BV(2)); //we use xtarget
  write43x(CLK_FREQ_REGISTER,CLOCK_FREQUENCY);
  write43x(START_CONFIG_REGISTER,_BV(10)); //start automatically
  write43x(RAMP_MODE_REGISTER,_BV(2) | 1); //we want to go to positions in nice S-Ramps

  setStepsPerRevolution(steps_per_revolution);
}

const __FlashStringHelper* setStepsPerRevolution(unsigned int steps) {
  //configure the motor type
  unsigned long motorconfig = 0x00; //we want 256 microsteps
  motorconfig |= steps_per_revolution<<4;
  write43x(STEP_CONF_REGISTER,motorconfig);
  steps_per_revolution = steps;
  return NULL;
}

const __FlashStringHelper* moveMotor(unsigned long pos, unsigned long vMax, unsigned long aMax, unsigned long dMax) {
  if (dMax==0) {
    dMax = aMax;
  }
  write43x(V_MAX_REGISTER,vmax << 8); //set the velocity - TODO recalculate float numbers
  write43x(A_MAX_REGISTER,amax); //set maximum acceleration
  write43x(D_MAX_REGISTER,dmax); //set maximum deceleration
  write43x(X_TARGET_REGISTER,pos);
  Serial.print("moving to ");
  Serial.println(pos);
  return NULL;
}

