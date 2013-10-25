// Die Kommandos die der Beagle senden kann
enum
{
  // Kommandos zur Bewegung
  
  //komandos zur Konfiguration
  kMotorCurrent = 100,
  kStepsPerRev = 101,
  //Sonstiges
  kOK = 0,
  kError = -1,
  kWarn = -2,
  kInfo = -128,
};


void attachCommandCallbacks()
{
  // Attach callback methods
  messenger.attach(OnUnknownCommand);
//  cmdMessenger.attach(kFloatAddition, OnFloatAddition);
}

// ------------------  C A L L B A C K S -----------------------

// Called when a received command has no attached function
void OnUnknownCommand()
{
  messenger.sendCmd(kError,"Command without attached callback");
}

void watchDogPing() {
    messenger.sendCmd(kInfo,"still Alive");
}

