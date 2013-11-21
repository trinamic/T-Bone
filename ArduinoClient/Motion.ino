volatile boolean next_move_prepared = false;
volatile boolean prepare_shaddow_registers = false;

void startMotion() {
  in_motion = true;
  next_move_prepared=false; //TODO in theory this is not needed  
  prepare_shaddow_registers = false;
  //TODO initialize drivers??
}

void stopMotion() {
  in_motion = false;
}

void checkMotion() {
  if (in_motion && !next_move_prepared) {
    if (moveQueue.count()>0) {
      byte moving_motors=0;
      //analysze the movement (nad take a look at the next
      movement move = moveQueue.pop();
      movement followers[MAX_FOLLOWING_MOTORS];
      //check out which momtors are geared with this

#ifdef DEBUG_MOTION
      Serial.print(F("Moving motor "));
      Serial.print(move.motor,DEC);
      Serial.print(F(" to "));
      Serial.print(move.data.move.target);
      Serial.print(F(" at "));
      Serial.print(move.data.move.vmax);
#endif
      moveMotor(move.motor, move.data.move.target,move.data.move.vmax, 1, prepare_shaddow_registers);
      moving_motors |= _BV(move.motor);

      movement follower;
      do {
        follower = moveQueue.peek();
        if (follower.type==followmotor) {  
          moveQueue.pop();
#ifdef DEBUG_MOTION
          Serial.print(F(", following motor "));
          Serial.print(follower.motor,DEC);
          Serial.print(F(" by "));
          Serial.print(follower.data.follow.factor,DEC);
#endif
          moveMotor(follower.motor, follower.data.follow.target,move.data.move.vmax, follower.data.follow.factor, prepare_shaddow_registers);
          moving_motors |= _BV(follower.motor);
        }
      } 
      while (follower.type == followmotor);

      for (char i; i<nr_of_motors;i++) {
        //configure all non moving motors to stop
        if (moving_motors && _BV(i) == 0) {
          moveMotor(i, 0,0, 0, prepare_shaddow_registers);
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

        //give all motors a nice start signal
        if (!prepare_shaddow_registers) {
          write43x(motors[i].cs_pin, START_CONFIG_REGISTER, 0
            | _BV(0) //xtarget requires start
          | _BV(1) //vmax requires start
          | _BV(5) //external start is an start
          | _BV(10)//immediate start
          );   
        } 
        else {
          write43x(motors[i].cs_pin, START_CONFIG_REGISTER, 0
            | _BV(0) //from now on listen to your own start signal
          | _BV(4)  //use shaddow motion profiles
          | _BV(5)  //target reached triggers start event
          | _BV(6)  //target reached triggers start event
          | _BV(10) //immediate start
          | _BV(11)  // the shaddow registers cycle
          | _BV(13)  // coordinate yourself with busy starts
          );   
        }

      }

      //for the first move we need to configure everything a bit 
      if (!prepare_shaddow_registers) {
        //and carefully trigger the start pin 
        digitalWrite(start_signal_pin,HIGH);
        pinMode(start_signal_pin,OUTPUT);
        digitalWrite(start_signal_pin,LOW);
        pinMode(start_signal_pin,INPUT);
        //From now on the motor drivers move themeself - or somethinglike this
        attachInterrupt(start_signal_pin , target_reached_handler, RISING);
        //and we need to prepare the next move for the shadow registers
        prepare_shaddow_registers = true;
        next_move_prepared = false;
      } 
      else {
        //ok normally we can relax until the enxt start event occured
        next_move_prepared = true;
      }
    } 
    else {
      //we are finished here
      stopMotion();
    }
  }
}


void target_reached_handler() {
  next_move_prepared=false;
}











































