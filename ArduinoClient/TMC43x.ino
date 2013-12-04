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
    write43x(motors[i].cs_pin,START_DELAY_REGISTER, 256); //NEEDED so THAT THE SQUIRREL CAN RECOMPUTE EVERYTHING!
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

inline long getMotorPosition(unsigned char motor_nr) {
  return read43x(motors[motor_nr].cs_pin, X_TARGET_REGISTER ,0);
  //TODO do we have to take into account that the motor may never have reached the x_target??
  //vactual!=0 -> x_target, x_pos sonst or similar
}

void moveMotor(unsigned char motor_nr, long pos, double vMax, long aMax, long dMax, long startBow, long endBow, boolean configure_shadow) {
  unsigned char cs_pin = motors[motor_nr].cs_pin;

  if (pos==0) {
    //We avoid 0 since it is a marker 
    //TODO silly hack!
    pos=1;
  }


  //calculate the value for x_target so taht we go over pos_comp
  long last_pos = getMotorPosition(motor_nr);
  long aim_target = 2*(pos-last_pos)+last_pos;


#ifdef DEBUG_MOTOR_CONTFIG  
  if (!configure_shadow) {
    Serial.print(F("Moving motor "));
  } 
  else {
    Serial.print(F("Preparing motor "));
  }
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
    write43x(cs_pin,POS_COMP_REGISTER,pos);

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
  //prepare the pos compr registers
  for (char i=0; i< nr_of_motors; i++) {
    read43x(motors[i].cs_pin,EVENTS_REGISTER,0);
    if (next_pos_comp[i]!=0) {
      write43x(motors[i].cs_pin,POS_COMP_REGISTER,next_pos_comp[i]);
      next_pos_comp[i] = 0;
    }
  }    
  //carefully trigger the start pin 
  digitalWrite(start_signal_pin,HIGH);
  pinMode(start_signal_pin,OUTPUT);
  digitalWrite(start_signal_pin,LOW);
  pinMode(start_signal_pin,INPUT);
#ifdef DEBUG_MOTION_TRACE
  Serial.println(F("Sent start signal"));
#endif
}









