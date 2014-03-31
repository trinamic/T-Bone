

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
