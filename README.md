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
   	* [Webserver and Webclient](#webserver--webclient)

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
Face recognition is executed when there are lights enrolled, they are ON and there is no pending OFF command on them. There´s also a flag so that the scene change is executed only once after a knwon face has been found. This flag is reset with any ON/OFF action. 

After this, the setLampState method of the _room_ class instance is invoked with the SET_SCENE parameter. The class is defined in _lightsControl.h_. The method then uses the SET_SCENE parameter to translate the current time using the current millis() against the latest reference millis() (see [Time of Day](#time-of-day)), selects the correspnding scene for that time of day and that face Id, and goes on to issue the UDP packet.
#### Face Recognition
The actual act of face recognition is done during the Person Sensor reads in _presenceDetection.h_. After there is 70% confidence that there is at least one face in the sensor's view, the closest face to the camera is compared against the saved (i.e. enrolled) faces and the first face with a confidence of at least 95% is selected as the recognized face. 
## Auto Temperature via Circadian Rhythm
Light temperature based using circadian rhythm principles is set when there is no face recognized or enrolled. This means that face recongotion has precedence over circadian rhythm. When this condition occurs, the current time of day is compared against the last time that circadian rhythm temperature was set to determine is a new temperature should be set.
#### Time of Day
A reference time is acquired in _setup()_ (i.e. initial boot and any reboot) and after any saved data from the web interface (i.e. enrolled lights, circadian rhythm temperatures and enrolled faces & scenes). This is accomplished by sending a UDP packet to the first enrolled light in the list and to a couple of NTP servers in _getReferenceHour()_ as defined in _declarations.h_. A loop waits for 5 seconds for any response from any of these two sources. The NTP server time, if received, has preference over the time from the enrolled light.  This reference UTC (in UNIX epoch format) and reference millis() are used to calculate current time at any moment using current millis().
#### Temperature Table
The default temperature values from 6AM thru 8PM can include five values (in Kelvin: 2700, 3400, 4000, 5200, 6500) and are defined as: (picture)

The user can modify these (see [Circadian Rhythm Temperatures](#circadian-rhythm-temperatures))
## Light Adjustments via Hand Gestures

#### Gesture Recognition
## System Configuration
#### Enroll Lights
#### Enroll Faces
#### Circadian Rhythm Temperatures
#### Faces-Scenes Table
#### Sensitivity Parameters
#### Webserver & Webclient
The webpage is designed to discover and manage smart lights within a network. Upon loading, the page fetches information on available lights, displaying details like IP address, MAC address, signal strength, and state. Users can select which lights to enroll. The page also presents a table for adjusting light temperatures at different times of the day. Additionally, there's a table for managing Face IDs, where each face can be associated with up to three lighting scenes. The user can save changes with a "Confirm" button. Inactivity for 5 minutes triggers a notification prompting the user to reload for updated data.
1. **Listing and Selecting Lights:**
The JavaScript sends an asynchronous request to the server, fetching a JSON of available lights. Upon reception, the script populates the HTML table with IP, MAC, signal, and state data. Each row is equipped with a checkbox. Users check desired lights, and upon form submission, JavaScript captures these selections to enroll the chosen lights.

2. **Temperature Adjustment:**
The page loads with a table showing different times and corresponding light temperatures. The table cells are input fields pre-populated with defaults. Users can modify temperatures for any specified time. The script consolidates changes upon form submission.

3. **Face ID Association:**
An HTML dropdown allows users to pick one of three available lighting scenes. They upload a face image using the file input. The JavaScript reads the uploaded image and the selected scene. Users can repeat this three times, associating each face with a unique scene. Data is saved upon "Confirm".

_____________________________________________________________________________________________________

#### Sensors
#### MCU
#### Other Parts
#### 3D Printed Case

