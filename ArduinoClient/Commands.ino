// Die Kommandos die der Beagle senden kann
enum {
  // Kommandos zur Bewegung

  //Komandos zur Konfiguration
  kMotorCurrent = 1,
  kStepsPerRev = 2,
  kRampBows = 3,
  //Kommandos die Aktionen auslösen
  kMove = 10,
  //Kommandos zur Information
  kPos = 30,
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
  messenger.attach(kMotorCurrent, onConfigMotorCurrent);
  messenger.attach(kStepsPerRev, onStepsPerRevolution); 
  messenger.attach(kRampBows, onRampBows);
  messenger.attach(kMove, onMove);
  messenger.attach(kPos, onPosition);
}

// ------------------  C A L L B A C K S -----------------------

// Fehlerfunktion wenn ein Kommand nicht bekannt ist
void OnUnknownCommand() {
  messenger.sendCmd(kError,F("Unknonwn command"));
}

//Motor Strom einstellen
void onConfigMotorCurrent() {
  int newCurrent = messenger.readIntArg();
  if (newCurrent==0) {
    messenger.sendCmdStart(kMotorCurrent);
    messenger.sendCmdArg(current_in_ma);
    messenger.sendCmdEnd();
    return;
  }
  if (newCurrent<0) {
    messenger.sendCmd (kError,F("Current too low")); 
    return;
  }
  const __FlashStringHelper* error = setCurrent(newCurrent);
  if (error==NULL) {
    messenger.sendCmd(kOK,F("Current set"));
  } 
  else {
    messenger.sendCmd(kError,error);
  }
}

//set the steps per revolution 
void onStepsPerRevolution() {
  int newSteps = messenger.readIntArg();
  if (newSteps==0) {
    messenger.sendCmdStart(kStepsPerRev);
    messenger.sendCmdArg(steps_per_revolution);
    messenger.sendCmdEnd();
    return;
  }
  if (newSteps<0) {
    messenger.sendCmd (kError,F("there cannot be negative steps pre revolution")); 
    return;
  }
  const __FlashStringHelper* error =  setStepsPerRevolution(newSteps);
  if (error==NULL) {
    messenger.sendCmd(kOK,F("Steps set"));
  } 
  else {
    messenger.sendCmd(kError,error);
  }
}

void onRampBows() {
  long startBow = messenger.readLongArg();
  if (startBow==0) {
    messenger.sendCmdStart(kRampBows);
    messenger.sendCmdArg(current_startbow);
    messenger.sendCmdArg(current_endbow);
    messenger.sendCmdEnd();
    return;
  }
  if (startBow<0) {
    messenger.sendCmd (kError,F("Start bow cannot be negative")); 
    return;
  }
  long endBow = messenger.readLongArg();
  if (endBow<0) {
    messenger.sendCmd (kError,F("Start bow cannot be negative")); 
    return;
  }
  const __FlashStringHelper* error = setRampBows(startBow, endBow);
  if (error==NULL) {
    messenger.sendCmd(kOK,F("Ramp Bows set"));
  } 
  else {
    messenger.sendCmd(kError,error);
  }
}

void onMove() {
  long newPos = messenger.readLongArg();
  if (newPos<0) {
    messenger.sendCmd (kError,F("cannot move beyond home"));
  }
  int vMax = messenger.readIntArg();
  if (vMax<=0) {
    messenger.sendCmd (kError,F("cannot move with negative speed"));
  }
  int aMax = messenger.readIntArg();
  if (aMax<=0) {
    messenger.sendCmd(kError,F("cannot move with negative speed"));
  }
  int dMax = messenger.readIntArg();
  if (dMax<0) {
    messenger.sendCmd(kError,F("cannot move with negative speed"));
  }
  const __FlashStringHelper* error =  moveMotor(newPos, vMax, aMax, dMax);
  if (error==NULL) {
    messenger.sendCmd(kOK,F("Moving"));
  } 
  else {
    messenger.sendCmd(kError,error);
  }
} 

void onPosition() {
  unsigned long position = read43x(X_ACTUAL_REGISTER,0);
  messenger.sendCmdStart(kPos);
  messenger.sendCmdArg(position);
  messenger.sendCmdEnd();
}  

void watchDogPing() {
  messenger.sendCmdStart(kKeepAlive);
  messenger.sendCmdArg(moveQueue.count());
  messenger.sendCmdArg("still alive");
  messenger.sendCmdEnd();
}

void watchDogStart() {
  messenger.sendCmd(kOK,F("ready"));
  watchDogPing();
}






