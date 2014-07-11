volatile long pos_comp[nr_of_coordinated_motors];
volatile long next_pos_comp[nr_of_coordinated_motors];
long last_target[nr_of_coordinated_motors];
const unsigned long default_4361_start_config = 0
| _BV(0) //x_target requires start
| _BV(4)  //use shaddow motion profiles
| _BV(5) //external start is an start
;

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
  for (char i=0; i<nr_of_coordinated_motors; i++) {
    pinModeFast(motors[i].target_reached_interrupt_pin,INPUT);
    digitalWriteFast(motors[i].target_reached_interrupt_pin,LOW);
    writeRegister(i, TMC4361_START_CONFIG_REGISTER, 0
      | _BV(5) //external start is an start (to ensure start is an input)
    );   
  }

  pinModeFast(START_SIGNAL_PIN,OUTPUT);
  digitalWriteFast(START_SIGNAL_PIN,HIGH);

}

void initialzeTMC4361() {
  //reset the quirrel
  resetTMC4361(true,false);

  digitalWriteFast(START_SIGNAL_PIN,HIGH);

  //initialize CS pin
  digitalWriteFast(CS_4361_1_PIN,LOW);
  digitalWriteFast(CS_4361_2_PIN,LOW);
  digitalWriteFast(CS_4361_3_PIN,LOW);

  //will be released after setup is complete   
  for (char i=0; i<nr_of_coordinated_motors; i++) {
    digitalWriteFast(motors[i].target_reached_interrupt_pin,LOW);
  }
  //enable the reset again
  delay(1);
  resetTMC4361(false,true);
  delay(10);

  //preconfigure the TMC4361
  for (char i=0; i<nr_of_coordinated_motors;i++) {
    writeRegister(i, TMC4361_GENERAL_CONFIG_REGISTER, 0 | _BV(5)); //we don't use direct values
    writeRegister(i, TMC4361_RAMP_MODE_REGISTER,_BV(2) | 2); //we want to go to positions in nice S-Ramps)
    writeRegister(i, TMC4361_SH_RAMP_MODE_REGISTER,_BV(2) | 2); //we want to go to positions in nice S-Ramps)
    writeRegister(i,TMC4361_CLK_FREQ_REGISTER,CLOCK_FREQUENCY);
    writeRegister(i,TMC4361_START_DELAY_REGISTER, 512); //NEEDED so THAT THE SQUIRREL CAN RECOMPUTE EVERYTHING!
    //TODO shouldn't we add target_reached - just for good measure??
    setStepsPerRevolutionTMC4361(i,motors[i].steps_per_revolution);
    writeRegister(i, TMC4361_START_CONFIG_REGISTER, default_4361_start_config);   
    unsigned long filter = (0x20000ul /*2<<16*/) | (0x400000ul /*4<<20*/); //filter start
    filter |= (2<<8) | (0x4000ul /*4<<20*/); //filter ref
    Serial.println(filter);
    writeRegister(i,TMC4361_INPUT_FILTER_REGISTER,filter);

    last_target[i]=0;
  }
}

const __FlashStringHelper* setStepsPerRevolutionTMC4361(unsigned char motor_nr, unsigned int steps) {
#ifdef DEBUG_MOTOR_CONTFIG
  Serial.print(F("Settings steps per rev for #"));
  Serial.print(motor_nr);
  Serial.print(F(" to "));
  Serial.println(steps);
#endif
  //configure the motor type
  unsigned long motorconfig = 0x00; //we want 256 microsteps
  motorconfig |= steps<<4;
  writeRegister(motor_nr,TMC4361_STEP_CONF_REGISTER,motorconfig);
  motors[motor_nr].steps_per_revolution = steps;
  return NULL;
}

