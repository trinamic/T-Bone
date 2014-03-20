// Die Kommandos die der Beagle senden kann
enum {
  // Kommandos zur Bewegung

  //Komandos zur Konfiguration
  kMotorCurrent = 1,
  kStepsPerRev = 2,
  kEndStops = 3,
  kInvertMotor = 4,
  //intitalize all drivers with default values - TODO or preconfigured??
  kInit=9,
  //Kommandos die Aktionen auslösen
  kMove = 10,
  kMovement = 11, //controls if a new movement is started or a running one is stopped
  kHome=12, //Home one axis
  kSetPos = 13, //set an axis position
  //Kommandos zur Information
  kPos = 30,
  kCommands = 31,
  //weiteres
  kCurrentReading = 41,
  //Sonstiges
  kOK = 0,
  kError =  -9,
  kWarn = -5,
  kInfo = -1,
  kKeepAlive = -128,
};


void attachCommandCallbacks() {
  // Attach callback methods
  messenger.attach(OnUnknownCommand);
  messenger.attach(kInit, onInit);
  messenger.attach(kMotorCurrent, onConfigMotorCurrent);
  messenger.attach(kEndStops,onConfigureEndStop);
  messenger.attach(kStepsPerRev, onStepsPerRevolution); 
  messenger.attach(kInvertMotor,onInvertMotor);
  messenger.attach(kMove, onMove);
  messenger.attach(kMovement, onMovement);
  messenger.attach(kSetPos, onSetPosition);
  messenger.attach(kPos, onPosition);
  messenger.attach(kHome, onHome);
  messenger.attach(kCommands, onCommands);
  messenger.attach(kCurrentReading, onCurrentReading);
}

// ------------------  C A L L B A C K S -----------------------

// Fehlerfunktion wenn ein Kommand nicht bekannt ist
void OnUnknownCommand() {
  messenger.sendCmd(kError,F("UC"));
  Serial.print(messenger.CommandID());
}

void onInit() {
  //initialize the 43x
  initialzeTMC4361();
  //start the tmc260 driver
  intializeTMC260();
  //initialize the 5041 chpi
  initialzeTMC5041();
  //we stop the motion anyway
  resetMotion();
  //finally clear the command queue if there migth be some entries left
  while(!moveQueue.isEmpty()) {
    moveQueue.pop();
  }
  //and we are done here
  messenger.sendCmd(kOK,0);
}

//Motor Strom einstellen
void onConfigMotorCurrent() {
  char motor = decodeMotorNumber(true);
  if (motor<0) {
    return;
  }
  int newCurrent = messenger.readIntArg();
  if (newCurrent==0) {
    if (IS_COORDINATED_MOTOR(motor)) {
      messenger.sendCmdStart(kMotorCurrent);
      messenger.sendCmdArg(motor+1);
      messenger.sendCmdArg(motors[motor].tmc260.getCurrent());
      messenger.sendCmdEnd();
    } 
    else {
      messenger.sendCmd (kError,0); //not implemented yet
      //get back the TMC5041 current 
    }
    return;
  }
  if (newCurrent<0) {
    messenger.sendCmd (kError,-1); 
    return;
  }
  const __FlashStringHelper* error;
  if (IS_COORDINATED_MOTOR(motor)) {
    error = setCurrentTMC260(motor,newCurrent);
  } 
  else {
    error = setCurrentTMC5041(motor-nr_of_coordinated_motors,newCurrent);
  }
  if (error==NULL) {
    messenger.sendCmd(kOK,0);
  } 
  else {
    messenger.sendCmd(kError,error);
  }
}

//set the steps per revolution 
void onStepsPerRevolution() {
  char motor = decodeMotorNumber(true);
  if (motor<0) {
    return;
  }
  if (IS_COORDINATED_MOTOR(motor)) {
    messenger.sendCmd (kError,-100); 
  }
  int newSteps = messenger.readIntArg();
  if (newSteps==0) {
    messenger.sendCmdStart(kStepsPerRev);
    messenger.sendCmdArg(motor+1);
    messenger.sendCmdArg(motors[motor].steps_per_revolution);
    messenger.sendCmdEnd();
    return;
  }
  if (newSteps<0) {
    messenger.sendCmd (kError,-1); 
    return;
  }
  const __FlashStringHelper* error =  setStepsPerRevolutionTMC4361(motor,newSteps);
  motors[motor].steps_per_revolution=newSteps;
  if (error==NULL) {
    messenger.sendCmd(kOK,0);
  } 
  else {
    messenger.sendCmd(kError,error);
  }
}

