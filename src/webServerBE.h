// look for currently discoved device in existing device array
int findDevice(const IPAddress& ipAddress) { 
  for (int i = 0; i < deviceCount; i++) 
    if (devices[i].ipAddress == ipAddress) 
      return i; 
  return -1; // Device not found
}

// convert IP to long for sorting
unsigned long ipToLong(String ip) { 
    int idx = 0;
    unsigned long ipAsLong = 0;
    for (int i = 0; i < 4; i++) {
        int end = ip.indexOf('.', idx);
        if (end == -1) end = ip.length();
        ipAsLong = ipAsLong << 8;
        ipAsLong += ip.substring(idx, end).toInt();
        idx = end + 1;
    }
    return ipAsLong;
}

void discoverDevices() {
  unsigned long discoveryDuration = 9000;
  AsyncUDP udp;
  unsigned int UDPlocalPort = 8888;
  if (!udp.listen(UDPlocalPort)) { //possible reasons: udp object already listening on another port; port already in use
    debug_write("ERROR: listen UDP for Discovery");
    return;
  }
  debug_print("\nUDP Listening on IP: ");
  debug_print(WiFi.localIP());
  // init the key in the devices array
  for (int i=0; i<maxDevices; i++)
    devices[i].ipAddress = IPAddress(0,0,0,0);
  deviceCount = 0;

  // callback for UDP Broadcast
  udp.onPacket([](AsyncUDPPacket packet) {
    debug_print(".");
    // Check if the first octet of the received IP matches the first octet of the local IP
    String receivedIP = packet.remoteIP().toString();
    String localIP = WiFi.localIP().toString();
    if (receivedIP.substring(0, receivedIP.indexOf(".")) == localIP.substring(0, localIP.indexOf("."))) {
      StaticJsonDocument<256> docIN;
      DeserializationError error = deserializeJson(docIN, packet.data());
      if (error) {
        debug_write("Parsing failed: ");
        debug_println(error.c_str());
        return;
      }
      // Add device that answered broadcast to the array if it is not already present. Not considering re-occurrences, 
      // so if rssi or state change during discovery duration only first response will be recorded
      if (findDevice(packet.remoteIP()) == -1) { 
        if (deviceCount < maxDevices) {
          devices[deviceCount].ipAddress = packet.remoteIP(); 
          devices[deviceCount].macAddress = (docIN["result"]["mac"].as<String>()); 
          devices[deviceCount].macAddress.toUpperCase();
          devices[deviceCount].port = packet.remotePort(); 
          devices[deviceCount].rssi = docIN["result"]["rssi"].as<int>(); 
          devices[deviceCount].state = docIN["result"]["state"].as<bool>(); 
          devices[deviceCount].enrolled = false; // default value that will be assigned down below in file read
          deviceCount++;
        }else {
          debug_write("ERROR: Device array is full. Cannot add more devices.");
        }
      }
    }
  });

  // listen for responses from WiZ_PORT until discovery duration has expired
  const char* GET_DeviceData = "{\"method\":\"getPilot\"}"; // Packet to get the state of a light
  ulong startTime = millis(); 
  while (millis() - startTime < discoveryDuration) { 
    udp.broadcastTo(GET_DeviceData, WiZ_PORT); //udp.onPacket fills the "devices" array with discoveries
    delay(1000);
  }
  udp.close();

  //add "enrolled" data using littleFS to devices array. LittleFS was mounted on handleRoot
  File file = LittleFS.open("/enrolledLights.txt", "r"); //  NOTE: "File" class is derived from Arduino Stream class
  if (file) {   // if file does not exist leave all "enrolled" fields as false = default
    while (file.available()) {
      String ipAddressStr = file.readStringUntil('\n');
      ipAddressStr.trim();
      debug_print("\nadd enrolled to devices array - ");
      debug_println(ipAddressStr);
      IPAddress TEMPipAddress; // create an IPAddress object
      if (!TEMPipAddress.fromString(ipAddressStr)) {
        debug_write("Invalid IP Address format in reading file");
        continue;
      }
      int i = findDevice(TEMPipAddress); 
      if (i > -1) devices[i].enrolled = true; //if IPAddress was found on the network, and is enrolled in file
    }
    file.close();
    debug_println("\nDone reading Enrolled Lights file");
  }else 
    {debug_println("\nenrolledLights.txt does not exist");}
  LittleFS.end(); // was mounted with handleRoot and getFacesId uses array not enrolledFaces.txt

  // Sort devices array by IP Address
  for (int i = 0; i < deviceCount - 1; i++) {
    for (int j = i + 1; j < deviceCount; j++) {
      if (ipToLong(devices[i].ipAddress.toString()) > ipToLong(devices[j].ipAddress.toString())) {
        DeviceInfo temp = devices[i];
        devices[i] = devices[j];
        devices[j] = temp;
      }
    }
  }
  // debug output
  debug_print("\n\nDevices found:");
  debug_println(deviceCount);
  for (int i = 0; i < deviceCount; i++) {
    debug_print("\nDevice ");
    debug_print(i + 1);
    debug_println(":");
    debug_print("IP Address: ");
    debug_println(devices[i].ipAddress);
    debug_print("Port: ");
    debug_println(devices[i].port);
    debug_print("MAC Address: ");
    debug_println(devices[i].macAddress);
    debug_print("RSSI: ");
    debug_println(devices[i].rssi);
    debug_print("State: ");
    debug_println(devices[i].state);
    debug_print("Enrolled:");
    debug_println(devices[i].enrolled);
  }
}

