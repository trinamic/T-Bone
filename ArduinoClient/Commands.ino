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
  //Kommandos zur Information
  kPos = 30,
  kCommands = 31,
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
  messenger.attach(kPos, onPosition);
  messenger.attach(kHome, onHome);
  messenger.attach(kCommands, onCommands);
}

// ------------------  C A L L B A C K S -----------------------

// Fehlerfunktion wenn ein Kommand nicht bekannt ist
void OnUnknownCommand() {
  messenger.sendCmd(kError,F("Unknonwn command"));
  Serial.print(messenger.CommandID());
  Serial.println(F(" - unknown command"));
}

void onInit() {
  //initialize the 43x
  initialzeTMC4361();
  //start the tmc260 driver
  intializeTMC260();
  //initialize the 5041 chpi
  init5041();
  //we stop the motion anyway
  resetMotion();
  //finally clear the command queue if there migth be some entries left
  while(!moveQueue.isEmpty()) {
    moveQueue.pop();
  }
  //and we are done here
  messenger.sendCmd(kOK,F("initialized"));
}

//Motor Strom einstellen
void onConfigMotorCurrent() {
  char motor = decodeMotorNumber();
  if (motor<0) {
    return;
  }
  int newCurrent = messenger.readIntArg();
  if (newCurrent==0) {
    messenger.sendCmdStart(kMotorCurrent);
    messenger.sendCmdArg(motor+1);
    messenger.sendCmdArg(motors[motor].tmc260.getCurrent());
    messenger.sendCmdEnd();
    return;
  }
  if (newCurrent<0) {
    messenger.sendCmd (kError,F("too low")); 
    return;
  }
  if (motor<nr_of_coordinated_motors) {
    const __FlashStringHelper* error = setCurrent260(motor,newCurrent);
    if (error==NULL) {
      messenger.sendCmd(kOK,F("Current set"));
    } 
    else {
      messenger.sendCmd(kError,error);
    }
  } 
  else {
    const __FlashStringHelper* error = setCurrent5041(motor-nr_of_coordinated_motors,newCurrent);
    if (error==NULL) {
      messenger.sendCmd(kOK,F("Current set"));
    } 
    else {
      messenger.sendCmd(kError,error);
    }
  }   
}

//set the steps per revolution 
void onStepsPerRevolution() {
  char motor = decodeMotorNumber();
  if (motor<0) {
    return;
  }
  if (I_COORDINATED_MOTOR(motor)) {
    messenger.sendCmd (kError,F("Steps per rev only for coordinated motors")); 
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
    messenger.sendCmd (kError,F("there cannot be negative steps pre revolution")); 
    return;
  }
  const __FlashStringHelper* error =  setStepsPerRevolutionTMC4361(motor,newSteps);
  motors[motor].steps_per_revolution=newSteps;
  if (error==NULL) {
    messenger.sendCmd(kOK,F("Steps set"));
  } 
  else {
    messenger.sendCmd(kError,error);
  }
}

void onInvertMotor() {
  char motor = decodeMotorNumber();
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
    messenger.sendCmd (kError,F("not funcion"));
    return;
    //TODO somehow the endstops are not reacting as expected at least there is a problem with retract after hitting
    inverted_motors |= _BV(motor);
    //invert endstops
    unsigned long endstop_config = readRegister(motor, TMC4361_REFERENCE_CONFIG_REGISTER, 0);
    endstop_config |= _BV(4);
    readRegister(motor, TMC4361_REFERENCE_CONFIG_REGISTER, endstop_config);

    messenger.sendCmd(kOK,F("Motor inverted"));
  } 
  else {
    inverted_motors &= ~(_BV(motor));
    messenger.sendCmd(kOK,F("Motor not inverted"));
  }
}

void onMove() {
  char motor = decodeMotorNumber();
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
  Serial.print(F(", dMax="));
  Serial.print(move.dMax);
  Serial.print(F(": startBow="));
  Serial.print(move.startBow);
  Serial.print(F(", endBow="));
  Serial.print(move.endBow);
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
      Serial.print(F(", dMax="));
      Serial.print(followers[following_motors].dMax);
      Serial.print(F(": startBow="));
      Serial.print(followers[following_motors].startBow);
      Serial.print(F(", endBow="));
      Serial.print(followers[following_motors].endBow);
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
    messenger.sendCmd(kError,F("Queue is full"));
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
  messenger.sendCmdArg(F("command added"));
  messenger.sendCmdEnd();
} 

char readMovementParameters(movement* move) {
  long newPos = messenger.readLongArg();
  if (newPos<0) {
    messenger.sendCmd (kError,F("cannot move beyond home"));
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
    messenger.sendCmd (kError,F("unknown movement type (s or w)"));
    return -1;
  }  
  double vMax = messenger.readFloatArg();
  if (vMax<=0) {
    messenger.sendCmd (kError,F("cannot move with no or negative speed"));
    return -1;
  }
  double aMax = messenger.readFloatArg();
  if (aMax<=0) {
    messenger.sendCmd(kError,F("cannot move with no or negative acceleration"));
    return -1;
  }
  long jerk = messenger.readLongArg();
  if (jerk<0) {
    messenger.sendCmd (kError,F("Jerk cannot be negative")); 
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
      messenger.sendCmdArg(F("running"));
    } 
    else {
      messenger.sendCmdArg(-1);
      messenger.sendCmdArg(F("stopped"));
    }
    messenger.sendCmdEnd();
  } 
  else if (movement<0) {
    //below zero means nostop the motion
    if (!current_motion_state==in_motion) {
      messenger.sendCmd(kError,F("there is currently no motion to stop"));
    } 
    else {
      //TODO should we handle double finishing ??
      Serial.println(F("motion will finish"));
      messenger.sendCmd(kOK,F("motion finishing"));
      finishMotion();
    }
  } 
  else {
    char initial_command_buffer_depth = messenger.readIntArg(); //defaults to zero and is optionnal
    //a new movement is started
    if (current_motion_state!=no_motion) {
      messenger.sendCmd(kError,F("There is already a motion running"));
    } 
    else {
      messenger.sendCmd(kOK,F("motion started"));
      startMotion(initial_command_buffer_depth);
    }
  }
}

