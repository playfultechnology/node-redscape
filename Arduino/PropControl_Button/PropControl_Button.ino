/**
 * Node-Redscape Prop Control - Button
 * 
 * Example code for a simple button escape room puzzle integrated with Node-RED GM control.
 *
 * This code is designed to be run on an Arduino-type device (Uno/Nano/ESP32/ESP8266 etc.) 
 * connected, via a serial, wired ethernet, or Wi-Fi to a Node-RED server, using JSON-formatted 
 * messages delivered over Serial/MQTT/TCP/UDP. 
 * 
 * A gamesmaster can use the Node-RED dashboard to monitor all inputs the players make, 
 * reset, and override the puzzle state remotely.
 * 
 * Tested on:
 * Arduino Nano / Arduino UNO / Arduino Mega / Wemos D1 Mini / Devkit ESP32 v1 (via USB Serial)
 * Arduino UNO (via Ethernet Shield)
 * Wemos D1 Mini / Devkit ESP32 v1 (via Wi-Fi)
 */

// INCLUDES
#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>
#include <ArduinoJson.h>
#include "PropControl.h"
#include <Button2.h>

// CONSTANTS
// Define the characters in the matrix
const byte buttonPin = D3;

// GLOBALS
Button2 button;
LiquidCrystal_PCF8574 lcd(0x27);  // set the LCD address to 0x27 for a 16 chars and 2 line display
bool ledState = 0;

// Track state of overall puzzle
enum State {Initialising, Running, Solved};
State state = Initialising;

void setup(){
  // Initialise serial connection
  Serial.begin(115200);
  Serial.println("");
	Serial.println(__FILE__ __DATE__);

  lcd.begin(16, 2);
  lcd.setBacklight(128);
  lcd.print("Hello LCD");

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, ledState);  

  button.begin(buttonPin);
  button.setClickHandler(click);
  button.setLongClickDetectedHandler(longClickDetected);
  button.setDoubleClickHandler(doubleClick);

  // Set the puzzle state
  state = State::Running;

  pc_setup();
}

void click(Button2& btn) {
    Serial.println("click\n");
    // Create JSON document - determine size using https://arduinojson.org/v6/assistant/
    StaticJsonDocument<48> jsonDoc;
    jsonDoc["id"] = "Button";
    jsonDoc["state"] = (state == State::Solved) ? "SOLVED" : "UNSOLVED";
    jsonDoc["input"] = "Click";
    pc_sendUpdate(jsonDoc);
}
void longClickDetected(Button2& btn) {
    Serial.println("long click detected");
    // Create JSON document - determine size using https://arduinojson.org/v6/assistant/
    StaticJsonDocument<48> jsonDoc;
    jsonDoc["id"] = "Button";
    jsonDoc["state"] = (state == State::Solved) ? "SOLVED" : "UNSOLVED";
    jsonDoc["input"] = "LongClick";
    pc_sendUpdate(jsonDoc);    
}
void doubleClick(Button2& btn) {
    Serial.println("double click\n");
    // Create JSON document - determine size using https://arduinojson.org/v6/assistant/
    StaticJsonDocument<48> jsonDoc;
    jsonDoc["id"] = "Button";
    jsonDoc["state"] = (state == State::Solved) ? "SOLVED" : "UNSOLVED";
    jsonDoc["input"] = "DoubleClick";
    pc_sendUpdate(jsonDoc);
}

void loop(){
  button.loop();

  // Important - call this on every iteration of loop
  pc_loop();
}
