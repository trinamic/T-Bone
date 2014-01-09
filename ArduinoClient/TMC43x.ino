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

const __FlashStringHelper* homeMotor(unsigned char motor_nr, unsigned long timeout, 
double homing_fast_speed, double homing_low_speed, 
unsigned long homming_accel, unsigned long homing_deccel,
unsigned long homming_start_bow, unsigned long homing_end_bow)
{
  unsigned char cs_pin = motors[motor_nr].cs_pin;
  //todo shouldn't we check if there is a movement going??

  write43x(cs_pin, A_MAX_REGISTER,homming_accel); //set maximum acceleration
  write43x(cs_pin, D_MAX_REGISTER,homing_deccel); //set maximum deceleration
  write43x(cs_pin,BOW_1_REGISTER,homming_start_bow);
  write43x(cs_pin,BOW_2_REGISTER,homing_end_bow);
  write43x(cs_pin,BOW_3_REGISTER,homing_end_bow);
  write43x(cs_pin,BOW_4_REGISTER,homming_start_bow);
  write43x(cs_pin, START_CONFIG_REGISTER, 0
    | _BV(10)//immediate start        
  //since we just start 
  );   

  //TODO obey the timeout!!
  unsigned char homed = 0; //this is used to track where at homing we are 
  long target = 0;
  while (homed!=0xff) { //we will never have 255 homing phases - but whith this we not have to think about it 
    if (homed==0 || homed==1) {
      double homing_speed=homing_fast_speed; 
      if (homed==2) {
        homing_speed /= homing_low_speed;
      }  
      unsigned long status = read43x(cs_pin, STATUS_REGISTER,0);
      unsigned long events = read43x(cs_pin, EVENTS_REGISTER,0);
      if (!(status & (_BV(7) | _BV(9)))) {
        if ((status & (_BV(0) | _BV(6))) || (!(status && (_BV(4) | _BV(3))))) {
#ifdef DEBUG_HOMING
          Serial.print(F("Homing to "));
          Serial.print(target);
          Serial.print(F(" @ "));
          Serial.println(homing_speed);
          Serial.print(F("Status "));
          Serial.println(status,HEX);
          Serial.print(F("Events "));
          Serial.println(events,HEX);
#endif
          target -= 1000l;
          write43x(cs_pin,V_MAX_REGISTER, FIXED_24_8_MAKE(homing_speed));
          write43x(cs_pin, X_TARGET_REGISTER,target);
        }
      } 
      else {
        long go_back_to;
        if (homed==0) {
          long actual = read43x(cs_pin, X_ACTUAL_REGISTER,0);
          go_back_to = target + 100000ul; //TODO configure me!
#ifdef DEBUG_HOMING
          Serial.print(F("home near "));
          Serial.print(actual);
          Serial.print(F(" - going back to "));
          Serial.println(go_back_to);
#endif
        } 
        else {
          long actual = read43x(cs_pin, X_LATCH_REGISTER,0);
          go_back_to = actual;
#ifdef DEBUG_HOMING
          Serial.println(F("homed at "));
          Serial.println(actual);
#endif
        }
        write43x(cs_pin,V_MAX_REGISTER, FIXED_24_8_MAKE(homing_fast_speed));
        write43x(cs_pin, X_TARGET_REGISTER,go_back_to);
        delay(10ul);
        status = read43x(cs_pin, STATUS_REGISTER,0);
        while (!(status & _BV(0))) {
          status = read43x(cs_pin, STATUS_REGISTER,0);
        }
        if (homed==0) {

          homed = 1;
        } 
        else {
          write43x(cs_pin, X_ACTUAL_REGISTER,0);
          homed=0xff;
        }     
      } 
    } 
  }
  return NULL;
}

inline long getMotorPosition(unsigned char motor_nr) {
  return read43x(motors[motor_nr].cs_pin, X_TARGET_REGISTER ,0);
  //TODO do we have to take into account that the motor may never have reached the x_target??
  //vactual!=0 -> x_target, x_pos sonst or similar
}

