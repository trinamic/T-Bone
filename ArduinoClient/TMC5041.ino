const unsigned long default_chopper_config = 0
| (2<<15) // comparator blank time 2=34
| _BV(13) //random t_off
| (3 << 7) //hysteresis end time
| (5 << 4) // hysteresis start time
| 5 //t OFF
;
//the endstop config seems to be write only 
unsigned long endstop_config_shadow[2] = {
  0,0};

volatile boolean tmc5041_read_position_read_status = true;

TMC5041_motion_info tmc5031_next_movement[2];

void prepareTMC5041() {
  //configure the cs pin
  pinMode(CS_5041_PIN, OUTPUT);
  digitalWrite(CS_5041_PIN, LOW);
  // configure the interrupt pin
  pinMode(INT_5041_PIN, INPUT);
  digitalWrite(INT_5041_PIN, LOW);
  //enable the pin change interrupts
  PCICR |= _BV(0); //PCIE0 - there are no others 
  PCMSK0 = _BV(4); //we want jsut to lsiten to the INT 5031
  writeRegister(TMC5041_MOTORS, TMC5041_GENERAL_CONFIG_REGISTER, _BV(3)); //int/PP are outputs
}

void initialzeTMC5041() {

  //get rid of the 'something happened after reboot' warning
  readRegister(TMC5041_MOTORS, TMC5041_GENERAL_STATUS_REGISTER);
  writeRegister(TMC5041_MOTORS, TMC5041_GENERAL_CONFIG_REGISTER, _BV(3)); //int/PP are outputs
  // motor #1
  writeRegister(TMC5041_MOTORS, TMC5041_RAMP_MODE_REGISTER_1,0); //enforce positioing mode
  writeRegister(TMC5041_MOTORS, TMC5041_X_TARGET_REGISTER_1,0);
  writeRegister(TMC5041_MOTORS, TMC5041_X_ACTUAL_REGISTER_1,0);
  setCurrentTMC5041(0,DEFAULT_CURRENT_IN_MA);
  writeRegister(TMC5041_MOTORS, TMC5041_CHOPPER_CONFIGURATION_REGISTER_1,default_chopper_config);

  // motor #2
  //get rid of the 'something happened after reboot' warning
  writeRegister(TMC5041_MOTORS, TMC5041_RAMP_MODE_REGISTER_2,0); //enforce positioing mode
  writeRegister(TMC5041_MOTORS, TMC5041_X_TARGET_REGISTER_2,0);
  writeRegister(TMC5041_MOTORS, TMC5041_X_ACTUAL_REGISTER_2,0);
  setCurrentTMC5041(1,DEFAULT_CURRENT_IN_MA);
  writeRegister(TMC5041_MOTORS, TMC5041_CHOPPER_CONFIGURATION_REGISTER_2,default_chopper_config);

  //configure reference switches (to nothing)
  writeRegister(TMC5041_MOTORS, TMC5041_REFERENCE_SWITCH_CONFIG_REGISTER_1, 0x0);
  writeRegister(TMC5041_MOTORS, TMC5041_REFERENCE_SWITCH_CONFIG_REGISTER_2, 0x0);

  //those values are static and will anyway reside in the tmc5041 settings
  writeRegister(TMC5041_MOTORS, TMC5041_A_1_REGISTER_1,0);
  writeRegister(TMC5041_MOTORS, TMC5041_D_1_REGISTER_1,0); //the datahseet says it is needed
  writeRegister(TMC5041_MOTORS, TMC5041_V_START_REGISTER_1, 0);
  writeRegister(TMC5041_MOTORS,TMC5041_V_STOP_REGISTER_1,1); //needed acc to the datasheet?
  writeRegister(TMC5041_MOTORS, TMC5041_V_1_REGISTER_1,0);
  writeRegister(TMC5041_MOTORS, TMC5041_A_1_REGISTER_2,0);
  writeRegister(TMC5041_MOTORS, TMC5041_D_1_REGISTER_2,0); //the datahseet says it is needed
  writeRegister(TMC5041_MOTORS, TMC5041_V_START_REGISTER_2, 0);
  writeRegister(TMC5041_MOTORS,TMC5041_V_STOP_REGISTER_2,1); //needed acc to the datasheet?
  writeRegister(TMC5041_MOTORS, TMC5041_V_1_REGISTER_2,0);

  //and ensure that the event register are emtpy 
  readRegister(TMC5041_MOTORS, TMC5041_RAMP_STATUS_REGISTER_1);
  readRegister(TMC5041_MOTORS, TMC5041_RAMP_STATUS_REGISTER_2);
}

