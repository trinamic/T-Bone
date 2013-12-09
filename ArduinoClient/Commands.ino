// Die Kommandos die der Beagle senden kann
enum {
  // Kommandos zur Bewegung

  //Komandos zur Konfiguration
  kMotorCurrent = 1,
  kStepsPerRev = 2,
  //intitalize all drivers with default values - TODO or preconfigured??
  kInit=9,
  //Kommandos die Aktionen auslösen
  kMove = 10,
  kMovement = 11, //controls if a new movement is started or a running one ist stopped
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
  messenger.attach(kStepsPerRev, onStepsPerRevolution); 
  messenger.attach(kMove, onMove);
  messenger.attach(kMovement, onMovement);
  messenger.attach(kPos, onPosition);
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
  initialzeTMC43x();
  //start the tmc260 driver
  intializeTMC260();
  //we stop the motion anyway
  stopMotion();
  //finally clear the command queue if there migth be some entries left
  while(!moveQueue.isEmpty()) {
    moveQueue.pop();
  }
  //and we are done here
  messenger.sendCmd(kOK,F("All systems initialized"));
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
    messenger.sendCmd (kError,F("Current too low")); 
    return;
  }
  const __FlashStringHelper* error = setCurrent(motor,newCurrent);
  if (error==NULL) {
    messenger.sendCmd(kOK,F("Current set"));
  } 
  else {
    messenger.sendCmd(kError,error);
  }
}

//set the steps per revolution 
void onStepsPerRevolution() {
  char motor = decodeMotorNumber();
  if (motor<0) {
    return;
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
  const __FlashStringHelper* error =  setStepsPerRevolution(motor,newSteps);
  motors[motor].steps_per_revolution=newSteps;
  if (error==NULL) {
    messenger.sendCmd(kOK,F("Steps set"));
  } 
  else {
    messenger.sendCmd(kError,error);
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
  Serial.print(F(" to "));
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
      Serial.print(F(" to "));
      Serial.print(move.target);
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
  long aMax = messenger.readLongArg();
  if (aMax<=0) {
    messenger.sendCmd(kError,F("cannot move with no or negative acceleration"));
    return -1;
  }
  long dMax = messenger.readLongArg();
  if (dMax<=0) {
    messenger.sendCmd(kError,F("cannot move with no or negative deceleration"));
    return -1;
  }
  long startBow = messenger.readLongArg();
  if (startBow<0) {
    messenger.sendCmd (kError,F("Start bow cannot be negative")); 
    return -1;
  }
  long endBow = messenger.readLongArg();
  if (endBow<0) {
    messenger.sendCmd (kError,F("Start bow cannot be negative")); 
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
  move->dMax=dMax;
  move->startBow=startBow;
  move->endBow=endBow;

  return 0;
}

void onMovement() {
  char movement = messenger.readIntArg();
  if (movement==0) {
    messenger.sendCmdStart(kCommands);
    //just give out the current state of movement
    if (in_motion) {
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
    if (!in_motion) {
      messenger.sendCmd(kError,F("there is currently no motion to stop"));
    } 
    else {
      Serial.println(F("motion stoped"));
      messenger.sendCmd(kOK,F("motion stoped"));
      stopMotion();
    }
  } 
  else {
    //a new movement is started
    if (in_motion) {
      messenger.sendCmd(kError,F("There is already a motion running"));
    } 
    else {
      Serial.println(F("motion started"));
      messenger.sendCmd(kOK,F("motion started"));
      startMotion();
    }
  }
}

void onPosition() {
  char motor = decodeMotorNumber();
  if (motor<0) {
    return;
  }
  unsigned char cs_pin = motors[motor-1].cs_pin;
  unsigned long position = read43x(cs_pin,X_ACTUAL_REGISTER,0);
  messenger.sendCmdStart(kPos);
  messenger.sendCmdArg(position);
  messenger.sendCmdEnd();
}

void onCommands() {
  messenger.sendCmdStart(kCommands);
  messenger.sendCmdArg(moveQueue.count());
  messenger.sendCmdArg(COMMAND_QUEUE_LENGTH);
  messenger.sendCmdEnd();
}

void watchDogPing() {
  int ram = freeRam();
  messenger.sendCmdStart(kKeepAlive);
  messenger.sendCmdArg(moveQueue.count());
  messenger.sendCmdArg(COMMAND_QUEUE_LENGTH);
  messenger.sendCmdArg(ram);
  messenger.sendCmdArg(F("still alive"));
  messenger.sendCmdEnd();
#ifdef DEBUG_STATUS
  Serial.print(F("Queue: "));
  Serial.print(moveQueue.count());
  Serial.print(F(" of "));
  Serial.print(COMMAND_QUEUE_LENGTH);
  Serial.print(F("\tRAM:  "));
  Serial.print(ram);
  Serial.print(in_motion? F("\tin motion"): F("\tstopped"));
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
    messenger.sendCmdArg(nr_of_motors,DEC);
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






















