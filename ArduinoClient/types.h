//what kind of information is in the queue

#define MOVING_MOTOR _BV(1)
#define MOVING_OVER _BV(2)

enum movement_type {
  move_to = MOVING_MOTOR | MOVING_OVER,
  follow_to = MOVING_OVER ,
  move_over = MOVING_MOTOR,
  follow_over = 0
};

//general datatypes we use, conveniently in an header file 
struct movement { 
  movement_type type; //movement to pos or gearing chain
  unsigned char motor;
  long target;
  double vMax;
  long aMax;
  long dMax;
  long startBow;
  long endBow;
};

struct squirrel {
  char cs_pin;
  char target_reached_interrupt_pin;
  char target_reached_interrupt_nr;
  void (*target_reached_interrupt_routine)();
  //we have a TMC260 at the end so we configure a configurer
  TMC26XGenerator tmc260;
  int steps_per_revolution;
};




