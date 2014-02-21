unsigned char status;


void writeRegister(unsigned const char cs_squirrel,unsigned const char tmc4361_register, unsigned const long datagram) {
  sendRegister(cs_squirrel,tmc4361_register | 0x80,datagram);
}

unsigned long readRegister(unsigned const char cs_squirrel,unsigned const char tmc4361_register, unsigned const long datagram) {
  sendRegister(cs_squirrel,tmc4361_register, datagram);
  unsigned long result = sendRegister(cs_squirrel, tmc4361_register, datagram);
  return result;
}

long sendRegister(unsigned const char motor_nr, unsigned const char tmc4361_register, unsigned const long datagram) {
  unsigned long i_datagram;

  //select the TMC driver
  if (motor_nr==0) {
    digitalWriteFast(CS_4361_1_PIN,HIGH);
  } 
  else if (motor_nr==1) {
    digitalWriteFast(CS_4361_2_PIN,HIGH);
  }
  //TODO this won't work  but will be the nost successfuly optimization come up with an idea

#ifdef DEBUG_SPI
  if (tmc4361_register & 0x80) {
    Serial.print(F("Writing "));
  } 
  else {
    Serial.print(F("Sending "));
  }
  Serial.print(datagram,HEX);
  Serial.print(F(" to "));
  Serial.print(tmc4361_register & (0x80-1),HEX);
  Serial.print(F(" of #"));
  Serial.println(motor_nr,HEX);
#endif

  //the first value is irgnored
  status = SPI.transfer(tmc4361_register);
  //write/read the values
  i_datagram = SPI.transfer((datagram >> 24) & 0xff);
  i_datagram <<= 8;
  i_datagram |= SPI.transfer((datagram >> 16) & 0xff);
  i_datagram <<= 8;
  i_datagram |= SPI.transfer((datagram >>  8) & 0xff);
  i_datagram <<= 8;
  i_datagram |= SPI.transfer((datagram) & 0xff);

#ifdef DEBUG_SPI
  Serial.print(F("Status :"));
  Serial.println(status,BIN);
  Serial.print(F("Received "));
  Serial.println(i_datagram,HEX);
  Serial.println();
#endif

  //deselect the TMC chip
  if (motor_nr==0) {
    digitalWriteFast(CS_4361_1_PIN,LOW);
  } 
  else if (motor_nr==1) {
    digitalWriteFast(CS_4361_2_PIN,LOW);
  }

  return i_datagram;
}





