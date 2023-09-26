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
	* [Detect Faces](#detect-faces)
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
To determine if the lights should be acted upon (ON/OFF), the accessory uses the PIR sensor to detect motion and the [Person Sensor](https://www.sparkfun.com/products/21231) to detect the presence of human faces. This means that simple motion, like someone passing by, will not trigger ON and that a human sitting perfectly still (like when reading), will not trigger OFF. Because the Person Sensor uses a camera, it is subject to variability based on ambient light and back light. By combining these two sensors, false positives and negatives are greatly reduced. 

The class _SensorReader_, defined in _presenceDetection.h_, is used to manage the samples from both sensors. Understanding it's constructor parameters will give a good idea on how this hystereis / sensor fusion process takes place.

| Variable | Description |
| -------- | ----------- |
| _sensorType_ | 0 for PIR, 1 for Person Sensor, ready for additional sensors |
| _timeBetweenSamples_ | rate to read sensor in ms |
| _pirPin_ | GPIO PIN for the input data from the PIR sensor |
| _offThreshold_ | % of time spent on LOW in rolling sum above which lamp is turned OFF |
| _offCountLimit_ | # of changes to go from (offTheshold + OffMargin) to just offThreshold |
| _lampOnTime_ | time spent on HIGH to turn lamp on (as a timeout for hysteresis) |
| _lampOffTime_ | time spent on LOW to turn lamp off (as a timeout for hysteresis) |
| _bufferSize_ | Number of samples to keep in the running sum of HIGH's and LOW's |
| _DEBUG_MODE_ | TRUE for detailed debug output to serial port (NOTE: DEBUG must de defined in RC3.ino) |
#### Detect Motion
The selected PIR sensor is based on the SR602 and has a non-programable block time of 2.5 seconds. The GPIO pin is read according to the frequencey defined in its constructor
#### Detect Faces
The Person Sensor data is accessed via I2C. The struct returned by the sensor includes the confidence that the image taken contains at least one human face and if so, how many faces are there. If the confidence that the object closest to the camera is a human face is greater than 70%, this is considered as a HIGH state. 
#### Hysteresis
A running total of a predefined number of samples of the time that the sensor stayed on HIGH or LOW through consecutive readings is kept for each sensor. This running total is updated every time the state of the sensor changes. 

Comparing the current state of the lamp(s) and the state of teh sensor, the system checks if the thresholds defined in the sensor's constructor are met to conclude on an action on the lamp. If both sensors concur on the action, then such an action is executed. 
## Auto Scene via Face Recognition
Face recognition is executed when there are lights enrolled, they are ON and there is no pending OFF command on them. There´s also a flag so that the scene change is executed only once after a knwon face has been found. This flag is reset with any ON/OFF action
#### Face Recognition
The actual act of face recognition is done during the Person Sensor reads in _presenceDetection.h_. After there is 70% confidence that there is at least one face in the sensor's view, the closest face to the camera is compared against the saved (i.e. enrolled) faces and the first face with a confidence of at least 95% is selected as the recognized face. 
## Auto Temperature via Circadian Rhythm
#### Time of Day
#### Temperature Table
## Light Adjustments via Hand Gestures
#### Gesture Recognition
## System Configuration
#### Enroll Lights
#### Enroll Faces
#### Circadian Rhythm Temperatures
#### Faces-Scenes Table
#### Sensitivity Parameters

_____________________________________________________________________________________________________

#### Sensors
#### MCU
#### Other Parts
#### 3D Printed Case

