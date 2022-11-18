/**
 * Node-Redscape example - ESP8266 MQTT over Wi-Fi
 * Copyright (c)2022 Alastair Aitchison, Playful Technology
 * 
 * Basic escape room puzzle controller integrated with Node-RED GM control
 * 
 * NOTE:
 * - If you have problems establishing a network connection on the ESP8266, 
 *   select the following option in Arduino IDE: 
 *   Tools > Erase Flash > All Flash Contents
 * 
 */

// REQUIREMENT CHECKS
#ifndef ESP8266
  // Example uses features not present in WiFi.h implementation used in Arduino boards
  #error "This code is designed for ESP8266 architecture"
#endif

// INCLUDES
// ESP8266 WiFi library, see https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/readme.html
#include <ESP8266WiFi.h>
// MQTT client, see https://github.com/knolleary/pubsubclient
#include <PubSubClient.h>
// JSON serialisation, see https://arduinojson.org/
#include <ArduinoJson.h>
// RFID input, see https://github.com/playfultechnology/PN5180-Library
#include <PN5180.h>
#include <PN5180ISO15693.h>
// OLED display. https://github.com/lexus2k/lcdgfx
#include "lcdgfx.h"

// CONSTANTS
// Unique name of this device, used as client ID to connect to MQTT server
// and also topic name for messages published to this device
const char* deviceID = "RFID-1";
// SSID of the network to join
const char* wifiSSID = "Hyrule";
// Wi-Fi password if required
const char* wifiPassword = "molly1869";
// IP address of remote MQTT server
const char* remoteMQTTServer = "192.168.0.114";
const int remoteMQTTPort = 1883;
const char* remoteMQTTUser = "user";
const char* remoteMQTTPass = "pass";

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
// Define GPGIO pins for PN5180 NSS, BUSY, and RESET lines
PN5180ISO15693 nfc(D8, D0, D4);
// WiFi network event handlers, see https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/generic-examples.html
WiFiEventHandler gotIpEventHandler, disconnectedEventHandler;
// Last UID scanned
unsigned long lastUpdateTime;
uint8_t lastUid[8];
// OLED display
DisplaySSD1306_128x64_I2C display(-1);

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

  // OLED display  
  display.setFixedFont(ssd1306xled_font6x8);
  display.begin();
  display.clear();
  display.printFixed(0, 0, "Node-REDscape", STYLE_NORMAL);
  display.printFixed(0, 8, "RFID Controller", STYLE_NORMAL);
  display.printFixed (0, 32, "Playful Technology", STYLE_BOLD);

  //PN5180 setup NSS, BUSY, and RESET
  Serial.println(F("Beginning NFC..."));
  nfc.begin();
  delay(100);
  Serial.println(F("Resetting NFC..."));
  nfc.reset();
  delay(100);
  Serial.println(F("Enabling RF field..."));
  nfc.setupRF();
  delay(100);

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
  gotIpEventHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& event) {
    Serial.print(F("Connected to "));
    Serial.print(wifiSSID);
    Serial.print(F(", IP:"));
    Serial.println(WiFi.localIP());
    networkState = WLAN_UP_MQTT_DOWN;
  });
  // Called when disconnected due to WiFi.disconnect() (because Wi-Fi signal is weak, or because the access point is switched off)
  disconnectedEventHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event) {
    // Fix to ensure WiFi.status() reflects correct status
    // see https://github.com/esp8266/Arduino/issues/7432#issuecomment-895352866
    (void)event;
    WiFi.disconnect();  
    Serial.println("Disconnected from WiFi");
    networkState = WLAN_DOWN_MQTT_DOWN;
    // ESP.restart();
  });
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
  jsonDoc["id"] = lastUid;
  snprintf(mqttTopic, 32, "FromDevice/%s", deviceID);
  serializeJson(jsonDoc, mqttMsg);
  mqttClient.publish(mqttTopic, mqttMsg);
}

void inputLoop() {
  // No need to poll the sensor every frame
  if(millis() - lastUpdateTime > 500) {
    // PN5180 can become unreponsive if receiving a corrupted response (e.g. if two tags are 
    // in range simultanesouly), so perform a complete reset and field reactivation on each iteration of loop():
    // https://github.com/ATrappmann/PN5180-Library/issues/48
    nfc.reset();
    nfc.setupRF();
  
    uint8_t currentUid[8] = {};
    memset(currentUid, 0, sizeof currentUid);
    // Try to read a tag ID (or "get inventory" in ISO15693-speak) 
    ISO15693ErrorCode rc = nfc.getInventory(currentUid);
    // If a card can be read
    if(rc == ISO15693_EC_OK) {
      // If the ID is different from that we read last frame
      if(memcmp(currentUid, lastUid, 8) != 0) {
        // Format it nicely for display
        char buffer[16];
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, 16, "%02x%02x%02x%02x%02x%02x%02x%02x", currentUid[7], currentUid[6], currentUid[5], currentUid[4], currentUid[3], currentUid[2], currentUid[1], currentUid[0] );
        display.printFixed(0,  16, buffer, STYLE_NORMAL);
        
        // Output to serial monitor
        Serial.println(buffer);
  
        //Update the stored value
        memcpy(lastUid, currentUid, 8);

        // Send status update to Node-RED
        sendUpdate();
      }
    }
    else {
      memset(lastUid, 0, sizeof lastUid);
      display.printFixed(0,  16, "No card in range", STYLE_NORMAL);
    }
    lastUpdateTime = millis();
  }
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
      // Set ESP8266 Wi-Fi Configuration. See https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/generic-class.html
      WiFi.setSleepMode(WIFI_NONE_SLEEP); // Ensure broadcast data is not missed by going to sleep
      WiFi.setOutputPower(17.5); // Setting lower Tx power can reduce signal noise
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
        networkState = WLAN_UP_MQTT_DOWN;
      }
    break;
  }
}

void loop(){
  // Use built-in LED as indicator of device state
  digitalWrite(LED_BUILTIN, !(state == State::Solved));
    
  // Process update loops
  inputLoop();
  networkLoop();
}
