class room {
private:
    String groupIdentifier;
    AsyncUDP udpLampState; 
    String PUT_pilot, GET_pilot;
    int lampTemp = 2700;
    int lampDim = 100;
    bool success = false;
    bool getPilotDone = false;

public:
    room(String id) : groupIdentifier(id) {} // for future dev allowing several rooms or light groups

    void setLampState(lampActions lampAction) {
      debug_print("---Lamp Action: ");
      debug_println(lampAction);
      bool noAction = false; //when trying to dim/temp beyond the max/min, do nothing

      StaticJsonDocument<128> docOUT;
      docOUT["method"] = "setPilot";
      int timeSlot = 0; //not allowed to be defined inside case statement
      int localHr = 0;  //not allowed to be defined inside case statement
      JsonObject params = docOUT.createNestedObject("params");
      switch (lampAction) {
        case LAMP_OFF:  lampState = LAMP_OFF; 
                        PIR.resetValues();
                        PSE.resetValues(); 
                        params["state"] = 0; 
                        break;
        case LAMP_ON:   lampState = LAMP_ON;  
                        PIR.resetValues();
                        PSE.resetValues();  
                        params["state"] = 1; 
                        break;
        case BRIGHTER:  if (lampDim < 100) //dim steps are: 20,60,100
                          lampDim += 40; 
                        else {
                          fastBlink(L_YELLOW, 2); 
                          noAction = true;
                        }
                        params["dimming"] = lampDim; 
                        break;
        case DIMMER:    if (lampDim > 20)  //dim steps are: 20,60,100
                          lampDim -= 40;
                        else {
                          fastBlink(L_YELLOW, 2); 
                          noAction = true;
                        }
                        params["dimming"] = lampDim; 
                        break;
        case WARMER:    if (lampTemp > 2700) //temp steps are: 2700,4600,6500
                          lampTemp -= 1900; 
                        else {
                          fastBlink(L_YELLOW, 2); 
                          noAction = true;
                        }
                        params["temp"] = lampTemp;
                        break;
        case COOLER:    if (lampTemp < 6500) //temp steps are: 2700,4600,6500
                          lampTemp += 1900; 
                        else {
                          fastBlink(L_YELLOW, 2); 
                          noAction = true;
                        }
                        params["temp"] = lampTemp;
                        break;
        case SET_COLOR: params["r"] = lightColor[currLightColor].red;
                        params["g"] = lightColor[currLightColor].green;
                        params["b"] = lightColor[currLightColor].blue;
                        break;
        case SET_SCENE: localHr = getHour();
                        timeSlot = (localHr >= TOD_1 && localHr < TOD_2) ? 0 : (localHr >= TOD_2 && localHr < TOD_3) ? 1 : 2;
                        params["sceneId"] = enrolledFaces[knownFaceID - 1].scene[timeSlot];
                        knownFaceActioned = true; 
                        break;
        case SET_TEMP:  timeSlot = (lastTempTOD <= 6 ) ? 0 : (lastTempTOD >= 20) ? 14 : lastTempTOD - 6;
                        params["temp"] = temperatures[temperTOD[timeSlot]];
                        break;
        default: debug_write("ERROR: Invalid LampState in JSONBuild"); return;
      }
      if (noAction) return; //reached max/min of dim/temp

      PUT_pilot = ""; 
      serializeJson(docOUT, PUT_pilot);
      //>>> lambda function = a way to define an anonymous function to be used to create function objects. 
      //>>> Useful for tasks that need a function as a parameter, such as callbacks or single-use tasks.
      //>>> [] before the lambda function = capture clause: external variables accessible and how to the function
      //>>> either by value (creating a copy) or by reference (pointing to the original variable).
      //>>> ORG reference w/o "this->"    [&success](AsyncUDPPacket packet) {
      //>>> [this]: Captures the "this" pointer of the current object, allowing access its members inside the lambda
      udpLampState.onPacket(
        [this](AsyncUDPPacket packet) {
          StaticJsonDocument<256> docIN;
          DeserializationError error = deserializeJson(docIN, packet.data());
          if (error) {
            debug_write("ERROR: Parsing failed: ");
            debug_println(error.c_str());
            return;
          }
          //see RC0.2 and lower for example on setPilot & getPilot on the same udp.onPacket()
          if (docIN["result"]["success"]) this->success = true;
        }
      );

      for (int i = 0; i <= totalSelectedLights; i++) {
        IPAddress TEMPipAddress; //create an IPAddress object
        TEMPipAddress.fromString(selectedLights[i]);
        if(!udpLampState.connect(TEMPipAddress, WiZ_PORT)) {
          debug_print("ERROR: Failed to connect to UDP from IP:");
          debug_print(TEMPipAddress);
          debug_print("with ID:");
          debug_println(i);
          return;
        } 
        success = false;
        unsigned long startTime; 
        for (int j = 0; j < 7; j++) { // up to 6 attempts to get "success" from lamp
          udpLampState.print(PUT_pilot);
          String ipStr = TEMPipAddress.toString();
          //>>>the printf function expects a C-style string as an argument for the %s format specifier, 
          //>>>so we use c_str() to get the C-style string representation of the std::string object
          debug_printf("Waiting on response from IP: %s ID: %d\n", ipStr.c_str(), i);
          debug_println(PUT_pilot);
          startTime = millis();
          while (!success && millis() - startTime < 3000) delay(10);  //wait up to 3 seconds for UDP response
          if (success) {
            debug_write("Success");
            fastBlink(L_GREEN);
            break;
          }else {
            fastBlink(L_YELLOW);
            debug_print("Cycle #");
            debug_println(j);
            if (j == 6) blockBlink(L_ORANGE,0,5);
          }
        }
      }
      udpLampState.close(); //close the udp.onPacket callback after all lamps are actioned
    }

}; //end of class

