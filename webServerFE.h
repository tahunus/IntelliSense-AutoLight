void handleRoot() {
  if (!mountedLittleFS()) // LittleFS will be unmounted at the end of discoverDevices()
    return;
  File file = LittleFS.open("/index.html", "r");
  if (!file) {
    wizWebServer.send(404, "text/plain", "File not found");
    return;
  }
  wizWebServer.streamFile(file, "text/html");
  file.close();
}

void handleCSS() {
  File file = LittleFS.open("/styles.css", "r");
  if (!file) {
    wizWebServer.send(404, "text/plain", "File not found");
    return;
  }
  wizWebServer.streamFile(file, "text/css");
  file.close();
}

void handleJS() {
  File file = LittleFS.open("/script.js", "r");
  if (!file) {
    wizWebServer.send(404, "text/plain", "File not found");
    return;
  }
  wizWebServer.streamFile(file, "application/javascript");
  file.close();
}

void handleDiscoverLights() {
  debug_println("Discover");
  discoverDevices();
  size_t safeSize = JSON_OBJECT_SIZE(1)  // For the root object.
               + JSON_ARRAY_SIZE(deviceCount)    // For the devices array.
               + deviceCount * JSON_OBJECT_SIZE(5)  // For each device object.
               + deviceCount * (15 + 12 + 4 + 6 + 6)  // For each string value 
               + 200; // buffer
  DynamicJsonDocument json(safeSize);
  JsonArray devicesArray = json.createNestedArray("devices"); // "devices" is JSON ID (1st key) also in js code
  for (int i = 0; i < deviceCount; i++) {
    JsonObject deviceObject = devicesArray.createNestedObject(); 
    deviceObject["ipAddress"] = devices[i].ipAddress.toString();
    deviceObject["MACAddress"] = devices[i].macAddress;
    deviceObject["rssi"] = devices[i].rssi;
    deviceObject["state"] = devices[i].state;
    deviceObject["enrolled"] = devices[i].enrolled;
  }
  String jsonResponse;
  serializeJson(json, jsonResponse);
  debug_println(jsonResponse);
  wizWebServer.send(200, "application/json", jsonResponse);
}

void handleGetFaceIDs() {  
  debug_println("Get Face IDs");
  StaticJsonDocument<512> doc; // based on https://arduinojson.org/v6/assistant
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
  String jsonStringOut = "";
  serializeJson(doc, jsonStringOut);
  debug_println(jsonStringOut);
  wizWebServer.send(200, "application/json", jsonStringOut);
}

void handleGetTODtemps() {
  debug_println("Get TOD Temps");
  StaticJsonDocument<256> doc;
  JsonArray tempsArray = doc.createNestedArray("temperatureData");
  for (int i=0; i<15; i++) {
    tempsArray[i] = temperTOD[i];
  }
  String jsonStringOut = "";
  serializeJson(doc, jsonStringOut);
  debug_println(jsonStringOut);
  wizWebServer.send(200, "application/json", jsonStringOut);
}

void handleSaveData() {
  debug_println("Save Data");
  if (wizWebServer.args() > 0) {
    String jsonData = wizWebServer.arg("data");
    debug_println("Received JSON data:");
    debug_println(jsonData);
    saveData(jsonData);
  }
  wizWebServer.send(200, "text/plain", "Data saved succesfully");
}

