# AutoSense.IntelliLight
An experiment in user interfaces for automated control of lights

Write here a summary, objectives, etc
## Components
1. [**Sensors**](#sensors)
2. [**MCU**](#mcu)
3. [**Other Parts**](#other-parts)
4. [**3D Printed Case**](#3d-printed-case)
## Functions
1. [**Auto On/Off via Presence Detection**](#auto-onoff-via-presence-detection)
	* [Detect Motion](#detect-motion)
	* [Detect faces](#detect-faces)
	* [Hysteresis](#hysteresis)
3. [**Auto Scene via Face Recognition**](#auto-scene-via-face-recognition)
	* [Face Recognition](#face-recognition)
4. [**Auto Temperature via Circadian Rhythm**](#auto-temperature-via-circadian-rhythm)
   	* [Time of Day](#time-of-day)
   	* [Temperature Table](#temperature-table)
5. [**Light Adjustments via Hand Gestures**](#light-adjustments-via-hand-gestures)
   	* [Gesture Recognition](#gesture-recognition)
6. [**System Configuration**](#system-configuration)
   	* [Enroll Lights](#enroll-lights)
   	* [Enroll Faces](#enroll-faces)
   	* [Circadian Rhythm Temperatures](#circadian-rhythm-temperatures)
   	* [Faces-Scenes Table](#faces-scenes-table)
   	* [Sensitivity Parameters](#sensitivity-parameters)

text to separate content

more text 

_____________________________________________________________________________________________________


## Auto On/Off via Presence Detection
To determine if the lights should be acted upon (ON/OFF), the accessory uses the PIR sensor to detect motion and the [Person Sensor](https://www.sparkfun.com/products/21231) to detect the presence of human faces. This means that simple motion, like someone passing by, will not trigger ON and that a human sitting perfectly still (like when reading), will not trigger OFF. Because the Person Sensor uses a camera, it is subject to variability based on ambient light and back light. By combining this sensor with the PIR sensor, false positives and negatives are greatly reduced. 

The class _SensorReader_, defined in _presenceDetection.h_, is used to manage the samples from both sensors. It's constructor parameters are:

| Variable | Description |
| -------- | ----------- |
| sensorType | 0 for PIR and 1 for Person Sensor |
| timeBetweenSamples | rate to read sensor in ms |
| pirPin | GPIO PIN for the input data from the PIR sensor |
| offThreshold | % of time spent on LOW below which lamp should be set to OFF |
| offCountLimit	| Ramp up |
| lampOnTime | M |
| lampOffTime |	Time limit to set lamp to OFF independent of hysteresis |
| bufferSize | Number of samples to keep in the running totals for averages |
| DEBUG_MODE | TRUE for detailed debug output to serial port (NOTE: DEBUG must de defined in RC3.ino) |
### Detect Motion



### Detect Faces
### Hysteresis
## Auto Scene via Face Recognition
### Face Recognition
## Auto Temperature via Circadian Rhythm
### Time of Day
### Temperature Table
## Light Adjustments via Hand Gestures
### Gesture Recognition
## System Configuration
### Enroll Lights
### Enroll Faces
### Circadian Rhythm Temperatures
### Faces-Scenes Table
### Sensitivity Parameters

_____________________________________________________________________________________________________

### Sensors
### MCU
### Other Parts
### 3D Printed Case

