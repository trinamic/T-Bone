//we have a TMC260 at the end so we configure a configurer
TMC26XGenerator tmc260 = TMC26XGenerator(current_in_ma,TMC260_SENSE_RESISTOR_IN_MO);

void intializeTMC260() {
  //configure TMC43x SPI
  write43x(SPIOUT_CONF_REGISTER,TMC_26X_CONFIG);
  //configure the TMC26x
  tmc260.setMicrosteps(256);
  setTMC260Registers();
}

const __FlashStringHelper* setCurrent(int newCurrent) {
  if (newCurrent>MAX_MOTOR_CURRENT) {
    return F("Current too high");
  } 
  tmc260.setCurrent(newCurrent);
  setTMC260Registers();
  return NULL;
}

void setTMC260Registers() {
  set260Register(tmc260.getDriverControlRegisterValue());
  set260Register(tmc260.getChopperConfigRegisterValue());
  set260Register(tmc260.getStallGuard2RegisterValue());
  set260Register(tmc260.getDriverConfigurationRegisterValue() | 0x80);
}


void set260Register(unsigned long value) {
  //santitize to 20 bits 
  value &= 0xFFFFF;
  write43x(COVER_LOW_REGISTER,value);  //Cover-Register: Einstellung des SMARTEN=aus

  read43x(STATUS_REGISTER,0x0); //Abfrage Status, um SPI-Transfer zu beenden
  read43x(STATUS_REGISTER,0x0); //Abfrage Status, um SPI-Transfer zu beenden
  read43x(STATUS_REGISTER,0x0); //Abfrage Status, um SPI-Transfer zu beenden
}



