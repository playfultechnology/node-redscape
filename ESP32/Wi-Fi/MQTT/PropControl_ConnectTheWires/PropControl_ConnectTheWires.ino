/**
 * Connect the Wires Puzzle
 * 
 * Node-Redscape example - ESP32 MQTT over Wi-Fi
 * Copyright (c)2022 Alastair Aitchison, Playful Technology
 * 
 * Example code for a simple RFID escape room puzzle integrated with Node-RED GM control.
*/

// REQUIREMENT CHECKS
#ifndef ESP32
  // Example uses features not present in Arduino WiFi.h implementation
  #error "This code is designed for ESP32 architecture"
#endif

// INCLUDES
// ESP32 Wi-Fi library
#include <WiFi.h>
// MQTT client (tested with v2.8.0), see https://github.com/knolleary/pubsubclient
#include <PubSubClient.h>
// Programmable LED display (tested with v3.5), see https://github.com/FastLED/FastLED
#include <FastLED.h>
// JSON serialization (tested with v6.19.4), see https://arduinojson.org
#include <ArduinoJson.h>

// CONSTANTS
// Unique ID used to identify this device
const char* deviceID = "Wires-1";
// SSID of the network to join
const char* wifiSSID = "Hyrule";
// Wi-Fi password if required
const char* wifiPassword = "molly1869";
// IP address of remote MQTT server
const char* remoteMQTTServer = "192.168.0.114";
const int remoteMQTTPort = 1883;
const char* remoteMQTTUser = "user";
const char* remoteMQTTPass = "pass";
// The total number of possible sockets from which connections can be made
const byte numSockets = 8;
// The array of pins to which each socket is connected
const byte socketPins[numSockets] = { 33, 25, 26, 27, 21, 19, 18, 5 };
// The total number of connections required to be made
const byte numConnections = 4;
// The connections that need to be made between pins, referenced by their index in the socketPins array
const byte connections[numConnections][2] = { {0,1}, {2,5}, {3,4}, {6,7} };
// Connected to the DIN of the programmable LED
const byte ledPin = 13;

// GLOBAL VARIABLES
// Instance of the WiFi client object
WiFiClient networkClient;
// MQTT client based on the chosen network
PubSubClient mqttClient(remoteMQTTServer, remoteMQTTPort, networkClient);
// A re-usable buffer to hold MQTT messages to be sent/have been received
char mqttMsg[256];
// The MQTT topic in which to publish a message
char mqttTopic[32];
// Keep track of connection state of both WiFi and MQTT network
enum : byte { WLAN_DOWN_MQTT_DOWN, WLAN_STARTING_MQTT_DOWN, WLAN_UP_MQTT_DOWN, WLAN_UP_MQTT_STARTED, WLAN_UP_MQTT_UP } NetworkState;
byte networkState = WLAN_DOWN_MQTT_DOWN;
// Track state of overall puzzle
enum State {Initialising, Running, Solved};
State state = Initialising;
// Track state of which output pins are correctly connected
bool connectionState[numConnections] = { false, false, false, false };
// WiFi network event handlers, see https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WiFiClientEvents/WiFiClientEvents.ino
WiFiEventId_t gotIpEventHandler, disconnectedEventHandler;
// Programmable LED array
CRGB leds[1];

