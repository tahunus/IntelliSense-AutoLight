# AutoSense.IntelliLight
Experiments in UI/UX for a lighting experience that knows what I want and when I want it. Using ESP32-Arduino, sensors & Machine Learning

For a summary of WHY, WHAT & HOW, see description on [Hackster.io](https://www.hackster.io/pedro-martin/screenless-seamless-sensing-user-interface-921404)

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
If a gesture is detected (_gestureSensor.getDataReady()_ returns true), the program fetches the current gesture state with _gestureSensor.getGesturesState()_. The program then converts this gesture state to a gesture index by analyzing a 16-bit number. Each bit represents a potential gesture. It returns the index of the set bit or -1 if multiple gestures are detected simultaneously or none is detected

There are gestures that need confirmation and those that don’t. For gestures needing confirmation ("Up", "Down", and "Forward"), the user has to make another gesture within a set timeout (_twoStepTimeout_) to confirm their initial gesture. For instance, if the user first makes an "Up" gesture and then confirms it within the time limit, the lamp brightness will increase if it's already ON, or it will simply turn ON if it was OFF. Other gestures like "Left", "Right", "Clockwise", and "Counterclockwise" do not require confirmation and directly map to corresponding light actions like changing temperature or toggling light color.

The program also uses LED indications to show the user when they've made a gesture and when they need to confirm it (_fastBlink()_ function).

## System Configuration

#### Enroll Faces
The face enrollment class uses a state machine with 5 states to acquire face samples and train (i.e. calibrate) the model in the person sensor.

1. **INITIATE**: Triggered on the initial button press. It initializes variables, debug messages, and the LED, setting the stage for face calibration.

3. **CALIBRATE**: The system actively reads the sensor for a face, blinking a blue LED. First, it determines the presence of an unenrolled face with at least _unrecognizedFaceCount_ samples of unrecognized faces.  It then saves several samples in the Person Sensor to calibrate the new face. 

4. **SAVE_DATA**: If the calibration was successful, the face data, including ID and default scenes, is saved to the filesystem. A solid blue LED indicates a successfully enrolled face, while a yellow LED indicates enrollment failure (i.e. face already exists or not enough valid samples)

5. **ERASE_WARNING**: If the button remains pressed past a time limit after enrollment, the program enters a warning phase. The LED blinks red, signaling the impending erasure of all enrolled faces if the button isn't released.

6. **ERASE_DATA**: If the button is still pressed after the warning, this state is entered, and all enrolled faces are removed from the system. The LED then turns solid red to confirm erasure.
 
#### Enrolled Lights, Circadian Rhythm Temperatures and Enrolled Faces

The _saveData_ function stores configuration data received from the webpage interface. It parses the incomning JSON string and updates the corresponding variables & arrays. As a form of visual feedback, each selected light is toggled ON/OFF. It then saves this data in 3 files: _enrolledLights.txt_, _TODtemps.txt_ and _enrolledFAces.txt_. It then updates the reference hour described in [Time of Day](#time-of-day). Finally, it sets the lights' temperature based on the incoming data.

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

## Components