void saveData(const String& jsonString) {
  AsyncUDP udp;
  bool statusOfLight[10] = {}; // temp array to hold On/Off status of enrolled light for visual confirmation

  // update selected lights and facesID global variable tables
  StaticJsonDocument<1536> doc;  //size can handle up to deviceCount = 6 + 3 FacesID
  DeserializationError error = deserializeJson(doc, jsonString);
  if (error) {
    debug_write("deserializeJson() failed: ");
    debug_println(error.f_str());
    return;
  }
  totalSelectedLights = -1;
  JsonArray lightsData = doc["lightsData"];
  for (JsonObject lightData : lightsData) {
    if (lightData["Selected"].as<bool>() == true) {
      totalSelectedLights++;
      selectedLights[totalSelectedLights] = lightData["IP Address"].as<String>();
      statusOfLight[totalSelectedLights] = lightData["state"].as<bool>();
    }
  }
  if (next_unused_id > 1) { //only save if there are enrolled faces
    int faceIndex = 0;
    JsonArray faceData = doc["faceData"];
    for (JsonObject faceObj : faceData) {
      FacesInfo faceInfo;
      faceInfo.ID = faceObj["ID"].as<int>();
      faceInfo.label = faceObj["Label"].as<String>();
      JsonArray scenesArray = faceObj["Scenes"];
      for (int i = 0; i < 3; i++) {
        faceInfo.scene[i] = scenesArray[i].as<int>();
      }
      enrolledFaces[faceIndex++] = faceInfo;
    }
    knownFaceActioned = false; // resetting this flag to set scene accroding to latest selection
    knownFace = false;
  }

  JsonArray temperatureData = doc["temperatureData"];
  for (int i=0; i< temperatureData.size(); i++) {
    temperTOD[i] = temperatureData[i].as<int>();
  } 

  // callback for responses after PUT for On-Off visual confirmation
  bool success = false;     //-----------------------------------------------------------------------add BOOL to cut short delay for ON/OFF
  StaticJsonDocument<128> PUT_response;
  udp.onPacket([&PUT_response, &success](AsyncUDPPacket packet){ 
    PUT_response.clear();
    DeserializationError error = deserializeJson(PUT_response, packet.data());
    if (error) {
      debug_write("ERROR: Parsing failed: ");
      debug_println(error.c_str());
      return;
    }
    // these two lines only for debugging since it seems "success" means only message received not command executed
    if (PUT_response["result"]["success"])  {debug_println("Success in Toggle"); success = true;}
    else                                    {debug_println("Failure in Toggle"); success = false;}
  }); 

  // Iterate over each selected light and toggle ON /OFF for visual confirmation
  for (int i=0; i<=totalSelectedLights; i++) {  
    IPAddress tempIpAddress;
    //debug_println(selectedLights[i]);
    if (!tempIpAddress.fromString(selectedLights[i])) {
      debug_write("ERROR: Invalid IP Address format");
      continue;
    }
    StaticJsonDocument<128> jsonForPilot;  // to send On-Off commands
    jsonForPilot["method"] = "setPilot";
    JsonObject params = jsonForPilot.createNestedObject("params");
    bool status = statusOfLight[i];
    for (int j=1; j<=2; j++) {
      params["state"] = (status ? 0 : 1); // is status=true, put 0 to turn lamp off else put 1
      status = !status;
      String stringForPilot = "";
      serializeJson(jsonForPilot, stringForPilot);
      debug_println(stringForPilot);
      debug_println(tempIpAddress);
      unsigned long startTimer = millis();
      if(udp.connect(tempIpAddress, WiZ_PORT)) {
        debug_println("UDP connected");
        success = false;
        udp.print(stringForPilot);
      }
      //if (j == 1) // don't delay On-Off on last iteration
        while ((millis() - startTimer < 2000)) delay(10); // wait up to 2 seconds for success
    }
  }
  udp.close();

  // write selected lights or zap file if selected = 0  
  if (!mountedLittleFS()) return;
  // Open file for W,R. If exists, truncate it to zero length. If not exist, create new file.
  File file = LittleFS.open("/enrolledLights.txt", "w+"); 
  if (!file) {
    debug_write("enrolledLights non existant. Newly created");
  }
  for (int i=0; i<=totalSelectedLights; i++) {
    file.println(selectedLights[i]);
    debug_println(selectedLights[i]);
  }
  file.close();
  debug_println("Done writing Enrolled Lights file");

  // write changes to enrolled Faces
  if (next_unused_id > 1) // otherwise there are no faces so nothing to save
    saveFaceIDs(); 

  //write changes to TOD temperatures
  file = LittleFS.open("/TODtemps.txt", "w+"); 
  if (!file) {
    debug_write("TODtemps non existant. Newly created");
  }
  for (int i=0; i<15; i++) {
    file.println(temperTOD[i]);
    debug_print(temperTOD[i]);
  }
  file.close();
  debug_println(" Done writing TOD temps");
  LittleFS.end();

  getReferenceHour();

  //update temp in case it changed in webpage
  int localHr = getHour();
  lastTempTOD = localHr;
  debug_printf("SET_TEMP Time in saveData(): %d\n", lastTempTOD);
  roomOne.setLampState(SET_TEMP);

} //end of saveData()

