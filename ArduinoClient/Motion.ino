volatile boolean currently_running = false;

void startMotion() {
  currently_running = true;
  //TODO initialize drivers??
}

void checkMotion() {
  if (currently_running) {
    if (moveQueue.count()>0) {
      //initiate movement
      //TODO don'T we habve to wait until the queue contains a complete move command??
    } else {
      //we are finished here
    }
  }
}
  