const __FlashStringHelper* homeMotorTMC4361(unsigned char motor_nr, unsigned long timeout, 
double homing_fast_speed, double homing_low_speed, long homing_retraction,
double homming_accel,
unsigned long homming_jerk, 
unsigned long right_homing_point)
{
  const boolean homing_right = (right_homing_point!=0);
  //todo shouldn't we check if there is a movement going??
#ifdef DEBUG_HOMING
  Serial.print(F("Homing for TMC4361 motor "));
  Serial.print(motor_nr,DEC);
  if (homing_right) {
    Serial.print(F(" right to pos="));
    Serial.print(right_homing_point);
  }
  Serial.print(F(", timeout="));
  Serial.print(timeout);
  Serial.print(F(", fast="));
  Serial.print(homing_fast_speed);
  Serial.print(F(", slow="));
  Serial.print(homing_low_speed);
  Serial.print(F(", retract="));
  Serial.print(homing_retraction);
  Serial.print(F(", aMax="));
  Serial.print(homming_accel);
  Serial.print(F(": jerk="));
  Serial.print(homming_jerk);
  Serial.println();
#endif

  //comfigure homing movement
  writeRegister(motor_nr, TMC4361_START_CONFIG_REGISTER, 0
    | _BV(10)//immediate start        
  //since we just start 
  );   

  long fixed_a_max = FIXED_22_2_MAKE(homming_accel);

  //comfigire homing movement config - acceleration & jerk
  writeRegister(motor_nr, TMC4361_A_MAX_REGISTER,fixed_a_max); //set maximum acceleration
  writeRegister(motor_nr, TMC4361_D_MAX_REGISTER,fixed_a_max); //set maximum deceleration
  writeRegister(motor_nr,TMC4361_BOW_1_REGISTER,homming_jerk);
  writeRegister(motor_nr,TMC4361_BOW_2_REGISTER,homming_jerk);
  writeRegister(motor_nr,TMC4361_BOW_3_REGISTER,homming_jerk);
  writeRegister(motor_nr,TMC4361_BOW_4_REGISTER,homming_jerk);

  //TODO obey the timeout!!
  long start = millis();
  long last_wait_time = start;

  unsigned char homed = 0; //this is used to track where at homing we are 
  long target = 0;
#ifdef DEBUG_HOMING_STATUS
  unsigned long old_status = -1;
#endif
  while (homed!=0xff) { //we will never have 255 homing phases - but whith this we not have to think about it 
    status_wait_ping(&last_wait_time, homed);
    if (homed==0 || homed==1) {
      double homing_speed=homing_fast_speed; 
      if (homed==1) {
        homing_speed = homing_low_speed;
      }  
      unsigned long status = readRegister(motor_nr, TMC4361_STATUS_REGISTER);
      unsigned long events = readRegister(motor_nr, TMC4361_EVENTS_REGISTER);
      unsigned long end_stop_mask;
      if (homing_right) {
        end_stop_mask = _BV(8) | _BV(10);
      } 
      else {
        end_stop_mask = _BV(7) | _BV(9);
      }
#ifdef DEBUG_HOMING_STATUS
      if (status!=old_status) {
        Serial.print(F("Status1 "));
        Serial.println(status,HEX);
        Serial.print(F("Position "));
        Serial.print((long)readRegister(TMC5041_MOTORS, TMC5041_X_ACTUAL_REGISTER_1));
        Serial.print(F(", Targe "));
        Serial.println((long)readRegister(TMC5041_MOTORS, TMC5041_X_TARGET_REGISTER_1));
        Serial.print(F(", Velocity "));
        Serial.println((long)readRegister(TMC5041_MOTORS, TMC5041_V_ACTUAL_REGISTER_1));
        Serial.println(status & end_stop_mask,HEX);
        old_status=status;
      }
#endif
      if (!(status & end_stop_mask)) { //stopl not active
        if ((status & (_BV(0) | _BV(6))) || (!(status && (_BV(4) | _BV(3))))) { //at target or not moving
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
          if (homing_right) {
            target += 100000l;
          } 
          else {
            target -=100000l;
          }
          writeRegister(motor_nr,TMC4361_V_MAX_REGISTER, FIXED_23_8_MAKE(homing_speed));
          writeRegister(motor_nr, TMC4361_X_TARGET_REGISTER,X_TARGET_IN_DIRECTION(motor_nr,target));
        }
      } 
      else {
        long go_back_to;
        if (homed==0) {
          long actual = X_TARGET_IN_DIRECTION(motor_nr,readRegister(motor_nr, TMC4361_X_ACTUAL_REGISTER));
          if (homing_right) {
            go_back_to = actual - homing_retraction;
            if (go_back_to<=0) {
              writeRegister(motor_nr, TMC4361_X_TARGET_REGISTER,homing_retraction*2);
              writeRegister(motor_nr, TMC4361_X_ACTUAL_REGISTER,homing_retraction*2);
              go_back_to = homing_retraction;
            }
          } 
          else {
            go_back_to = actual + homing_retraction;
          } 
#ifdef DEBUG_HOMING
          Serial.print(F("home near "));
          Serial.print(actual);
          Serial.print(F(" - going back to "));
          Serial.println(go_back_to);
          Serial.print(F("Status "));
          Serial.println(status,HEX);
#endif
        } 
        else {
          long actual = X_TARGET_IN_DIRECTION(motor_nr,readRegister(motor_nr, TMC4361_X_LATCH_REGISTER));
          go_back_to = actual;
#ifdef DEBUG_HOMING
          Serial.print(F("homed at "));
          Serial.println(actual);
#endif
        }
        writeRegister(motor_nr,TMC4361_V_MAX_REGISTER, FIXED_23_8_MAKE(homing_fast_speed));
        writeRegister(motor_nr, TMC4361_X_TARGET_REGISTER, X_TARGET_IN_DIRECTION(motor_nr,go_back_to));
        delay(10ul);
        status = readRegister(motor_nr, TMC4361_STATUS_REGISTER);
        while (!(status & _BV(0))) {
#ifdef DEBUG_HOMING_STATUS
          if (status!=old_status) {
            Serial.print(F("Status2 "));
            Serial.println(status,HEX);
            Serial.print(F("Position "));
            Serial.print((long)readRegister(TMC5041_MOTORS, TMC5041_X_ACTUAL_REGISTER_1));
            Serial.print(F(", Targe "));
            Serial.println((long)readRegister(TMC5041_MOTORS, TMC5041_X_TARGET_REGISTER_1));
            Serial.print(F(", Velocity "));
            Serial.println((long)readRegister(TMC5041_MOTORS, TMC5041_V_ACTUAL_REGISTER_1));
            Serial.println(status & end_stop_mask,HEX);
            old_status=status;
          }
#endif
          status = readRegister(motor_nr, TMC4361_STATUS_REGISTER);
          status_wait_ping(&last_wait_time, homed);
        }
        if (homed==0) {
          homed = 1;
        } 
        else {
          if (homing_right) {
            writeRegister(motor_nr, TMC4361_X_TARGET_REGISTER,right_homing_point);
            writeRegister(motor_nr, TMC4361_X_ACTUAL_REGISTER,right_homing_point);
            //go back to zero
            writeRegister(motor_nr, TMC4361_X_TARGET_REGISTER, 0);
            delay(10ul);
            status = readRegister(motor_nr, TMC4361_STATUS_REGISTER);
            while (!(status & _BV(0))) {
              status_wait_ping(&last_wait_time, homed);
              status = readRegister(motor_nr, TMC4361_STATUS_REGISTER);
            } 
          } 
          else {
            writeRegister(motor_nr, TMC4361_X_ACTUAL_REGISTER,0);
            writeRegister(motor_nr, TMC4361_X_TARGET_REGISTER,0);
          }
          last_target[motor_nr]=0;
          homed=0xff;
        }     
      } 
    } 
  }

  //reset everything to 0
  writeRegister(motor_nr, TMC4361_V_MAX_REGISTER,0); 
  writeRegister(motor_nr, TMC4361_A_MAX_REGISTER,0); 
  writeRegister(motor_nr, TMC4361_D_MAX_REGISTER,0); 
  writeRegister(motor_nr,TMC4361_BOW_1_REGISTER,0);
  writeRegister(motor_nr,TMC4361_BOW_2_REGISTER,0);
  writeRegister(motor_nr,TMC4361_BOW_3_REGISTER,0);
  writeRegister(motor_nr,TMC4361_BOW_4_REGISTER,0);
  writeRegister(motor_nr, TMC4361_START_CONFIG_REGISTER, default_4361_start_config);   

  return NULL;
}

