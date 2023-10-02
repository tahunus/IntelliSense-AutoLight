int getGestureIndex(uint16_t gestures) {
  int count = 0;
  int index = -1;
  for (int i = 0; i < 16; i++) {
    if (gestures & (1 << i)) { //chk if bit is set
      count++;
      if (count > 1) {
        debug_println("ERROR: More than one gesture detected.");
        debug_print("BIN: ");debug_println(gestures, BIN);
        return -1;
      }
      index = i; 
    }
  }
  return index; //returns the index of the set bit or -1 if no bit was set
}

void getGestures() { 
  if (gestureSensor.getDataReady()) { //Get if a gesture is detected
    uint16_t currGesture = gestureSensor.getGesturesState(); // bits of gesture(s) detected
    int gestureIndex = getGestureIndex(currGesture); //convert bits to index value range 0..15 or -1
    debug_printf("Gesture Index: %d\n", gestureIndex);
    std::set<int> nonConfirmGestures = {2,3,6,7};
    std::set<int> confirmGestures = {0,1,4};

    if (millis() - gestureTimer > twoStepTimeout) initialGesture = -1; //after X sec of ANY gesture, reset two-step
    
    if ((confirmGestures.find(gestureIndex) != confirmGestures.end()) && initialGesture == -1 ) { // 0,1 & 4 need confirm gestures
      debug_println("1st Gesture");
      fastBlink(L_GREEN);
      gestureTimer = millis(); //start timer for two-step confirmation
      initialGesture = gestureIndex; //first gesture of two-step
    }else {
      if ( (nonConfirmGestures.find(gestureIndex) != nonConfirmGestures.end()) ||  //it's 2,3,6 or 7
           (gestureIndex == 4 && millis() - gestureTimer < twoStepTimeout) ) {     //it's 4 within time limit
        if (gestureIndex == 4) gestureIndex = initialGesture; // 0,1, or 4 is initialGesture
        fastBlink(L_BLUE);
        switch(gestureIndex) {
          case 0: 
            debug_println("Up");
            if (lampState == LAMP_ON) roomOne.setLampState(BRIGHTER);
            else roomOne.setLampState(LAMP_ON);
            break;
          case 1: 
            debug_println("Down");
            roomOne.setLampState(DIMMER);
            break;
          case 2: 
            debug_println("Left");
            roomOne.setLampState(WARMER);
            break;
          case 3: 
            debug_println("Right");
            roomOne.setLampState(COOLER);
            break;
          case 4: 
            debug_println("Forward");
            if (lampState == LAMP_ON) {
              roomOne.setLampState(LAMP_OFF);
              blockBlink(L_YELLOW,4,2); //allow user to step out of view of sensors for 6 sec (4 slow + 2 fast blink)
            }
            break;
          case 6: 
            debug_println("Clockwise");
            currLightColor++;
            if (currLightColor > toggleColors-1) currLightColor = 0;
            roomOne.setLampState(SET_COLOR);
            break;
          case 7: 
            debug_println("Counterclockwise");
            currLightColor--;
            if (currLightColor < 0) currLightColor = toggleColors-1;
            roomOne.setLampState(SET_COLOR);
            break;
          default: 
            debug_printf("UnChecked Gesture: %d", gestureIndex); break;
        }
      }
      initialGesture = -1; //reset two-step confirmation after action or no valid confirm gesture detected in time interval
    }
  }
}
