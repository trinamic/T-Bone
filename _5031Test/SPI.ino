unsigned char status;


void write5031(unsigned char tmc_register, unsigned long datagram) {
  //select the TMC driver
  digitalWrite(tmc_5031,LOW);

  send5031(tmc_register | 0x80,datagram);

  //deselect the TMC chip
  digitalWrite(tmc_5031,HIGH); 
}

void read5031x(unsigned char tmc_register, unsigned long datagram) {
  //select the TMC driver
  digitalWrite(tmc_5031,LOW);

  send5031(tmc_register, datagram);

  //deselect the TMC chip
  digitalWrite(tmc_5031,HIGH); 
}

void send5031(unsigned char tmc_register, unsigned long datagram) {
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
  i_datagram = SPI.transfer((datagram >> 16) & 0xff);
  i_datagram <<= 8;
  i_datagram |= SPI.transfer((datagram >>  8) & 0xff);
  i_datagram <<= 8;
  i_datagram |= SPI.transfer((datagram) & 0xff);

#ifdef DEBUG
  Serial.print("Status :");
  Serial.println(status,BIN);
  Serial.print("Received ");
  Serial.println(i_datagram,HEX);
#endif
}

