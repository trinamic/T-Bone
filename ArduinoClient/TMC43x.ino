
void initialzeTMC43x() {
  //reset the quirrel
  digitalWrite(reset_squirrel, LOW);

  pinMode(start_signal_pin,INPUT);
  digitalWrite(start_signal_pin,LOW);

  //will be released after setup is complete   
  for (char i=0; i<nr_of_motors; i++) {
    //initialize CS pin
    digitalWrite(motors[i].cs_pin,HIGH);
    pinMode(motors[i].cs_pin,OUTPUT);
    pinMode(motors[i].target_reached_interrupt_pin,INPUT);
    digitalWrite(motors[i].target_reached_interrupt_pin,LOW);
  }
  //enable the reset again
  delay(1);
  digitalWrite(reset_squirrel, HIGH);
  delay(10);
  //initialize SPI
  SPI.begin();
  //preconfigure the TMC43x
  for (char i=0; i<nr_of_motors;i++) {
    write43x(motors[i].cs_pin, GENERAL_CONFIG_REGISTER, 0); //we use direct values
    write43x(motors[i].cs_pin, RAMP_MODE_REGISTER,_BV(2) | 2); //we want to go to positions in nice S-Ramps ()TDODO does not work)
    write43x(motors[i].cs_pin, SH_RAMP_MODE_REGISTER,_BV(2) | 2); //we want to go to positions in nice S-Ramps ()TDODO does not work)
    write43x(motors[i].cs_pin,CLK_FREQ_REGISTER,CLOCK_FREQUENCY);
    setStepsPerRevolution(motors[i].cs_pin,motors[i].steps_per_revolution);
  }
}

const __FlashStringHelper* setStepsPerRevolution(unsigned char motor_nr, unsigned int steps) {
  //get the motor number
  unsigned char cs_pin = motors[motor_nr].cs_pin;
  //configure the motor type
  unsigned long motorconfig = 0x00; //we want 256 microsteps
  motorconfig |= steps<<4;
  write43x(cs_pin,STEP_CONF_REGISTER,motorconfig);
  motors[motor_nr].steps_per_revolution = steps;
  return NULL;
}


const __FlashStringHelper* setAccelerationSetttings(unsigned char motor_nr, long aMax, long dMax,long startbow, long endbow) {
  //get the motor number
  unsigned char cs_pin = motors[motor_nr].cs_pin;
  //TODO some validity settings??
  motors[motor_nr].aMax = aMax;
  motors[motor_nr].dMax = dMax;
  motors[motor_nr].startBow = startbow;
  motors[motor_nr].endBow = endbow!=0;

  if (endbow==0) {
    endbow=startbow;
  }

  return NULL;
}

void moveMotor(unsigned char motor_nr, long pos, double vMax, double factor, boolean configure_shadow) {
  unsigned char cs_pin = motors[motor_nr].cs_pin;

  long aMax = motors[motor_nr].aMax;
  long dMax = motors[motor_nr].dMax;
  long startBow = motors[motor_nr].startBow;
  long endBow = motors[motor_nr].endBow;

  //TODO can't start commands be part of this movement ??

  if (factor==0) {
    vMax = 0;
    aMax = 0;
    dMax = 0;
    startBow = 0;
    endBow = 0;
  } 
  else if (factor!=1.0) {
    vMax = factor*vMax;
    aMax = aMax * factor;
    dMax = (dMax!=0)? dMax * factor: aMax;
    startBow = startBow * factor;
    endBow = (endBow!=0)? endBow * factor: startBow;
  }
  
  /*
  Master: (Startsignal generieren)
	bit4 = 1 (shadow on)
	bit9 je nach Bedarf und in Abstimmung mit dem Slave
	bit12= 1 (pipeline on)

	rest = 0 (wenn kein zyklische und/oder verzögerte Shadowregisterübernahme gewünscht)

Slave: (auf externes Startsignal reagieren)
	bit0 = 1
	bit5 = 1
	bit4 = 1 (shadow on)
	bit9 je nach Bedarf und in Abstimmung mit dem Master
	bit12= 1 (pipeline on)

	rest = 0 (wenn kein zyklische und/oder verzögerte Shadowregisterübernahme gewünscht)

Mindestens eines der ersten 5Bit muss eingeschaltet sein, sonst gibt es kein internes Startsignal. Aufgrund des Bugs, sollten es nicht Bit0 bis Bit2 sein. Wenn auch kein Shadowregister benutzt werden soll, dann bleibt nur Bit3, was allerdings auch gleichzeitig eine Gearingfaktoränderung bis zum nächsten Startsignal hält. Daher folgendes Setup für das Startregister:
	bit3 = 1 (Startsignal notwendig für Gearingfaktoränderung)
	bit6 = 1 (Startsignal nach Target_reached-Event)
	bit12= 1 (pipeline on) 
	bit13= 1 (busy-state on)
	Rest=0

*/

/* my conclusions

  b 0 -> xtarget
  b 3 -> bug
  b 4 -> shaddow
  b 6 -> target reach is start
  b11 -> shaddow
  b13 für busy (ab erstem oder erst mit nachfolgenden??
  
  start_out_add für 'wie lange warte ich '
  
  beim ersten eventuell b 5 für externen start??
*/


  if (factor!=0) {
    if (!configure_shadow) {
      write43x(motors[motor_nr].cs_pin, START_CONFIG_REGISTER, 0
        | _BV(0) //xtarget requires start
      | _BV(1) //vmax requires start
      | _BV(5) //external start is an start
      | _BV(10)//immediate start
      );   
      //if the next move is in the same direction prepare the shadow registers
      prepare_shaddow_registers = true; //TODO this is only trtue if ... there is something left in the queue??  
      //we need to generate a start event
    } 
    else {
      write43x(motors[motor_nr].cs_pin, START_CONFIG_REGISTER, 0
        | _BV(0) //from now on listen to your own start signal
      | _BV(4)  //use shaddow motion profiles
      | _BV(5)  //target reached triggers start event
      | _BV(6)  //target reached triggers start event
      | _BV(10) //immediate start
      | _BV(11)  // the shaddow registers cycle
      );   
    }
  }

  if (!configure_shadow) {
    write43x(cs_pin,V_MAX_REGISTER,FIXED_24_8_MAKE(vMax)); //set the velocity 
    write43x(cs_pin, A_MAX_REGISTER,aMax); //set maximum acceleration
    write43x(cs_pin, D_MAX_REGISTER,dMax); //set maximum deceleration
    write43x(cs_pin,BOW_1_REGISTER,startBow);
    write43x(cs_pin,BOW_2_REGISTER,endBow);
    write43x(cs_pin,BOW_3_REGISTER,endBow);
    write43x(cs_pin,BOW_4_REGISTER,startBow);
  } 
  else {
    write43x(cs_pin,SH_V_MAX_REGISTER,FIXED_24_8_MAKE(vMax)); //set the velocity 
    write43x(cs_pin, SH_A_MAX_REGISTER,aMax); //set maximum acceleration
    write43x(cs_pin, SH_D_MAX_REGISTER,dMax); //set maximum deceleration
    write43x(cs_pin,SH_BOW_1_REGISTER,startBow);
    write43x(cs_pin,SH_BOW_2_REGISTER,endBow);
    write43x(cs_pin,SH_BOW_3_REGISTER,endBow);
    write43x(cs_pin,SH_BOW_4_REGISTER,startBow);
  }
  write43x(cs_pin,X_TARGET_REGISTER,pos);
}








