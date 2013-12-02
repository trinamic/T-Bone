volatile long next_pos_comp[nr_of_motors];

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
    attachInterrupt(motors[i].target_reached_interrupt_nr,motors[i].target_reached_interrupt_routine, FALLING);
  }
  //enable the reset again
  delay(1);
  digitalWrite(reset_squirrel, HIGH);
  delay(10);
  //initialize SPI
  SPI.begin();
  //preconfigure the TMC43x
  for (char i=0; i<nr_of_motors;i++) {
    write43x(motors[i].cs_pin, GENERAL_CONFIG_REGISTER, 0); //we use direct values
    write43x(motors[i].cs_pin, RAMP_MODE_REGISTER,_BV(2) | 2); //we want to go to positions in nice S-Ramps ()TDODO does not work)
    write43x(motors[i].cs_pin, SH_RAMP_MODE_REGISTER,_BV(2) | 2); //we want to go to positions in nice S-Ramps ()TDODO does not work)
    write43x(motors[i].cs_pin,CLK_FREQ_REGISTER,CLOCK_FREQUENCY);
    write43x(motors[i].cs_pin, INTERRUPT_CONFIG_REGISTER,_BV(1)); //POS_COMP_REACHED is our target reached
    //TODO shouldn't we add target_reached - just for good measure??
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
  motors[motor_nr].dMax = dMax;
  motors[motor_nr].startBow = startbow;
  motors[motor_nr].endBow = endbow!=0;

  if (endbow==0) {
    endbow=startbow;
  }

  return NULL;
}

inline long getMotorPosition(unsigned char motor_nr) {
  return read43x(motors[motor_nr].cs_pin, X_TARGET_REGISTER ,0);
  //TODO do we have to take into account that the motor may never have reached the x_target??
  //vactual!=0 -> x_target, x_pos sonst or similar
}

void moveMotor(unsigned char motor_nr, long pos, double vMax, double factor, boolean configure_shadow) {
  unsigned char cs_pin = motors[motor_nr].cs_pin;

  long aMax = motors[motor_nr].aMax;
  long dMax = motors[motor_nr].dMax;
  long startBow = motors[motor_nr].startBow;
  long endBow = motors[motor_nr].endBow;

  //TODO can't start commands be part of this movement ??

  if (factor==0) {
    vMax = 0;
    aMax = 0;
    dMax = 0;
    startBow = 0;
    endBow = 0;
  } 
  else if (factor!=1.0) {
    vMax = factor*vMax;
    aMax = aMax * factor;
    dMax = (dMax!=0)? dMax * factor: aMax;
    startBow = startBow * factor;
    endBow = (endBow!=0)? endBow * factor: startBow;
  } 
  else {
    dMax = (dMax!=0)? dMax: aMax;
    endBow = (endBow!=0)? endBow: startBow;
  }

  //calculate the value for x_target so taht we go over pos_comp
  long last_pos = getMotorPosition(motor_nr);
  long aim_target = 2*(pos-last_pos)+last_pos;


#ifdef DEBUG_MOTOR_CONTFIG    
  Serial.print(F("Motor "));
  Serial.print(motor_nr,DEC);
  Serial.print(F(": pos="));
  Serial.print(pos);
  Serial.print(F(" ["));
  Serial.print(last_pos);
  Serial.print(F(" -> "));
  Serial.print(aim_target);
  Serial.print(F("], vMax="));
  Serial.print(vMax);
  Serial.print(F(", aMax="));
  Serial.print(aMax);
  Serial.print(F(", dMax="));
  Serial.print(dMax);
  Serial.print(F(": startBow="));
  Serial.print(startBow);
  Serial.print(F(", endBow="));
  Serial.print(endBow);
  Serial.println();
#endif    

  if (!configure_shadow) {
    write43x(cs_pin,V_MAX_REGISTER,FIXED_24_8_MAKE(vMax)); //set the velocity 
    write43x(cs_pin, A_MAX_REGISTER,aMax); //set maximum acceleration
    write43x(cs_pin, D_MAX_REGISTER,dMax); //set maximum deceleration
    write43x(cs_pin,BOW_1_REGISTER,startBow);
    write43x(cs_pin,BOW_2_REGISTER,endBow);
    write43x(cs_pin,BOW_3_REGISTER,endBow);
    write43x(cs_pin,BOW_4_REGISTER,startBow);
    //TODO pos comp is not shaddowwed
    next_pos_comp[motor_nr] = 0;
    write43x(cs_pin,POS_COMP_REGISTER,startBow);

  } 
  else {
    write43x(cs_pin,SH_V_MAX_REGISTER,FIXED_24_8_MAKE(vMax)); //set the velocity 
    write43x(cs_pin, SH_A_MAX_REGISTER,aMax); //set maximum acceleration
    write43x(cs_pin, SH_D_MAX_REGISTER,dMax); //set maximum deceleration
    write43x(cs_pin,SH_BOW_1_REGISTER,startBow);
    write43x(cs_pin,SH_BOW_2_REGISTER,endBow);
    write43x(cs_pin,SH_BOW_3_REGISTER,endBow);
    write43x(cs_pin,SH_BOW_4_REGISTER,startBow);

    //TODO pos comp is not shaddowwed
    next_pos_comp[motor_nr] = pos;

  }
  write43x(cs_pin,X_TARGET_REGISTER,pos);
}


inline void signal_start() {
  //carefully trigger the start pin 
  digitalWrite(start_signal_pin,HIGH);
  pinMode(start_signal_pin,OUTPUT);
  digitalWrite(start_signal_pin,LOW);
  pinMode(start_signal_pin,INPUT);
}