inline long getMotorPositionTMC4361(unsigned char motor_nr) {
  return readRegister(motor_nr, TMC4361_X_TARGET_REGISTER);
  //TODO do we have to take into account that the motor may never have reached the x_target??
  //vactual!=0 -> x_target, x_pos sonst or similar
}

void moveMotorTMC4361(unsigned char motor_nr, long target_pos, double vMax, double aMax, long jerk, boolean isWaypoint) {
  long aim_target;
  //calculate the value for x_target so taht we go over pos_comp
  long last_pos = last_target[motor_nr]; //this was our last position
  next_direction[motor_nr]=(target_pos)>last_pos? 1:-1;  //and for failsafe movement we need to write down the direction
  if (isWaypoint) {
    aim_target = target_pos+(target_pos-last_pos); // 2*(target_pos-last_pos)+last_pos;
  } 
  else {
    aim_target=target_pos;
  }

#ifdef DEBUG_MOTION  
  Serial.print(F("Preparing motor "));
  Serial.print(motor_nr,DEC);
  if (isWaypoint) {
    Serial.print(" via");
  } 
  else {
    Serial.print(" to");
  }
  Serial.print(F(": pos="));
  Serial.print(target_pos);
  Serial.print(F(" ["));
  Serial.print(last_target[motor_nr]);
  Serial.print(F(" -> "));
  Serial.print(target_pos);
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
#ifdef DEBUG_MOTION_SHORT
  Serial.print('M');
  Serial.print(motor_nr,DEC);
  if (isWaypoint) {
    Serial.print(F(" v "));
    Serial.print(target_pos);
  }  
  Serial.print(F(" t "));
  Serial.print(aim_target);
  Serial.print(F(" @ "));
  Serial.println(vMax);
#endif

  long fixed_a_max = FIXED_22_2_MAKE(aMax);

  writeRegister(motor_nr, TMC4361_SH_V_MAX_REGISTER,FIXED_23_8_MAKE(vMax)); //set the velocity 
  writeRegister(motor_nr, TMC4361_SH_A_MAX_REGISTER,fixed_a_max); //set maximum acceleration
  writeRegister(motor_nr, TMC4361_SH_D_MAX_REGISTER,fixed_a_max); //set maximum deceleration
  writeRegister(motor_nr, TMC4361_SH_BOW_1_REGISTER,jerk);
  writeRegister(motor_nr, TMC4361_SH_BOW_2_REGISTER,jerk);
  writeRegister(motor_nr, TMC4361_SH_BOW_3_REGISTER,jerk);
  writeRegister(motor_nr, TMC4361_SH_BOW_4_REGISTER,jerk);
  //pos comp is not shaddowed
  next_pos_comp[motor_nr] = target_pos;
  writeRegister(motor_nr, TMC4361_X_TARGET_REGISTER,aim_target);

  last_target[motor_nr]=target_pos;
  /*
//Might be usefull, but takes a lot of space
   #ifdef DEBUG_MOTION_REGISTERS
   Serial.println('S');
   Serial.println(motor_nr,DEC);
   Serial.println((long)readRegister(motor_nr,TMC4361_X_TARGET_REGISTER));
   Serial.println(readRegister(motor_nr,TMC4361_SH_V_MAX_REGISTER));
   Serial.println(readRegister(motor_nr,TMC4361_SH_A_MAX_REGISTER));
   Serial.println(readRegister(motor_nr,TMC4361_SH_D_MAX_REGISTER));
   Serial.println(readRegister(motor_nr,TMC4361_SH_BOW_1_REGISTER));
   Serial.println(readRegister(motor_nr,TMC4361_SH_BOW_2_REGISTER));
   Serial.println(readRegister(motor_nr,TMC4361_SH_BOW_3_REGISTER));
   Serial.println(readRegister(motor_nr,TMC4361_SH_BOW_4_REGISTER));
   Serial.println();
   #endif
   */
}