const __FlashStringHelper*  setCurrentTMC5041(unsigned char motor_number, int newCurrent) {
#ifdef DEBUG_MOTOR_CONTFIG
  Serial.print(F("Settings current for TMC5041 #"));
  Serial.print(motor_number);
  Serial.print(F(" to "));
  Serial.print(newCurrent);
  Serial.println(F("mA."));
#endif

  //TODO this seems quite strange - the run current is trucated anyway??
  unsigned char run_current = calculateCurrentValue(newCurrent);
  boolean low_sense = run_current & 0x80;
  run_current = run_current & 0x7F;
  unsigned char hold_current;
  if (low_sense) {
    hold_current=calculateCurrentValue(newCurrent,false);
  } 
  else {
    hold_current=calculateCurrentValue(newCurrent,true);
  }
  unsigned long current_register=0;
  //se89t the holding delay
  current_register |= I_HOLD_DELAY << 16;
  current_register |= run_current << 8;
  current_register |= hold_current;
  if (motor_number==0) {
    writeRegister(TMC5041_MOTORS, TMC5041_HOLD_RUN_CURRENT_REGISTER_1,current_register);
  } 
  else {
    writeRegister(TMC5041_MOTORS, TMC5041_HOLD_RUN_CURRENT_REGISTER_2,current_register);
  }
  unsigned long chopper_config = 0
    | (2<<15) // comparator blank time 2=34
    | _BV(13) //random t_off
      | (3 << 7) //hysteresis end time
        | (5 << 4) // hysteresis start time
          | 5 //t OFF
          ;
  if (!low_sense) {
    chopper_config|= _BV(17); //lower v_sense
  } 
  if (motor_number==0) {
    writeRegister(TMC5041_MOTORS, TMC5041_CHOPPER_CONFIGURATION_REGISTER_1,chopper_config);
  } 
  else {
    writeRegister(TMC5041_MOTORS, TMC5041_CHOPPER_CONFIGURATION_REGISTER_2,chopper_config);
  }
  return NULL;
}

const __FlashStringHelper* configureEndstopTMC5041(unsigned char motor_nr, boolean left, boolean active, boolean active_high) {
#ifdef DEBUG_ENDSTOPS_DETAIL
  Serial.print(F("Enstop config before "));
  Serial.println(endstop_config_shadow[motor_nr],HEX);
  
#endif
  unsigned long endstop_config = getClearedEndstopConfigTMC5041(motor_nr, left);
#ifdef DEBUG_ENDSTOPS_DETAIL
  Serial.print(F("Cleared enstop config before "));
  Serial.println(endstop_config,HEX);
#endif

  if (active) {
    if (left) {
      endstop_config |= _BV(0);
      if (active_high) {
#ifdef DEBUG_ENDSTOPS
        Serial.print(F("TMC5041 motor "));
        Serial.print(motor_nr);
        Serial.println(F(" - configuring left end stop as active high"));
#endif
        endstop_config |= _BV(5); //nothing to do here ...
      }  
      else {
#ifdef DEBUG_ENDSTOPS
        Serial.print(F("TMC5041 motor "));
        Serial.print(motor_nr);
        Serial.println(F(" - configuring left end stop as active low"));
#endif
        endstop_config |= _BV(2) | _BV(5);
      }
    } 
    else {
      endstop_config |= _BV(1);
      if (active_high) {
#ifdef DEBUG_ENDSTOPS
        Serial.print(F("TMC5041 motor "));
        Serial.print(motor_nr);
        Serial.println(F(" - configuring right end stop as active high"));
#endif
        endstop_config |= _BV(7);
      }  
      else {
#ifdef DEBUG_ENDSTOPS
        Serial.print(F("TMC5041 motor "));
        Serial.print(motor_nr);
        Serial.println(F(" - configuring right end stop as active low"));
#endif
        endstop_config |= _BV(3) | _BV(7);
      }
    }
  }
#ifdef DEBUG_ENDSTOPS_DETAIL
  Serial.print(F("New enstop config "));
  Serial.println(endstop_config,HEX);
#endif
  if (motor_nr == 0) {
    writeRegister(TMC5041_MOTORS,TMC5041_REFERENCE_SWITCH_CONFIG_REGISTER_1,endstop_config);
  } 
  else {
    writeRegister(TMC5041_MOTORS,TMC5041_REFERENCE_SWITCH_CONFIG_REGISTER_2,endstop_config);
  }
  return NULL;
}

