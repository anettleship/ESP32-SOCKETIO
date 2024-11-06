![Build](https://img.shields.io/github/workflow/status/interactionresearchstudio/ESP32-SOCKETIO/build)
![Downloads](https://img.shields.io/github/downloads/interactionresearchstudio/ESP32-SOCKETIO/total)
![Release](https://img.shields.io/github/v/release/interactionresearchstudio/ESP32-SOCKETIO?include_prereleases)

# Yo-Yo Machines Firmware
Firmware for Yo-Yo Machines.

## How to use
This is a fork of the main project, attempting to fix a bug where the light touch machine removes its own wifi credentials and resets, in environments where wifi signal is poor.
In order to make these changes it was necessary to build the project from source using modern dependencies, which was less than easy several years after the project was last developed.
Pending addition to this readme, the procedure to build from source is described here: https://github.com/interactionresearchstudio/ESP32-SOCKETIO/issues/63
It requires Arduino IDE v1.x and a specfic version of esp32 core to build successfully. A compiled bin of the current version is included for those who can work out what to do with it.
It's probably easier than building your own from source!

## Issues
Please report issues on here with as much information as possible, including relevant
screenshots and system information.

## Libraries
Yo-Yo Machines devices use third party libraries. Thank you to everyone who has developed and maintained these libraries. Here's a list of the libraries used
* [AsyncTCP](https://github.com/me-no-dev/AsyncTCP)
* [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
* [SocketIOClient](https://github.com/timum-viw/socket.io-client)
* [ArduinoWebSockets](https://github.com/Links2004/arduinoWebSockets)
* [ArduinoJSON](https://github.com/bblanchon/ArduinoJson)
* [AceButton](https://github.com/bxparks/AceButton)
* [FastLed](https://github.com/FastLED/FastLED) (Light Touch)
* [ESP32Servo](https://madhephaestus.github.io/ESP32Servo/annotated.html) (Speed Dial)
