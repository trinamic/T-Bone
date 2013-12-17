unsigned char status;


void write43x(unsigned char cs_squirrel,unsigned char tmc43x_register, unsigned long datagram) {
  send43x(cs_squirrel,tmc43x_register | 0x80,datagram);
}

unsigned long read43x(unsigned char cs_squirrel,unsigned char tmc43x_register, unsigned long datagram) {
  send43x(cs_squirrel,tmc43x_register, datagram);
  unsigned long result =  send43x(cs_squirrel, tmc43x_register, datagram);
  return result;
}

long send43x(unsigned char cs_squirrel, unsigned char tmc43x_register, unsigned long datagram) {
  unsigned long i_datagram;

  //select the TMC driver
  digitalWrite(cs_squirrel,LOW);

#ifdef DEBUG_SPI
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

#ifdef DEBUG_SPI
  Serial.print("Status :");
  Serial.println(status,BIN);
  Serial.print("Received ");
  Serial.println(i_datagram,HEX);
#endif

  //deselect the TMC chip
  digitalWrite(cs_squirrel,HIGH); 

  return i_datagram;
}


void set260Register(unsigned char cs_pin, unsigned long value) {
  //santitize to 20 bits 
  value &= 0xFFFFF;
  write43x(cs_pin, COVER_LOW_REGISTER,value);  //Cover-Register: Einstellung des SMARTEN=aus

  read43x(cs_pin, STATUS_REGISTER,0x0); //Abfrage Status, um SPI-Transfer zu beenden
  read43x(cs_pin, STATUS_REGISTER,0x0); //Abfrage Status, um SPI-Transfer zu beenden
  read43x(cs_pin, STATUS_REGISTER,0x0); //Abfrage Status, um SPI-Transfer zu beenden
}




