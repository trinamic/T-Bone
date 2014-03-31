const unsigned long default_4361_start_config = 0
| _BV(0) //x_target requires start
| _BV(4)  //use shaddow motion profiles
| _BV(5) //external start is an start
;


TMC4361_info motors[nr_of_coordinated_motors] = {
  {
    INT_4361_1_PIN,0, NULL, 
    TMC26XGenerator(DEFAULT_CURRENT_IN_MA,TMC260_SENSE_RESISTOR_IN_MO),
    DEFAULT_STEPS_PER_REVOLUTION                  }
  ,
  {
    INT_4361_2_PIN,1, NULL, 
    TMC26XGenerator(DEFAULT_CURRENT_IN_MA,TMC260_SENSE_RESISTOR_IN_MO), 
    DEFAULT_STEPS_PER_REVOLUTION                   }
  ,
  {
    INT_4361_3_PIN,4, NULL, 
    TMC26XGenerator(DEFAULT_CURRENT_IN_MA,TMC260_SENSE_RESISTOR_IN_MO), 
    DEFAULT_STEPS_PER_REVOLUTION                   }
};


void initTMC4361() {
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
    unsigned long filter = (2<<16) | (4<<20);
    Serial.println(filter);
    writeRegister(i,TMC4361_INPUT_FILTER_REGISTER,filter);
  }
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

const __FlashStringHelper* setStepsPerRevolutionTMC4361(unsigned char motor_nr, unsigned int steps) {
  //configure the motor type
  unsigned long motorconfig = 0x00; //we want 256 microsteps
  motorconfig |= steps<<4;
  writeRegister(motor_nr,TMC4361_STEP_CONF_REGISTER,motorconfig);
  motors[motor_nr].steps_per_revolution = steps;
  return NULL;
}



