unsigned char status;


void write43x(unsigned char motor, unsigned char tmc43x_register, unsigned long datagram) {

  send43x(motor,tmc43x_register | 0x80,datagram);

}

 long read43x(unsigned char motor, unsigned char tmc43x_register, unsigned long datagram) {
  send43x(motor, tmc43x_register, datagram);
   long result =  send43x(motor,tmc43x_register, datagram);
  return result;
}

 long send43x(unsigned char motor,unsigned char tmc43x_register, unsigned long datagram) {
  unsigned long i_datagram;
  //select the TMC driver
  digitalWrite(motor,LOW);

#ifdef DEBUG
  Serial.print("Sending ");
  Serial.println(datagram,HEX);
#endif

  //the first value is irgnored
  status = SPI.transfer(tmc43x_register);
  //write/read the values
  i_datagram = SPI.transfer((datagram >> 24) & 0xff);
  i_datagram <<= 8;
  i_datagram |= SPI.transfer((datagram >> 16) & 0xff);
  i_datagram <<= 8;
  i_datagram |= SPI.transfer((datagram >>  8) & 0xff);
  i_datagram <<= 8;
  i_datagram |= SPI.transfer(datagram & 0xff);

#ifdef DEBUG
  Serial.print("Status :");
  Serial.println(status,BIN);
  Serial.print("Received ");
  Serial.println(i_datagram,HEX);
#endif
  //deselect the TMC chip
  digitalWrite(motor,HIGH); 

  return i_datagram;
}

void set260Register(unsigned char motor, unsigned long value) {
  //santitize to 20 bits 
  value &= 0xFFFFF;
  write43x(motor, COVER_LOW_REGISTER,value);  //Cover-Register: Einstellung des SMARTEN=aus

  read43x(motor, STATUS_REGISTER,0x0); //Abfrage Status, um SPI-Transfer zu beenden
  read43x(motor, STATUS_REGISTER,0x0); //Abfrage Status, um SPI-Transfer zu beenden
  read43x(motor, STATUS_REGISTER,0x0); //Abfrage Status, um SPI-Transfer zu beenden
}