void setup() {
  // Initialise serial connection
  Serial.begin(115200);
  Serial.println("");
  Serial.println(__FILE__ __DATE__);
  
  // Initialise the pins. When wires are disconnected, the input pins would be
  // floating and digitalRead would be unpredictable, so we'll initialise them
  // as INPUT_PULLUP and use the wires to connect them to ground.
  for (int i=0; i<numSockets; i++){	
    // The default state of all pins will be INPUT_PULLUP, though they will be reassigned throughout the duration of the program in order to test connection between pins
    pinMode(socketPins[i], INPUT_PULLUP);
  }

  // Set the device state
  state = Running;

  // Callback when MQTT message received in topic to which we are subscribed 
  mqttClient.setCallback([](char* topic, byte* payload, unsigned int length) {
    // Create a JSON document from the MQTT message received. Note best practice is NOT to have a reusable 
    // JSON document, but create a new one each time it is needed.  https://arduinojson.org/v6/assistant/
    StaticJsonDocument<128> jsonDoc;      
    deserializeJson(jsonDoc, payload, length);
    // Process the contents of the update received
    receiveUpdate(jsonDoc);
  });
  // Callback when assigned an IP address
  gotIpEventHandler = WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
    Serial.print(F("Connected to "));
    Serial.print(wifiSSID);
    Serial.print(F(", IP:"));
    Serial.println(WiFi.localIP());
    networkState = WLAN_UP_MQTT_DOWN;
  }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);  
  // Called when disconnected due to WiFi.disconnect() (because Wi-Fi signal is weak, or because the access point is switched off)
  disconnectedEventHandler = WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
        Serial.print("Disconnected from WiFi. Reason: ");
        Serial.println(info.wifi_sta_disconnected.reason);
        networkState = WLAN_DOWN_MQTT_DOWN;
    }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  // Tell FastLED about the LED strip configuration
  FastLED.addLeds<PL9823, ledPin>(leds, 1).setCorrection(TypicalLEDStrip);
  leds[0] = CRGB(255,0,0);
  FastLED.show();      
}

// Set the LED colour/pattern to indicate the status of the device
void ledLoop() {
  uint8_t hue = 0; // colour
  uint8_t v = 128; // brightness
  switch (networkState) {
    // If there is no Wi-Fi connection
    case WLAN_DOWN_MQTT_DOWN:
      v = beatsin8(125, 16, 128);
      break;
    case WLAN_STARTING_MQTT_DOWN:
      v = beatsin8(100, 16, 128);
      break;
    case WLAN_UP_MQTT_DOWN:
      v = beatsin8(75, 16, 128);
      break;
    case WLAN_UP_MQTT_STARTED:
      v = beatsin8(50, 16, 128);
      break;
    case WLAN_UP_MQTT_UP:
      unsigned long beat = millis() >> 10; // Approx once per second
      v = (beat % 2) ? 128 : 0;
      break;
  }
  // Solid Green
  if(state == State::Solved) {
    hue = 96; 
    v = 128;
  }
  leds[0] = CHSV(hue, 255, v); 
  FastLED.show();
}

void receiveUpdate(const JsonDocument& jsonDoc) {
  // Act upon command received
  if(jsonDoc["command"] == "SOLVE") { state = Solved; }
  else if(jsonDoc["command"] == "RESET") { state = Running; }
  // Now send refreshed values back
  sendUpdate();
}

void sendUpdate() {
  // Create JSON document reflecting current state - determine size using https://arduinojson.org/v6/assistant/
  StaticJsonDocument<512> jsonDoc;
  jsonDoc["id"] = deviceID;
  jsonDoc["state"] = (state == State::Solved) ? "S" : "U" ;
  JsonArray inputs = jsonDoc.createNestedArray("inputs");
  for(int i=0; i<numConnections; i++){
    JsonObject connection = inputs.createNestedObject();
    connection["F"] = connections[i][0];
    connection["T"] = connections[i][1];
    connection["S"] = connectionState[i] ? 1 : 0;
  }
  // Send to Serial
  serializeJson(jsonDoc, Serial);
  Serial.println("");
  // Or send via MQTT
  snprintf(mqttTopic, 32, "FromDevice/%s", deviceID);
  serializeJson(jsonDoc, mqttMsg);
  mqttClient.publish(mqttTopic, mqttMsg);
}

/**
 * A robust method that re-establishes connection to Wi-Fi WLAN network and MQTT server,
 * in the event of dropped connection/power loss etc.
 * See https://esp32.com/viewtopic.php?t=16109#p61846
 **/
