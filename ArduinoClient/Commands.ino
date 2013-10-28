// Die Kommandos die der Beagle senden kann
enum {
  // Kommandos zur Bewegung

  //Komandos zur Konfiguration
  kMotorCurrent = 1,
  kStepsPerRev = 2,
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
  if (newCurrent<0) {
    messenger.sendCmd (kError,"Current too low"); 
    return;
  }
  if (newCurrent==0) {
    messenger.sendCmdStart(kMotorCurrent);
    messenger.sendCmdArg(current_in_ma);
    messenger.sendCmdEnd();
    return;
  }
  char* error = setCurrent(newCurrent);
  if (error==NULL) {
    messenger.sendCmd(kOK,"Current is OK");
  } else {
    messenger.sendCmd(kError,error);
  }
}

void watchDogPing() {
  messenger.sendCmd(kKeepAlive,"still alive");
}

void watchDogStart() {
  messenger.sendCmd(kOK,"ready");
  watchDogPing();
}



