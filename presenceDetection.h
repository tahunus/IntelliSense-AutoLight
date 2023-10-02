#define PIR_SENSOR 0     //simple label to avoid using magic numbers
#define PERSON_SENSOR 1  //simple label to avoid using magic numbers

class SensorReader {
  private:
    //parameters for constructor
    unsigned char sensorType;     // 0=PIR, 1=PS, ...
    const int timeBetweenSamples; // 0 for PIR, 200-ish for PS in millis()
    const unsigned char pirPin;   // A0=Huzzah V2. D203S UP=no motion w/7 sec block. SR602 DOWN=no motion w/2.5 sec block
    const float offThreshold;     // % of LOW's in rolling sum above which Lamp is turned off. Starts at Threshold+Margin
    const int offCountLimit;      // # of changes to go from (offTheshold + OffMargin) to just offThreshold
    const int lampOnTime;         // time in HIGH (time-out) to turn lamp on
    const int lampOffTime;        // time in LOW (time-out) to turn lamp off
    const int bufferSize;         // number of values for rolling sum of sumLow & sumHigh
    const bool DEBUG_MODE;

    //state-of-process variables
    float offMargin;                        // margin to go from 100% down to offthreshold
    bool currState, prevState;              // HI/LO for PIR, num_faces & box_confidence for PS
    unsigned long changeStarted, elapsedLowSinceChange, elapsedHighSinceChange;  // in millis
    unsigned long lastSampleTime;           // timer to read samples from sensor
    int countChanges;                       // from 0 to offCountLimit
    int bufferIndexHigh, bufferIndexLow;
    std::vector<float> bufferLow;           // size will be defined by bufferSize in the constructor
    std::vector<float> bufferHigh;          // size will be defined by bufferSize in the constructor
    float sumHigh, sumLow;                  // rolliing sum in seconds
    lampActions lampActionVar = LAMP_OFF;   // sensor's conclusion on what the action should be ON/OFF

    //>>>>> functions
    float percentLow() {
      return ((sumLow + float(elapsedLowSinceChange)) / (sumHigh + float(elapsedHighSinceChange) + 
               sumLow + float(elapsedLowSinceChange)));
    }

    void debugAddValue(int type) {
      if (!DEBUG_MODE) return;
      if (type == 0) {
        debug_printf("Sensor:%d RISE after:%2.1f change:%d", sensorType, elapsedLowSinceChange/1000.0, countChanges-1);
        debug_printf(" added to LO in index:%d with newSum LO:%3.1f", bufferIndexLow, sumLow);
      } else {
        debug_printf("Sensor:%d FALL after:%2.1f change:%d", sensorType,elapsedHighSinceChange/1000.0, countChanges-1);
        debug_printf(" added to HI in index:%d with newSum HI:%3.1f", bufferIndexHigh, sumHigh);
      }
      debug_printf(" >> %%Low:%1.2f, Lim:%1.2f\n", percentLow(), offThreshold + offMargin * (1.0 - (countChanges - 1) /
                    float(offCountLimit)));
    }

    void debugFacesData() {
      if (!DEBUG_MODE) return;
      for (int i = 0; i < results.num_faces; ++i) {
        const person_sensor_face_t* face = &results.faces[i];
        debug_printf("Face #%d w/confidence %d%%, ID #%d w/confidence:%d%%, facing:", i + 1, face->box_confidence, 
                      face->id, face->id_confidence);
        if (face->is_facing) debug_print("Yes, ");
        else {debug_print("No, "); }
        debug_printf("Box (%d, %d, %d, %d)\n", face->box_left, face->box_top, face->box_right, face->box_bottom);
      }
    }

    void debugLampAction(lampActions state, int type) {
      if (!DEBUG_MODE) return;
      if (state == LAMP_OFF) {
        if (type == 0)
          debug_printf("--Turn OFF by averages in Sensor:%d, pLow:%1.3f, Lim:%1.3f\n", sensorType, percentLow(),
                        (offThreshold + offMargin * (1.0 - (countChanges - 1) / float(offCountLimit))));
        else
          {debug_printf("--Turn OFF by elapsedSinceChange in Sensor:%d\n", sensorType);}
      } else {
        if (type == 0)
          {debug_printf("--Turn on by HIGH with percentLOW() < 0.15 in Sensor:%d\n", sensorType);}
        else
          {debug_printf("--Turn on by HIGH after elapsedSinceChange in Sensor:%d\n", sensorType);}
      }
    }

    void addValue(float newValue, int type) {
      if (newValue > 0) {
        if (type == LOW) {
          sumLow -= bufferLow[bufferIndexLow];   // Subtract the oldest value from the sum
          sumLow += newValue;                    // Add the new value to the sum
          bufferLow[bufferIndexLow] = newValue;  // Put the new value in the buffer
          debugAddValue(type);
          bufferIndexLow = (bufferIndexLow + 1) % bufferSize; //Move to next position in buffer, wrap around if req'd
        } else {
          sumHigh -= bufferHigh[bufferIndexHigh];  // Subtract the oldest value from the sum
          sumHigh += newValue;                     // Add the new value to the sum
          bufferHigh[bufferIndexHigh] = newValue;  // Put the new value in the buffer
          debugAddValue(type);
          bufferIndexHigh = (bufferIndexHigh + 1) % bufferSize; //Move to next position in buffer, wrap around if req'd
        };
      } else
        {debug_println("-->> newValue = 0");}
    }

