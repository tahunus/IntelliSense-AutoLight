//Libraries and their variables ============================================================================================
//Communications
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <WiFi.h> //------------------------------------included in WiFiManager. Try w/O this one
#include <ESPmDNS.h>    //------------------------------ifdef WM_MDNS included in WiFiManager line 107 .cpp
#include <WebServer.h>  //------------------------------included in WiFiManager
WebServer wizWebServer(80);
#include "AsyncUDP.h"
const int WiZ_PORT = 38899;

//File management
#include <LittleFS.h>
bool mountedLittleFS() {
  if (!LittleFS.begin()) {
    debug_write("ERROR: Failed to mount LittleFS");
    return false;
  }else
    return true;
}

//Sensors
#include <Wire.h>  // PSE connector order = GND, 3.3V, SDA, SCL
const uint8_t I2C_SDA = 22;
const uint8_t I2C_SCL = 20;
#include "person_sensor.h"
#include "DFRobot_GR10_30.h"
DFRobot_GR10_30 gestureSensor(/*addr = */GR10_30_DEVICE_ADDR, /*pWire = */&Wire);
int initialGesture = -1;
unsigned long gestureTimer = 0;
const int twoStepTimeout = 4000; //time limit to get 2nd gesture for two-step confirmation;

//Process support
#include <ArduinoJson.h>
#include "IPAddress.h"
#include <ctime>
const int TOD_1 = 7; //time slots in 24hr format to set scenes for faceIDs
const int TOD_2 = 14;
const int TOD_3 = 21;
#include <vector>
#include <set>
#include <LiteLED.h> // https://github.com/Xylopyrographer/LiteLED
LiteLED onBoardLED(LED_STRIP_WS2812, 0, RMT_CHANNEL_0);  // type, isRGBW?
const uint8_t RGBLED_GPIO = 0;  // 8=on the ESP32-C3-DevKitC-02, 0=on Huzzah V2
uint8_t LED_bright = 5; // 0..255
const crgb_t L_RED = 0xff0000; // format 0xRRGGBB
const crgb_t L_ORANGE = 0xffa500;
const crgb_t L_GREEN = 0x00ff00;
const crgb_t L_BLUE = 0x0000ff;
const crgb_t L_YELLOW = 0xffc800;
const crgb_t L_WHITE = 0xe0e0e0;
#include "esp32-hal.h"
//>>>>https://techtutorialsx.com/2017/10/07/esp32-arduino-timer-interrupts/
//>>>>https://circuitdigest.com/microcontroller-projects/esp32-timers-and-timer-interrupts
//>>>>https://espressif-docs.readthedocs-hosted.com/projects/arduino-esp32/en/latest/api/timer.html#
hw_timer_t *bootLED_timer = NULL;
volatile bool bootLedState = true;
void IRAM_ATTR onTimer(){
  bootLedState = !bootLedState;
  if (bootLedState) onBoardLED.brightness(LED_bright, true);
  else onBoardLED.brightness(0, true);
}

//Global variables==========================================================================================================
//Webserver Backend & Frontend
int deviceCount = 0; // Total discovered devices
const int maxDevices = 10; // Maximum number of devices discovered in network to store
struct DeviceInfo { 
  IPAddress ipAddress;
  unsigned int port;
  String macAddress;
  int rssi;
  bool state;
  bool enrolled;
};
DeviceInfo devices[maxDevices]; // Array to store discovered devices

//Lights control
int currLightColor = 0;
enum lampActions {LAMP_OFF, LAMP_ON, BRIGHTER, DIMMER, WARMER, COOLER, SET_COLOR, SET_SCENE, SET_TEMP};
lampActions lampState = LAMP_OFF;
unsigned long referenceUTC = 0;
unsigned long referenceMillis = 0; 
String selectedLights[10] = {}; // to hold IPAddresses of selected lights
int totalSelectedLights = -1;
int temperTOD[15] = {0, 1, 1, 2, 2, 4, 4, 4, 3, 3, 2, 2, 1, 1, 0}; //temp per TOD circadian rhythm. TOD=6AM..8PM, index=0..14
int lastTempTOD = -1;
int temperatures[5] = {2700, 3400, 4000, 5200, 6500};
struct wizColor {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};
const int toggleColors = 5; //const will be used in code (gestureDetection)
wizColor lightColor[toggleColors] = { //lamp colors to toggle with CW & CCW gestures
    {255, 0, 0},      // Red
    {0, 255, 0},      // Dark Green
    {0,128,128},      // Greenish blue
    {0, 0, 255},      // Dark Blue
    {128, 0, 128}     // Purple
};

