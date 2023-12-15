//#define DEV
//#define STAGING

#define FADE_3 22
#define FADE_1 21

#define EXTERNAL_BUTTON 23
#define CAPTOUCH T0

#define LED_BUILTIN 2
#define LED_BUILTIN_ON HIGH

int BUTTON_BUILTIN =  0;

bool disconnected = false;

unsigned long wificheckMillis;
unsigned long wifiCheckTime = 5000;


enum PAIRED_STATUS {
  remoteSetup,
  localSetup,
  pairedSetup
};
int currentPairedStatus = remoteSetup;

enum SETUP_STATUS {
  setup_pending,
  setup_client,
  setup_server,
  setup_finished
};
int currentSetupStatus = setup_pending;

#define PROJECT_SLUG "ESP32-SOCKETIO"
#define VERSION "v0.2"
#define ESP32set
#define WIFICONNECTTIMEOUT 240000
#define SSID_MAX_LENGTH 31

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

//socketIO
#include <SocketIoClient.h>
/* HAS TO BE INITIALISED BEFORE WEBSOCKETSCLIENT LIB */
SocketIoClient socketIO;

//Local Websockets
#include <WebSocketsClient.h>

#include <Preferences.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiAP.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

#include <AceButton.h>
using namespace ace_button;

//rgb led variables
#include <FastLED.h>
#define WS2812PIN        5
#define NUMPIXELS 2
#define PIXELUPDATETIME 30
#define PIXELUPDATETIMELONG 5000
#define USERLED 0
#define REMOTELED 1
#define RGBLEDPWMSTART 120
#define FASTLONGFADE 120
unsigned long LONGFADEMINUTESMAX = 360;
#define LONGFADECHECKMILLIS 120000
unsigned long  prevLongFadeVal[NUMPIXELS] = {0, 0};
uint8_t hue[NUMPIXELS];
uint8_t saturation[NUMPIXELS];
uint8_t value[NUMPIXELS];
bool ledChanged[NUMPIXELS] = {false, false};
unsigned long prevPixelMillis;
bool isLongFade[NUMPIXELS] = {false, false};
unsigned long prevlongPixelMillis;
unsigned long longFadeMinutes[NUMPIXELS];
unsigned long prevLongFadeMillis[NUMPIXELS];
bool isRemoteLedFading = false;
CRGB leds[NUMPIXELS];
bool readyToFadeRGB[NUMPIXELS] = {false, false};
bool isFadingRGB[NUMPIXELS] = {false, false};
unsigned long fadeTimeRGB[NUMPIXELS];
#define RGBFADEMILLIS 6

#include "SPIFFS.h"

#include <esp_task_wdt.h>
// 300 seconds WDT, longer than wifi timeout
#define WDT_TIMEOUT 300


//Access Point credentials
String scads_ssid = "";
String scads_pass = "blinkblink";

const byte DNS_PORT = 53;
DNSServer dnsServer;
IPAddress apIP(192, 168, 4, 1);

Preferences preferences;

//local server
AsyncWebServer server(80);
AsyncWebSocket socket_server("/ws");

//local websockets client
WebSocketsClient socket_client;
byte webSocketClientID;

WiFiMulti wifiMulti;

String wifiCredentials = "";
String macCredentials = "";

/// Led Settings ///
bool isBlinking = false;
bool readyToBlink = false;
unsigned long blinkTime;
int blinkDuration = 200;

// Touch settings and config
class CapacitiveConfig: public ButtonConfig {
  public:
    uint8_t _pin;
    uint16_t _threshold;
    CapacitiveConfig(uint8_t pin, uint16_t threshold) {
      _pin = pin;
      _threshold = threshold;
    }
    void setThreshold(uint16_t CapThreshold) {
      _threshold = CapThreshold;
    }
  protected:
    int readButton(uint8_t /*pin*/) override {
      uint16_t val = touchRead(_pin);
      return (val < _threshold) ? LOW : HIGH;
    }

};

int TOUCH_THRESHOLD = 60;
int TOUCH_HYSTERESIS = 20;
#define LONG_TOUCH 1500
#define LONG_PRESS 10000
CapacitiveConfig touchConfig(CAPTOUCH, TOUCH_THRESHOLD);
AceButton buttonTouch(&touchConfig);
bool isSelectingColour = false;