void onInvertMotor() {
  char motor = decodeMotorNumber(true);
  if (motor<0) {
    return;
  }
  char invert = messenger.readIntArg();
  if (invert==0) {
    messenger.sendCmdStart(kInvertMotor);
    messenger.sendCmdArg(motor+1);
    if (inverted_motors| _BV(motor)) {
      messenger.sendCmdArg(-1);
    } 
    else {
      messenger.sendCmdArg(1);
    }
    messenger.sendCmdEnd();
    return;
  }
  if (invert<0) {
    if (IS_COORDINATED_MOTOR(motor)) {
      messenger.sendCmd (kError,-100); //not implmented yet
      return;
      /*
      //TODO somehow the endstops are not reacting as expected at least there is a problem with retract after hitting
       inverted_motors |= _BV(motor);
       //invert endstops
       unsigned long endstop_config = readRegister(motor, TMC4361_REFERENCE_CONFIG_REGISTER, 0);
       endstop_config |= _BV(4);
       readRegister(motor, TMC4361_REFERENCE_CONFIG_REGISTER, endstop_config);
       
       messenger.sendCmd(kOK,F("Motor inverted"));
       */
    } 
    else {
      if (invertMotorTMC5041(motor-nr_of_coordinated_motors,true)) {
        messenger.sendCmd(kOK,0);
      } 
      else {
        messenger.sendCmd (kError,-1);
      }
    }
  } 
  else {
    if (IS_COORDINATED_MOTOR(motor)) {

      inverted_motors &= ~(_BV(motor));
      messenger.sendCmd(kOK,0);
    } 
    else {
      if (invertMotorTMC5041(motor-nr_of_coordinated_motors,false)) {
        messenger.sendCmd(kError,-1);
      } 
      else {
        messenger.sendCmd (kOK,0);
      }
    }
  }
}

void onMove() {
  char motor = decodeMotorNumber(true);
  if (motor<0) {
    return;
  }
  movement move;
  movement followers[MAX_FOLLOWING_MOTORS];
  move.type = move_to;
  move.motor=motor;
  if (readMovementParameters(&move)) {
    //if there was an error return 
    return;
  }
  int following_motors=0;
#ifdef DEBUG_MOTOR_QUEUE
  Serial.print(F("Adding movement for motor "));
  Serial.print(motor,DEC);
  if (move.type==move_to) {
    Serial.print(F(" to "));
  } 
  else {
    Serial.print(F(" via "));
  }
  Serial.print(move.target);
#ifdef DEBUG_MOTION    
  Serial.print(F(", vMax="));
  Serial.print(move.vMax);
  Serial.print(F(", aMax="));
  Serial.print(move.aMax);
  Serial.print(F(": jerk="));
  Serial.print(move.jerk);
#endif    
#endif

  do {
    motor = messenger.readIntArg();
    if (motor!=0) {
      followers[following_motors].type=follow_to;
      followers[following_motors].motor=motor - 1;
      if (readMovementParameters(&followers[following_motors])) {
        //if there was an error return 
        return;
      } 
#ifdef DEBUG_MOTOR_QUEUE
      Serial.print(F(", following motor "));
      Serial.print(motor - 1,DEC);
      if (move.type==follow_to) {
        Serial.print(F(" to "));
      } 
      else {
        Serial.print(F(" via "));
      }
      Serial.print(followers[following_motors].target);
#ifdef DEBUG_MOTION    
      Serial.print(F(", vMax="));
      Serial.print(followers[following_motors].vMax);
      Serial.print(F(", aMax="));
      Serial.print(followers[following_motors].aMax);
      Serial.print(F(": jerk="));
      Serial.print(followers[following_motors].jerk);
#endif    
#endif
      following_motors++;

    }  
  } 
  while (motor!=0);
#ifdef DEBUG_MOTOR_QUEUE
  Serial.println();
#endif
  if (moveQueue.count()+following_motors+1>COMMAND_QUEUE_LENGTH) {
    messenger.sendCmd(kError,-1);
    return;
  }
  moveQueue.push(move);
  for (char i=0; i<following_motors; i++) {
    moveQueue.push(followers[i]);
  }

  messenger.sendCmdStart(kOK);
  messenger.sendCmdArg(moveQueue.count());
  messenger.sendCmdArg(COMMAND_QUEUE_LENGTH);
  if (in_motion) {
    messenger.sendCmdArg(1);
  } 
  else {
    messenger.sendCmdArg(-1);
  }
  messenger.sendCmdEnd();
} 

