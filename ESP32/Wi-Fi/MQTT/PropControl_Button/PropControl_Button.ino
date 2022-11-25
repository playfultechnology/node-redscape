/**
 * Node-Redscape example - ESP32 MQTT over Wi-Fi
 * Copyright (c)2022 Alastair Aitchison, Playful Technology
 * 
 * Basic escape room puzzle controller integrated with Node-RED GM control
 */

// REQUIREMENT CHECKS
#ifndef ESP32
  // Example uses features not present in WiFi.h implementation used in Arduino boards
  #error "This code is designed for ESP32 architecture"
#endif
#ifndef LED_BUILTIN
  // Different ESP32 boards have built-in LEDs attached to different pins
  #define LED_BUILTIN 2 
#endif

// INCLUDES
// ESP32 WiFi library, see https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/readme.html
#include <WiFi.h>
// MQTT client, see https://github.com/knolleary/pubsubclient
#include <PubSubClient.h>
// JSON serialisation, see https://arduinojson.org/
#include <ArduinoJson.h>
// Button input, see https://github.com/LennartHennigs/Button2
#include <Button2.h>

// CONSTANTS
// Unique name of this device, used as client ID to connect to MQTT server
// and also topic name for messages published to this device
const char* deviceID = "Button-1";
// SSID of the network to join
const char* wifiSSID = "YOURWIFINAME";
// Wi-Fi password if required
const char* wifiPassword = "YOURWIFIPASSWORD";
// IP address of remote MQTT server
const char* remoteMQTTServer = "192.168.0.136";
const int remoteMQTTPort = 1883;
const char* remoteMQTTUser = "user";
const char* remoteMQTTPass = "pass";
// Define the pin to which button is attached
const byte buttonPin = 5;

// GLOBALS
// Instance of the WiFi client object
WiFiClient networkClient;
// MQTT client based on the chosen network
PubSubClient mqttClient(remoteMQTTServer, remoteMQTTPort, networkClient);
// A re-usable buffer to hold MQTT messages to be sent/have been received
char mqttMsg[128];
// The MQTT topic in which to publish a message
char mqttTopic[32];
// Keep track of connection state of both WiFi and MQTT network
enum : byte { WLAN_DOWN_MQTT_DOWN, WLAN_STARTING_MQTT_DOWN, WLAN_UP_MQTT_DOWN, WLAN_UP_MQTT_STARTED, WLAN_UP_MQTT_UP } connectionState;
byte networkState = WLAN_DOWN_MQTT_DOWN;
// Track state of overall puzzle
enum State {Initialising, Running, Solved};
State state = Initialising;
Button2 button;
// WiFi network event handlers, see https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WiFiClientEvents/WiFiClientEvents.ino
WiFiEventId_t gotIpEventHandler, disconnectedEventHandler;

void setup(){
  // Initialise serial connection
  Serial.begin(115200);
  Serial.println("");
	Serial.println(__FILE__ __DATE__);
  
  // We'll use built-in LED to indicate state of the controller
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  
  
  // Set the puzzle state
  state = State::Running;

  button.begin(buttonPin);
  button.setPressedHandler([](Button2& btn) {
    // Debug
    Serial.println(F("Button Pressed"));
    // Toggle State
    if(state != State::Solved) { state = State::Solved; }
    else { state = State::Running; }
    // Update Node-RED with new state
    sendUpdate();  
  });

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
  StaticJsonDocument<128> jsonDoc;
  jsonDoc["id"] = deviceID;
  jsonDoc["state"] = (state == State::Solved) ? "SOLVED" : "UNSOLVED";
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

void loop(){
  // Use built-in LED as indicator of device state
  digitalWrite(LED_BUILTIN, (state == State::Solved));
    
  // Process update loops
  button.loop();
  networkLoop();
}
