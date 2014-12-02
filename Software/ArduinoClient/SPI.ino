unsigned char spi_status;


void writeRegister(unsigned const char cs_squirrel,unsigned const char the_register, unsigned const long datagram) {
  sendRegister(cs_squirrel,the_register | 0x80, datagram);
//  Serial1.print("wR ");  Serial1.print(cs_squirrel,HEX);   Serial1.print(": (0x");   Serial1.print(the_register,HEX);   Serial1.print(") <= 0x");   Serial1.println(datagram,HEX);
}

unsigned long readRegister(unsigned const char cs_squirrel,unsigned const char the_register) {
  sendRegister(cs_squirrel,the_register, 0);
  unsigned long result = sendRegister(cs_squirrel, the_register & 0x7F, 0);
//  Serial1.print("rR ");  Serial1.print(cs_squirrel,HEX);   Serial1.print(": (0x");   Serial1.print(the_register,HEX);   Serial1.print(") = 0x");   Serial1.println(result,HEX);
  return result;
}

long sendRegister(unsigned const char motor_nr, unsigned const char the_register, unsigned const long datagram) {
  unsigned long i_datagram;

  //select the TMC driver
  switch(motor_nr) {
  case 0:
    digitalWriteFast(CS_4361_1_PIN,HIGH);
    break;
  case 1:
    digitalWriteFast(CS_4361_2_PIN,HIGH);
    break;
  case 2:
    digitalWriteFast(CS_4361_3_PIN,HIGH);
    break;
  case TMC5041_MOTORS:
    digitalWriteFast(CS_5041_PIN,HIGH);
    break;
  default:
    return 0; //jsut do nothing
  }

#ifdef DEBUG_SPI
  if (the_register & 0x80) {
    Serial.print(F("Writing "));
  } 
  else {
    Serial.print(F("Sending "));
  }
  Serial.print(datagram,HEX);
  Serial.print(F(" to "));
  Serial.print(the_register & (0x80-1),HEX);
  Serial.print(F(" of #"));
  Serial.println(motor_nr,HEX);
#endif

  //the first value is irgnored
  spi_status = SPI.transfer(the_register);
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
  Serial.println(spi_status,BIN);
  Serial.print(F("Received "));
  Serial.println(i_datagram,HEX);
  Serial.println();
#endif

  //deselect the TMC chip
  switch(motor_nr) {
  case 0:
    digitalWriteFast(CS_4361_1_PIN,LOW);
    break;
  case 1:
    digitalWriteFast(CS_4361_2_PIN,LOW);
    break;
  case 2:
    digitalWriteFast(CS_4361_3_PIN,LOW);
    break;
  case TMC5041_MOTORS:
    digitalWriteFast(CS_5041_PIN,LOW);
    break;
  }

  return i_datagram;
}
