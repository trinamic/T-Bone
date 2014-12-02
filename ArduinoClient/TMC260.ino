char setCurrentTMC260(unsigned char motor_number, int newCurrent) {
  if (newCurrent>MAX_MOTOR_CURRENT_260) {
    return -1;
  } 
#ifdef DEBUG_MOTOR_CONTFIG
  Serial.print(F("Settings current for TMC260 #"));
  Serial.print(motor_number);
  Serial.print(F(" to "));
  Serial.print(newCurrent);
  Serial.println(F("mA."));
#endif

  motors[motor_number].tmc260.setCurrent(newCurrent);
  setTMC260Registers(motor_number);
  return NULL;
}

void intializeTMC260() {
  for (char i=0; i<nr_of_coordinated_motors;i++) {
    //configure TMC43x SPI
    writeRegister(i, TMC4361_SPIOUT_CONF_REGISTER,TMC_260_CONFIG);
    //configure the TMC26x
    motors[i].tmc260.setMicrosteps(256);
    setTMC260Registers(i);
  }
}

void setTMC260Registers(unsigned char motor_number) {
  set260Register(motor_number,motors[motor_number].tmc260.getDriverControlRegisterValue());
  set260Register(motor_number,motors[motor_number].tmc260.getChopperConfigRegisterValue());
  set260Register(motor_number,motors[motor_number].tmc260.getStallGuard2RegisterValue());
  set260Register(motor_number,motors[motor_number].tmc260.getDriverConfigurationRegisterValue() | 0x80);
}


void set260Register(unsigned char motor_number, unsigned long value) {
  //santitize to 20 bits 
  value &= 0xFFFFF;
  writeRegister(motor_number, TMC4361_COVER_LOW_REGISTER,value);  //Cover-Register: Einstellung des SMARTEN=aus

  writeRegister(motor_number, TMC4361_STATUS_REGISTER,0x0); //Abfrage Status, um SPI-Transfer zu beenden
  writeRegister(motor_number, TMC4361_STATUS_REGISTER,0x0); //Abfrage Status, um SPI-Transfer zu beenden
  writeRegister(motor_number, TMC4361_STATUS_REGISTER,0x0); //Abfrage Status, um SPI-Transfer zu beenden
}
