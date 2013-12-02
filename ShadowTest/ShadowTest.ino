#include <SPI.h>
#include <TMC26XGenerator.h>
#include <Metro.h>

//#define DEBUG

//simple FP math see https://ucexperiment.wordpress.com/2012/10/28/fixed-point-math-on-the-arduino-platform/
#define FIXED_8_24_MAKE(a)     (int32_t)((a*(1ul << 24ul)))
#define FIXED_24_8_MAKE(a)     (int32_t)((a*(1ul << 8ul)))

//config
unsigned char steps_per_revolution = 200;
unsigned int current_in_ma = 500;
unsigned long vmax = 20ul*256ul;
unsigned long bow = vmax*1000;
unsigned long end_bow = bow;
unsigned long amax = vmax*1000;
unsigned long dmax = amax;

//register
#define GENERAL_CONFIG_REGISTER 0x0
#define START_CONFIG_REGISTER 0x2
#define INPUT_FILTER_REGISTER 0x3
#define SPIOUT_CONF_REGISTER 0x04
#define STEP_CONF_REGISTER 0x0A
#define EVENT_CLEAR_CONF_REGISTER 0x0c
#define INTERRUPT_CONFIG_REGISTER 0x0d
#define EVENTS_REGISTER 0x0e
#define STATUS_REGISTER 0x0f
#define START_OUT_ADD_REGISTER 0x11
#define GEAR_RATIO_REGISTER 0x12
#define START_DELAY_REGISTER 0x13
#define RAMP_MODE_REGISTER 0x20
#define X_ACTUAL_REGISTER 0x21 
#define V_ACTUAL_REGISTER 0x22
#define V_MAX_REGISTER 0x24
#define V_START_REGISTER 0x25
#define V_STOP_REGISTER 0x26
#define A_MAX_REGISTER 0x28
#define D_MAX_REGISTER 0x29
#define BOW_1_REGISTER 0x2d
#define BOW_2_REGISTER 0x2e
#define BOW_3_REGISTER 0x2f
#define BOW_4_REGISTER 0x30
#define CLK_FREQ_REGISTER 0x31
#define POS_COMP_REGISTER 0x32
#define X_TARGET_REGISTER 0x37
#define X_TARGET_PIPE_0_REGSISTER 0x38
#define SH_RAMP_MODE_REGISTER 0x40
#define SH_V_MAX_REGISTER 0x41
#define SH_V_START_REGISTER 0x42
#define SH_V_STOP_REGISTER 0x43
#define SH_VBREAK_REGISTER 0x44
#define SH_A_MAX_REGISTER 0x45
#define SH_D_MAX_REGISTER 0x46
#define SH_BOW_1_REGISTER 0x49
#define SH_BOW_2_REGISTER 0x4a
#define SH_BOW_3_REGISTER 0x4b
#define SH_BOW_4_REGISTER 0x4c
#define COVER_LOW_REGISTER 0x6c
#define COVER_HIGH_REGISTER 0x6d
#define START_OUT_ADD_REGISTER 0x11

//values
#define TMC_26X_CONFIG_SPI 0x8440000a //SPI-Out: block/low/high_time=8/4/4 Takte; CoverLength=autom; TMC26x
#define TMC_26X_CONFIG_SD 0x8440000b //SPI-Out: block/low/high_time=8/4/4 Takte; CoverLength=autom; TMC26x
#define TMC260_SENSE_RESISTOR_IN_MO 150
#define CLOCK_FREQUENCY 16000000ul

//we have a TMC260 at the end so we configure a configurer
TMC26XGenerator tmc260 = TMC26XGenerator(current_in_ma,TMC260_SENSE_RESISTOR_IN_MO);

//a metro to control the movement
Metro moveMetro = Metro(5000ul);
Metro checkMetro = Metro(1000ul);

int squirrel_a = 8;
int interrupt_a = 3;
int pos_com_reached_interrupt_a=0;

int squirrel_b = 12;
int interrupt_b = 2;
int reset_squirrel = 4;
int pos_com_reached_interrupt_b=1;
int start_signal_pin = 7;