char readMovementParameters(movement* move) {
  long newPos = messenger.readLongArg();
  if (newPos<0) {
    messenger.sendCmd (kError,-1); //TODO really?
    return -1;
  }
  char movementType = (char)messenger.readIntArg();
  boolean isWaypoint;
  if (movementType == 's') {
    //the movement is no waypoint  we do not have to do anything
    isWaypoint = false;
  } 
  else if (movementType== 'w') {
    isWaypoint = true;
  } 
  else {
    messenger.sendCmd (kError,-2);
    return -1;
  }  
  double vMax = messenger.readFloatArg();
  if (vMax<=0) {
    messenger.sendCmd (kError,3);
    return -1;
  }
  double aMax = messenger.readFloatArg();
  if (aMax<=0) {
    messenger.sendCmd(kError,-4);
    return -1;
  }
  long jerk = messenger.readLongArg();
  if (jerk<0) {
    messenger.sendCmd (kError,-5); 
    return -1;
  }

  move->target = newPos;
  if (isWaypoint) {
    if (move->type==move_to) {
      move->type=move_over;
    } 
    else {
      move->type=follow_over;
    }
  }
  move->vMax=vMax;
  move->aMax=aMax;
  move->jerk=jerk;

  return 0;
}

void onMovement() {
  char movement = messenger.readIntArg();
  if (movement==0) {
    messenger.sendCmdStart(kMovement);
    //just give out the current state of movement
    if (current_motion_state==in_motion || current_motion_state==finishing_motion) {
      messenger.sendCmdArg(1);
    } 
    else {
      messenger.sendCmdArg(-1);
    }
    messenger.sendCmdEnd();
  } 
  else if (movement<0) {
    //below zero means nostop the motion
    if (!current_motion_state==in_motion) {
      messenger.sendCmd(kError,-1);
    } 
    else {
      //TODO should we handle double finishing ??
#ifdef DEBUG_MOTION_STATUS
      Serial.println(F("motion will finish"));
#endif
      messenger.sendCmd(kOK,0);
      finishMotion();
    }
  } 
  else {
    char initial_command_buffer_depth = messenger.readIntArg(); //defaults to zero and is optionnal
    //a new movement is started
    if (current_motion_state!=no_motion) {
      messenger.sendCmd(kError,-1);
    } 
    else {
      messenger.sendCmd(kOK,0);
      startMotion(initial_command_buffer_depth);
    }
  }
}

void onSetPosition() {
  char motor = decodeMotorNumber(true);
  if (motor<0) {
    return;
  }
  long newPos = messenger.readLongArg();
  if (newPos<0) {
    messenger.sendCmd (kError,-1);
    return;
  }
#ifdef DEBUG_SET_POS
  Serial.print(F("Enqueing setting motor "));
  Serial.print(motor,DEC);
  Serial.print(F(" to position "));
  Serial.println(newPos,DEC);
#endif  
  //configure a movement
  movement move;
  move.type = set_position;
  move.motor=motor;
  moveQueue.push(move);

  messenger.sendCmdStart(kOK);
  messenger.sendCmdArg(moveQueue.count());
  messenger.sendCmdArg(COMMAND_QUEUE_LENGTH);
}