inline void signal_start() {
#ifdef RX_TX_BLINKY
  RXLED1;
  TXLED1;
#endif
  //prepare the pos compr registers
  for (char i=0; i< nr_of_coordinated_motors; i++) {
    //clear the event register
    readRegister(i, TMC4361_EVENTS_REGISTER);
    pos_comp[i] = next_pos_comp[i];
    //write the new pos_comp
    writeRegister(i, TMC4361_POSITION_COMPARE_REGISTER,next_pos_comp[i]);
    //and mark it written 
    next_pos_comp[i] = 0;
    direction[i] = next_direction[i];
  }
  //start the motors, please
  digitalWriteFast(START_SIGNAL_PIN,LOW);
  delayMicroseconds(3); //the call by itself may have been enough
  digitalWriteFast(START_SIGNAL_PIN,HIGH);

#ifdef DEBUG_MOTION_REGISTERS
  //and deliver some additional logging
  for (char i=0; i< nr_of_coordinated_motors; i++) {
    Serial.println('R');
    Serial.println(i,DEC);
    Serial.println((long)readRegister(i,TMC4361_X_ACTUAL_REGISTER));
    Serial.println((long)readRegister(i,TMC4361_X_TARGET_REGISTER));
    Serial.println((long)readRegister(i,TMC4361_POSITION_COMPARE_REGISTER));
    Serial.println(readRegister(i,TMC4361_V_MAX_REGISTER));
    Serial.println(readRegister(i,TMC4361_A_MAX_REGISTER));
    Serial.println(readRegister(i,TMC4361_D_MAX_REGISTER));
    Serial.println(readRegister(i,TMC4361_BOW_1_REGISTER));
    Serial.println(readRegister(i,TMC4361_BOW_2_REGISTER));
    Serial.println(readRegister(i,TMC4361_BOW_3_REGISTER));
    Serial.println(readRegister(i,TMC4361_BOW_4_REGISTER));
    Serial.println();
  }
#endif
#ifdef DEBUG_MOTION_TRACE
  Serial.println(F("Sent start signal"));
#endif
#ifdef DEBUG_MOTION_START
  Serial.println('S');
#endif
#ifdef DEBUG_MOTION_REGISTERS
  Serial.println();
#endif
#ifdef RX_TX_BLINKY
  RXLED0;
  TXLED0;
#endif
}

