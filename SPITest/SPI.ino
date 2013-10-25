unsigned char status;


void write43x(unsigned char tmc43x_register, unsigned long datagram) {
  send43x(tmc43x_register,datagram,true);
}

void send43x(unsigned char tmc43x_register, unsigned long datagram, boolean write_access) {
  unsigned long i_datagram;

  //select the TMC driver
  digitalWrite(cs_squirrel,LOW);

  if (write_access) {
    tmc43x_register |= 0x80;
  }  


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
  //deselect the TMC chip
  digitalWrite(cs_squirrel,HIGH); 
}

set260Register(unsigned long value) {
  //santitize to 20 bits 
  value &= 0xFFFFF;
  send43x(0xEC,value, false);  //Cover-Register: Einstellung des SMARTEN=aus

  send43x(0x0e,0x0,false); //Abfrage Status, um SPI-Transfer zu beenden
  send43x(0x0e,0x0,false); //Abfrage Status, um SPI-Transfer zu beenden
  send43x(0x0e,0x0,false); //Abfrage Status, um SPI-Transfer zu beenden
}