//Face recognition & enrollment and their scenes
const int maxFaces = 3; // Person Sensor can handle up to 7
int next_unused_id = 1;
bool enrolledFace = false;
bool firstPress = true;
bool knownFace;
int knownFaceID;
bool knownFaceActioned;
// These are all scenes according to WiZ. With XX the ones that will not be listed in website nor used. Total used = 13
// in script.js "XX" will mean skipped
//1XX=Ocean,     2=Romance,    3=Sunset,       4XX=Party,      5XX=Fireplace,  6=Cozy,         7=Forest,       8XX=Pastel Colors, 
//9XX=Wake up,   10XX=Bedtime, 11=Warm white,  12=Daylight,    13=Cool white,  14=Night light, 15=Focus,       16=Relax,    
//17XX=TrueClrs, 18=TV Time,   19XX=PlanGrw,   20XX=Spring,    21XX=Summer,    22XX=Fall,      23XX=Deep dive, 24XX=Jungle, 
//25XX=Mojito,   26XX=Club,    27XX=Christmas, 28XX=Halloween, 29=Candlelight, 30=Golden white
struct FacesInfo {
  int ID;
  String label;
  int scene[3];
};
FacesInfo enrolledFaces[maxFaces]; // enrolled faces and their preferences: enrolledFaces[1].scene[2] = 6 i.e. Cozy;

//System & battery management
#define FACT_RESET_PIN 38
#define BATT_PIN A13
unsigned long battTimer = 0; 
unsigned long battLEDTimer = 0;
bool battLEDState = LOW;
bool battOK = true;
const unsigned long FIVE_MINUTES = 5UL * 60UL * 1000UL;

//Global functions==========================================================================================================
void fastBlink(crgb_t LEDcolor, int reps = 1) {
  onBoardLED.setPixel(0, LEDcolor, 1);
  for (int i=0; i<reps; i++){
    onBoardLED.brightness(LED_bright, true);
    delay(70);
    onBoardLED.brightness(0, true);
    if (reps > 1) delay(50);
  }
}

void blockBlink(crgb_t LEDcolor, int secondsSlow, int secondsFast) { 
  unsigned long startTime = millis();
  onBoardLED.setPixel(0, LEDcolor, 1); 
  while (millis() - startTime < secondsSlow * 1000) { 
    onBoardLED.brightness(LED_bright, true);
    delay(500); 
    onBoardLED.brightness(0, true);  
    delay(500);
  }  
  startTime = millis();
  while (millis() - startTime < secondsFast * 1000) { 
    onBoardLED.brightness(LED_bright, true);
    delay(250); 
    onBoardLED.brightness(0, true);  
    delay(250);
  }
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  debug_print("WiFi connection lost: ");
  debug_println(info.wifi_sta_disconnected.reason);
  for (int i=0; i<5; i++) { // blink RED 4sec as signal of error and delay to reboot
    onBoardLED.setPixel(0, L_RED, true); 
    onBoardLED.brightness(LED_bright, true);
    delay(400);
    onBoardLED.brightness(0, true); 
    delay(400);
  }
  ESP.restart();
}