const __FlashStringHelper* configureVirtualEndstopTMC5041(unsigned char motor_nr, boolean left, long positions) {
  //todo also with active
  return NULL; //TODO this has to be implemented ...
}

// Sense calculation
unsigned char calculateCurrentValue(int current) {
  int high_sense_current = calculateCurrentValue(current, true);
  if (high_sense_current<32) {
    return high_sense_current;
  }
  int low_sense_current = calculateCurrentValue(current, false);
  if (low_sense_current>31) {
    low_sense_current=31;
  }
  return low_sense_current | 0x80;
}

void  moveMotorTMC5041(char motor, long target, double vMax, double aMax, boolean prepare_next_movement, boolean isWayPoint) {
  if (prepare_next_movement) {
    tmc5031_next_movement[motor].target = target;
    tmc5031_next_movement[motor].vMax = vMax;
    tmc5031_next_movement[motor].aMax = aMax;
  } 
  else {
    tmc5031_next_movement[0].vMax=0;
    tmc5031_next_movement[1].vMax=0;
    if (motor==0) {
#ifdef DEBUG_MOTION
      Serial.print(F("5041 #1 is first going to "));
      Serial.print(target,DEC);
      Serial.print(F(" @ "));
      Serial.println(vMax,DEC);
#endif

      writeRegister(TMC5041_MOTORS, TMC5041_A_MAX_REGISTER_1,aMax);
      writeRegister(TMC5041_MOTORS, TMC5041_D_MAX_REGISTER_1,aMax);

      writeRegister(TMC5041_MOTORS,TMC5041_V_MAX_REGISTER_1, vMax);
      writeRegister(TMC5041_MOTORS, TMC5041_X_TARGET_REGISTER_1,target);
    } 
    else {
#ifdef DEBUG_MOTION
      Serial.print(F("5041 #2 is first going to "));
      Serial.print(target,DEC);
      Serial.print(F(" @ "));
      Serial.println(vMax,DEC);
#endif
      writeRegister(TMC5041_MOTORS, TMC5041_A_MAX_REGISTER_2,aMax);
      writeRegister(TMC5041_MOTORS, TMC5041_D_MAX_REGISTER_2,aMax);

      writeRegister(TMC5041_MOTORS,TMC5041_V_MAX_REGISTER_2, vMax);
      writeRegister(TMC5041_MOTORS, TMC5041_X_TARGET_REGISTER_2,target);
    }
  }
}