void onPosition() {
  char motor = decodeMotorNumber(true);
  if (motor<0) {
    return;
  }
  long position;
  if (IS_COORDINATED_MOTOR(motor)) {

    position = (long)readRegister(motor,TMC4361_X_ACTUAL_REGISTER);
  } 
  else {
    if (motor == nr_of_coordinated_motors) {
      position = (long) readRegister(TMC5041_MOTORS, TMC5041_X_ACTUAL_REGISTER_1);
    } 
    else {
      position = (long) readRegister(TMC5041_MOTORS, TMC5041_X_ACTUAL_REGISTER_2);
    }
  }
  messenger.sendCmdStart(kPos);
  messenger.sendCmdArg(position);
  messenger.sendCmdEnd();
}

void onConfigureEndStop() {
  char motor = decodeMotorNumber(true);
  if (motor<0) {
    return;
  }
  int position = messenger.readIntArg();
  if (position==0) {
    messenger.sendCmd(kError,-1);
  }
  int type = messenger.readIntArg();
  if (type!=0 && type!=1) {
    messenger.sendCmd(kError,-1);
    return;
  }
  const __FlashStringHelper* error;
  switch(type) {
  case 0: //virtual endstops
    {
      long virtual_pos = messenger.readLongArg();
      if (IS_COORDINATED_MOTOR(motor)) {
        error = configureVirtualEndstopTMC4361(motor, position<0, virtual_pos);
      } 
      else {
        error = configureVirtualEndstopTMC5041(motor, position<0, virtual_pos);
      }
    }
    break;
  case 1: //real endstop
    {
      int polarity = messenger.readIntArg();
      if (polarity==0) {
        messenger.sendCmd(kError,-1);
        return;
      }
      if (IS_COORDINATED_MOTOR(motor)) {
        error= configureEndstopTMC4361(motor, position<0, polarity>0);
      } 
      else {
        error= configureEndstopTMC5041(motor-nr_of_coordinated_motors, position<0, true, polarity>0);
      }     
    }
    break;
  }
  if (error!=NULL) {
    messenger.sendCmd(kError,error);
  } 
  else {
    messenger.sendCmd(kOK,0);
  }
}

void onHome() {
  //TODO wew will need a timeout afte which we have to stop homing 
  char motor = decodeMotorNumber(true);
  if (motor<0) {
    return;
  }
  long timeout = messenger.readLongArg();
  if (timeout<=0) {
    timeout=0;
  }
  double homeFastSpeed = messenger.readFloatArg();
  if (homeFastSpeed<=0) {
    messenger.sendCmd (kError,-1);
    return;
  }
  double homeSlowSpeed = messenger.readFloatArg();
  if (homeSlowSpeed<=0) {
    messenger.sendCmd (kError,-2);
    return;
  }
  long homeRetract = messenger.readLongArg();
  if (homeRetract<=0) {
    messenger.sendCmd(kError,-3);
    return;
  }
  double aMax = messenger.readFloatArg();
  if (aMax<=0) {
    messenger.sendCmd(kError,-4);
    return;
  }
  const __FlashStringHelper* error;
  if (motor<nr_of_coordinated_motors) {
    long jerk = messenger.readLongArg();
    if (jerk<0) {
      messenger.sendCmd (kError,-5); 
      return;
    }

    unsigned long home_right_position = messenger.readLongArg();

    error =  homeMotorTMC4361(
    motor,timeout,
    homeFastSpeed, homeSlowSpeed,homeRetract,aMax,jerk, home_right_position);
  } 
  else {
    char following_motors[homing_max_following_motors]; //we can only home follow controlled motors
    for (char i = 0; i<homing_max_following_motors ;i++) {
      following_motors[i] = decodeMotorNumber(false);
      if (following_motors[i]==-1) {
        break;
      } 
      else {
        following_motors[i] -= nr_of_coordinated_motors;
      }
    }
    error =  homeMotorTMC5041(
    motor-nr_of_coordinated_motors,timeout,
    homeFastSpeed, homeSlowSpeed,homeRetract,aMax,following_motors);
  }
  if (error==NULL) {
    messenger.sendCmdStart(kOK);
    messenger.sendCmdArg(motor,DEC);
    messenger.sendCmdEnd();
  } 
  else {
    messenger.sendCmd(kError,error);
    messenger.sendCmdStart(kError);
    messenger.sendCmdArg(error);
    messenger.sendCmdArg(motor,DEC);
    messenger.sendCmdEnd();
  }

}

