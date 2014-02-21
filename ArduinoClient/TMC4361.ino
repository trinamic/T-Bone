volatile long next_pos_comp[nr_of_motors];
long last_target[nr_of_motors];

void prepareTMC4361() {
  pinModeFast(START_SIGNAL_PIN,INPUT);
  digitalWriteFast(START_SIGNAL_PIN,LOW);

  //initialize CS pin
  digitalWriteFast(CS_4361_1_PIN,LOW);
  pinModeFast(CS_4361_1_PIN,OUTPUT);
  digitalWriteFast(CS_4361_2_PIN,LOW);
  pinModeFast(CS_4361_2_PIN,OUTPUT);
  digitalWriteFast(CS_4361_3_PIN,LOW);
  pinModeFast(CS_4361_3_PIN,OUTPUT);

  //will be released after setup is complete   
  for (char i=0; i<nr_of_motors; i++) {
    pinModeFast(motors[i].target_reached_interrupt_pin,INPUT);
    digitalWriteFast(motors[i].target_reached_interrupt_pin,LOW);
  }
}

void initialzeTMC4361() {
  //reset the quirrel
  resetTMC4361(true,false);

  digitalWriteFast(START_SIGNAL_PIN,LOW);

  //initialize CS pin
  digitalWriteFast(CS_4361_1_PIN,LOW);
  digitalWriteFast(CS_4361_2_PIN,LOW);
  digitalWriteFast(CS_4361_3_PIN,LOW);

  //will be released after setup is complete   
  for (char i=0; i<nr_of_motors; i++) {
    digitalWriteFast(motors[i].target_reached_interrupt_pin,LOW);
  }
  //enable the reset again
  delay(1);
  resetTMC4361(false,true);
  delay(10);

  //initialize SPI
  SPI.begin();
  //SPI.setClockDivider(SPI_CLOCK_DIV4);
  //preconfigure the TMC4361
  for (char i=0; i<nr_of_motors;i++) {
    write4361(i, TMC4361_GENERAL_CONFIG_REGISTER, 0 | _BV(5)); //we use direct values
    write4361(i, TMC4361_RAMP_MODE_REGISTER,_BV(2) | 2); //we want to go to positions in nice S-Ramps)
    write4361(i, TMC4361_SH_RAMP_MODE_REGISTER,_BV(2) | 2); //we want to go to positions in nice S-Ramps)
    write4361(i,TMC4361_CLK_FREQ_REGISTER,CLOCK_FREQUENCY);
    write4361(i,TMC4361_START_DELAY_REGISTER, 256); //NEEDED so THAT THE SQUIRREL CAN RECOMPUTE EVERYTHING!
    //TODO shouldn't we add target_reached - just for good measure??
    setStepsPerRevolution(i,motors[i].steps_per_revolution);
    last_target[i]=0;
  }
}

const __FlashStringHelper* setStepsPerRevolution(unsigned char motor_nr, unsigned int steps) {
#ifdef DEBUG_MOTOR_CONTFIG
  Serial.print(F("Settings steps per rev for #"));
  Serial.print(motor_nr);
  Serial.print(F(" to "));
  Serial.println(steps);
#endif
  //configure the motor type
  unsigned long motorconfig = 0x00; //we want 256 microsteps
  motorconfig |= steps<<4;
  write4361(motor_nr,TMC4361_STEP_CONF_REGISTER,motorconfig);
  motors[motor_nr].steps_per_revolution = steps;
  return NULL;
}