void tmc5041_prepare_next_motion() {
  if (tmc5031_next_movement[0].vMax!=0) {
#ifdef DEBUG_MOTION
    Serial.print(F("5041 #1 is going to "));
    Serial.print(tmc5031_next_movement[0].target,DEC);
    Serial.print(F(" @ "));
    Serial.println(tmc5031_next_movement[0].vMax,DEC);
#endif
    writeRegister(TMC5041_MOTORS, TMC5041_A_MAX_REGISTER_1,tmc5031_next_movement[0].aMax);
    writeRegister(TMC5041_MOTORS, TMC5041_D_MAX_REGISTER_1,tmc5031_next_movement[0].aMax);

    writeRegister(TMC5041_MOTORS,TMC5041_V_MAX_REGISTER_1, tmc5031_next_movement[0].vMax);
    writeRegister(TMC5041_MOTORS, TMC5041_X_TARGET_REGISTER_1,tmc5031_next_movement[0].target);
    tmc5031_next_movement[0].vMax=0;
  }
  if (tmc5031_next_movement[1].vMax!=0) {
#ifdef DEBUG_MOTION
    Serial.print(F("5041 #2 is going to "));
    Serial.print(tmc5031_next_movement[1].target,DEC);
    Serial.print(F(" @ "));
    Serial.println(tmc5031_next_movement[1].vMax,DEC);
#endif
    writeRegister(TMC5041_MOTORS, TMC5041_A_MAX_REGISTER_2,tmc5031_next_movement[1].aMax);
    writeRegister(TMC5041_MOTORS, TMC5041_D_MAX_REGISTER_2,tmc5031_next_movement[1].aMax);

    writeRegister(TMC5041_MOTORS,TMC5041_V_MAX_REGISTER_2, tmc5031_next_movement[1].vMax);
    writeRegister(TMC5041_MOTORS, TMC5041_X_TARGET_REGISTER_2,tmc5031_next_movement[1].target);
    tmc5031_next_movement[1].vMax=0;
  }
}