//this method is needed to manually verify that the motors are to running beyond their targe
void checkTMC4361Motion() {
  if (target_motor_status & (_BV(nr_of_coordinated_motors)-1)) {
    //now check for every motor if iis alreaday ove the target..
    for (unsigned char i=0; i< nr_of_coordinated_motors; i++) {
      //and deliver some additional logging
      if (target_motor_status & _BV(i) & ~motor_status) {
#ifdef RX_TX_BLINKY
        RXLED1;
#endif
        unsigned long motor_pos = readRegister(i, TMC4361_X_ACTUAL_REGISTER);
        if ((direction[i]==1 && motor_pos>=pos_comp[i])
          || (direction[i]==-1 && motor_pos<=pos_comp[i])) {
#ifdef DEBUG_MOTION_TRACE_SHORT
          Serial.print('*');
#endif
          motor_target_reached(i);
        }
#ifdef RX_TX_BLINKY
        RXLED0;
#endif
      }
    }
  }
}  

void setMotorPositionTMC4361(unsigned char motor_nr, long position) {
#ifdef DEBUG_MOTION_SHORT
  Serial.print('M');
  Serial.print(motor_nr);
  Serial.println(F(":=0"));
#endif
  unsigned long oldStartRegister = readRegister(motor_nr, TMC4361_START_CONFIG_REGISTER);
  //we want an immediate start
  writeRegister(motor_nr, TMC4361_START_CONFIG_REGISTER, 0);  

  //we write x_actual, x_target and pos_comp to the same value to be safe 
  writeRegister(motor_nr, TMC4361_V_MAX_REGISTER,0);
  writeRegister(motor_nr, TMC4361_X_TARGET_REGISTER,position);
  writeRegister(motor_nr, TMC4361_X_ACTUAL_REGISTER,position);
  writeRegister(motor_nr, TMC4361_POSITION_COMPARE_REGISTER,position);
  last_target[motor_nr]=position;

  //back to normal
  writeRegister(motor_nr, TMC4361_START_CONFIG_REGISTER, oldStartRegister);  
}

