const uint8_t ENROLL_GPIO = A0;
const bool MOUNT_FS = true;

bool buttonPressed () {
  int c = 0;
  int prev_read = 9;
  int curr_read = 9;
  while (c < 7) {
    curr_read = digitalRead(ENROLL_GPIO);
    if (curr_read == prev_read) c++; else delay(2);
    prev_read = curr_read;
  }
  return !curr_read;
}

void saveFaceIDs(bool mountLittleFS = false) {
  if (mountLittleFS && !mountedLittleFS()) {
    debug_write("ERROR: mount LittleFS in erase Enrolled Faces");
    return;
  }

  StaticJsonDocument<512> doc; //size validated with arduinoJson assistant
  JsonArray faceArray = doc.createNestedArray("faceIDs"); // 'faceIDs' is the JSON key also used in js
  for (int i = 0; i < maxFaces; i++) {
    JsonObject face = faceArray.createNestedObject();
    face["ID"] = enrolledFaces[i].ID;
    face["label"] = enrolledFaces[i].label;
    JsonArray sceneArray = face.createNestedArray("scenes");
    for (int j = 0; j < 3; j++) {
      sceneArray.add(enrolledFaces[i].scene[j]);
    }
  }
  File file = LittleFS.open("/enrolledFaces.txt", "w+");
  if (!file) {
    debug_write("enrolledFaces.txt was non existent: newly created");
  }
  serializeJson(doc, file);
  //serializeJsonPretty(doc, Serial); //alternate form for debug print
  debug_println("Done saving Face IDs");
  file.close();

  if (mountLittleFS) LittleFS.end();
}

class faceEnrollment {
  private:
    const uint16_t SAMPLE_DELAY_MS = 180;      // 1000/x=fps. Not using global timer re: its blocking plus sample must be faster
    unsigned long lastSampleTime = 0;
    person_sensor_results_t resultsFR = {};    //-------------------it's own copy for the same reasons FR= Face Enrollment
    const uint16_t TIME_TO_ENROLL = 4000;      //time in ms to confirm it's a new face and train (calibrate) the model with it
    const uint16_t MARGIN_TO_ABORT = 3000;     //time in ms as buffer to let go of button after calibration
    const uint16_t TIME_TO_ERASE = 3000;       //time in ms for warning of erasing all faces
    const uint16_t BLINK_RATE = 300;           //RGB LED blink rate in ms
    unsigned long buttonTimer = 0;             //millis() when button was first pressed thus calling this function
    bool calibrationOk = false;                //flag set when new face calibration (i.e. ML model trained) is successful
    bool IDsErased = false;
    int unrecognizedFaceCount = 0;             // sum of : valid, single face, but not recognized
    const uint16_t UNRECOGNIZED_THRESHOLD = 4; // time elapsed = threshold x sample_delay ... 800
                                               // calibration time = time_to_enroll - time elapsed ... 3200

    enum states {
      INITIATE,
      CALIBRATE,
      SAVE_DATA,
      ERASE_WARNING,
      ERASE_DATA
    };
    states currentState = INITIATE;              // current state of Finit State Machine
    
    void blinkRGB(crgb_t RGBcolor) {
      if (((millis() - buttonTimer)/BLINK_RATE) % 2 == 1) { 
        onBoardLED.brightness(0, true); 
        //onBoardLED.show();
      }else {
        onBoardLED.brightness(LED_bright); 
        onBoardLED.setPixel(0, RGBcolor, 1);
      }
    }

    void updateFacesArray(int thisIndex, String thisLabel) {
      enrolledFaces[thisIndex].ID = thisIndex + 1;
      enrolledFaces[thisIndex].label = thisLabel;
      enrolledFaces[thisIndex].scene[0] = 11; //Warm_White = default
      enrolledFaces[thisIndex].scene[1] = 12; //Daylight   = default
      enrolledFaces[thisIndex].scene[2] = 6;  //Cozy       = default
    }

    //debug vars & functions
    uint16_t dMsgCounter[5][5]; 

    void debugMsg(int msgType, int msgNum) {
    #ifdef DEBUG 
      if (dMsgCounter[msgType][msgNum] == 0) { // display only once
        if (msgType == 4) {
          if (msgNum == 0) debug_println("Getting ready to erase faces. Release button to abort");
          if (msgNum == 1) debug_println("All enrolled faces have been erased");
        }
        if (msgType == 3) {
          debug_print("Current face has been enrolled with ID# "); 
          debug_println(msgNum);
        }
        if (msgType == 2) {
          if (msgNum == 0) 
            debug_println("Enroll failed: could not get 4 samples of unrecognized. Face Exists");  
          else             
            debug_println("Enroll failed: bad samples from Calibration"); 
        }
        if (msgType == 0) debug_print("A: "); 
        if (msgType == 1) debug_print("B: ");
        if (msgType < 2) {
          switch (msgNum) {
            case 0: debug_println("ID Count limit reached"); break;
            case 1: debug_println("Not Facing forward"); break;
            case 2: debug_println("Face already exists"); break;
            case 3: debug_println("Low Confidence of existance of face"); break;
            case 4: debug_println("# faces > 1 or = 0"); break;
          }
        }   
      }   
      dMsgCounter[msgType][msgNum] ++;
    #endif
    }

