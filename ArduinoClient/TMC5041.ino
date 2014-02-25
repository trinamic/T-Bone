void prepareTMC5041() {
  //configure the cs pin
  pinMode(CS_5041_PIN, OUTPUT);
  digitalWrite(CS_5041_PIN, LOW);
  // configure the interrupt pin
  pinMode(INT_5041_PIN, INPUT);
  digitalWrite(INT_5041_PIN, LOW);
}

void initialzeTMC5041() {

  //get rid of the 'something happened after reboot' warning
  readRegister(CS_5041_PIN, TMC5041_GENERAL_STATUS_REGISTER,0);
  writeRegister(CS_5041_PIN, TMC5041_GENERAL_CONFIG_REGISTER, _BV(3)); //int/PP are outputs
  // motor #1
  writeRegister(CS_5041_PIN, TMC5041_RAMP_MODE_REGISTER_1,0); //enforce positioing mode
  setCurrentTMC5041(0,DEFAULT_CURRENT_IN_MA);
  // motor #2
  //get rid of the 'something happened after reboot' warning
  writeRegister(CS_5041_PIN, TMC5041_RAMP_MODE_REGISTER_2,0); //enforce positioing mode
  setCurrentTMC5041(1,DEFAULT_CURRENT_IN_MA);
}

const __FlashStringHelper*  setCurrentTMC5041(unsigned char motor_number, int newCurrent) {
  #ifdef DEBUG_MOTOR_CONTFIG
  Serial.print(F("Settings current for #"));
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
    writeRegister(CS_5041_PIN, TMC5041_HOLD_RUN_CURRENT_REGISTER_1,current_register);
  } 
  else {
    writeRegister(CS_5041_PIN, TMC5041_HOLD_RUN_CURRENT_REGISTER_2,current_register);
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
    writeRegister(CS_5041_PIN, TMC5041_CHOPPER_CONFIGURATION_REGISTER_1,chopper_config);
  } 
  else {
    writeRegister(CS_5041_PIN, TMC5041_CHOPPER_CONFIGURATION_REGISTER_2,chopper_config);
  }
  return NULL;
}

const __FlashStringHelper* configureEndstopTMC5041(unsigned char motor_nr, boolean left, boolean active, boolean active_high) {
  unsigned long endstop_config = getClearedEndstopConfigTMC5041(motor_nr, left);
  if (active) {
    if (left) {
      endstop_config |= _BV(0);
        if (active_high) {
#ifdef DEBUG_ENDSTOPS
      Serial.println(F("Configuring left end stop as active high"));
#endif
          endstop_config |= _BV(2) | _BV(5);
      }  
      else {
#ifdef DEBUG_ENDSTOPS
      Serial.println(F("Configuring left end stop as active low"));
#endif
          endstop_config |= _BV(6);
      }
    } 
    else {
      endstop_config |= _BV(1);
        if (active_high) {
 #ifdef DEBUG_ENDSTOPS
      Serial.println(F("Configuring left end stop as active high"));
#endif
         endstop_config |= _BV(3) | _BV(7);
      }  
      else {
#ifdef DEBUG_ENDSTOPS
      Serial.println(F("Configuring right end stop as active low"));
#endif
          endstop_config |= _BV(8);
      }
    }
  }
  if (motor_nr == 0) {
    writeRegister(CS_5041_PIN,TMC5041_REFERENCE_SWITCH_CONFIG_REGISTER_1,endstop_config);
  } 
  else {
    writeRegister(CS_5041_PIN,TMC5041_REFERENCE_SWITCH_CONFIG_REGISTER_2,endstop_config);
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

int calculateCurrentValue(int current, boolean high_sense) {
  float real_current = (float)current/1000.0;
  int high_sense_current = (int)(real_current*32.0*SQRT_2*TMC_5041_R_SENSE/(high_sense?V_HIGH_SENSE:V_LOW_SENSE))-1;
  return high_sense_current;
}

unsigned long getClearedEndstopConfigTMC5041(char motor_nr, boolean left) {
  unsigned long endstop_config;
  if (motor_nr == 0) {
    endstop_config = readRegister(CS_5041_PIN,TMC5041_REFERENCE_SWITCH_CONFIG_REGISTER_1,0);
  } 
  else {
    endstop_config = readRegister(CS_5041_PIN,TMC5041_REFERENCE_SWITCH_CONFIG_REGISTER_2,0);
  }
  //clear everything
  unsigned long clearing_pattern; // - a trick to ensure the use of all 32 bits
  if (left) {
    clearing_pattern = TMC5041_LEFT_ENDSTOP_REGISTER_PATTERN;
  } 
  else {
    clearing_pattern = TMC5041_RIGHT_ENDSTOP_REGISTER_PATTERN;
  }
  clearing_pattern = ~clearing_pattern;
  endstop_config |= clearing_pattern;
  return endstop_config;

}






