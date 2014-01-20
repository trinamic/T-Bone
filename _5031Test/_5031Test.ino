#include <SPI.h>
#include <TMC26XGenerator.h>
#include <Metro.h>

#define DEBUG

//config
unsigned int current_in_ma = 500;
long vmax = 100000000ul;

#define TMC_5031_R_SENSE 0.27

//register
#define TMC5031_GENERAL_CONFIG_REGISTER 0x00
#define TMC5031_RAMP_MODE_REGISTER_1 0x20
#define TMC5031_HOLD_RUN_CURRENT_REGISTER_1 0x30

#define TMC5031_RAMP_MODE_REGISTER_2 0x40
#define TMC5031_HOLD_RUN_CURRENT_REGISTER_1 0x50

//values

//we have a TMC260 at the end so we configure a configurer

//a metro to control the movement
Metro moveMetro = Metro(5000ul);
Metro checkMetro = Metro(1000ul);

int squirrel_a = 12;
int squirrel_b = 8;
int tmc_5031 = 11;
int reset_squirrel = 4;

void setup() {
  //initialize the serial port for debugging
  Serial.begin(9600);

  //reset the quirrel
  pinMode(reset_squirrel,OUTPUT);
  digitalWrite(reset_squirrel, LOW);
  delay(1);
  digitalWrite(reset_squirrel, HIGH);
  pinMode(squirrel_a,OUTPUT);
  digitalWrite(squirrel_a,HIGH);
  pinMode(squirrel_b,OUTPUT);
  digitalWrite(squirrel_b,HIGH);
  //initialize SPI
  SPI.begin();
  //initialize the genereal configuration of the tmc 5031
  write5031(TMC5031_GENERAL_CONFIG_REGISTER, _BV(3)); //int/PP are outputs
  write5031(TMC5031_RAMP_MODE_REGISTER_1,0); //enforce positioing mode
  //TODOD write 1s to the TMC5031_HOLD_RUN_CURRENT_REGISTER_1??
}

unsigned long target=0;

void loop() {
  if (target==0 | moveMetro.check()) {
    target=random(1000000ul);
    unsigned long this_v = vmax+random(100)*vmax;
    Serial.print("Move to ");
    Serial.println(target);
    Serial.println();
  }
  if (checkMetro.check()) {
    // put your main code here, to run repeatedly: 
    unsigned char cs = calculateCurrent(current_in_ma);
    Serial.print("Current");
    if (cs & 0x80) {
      Serial.print(" (h);");
      Serial.println((cs&0x7F),DEC);
    } 
    else {
      Serial.print(" (l);");
      Serial.println(cs,DEC);
    }
  }
}




