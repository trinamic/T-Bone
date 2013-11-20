// Die Kommandos die der Beagle senden kann
enum {
  // Kommandos zur Bewegung

  //Komandos zur Konfiguration
  kMotorCurrent = 1,
  kStepsPerRev = 2,
  kAccelerationSetttings = 3,
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
  messenger.attach(kAccelerationSetttings, onAccelerationSetttings);
  messenger.attach(kMove, onMove);
  messenger.attach(kMovement, onMovement);
  messenger.attach(kPos, onPosition);
  messenger.attach(kCommands, onCommands);
}

// ------------------  C A L L B A C K S -----------------------

// Fehlerfunktion wenn ein Kommand nicht bekannt ist
void OnUnknownCommand() {
  messenger.sendCmd(kError,F("Unknonwn command"));
  Serial.println(F("communication error"));
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

void onAccelerationSetttings() {
  char motor = decodeMotorNumber();
  if (motor<0) {
    return;
  }
  long aMax = messenger.readLongArg();
  if (aMax==0) {
    messenger.sendCmdStart(kAccelerationSetttings);
    messenger.sendCmdArg(motors[motor].aMax);
    messenger.sendCmdArg(motors[motor].dMax);
    messenger.sendCmdArg(motors[motor].startBow);
    messenger.sendCmdArg(motors[motor].endBow);
    messenger.sendCmdEnd();
    return;
  }
  if (aMax<0) {
    messenger.sendCmd(kError,F("cannot move with no or negative acceleration"));
    return;
  }
  long dMax = messenger.readLongArg();
  if (dMax<0) {
    messenger.sendCmd(kError,F("cannot move with no or negative deceleration"));
    return;
  }
  long startBow = messenger.readLongArg();
  if (startBow<0) {
    messenger.sendCmd (kError,F("Start bow cannot be negative")); 
    return;
  }
  long endBow = messenger.readLongArg();
  if (endBow<0) {
    messenger.sendCmd (kError,F("Start bow cannot be negative")); 
    return;
  }
  const __FlashStringHelper* error = setAccelerationSetttings(motor, aMax, dMax, startBow, endBow);
  if (error==NULL) {
#ifdef DEBUG_MOTOR_CONTFIG    Serial.print(F("Motor "));
    Serial.print(motor,DEC);
    Serial.print(F(": aMax="));
    Serial.print(aMax);
    Serial.print(F(", dMax="));
    Serial.print(dMax);
    Serial.print(F(": startBow="));
    Serial.print(aMax);
    Serial.print(F(", endBow="));
    Serial.print(aMax);
    Serial.println();
#endif    
    messenger.sendCmd(kOK,F("Ramp Bows set"));
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
  long newPos = messenger.readLongArg();
  if (newPos<0) {
    messenger.sendCmd (kError,F("cannot move beyond home"));
    return;
  }
  double vMax = messenger.readFloatArg();
  if (vMax<=0) {
    Serial.println(vMax);
    messenger.sendCmd (kError,F("cannot move with no or negative speed"));
    return;
  }
  movement move;
  movement followers[MAX_FOLLOWING_MOTORS];
  move.type = movemotor;
  move.motor=motor;
  move.data.move.target=newPos;
  move.data.move.vmax = vMax;
  int following_motors=0;
#ifdef DEBUG_MOTOR_QUEUE
  Serial.print(F("Adding movement for motor "));
  Serial.print(motor,DEC);
  Serial.print(F(" to "));
  Serial.print(newPos);
#endif
  do {
    motor = messenger.readIntArg();
    double motionFactor =  messenger.readFloatArg();
    long newPos = messenger.readLongArg();
    if (newPos<0) {
      messenger.sendCmd (kError,F("cannot move beyond home"));
      return;
    }
    if (motor!=0 && motionFactor!=0) {
      followers[following_motors].type=followmotor;
      followers[following_motors].motor=motor - 1;
      followers[following_motors].data.follow.target=newPos;
      followers[following_motors].data.follow.factor=motionFactor;
      following_motors++;
#ifdef DEBUG_MOTOR_QUEUE
      Serial.print(F(", gearing motor "));
      Serial.print(gearmotor,DEC);
      Serial.print(F(" by "));
      Serial.print(gearingFactor);
      Serial.print(F(" to "));
      Serial.print(newPos);
#endif
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


