    bool readSensor() {
      switch (sensorType) {
        case PIR_SENSOR: return digitalRead(pirPin); break;
        case PERSON_SENSOR:{
          results = {};
          bool i2cEmpty = !person_sensor_read(&results);
          if (i2cEmpty) {
            debug_write("ERROR: No person sensor results found on the i2c bus");
            return 0; }
          if (results.num_faces > 0 && results.faces[0].box_confidence > 70) {  //ML Model threshold at 52%
            if (results.faces[0].id_confidence > 95) { //--------------------------------fine tune error wrong face detected
              knownFace = true;
              knownFaceID = results.faces[0].id;
            }else {
              knownFace = false;
            }
            return HIGH;
          }else
            return LOW;
          break; }
        default: { 
          debug_write("ERROR: bad sensor definition");
          return 0; }
      }
    }

  public:
    person_sensor_results_t results = {};

    void resetValues() {
      sumLow = 0.0;
      sumHigh = 0.0;
      countChanges = 0;
      changeStarted = millis();
      elapsedLowSinceChange = 0;
      elapsedHighSinceChange = 0;
      bufferIndexLow = 0;
      bufferIndexHigh = 0;
      lastSampleTime = 0;
      bufferLow = std::vector<float>(bufferSize, 0);
      bufferHigh = std::vector<float>(bufferSize, 0);
      lampActionVar = lampState;
      // these three vars only apply to PSE (not PIR) and are global variables that should be reset
      // with every ON or OFF since a new known face might be present
      knownFace = false;
      knownFaceID = 0;
      knownFaceActioned = false;
    }

    void chkSensor() {
      if (millis() - lastSampleTime >= timeBetweenSamples) {
        lastSampleTime = millis();
        currState = readSensor();
      }

      //accrue elapsed time and change counts of states in sensor
      if (currState == HIGH) {
        if (prevState == LOW) {  //state changed: RISE
          changeStarted = millis();
          prevState = HIGH;
          countChanges++;
          addValue(elapsedLowSinceChange / 1000.0, LOW); 
          debugFacesData();
        } else {  //prevState == HIGH
          elapsedHighSinceChange = millis() - changeStarted;
        }
      } else { //currState == LOW
        if (prevState == HIGH) {  //state changed: FALL
          changeStarted = millis();
          prevState = LOW;
          countChanges++;
          addValue(elapsedHighSinceChange / 1000.0, HIGH);  
          debugFacesData();
        } else { //prevState == LOW
          elapsedLowSinceChange = millis() - changeStarted;
        }
      }

      //compare against timeouts and thresholds to determine the need for action
      if (lampActionVar == LAMP_ON && currState == LOW) {
        if (percentLow() > (offThreshold + offMargin * (1.0 - (countChanges - 1) / float(offCountLimit)))) {
          lampActionVar = LAMP_OFF;
          debugLampAction(LAMP_OFF, 0);
        } else {
          if ((millis() - changeStarted) > lampOffTime) {  // sensor remained "lapOffTime" seconds in LOW
            lampActionVar = LAMP_OFF;
            debugLampAction(LAMP_OFF, 1);
          }
        }
      }
      if (lampActionVar == LAMP_OFF && currState == HIGH) {
        if (percentLow() < 0.15) {
          lampActionVar = LAMP_ON;
          debugLampAction(LAMP_ON, 0);
        } else {
          if ((millis() - changeStarted) > lampOnTime) {  // sensor remained "lampOntIme" seconds in HIGH
            lampActionVar = LAMP_ON;
            debugLampAction(LAMP_ON, 1);
          }
        }
      }

      if (countChanges > offCountLimit) countChanges = offCountLimit;
    }

    bool lampAction() {
      return lampActionVar;
    }

    //>>>> parameters to receive values into constructor when object is created <<<<
    SensorReader(unsigned char sensorType, int timeBetweenSamples, unsigned char pin, float threshold, int countLimit,
                 int lampOnTime, int lampOffTime, int bufferSize, bool DEBUG_MODE)
      //>>>> initializer list where "data members" are initialized with the parameters <<<<
      : sensorType(sensorType), timeBetweenSamples(timeBetweenSamples), pirPin(pin), offThreshold(threshold),
        offCountLimit(countLimit), lampOnTime(lampOnTime), lampOffTime(lampOffTime), bufferSize(bufferSize), 
        DEBUG_MODE(DEBUG_MODE) 
      {
        offMargin = 1.0 - offThreshold;
        bufferLow.resize(bufferSize);
        bufferHigh.resize(bufferSize);
        pinMode(pirPin, INPUT);
        resetValues();
        prevState = LOW;
      }

};  //end of class
