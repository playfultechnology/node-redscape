/**
 * Node-Redscape Prop Control - Keypad
 * 
 * Example code for a keypad entry escape room puzzle integrated with Node-RED GM control.
 *
 * This code is designed to be run on an Arduino-type device (Uno/Nano/ESP32/ESP8266 etc.) with 
 * a keypad in an escape room. The controller is connected, via a serial, wired ethernet, or Wi-Fi
 * connection to a Node-RED server, using JSON-formatted messages delivered over Serial/MQTT/HTTP. 
 * 
 * A gamesmaster can use the Node-RED dashboard to monitor all inputs the players make on the keypad, 
 * reset, and override the puzzle state remotely.
 * 
 * Usage:
  jsonDoc["id"] = "Keypad";
  jsonDoc["state"] = (state == State::Solved) ? "SOLVED" : "UNSOLVED" ;
  jsonDoc["input"] = inputCode;
 *
 * Tested on:
 * Arduino Nano / Arduino UNO / Arduino Mega / Wemos D1 Mini / Devkit ESP32 v1 (via USB Serial)
 * Arduino UNO (via Ethernet Shield)
 * Wemos D1 Mini / Devkit ESP32 v1 (via Wi-Fi)
 */
#pragma once

#include <ArduinoJson.h>

// Physical network layers
#define USE_ETHERNET 0x01
#define USE_WIFI 0x02
#define USE_SERIAL 0x03
#define USE_SERIAL1 0x04
#define USE_SERIAL2 0x05
#define USE_SERIAL3 0x06
#define USE_SOFTWARESERIAL 0x07
// Transport/Application layers
#define USE_TCP 0x01 // Implicit when using USE_MQTT or USE_HTTP application layers
#define USE_UDP 0x02
#define USE_SERIAL 0x03 // Implicit when using USE_SERIALxxx physical layer
#define USE_MQTT 0x04 

// Declare function prototypes 
void pc_setup();
void pc_sendUpdate(const JsonDocument& jsonDoc);
void pc_receiveUpdate(const JsonDocument& jsonDoc);
void pc_loop();