const __FlashStringHelper* homeMotorTMC5041(unsigned char motor_nr, unsigned long timeout, 
double homing_fast_speed, double homing_low_speed, long homing_retraction,
double homming_accel,
char* followers)
{
  //TODO shouldn't we check if there is a motion going on??
  unsigned long acceleration_value = 1000;// min((unsigned long) homming_accel,0xFFFFul);

#ifdef DEBUG_HOMING
  Serial.print(F("Homing for motor "));
  Serial.print(motor_nr,DEC);
  Serial.print(F(", timeout="));
  Serial.print(timeout);
  Serial.print(F(", fast="));
  Serial.print(homing_fast_speed);
  Serial.print(F(", slow="));
  Serial.print(homing_low_speed);
  Serial.print(F(", retract="));
  Serial.print(homing_retraction);
  Serial.print(F(", aMax="));
  Serial.print(acceleration_value);
  Serial.print(F(": follow=("));
  for (char i = 0; i< homing_max_following_motors ;i++) {
    Serial.print(followers[i],DEC);
    Serial.print(F(", "));
  }
  Serial.print(')');
  Serial.println();
#endif
  //configure acceleration for homing
  writeRegister(TMC5041_MOTORS, TMC5041_A_MAX_REGISTER_1,acceleration_value);
  writeRegister(TMC5041_MOTORS, TMC5041_D_MAX_REGISTER_1,acceleration_value);
  writeRegister(TMC5041_MOTORS, TMC5041_A_1_REGISTER_1,acceleration_value);
  writeRegister(TMC5041_MOTORS, TMC5041_D_1_REGISTER_1,acceleration_value); //the datahseet says it is needed
  writeRegister(TMC5041_MOTORS, TMC5041_V_START_REGISTER_1, 0);
  writeRegister(TMC5041_MOTORS,TMC5041_V_STOP_REGISTER_1,1); //needed acc to the datasheet?
  writeRegister(TMC5041_MOTORS, TMC5041_V_1_REGISTER_1,0);

  writeRegister(TMC5041_MOTORS, TMC5041_A_MAX_REGISTER_2,acceleration_value);
  writeRegister(TMC5041_MOTORS, TMC5041_D_MAX_REGISTER_2,acceleration_value);
  writeRegister(TMC5041_MOTORS, TMC5041_A_1_REGISTER_2,acceleration_value);
  writeRegister(TMC5041_MOTORS, TMC5041_D_1_REGISTER_2,acceleration_value); //the datahseet says it is needed
  writeRegister(TMC5041_MOTORS, TMC5041_V_START_REGISTER_2, 0);
  writeRegister(TMC5041_MOTORS,TMC5041_V_STOP_REGISTER_2,1); //needed acc to the datasheet?
  writeRegister(TMC5041_MOTORS, TMC5041_V_1_REGISTER_2,0);

  //so here is the theoretic trick:
  /*
  homing is just a homing the 4361
   go down a bit until RAMP_STAT velocity_ reached or event_pos_ reached
   if event_stop_r the x_latch can be read 
   
   the following motor is a bit trickier:
   zero all motors before we start - they will be zerored anyway (x_actual)
   closely monitor event_stop_r
   if it is hit stop the motor manually
   - should be fast enough
   - and is an excuse to implement the home moving blocking 
   */

  //TODO obey the timeout!!
  unsigned char homed = 0; //this is used to track where at homing we are 
  long target = 0;
#ifdef DEBUG_HOMING_STATUS_5041
  unsigned long old_status = -1;
#endif

  while (homed!=0xff) { //we will never have 255 homing phases - but whith this we not have to think about it 
    unsigned long status=0;
    if (homed==0 || homed==1) {
      unsigned long homing_speed=(unsigned long) homing_fast_speed; 
      if (homed==1) {
        homing_speed = (unsigned long) homing_low_speed;
      }  
      if (motor_nr==0) {
        status = readRegister(TMC5041_MOTORS, TMC5041_RAMP_STATUS_REGISTER_1);
      } 
      else {
        status = readRegister(TMC5041_MOTORS, TMC5041_RAMP_STATUS_REGISTER_2);
      }
#ifdef DEBUG_HOMING_STATUS_5041
      if (status!=old_status) {
        Serial.print("Status1 ");
        Serial.println(status,HEX);
        Serial.print(F("Position "));
        Serial.print((long)readRegister(TMC5041_MOTORS, TMC5041_X_ACTUAL_REGISTER_1));
        Serial.print(F(", Targe "));
        Serial.println((long)readRegister(TMC5041_MOTORS, TMC5041_X_TARGET_REGISTER_1));
        Serial.print(F(", Velocity "));
        Serial.println((long)readRegister(TMC5041_MOTORS, TMC5041_V_ACTUAL_REGISTER_1));
        old_status=status;
      }
#endif
      if ((status & (_BV(4) | _BV(0)))==0){ //reference switches not hit
        if (status & (_BV(12) | _BV(10) | _BV(9))) { //not moving or at or beyond target
          target -= 9999999l;
#ifdef DEBUG_HOMING
          Serial.print(F("Homing to "));
          Serial.print(target);
          Serial.print(F(" @ "));
          Serial.println(homing_speed);
          Serial.print(F("Status "));
          Serial.print(status,HEX);
          Serial.print(F(" phase "));
          Serial.println(homed,DEC);
          Serial.print(F("Position "));
          Serial.print(readRegister(TMC5041_MOTORS, TMC5041_X_ACTUAL_REGISTER_1));
          Serial.print(F(", Velocity "));
          Serial.println((long)readRegister(TMC5041_MOTORS, TMC5041_V_ACTUAL_REGISTER_1));
#endif
          if (motor_nr==0) {
            writeRegister(TMC5041_MOTORS,TMC5041_V_MAX_REGISTER_1, homing_speed);
            writeRegister(TMC5041_MOTORS, TMC5041_X_TARGET_REGISTER_1,target);
          } 
          else {            
            writeRegister(TMC5041_MOTORS,TMC5041_V_MAX_REGISTER_2, homing_speed);
            writeRegister(TMC5041_MOTORS, TMC5041_X_TARGET_REGISTER_2,target);
          }
          for (char i = 0; i< homing_max_following_motors ;i++) {
            if (followers[i]==0) {
              writeRegister(TMC5041_MOTORS,TMC5041_V_MAX_REGISTER_1, homing_speed);
              writeRegister(TMC5041_MOTORS, TMC5041_X_TARGET_REGISTER_1,target);
            } 
            else if (followers[i]==1) {
              writeRegister(TMC5041_MOTORS,TMC5041_V_MAX_REGISTER_2, homing_speed);
              writeRegister(TMC5041_MOTORS, TMC5041_X_TARGET_REGISTER_2,target);
            }
          }
        }
      } 
      else{ //reference switches hit
        long go_back_to;
        if (homed==0) {
          long actual;
          if (motor_nr==0) {
            actual = (long) readRegister(TMC5041_MOTORS, TMC5041_X_ACTUAL_REGISTER_1);
          } 
          else {          
            actual = (long) readRegister(TMC5041_MOTORS, TMC5041_X_ACTUAL_REGISTER_2);
          }
          go_back_to = actual + homing_retraction;
#ifdef DEBUG_HOMING
          Serial.print(F("home near "));
          Serial.print(actual);
          Serial.print(F(" - going back to "));
          Serial.println(go_back_to);
#endif
        } 
        else {
          if (motor_nr==0) {
            go_back_to = (long) readRegister(TMC5041_MOTORS, TMC5041_X_LATCH_REGISTER_1);
          } 
          else {          
            go_back_to = (long) readRegister(TMC5041_MOTORS, TMC5041_X_LATCH_REGISTER_2);
          }
          if (go_back_to==0) {
            //&we need some kiond of backu if there is something wrong with x_latch 
            if (motor_nr==0) {
              go_back_to = (long) readRegister(TMC5041_MOTORS, TMC5041_X_ACTUAL_REGISTER_1);
            } 
            else {          
              go_back_to = (long) readRegister(TMC5041_MOTORS, TMC5041_X_ACTUAL_REGISTER_1);
            }
          }
#ifdef DEBUG_HOMING
          Serial.print(F("homed at "));
          Serial.println(go_back_to);
#endif
        }
        if (motor_nr==0) {
          writeRegister(TMC5041_MOTORS,TMC5041_V_MAX_REGISTER_1,homing_speed);
          writeRegister(TMC5041_MOTORS, TMC5041_X_TARGET_REGISTER_1,go_back_to);
        } 
        else {
          writeRegister(TMC5041_MOTORS,TMC5041_V_MAX_REGISTER_2, homing_speed);
          writeRegister(TMC5041_MOTORS, TMC5041_X_TARGET_REGISTER_2,go_back_to);
        }        
        for (char i = 0; i< homing_max_following_motors ;i++) {
          if (followers[i]==0) {
            writeRegister(TMC5041_MOTORS,TMC5041_V_MAX_REGISTER_1,homing_speed);
            writeRegister(TMC5041_MOTORS, TMC5041_X_TARGET_REGISTER_1,go_back_to);
          } 
          else if (followers[i]==1) {
            writeRegister(TMC5041_MOTORS,TMC5041_V_MAX_REGISTER_2, homing_speed);
            writeRegister(TMC5041_MOTORS, TMC5041_X_TARGET_REGISTER_2,go_back_to);
          }
        }
        delay(10ul);
        if (motor_nr==0) {
          status = readRegister(TMC5041_MOTORS, TMC5041_RAMP_STATUS_REGISTER_1);
        } 
        else {
          status = readRegister(TMC5041_MOTORS, TMC5041_RAMP_STATUS_REGISTER_2);
        }
        while (!(status & _BV(9))) { //are we there yet??
          if (motor_nr==0) {
            status = readRegister(TMC5041_MOTORS, TMC5041_RAMP_STATUS_REGISTER_1);
          } 
          else {
            status = readRegister(TMC5041_MOTORS, TMC5041_RAMP_STATUS_REGISTER_2);
          }
        }
        if (homed==0) {
          homed = 1;
        } 
        else {
          if (motor_nr==0) {
            writeRegister(motor_nr, TMC5041_X_ACTUAL_REGISTER_1,0);
            writeRegister(motor_nr, TMC5041_X_TARGET_REGISTER_1,0);
          } 
          else {
            writeRegister(motor_nr, TMC5041_X_ACTUAL_REGISTER_2,0);
            writeRegister(motor_nr, TMC5041_X_TARGET_REGISTER_2,0);
          }        
          for (char i = 0; i< homing_max_following_motors ;i++) {
            if (followers[i]==0) {
              writeRegister(motor_nr, TMC5041_X_ACTUAL_REGISTER_1,0);
              writeRegister(motor_nr, TMC5041_X_TARGET_REGISTER_1,0);
            } 
            else if (followers[i]==1) {
              writeRegister(motor_nr, TMC5041_X_ACTUAL_REGISTER_2,0);
              writeRegister(motor_nr, TMC5041_X_TARGET_REGISTER_2,0);
            }
          }
          homed=0xff;
        }     
      } 
    } 
  }
  return NULL;
}