void moveMotor(unsigned char motor_nr, long pos, double vMax, long aMax, long dMax, long startBow, long endBow, boolean configure_shadow, boolean isWaypoint) {
  unsigned char cs_pin = motors[motor_nr].cs_pin;

  if (pos==0) {
    //We avoid 0 since it is a marker 
    //TODO silly hack!
    pos=1;
  }


  long aim_target;
  long comp_pos;
  //calculate the value for x_target so taht we go over pos_comp
  if (isWaypoint) {
    long last_pos = last_target[motor_nr];
    comp_pos = pos;
    aim_target = 2*(pos-last_pos)+last_pos;
  } 
  else {
    comp_pos = 0;
    aim_target=pos;
  }

#ifdef DEBUG_MOTION_TRACE  
  if (!configure_shadow) {
    Serial.print(F("Moving motor "));
  } 
  else {
    Serial.print(F("Preparing motor "));
  }
  Serial.print(motor_nr,DEC);
  if (isWaypoint) {
    Serial.print(" via");
  } 
  else {
    Serial.print(" to");
  }
  Serial.print(F(": pos="));
  Serial.print(pos);
  Serial.print(F(" ["));
  Serial.print(last_target[motor_nr]);
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
    next_pos_comp[motor_nr] = comp_pos;
    write43x(cs_pin,POS_COMP_REGISTER,comp_pos);

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
    next_pos_comp[motor_nr] = comp_pos;

  }
  write43x(cs_pin,X_TARGET_REGISTER,aim_target);
}

inline void signal_start() {
  //prepare the pos compr registers
  for (char i=0; i< nr_of_motors; i++) {

    //clear the event register
    read43x(motors[i].cs_pin,EVENTS_REGISTER,0);
    write43x(motors[i].cs_pin,POS_COMP_REGISTER,next_pos_comp[i]);
    next_pos_comp[i] = 0;
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

const __FlashStringHelper* configureEndstop(unsigned char motor_nr, boolean left, boolean active_high) {
  unsigned long endstop_config = getClearedEndStopConfig(motor_nr, left);
  unsigned long clearing_pattern;
  if (left) {
    if (active_high) {
  #ifdef DEBUG_ENDSTOPS
    Serial.println(F("Configuring left end stop as active high"));
  #endif
      clearing_pattern |= 0 
        | _BV(0) //STOP_LEFT enable
        | _BV(2) //positive Stop Left stops motor
          | _BV(11) //X_LATCH if stopl becomes active ..
          ;
          } 
    else {
  #ifdef DEBUG_ENDSTOPS
    Serial.println(F("Configuring left end stop as active low"));
  #endif
      clearing_pattern |= 0 
        | _BV(0) //STOP_LEFT enable
        | _BV(10) //X_LATCH if stopl becomes inactive ..
        ;
        }
      } 
  else {
    if (active_high) {
  #ifdef DEBUG_ENDSTOPS
    Serial.println(F("Configuring right end stop as active high"));
  #endif
      clearing_pattern |= 0 
        | _BV(1) //STOP_RIGHT enable
        | _BV(3) //positive Stop right stops motor
          | _BV(13) //X_LATCH if storl becomes active ..
          ;
          } 
    else {
  #ifdef DEBUG_ENDSTOPS
    Serial.println(F("Configuring right end stop as active low"));
  #endif
      clearing_pattern |= 0 
        | _BV(0) //STOP_LEFT enable
        | _BV(12) //X_LATCH if stopr becomes inactive ..
        ;
        }
      }
      return NULL;
}


const __FlashStringHelper* configureVirtualEndstop(unsigned char motor_nr, boolean left, long positions) {
  return NULL;
}

inline unsigned long getClearedEndStopConfig(unsigned char motor_nr, boolean left) {
  unsigned long endstop_config = read43x(motors[motor_nr].cs_pin, REFERENCE_CONFIG_REGISTER, 0);
  //clear everything
  unsigned long clearing_pattern; // - a trick to ensure the use of all 32 bits
  if (left) {
    clearing_pattern = LEFT_ENDSTOP_REGISTER_PATTERN;
  } 
  else {
    clearing_pattern = RIGHT_ENDSTOP_REGISTER_PATTERN;
  }
  clearing_pattern = ~clearing_pattern;
  endstop_config |= clearing_pattern;
  return endstop_config;
}  

























