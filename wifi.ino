#include <esp_task_wdt.h>

boolean scanAndConnectToLocalSCADS() {
  boolean foundLocalSCADS = false;

  // WiFi.scanNetworks will return the number of networks found
  Serial.println("scan start");
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
      delay(10);
      String networkSSID = WiFi.SSID(i);
      if (networkSSID.length() <= SSID_MAX_LENGTH) {
        scads_ssid = WiFi.SSID(i);
        if (scads_ssid.indexOf("Yo-Yo-") > -1) {
          Serial.println("Found YOYO");
          foundLocalSCADS = true;
          wifiMulti.addAP(scads_ssid.c_str(), scads_pass.c_str());
          while ((wifiMulti.run() != WL_CONNECTED)) {
            delay(500);
            Serial.print(".");
          }
          Serial.println("");
          Serial.println("WiFi connected");
          Serial.println("IP address: ");
          Serial.println(WiFi.localIP());
        }
      } else {
        // SSID too long
        Serial.println("SSID too long for use with current ESP-IDF");
      }
    }
  }
  return (foundLocalSCADS);
}

void createSCADSAP() {
  //Creates Access Point for other device to connect to
  scads_ssid = "Yo-Yo-" + generateID();
  Serial.print("Wifi name:");
  Serial.println(scads_ssid);

  WiFi.mode(WIFI_AP);
  delay(2000);

  WiFi.persistent(false);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(scads_ssid.c_str(), scads_pass.c_str());
  IPAddress myIP = WiFi.softAPIP();
  Serial.println(myIP);
}

void connectToWifi(String credentials) {
  WiFi.persistent(false);
  String _wifiCredentials = credentials;
  const size_t capacity = 2 * JSON_ARRAY_SIZE(6) + JSON_OBJECT_SIZE(2) + 150;
  DynamicJsonDocument doc(capacity);
  deserializeJson(doc, _wifiCredentials);
  JsonArray ssid = doc["ssid"];
  JsonArray pass = doc["password"];
  if (ssid.size() > 0) {
    for (int i = 0; i < ssid.size(); i++) {
      wifiMulti.addAP(checkSsidForSpelling(ssid[i]).c_str(), pass[i]);
    }
  } else {
    Serial.println("issue with wifi credentials, creating access point");
  }

  Serial.println("Connecting to Router");

  long wifiMillis = millis();
  long captivePortalMillis = millis();
  char remainingTime[40];
  bool connectSuccess = false;

  while (!connectSuccess) {

    // reset watchdog timer to tell it that we 
    // have not hung while searching for wifi
    esp_task_wdt_reset();
    uint8_t currentStatus = wifiMulti.run();

    //#ifdef DEV
    Serial.print("Status: ");
    switch (currentStatus) {
      case WL_CONNECTED:
        Serial.println("WL_CONNECTED");
        break;
      case WL_IDLE_STATUS:
        Serial.println("WL_IDLE_STATUS");
        break;
      case WL_NO_SSID_AVAIL:
        Serial.println("WL_NO_SSID_AVAIL");
        break;
      case WL_SCAN_COMPLETED:
        Serial.println("WL_SCAN_COMPLETED");
        break;
      case WL_CONNECT_FAILED:
        Serial.println("WL_CONNECT_FAILED");
        break;
      case WL_CONNECTION_LOST:
        Serial.println("WL_CONNECTION_LOST");
        break;
      case WL_DISCONNECTED:
        Serial.println("WL_DISCONNECTED");
        break;
    }
    //#endif

    if (currentStatus == WL_CONNECTED) {
      // Connected!
      connectSuccess = true;
      Serial.println("Setting last connected");
      Serial.println(WiFi.SSID());
      setLastConnected(WiFi.SSID());
      break;
    }

    if (millis() - wifiMillis > WIFICONNECTTIMEOUT) {
      //Wifi credentials are potentially incorrect as not connected to a network before.
      if (getLastConnected() == "") {
        // Timeout, check if we're out of range.
        // Wipe credentials and reset
        Serial.println("Wifi connect failed, Please try your details again in the captive portal");
        preferences.begin("scads", false);
        preferences.putString("wifi", "");
        preferences.end();
        ESP.restart();
      } else {
        //Last connected network not in the wifiScan, so most likely in a new location
        if (currentStatus == WL_DISCONNECTED) {
          Serial.println("Wifi Timeout Reached, Scanning for available SCADS and falling back to captive portal");
          boolean foundLocalSCADS = scanAndConnectToLocalSCADS();
          // set captive portal timeout counter
          captivePortalMillis = millis(); 
          if (!foundLocalSCADS) {
            //become server
            Serial.println("Becoming server...");
            currentSetupStatus = setup_server;
            createSCADSAP();
            setupCaptivePortal();
            setupLocalServer();
          }
          else {
            //become client
            Serial.println("Becoming client...");
            currentSetupStatus = setup_client;
            setupSocketClientEvents();
          }
        } else {
          // if we were disconnected long enough to trigger the captive portal, pause here to give the user time to change wifi credentials 
          while(millis() - captivePortalMillis < WIFICONNECTTIMEOUT * 8){
            sprintf(remainingTime,"Seconds: %lu", ((WIFICONNECTTIMEOUT * 8) - (millis() - captivePortalMillis)) / 1000);
            Serial.println("Pausing to allow access to captive portal, will reboot after:");
            Serial.println(remainingTime);
            // force reset from captive portal
            if(isResetting == true) {
              Serial.println("Reset message detected from captive portal, rebooting now to apply new credentials in 5 seconds...");
              delay(5000);
              ESP.restart();
            }
            delay(3000);
          } 
          Serial.println("Timeout reached, rebooting to try connection again, you may need to move closer to your router");
          ESP.restart();
        }
      }
    }

    delay(100);
    yield();
    Serial.print(".");
    buttonBuiltIn.check();
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  disconnected = false;
}

String checkSsidForSpelling(String incomingSSID) {
  int n = WiFi.scanNetworks();
  int currMatch = 255;
  int prevMatch = currMatch;
  int matchID;
  Serial.println("scan done");
  if (n == 0) {
    Serial.println("no networks found");
    Serial.println("can't find any wifi in the area");
    return incomingSSID;
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      Serial.println(WiFi.SSID(i));
      String networkSSID = WiFi.SSID(i);
      if (networkSSID.length() <= SSID_MAX_LENGTH) {
        currMatch = levenshteinIgnoreCase(incomingSSID.c_str(), WiFi.SSID(i).c_str()) < 2;
        if (levenshteinIgnoreCase(incomingSSID.c_str(), WiFi.SSID(i).c_str()) < 2) {
          if (currMatch < prevMatch) {
            prevMatch = currMatch;
            matchID = i;
          }
        }
      } else {
        // SSID too long
        Serial.println("SSID too long for use with current ESP-IDF");
      }
    }
    if (prevMatch != 255) {
      Serial.println("Found a match!");
      return WiFi.SSID(matchID);
    } else {
      Serial.println("can't find any wifi that are close enough matches in the area");
      return incomingSSID;
    }
  }
}

