#define DEBUG //if not defined, all serial output statments become empty in compiled output
#ifdef DEBUG
  #define debug_print(x)      Serial.print(x)
  #define debug_println(...)  Serial.println(__VA_ARGS__)
  #define debug_printf(...)   Serial.printf(__VA_ARGS__)
  #define debug_write(...)   {Serial.print("[");Serial.print(__FUNCTION__);Serial.print("():");Serial.print(__LINE__);Serial.print("] ");Serial.println(__VA_ARGS__);}
#else
  #define debug_print(x)
  #define debug_println(...)
  #define debug_printf(...)
  #define debug_write(...)
#endif

#include "declarations.h"

#include "faceRecognition.h"
faceEnrollment faceSensor;  //created as class as training exercise on the subject and to encapsulate variables
#include "presenceDetection.h"
//(sensorType, timeBetweenSamples, pin, Offthreshold, countLimit, lampOnTime, lampOffTime, bufferSize, debugMode)
SensorReader PIR(PIR_SENSOR,    250, A1, 0.85, 5, 750, 25000, 10, false); //PIR SR602 has retrigger time 2.5sec
SensorReader PSE(PERSON_SENSOR, 250, 99, 0.90, 5, 500, 20000, 20, false); //lampOnTime=500 -> 2 continous samples
#include "lightsControl.h"
room roomOne("Just a Label");
#include "gestureDetection.h" 
#include "webServerBE.h"
#include "webServerFE.h"

void setup() {
  #ifdef DEBUG 
    Serial.begin(115200);
    delay(1000);
  #endif
  debug_println("---Setup Started---");

  onBoardLED.begin(RGBLED_GPIO, 1, false); 
  onBoardLED.setPixel(0, L_GREEN, true); //Green LED during boot & WiFi provisioning
  onBoardLED.brightness(LED_bright, true);

  pinMode(FACT_RESET_PIN, INPUT_PULLUP); //for factory reset, incl wifi credentials. PIN 38 on Feather V2
  pinMode(LED_BUILTIN, OUTPUT); //pin 13 on V2, red LED

  WiFi.mode(WIFI_STA);
  WiFiManager wm; 

  #ifndef DEBUG 
    wm.setDebugOutput(false); //debug is enabled by default
  #endif
  wm.setConfigPortalTimeout(180); //auto close configportal after n seconds
  wm.setConnectTimeout(30); //how long to try to connect for before continuing 
  wm.setConnectRetries(3); //force retries after wait failure exit
  wm.setMinimumSignalQuality(10); //will not show networks under 10% signal quality
  bool res = wm.autoConnect("WiZConnect_NET"); //blocking code: either start AP provisioning or connect as STA
  if(!res) {
    debug_write("Timeout: failed to connect as AP or STA");
    onBoardLED.brightness(LED_bright); //red light to indicate reboot will happen in 3,2,1
    onBoardLED.setPixel(0, L_RED, 1);
    //onBoardLED.show(); //-----------------------------------------redundant??
    delay(4000);
    ESP.restart();
  }else { 
    debug_println("connected to WiFi..");
    wm.stopConfigPortal();
    wm.stopWebPortal();
    MDNS.end();
  }

  WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  //timer to blink LED during non-WiFi setup chores. During WiFi chores is solid ON
  bootLED_timer = timerBegin(0, 80, true); //timer #0, 80=1MHz (once per microsecond), count up
  timerAttachInterrupt(bootLED_timer, &onTimer, true);
  timerAlarmWrite(bootLED_timer, 300000, true); //value of timer counter in which to generate timer interrupt, true=reload timer
  timerAlarmEnable(bootLED_timer);

  unsigned long sensorInitTime = millis();

  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  while(gestureSensor.begin() != 0){
    debug_println("Gesture Sensor init ...");
    delay(500);
  }

  gestureSensor.enGestures(GESTURE_UP|GESTURE_DOWN|GESTURE_LEFT|GESTURE_RIGHT|GESTURE_FORWARD|
                           GESTURE_CLOCKWISE|GESTURE_COUNTERCLOCKWISE);  
  gestureSensor.setUdlrWin(30, 30);   //detection window size 1..30 Up-Down, Left-Right
  gestureSensor.setLeftRange(25);     //how far hand should move l/r/u/d to recognize as gesture
  gestureSensor.setRightRange(25);    //... range 5..25 & less than detection window
  gestureSensor.setUpRange(25);       //same
  gestureSensor.setDownRange(25);     //same
  gestureSensor.setForwardRange(5);   //how far hand should move f/b to recognize as gesture
  gestureSensor.setCwsAngle(16);      //count * 22.5 = degrees to trigger.  31*22.5= 2 turns
  gestureSensor.setCcwAngle(16);      //... range 1..31 default 16 = 360 degrees

  pinMode(ENROLL_GPIO, INPUT_PULLUP); //Pin 39 on Huzzah V2 for button
  person_sensor_write_reg(PERSON_SENSOR_REG_PERSIST_IDS, 1); //Store any recognized IDs even when unpowered.
  person_sensor_write_reg(PERSON_SENSOR_REG_ENABLE_ID, 1);   //enable ID Model
  person_sensor_write_reg(PERSON_SENSOR_REG_DEBUG_MODE, 1);  //enable LED indicator

  knownFaceActioned = false;

  wizWebServer.on("/", handleRoot);
  wizWebServer.on("/styles.css", handleCSS);
  wizWebServer.on("/script.js", handleJS);
  wizWebServer.on("/discoverLights", handleDiscoverLights);
  wizWebServer.on("/getFaceIDs", handleGetFaceIDs);
  wizWebServer.on("/getTODtemps", handleGetTODtemps);
  wizWebServer.on("/saveData", handleSaveData);
  wizWebServer.begin();
  debug_println("HTTP server started");

  if (!MDNS.begin("wizconfig")) debug_println("ERROR setting up MDNS responder");
  else {
    debug_println("mDNS responder started");
    MDNS.addService("http", "tcp", 80);
  }

  loadAllFiles();
  getReferenceHour();

  while (millis() - sensorInitTime < 3000) delay(10); //give sensors 3 secs to stabilize: mostly IR-based sensors
  debug_println("All Sensors Stable\n");
  
  timerDetachInterrupt(bootLED_timer);
  timerAlarmDisable(bootLED_timer);
  timerEnd(bootLED_timer);

  //setCpuFrequencyMhz(80); //Min Freq for WiFi & BLE to save power. If webserver starts timeout-ing, increase
  //debug_println("\n\nCPU Frequency set @ 80MHz");

  onBoardLED.brightness(0, true); 
  debug_println("---Setup Done---");
  //--------------------------------------TO DO: check state of lamps to define lampState's starting value and avoid 
  //--------------------------------------not LAMP_OFF because it thinks it's already off
}  