void onPosition() {
  char motor = decodeMotorNumber();
  if (motor<0) {
    return;
  }
  unsigned long position = readRegister(motor,TMC4361_X_ACTUAL_REGISTER,0);
  messenger.sendCmdStart(kPos);
  messenger.sendCmdArg(position);
  messenger.sendCmdEnd();
}

void onConfigureEndStop() {
  char motor = decodeMotorNumber();
  if (motor<0) {
    return;
  }
  int position = messenger.readIntArg();
  if (position==0) {
    messenger.sendCmd(kError,F("Use position smaller or bigger 0 for left or right position"));
  }
  int type = messenger.readIntArg();
  if (type!=0 && type!=1) {
    messenger.sendCmd(kError,F("Use type 0 for virtual and 1 for real endstops"));
    return;
  }
  const __FlashStringHelper* error = F("Unkown error occurred");
  switch(type) {
  case 0: //virtual endstops
    {
      long virtual_pos = messenger.readLongArg();
      if (I_COORDINATED_MOTOR(motor)) {
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
        messenger.sendCmd(kError,F("Use polarity 1 for active on or -1 for active off endstops"));
        return;
      }
      if (I_COORDINATED_MOTOR(motor)) {
        error= configureEndstopTMC4361(motor, position<0, polarity>0);
      } 
      else {
        error= configureEndstopTMC5041(motor, position<0, polarity>0);
      }     
    }
    break;
  }
  if (error!=NULL) {
    messenger.sendCmd(kError,error);
  } 
  else {
    messenger.sendCmd(kOK,F("endstop configured"));
  }

  //TODO this is dummy config - we need specific settings for specific motors 
  for (char i=0; i<nr_of_coordinated_motors; i++) {
    writeRegister(i, TMC4361_REFERENCE_CONFIG_REGISTER, 0 
      | _BV(0) //STOP_LEFT enable
    | _BV(2) //positive Stop Left stops motor
    //  | _BV(3)
    //  | _BV(1)  //STOP_RIGHT enable
    //  | _BV(5) //soft stop 
    // | _BV(6) //virtual left enable
    //| _BV(7) //virtual right enable
    | _BV(11) //X_LATCH if stopl becomes active ..
    );
  }
}

void onHome() {
  //TODO wew will need a timeout afte which we have to stop homing 
  char motor = decodeMotorNumber();
  if (motor<0) {
    return;
  }
  long timeout = messenger.readLongArg();
  if (timeout<=0) {
    timeout=0;
  }
  double homeFastSpeed = messenger.readFloatArg();
  if (homeFastSpeed<=0) {
    messenger.sendCmd (kError,F("cannot home with no or negative homing speed"));
    return;
  }
  double homeSlowSpeed = messenger.readFloatArg();
  if (homeSlowSpeed<=0) {
    messenger.sendCmd (kError,F("cannot home with no or negative precision homing speed"));
    return;
  }
  long homeRetract = messenger.readLongArg();
  if (homeRetract<=0) {
    messenger.sendCmd(kError,F("cannot retract beyond home point after homing."));
    return;
  }
  long aMax = messenger.readLongArg();
  if (aMax<=0) {
    messenger.sendCmd(kError,F("cannot home with no or negative acceleration"));
    return;
  }
  long jerk = messenger.readLongArg();
  if (jerk<0) {
    messenger.sendCmd (kError,F("Jerk cannot be negative")); 
    return;
  }
#ifdef DEBUG_HOMING
  Serial.print(F("Homing for motor "));
  Serial.print(motor,DEC);
  Serial.print(F(", timeout="));
  Serial.print(timeout);
  Serial.print(F(", fast="));
  Serial.print(homeFastSpeed);
  Serial.print(F(", slow="));
  Serial.print(homeSlowSpeed);
  Serial.print(F(", retract="));
  Serial.print(homeRetract);
  Serial.print(F(", aMax="));
  Serial.print(aMax);
  Serial.print(F(": jerk="));
  Serial.print(jerk);
  Serial.println();
#endif
  const __FlashStringHelper* error =  homeMotorTMC4361(
  motor,timeout,
  homeFastSpeed, homeSlowSpeed,homeRetract,aMax,jerk);
  if (error==NULL) {
    messenger.sendCmdStart(kOK);
    messenger.sendCmdArg(F("homed"));
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
}

void watchDogStart() {
  messenger.sendCmd(kOK,F("ready"));
  watchDogPing();
}

char decodeMotorNumber() {
  char motor = messenger.readIntArg();
  if (motor<1) {
    messenger.sendCmdStart(kError);
    messenger.sendCmdArg(motor,DEC);
    messenger.sendCmdArg(1,DEC);
    messenger.sendCmdArg(nr_of_coordinated_motors,DEC);
    messenger.sendCmdArg(F("motor number too small"));
    messenger.sendCmdEnd();
    return -1;
  } 
  else if (motor>nr_of_motors) {
    messenger.sendCmdStart(kError);
    messenger.sendCmdArg(motor,DEC);
    messenger.sendCmdArg(1,DEC);
    messenger.sendCmdArg(nr_of_motors,DEC);
    messenger.sendCmdArg(F("motor number too big"));
    messenger.sendCmdEnd();
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














