//Button Settings
AceButton buttonBuiltIn(BUTTON_BUILTIN);
AceButton buttonExternal(EXTERNAL_BUTTON);

void handleButtonEvent(AceButton*, uint8_t, uint8_t);
int longButtonPressDelay = 5000;

//reset timers
bool isResetting = false;
unsigned long resetTime;
int resetDurationMs = 4000;

String myID = "";

/// Socket.IO Settings ///
#ifndef STAGING
char host[] = "irs-socket-server.herokuapp.com"; // Socket.IO Server Address
#else
char host[] = "irs-socket-server-staging.herokuapp.com"; // Socket.IO Staging Server Address
#endif
int port = 80; // Socket.IO Port Address
char path[] = "/socket.io/?transport=websocket"; // Socket.IO Base Path

void setup() {
  setupPixels();
  Serial.begin(9600);
  setupPins();
  LONGFADEMINUTESMAX = checkFadingLength();
  setupCapacitiveTouch();

  // setup WDT
  Serial.println("Configuring WDT...");
  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch

  //create 10 digit ID
  myID = generateID();
  Serial.print("My Yo-Yo ID is:");
  Serial.println(myID);

  SPIFFS.begin();

  preferences.begin("scads", false);
  // RESET WIFI for testing
  preferences.putString("wifi", "");
  wifiCredentials = preferences.getString("wifi", "");
  macCredentials = preferences.getString("mac", "");
  preferences.end();

  setPairedStatus();

  if (wifiCredentials == "" || getNumberOfMacAddresses() < 2) {
    Serial.println("Scanning for available SCADS");
    boolean foundLocalSCADS = scanAndConnectToLocalSCADS();
    if (!foundLocalSCADS) {
      //become server
      currentSetupStatus = setup_server;
      createSCADSAP();
      setupCaptivePortal();
      setupLocalServer();
    }
    else {
      //become client
      currentSetupStatus = setup_client;
      setupSocketClientEvents();
    }
  }
  else {
    Serial.print("List of Mac addresses:");
    Serial.println(macCredentials);
    //connect to router to talk to server
    digitalWrite(LED_BUILTIN, 0);
    Serial.print("Last connected WiFi SSID: ");
    Serial.println(getLastConnected());
    connectToWifi(wifiCredentials);
    //checkForUpdate();
    setupSocketIOEvents();
    currentSetupStatus = setup_finished;
    Serial.println("setup complete");
  }
}

void setPairedStatus() {
  int numberOfMacAddresses = getNumberOfMacAddresses();
  if (numberOfMacAddresses == 0) {
    Serial.println("setting up JSON database for mac addresses");
    preferences.clear();
    addToMacAddressJSON(myID);
  }
  else if (numberOfMacAddresses < 2) {
    //check it has a paired mac address
    Serial.println("Already have local mac address in preferences, but nothing else");
  }
  else {
    currentPairedStatus = pairedSetup;
    Serial.println("Already has one or more paired mac address");
  }
}

String getCurrentPairedStatusAsString() {
  String currentPairedStatusAsString = "";

  switch (currentPairedStatus) {
    case remoteSetup:      currentPairedStatusAsString = "remoteSetup"; break;
    case localSetup:       currentPairedStatusAsString = "localSetup";  break;
    case pairedSetup:        currentPairedStatusAsString = "pairedSetup";   break;
  }

  return (currentPairedStatusAsString);
}

void loop() {
  // reset watchdog within timeout to prevent hardware reset
  esp_task_wdt_reset();
  switch (currentSetupStatus) {
    case setup_pending:
      break;
    case setup_client:
      socket_client.loop();
      break;
    case setup_server:
      socket_server.cleanupClients();
      dnsServer.processNextRequest();
      break;
    case setup_finished:
      if (!disconnected) {
        socketIO.loop();
        rgbLedHandler();
      }
      ledHandler();
      wifiCheck();
      break;
  }

  buttonBuiltIn.check();
  if (!disconnected) {
    buttonExternal.check();
    buttonTouch.check();
  }
  checkReset();
}
