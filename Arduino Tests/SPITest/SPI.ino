unsigned char status;


void write43x(unsigned char motor, unsigned char tmc43x_register, unsigned long datagram) {
  //select the TMC driver
  digitalWrite(motor,LOW);

  send43x(tmc43x_register | 0x80,datagram);

  //deselect the TMC chip
  digitalWrite(motor,HIGH); 
}

void read43x(unsigned char motor, unsigned char tmc43x_register, unsigned long datagram) {
  //select the TMC driver
  digitalWrite(motor,LOW);

  send43x(tmc43x_register, datagram);

  //deselect the TMC chip
  digitalWrite(motor,HIGH); 
}

void send43x(unsigned char tmc43x_register, unsigned long datagram) {
  unsigned long i_datagram;

#ifdef DEBUG
  Serial.print("Sending ");
  Serial.println(datagram,HEX);
#endif

  //the first value is irgnored
  status = SPI.transfer(tmc43x_register);
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

void set260Register(unsigned char motor, unsigned long value) {
  //santitize to 20 bits 
  value &= 0xFFFFF;
  write43x(motor, COVER_LOW_REGISTER,value);  //Cover-Register: Einstellung des SMARTEN=aus

  read43x(motor, STATUS_REGISTER,0x0); //Abfrage Status, um SPI-Transfer zu beenden
  read43x(motor, STATUS_REGISTER,0x0); //Abfrage Status, um SPI-Transfer zu beenden
  read43x(motor, STATUS_REGISTER,0x0); //Abfrage Status, um SPI-Transfer zu beenden
}



