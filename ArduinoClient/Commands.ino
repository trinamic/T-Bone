// Die Kommandos die der Beagle senden kann
enum {
  // Kommandos zur Bewegung

  //Komandos zur Konfiguration
  kMotorCurrent = 10,
  kStepsPerRev = 11,
  //Sonstiges
  kOK = 0,
  kError =  9,
  kWarn = 5,
  kInfo = 1,
};


void attachCommandCallbacks() {
  // Attach callback methods
  messenger.attach(OnUnknownCommand);
  messenger.attach(kMotorCurrent, onConfigMotorCurrent);
  //  cmdMessenger.attach(kFloatAddition, OnFloatAddition);
}

// ------------------  C A L L B A C K S -----------------------

// Fehlerfunktion wenn ein Kommand nicht bekannt ist
void OnUnknownCommand() {
  messenger.sendCmd(kError,"Unknonwn command");
}

//Motor Strom einstellen
void onConfigMotorCurrent() {
  int newCurrent = messenger.readIntArg();
  if (newCurrent<=0) {
    messenger.sendCmd (kError,"Current too low"); 
    return;
  }
  if (newCurrent>MAX_MOTOR_CURRENT) {
    messenger.sendCmd(kError,"Current too high");
    return;
  } 
    messenger.sendCmd(kOK,"Current is OK");
}

void watchDogPing() {
  messenger.sendCmd(kInfo,"still Alive");
}


