//general datatypes we use, conveniently in an header file 
struct movement { 
  unsigned char type; //movement to pos or gearing chain
  unsigned char motor;
  unsigned long parameter_1; //gearing or target
  unsigned long parameter_2; //speed
}
