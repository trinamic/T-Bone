unsigned char status;


void write5031(unsigned char tmc_register, unsigned long datagram) {
  //select the TMC driver
  digitalWrite(tmc_5031,LOW);

  send5031(tmc_register | 0x80,datagram);

  //deselect the TMC chip
  digitalWrite(tmc_5031,HIGH); 
}

unsigned long read5031(unsigned char tmc_register, unsigned long datagram) {
  //select the TMC driver
  digitalWrite(tmc_5031,LOW);

  send5031(tmc_register, datagram);
  unsigned long return_value = send5031(tmc_register, datagram);

  //deselect the TMC chip
  digitalWrite(tmc_5031,HIGH); 
  return return_value;
}

unsigned long send5031(unsigned char tmc_register, unsigned long datagram) {
  unsigned long i_datagram;

#ifdef DEBUG
  Serial.print("Sending ");
  Serial.println(datagram,HEX);
#endif

  //the first value is irgnored
  status = SPI.transfer(tmc_register);
  //write/read the values
  i_datagram = SPI.transfer((datagram >> 24) & 0xff);
  i_datagram <<= 8;
  i_datagram |= SPI.transfer((datagram >> 16) & 0xff);
  i_datagram <<= 8;
  i_datagram |= SPI.transfer((datagram >>  8) & 0xff);
  i_datagram <<= 8;
  i_datagram |= SPI.transfer((datagram) & 0xff);

#ifdef DEBUG
  Serial.print("Status :");
  Serial.println(status,HEX);
  Serial.print("Received ");
  Serial.println(i_datagram,HEX);
#endif

  return i_datagram;
}

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



