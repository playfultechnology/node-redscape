/**
 * Arduino DMX Controller
 * Playful Technology (c) 2019
 * 
 * Demonstrates how an Arduino + DMX shield can be used as a device to trigger
 * lighting and other stage effects, either based on direct input (e.g. button press)
 * or via a JSON payload received on serial port connection, such as
 * might be transmitted by Node-RED.
 * 
 * Tested using the Conceptinetics Arduino DMX shield available from 
 * https://www.tindie.com/products/Conceptinetics/25kv-isolated-dmx-512-shield-for-arduino-r2/
 * but should work with other DMX shields, or even DIY DMX-shields using MAX485 chip as described at
 * https://playground.arduino.cc/DMX/DMXShield/
 */

// INCLUDES
// Modified version of the DMX library based on https://github.com/PaulStoffregen/DmxSimple
#include "src/DMXSimple/DMXSimple.h"
// JSON serialisation/desserialisation library download from https://arduinojson.org/
#include <ArduinoJson.h>

// CONSTANTS
// The pin on which Arduino will transmit DMX packets
const byte dmxTransmitPin = 4;
// Used to switch between Master/Slave modes
const byte modeSelectPin = 2;
// Buttons attached to following pins will be used to demonstrate inputs
const byte blueButton = A0;
const byte redButton = A1;

void setup() {         
  // Serial connection is used to receive DMX request as JSON payload from another device, e.g. Node-RED
  Serial.begin(9600);

  // Print out some help text to show example payload expected
  Serial.println("Send DMX channel/values as JSON payload in the following format:");
  Serial.println("{\"channels\":[");
  Serial.println("  {\"channel\":1,\"value\":255},");
  Serial.println("  {\"channel\":3,\"value\":128}");
  Serial.println("]}");

  // If using the Conceptinetics Arduino DMX shield, set to "DMX Master" mode
  // (i.e. sets the line driver to write mode rather than slave mode)
  pinMode(modeSelectPin, OUTPUT);
  digitalWrite(modeSelectPin, HIGH);
  
  // Specify the pin that the Arduino will use to communicate with the DMX shield
  DmxSimple.usePin(dmxTransmitPin);

  // By default, DmxSimple only sends up data in DMX channels that have actually been
  // used. Some DMX devices require the whole 512byte message to be sent, which can be
  // done as follows: 
  DmxSimple.maxChannel(512);

  // Initialise the button pins
  pinMode(blueButton, INPUT_PULLUP);
  pinMode(redButton, INPUT_PULLUP);
}

void loop() {
  // For simple effects, we can manually set individual channel values
  // by calling DmxSimple.write
  if(!digitalRead(blueButton)) {
    DmxSimple.write(1,255); // Brightness
    DmxSimple.write(2,0); // R
    DmxSimple.write(3,0); // G
    DmxSimple.write(4,255); // B
  }
  if(!digitalRead(redButton)) {
    DmxSimple.write(1,255); // Brightness
    DmxSimple.write(2,255); // R
    DmxSimple.write(3,0); // G
    DmxSimple.write(4,0); // B
  }

  // For more complex effects, we'll process a JSON encoded DMX string
  // containing an array of channel:value pairs received over the serial port
  if(Serial.available()){
    // Create a new JSON document
    DynamicJsonDocument doc(512);
    // Parse any incoming data received over the serial connection
    DeserializationError error = deserializeJson(doc, Serial);
    // Check that the data is valid
    if(error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      return;
    }
    // Parse the array of channel:values from the received string
    JsonArray array = doc["channels"].as<JsonArray>();
      for(JsonVariant v : array) {
        Serial.println(v["channel"].as<int>());
        Serial.println(v["value"].as<int>());
        // Write each value to the correct channel
        DmxSimple.write(v["channel"].as<int>(), v["value"].as<int>());
    }
  }
}