const __FlashStringHelper* homeMotor(unsigned char motor_nr, unsigned long timeout, 
double homing_fast_speed, double homing_low_speed, long homing_retraction,
unsigned long homming_accel,
unsigned long homming_jerk)
{
  //todo shouldn't we check if there is a movement going??

  write4361(motor_nr, TMC4361_START_CONFIG_REGISTER, 0
    | _BV(10)//immediate start        
  //since we just start 
  );   
  write4361(motor_nr, TMC4361_A_MAX_REGISTER,homming_accel); //set maximum acceleration
  write4361(motor_nr, TMC4361_D_MAX_REGISTER,homming_accel); //set maximum deceleration
  write4361(motor_nr,TMC4361_BOW_1_REGISTER,homming_jerk);
  write4361(motor_nr,TMC4361_BOW_2_REGISTER,homming_jerk);
  write4361(motor_nr,TMC4361_BOW_3_REGISTER,homming_jerk);
  write4361(motor_nr,TMC4361_BOW_4_REGISTER,homming_jerk);

  //TODO obey the timeout!!
  unsigned char homed = 0; //this is used to track where at homing we are 
  long target = 0;
  while (homed!=0xff) { //we will never have 255 homing phases - but whith this we not have to think about it 
    if (homed==0 || homed==1) {
      double homing_speed=homing_fast_speed; 
      if (homed==1) {
        homing_speed = homing_low_speed;
      }  
      unsigned long status = read4361(motor_nr, TMC4361_STATUS_REGISTER,0);
      unsigned long events = read4361(motor_nr, TMC4361_EVENTS_REGISTER,0);
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
          Serial.print(events,HEX);
          Serial.print(F(" phase "));
          Serial.println(homed,DEC);
#endif
          target -= 1000l;
          write4361(motor_nr,TMC4361_V_MAX_REGISTER, FIXED_23_8_MAKE(homing_speed));
          write4361(motor_nr, TMC4361_X_TARGET_REGISTER,X_TARGET_IN_DIRECTION(motor_nr,target));
        }
      } 
      else {
        long go_back_to;
        if (homed==0) {
          long actual = X_TARGET_IN_DIRECTION(motor_nr,read4361(motor_nr, TMC4361_X_ACTUAL_REGISTER,0));
          go_back_to = actual + homing_retraction;
#ifdef DEBUG_HOMING
          Serial.print(F("home near "));
          Serial.print(actual);
          Serial.print(F(" - going back to "));
          Serial.println(go_back_to);
#endif
        } 
        else {
          long actual = read4361(motor_nr, TMC4361_X_LATCH_REGISTER,0);
          go_back_to = actual;
#ifdef DEBUG_HOMING
          Serial.println(F("homed at "));
          Serial.println(actual);
#endif
        }
        write4361(motor_nr,TMC4361_V_MAX_REGISTER, FIXED_23_8_MAKE(homing_fast_speed));
        write4361(motor_nr, TMC4361_X_TARGET_REGISTER, X_TARGET_IN_DIRECTION(motor_nr,go_back_to));
        delay(10ul);
        status = read4361(motor_nr, TMC4361_STATUS_REGISTER,0);
        while (!(status & _BV(0))) {
          status = read4361(motor_nr, TMC4361_STATUS_REGISTER,0);
        }
        if (homed==0) {
          homed = 1;
        } 
        else {
          write4361(motor_nr, TMC4361_X_ACTUAL_REGISTER,0);
          homed=0xff;
        }     
      } 
    } 
  }
  return NULL;
}

inline long getMotorPosition(unsigned char motor_nr) {
  return read4361(motor_nr, TMC4361_X_TARGET_REGISTER ,0);
  //TODO do we have to take into account that the motor may never have reached the x_target??
  //vactual!=0 -> x_target, x_pos sonst or similar
}