boolean invertMotorTMC5041(char motor_nr, boolean inverted) {
  unsigned long general_conf= readRegister(TMC5041_MOTORS, TMC5041_GENERAL_CONFIG_REGISTER);
  unsigned long pattern; //used for deletion and setting of the bit
  if (motor_nr==0) {
    pattern = _BV(8);
  } 
  else {
    pattern = _BV(9);
  }
  if (inverted) {
    //set the bit
    general_conf|= pattern;
  } 
  else {
    //clear the bit
    general_conf &= ~pattern;
  }
  writeRegister(TMC5041_MOTORS, TMC5041_GENERAL_CONFIG_REGISTER,general_conf); 
  general_conf= readRegister(TMC5041_MOTORS, TMC5041_GENERAL_CONFIG_REGISTER);
  //finally return if the bit iss set 
  return ((general_conf & pattern)==pattern);
}

void setMotorPositionTMC5041(unsigned char motor_nr, long position) {
  //we write x_actual an x_target to the same value to be safe 
  if (motor_nr==0) {
    writeRegister(TMC5041_MOTORS, TMC5041_X_TARGET_REGISTER_1,position);
    writeRegister(TMC5041_MOTORS, TMC5041_X_ACTUAL_REGISTER_1,position);
  } 
  else {
    writeRegister(TMC5041_MOTORS, TMC5041_X_TARGET_REGISTER_2,position);
    writeRegister(TMC5041_MOTORS, TMC5041_X_ACTUAL_REGISTER_2,position);
  }
}


