volatile boolean next_move_prepared = false;
volatile boolean prepare_shaddow_registers = false;

void startMotion() {
  in_motion = true;
  next_move_prepared=false; //TODO in theory this is not needed  
  prepare_shaddow_registers = false;
  //TODO initialize drivers??
}

void stopMotion() {
  in_motion = false;
}

void checkMotion() {
  if (in_motion && !next_move_prepared) {
    if (moveQueue.count()>0) {
      //analysze the movement (nad take a look at the next
      movement move = moveQueue.pop();
      movement followers[MAX_FOLLOWING_MOTORS];
#ifdef DEBUG_MOTION
      Serial.print(F("Moving motor "));
      Serial.print(move.motor,DEC);
      Serial.print(F(" to "));
      Serial.print(move.data.move.target);
      Serial.print(F(" at "));
      Serial.print(move.data.move.vmax);
#endif
      char moved_motor = move.motor;
      //TODO configure the motor so that it - uhm - moves
      //check out which momtors are geared with this
      int following_motors_count = 0;
      do {
        followers[following_motors_count] = moveQueue.peek();
        if (followers[following_motors_count].type==followmotor) {  
          moveQueue.pop();
          following_motors_count++;
        }
      } 
      while (followers[following_motors_count].type == followmotor);

      byte following_motors=0;
      for (char i=0;i<following_motors_count;i++) {
#ifdef DEBUG_MOTION
        Serial.print(F(", following motor "));
        Serial.print(followers[i].motor,DEC);
        Serial.print(F(" by "));
        Serial.print(followers[i].data.follow.factor,DEC);
#endif
        char following_motor = followers[i].motor;
        //all motors mentioned here are configured
        float follow_factor = followers[i].data.follow.factor;
        //TODO compute and write all the values for amax/dmax/bow1-4
        following_motors |= _BV(i);
      }
      for (char i; i<nr_of_motors;i++) {
        if (following_motors && _BV(i) == 0) {
          //TODO configure other to stop
        }
      }
      //finally configure the running motor

      boolean send_start=false;
      write43x(motors[moved_motor].cs_pin, GENERAL_CONFIG_REGISTER, 0); //we use direct values
      if (!prepare_shaddow_registers) {
        write43x(motors[moved_motor].cs_pin, V_MAX_REGISTER,FIXED_24_8_MAKE(move.data.move.vmax)); //set the velocity - TODO recalculate float numbers
        write43x(motors[moved_motor].cs_pin, START_CONFIG_REGISTER, 0
          | _BV(0) //xtarget requires start
        | _BV(1) //vmax requires start
        | _BV(5) //external start is an start
        | _BV(10)//immediate start
        );   
        //if the next move is in the same direction prepare the shadow registers
        prepare_shaddow_registers = true; //TODO this is only trtue if ... there is something left in the queue??  
        //we need to generate a start event
        send_start=true;
      } 
      else {
        write43x(motors[moved_motor].cs_pin, V_MAX_REGISTER,FIXED_24_8_MAKE(move.data.move.vmax)); //set the velocity - TODO recalculate float numbers
        write43x(motors[moved_motor].cs_pin, START_CONFIG_REGISTER, 0
          | _BV(0) //from now on listen to your own start signal
        | _BV(4)  //use shaddow motion profiles
        | _BV(5)  //target reached triggers start event
        | _BV(6)  //target reached triggers start event
        | _BV(10) //immediate start
        | _BV(11)  // the shaddow registers cycle
        );   
        //if the next move is in the same direction prepare the shadow registers
        prepare_shaddow_registers = true;  
        next_move_prepared = true;
      }
      //and write the target 
      write43x(motors[moved_motor].cs_pin, X_TARGET_REGISTER,move.data.move.target);

      //register the interrupt handler for this motor
      attachInterrupt(motors[moved_motor].target_reached_interrupt_nr , target_reached_handler, RISING);

      if (send_start) {
        //and carefully trigger the start pin 
        digitalWrite(start_signal_pin,HIGH);
        pinMode(start_signal_pin,OUTPUT);
        digitalWrite(start_signal_pin,LOW);
        pinMode(start_signal_pin,INPUT);
      }
    } 
    else {
      //we are finished here
    }
  }
}


void target_reached_handler() {
  next_move_prepared=false;
}




