void moveMotor(unsigned char motor_nr, long target_pos, double vMax, double aMax, long jerk, boolean configure_shadow, boolean isWaypoint) {
  long aim_target;
  //calculate the value for x_target so taht we go over pos_comp
  long last_pos = last_target[motor_nr]; //this was our last position
  direction[motor_nr]=(target_pos)>last_pos? 1:-1;  //and for failsafe movement we need to write down the direction
  if (isWaypoint) {
    aim_target = target_pos+(target_pos-last_pos); // 2*(target_pos-last_pos)+last_pos;
  } 
  else {
    aim_target=target_pos;
  }

#ifdef DEBUG_MOTION  
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
  Serial.print(F(": jerk="));
  Serial.print(jerk);
  Serial.println();
#endif    

  long fixed_a_max = FIXED_22_2_MAKE(aMax);

  if (!configure_shadow) {
    write4361(motor_nr, TMC4361_V_MAX_REGISTER,FIXED_23_8_MAKE(vMax)); //set the velocity 
    write4361(motor_nr, TMC4361_A_MAX_REGISTER,fixed_a_max); //set maximum acceleration
    write4361(motor_nr, TMC4361_D_MAX_REGISTER,fixed_a_max); //set maximum deceleration
    write4361(motor_nr, TMC4361_BOW_1_REGISTER,jerk);
    write4361(motor_nr, TMC4361_BOW_2_REGISTER,jerk);
    write4361(motor_nr, TMC4361_BOW_3_REGISTER,jerk);
    write4361(motor_nr, TMC4361_BOW_4_REGISTER,jerk);
    //TODO pos comp is not shaddowwed
    next_pos_comp[motor_nr] = target_pos;
    write4361(motor_nr, TMC4361_POS_COMP_REGISTER,target_pos);

  } 
  else {
    write4361(motor_nr, TMC4361_SH_V_MAX_REGISTER,FIXED_23_8_MAKE(vMax)); //set the velocity 
    write4361(motor_nr, TMC4361_SH_A_MAX_REGISTER,fixed_a_max); //set maximum acceleration
    write4361(motor_nr, TMC4361_SH_D_MAX_REGISTER,fixed_a_max); //set maximum deceleration
    write4361(motor_nr, TMC4361_SH_BOW_1_REGISTER,jerk);
    write4361(motor_nr, TMC4361_SH_BOW_2_REGISTER,jerk);
    write4361(motor_nr, TMC4361_SH_BOW_3_REGISTER,jerk);
    write4361(motor_nr, TMC4361_SH_BOW_4_REGISTER,jerk);

    //TODO pos comp is not shaddowwed
    next_pos_comp[motor_nr] = target_pos;

  }
  write4361(motor_nr, TMC4361_X_TARGET_REGISTER,aim_target);
  last_target[motor_nr]=target_pos;
}

inline void signal_start() {
  //prepare the pos compr registers
  for (char i=0; i< nr_of_motors; i++) {
    //clear the event register
    read4361(i, TMC4361_EVENTS_REGISTER,0);
    write4361(i, TMC4361_POS_COMP_REGISTER,next_pos_comp[i]);
    if (target_motor_status & _BV(i)) {
      unsigned long motor_pos = read4361(i, TMC4361_X_ACTUAL_REGISTER,0);
      if ((direction[i]==1 && motor_pos>=next_pos_comp[i])
        || (direction[i]==-1 && motor_pos<=next_pos_comp[i])) {
        motor_target_reached(i);
#ifdef DEBUG_X_POS
        Serial.print('*');
#endif
      }
#ifdef DEBUG_X_POS
      Serial.println();
#endif
    }
    next_pos_comp[i] = 0;
  }    
  //carefully trigger the start pin 
  digitalWriteFast(START_SIGNAL_PIN,HIGH);
  pinModeFast(START_SIGNAL_PIN,OUTPUT);
  digitalWriteFast(START_SIGNAL_PIN,LOW);
  pinModeFast(START_SIGNAL_PIN,INPUT);
#ifdef DEBUG_MOTION_TRACE
  Serial.println(F("Sent start signal"));
#endif
#ifdef DEBUG_MOTION_TRACE
  Serial.println('-');
#endif
#ifdef DEBUG_X_POS
  Serial.println();
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
  unsigned long endstop_config = read4361(motor_nr, TMC4361_REFERENCE_CONFIG_REGISTER, 0);
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











