void setup() {
  //reset the quirrel
  pinMode(reset_squirrel,OUTPUT);
  digitalWrite(reset_squirrel, LOW);
  delay(10);
  digitalWrite(reset_squirrel, HIGH);
  pinMode(squirrel_a,OUTPUT);
  digitalWrite(squirrel_a,HIGH);
  pinMode(squirrel_b,OUTPUT);
  digitalWrite(squirrel_b,HIGH);
  //configure the interrupt routines
  pinMode(interrupt_a,INPUT);
  digitalWrite(interrupt_a,LOW);
  pinMode(interrupt_b,INPUT);
  digitalWrite(interrupt_b,LOW);
  pinMode(start_signal_pin,OUTPUT);
  digitalWrite(start_signal_pin,LOW);
  //initialize the serial port for debugging
  Serial.begin(9600);
  //initialize SPI
  SPI.begin();
  //configure the TMC26x A
  write43x(squirrel_a, GENERAL_CONFIG_REGISTER, _BV(0) | _BV(1)); //we use direct values
  write43x(squirrel_a, CLK_FREQ_REGISTER,CLOCK_FREQUENCY);
  write43x(squirrel_a, START_CONFIG_REGISTER, 0
    | _BV(0) //bug
  | _BV(1) //vmax requires start
  | _BV(5) //external start is an start
  | _BV(10)//immediate start
  );   

  write43x(squirrel_a, RAMP_MODE_REGISTER,_BV(2) | 2); //we want to go to positions in nice S-Ramps ()TDODO does not work)
  write43x(squirrel_a, SH_RAMP_MODE_REGISTER,_BV(2) | 2); //we want to go to positions in nice S-Ramps ()TDODO does not work)
  write43x(squirrel_a, BOW_1_REGISTER,bow);
  write43x(squirrel_a, BOW_2_REGISTER,end_bow);
  write43x(squirrel_a, BOW_3_REGISTER,end_bow);
  write43x(squirrel_a, BOW_4_REGISTER,bow);
  write43x(squirrel_a, SH_BOW_1_REGISTER,bow);
  write43x(squirrel_a, SH_BOW_2_REGISTER,end_bow);
  write43x(squirrel_a, SH_BOW_3_REGISTER,end_bow);
  write43x(squirrel_a, SH_BOW_4_REGISTER,bow);
  write43x(squirrel_a, A_MAX_REGISTER,amax); //set maximum acceleration
  write43x(squirrel_a, D_MAX_REGISTER,dmax); //set maximum deceleration
  write43x(squirrel_a, SH_A_MAX_REGISTER,amax); //set maximum acceleration
  write43x(squirrel_a, SH_D_MAX_REGISTER,dmax); //set maximum deceleration
  //configure the motor type
  unsigned long motorconfig = 0x00; //we want 256 microsteps
  motorconfig |= steps_per_revolution<<4;

  write43x(squirrel_a, STEP_CONF_REGISTER,motorconfig);
  tmc260.setMicrosteps(256);
  write43x(squirrel_a, SPIOUT_CONF_REGISTER,TMC_26X_CONFIG_SD);
  set260Register(squirrel_a, tmc260.getDriverControlRegisterValue());
  set260Register(squirrel_a, tmc260.getChopperConfigRegisterValue());
  set260Register(squirrel_a, tmc260.getStallGuard2RegisterValue());
  set260Register(squirrel_a, tmc260.getDriverConfigurationRegisterValue());

  //configure the TMC26x B
  write43x(squirrel_b, GENERAL_CONFIG_REGISTER, _BV(0) | _BV(1)); //direct values and no clock
  write43x(squirrel_b, CLK_FREQ_REGISTER,CLOCK_FREQUENCY);
  write43x(squirrel_b, START_CONFIG_REGISTER, 0
    | _BV(0) //xstart requires start
  | _BV(1) //vmax requires start
  | _BV(5) //external start is an start
  | _BV(10)//immediate start
  );

  write43x(squirrel_b, RAMP_MODE_REGISTER,_BV(2) | 2); //we want to go to positions in nice S-Ramps ()TDODO does not work)
  write43x(squirrel_b, SH_RAMP_MODE_REGISTER,_BV(2) | 2); //we want to go to positions in nice S-Ramps ()TDODO does not work)
  write43x(squirrel_b, BOW_1_REGISTER,bow);
  write43x(squirrel_b, BOW_2_REGISTER,end_bow);
  write43x(squirrel_b, BOW_3_REGISTER,end_bow);
  write43x(squirrel_b, BOW_4_REGISTER,bow);
  write43x(squirrel_b, SH_BOW_1_REGISTER,bow);
  write43x(squirrel_b, SH_BOW_2_REGISTER,end_bow);
  write43x(squirrel_b, SH_BOW_3_REGISTER,end_bow);
  write43x(squirrel_b, SH_BOW_4_REGISTER,bow);
  write43x(squirrel_b, A_MAX_REGISTER,amax); //set maximum acceleration
  write43x(squirrel_b, D_MAX_REGISTER,dmax); //set maximum deceleration
  write43x(squirrel_b, SH_A_MAX_REGISTER,amax); //set maximum acceleration
  write43x(squirrel_b, SH_D_MAX_REGISTER,dmax); //set maximum deceleration

  //configure the motor type
  motorconfig = 0x00; //we want 256 microsteps
  motorconfig |= steps_per_revolution<<4;

  write43x(squirrel_b, STEP_CONF_REGISTER,motorconfig);
  tmc260.setMicrosteps(256);
  write43x(squirrel_b, SPIOUT_CONF_REGISTER,TMC_26X_CONFIG_SD);
  set260Register(squirrel_b, tmc260.getDriverControlRegisterValue());
  set260Register(squirrel_b, tmc260.getChopperConfigRegisterValue());
  set260Register(squirrel_b, tmc260.getStallGuard2RegisterValue());
  set260Register(squirrel_b, tmc260.getDriverConfigurationRegisterValue());

  write43x(squirrel_a, START_OUT_ADD_REGISTER,100ul*CLOCK_FREQUENCY); //set maximum acceleration
  write43x(squirrel_b, START_OUT_ADD_REGISTER,100ul*CLOCK_FREQUENCY); //set maximum acceleration

  write43x(squirrel_a,START_DELAY_REGISTER, 256); //NEEDED so THAT THE SQUIRREL CAN RECOMPUTE EVERYTHING!
  write43x(squirrel_b,START_DELAY_REGISTER, 256);

  //attach our pos comp handlers to the internal interrupts and configure them
  attachInterrupt(pos_com_reached_interrupt_a,squirrel_a_pos_comp_reached,FALLING);
  write43x(squirrel_a, INTERRUPT_CONFIG_REGISTER,_BV(1));
  attachInterrupt(pos_com_reached_interrupt_b,squirrel_b_pos_comp_reached,FALLING);
  write43x(squirrel_b, INTERRUPT_CONFIG_REGISTER,_BV(1));

  delay(3000ul);
}

