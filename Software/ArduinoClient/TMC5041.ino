const unsigned long default_chopper_config = 0
| (2<<15) // comparator blank time 2=34
| _BV(13) //random t_off
| (3 << 7) //hysteresis end time
| (5 << 4) // hysteresis start time
| 5 //t OFF
;
//the endstop config seems to be write only 
unsigned long endstop_config_shadow[2] = {
  _BV(11), _BV(11)};

volatile boolean tmc5041_read_position_read_status = true;
volatile boolean v_sense_high[2] = {true, true};

int set_currents[2];

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
  writeRegister(TMC5041_MOTORS, TMC5041_D_1_REGISTER_1,1); //the datahseet says it is needed
  writeRegister(TMC5041_MOTORS, TMC5041_V_START_REGISTER_1, 0);
  writeRegister(TMC5041_MOTORS,TMC5041_V_STOP_REGISTER_1,1); //needed acc to the datasheet?
  writeRegister(TMC5041_MOTORS, TMC5041_V_1_REGISTER_1,0);
  writeRegister(TMC5041_MOTORS, TMC5041_A_1_REGISTER_2,0);
  writeRegister(TMC5041_MOTORS, TMC5041_D_1_REGISTER_2,1); //the datahseet says it is needed
  writeRegister(TMC5041_MOTORS, TMC5041_V_START_REGISTER_2, 0);
  writeRegister(TMC5041_MOTORS,TMC5041_V_STOP_REGISTER_2,1); //needed acc to the datasheet?
  writeRegister(TMC5041_MOTORS, TMC5041_V_1_REGISTER_2,0);

  //and ensure that the event register are emtpy 
  readRegister(TMC5041_MOTORS, TMC5041_RAMP_STATUS_REGISTER_1);
  readRegister(TMC5041_MOTORS, TMC5041_RAMP_STATUS_REGISTER_2);
}

int getCurrentTMC5041(unsigned char motor_number) {
  if (motor_number > 1) {
    return -1;
  }
  return set_currents[motor_number];
}

const char setCurrentTMC5041(unsigned char motor_number, int newCurrent) {
#ifdef DEBUG_MOTOR_CONTFIG
  Serial.print(F("Settings current for TMC5041 #"));
  Serial.print(motor_number);
  Serial.print(F(" to "));
  Serial.print(newCurrent);
  Serial.println(F("mA."));
#endif

  unsigned char run_current = calculateCurrentValue(motor_number, newCurrent);
  unsigned char hold_current = run_current;
  unsigned long current_register=0;


  //set the holding delay
  current_register  = I_HOLD_DELAY;
  current_register <<= 8;
  current_register |= run_current;
  current_register <<= 8;
  current_register |= hold_current;
  if (motor_number==0) {
    writeRegister(TMC5041_MOTORS, TMC5041_HOLD_RUN_CURRENT_REGISTER_1,current_register);
  } else {
    writeRegister(TMC5041_MOTORS, TMC5041_HOLD_RUN_CURRENT_REGISTER_2,current_register);
  }

  set_currents[motor_number] = (int)(((run_current+1.0)/32.0)*((v_sense_high[motor_number]?V_HIGH_SENSE:V_LOW_SENSE)/TMC_5041_R_SENSE)*(1000.0/SQRT_2));

  unsigned long chopper_config = 0
    | (2ul << 15ul) // comparator blank time 2=34
    | (1ul << 13ul) // random t_off
    | (3ul << 7ul)  // hysteresis end time
    | (5ul << 4ul)  // hysteresis start time
    | 5             // t OFF
    ;
  if (v_sense_high[motor_number]) {
    chopper_config |=  (1ul<<17ul); // lower v_sense voltage
  } else {
    chopper_config &= ~(1ul<<17ul); // higher v_sense voltage
  } 
  if (motor_number==0) {
    writeRegister(TMC5041_MOTORS, TMC5041_CHOPPER_CONFIGURATION_REGISTER_1,chopper_config);
  } else {
    writeRegister(TMC5041_MOTORS, TMC5041_CHOPPER_CONFIGURATION_REGISTER_2,chopper_config);
  }
  return 0;
}

const char configureEndstopTMC5041(unsigned char motor_nr, boolean left, boolean active, boolean active_high) {
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
  return 0;
}

const char configureVirtualEndstopTMC5041(unsigned char motor_nr, boolean left, long positions) {
  //todo also with active
  return NULL; //TODO this has to be implemented ...
}

