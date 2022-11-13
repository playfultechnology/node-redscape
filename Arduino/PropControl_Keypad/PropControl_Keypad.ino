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

// INCLUDES
#include <Keypad.h>
#include <ArduinoJson.h>
#include "PropControl.h"

// CONSTANTS
// Define the characters in the matrix
const byte ROWS = 4;
const byte COLS = 3;
const char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[ROWS] = {D4, D3, D2, D1}; // Row pinouts of the keypad
byte colPins[COLS] = {D5, D6, D7}; // Column pinouts of the keypad
char solutionCode[5] = "1234";

// GLOBALS
// Create a new keypad based on the paremeters defined above
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS );
// Record the code which the user has entered
char inputCode[] = "    ";
// What position through the code is being entered?
byte sequenceNum = 0;
// Track state of overall puzzle
enum State {Initialising, Running, Solved};
State state = Initialising;

void setup(){
  // Initialise serial connection
  Serial.begin(115200);
  Serial.println("");
	Serial.println(__FILE__ __DATE__);

  // Set the puzzle state
  state = State::Running;

  pc_setup();
}

void loop(){
  char key = keypad.getKey();
  if (key){
		// Set the current position of the code sequence to the key pressed
    inputCode[sequenceNum] = key;
    // Increment the counter
    sequenceNum++;
    if(strcmp(inputCode, solutionCode) == 0){
      state = State::Solved;
    }
    else{
      state = State::Running;
    }
    // Create JSON document - determine size using https://arduinojson.org/v6/assistant/
    StaticJsonDocument<48> jsonDoc;
    jsonDoc["id"] = "Keypad";
    jsonDoc["state"] = (state == State::Solved) ? "SOLVED" : "UNSOLVED";
    jsonDoc["input"] = inputCode;
    pc_sendUpdate(jsonDoc);
    // Reset
    if(sequenceNum == 4) {
      sequenceNum = 0;
      memset(inputCode, 0, sizeof(inputCode));
    }
  }

  // Important - call this on every iteration of loop
  pc_loop();
}