unsigned long tmc43xx_write;
unsigned long tmc43xx_read;

volatile boolean toMove = true;
boolean isMoving =false;

long prev_target = 0;
long target= 0;

unsigned long prev_v = 0;
unsigned long this_v = 0;
unsigned long next_v =vmax+random(10)*vmax;
long dir = 0;

long next_pos_comp_a;
long next_pos_comp_b;

char all_targets_reached = 0;
#define SQUIRREL_A_TARGET_REACHED 0
#define SQUIRREL_B_TARGET_REACHED 1
#define ALL_TARGETS_REACHED 3


void loop() {
  if (toMove || !isMoving) {
    prev_target = target;
    target = random(1000ul)*100ul;

    next_v = random(10)*vmax+1;

    float gear_ratio = (float)random(101)/100.0;

    next_pos_comp_a = target;
    unsigned long drive_target = calculate_x_target(prev_target,target); //ok let's try to drive twice as far

    long geared_target = target*gear_ratio;
    next_pos_comp_b = geared_target;
    long geared_drive_target =  calculate_x_target(prev_target*gear_ratio, geared_target);
    unsigned long geared_speed = next_v*gear_ratio;

    if (isMoving){
      toMove=false;

      write43x(squirrel_a, X_TARGET_REGISTER,drive_target);
      write43x(squirrel_a, SH_V_MAX_REGISTER, FIXED_24_8_MAKE(next_v)); //set the velocity - TODO recalculate float numbers

      write43x(squirrel_b, X_TARGET_REGISTER,geared_drive_target);
      write43x(squirrel_b, SH_V_MAX_REGISTER, FIXED_24_8_MAKE(geared_speed)); //set the velocity - TODO recalculate float numbers

    } 
    else {

      isMoving=true;

      toMove=true;
      write43x(squirrel_a, POS_COMP_REGISTER,target);
      write43x(squirrel_a, X_TARGET_REGISTER,drive_target);
      write43x(squirrel_a, V_MAX_REGISTER, FIXED_24_8_MAKE(next_v)); //set the velocity - TODO recalculate float numbers

      write43x(squirrel_b, POS_COMP_REGISTER,geared_target);
      write43x(squirrel_b, X_TARGET_REGISTER,geared_drive_target);
      write43x(squirrel_b, V_MAX_REGISTER, FIXED_24_8_MAKE(geared_speed)); //set the velocity - TODO recalculate float numbers

      signal_start();

      const unsigned long second_start= 0
        | _BV(0) //x_target
        //| _BV(1) //v_max requires start
        | _BV(4)  //use shaddow motion profiles
          | _BV(5) //external start is an start
            //  | _BV(8)  //poscomp reached triggers start event
            // not neeeded since no external start            | _BV(10) //immediate start
            //     | _BV(11)  // the shaddow registers cycle
            //  | _BV(13)  // coordinate yourself with busy starts
            ;


      write43x(squirrel_a, START_CONFIG_REGISTER, second_start);   
      write43x(squirrel_b, START_CONFIG_REGISTER, second_start);   


    }

    Serial.print("Move to ");
    Serial.print(target);
    Serial.print(" with ");
    Serial.println(next_v);
    Serial.print("Gear ration: ");
    Serial.println(gear_ratio);
    Serial.println(); 


  }
  if (checkMetro.check()) {
    long position;
    Serial.print("X A: ");
    position = read43x(squirrel_a,X_ACTUAL_REGISTER,0);
    Serial.print(position);
    Serial.print(" -> ");
    position  = read43x(squirrel_a,X_TARGET_REGISTER,0);
    Serial.println(position);

    unsigned long velocity;
    Serial.print("V A: ");
    velocity = read43x(squirrel_a,V_ACTUAL_REGISTER,0);
    Serial.print(velocity);
    Serial.print(" -> ");
    velocity  = read43x(squirrel_a,V_MAX_REGISTER,0) >> 8;
    Serial.println(velocity);

    Serial.print("X B: ");
    position = read43x(squirrel_b,X_ACTUAL_REGISTER,0);
    Serial.print(position);
    Serial.print(" -> ");
    position  = read43x(squirrel_b,X_TARGET_REGISTER,0);
    Serial.println(position);

    Serial.print("V B: ");
    velocity = read43x(squirrel_b,V_ACTUAL_REGISTER,0);
    Serial.print(velocity);
    Serial.print(" -> ");
    velocity  = read43x(squirrel_b,V_MAX_REGISTER,0) >> 8;
    Serial.println(velocity);

    Serial.println();
  }
}