unsigned char calculateCurrentValue(byte motor, int current) {
  // I_RMS = ((CS+1)/32)*(V_FS/R_SENSE)*(1/SQRT(2))
  // CS = ((I_RMS*32*SQRT(2)*R_SENSE)/(V_FS))-1
  float i_rms = current/1000.0;
  float _cs;
  _cs = ((i_rms * 32 * SQRT_2 * TMC_5041_R_SENSE)/V_HIGH_SENSE)-0.5;
  v_sense_high[motor] = true;
  if (_cs > 31) {
    _cs = ((i_rms * 32 * SQRT_2 * TMC_5041_R_SENSE)/V_LOW_SENSE)-0.5;
    v_sense_high[motor] = false;
    if (_cs > 31) {
      return 31;
    } else if (_cs < 0) {
      return 0;
    } else {
      return (unsigned char)_cs;
    }
  } else if (_cs < 0) {
    return 0;
  } else {
    return (unsigned char)_cs;
  }
}

void  moveMotorTMC5041(char motor_nr, long target_pos, double vMax, double aMax, boolean isWaypoint) {
#ifdef DEBUG_MOTION_SHORT
  Serial.print('M');
  Serial.print(motor_nr,DEC);
  Serial.print(F(" t "));
  Serial.print(target_pos);
  Serial.print(F(" @ "));
  Serial.println(vMax);
#endif
#ifdef DEBUG_MOTION
  Serial.print(F("5041 #1 is going to "));
  Serial.print(tmc5031_next_movement[0].target,DEC);
  Serial.print(F(" @ "));
  Serial.println(tmc5031_next_movement[0].vMax,DEC);
#endif
  tmc5031_next_movement[motor_nr].target = target_pos;
  tmc5031_next_movement[motor_nr].vMax = vMax;
  tmc5031_next_movement[motor_nr].aMax = aMax;
}

