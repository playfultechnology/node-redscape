/**
 * Node-Redscape example - Arduino MQTT over Ethernet
 * Copyright (c)2022 Alastair Aitchison, Playful Technology
 * 
 * Example code for a simple RFID escape room puzzle integrated with Node-RED GM control.
 */

// INCLUDES
// Networking library (tested with v2.0.1)
#include <Ethernet.h>
// MQTT client, see https://github.com/knolleary/pubsubclient
#include <PubSubClient.h>
// JSON serialization (tested with v6.19.4), see https://arduinojson.org
#include <ArduinoJson.h>
// Debouncing switch input (tested with v2.71.0), see https://github.com/thomasfredericks/Bounce2
#include <Bounce2.h>

// CONSTANTS
// Unique name of this device, used as client ID to connect to MQTT server
// and also topic name for messages published to this device
const char* deviceID = "Switches-1";
const byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
const IPAddress localIPAddress(192, 168, 1, 177);
// Credentials of remote MQTT server
const char* remoteMQTTServer = "192.168.0.114";
const int remoteMQTTPort = 1883;
const char* remoteMQTTUser = "user";
const char* remoteMQTTPass = "pass";
// GPIO pin connected to SS pin of ethernet module (default is 10 for most sheilds)
const byte ssPin = 10;
// The number of switches. Can be as many as you have digital pins available
const byte numSwitches = 5;
// The pins to which switches are connected. 
// Note that GPIO Pins 4, 10, 11, 12, 13 are used by the SPI interface to the Ethernet shield
const byte switchPins[numSwitches] = {A4, A3, A2, A1, A0};
// The desired solution state
const bool solution[numSwitches] = {true, false, true, false, true};

// GLOBALS
// Newtwork client
EthernetClient networkClient;
// MQTT client based on the chosen network
PubSubClient mqttClient(remoteMQTTServer, remoteMQTTPort, networkClient);
// A re-usable buffer to hold MQTT messages to be sent/have been received
char mqttMsg[128];
// The MQTT topic in which to publish a message
char mqttTopic[32];
// Keep track of connection state of both WiFi and MQTT server
enum : byte { LAN_DOWN_MQTT_DOWN, LAN_UP_MQTT_DOWN, LAN_UP_MQTT_STARTED, LAN_UP_MQTT_UP } NetworkState;
byte networkState = LAN_DOWN_MQTT_DOWN;
// Track state of overall puzzle
enum DeviceState {Initialising, Running, Solved};
DeviceState devicestate = DeviceState::Initialising;
// An array of bounce objects for each switch input
Bounce switches[numSwitches] = {Bounce(), Bounce(), Bounce(), Bounce(), Bounce()};
bool inputState[numSwitches] = {true, false, true, false, true};

void setup(){
  // Initialise serial connection
  Serial.begin(115200);
  Serial.println("");
	Serial.println(__FILE__ __DATE__);

  // Attach a debouncer to each switch
  for(int i=0; i<numSwitches; i++){
    switches[i].attach(switchPins[i], INPUT_PULLUP);
  }
 
  mqttClient.setCallback([](char* topic, byte* payload, unsigned int length) {
    // Create a JSON document from the MQTT message received. Note best practice is NOT to have a reusable 
    // JSON document, but create a new one each time it is needed.  https://arduinojson.org/v6/assistant/
    StaticJsonDocument<128> jsonDoc;      
    deserializeJson(jsonDoc, payload, length);
    // Process the contents of the update received
    receiveUpdate(jsonDoc);
  });
	
  // Set the puzzle state
  deviceState = DeviceState::Running;	
}

void inputLoop() {
  bool changed = false;
  bool solved = true;
  
  for(int i=0; i<numSwitches; i++){
    switches[i].update();
    if(switches[i].changed()) {
      inputState[i] = switches[i].read();
      changed = true;
    }
    if(switches[i].read() != solution[i]) { 
      // ... then the puzzle has not been solved
      solved = false;
    }
  }
  deviceState = (solved ? State::Solved : State::Running);
  // Only send an update if at least one input has been changed
  if(changed) {
    sendUpdate();
  }
}

void receiveUpdate(const JsonDocument& jsonDoc) {
  // Debug - send output to serial monitor
  // serializeJsonPretty(jsonDoc, Serial);
  // Act upon command received
  if(jsonDoc["command"] == "SOLVE") { deviceState = Solved; }
  else if(jsonDoc["command"] == "RESET") { deviceState = Running; }
  // Now send refreshed values back
  sendUpdate();
}

