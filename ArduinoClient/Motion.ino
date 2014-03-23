volatile boolean next_move_prepared = false;
volatile boolean prepare_shaddow_registers = false;
volatile unsigned int motor_status;
volatile unsigned char target_motor_status;
volatile unsigned char next_target_motor_status;
char direction[nr_of_coordinated_motors];
unsigned char min_buffer_depth = DEFAULT_COMMAND_BUFFER_DEPTH;

void startMotion(char initial_min_buffer_depth) {
  if (initial_min_buffer_depth > DEFAULT_COMMAND_BUFFER_DEPTH) {
    min_buffer_depth = initial_min_buffer_depth;
  } 
  else {
    min_buffer_depth = DEFAULT_COMMAND_BUFFER_DEPTH;
  }
#ifdef DEBUG_MOTION_STATUS
  Serial.println(F("starting motion"));
#endif
  //TODO initialize drivers??
  for (char i=0; i<nr_of_coordinated_motors;i++) {
    pinMode(motors[i].target_reached_interrupt_pin,INPUT);
    digitalWriteFast(motors[i].target_reached_interrupt_pin,LOW);
    attachInterrupt(motors[i].target_reached_interrupt_nr,motors[i].target_reached_interrupt_routine, FALLING);
    writeRegister(i, TMC4361_INTERRUPT_CONFIG_REGISTER, _BV(0) | _BV(1)); //POS_COMP_REACHED or TARGET_REACHED count as target reached
  }
  next_move_prepared=false; //TODO in theory this is not needed  
  prepare_shaddow_registers = false;
  current_motion_state = in_motion;
  target_motor_status=0;
  next_target_motor_status=0;
}

void finishMotion() {
#ifdef DEBUG_MOTION_STATUS
  Serial.println(F("finishing motion"));
#endif
  current_motion_state = finishing_motion;
  min_buffer_depth = 0; //reduce the motionbuffer depth to 0 to completely drain it
}

void resetMotion() {
  current_motion_state = no_motion;
  //empt hte queue
  while (!moveQueue.isEmpty()) {
    moveQueue.pop();
  }
  min_buffer_depth = DEFAULT_COMMAND_BUFFER_DEPTH;
}

