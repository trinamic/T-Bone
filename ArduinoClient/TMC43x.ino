
void initialzeTMC43x() {


  //reset the quirrel
  pinMode(reset_squirrel,OUTPUT);
  digitalWrite(reset_squirrel, LOW);
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
    write43x(motors[i].cs_pin,CLK_FREQ_REGISTER,CLOCK_FREQUENCY);
    setStepsPerRevolution(motors[i].cs_pin,motors[i].steps_per_revolution);
  }
}

const __FlashStringHelper* setStepsPerRevolution(unsigned char motor_number, unsigned int steps) {
  //get the motor number
  unsigned char cs_pin = motors[motor_number].cs_pin;
  //configure the motor type
  unsigned long motorconfig = 0x00; //we want 256 microsteps
  motorconfig |= steps<<4;
  write43x(cs_pin,STEP_CONF_REGISTER,motorconfig);
  motors[motor_number].steps_per_revolution = steps;
  return NULL;
}

/*
const __FlashStringHelper* setRampBows(unsigned char motor_nr, long startbow, long endbow) {
  if (endbow==0) {
    endbow=startbow;
  }
  write43x(BOW_1_REGISTER,startbow);
  write43x(BOW_2_REGISTER,endbow);
  write43x(BOW_3_REGISTER,endbow);
  write43x(BOW_4_REGISTER,startbow);
  current_startbow=startbow;
  current_endbow=endbow;
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
*/