void networkLoop() {
  // Keep track of time of last state change
  static unsigned long timeStamp;  

  switch (networkState) {
    // If there is no Wi-Fi connection
    case WLAN_DOWN_MQTT_DOWN:
      // Set ESP32 Wi-Fi Configuration. See https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/src/WiFiGeneric.h
      WiFi.setSleep(WIFI_PS_NONE); // Disable power-saving mode
      WiFi.setTxPower(WIFI_POWER_17dBm); // Setting lower Tx power can reduce signal noise. 
      WiFi.mode(WIFI_STA); // Operate only in STA station mode (i.e. client), not also in AP access point mode (i.e. server)
      WiFi.persistent(false); // Don't write Wi-Fi credentials to flash
      // Start the connection
      Serial.print("Starting WiFi connection to ");
      Serial.println(wifiSSID);
      WiFi.begin(wifiSSID, wifiPassword);
      // Set the timer
      timeStamp = millis();
      // And advance the state machine to the next state
      networkState = WLAN_STARTING_MQTT_DOWN;
      break;
    // If the WLAN router connection was started
    case WLAN_STARTING_MQTT_DOWN:
      // Allow 30 seconds since attempting to join the WiFi
      if (millis() - timeStamp >= 30000) {
        // Otherwise, if the WLAN router connection was not established
        Serial.println("Failed to start WiFi. Restarting...");
        // Clear the connection for the next attempt
        // WiFi stack corruption is the biggest reason for WiFi connection failures
        WiFi.disconnect();
        // And reset the state machine to its initial state (restart)
        networkState = WLAN_DOWN_MQTT_DOWN;
      }
      break;
    // If the WLAN router connection was established
    case WLAN_UP_MQTT_DOWN:
      // And if no MQTT broker connection was established yet
      if (!mqttClient.connected()) {
        Serial.print(F("Starting MQTT connection to "));
        Serial.println(remoteMQTTServer);
        timeStamp = millis();
        // Start the connection
        if (mqttClient.connect(deviceID, remoteMQTTUser, remoteMQTTPass)) {
          networkState = WLAN_UP_MQTT_STARTED;
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
    case WLAN_UP_MQTT_STARTED:
      if(mqttClient.connected()) {
        Serial.println("WLAN and MQTT connected");
        mqttClient.publish("ToHost", "Hi there");        
        // Subscribe to topics specifically for this device
        snprintf(mqttTopic, 32, "ToDevice/%s", deviceID);
        mqttClient.subscribe(mqttTopic);
        // Subscribe to topics intended for ALL devices on network
        mqttClient.subscribe("ToDevice/All");
        // Advance the state machine
        networkState = WLAN_UP_MQTT_UP;
      }
      else {
        networkState = WLAN_UP_MQTT_DOWN;
      }
      break;
    // If both the WLAN router and MQTT broker connections were established
    case WLAN_UP_MQTT_UP:
      if(mqttClient.connected()) {
        mqttClient.loop();
      }
      else {
        Serial.println("Network connection lost");
        networkState = WLAN_UP_MQTT_DOWN;
      }
    break;
  }
}

void inputLoop() {
  // Assume that the puzzle state has not changed since last reading
  bool stateChanged = false;
    // Assume that all wires are correctly inserted
  bool allConnectionsCorrect = true;
  // Check each connection in turn  
  for (int i=0; i<numConnections; i++){
    // Get the pin numbers that should be connected by this wire
    byte pin1 = socketPins[connections[i][0]];
    byte pin2 = socketPins[connections[i][1]];
    // Write a LOW signal to the output pin
    pinMode(pin1, OUTPUT);
    pinMode(pin2, INPUT_PULLUP);
    digitalWrite(pin1, LOW);
    delay(10);
    // If connected, the LOW signal should be detected on the input pin
    int currentState = !digitalRead(pin2);  
    // Set the output pin back to its default state
    pinMode(pin1, INPUT_PULLUP);
    // Has the connection state changed since last reading?
    if (currentState != connectionState[i]) {
      // Set the flag to show the state of the puzzle has changed
      stateChanged = true;
      // Update the stored connection state
      connectionState[i] = currentState;                  
    }
    // If any connection is incorrect, puzzle is not solved
    if(currentState == false) {
      allConnectionsCorrect = false;
    }    
  }
  // If the puzzle has just been solved
  if (allConnectionsCorrect && state == Running){
    state = Solved;
  }
  // If the previous solved puzzle has just been unsolved
  else if (!allConnectionsCorrect && state == Solved){
    state = Running;
  }
  // If a connection has been made/broken since last time we checked
  if(stateChanged) {
    sendUpdate();
  }
}

void loop() {
  // Process update loops
  inputLoop();
  networkLoop();
  ledLoop();
}
