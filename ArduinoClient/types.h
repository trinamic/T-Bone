//general datatypes we use, conveniently in an header file 
struct movement { 
  unsigned char type; //movement to pos or gearing chain
  unsigned char motor;
  unsigned long parameter_1; //gearing or target
  unsigned long parameter_2; //speed
  unsigned long parameter_3; //gearing or target
  unsigned long parameter_4; //speed
};

struct squirrel {
  char cs_pin;
  char target_reached_interrupt_pin;
  char target_reached_interrupt_nr;
  //we have a TMC260 at the end so we configure a configurer
  TMC26XGenerator tmc260;
  int steps_per_revolution;
};