void wifiCheck() {
  if (millis() - wificheckMillis > wifiCheckTime) {
    wificheckMillis = millis();
    uint8_t currentStatusCheck = wifiMulti.run();
    if (currentStatusCheck !=  WL_CONNECTED) {
      digitalWrite(LED_BUILTIN, 1);
      disconnected = true;
    } else {
      disconnected = false;
    }
  }
}

bool isWifiValid(String incomingSSID) {
  int n = WiFi.scanNetworks();
  int currMatch = 255;
  int prevMatch = currMatch;
  int matchID;
  Serial.println("scan done");
  if (n == 0) {
    Serial.println("no networks found");
    Serial.println("can't find any wifi in the area");
    return false;
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      Serial.println(WiFi.SSID(i));
      String networkSSID = WiFi.SSID(i);
      if (networkSSID.length() <= SSID_MAX_LENGTH) {
        currMatch = levenshteinIgnoreCase(incomingSSID.c_str(), WiFi.SSID(i).c_str()) < 2;
        if (levenshteinIgnoreCase(incomingSSID.c_str(), WiFi.SSID(i).c_str()) < 2) {
          if (currMatch < prevMatch) {
            prevMatch = currMatch;
            matchID = i;
          }
        }
      } else {
        // SSID too long
        Serial.println("SSID too long for use with current ESP-IDF");
      }
    }
    if (prevMatch != 255) {
      Serial.println("Found a match!");
      return true;
    } else {
      Serial.println("can't find any wifi that are close enough matches in the area");
      return false;
    }
  }
}


bool lastConnectedInNetworkList() {
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("no networks found");
    Serial.println("can't find any wifi in the area");
    return false;
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      Serial.println(WiFi.SSID(i));
      if (getLastConnected() == WiFi.SSID(i)) {
        return true;
      }
    }
    return false;
  }
}