void loadAllFiles() {
  // load initial state of enrolled Lights and enrolled faces
  if (mountedLittleFS()) { 
    File file = LittleFS.open("/enrolledLights.txt", "r"); 
    if (file) { // if file length = 0 or no file, totalSelectedLights will be -1 (startup value) and array will be null
      totalSelectedLights = -1;
      while (file.available()) {
        totalSelectedLights ++;
        String ipAddressStr = file.readStringUntil('\n');
        ipAddressStr.trim();
        selectedLights[totalSelectedLights] = ipAddressStr;
      }
    }
    file.close();
    file = LittleFS.open("/enrolledFaces.txt", "r");
    next_unused_id = 1;
    if (file) {
      StaticJsonDocument<512> doc; //size validated with arduinoJson assistant
      DeserializationError error = deserializeJson(doc, file);
      if (error) {
        debug_write("ERROR: Failed to parse enrolledFaces.txt in SETUP");
        file.close();
        return;
      }
      file.close();
      JsonArray faceArray = doc["faceIDs"];
      for (int i = 0; i < faceArray.size() && i < maxFaces; i++) {
        JsonObject face = faceArray[i];
        enrolledFaces[i].ID = face["ID"];
        enrolledFaces[i].label = face["label"].as<String>();
        if (enrolledFaces[i].label != "<available>")
          next_unused_id++;
        JsonArray sceneArray = face["scenes"];
        for (int j = 0; j < sceneArray.size() && j < 3; j++) {
            enrolledFaces[i].scene[j] = sceneArray[j].as<int>();
        }
      }
    }else {
      person_sensor_write_reg(PERSON_SENSOR_REG_ERASE_IDS, 1);
      for (int i=0; i<maxFaces; i++) {
        enrolledFaces[i].ID = i + 1; //enrolledFaces[] array is 0 indexed but ID as defined in person_sensor starts at 1
        enrolledFaces[i].label = "<available>";
        enrolledFaces[i].scene[0] = 11; //Warm_White
        enrolledFaces[i].scene[1] = 12; //Daylight
        enrolledFaces[i].scene[2] = 6; //Cozy
      }
    }
    file.close();
    file = LittleFS.open("/TODtemps.txt", "r"); 
    if (file) { // if file length = 0 or no file, will fill with defaults from initialization
      int i=0;
      while (file.available()) {
        String tempVal = file.readStringUntil('\n');
        temperTOD[i] = tempVal.toInt();
        i++;
      }
    }
    file.close();
  }
  LittleFS.end();
}

void chkFactoryReset() { 
  onBoardLED.setPixel(0, L_RED, true); 
  onBoardLED.brightness(LED_bright, true);
  //onBoardLED.show();
  unsigned long ledTimer = millis(); 
  bool ledState = true;
 
  unsigned long startTime = millis();
  bool buttonStillPressed = true;
  while (millis() - startTime < 2500) { // Block code For a period of 2.5 seconds
    if (digitalRead(FACT_RESET_PIN) == HIGH) { // If button is released within the 2.5 seconds
      buttonStillPressed = false;
      break;
    }
    if (millis() - ledTimer > 250) {
      ledTimer = millis();
      if (ledState) onBoardLED.brightness(0, true);
      else onBoardLED.brightness(LED_bright, true);
      ledState = !ledState;
    }
  }
  if (buttonStillPressed) { // If button was held down continuously for 2 seconds
    WiFiManager wm;
    wm.resetSettings();
    person_sensor_write_reg(PERSON_SENSOR_REG_ERASE_IDS, 1); // erase enrolled faces on person sensor
    if (mountedLittleFS()) {
      if (!LittleFS.remove("/enrolledLights.txt")) {debug_println("ERROR: erasing enrolledLights");}
      if (!LittleFS.remove("/enrolledFaces.txt")) {debug_println("ERROR: erasing enrolledFaces");}
      if (!LittleFS.remove("/TODtemps.txt")) {debug_println("ERROR: erasing TODtemps");}
    }
    LittleFS.end();
    delay(100);
    ESP.restart();
  } else {
    onBoardLED.brightness(0, true);
    //onBoardLED.show();
  }
  battTimer = millis();
}

void chkBattery() {
  if (millis() - battTimer > FIVE_MINUTES) { // chk battery every 5 minutes. Also works as debounce-ish for low Batt
    battTimer = millis();
    float measuredBatt = analogReadMilliVolts(BATT_PIN); //get ADC value for a given pin/ADC channel in millivolts
    measuredBatt *= 2; //double voltage divider divides measurement. need to multiply back
    measuredBatt /= 1000;
    debug_print("Batt: " ); debug_println(measuredBatt);
    if (measuredBatt < 3.0) {
      debug_println("entering deep sleep");
      //NEOPIXEL_I2C_POWER pin controls power to I2C and the NeoPixel LED in Feather V2, BUT peripherals connected to the 
      //3.3V pin will continue to receive power. Consider pull down the EN pin to turn off the 3.3v regulator
      pinMode(NEOPIXEL_I2C_POWER, OUTPUT);
      digitalWrite(NEOPIXEL_I2C_POWER, LOW);
      esp_deep_sleep_start();
    }
    if (measuredBatt < 3.4) { // 3.2V as cutoff for low loads on V2's regulator. 4.26V is measured max charge
      battLEDTimer = millis();
      battOK = false;
    }else {
      battOK = true;
      digitalWrite(LED_BUILTIN, LOW);
    }
  }
  if (!battOK) { // blink battery red LED
    if ((battLEDState == HIGH && millis() - battLEDTimer > 300 ) || // LED 300ms ON
        (battLEDState == LOW && millis() - battLEDTimer > 1300)) {  // LED 1300ms OFF
      battLEDTimer = millis();
      battLEDState = !battLEDState;
      digitalWrite(LED_BUILTIN, battLEDState); 
    }
  }
}

