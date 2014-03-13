void writeRegister(unsigned char CS, unsigned char tmcRegister, unsigned long datagram)
{
  digitalWrite(CS, HIGH);
  sendRegister(tmcRegister | 0x80,datagram);
  digitalWrite(CS, LOW);
}

unsigned long readRegister(unsigned char CS, unsigned char tmcRegister, unsigned long datagram)
{
  digitalWrite(CS, HIGH);
  sendRegister(tmcRegister, datagram);
  digitalWrite(CS, LOW);
}

void sendRegister(unsigned char tmcRegister, unsigned long datagram)
{
  unsigned long i_datagram;

#ifdef DEBUG
  Serial.print("Sending ");
  Serial.println(datagram, HEX);
#endif

  //the first value is ignored
  status = SPI.transfer(tmcRegister);
  //write/read the values
  i_datagram = SPI.transfer((datagram >> 24) & 0xff);
  i_datagram <<= 8;
  i_datagram = SPI.transfer((datagram >> 16) & 0xff);
  i_datagram <<= 8;
  i_datagram |= SPI.transfer((datagram >>  8) & 0xff);
  i_datagram <<= 8;
  i_datagram |= SPI.transfer((datagram) & 0xff);

#ifdef DEBUG
  Serial.print("Status :");
  Serial.println(status,BIN);
  Serial.print("Received ");
  Serial.println(i_datagram, DEC);
#endif
}

void set260Register(unsigned char CS, unsigned long value) {
  //santitize to 20 bits 
  value &= 0xFFFFF;
  writeRegister(CS, COVER_LOW_REGISTER, value);  //Cover-Register: Einstellung des SMARTEN=aus

  writeRegister(CS, STATUS_REGISTER, 0x0); //Abfrage Status, um SPI-Transfer zu beenden
  writeRegister(CS, STATUS_REGISTER, 0x0); //Abfrage Status, um SPI-Transfer zu beenden
  writeRegister(CS, STATUS_REGISTER, 0x0); //Abfrage Status, um SPI-Transfer zu beenden
}

// Sense calculation

#define V_LOW_SENSE 0.325
#define V_HIGH_SENSE 0.18
#define SQRT_2 (1.414213562373095)

unsigned char calculateCurrent(int current) {
  int high_sense_current = calculateCurrentHighSense(current);
  if (high_sense_current<32) {
    return high_sense_current;
  }
  int low_sense_current = calculateCurrentLowSense(current);
  if (low_sense_current>31) {
    low_sense_current=31;
  }
  return low_sense_current | 0x80;
}

int calculateCurrentHighSense(int current) {
  float real_current = (float)current/1000.0;
  int high_sense_current = (int)(real_current*32.0*SQRT_2*TMC_5031_R_SENSE/V_HIGH_SENSE)-1;
  return high_sense_current;
}

int calculateCurrentLowSense(int current) {
  float real_current = (float)current/1000.0;
  int low_sense_current = (int)(real_current*32.0*SQRT_2*TMC_5031_R_SENSE/V_LOW_SENSE)-1;
  return low_sense_current;
}