void loop() {
  if (digitalRead(FACT_RESET_PIN) == LOW) chkFactoryReset();    

  wizWebServer.handleClient();

  firstPress = true;
  while (buttonPressed()) //blocking code so no other action takes place during face enrollment
    faceSensor.enrollFaces(); 
  if (!firstPress) { //changed to false if enrollFaces() was called
    onBoardLED.brightness(0, true);  
    //onBoardLED.show();
    if (enrolledFace) //inits as false when faceEnrollment object is created and in firstPress
      next_unused_id++; 
  }

  if (totalSelectedLights >= 0) { //if no lights enrolled, there's nothing to check for. Starts at -1
    PIR.chkSensor();
    PSE.chkSensor();
    if (lampState == LAMP_ON) {
      if (PIR.lampAction() == LAMP_OFF && PSE.lampAction() == LAMP_OFF) { //first check if must turn off
        roomOne.setLampState(LAMP_OFF);
      }else { 
        if (knownFace && !knownFaceActioned) { //these two variables will reset with any ON/OFF
          debug_printf("FOUND FACE #%d\n", knownFaceID);
          roomOne.setLampState(SET_SCENE);
        }else { //chk if need to set circadian rhythm
          int localHr = getHour();
          if (lastTempTOD != localHr) {
            lastTempTOD = localHr;
            debug_printf("SET_TEMP Time in loop(): %d\n", lastTempTOD);
            roomOne.setLampState(SET_TEMP);
          }
        }    
      }
    }else {
      if (PIR.lampAction() == LAMP_ON && PSE.lampAction() == LAMP_ON) 
        roomOne.setLampState(LAMP_ON);
    }   
    getGestures();
  }

  chkBattery();
}
