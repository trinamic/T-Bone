
void send43x(unsigned char tmc43x_register, unsigned long datagram) {
  unsigned long i_datagram;

  //select the TMC driver
  digitalWrite(cs_squirrel,LOW);


#ifdef DEBUG
  Serial.print("Sending ");
  Serial.println(datagram,HEX);
#endif

  //the first value is irgnored
  SPI.transfer(tmc43x_register);
  //write/read the values
  i_datagram = SPI.transfer((datagram >> 24) & 0xff);
  i_datagram <<= 8;
  i_datagram = SPI.transfer((datagram >> 16) & 0xff);
  i_datagram <<= 8;
  i_datagram |= SPI.transfer((datagram >>  8) & 0xff);
  i_datagram <<= 8;
  i_datagram |= SPI.transfer((datagram) & 0xff);

#ifdef DEBUG
  Serial.print("Received ");
  Serial.println(i_datagram,HEX);
#endif
  //deselect the TMC chip
  digitalWrite(cs_squirrel,HIGH); 
}

