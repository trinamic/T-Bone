//what kind of information is in the queue
enum movement_type {
  movemotor,
  followmotor,
};

//general datatypes we use, conveniently in an header file 
struct movement { 
  movement_type type; //movement to pos or gearing chain
  unsigned char motor;
  union data {
    struct {
      long target;
      double vmax;
    } 
    move;
    struct {
      double factor;
    } 
    follow;
  } 
  data;
};

struct squirrel {
  char cs_pin;
  char target_reached_interrupt_pin;
  char target_reached_interrupt_nr;
  //we have a TMC260 at the end so we configure a configurer
  TMC26XGenerator tmc260;
  int steps_per_revolution;
  float aMax;
  float dMax;
  long startBow;
  long endBow;
};


