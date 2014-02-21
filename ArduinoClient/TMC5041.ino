void prepareTMC5041() {
  //configure the cs pin
  pinMode(CS_5041_PIN, OUTPUT);
  digitalWrite(CS_5041_PIN, LOW);
  // configure the interrupt pin
  pinMode(INT_5041_PIN, INPUT);
  digitalWrite(INT_5041_PIN, LOW);
}

void init5041() {
  writeRegister(CS_5041_PIN, TMC5041_GENERAL_CONFIG_REGISTER, _BV(3)); //int/PP are outputs
  writeRegister(CS_5041_PIN, TMC5041_RAMP_MODE_REGISTER_1,0); //enforce positioing mode
  writeRegister(CS_5041_PIN, TMC5041_RAMP_MODE_REGISTER_2,0); //enforce positioing mode

}



// Sense calculation

#define V_LOW_SENSE 0.325
#define V_HIGH_SENSE 0.18
#define SQRT_2 (1.414213562373095)
#define TMC_5041_R_SENSE 0.27


unsigned char calculateCurrent(int current) {
  int high_sense_current = calculateCurrent(current, true);
  if (high_sense_current<32) {
    return high_sense_current;
  }
  int low_sense_current = calculateCurrent(current, false);
  if (low_sense_current>31) {
    low_sense_current=31;
  }
  return low_sense_current | 0x80;
}

int calculateCurrent(int current, boolean high_sense) {
  float real_current = (float)current/1000.0;
  int high_sense_current = (int)(real_current*32.0*SQRT_2*TMC_5041_R_SENSE/(high_sense?V_HIGH_SENSE:V_LOW_SENSE))-1;
  return high_sense_current;
}