int calculateCurrentValue(int current, boolean high_sense) {
  float real_current = (float)current/1000.0;
  int high_sense_current = (int)(real_current*32.0*SQRT_2*TMC_5041_R_SENSE/(high_sense?V_HIGH_SENSE:V_LOW_SENSE))-1;
  return high_sense_current;
}

unsigned long getClearedEndstopConfigTMC5041(char motor_nr, boolean left) {
  unsigned long endstop_config = endstop_config_shadow[motor_nr];
  //clear everything
  unsigned long clearing_pattern; // - a trick to ensure the use of all 32 bits
  if (left) {
    clearing_pattern = TMC5041_LEFT_ENDSTOP_REGISTER_PATTERN;
  } 
  else {
    clearing_pattern = TMC5041_RIGHT_ENDSTOP_REGISTER_PATTERN;
  }
  clearing_pattern = ~clearing_pattern;
  endstop_config &= clearing_pattern;
  return endstop_config;
}

void checkTMC5041Motion() {
  if (tmc5041_read_position_read_status) {
    tmc5041_read_position_read_status=false;
    //We are only interested in the first 8 bits 
    unsigned char events0 = readRegister(TMC5041_MOTORS, TMC5041_RAMP_STATUS_REGISTER_1);
    unsigned char events1 = readRegister(TMC5041_MOTORS, TMC5041_RAMP_STATUS_REGISTER_2);
    if ((events0 | events1) & _BV(7)) {
      //we are only interested in target reached
      //TODO is that enough?? should not we track each motor??
      motor_target_reached(nr_of_coordinated_motors);
    }
  }  
}

ISR(PCINT0_vect)
{
  tmc5041_read_position_read_status = true;
}