int getHour() {
  unsigned long currentUTC = referenceUTC + ((millis() - referenceMillis) / 1000);
  time_t adjustedTime = static_cast<time_t>(currentUTC);  //convert to EPOCH in C Time Library <ctime> format 
  struct tm timeinfo;
  gmtime_r(&adjustedTime, &timeinfo);  //Convert epoch time to broken-down time in UTC
  int localTime = timeinfo.tm_hour - 6; //convert UTC to Mexico City time
  if (localTime < 0) localTime = 24 + localTime;
  return localTime;  //Return the local hour GMT-6 in 0..23
}

void getReferenceHour() {
  unsigned long UDP_referenceUTC = 0;
  unsigned long UDP_referenceMillis = 0;
  AsyncUDP udp;
  bool packetReceived = false;

  if (totalSelectedLights >= 0) { //lights counter starts at -1
    StaticJsonDocument<128> TIME_responseJSON;
    //response UDP callback no need to capture refUTC & ref Millis since are globals
    udp.onPacket(  
      [&TIME_responseJSON, &packetReceived, &UDP_referenceUTC, &UDP_referenceMillis] 
      (AsyncUDPPacket packet) { 
        TIME_responseJSON.clear();
        DeserializationError error = deserializeJson(TIME_responseJSON, packet.data());
        if (error) {
          debug_write("ERROR: TIME Parsing failed: ");
          debug_println(error.c_str());
          return;
        }
        UDP_referenceUTC = TIME_responseJSON["result"]["utc"];
        UDP_referenceMillis = millis();
        packetReceived = true;
      }
    ); 
    //getTime UDP request 
    IPAddress tempIpAddress;
    tempIpAddress.fromString(selectedLights[0]); //get Time from 1st selected light in list
    StaticJsonDocument<96> jsonAskForTime;
    jsonAskForTime.clear();
    jsonAskForTime["method"] = "getTime";
    String stringAskForTime = "";
    serializeJson(jsonAskForTime, stringAskForTime);
    if(udp.connect(tempIpAddress, WiZ_PORT)) {
      debug_println("UDP connected for getTIME");
      udp.print(stringAskForTime);
    }
  } 

  const char* ntpServer1 = "pool.ntp.org";
  const char* ntpServer2 = "time.nist.gov";
  const long  gmtOffset_sec = 0;
  const int   daylightOffset_sec = 0;
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
  time_t now;
  bool NTPtimeReceived = false;

  unsigned long startTime = millis();
  while (!packetReceived && !NTPtimeReceived && (millis() - startTime < 5000)) { //Wait up to 5 seconds for any response
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 60)) { //request time continuosly for x ms. NOTE: If x>0 it's blocking. internally it has delay(10) 
    // see https://github.com/espressif/arduino-esp32/blob/master/cores/esp32/esp32-hal-time.c
      time(&now);
      NTPtimeReceived = true;
      referenceMillis = millis();
      referenceUTC = now;
      debug_print("NTP Time received w/UTC = ");
      debug_println(referenceUTC);
      debug_println(getHour());
    }
  }

  if (!NTPtimeReceived) {
    if (packetReceived) {
      referenceUTC = UDP_referenceUTC;
      referenceMillis = UDP_referenceMillis;
      debug_print("TIME UDP packet received w/UTC = ");
      debug_println(referenceUTC);
      debug_println(getHour());
    }else {
      debug_println("TIME UDP & NTP Timeout");
    } 
  }
  udp.close();
}