void registerWithNodeRED() {
  Serial.println(F("Registering with Node-RED"));
  // Create JSON document - determine size using https://arduinojson.org/v6/assistant/
  StaticJsonDocument<128> jsonDoc;
  jsonDoc["id"] = deviceID;
  jsonDoc["ip"] = Ethernet.localIP();
  if(networkState == LAN_UP_MQTT_UP) {
    snprintf(mqttTopic, 32, "FromDevice/%s", deviceID);
    serializeJson(jsonDoc, mqttMsg);
    mqttClient.publish(mqttTopic, mqttMsg);
  }
}

void sendUpdate() {
  // Create JSON document - determine size using https://arduinojson.org/v6/assistant/
  StaticJsonDocument<128> jsonDoc;
  jsonDoc["id"] = deviceID;
  JsonArray inputs = jsonDoc.createNestedArray("inputs");
  for(int i=0;i<numSwitches; i++){ 
    inputs.add(inputState[i]);
  }
  jsonDoc["state"] = (deviceState == DeviceState::Solved) ? "SOLVED" : "UNSOLVED";
  // Debug - send update to serial connection
  serializeJson(jsonDoc, Serial);
  Serial.println("");
  if(networkState == LAN_UP_MQTT_UP) {
    snprintf(mqttTopic, 32, "FromDevice/%s", deviceID);
    serializeJson(jsonDoc, mqttMsg);
    mqttClient.publish(mqttTopic, mqttMsg);
  }
  else {
    Serial.println(F("Can't send update"));
  }
}

/**
 * A robust method that re-establishes connection to Wi-Fi WLAN network and MQTT server,
 * in the event of dropped connection/power loss etc.
 * See https://esp32.com/viewtopic.php?t=16109#p61846
 **/
void networkLoop() {

  static unsigned long timeStamp;  

  switch (networkState) {
    // If there is no Wi-Fi connection
    case LAN_DOWN_MQTT_DOWN:
      // Start the connection
      Serial.println("Starting LAN connection");
      Ethernet.init(ssPin);
      if(Ethernet.begin(mac) == 0) {
        Serial.println(F("Failed to configure Ethernet using DHCP"));
        // Check for Ethernet hardware present
        if(Ethernet.hardwareStatus() == EthernetNoHardware) {
          Serial.println(F("Ethernet hardware not found"));
        }
        else if (Ethernet.linkStatus() == LinkOFF) {
          Serial.println(F("Ethernet cable is not connected."));
        }
        else { 
          // Try to configure using IP address instead of DHCP:
          Ethernet.begin(mac, localIPAddress);
          Serial.print(F("  Using IP "));
          Serial.println(Ethernet.localIP());
          // Set the timer
          timeStamp = millis();
          // And advance the state machine to the next state
          networkState = LAN_UP_MQTT_DOWN;
        }
      }
      else {
        Serial.print(F("  DHCP assigned IP "));
        Serial.println(Ethernet.localIP());
        // Set the timer
        timeStamp = millis();
        // And advance the state machine to the next state
        networkState = LAN_UP_MQTT_DOWN;
      }
      break;

    // If the LAN connection was established
    case LAN_UP_MQTT_DOWN:
      // And if no MQTT broker connection was established yet
      if (!mqttClient.connected()) {
        Serial.print(F("Started MQTT connection to "));
        Serial.println(remoteMQTTServer);
        timeStamp = millis();
        // Start the connection
        if (mqttClient.connect(deviceID, remoteMQTTUser, remoteMQTTPass)) {
          networkState = LAN_UP_MQTT_STARTED;
        }
       else {
          Serial.print(F("Failed. RC="));
          Serial.print(mqttClient.state());
          // Otherwise if the MQTT broker connection could not be established
          Serial.println(" Retrying...");
        }
      }
      break;
      
    // If the MQTT broker connection was started
    case LAN_UP_MQTT_STARTED:
      if(mqttClient.connected()) {
        Serial.println("LAN and MQTT connected");
        mqttClient.publish("ToHost", "Hi there");        
        // Subscribe to topics specifically for this device
        snprintf(mqttTopic, 32, "ToDevice/%s", deviceID);
        mqttClient.subscribe(mqttTopic);
        // Subscribe to topics intended for ALL devices on network
        mqttClient.subscribe("ToDevice/All");
        // Advance the state machine
        networkState = LAN_UP_MQTT_UP;
      }
      else {
        networkState = LAN_UP_MQTT_DOWN;
      }
      break;
      
    case LAN_UP_MQTT_UP:
      if(mqttClient.connected()) {
        mqttClient.loop();
      }
      else {
        Serial.println("MQTT connection lost");
        networkState = LAN_UP_MQTT_DOWN;
      }
      break;
  }
}

void loop(){
  // Process update loops
  inputLoop();
  networkLoop();
}
