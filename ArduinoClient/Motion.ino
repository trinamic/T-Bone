volatile boolean is_running = false;

void startMotion() {
  in_motion = true;
  is_running=false; //TODO in theory this is not needed  
  //TODO initialize drivers??
}

void stopMotion() {
  in_motion = false;
}

void checkMotion() {
  if (in_motion && !is_running) {
    if (moveQueue.count()>0) {

      //analysze the movement (nad take a look at the next
      movement move = moveQueue.pop();
      movement gearings[MAX_GEARED_MOTORS];
#ifdef DEBUG_MOTION
      Serial.print(F("Moving motor "));
      Serial.print(move.motor,DEC);
      Serial.print(F(" to "));
      Serial.print(move.data.move.target);
      Serial.print(F(" at "));
      Serial.print(move.data.move.vmax);
#endif
      char moved_motor = move.motor;
      //check out which momtors are geared with this
      int gearingscount = 0;
      do {
        gearings[gearingscount] = moveQueue.peek();
        if (gearings[gearingscount].type==gearmotor) {  
          moveQueue.pop();
          gearingscount++;
        }
      } 
      while (gearings[gearingscount].type == gearmotor);
      //ppek a look if the next movbement will use the same gearing configuration
      boolean nextMotorWillBeSame = false;
      if (!moveQueue.isEmpty()) {
        movement nextMove = moveQueue.peek();
        char nextMotor = nextMove.motor;
        nextMotorWillBeSame = (nextMotor==moved_motor);
      }

      byte geared_motors=0;
      for (char i=0;i<gearingscount;i++) {
#ifdef DEBUG_MOTION
        Serial.print(F(", gearing motor "));
        Serial.print(gearings[i].motor,DEC);
        Serial.print(F(" by "));
        Serial.print(gearings[i].data.follow.gearing,DEC);
#endif
        char geared_motor = gearings[i].motor;
        //all motors mentioned here are configured
        float gear_ratio = gearings[i].data.follow.gearing;
        write43x(motors[geared_motor].cs_pin, GEAR_RATIO_REGISTER,FIXED_8_24_MAKE(gear_ratio));
        geared_motor |= _BV(i);
      }
      for (char i; i<nr_of_motors;i++) {
        if (i!=moved_motor) {
          write43x(motors[i].cs_pin, GENERAL_CONFIG_REGISTER, _BV(6)); //direct values and no clock
          write43x(motors[i].cs_pin, START_CONFIG_REGISTER, 0
            | _BV(3) //gear ration cahnge requries start
          | _BV(0) //xtarget requires start
          | _BV(1) //vmax requires start
          | _BV(5) //external start is an start
          | _BV(10)//immediate start
          );  //and due to a bug we NEED an internal start signal 
          if (geared_motors && _BV(i) == 0) {
            //unconfigure non geared motors
            write43x(motors[i].cs_pin, GEAR_RATIO_REGISTER,FIXED_8_24_MAKE(0));
          }
        }
      }

      //finally configure the running motor

#ifdef DEBUG_MOTION
      if (nextMotorWillBeSame) {
        Serial.println(F(" - next will be same motor"));
      }
      Serial.println();
#endif

      write43x(motors[moved_motor].cs_pin, GENERAL_CONFIG_REGISTER, 0); //we use direct values
      write43x(motors[moved_motor].cs_pin, V_MAX_REGISTER,FIXED_24_8_MAKE(move.data.move.vmax)); //set the velocity - TODO recalculate float numbers
      write43x(motors[moved_motor].cs_pin, START_CONFIG_REGISTER, 0
        | _BV(0) //xtarget requires start
      | _BV(1) //vmax requires start
      | _BV(5) //external start is an start
      | _BV(10)//immediate start
      );   

      //register the interrupt handler for this motor
      attachInterrupt(motors[moved_motor].target_reached_interrupt_nr , target_reached_handler, RISING);
      //ok we know that we are running
      is_running = true;
      //and finyll write the target to initiate the movement
      write43x(motors[moved_motor].cs_pin, X_TARGET_REGISTER,move.data.move.target);
      //and carefully trigger the start pin 
      digitalWrite(start_signal_pin,HIGH);
      pinMode(start_signal_pin,OUTPUT);
      digitalWrite(start_signal_pin,LOW);
      pinMode(start_signal_pin,INPUT);
    } 
    else {
      //we are finished here
    }
  }
}


void target_reached_handler() {
  is_running=false;
}



















