extern volatile boolean enable_start;
extern volatile boolean is_homing;

char active_motors;

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
  active_motors = 0;
  for (char i=0; i<nr_of_coordinated_motors;i++) {
    pinMode(motors[i].target_reached_interrupt_pin,INPUT);
    digitalWrite(motors[i].target_reached_interrupt_pin,LOW);
    attachInterrupt(motors[i].target_reached_interrupt_nr,motors[i].target_reached_interrupt_routine, FALLING);
    writeRegister(i, TMC4361_INTERRUPT_CONFIG_REGISTER, _BV(0));// TARGET_REACHED count as target reached
  }
  next_move_prepared=false; //TODO in theory this is not needed
  current_motion_state = in_motion;
//  target_motor_status=0;
//  next_target_motor_status=0;
  move_executing = false;
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
  boolean debug_sent = false;
  char motor_number;
  unsigned long motor_trg;
  unsigned long motor_pos;
  
  unsigned int countdown = 0;
//  unsigned long vact;

//  checkTMC5041Motion();
//  checkTMC4361Motion();
  if (current_motion_state==in_motion || current_motion_state==finishing_motion) {

    // check motor positions
    for (motor_number = 0; motor_number < (nr_of_coordinated_motors+2); motor_number++) {
      if ((active_motors & (1<<motor_number))) { // motor is still marked as active
        if (motor_number < nr_of_coordinated_motors) { // TMC4361 axis
//          motor_trg = readRegister(motor_number, TMC4361_X_TARGET_REGISTER);
//          motor_pos = readRegister(motor_number, TMC4361_X_ACTUAL_REGISTER);
          motor_trg = 0x01; // target: velocity == 0 (Bit 4:3), TARGET_REACHED == 1(Bit 0)
          motor_pos = readRegister(motor_number, TMC4361_STATUS_REGISTER) & ((1<<4)|(1<<3)|(1<<0));
//          vact = readRegister(motor_number, TMC4361_V_ACTUAL_REGISTER);
/*    
          Serial.print(motor_number,DEC);
          Serial.print(';');
          Serial.print(motor_pos);
          Serial.print(';');
          Serial.print(motor_trg);
          Serial.print(';');
          Serial.print(readRegister(motor_number, TMC4361_V_MAX_REGISTER));
          Serial.print(';');
          Serial.print(readRegister(motor_number, TMC4361_SH_V_MAX_REGISTER));
          Serial.print(';');
          Serial.print(readRegister(motor_number, TMC4361_V_STOP_REGISTER));
    
          Serial.println(';');
*/  
        } else if (motor_number == nr_of_coordinated_motors) { // first TMC5041 axis
          motor_trg = readRegister(nr_of_coordinated_motors, TMC5041_X_TARGET_REGISTER_1);
          motor_pos = readRegister(nr_of_coordinated_motors, TMC5041_X_ACTUAL_REGISTER_1);
//          vact = readRegister(nr_of_coordinated_motors, TMC5041_V_ACTUAL_REGISTER_1);
        } else if (motor_number == (nr_of_coordinated_motors+1)) { // second TMC5041 axis
          motor_trg = readRegister(nr_of_coordinated_motors, TMC5041_X_TARGET_REGISTER_2);
          motor_pos = readRegister(nr_of_coordinated_motors, TMC5041_X_ACTUAL_REGISTER_2);
//          vact = readRegister(nr_of_coordinated_motors, TMC5041_V_ACTUAL_REGISTER_2);
        }
//        if ((motor_pos == motor_trg) /*&& (vact == 0)*/) {
        if ((motor_pos == motor_trg) && (active_motors & (1<<motor_number))) {
          active_motors &= ~(1<<motor_number);
          
        } else {
          active_motors |=  (1<<motor_number);
        }
      }
    }

    
    if ((next_move_prepared) && (active_motors == 0)) {
      tmc5041_prepare_next_motion();
      signal_start();
      next_move_prepared=false;
    }

    if (!next_move_prepared) {
      if (moveQueue.count()>0 && (moveQueue.count()>min_buffer_depth || current_motion_state==finishing_motion)) {
        if (min_buffer_depth!=0 && min_buffer_depth>DEFAULT_COMMAND_BUFFER_DEPTH) {
          min_buffer_depth=DEFAULT_COMMAND_BUFFER_DEPTH;
        }

//        byte moving_motors=0;
        //analysze the movement (nad take a look at the next
        movement move = moveQueue.peek();
        //check out which momtors are geared with this

        if (move.type == move_to || move.type== move_over) {
          moveQueue.pop();

          if (move.motor<nr_of_coordinated_motors) {
            moveMotorTMC4361(move.motor, move.target, move.vMax, move.aMax, 0, 0, move.type==move_over);
//            moveMotorTMC4361(move.motor, move.target, move.vMax, move.aMax, move.vStart, move.vStop, move.type==move_over);
//            moving_motors |= _BV(move.motor);
          }
          else {
            moveMotorTMC5041(move.motor-nr_of_coordinated_motors, move.target, move.vMax, move.aMax, move.type==move_over);
//            moving_motors |= _BV(nr_of_coordinated_motors);
          }

          next_move_prepared = true;
          enable_start = true;

          movement follower;
          do {
            follower.type=uninitialized;
            if (!moveQueue.isEmpty()) {
              follower = moveQueue.peek();
              if (follower.type==follow_over || follower.type==follow_to) {
                moveQueue.pop();
                if (follower.motor<nr_of_coordinated_motors) {
                  moveMotorTMC4361(follower.motor, follower.target, follower.vMax, follower.aMax, follower.vStart, follower.vStop, follower.type==follow_over);
//                  moving_motors |= _BV(follower.motor);
                }
                else {
                  moveMotorTMC5041(follower.motor-nr_of_coordinated_motors, follower.target, follower.vMax, follower.aMax, follower.type==follow_over);
//                  moving_motors |= _BV(nr_of_coordinated_motors);
                }
              }
            }
          }
          while (follower.type == follow_over || follower.type == follow_to);

          //in the end all moviong motorts must have apssed pos_comp
//          next_target_motor_status = moving_motors;

        }
        else if (move.type == set_position) {
          // Only set motor position if motor has reached target
          if (!(active_motors & (1<<move.motor))) {
            moveQueue.pop();
            if (IS_COORDINATED_MOTOR(move.motor)) {
              setMotorPositionTMC4361(move.motor,move.target);
            } else {
              setMotorPositionTMC5041(move.motor-nr_of_coordinated_motors,move.target);
            }
          }
        }
        else {
        }
      }
      else if (current_motion_state==finishing_motion) {
        current_motion_state=no_motion;
      }
      else {
        //TODO so we know that there is currently no move executing??
        move_executing = false;
      }
    }

//    if (((target_motor_status!=0) && ((motor_status & target_motor_status) == target_motor_status)) ||
//        (next_move_prepared && !move_executing)) {
//      //TODO we need some kind of 'At least here'??
//#ifdef DEBUG_MOTION_TRACE
//      Serial.println(F("all motors reached target!"));
//#endif
//      motor_status=0;
//      target_motor_status = next_target_motor_status;
//#ifdef DEBUG_MOTION_TRACE_SHORT
//      Serial.print(F("# -> "));
//      Serial.println(target_motor_status,BIN);
//#endif
//      tmc5041_prepare_next_motion();
//      signal_start();
//      debug_sent=false;
//
//      move_executing = true;
//      next_move_prepared=false;
//    }

//    if (!next_move_prepared) {
//#ifdef CALCULATE_OUTPUT
//      digitalWriteFast(CALCULATE_OUTPUT,HIGH);
//#endif
//
//#ifdef RX_TX_BLINKY
//      RXLED1;
//#endif
///*
//       Serial.println(min_buffer_depth);
//       Serial.println(moveQueue.count());
//       Serial.println(current_motion_state);
//       Serial.println();
//*/
//      //we leave a rest in the move queue since it could be a partial movement
//      if (moveQueue.count()>0 && (moveQueue.count()>min_buffer_depth || current_motion_state==finishing_motion)) {
//        if (min_buffer_depth!=0 && min_buffer_depth>DEFAULT_COMMAND_BUFFER_DEPTH) {
//#ifdef DEBUG_MOTION_STATUS
//          Serial.println(F("Inital motion buffer full."));
//#endif
//          min_buffer_depth=DEFAULT_COMMAND_BUFFER_DEPTH;
//        }
//
//        byte moving_motors=0;
//        //analysze the movement (nad take a look at the next
//        movement move = moveQueue.pop();
//        //check out which momtors are geared with this
//
//        next_move_prepared = false;
//        if (move.type == move_to || move.type== move_over) {
//#ifdef DEBUG_MOTION
//          Serial.print(F("Moving motor "));
//          Serial.print(move.motor,DEC);
//          Serial.print(F(" to "));
//          Serial.print(move.target);
//          Serial.print(F(" at "));
//          Serial.println(move.vMax);
//#endif
//          if (move.motor<nr_of_coordinated_motors) {
//            moveMotorTMC4361(move.motor, move.target, move.vMax, move.aMax, move.vStart, move.vStop, move.type==move_over);
//            moving_motors |= _BV(move.motor);
//          }
//          else {
//            moveMotorTMC5041(move.motor-nr_of_coordinated_motors, move.target, move.vMax, move.aMax, move.type==move_over);
//            moving_motors |= _BV(nr_of_coordinated_motors);
//          }
//
//          next_move_prepared = true;
//          enable_start = true;
//
//          movement follower;
//          do {
//            follower.type=uninitialized;
//            if (!moveQueue.isEmpty()) {
//              follower = moveQueue.peek();
//              if (follower.type==follow_over || follower.type==follow_to) {
//                moveQueue.pop();
//#ifdef DEBUG_MOTION
//                Serial.print(F("Following motor "));
//                Serial.print(follower.motor,DEC);
//                Serial.print(F(" to "));
//                Serial.print(follower.target,DEC);
//                Serial.print(F(" at "));
//                Serial.println(follower.vMax);
//#endif
//                if (follower.motor<nr_of_coordinated_motors) {
//                  moveMotorTMC4361(follower.motor, follower.target, follower.vMax, follower.aMax, follower.vStart, follower.vStop, follower.type==follow_over);
//                  moving_motors |= _BV(follower.motor);
//                }
//                else {
//                  moveMotorTMC5041(follower.motor-nr_of_coordinated_motors, follower.target, follower.vMax, follower.aMax, follower.type==follow_over);
//                  moving_motors |= _BV(nr_of_coordinated_motors);
//                }
//              }
//            }
//          }
//          while (follower.type == follow_over || follower.type == follow_to);
//
//          //in the end all moviong motorts must have apssed pos_comp
//          next_target_motor_status = moving_motors;
//
//#ifdef DEBUG_MOTION
//          Serial.print(F("Next move : "));
//          Serial.println(next_target_motor_status,BIN);
//          Serial.println();
//#endif
//#ifdef DEBUG_MOTION_SHORT
//          Serial.print('>');
//          Serial.println(next_target_motor_status,BIN);
//          Serial.println();
//#endif
//        }
//        else if (move.type == set_position) {
//#ifdef DEBUG_SET_POS
//          Serial.print(F("Setting motor "));
//          Serial.print(move.motor,DEC);
//          Serial.print(F(" to position "));
//          Serial.print(move.target,DEC);
//#endif
//          if (IS_COORDINATED_MOTOR(move.motor)) {
//#ifdef DEBUG_SET_POS
//            Serial.println(F(" on TMC4361"));
//#endif
//            setMotorPositionTMC4361(move.motor,move.target);
//          }
//          else {
//#ifdef DEBUG_SET_POS
//            Serial.print(F(" on TMC5041 as #"));
//            Serial.print(move.motor-nr_of_coordinated_motors,DEC);
//#endif
//            setMotorPositionTMC5041(move.motor-nr_of_coordinated_motors,move.target);
//          }
//        }
//        else {
////          Serial.print(F("ignored move"));
//        }
//#ifdef CALCULATE_OUTPUT
//        digitalWriteFast(CALCULATE_OUTPUT,LOW);
//#endif
//      }
//      else if (current_motion_state==finishing_motion) {
//        current_motion_state=no_motion;
//      }
//      else {
////        if (!debug_sent) {
////          debug_sent=true;
////          Serial.print(F("Queue "));
////          Serial.println(moveQueue.count());
////        }
//#ifdef DEBUG_MOTION_STATUS
//        //TODO come up with a sensbilde warning
//        //        Serial.println(F("Move Queue emptied!"));
//#endif
//#ifdef DEBUG_MOTION
//        //TODO come up with a sensbilde warning
//        //        Serial.println(F("Move Queue emptied!"));
//#endif
//        //TODO so we know that there is currently no move executing??
//        move_executing = false;
//      }
//    }
  }
  RXLED0;
}