const __FlashStringHelper* configureEndstopTMC4361(unsigned char motor_nr, boolean left, boolean active_high) {
#ifdef DEBUG_ENDSTOPS_DETAIL
  Serial.print(F("Enstop config before "));
  Serial.println( readRegister(motor_nr, TMC4361_REFERENCE_CONFIG_REGISTER),HEX);
#endif
  unsigned long endstop_config = getClearedEndStopConfigTMC4361(motor_nr, left);
#ifdef DEBUG_ENDSTOPS_DETAIL
  Serial.print(F("Cleared enstop config before "));
  Serial.println(endstop_config, HEX);
#endif
  if (left) {
    if (active_high) {
#ifdef DEBUG_ENDSTOPS
      Serial.print(F("TMC4361 motor "));
      Serial.print(motor_nr);
      Serial.println(F(" - configuring left end stop as active high"));
#endif
      endstop_config |= 0 
        | _BV(0) //STOP_LEFT enable
        | _BV(2) //positive Stop Left stops motor
          | _BV(11) //X_LATCH if stopl becomes active ..
            ;
    } 
    else {
#ifdef DEBUG_ENDSTOPS
      Serial.print(F("TMC4361 motor "));
      Serial.print(motor_nr);
      Serial.println(F(" - configuring left end stop as active low"));
#endif
      endstop_config |= 0 
        | _BV(0) //STOP_LEFT enable
        | _BV(10) //X_LATCH if stopl becomes inactive ..
          ;
    }
  } 
  else {
    if (active_high) {
#ifdef DEBUG_ENDSTOPS
      Serial.print(F("TMC4361 motor "));
      Serial.print(motor_nr);
      Serial.println(F(" - cConfiguring right end stop as active high"));
#endif
      endstop_config |= 0 
        | _BV(1) //STOP_RIGHT enable
        | _BV(3) //positive Stop right stops motor
          | _BV(13) //X_LATCH if storl becomes active ..
            ;
    } 
    else {
#ifdef DEBUG_ENDSTOPS
      Serial.print(F("TMC4361 motor "));
      Serial.print(motor_nr);
      Serial.println(F(" - configuring right end stop as active low"));
#endif
      endstop_config |= 0 
        | _BV(1) //STOP_LEFT enable
        | _BV(12) //X_LATCH if stopr becomes inactive ..
          ;
    }
  }
  //ensure that the endstops are inverted if the motor is inverted
  if (inverted_motors & _BV(motor_nr)) {
    endstop_config |= _BV(4);
  }
#ifdef DEBUG_ENDSTOPS_DETAIL
  Serial.print(F("New enstop config "));
  Serial.println(endstop_config, HEX);
#endif
  writeRegister(motor_nr,TMC4361_REFERENCE_CONFIG_REGISTER, endstop_config);
#ifdef DEBUG_ENDSTOPS_DETAIL
  Serial.print(F("Written enstop config "));
  Serial.println( readRegister(motor_nr, TMC4361_REFERENCE_CONFIG_REGISTER),HEX);
#endif
  return NULL;
}

