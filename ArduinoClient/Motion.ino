volatile boolean is_running = false;

void startMotion() {
  in_motion = true;
  //TODO initialize drivers??
}

void stopMotion() {
  in_motion = false;
}

void checkMotion() {
  if (in_motion && !is_running) {
    if (moveQueue.count()>0) {
      //initiate movement
      //TODO don'T we habve to wait until the queue contains a complete move command??
      movement move = moveQueue.pop();
      movement gearings[MAX_GEARED_MOTORS];
      int gearingscount = 0;
      do {
        gearings[gearingscount] = moveQueue.peek();
        if (gearings[gearingscount].type==gearmotor) {  
          gearings[gearingscount] = moveQueue.pop();
        }
        gearingscount++;
      } 
      while (gearings[gearingscount].type);
      //TODO execute the movement
      is_running = true;
    } 
    else {
      //we are finished here
    }
  }
}