void tmc5041_prepare_next_motion() {
  if (tmc5031_next_movement[0].vMax!=0) {
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

// just parallel homing for axes 4 and 5, 
const char homeMotorTMC5041(unsigned char motor_nr, unsigned long timeout, double homing_fast_speed, double homing_low_speed, long homing_retraction, double homing_accel, boolean home_right)
{
  unsigned char homing_state = 0;
  unsigned long ref_conf[2];
  unsigned long status;
  long time_start;
  unsigned char pos_reached[2];

  //comfigire homing movement config - acceleration
  writeRegister5041P(TMC5041_RAMP_MODE_REGISTER_1, 0);
  
  writeRegister5041P(TMC5041_A_MAX_REGISTER_1, homing_accel);
  writeRegister5041P(TMC5041_D_MAX_REGISTER_1, homing_accel);
  writeRegister5041P(TMC5041_A_1_REGISTER_1, 0);
  writeRegister5041P(TMC5041_D_1_REGISTER_1, 1);
  writeRegister5041P(TMC5041_V_START_REGISTER_1, 0);
  writeRegister5041P(TMC5041_V_STOP_REGISTER_1, 1);
  writeRegister5041P(TMC5041_V_1_REGISTER_1, 0);
  writeRegister5041P(TMC5041_V_MAX_REGISTER_1, homing_fast_speed);

  ref_conf[0] = readRegister(TMC5041_MOTORS, TMC5041_REFERENCE_SWITCH_CONFIG_REGISTER_1);
  ref_conf[1] = readRegister(TMC5041_MOTORS, TMC5041_REFERENCE_SWITCH_CONFIG_REGISTER_2);
  

  time_start = millis();

  while (homing_state != 255) {
    if (((millis() - time_start)&0xFF) == 0) {
      messenger.sendCmd(kWait,homing_state);
    }
    if ((millis() - time_start) > timeout) {
      messenger.sendCmd(kError,-1);
      homing_state = 255;
    }

    switch (homing_state) {
      case 0:  // start movement towards the endstop
        if (!home_right) {
          // homing to the left
          writeRegister5041P(TMC5041_REFERENCE_SWITCH_CONFIG_REGISTER_1, ((1<<3)|(1<<2)|(1<<0)));
        } else {
          // homing to the right
          writeRegister5041P(TMC5041_REFERENCE_SWITCH_CONFIG_REGISTER_1, ((1<<3)|(1<<2)|(1<<1)));
        }
        writeRegister5041P(TMC5041_RAMP_MODE_REGISTER_1, (home_right?1:2));
        homing_state = 1;
        messenger.sendCmd(kWait,homing_state);
        break;

      case 1:  // wait until endstop is reached, start retraction
        status =  (readRegister(TMC5041_MOTORS, TMC5041_RAMP_STATUS_REGISTER_1) & ((1<<5)|(1<<4))) | 
                  (readRegister(TMC5041_MOTORS, TMC5041_RAMP_STATUS_REGISTER_2) & ((1<<5)|(1<<4)));
        if (status) {
          writeRegister5041P(TMC5041_V_MAX_REGISTER_1, 0);
          writeRegister5041P(TMC5041_X_ACTUAL_REGISTER_1, 0);
          if (!home_right) {
            // retract to the right
            writeRegister5041P(TMC5041_X_TARGET_REGISTER_1, homing_retraction);
          } else {
            // retract to the left
            writeRegister5041P(TMC5041_X_TARGET_REGISTER_1, -homing_retraction);
          }
          writeRegister5041P(TMC5041_RAMP_MODE_REGISTER_1, 0);
          writeRegister5041P(TMC5041_V_MAX_REGISTER_1, homing_fast_speed);
          homing_state = 2;
          pos_reached[0] = pos_reached[1] = 0;
          messenger.sendCmd(kWait,homing_state);
        }
        break;
      
      case 2:  // wait until retracted, abort with error -2 if endstop stays active, start slow movement towards endstop
        status   = readRegister(TMC5041_MOTORS, TMC5041_RAMP_STATUS_REGISTER_2) & 0xFFFF;
        status <<= 16;
        status  |= readRegister(TMC5041_MOTORS, TMC5041_RAMP_STATUS_REGISTER_1) & 0xFFFF;
        if ((status & (1ul<<25ul)) && (status & (1ul<<9ul))) { // check both position reached flags
          if (!((status & (1ul<<17ul)) || (status & (1ul<<16ul)) || (status & (1ul<<1ul)) || (status & (1ul<<0ul)))) { // check all endstop flags
            writeRegister5041P(TMC5041_V_MAX_REGISTER_1, homing_low_speed);
            writeRegister5041P(TMC5041_RAMP_MODE_REGISTER_1, (home_right?1:2));
            homing_state = 3;
            messenger.sendCmd(kWait,homing_state);
          } else {
            homing_state = 255;
            messenger.sendCmd(kError,-2);
          }
        }
        break;

      case 3:  // wait until endstop is reached, this is the home position
        status =  (readRegister(TMC5041_MOTORS, TMC5041_RAMP_STATUS_REGISTER_1) & ((1<<5)|(1<<4))) | 
                  (readRegister(TMC5041_MOTORS, TMC5041_RAMP_STATUS_REGISTER_2) & ((1<<5)|(1<<4)));
        if (status) {
          writeRegister5041P(TMC5041_V_MAX_REGISTER_1, 0);
          writeRegister5041P(TMC5041_X_TARGET_REGISTER_1, 0);
          writeRegister5041P(TMC5041_X_ACTUAL_REGISTER_1, 0);

          homing_state = 255;
          messenger.sendCmd(kWait,homing_state);
        }
        break;
      

      default: // end homing
        homing_state = 255;
    }
  }

  //reset everything
  writeRegister5041P(TMC5041_V_MAX_REGISTER_1, 0);
  writeRegister5041P(TMC5041_RAMP_MODE_REGISTER_1, 0);
  writeRegister5041P(TMC5041_A_MAX_REGISTER_1, 0);
  writeRegister5041P(TMC5041_D_MAX_REGISTER_1, 0);
  writeRegister(TMC5041_MOTORS, TMC5041_REFERENCE_SWITCH_CONFIG_REGISTER_1, ref_conf[0]);
  writeRegister(TMC5041_MOTORS, TMC5041_REFERENCE_SWITCH_CONFIG_REGISTER_2, ref_conf[1]);

  return 0;
}

void writeRegister5041P(unsigned const char the_register, unsigned const long datagram) {
  char regs[2];
  if ((the_register >= 0x20) && (the_register < 0x60)) {
    regs[0] = (the_register & 0x1F) | 0xA0;
    regs[1] = (the_register & 0x1F) | 0xC0;
  } else if ((the_register >= 0x60) && (the_register < 0x80)) {
    regs[0] = (the_register & 0x6F) | 0x80;
    regs[1] = (the_register & 0x6F) | 0x90;
  } else {
    return;
  }
  sendRegister(TMC5041_MOTORS, regs[0], datagram);
  sendRegister(TMC5041_MOTORS, regs[1], datagram);
//  Serial1.print("wRR 0x");  Serial1.print(regs[0],HEX);   Serial1.print(", 0x");   Serial1.print(regs[1],HEX);   Serial1.print(" <= 0x");   Serial1.println(datagram,HEX);
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
#ifdef DEBUG_MOTION_SHORT
  Serial.print('m');
  Serial.print(motor_nr);
  Serial.println(F(":=0"));
#endif
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