const __FlashStringHelper* configureVirtualEndstopTMC4361(unsigned char motor_nr, boolean left, long positions) {
#ifdef DEBUG_ENDSTOPS_DETAIL
  Serial.print(F("Enstop config before "));
  Serial.println( readRegister(motor_nr, TMC4361_REFERENCE_CONFIG_REGISTER),HEX);
#endif
  unsigned long endstop_config = getClearedEndStopConfigTMC4361(motor_nr, left);
#ifdef DEBUG_ENDSTOPS_DETAIL
  Serial.print(F("Cleared enstop config before "));
  Serial.println(endstop_config);
#endif
  unsigned long position_register;
  if (left) {
#ifdef DEBUG_ENDSTOPS
    Serial.print(F("TMC4361 motor "));
    Serial.print(motor_nr);
    Serial.print(F(" - configuring left virtual endstop at "));
    Serial.println(positions);
#endif
    endstop_config |= _BV(6);
    position_register = TMC4361_VIRTUAL_STOP_LEFT_REGISTER;
    //we doe not latch since we know where they are??
  } 
  else {
#ifdef DEBUG_ENDSTOPS
    Serial.print(F("TMC4361 motor "));
    Serial.print(motor_nr);
    Serial.print(F(" - configuring right virtual endstop at"));
    Serial.println(positions);
#endif
    endstop_config |= _BV(7);
#ifdef DEBUG_ENDSTOPS_DETAIL
    Serial.print(F("Cleared enstop config before "));
    Serial.println(endstop_config);
#endif

    position_register = TMC4361_VIRTUAL_STOP_RIGHT_REGISTER;
    //we doe not latch since we know where they are??
  }
#ifdef DEBUG_ENDSTOPS_DETAIL
  Serial.print(F("New enstop config "));
  Serial.println(endstop_config);
#endif
  writeRegister(motor_nr,TMC4361_REFERENCE_CONFIG_REGISTER, endstop_config);
#ifdef DEBUG_ENDSTOPS_DETAIL
  Serial.print(F("Written enstop config "));
  Serial.println( readRegister(motor_nr, TMC4361_REFERENCE_CONFIG_REGISTER),HEX);
#endif
  writeRegister(motor_nr,position_register, positions);
  return NULL; //TODO this has to be implemented ...
}

inline unsigned long getClearedEndStopConfigTMC4361(unsigned char motor_nr, boolean left) {
  unsigned long endstop_config = readRegister(motor_nr, TMC4361_REFERENCE_CONFIG_REGISTER);
  //clear everything
  unsigned long clearing_pattern; // - a trick to ensure the use of all 32 bits
  if (left) {
    clearing_pattern = TMC4361_LEFT_ENDSTOP_REGISTER_PATTERN;
  } 
  else {
    clearing_pattern = TMC4361_RIGHT_ENDSTOP_REGISTER_PATTERN;
  }
  clearing_pattern = ~clearing_pattern;
  endstop_config &= clearing_pattern;
  return endstop_config;
}  



inline void resetTMC4361(boolean shutdown, boolean bringup) {
  if (shutdown) {
    // Use HWBE pin to reset motion controller TMC4361
    PORTE &= ~(_BV(2));          //to check (reset for motion controller)
  }
  if (shutdown && bringup) {
    delay(1);
  }
  if (bringup) {
    PORTE |= _BV(2);
  }
}