    void clear_dMsgCounter () {
      for (int i=0; i<5; i++)
        for (int j=0; j<5; j++)
          dMsgCounter[i][j] = 0;
    }

  public:
    faceEnrollment()  {   }    
    ~faceEnrollment() {   } //>>>>> clean-up when the enrollment process ends, like close files or deallocate dynamic memory
    
    void enrollFaces() {
      if (firstPress) currentState = INITIATE;
      switch (currentState) { // state machine

        case INITIATE:
          firstPress = false;
          buttonTimer = millis();
          enrolledFace = false;
          calibrationOk = false;
          unrecognizedFaceCount = 0;
          IDsErased = false;
          onBoardLED.brightness(0, true);
          //onBoardLED.show();  
          clear_dMsgCounter();
          currentState = CALIBRATE;
          debug_println("Enrolling face. Release button to abort");
          break;

        case CALIBRATE:
          blinkRGB(L_BLUE);
          // read sensor
          if (millis() - lastSampleTime >= SAMPLE_DELAY_MS) {
            lastSampleTime = millis();
            resultsFR = {};
            bool i2c_isEmpty = !person_sensor_read(&resultsFR); 
            if (i2c_isEmpty) {
              debug_write("ERROR: No person sensor results found on the i2c bus");
              return;
            }
            //chk for THRESHOLD number of unrecognized sample face that qualify as new
            if (unrecognizedFaceCount < UNRECOGNIZED_THRESHOLD)
              if (resultsFR.num_faces == 1) 
                if (resultsFR.faces[0].box_confidence >= 95)
                  if (resultsFR.faces[0].id_confidence <= 90)
                    if (resultsFR.faces[0].is_facing == 1)
                      if (next_unused_id < PERSON_SENSOR_MAX_IDS_COUNT) {
                        unrecognizedFaceCount ++;
                        debug_print("Unrecognized Face Count: ");
                        debug_println(unrecognizedFaceCount);
                      }
                      else debugMsg(0, 0); //ID Count limit reached
                    else debugMsg(0, 1); //Not Facing forward
                  else debugMsg(0, 2); //Face already exists
                else debugMsg(0, 3); //Low Confidence of existance of face
              else debugMsg(0, 4); //# faces > 1 or = 0
            else
            //having at least UNRECOGNIZED_THRESHOLD face readings (certainty of new), train the model with several readings of such face
              if (resultsFR.num_faces == 1) 
                if (resultsFR.faces[0].box_confidence >= 90) 
                  if (resultsFR.faces[0].is_facing == 1) {
                    debug_println("Enrolling face...");
                    person_sensor_write_reg(PERSON_SENSOR_REG_CALIBRATE_ID, next_unused_id);
                    calibrationOk = true;
                  }
                  else debugMsg(1, 1); //Not Facing forward
                else debugMsg(1, 3); //Low Confidence of existance of face
              else debugMsg(1, 4); //# faces > 1 or = 0
          }
          if (millis() - buttonTimer > TIME_TO_ENROLL) currentState = SAVE_DATA;
          break;

        case SAVE_DATA:
          if (calibrationOk) {
            if (!enrolledFace) {    //save data to file once even if buttonPressed = true
              updateFacesArray(next_unused_id - 1, "Known Face #" + String(next_unused_id));
              saveFaceIDs(MOUNT_FS);
              enrolledFace = true;
              onBoardLED.brightness(LED_bright);               
              onBoardLED.setPixel(0, L_BLUE, 1); //change BLUE blink for fixed BLUE
              debugMsg(3, next_unused_id); // Current face has been enrolled with ID# 
            }
          }else {
            onBoardLED.brightness(LED_bright); // ------ another color for each fail case??        
            onBoardLED.setPixel(0, L_YELLOW, 1); //change BLUE blink for fixed YELLOW
            if (unrecognizedFaceCount < UNRECOGNIZED_THRESHOLD) 
              debugMsg(2, 0); //Enroll failed: no 4 samples of unrecognized, face exists
            else 
              debugMsg(2, 1); //Enroll failed: bad samples from Calibration, no valid samples
          }
          if (millis() - buttonTimer > TIME_TO_ENROLL + MARGIN_TO_ABORT) currentState = ERASE_WARNING;
          break;

        case ERASE_WARNING:
          debugMsg(4,0); //Getting ready to erase faces. Release button to abort
          blinkRGB(L_RED);
          if (millis() - buttonTimer > TIME_TO_ENROLL + MARGIN_TO_ABORT + TIME_TO_ERASE) currentState = ERASE_DATA;
          break;

        case ERASE_DATA:
          if (!IDsErased) { // to call write_reg only once
            person_sensor_write_reg(PERSON_SENSOR_REG_ERASE_IDS, 1);
            next_unused_id = 1;
            IDsErased = true;
            for (int i=0; i<maxFaces; i++) updateFacesArray(i, "<available>");
            saveFaceIDs(MOUNT_FS);
            onBoardLED.brightness(LED_bright);  //change RED blink for fixed RED
            onBoardLED.setPixel(0, L_RED, 1);        
            debugMsg(4, 1); // All enrolled faces have been erased
          }
          break;
      }
    }

}; // end of class