void checkMotion() {
  checkTMC5041Motion();

  if (current_motion_state==in_motion || current_motion_state==finishing_motion) {

    if (target_motor_status!=0 && (motor_status & target_motor_status) == target_motor_status) {
      //TODO we need some kind of 'At least here'??
#ifdef DEBUG_MOTION_TRACE
      Serial.println(F("all motors reached target!"));
#endif
#ifdef DEBUG_MOTION_TRACE_SHORT
      Serial.println('#');
#endif
      motor_status=0;
      target_motor_status = next_target_motor_status;
      tmc5041_prepare_next_motion();
      signal_start();
      next_move_prepared=false;
    }

    if (!next_move_prepared) {
#ifdef CALCULATE_OUTPUT
      digitalWriteFast(CALCULATE_OUTPUT,HIGH);
#endif
/*
      This was needed to decode single move queues 
      Serial.println(min_buffer_depth);
      Serial.println(moveQueue.count());
      Serial.println(current_motion_state);
      */
      //we leave a rest in the move queue since it could be a partial movement
      if (moveQueue.count()>0 && (moveQueue.count()>min_buffer_depth || current_motion_state==finishing_motion)) {
        if (min_buffer_depth!=0 && min_buffer_depth>DEFAULT_COMMAND_BUFFER_DEPTH) {
#ifdef DEBUG_MOTION_STATUS
          Serial.println(F("Inital motion buffer full."));
#endif
          min_buffer_depth=DEFAULT_COMMAND_BUFFER_DEPTH;
        }
        //configure the start conditions for the motors
        //TODO - is'nt this more or less something just to be done for the first tqwo moves??
        for (char i=0; i<nr_of_coordinated_motors;i++) {
          //give all motors a nice start config
          if (!prepare_shaddow_registers) {
            writeRegister(i, TMC4361_START_CONFIG_REGISTER, 0
              | _BV(0) //xtarget requires start
            | _BV(1) //vmax requires start
            | _BV(5) //external start is an start
            //TODO is that correct?
            //| _BV(10)//immediate start         
            );   
          } 
          else {
            writeRegister(i, TMC4361_START_CONFIG_REGISTER, 0
              | _BV(0) //x_target requires start
            | _BV(4)  //use shaddow motion profiles
            | _BV(5) //external start is an start
            );   
          }
        }

        byte moving_motors=0;
        //analysze the movement (nad take a look at the next
        movement move = moveQueue.pop();
        //check out which momtors are geared with this

        if (move.type == move_to || move.type== move_over) {
#ifdef DEBUG_MOTION
          Serial.print(F("Moving motor "));
          Serial.print(move.motor,DEC);
          Serial.print(F(" to "));
          Serial.print(move.target);
          Serial.print(F(" at "));
          Serial.println(move.vMax);
#endif
          //TODO this move_over looks really suspicious!
          if (move.motor<nr_of_coordinated_motors) {
            moveMotorTMC4361(move.motor, move.target, move.vMax, move.aMax, move.jerk, prepare_shaddow_registers, move.type==move_over);
            moving_motors |= _BV(move.motor);
          } 
          else {
            moveMotorTMC5041(move.motor-nr_of_coordinated_motors, move.target, move.vMax, move.aMax, prepare_shaddow_registers, move.type==move_over);
            moving_motors |= _BV(nr_of_coordinated_motors);
          }          

          movement follower;
          do {
            follower.type=uninitialized;
            if (!moveQueue.isEmpty()) {
              follower = moveQueue.peek();
              if (follower.type==follow_over || follower.type==follow_to) {  
                moveQueue.pop();
#ifdef DEBUG_MOTION
                Serial.print(F("Following motor "));
                Serial.print(follower.motor,DEC);
                Serial.print(F(" to "));
                Serial.print(follower.target,DEC);
                Serial.print(F(" at "));
                Serial.println(follower.vMax);
#endif
                if (follower.motor<nr_of_coordinated_motors) {
                  moveMotorTMC4361(follower.motor, follower.target, follower.vMax, follower.aMax, follower.jerk, prepare_shaddow_registers, follower.type==follow_over);
                  moving_motors |= _BV(follower.motor);
                } 
                else {
                  moveMotorTMC5041(follower.motor-nr_of_coordinated_motors, follower.target, follower.vMax, follower.aMax, prepare_shaddow_registers, follower.type==follow_over);
                  moving_motors |= _BV(nr_of_coordinated_motors);
                }          
              }
            }
          } 
          while (follower.type == follow_over || follower.type == follow_to);

          //in the end all moviong motorts must have apssed pos_comp
          next_target_motor_status = moving_motors;
          //for the first move we need to configure everything a bit 
          if (!prepare_shaddow_registers) {
            target_motor_status = next_target_motor_status;
            motor_status=0;
            signal_start();
            //and we need to prepare the next move for the shadow registers
            prepare_shaddow_registers = true;
            next_move_prepared = false;
          } 
          else {
            //ok normally we can relax until the enxt start event occured
            next_move_prepared = true;
          }
#ifdef DEBUG_MOTION
          Serial.print(F("Next move : "));
          Serial.println(next_target_motor_status,BIN);
          Serial.println();
#endif
        } 
        else if (move.type == set_position) {
#ifdef DEBUG_SET_POS
          Serial.print(F("Setting motor "));
          Serial.print(move.motor,DEC);
          Serial.print(F(" to position "));
          Serial.print(move.target,DEC);
#endif  
          if (IS_COORDINATED_MOTOR(move.motor)) {
#ifdef DEBUG_SET_POS
            Serial.println(F(" on TMC4361"));
#endif  
            setMotorPositionTMC4361(move.motor,move.target);
          } 
          else {
#ifdef DEBUG_SET_POS
            Serial.print(F(" on TMC5041 as #"));
            Serial.print(move.motor-nr_of_coordinated_motors,DEC);
#endif
            setMotorPositionTMC5041(move.motor-nr_of_coordinated_motors,move.target);
          }
        } 
        else {
          Serial.print(F("ignored move"));
        }
#ifdef CALCULATE_OUTPUT
        digitalWriteFast(CALCULATE_OUTPUT,LOW);
#endif
      } 
      else if (current_motion_state==finishing_motion) {
        current_motion_state=no_motion;
      } 
      else {
#ifdef DEBUG_MOTION_STATUS
        //TODO come up with a sensbilde warning
        // Serial.println(F("Move Queue emptied!"));
#endif
        //TODO should we react in any way to it?
      }
    }
  }
}

void motor_1_target_reached() {
  motor_target_reached(0);
}

void motor_2_target_reached() {
  motor_target_reached(1);
}

void motor_3_target_reached() {
  motor_target_reached(2);
}

inline void motor_target_reached(char motor_nr) {
  if (in_motion) {
    //clear the event register
    //read43x(motors[motor_nr].cs_pin,EVENTS_REGISTER,0);
    //and write down which motor touched the target
    motor_status |= _BV(motor_nr);
#ifdef DEBUG_MOTION_TRACE
    Serial.print(F("Motor "));
    Serial.print(motor_nr,DEC);
    Serial.print(F(" reached target! At "));
    Serial.print(motor_status,BIN);
    Serial.print(F(" of "));
    Serial.println(target_motor_status,BIN);
#endif
#ifdef DEBUG_MOTION_TRACE_SHORT
    Serial.print(motor_nr,DEC);
    Serial.print('/');
    Serial.print(motor_status,BIN);
    Serial.print('>');
    Serial.println(target_motor_status,BIN);
#endif
  }
}





































