void squirrel_a_pos_comp_reached() {
  all_targets_reached |= _BV(SQUIRREL_A_TARGET_REACHED);
  if (all_targets_reached == ALL_TARGETS_REACHED) {
    initialise_new_targets(); 
  }
}

void squirrel_b_pos_comp_reached() {  
  all_targets_reached |= _BV(SQUIRREL_B_TARGET_REACHED);
  if (all_targets_reached == ALL_TARGETS_REACHED) {
    initialise_new_targets(); 
  }
}

void initialise_new_targets() {
  Serial.println("Next");
  //set the new pos_comp values
  write43x(squirrel_a,POS_COMP_REGISTER,next_pos_comp_a);
  write43x(squirrel_b,POS_COMP_REGISTER,next_pos_comp_b);
  //and we start again
  all_targets_reached=0;
  //clear the squirrel events
  read43x(squirrel_a,EVENTS_REGISTER,0);
  read43x(squirrel_b,EVENTS_REGISTER,0);
  //and start again()
  signal_start();
  //we need a new position
  toMove=true;
}

inline void signal_start() {
  digitalWrite(start_signal_pin,HIGH);

  digitalWrite(start_signal_pin,LOW);
}

inline long calculate_x_target(long from, long to) {
  return 2l*(to-from)+from;
}