void onCommands() {
  messenger.sendCmdStart(kCommands);
  messenger.sendCmdArg(moveQueue.count());
  messenger.sendCmdArg(COMMAND_QUEUE_LENGTH);
  messenger.sendCmdEnd();
#ifdef DEBUG_STATUS
  int ram = freeRam();
  Serial.print(F("Queue: "));
  Serial.print(moveQueue.count());
  Serial.print(F(" of "));
  Serial.print(COMMAND_QUEUE_LENGTH);
  Serial.print(F("\tRAM:  "));
  Serial.print(ram);
  if (current_motion_state==no_motion) {
    Serial.println(F("\t - not moving"));
  } 
  else if (current_motion_state==in_motion) {
    Serial.println(F("\t - in motion"));
  } 
  else if (current_motion_state==finishing_motion) {
    Serial.println(F("\t - finishing motion"));
  } 
  else {
    Serial.println(F("Unkmown motion"));
  }  
  Serial.println();
#endif
}

void onCurrentReading() {
  //todo this is a hack because the current reading si onyl avail on arduino
  char number = messenger.readIntArg();
  if (number!=0) {
    number = 1;
  }
  unsigned int value = 0;
  for (char i=0; i<4; i++) {
    value += analogRead(4+number);
  }
  value >> 2;
  messenger.sendCmdStart(kCurrentReading);
  messenger.sendCmdArg(number,DEC);
  messenger.sendCmdArg(value);
  messenger.sendCmdEnd();
}

void watchDogPing() {
  messenger.sendCmdStart(kKeepAlive);
  messenger.sendCmdArg(moveQueue.count());
  messenger.sendCmdArg(COMMAND_QUEUE_LENGTH);
  messenger.sendCmdEnd();
#ifdef DEBUG_STATUS
  int ram = freeRam();
  Serial.print(F("Queue: "));
  Serial.print(moveQueue.count());
  Serial.print(F(" of "));
  Serial.print(COMMAND_QUEUE_LENGTH);
  Serial.print(F("\tRAM:  "));
  Serial.print(ram);
  if (current_motion_state==no_motion) {
    Serial.println(F("\t - not moving"));
  } 
  else if (current_motion_state==in_motion) {
    Serial.println(F("\t - in motion"));
  } 
  else if (current_motion_state==finishing_motion) {
    Serial.println(F("\t - finishing motion"));
  } 
  else {
    Serial.println(F("Unkmown motion"));
  }  
  Serial.println();
#endif
#ifdef DEBUG_TMC5041_STATUS
  Serial.print(F("#1: "));
  Serial.print(readRegister(TMC5041_MOTORS, TMC5041_RAMP_STATUS_REGISTER_1),HEX);
  Serial.print(F("\t#2: "));
  Serial.println(readRegister(TMC5041_MOTORS, TMC5041_RAMP_STATUS_REGISTER_2),HEX);
  Serial.println();
#endif
}

void watchDogStart() {
  messenger.sendCmd(kOK,0);
  watchDogPing();
}

char decodeMotorNumber(const boolean complaint) {
  char motor = messenger.readIntArg();
  if (motor<1) {
    if (complaint) {
      messenger.sendCmdStart(kError);
      messenger.sendCmdArg(motor,DEC);
      messenger.sendCmdArg(1,DEC);
      messenger.sendCmdArg(nr_of_coordinated_motors,DEC);
      messenger.sendCmdEnd();
    }
    return -1;
  } 
  else if (motor>nr_of_motors) {
    if (complaint) {
      messenger.sendCmdStart(kError);
      messenger.sendCmdArg(motor,DEC);
      messenger.sendCmdArg(1,DEC);
      messenger.sendCmdArg(nr_of_motors,DEC);
      messenger.sendCmdEnd();
    }
    return -1;
  } 
  else {
    return motor - 1;
  }
}

// see http://rollerprojects.com/2011/05/23/determining-sram-usage-on-arduino/ 
int freeRam() {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}










