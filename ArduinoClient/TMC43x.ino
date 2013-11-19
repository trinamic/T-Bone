
void initialzeTMC43x() {
  //reset the quirrel
  digitalWrite(reset_squirrel, LOW);

  pinMode(start_signal_pin,INPUT);
  digitalWrite(start_signal_pin,LOW);

  //will be released after setup is complete   
  for (char i=0; i<nr_of_motors; i++) {
    //initialize CS pin
    digitalWrite(motors[i].cs_pin,HIGH);
    pinMode(motors[i].cs_pin,OUTPUT);
    pinMode(motors[i].target_reached_interrupt_pin,INPUT);
    digitalWrite(motors[i].target_reached_interrupt_pin,LOW);
  }
  //enable the reset again
  delay(1);
  digitalWrite(reset_squirrel, HIGH);
  delay(10);
  //initialize SPI
  SPI.begin();
  //preconfigure the TMC43x
  for (char i=0; i<nr_of_motors;i++) {
    write43x(motors[i].cs_pin, RAMP_MODE_REGISTER,_BV(2) | 2); //we want to go to positions in nice S-Ramps ()TDODO does not work)
    write43x(motors[i].cs_pin, SH_RAMP_MODE_REGISTER,_BV(2) | 2); //we want to go to positions in nice S-Ramps ()TDODO does not work)
    write43x(motors[i].cs_pin,CLK_FREQ_REGISTER,CLOCK_FREQUENCY);
    setStepsPerRevolution(motors[i].cs_pin,motors[i].steps_per_revolution);
  }
}

const __FlashStringHelper* setStepsPerRevolution(unsigned char motor_nr, unsigned int steps) {
  //get the motor number
  unsigned char cs_pin = motors[motor_nr].cs_pin;
  //configure the motor type
  unsigned long motorconfig = 0x00; //we want 256 microsteps
  motorconfig |= steps<<4;
  write43x(cs_pin,STEP_CONF_REGISTER,motorconfig);
  motors[motor_nr].steps_per_revolution = steps;
  return NULL;
}


const __FlashStringHelper* setAccelerationSetttings(unsigned char motor_nr, long aMax, long dMax,long startbow, long endbow) {
  //get the motor number
  unsigned char cs_pin = motors[motor_nr].cs_pin;
  //TODO some validity settings??
  motors[motor_nr].aMax = aMax;
  motors[motor_nr].dMax = dMax!=0? dMax:aMax;
  motors[motor_nr].startBow = startbow;
  motors[motor_nr].endBow = (endbow!=0)? endbow:startbow;

  if (endbow==0) {
    endbow=startbow;
  }

  write43x(cs_pin, A_MAX_REGISTER,motors[motor_nr].aMax); //set maximum acceleration
  write43x(cs_pin, SH_A_MAX_REGISTER,motors[motor_nr].aMax); //set maximum acceleration
  write43x(cs_pin, D_MAX_REGISTER,motors[motor_nr].dMax); //set maximum deceleration
  write43x(cs_pin, SH_D_MAX_REGISTER,motors[motor_nr].dMax); //set maximum deceleration
  write43x(cs_pin,BOW_1_REGISTER,motors[motor_nr].startBow);
  write43x(cs_pin,SH_BOW_1_REGISTER,motors[motor_nr].startBow);
  write43x(cs_pin,BOW_2_REGISTER,motors[motor_nr].endBow);
  write43x(cs_pin,SH_BOW_2_REGISTER,motors[motor_nr].endBow);
  write43x(cs_pin,BOW_3_REGISTER,motors[motor_nr].endBow);
  write43x(cs_pin,SH_BOW_3_REGISTER,motors[motor_nr].endBow);
  write43x(cs_pin,BOW_4_REGISTER,motors[motor_nr].startBow);
  write43x(cs_pin,SH_BOW_4_REGISTER,motors[motor_nr].startBow);

  return NULL;
}

const __FlashStringHelper* moveMotor(unsigned char motor_nr, unsigned long pos, unsigned long vMax, unsigned long aMax, unsigned long dMax) {
  unsigned char cs_pin = motors[motor_nr].cs_pin;
  if (dMax==0) {
    dMax = aMax;
  }
  write43x(cs_pin,V_MAX_REGISTER,vMax << 8); //set the velocity - TODO recalculate float numbers
  write43x(cs_pin,A_MAX_REGISTER,aMax); //set maximum acceleration
  write43x(cs_pin,D_MAX_REGISTER,dMax); //set maximum deceleration
  write43x(cs_pin,X_TARGET_REGISTER,pos);
  Serial.print("moving to ");
  Serial.println(pos);
  return NULL;
}