void motor_1_target_reached() {
//  motor_target_reached(0);
}

void motor_2_target_reached() {
//  motor_target_reached(1);
}

void motor_3_target_reached() {
//  motor_target_reached(2);
}

//inline void motor_target_reached(char motor_nr) {
//  long actual, target;
//  long flags;
//
////  if (in_motion) {
//  if ((current_motion_state==in_motion) && (!is_homing)) {
//    //clear the event register
//    //read43x(motors[motor_nr].cs_pin,EVENTS_REGISTER,0);
//    //and write down which motor touched the target
///*
//    // check TMC4361 for target reached
//    if (motor_nr < nr_of_coordinated_motors) {
//        // checking one of the TMC4361
//        flags = readRegister(motor_nr,TMC4361_STATUS_REGISTER);
//        if (flags & (1<<0)) {
//          motor_status |= _BV(motor_nr);
//        }
//      } else {
//        // do the same as before
//        motor_status |= _BV(motor_nr);
//      }
///*
//    if (motor_nr < nr_of_coordinated_motors) {
//        // checking one of the TMC4361
//        actual = readRegister(motor_nr,TMC4361_X_ACTUAL_REGISTER);
//        target = readRegister(motor_nr,TMC4361_X_TARGET_REGISTER);
//        if (actual == target) {
//          motor_status |= _BV(motor_nr);
//        }
//      } else {
//        // do the same as before
//        motor_status |= _BV(motor_nr);
//      }
//*/
//    // always set TargetReached Bit
////    motor_status |= _BV(motor_nr);
//
////    Serial.print(motor_nr, DEC);
//
////    switch (motor_nr) {
////      case 0: Serial.print('y'); break;
////      case 1: Serial.print('x'); break;
////      case 2: Serial.print('e'); break;
////    }
//#ifdef DEBUG_MOTION_TRACE
//    Serial.print(F("Motor "));
//    Serial.print(motor_nr,DEC);
//    Serial.print(F(" reached target! At "));
////    Serial.print(motor_status,BIN);
//    Serial.print(F(" of "));
////    Serial.println(target_motor_status,BIN);
//#endif
//#ifdef DEBUG_MOTION_TRACE_SHORT
//    Serial.print(motor_nr,DEC);
//    Serial.print('/');
////    Serial.print(motor_status,BIN);
//    Serial.print('>');
////    Serial.println(target_motor_status,BIN);
//#endif
//  }
//}

