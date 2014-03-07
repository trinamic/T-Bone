//what kind of information is in the queue
enum movement_type {
  move_to,
  follow_to,
  move_over,
  follow_over, 
  uninitialized
};

//there is more to motion than ture and false
enum motion_state {
  no_motion,
  in_motion,
  finishing_motion
};

//general datatypes we use, conveniently in an header file 
struct movement { 
  movement_type type; //movement to pos or gearing chain
  unsigned char motor;
  long target;
  double vMax;
  double aMax;
  long jerk;
};

struct TMC4361_info{
  char target_reached_interrupt_pin;
  char target_reached_interrupt_nr;
  void (*target_reached_interrupt_routine)();
  //we have a TMC260 at the end so we configure a configurer
  TMC26XGenerator tmc260;
  int steps_per_revolution;
};

struct TMC5041_motion_info {
  long target;
  double vMax;
  double aMax;
};

enum endstop_position {
  left=1,
  right=2,
  both=3,
  none=0
};

struct endstop_config {
endstop_position position: 
  2;
unsigned char positive_edge:
  1;
};

