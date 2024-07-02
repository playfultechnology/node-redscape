/**
 * Node-Redscape example - ESP32 (tested with v2.0.16) MQTT over Wi-Fi
 * Copyright (c)2023 Alastair Aitchison, Playful Technology
 * 
 * Basic escape room puzzle controller integrated with Node-RED GM control
 * 
 * NOTE:
 * - If you have problems establishing a network connection on the ESP32, 
 *   it could be due to a corrupt WiFi configuration  - select the following in Arduino IDE: 
 *   Tools > Erase Flash > All Flash Contents
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
// JSON serialisation (tested with v6.19.4), see https://arduinojson.org/
#include <ArduinoJson.h>
// RFID input, see https://github.com/playfultechnology/PN5180-Library
#include <PN5180.h>
#include <PN5180ISO15693.h>
// OLED display (tested with v1.1.1), see https://github.com/lexus2k/lcdgfx
#include <SPI.h>
#include <lcdgfx.h>
// For LED status display
#include <FastLED.h>

// CONSTANTS
// Unique name of this device, used as client ID to connect to MQTT server
// and also topic name for messages published to this device
const char* deviceID = "RFID-1";
// SSID of the network to join
const char* wifiSSID = "vodafone1236D8";
// Wi-Fi password if required
const char* wifiPassword = "q3TGpAbgHL7KaYsp";
// IP address of remote MQTT server
const char* remoteMQTTServer = "192.168.1.152";
const int remoteMQTTPort = 1883;
const char* remoteMQTTUser = "user";
const char* remoteMQTTPass = "pass";
const uint8_t solveUid[8] = {0xA4, 0x0A, 0x6D, 0xBE, 0x50, 0x01, 0x04, 0xE0};
// Connected to the DIN of the programmable LED
const byte ledPin = 4;

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
enum : byte { WLAN_DOWN_MQTT_DOWN, WLAN_STARTING_MQTT_DOWN, WLAN_UP_MQTT_DOWN, WLAN_UP_MQTT_STARTED, WLAN_UP_MQTT_UP } NetworkState;
byte networkState = WLAN_DOWN_MQTT_DOWN;
// Track state of overall puzzle
enum DeviceState {Initialising, Running, Solved};
DeviceState deviceState = DeviceState::Initialising;
// Define GPIO pins for PN5180 NSS, BUSY, and RESET lines
PN5180ISO15693 nfc(16, 5, 17);
// WiFi network event handlers, see https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WiFiClientEvents/WiFiClientEvents.ino
WiFiEventId_t gotIpEventHandler, disconnectedEventHandler;
// Last UID scanned
unsigned long lastUpdateTime;
uint8_t lastUid[8];
// OLED display
DisplaySSD1306_128x64_I2C display(-1);
// Programmable LED array
CRGB leds[1];

void setup(){
  // Initialise serial connection
  Serial.begin(115200);
  Serial.println("");
	Serial.println(__FILE__ __DATE__);

  // OLED display  
  display.setFixedFont(ssd1306xled_font6x8);
  display.begin();
  display.clear();
  display.printFixed(0, 0, "Node-REDscape", STYLE_NORMAL);
  display.printFixed(0, 8, "RFID Controller", STYLE_NORMAL);
  display.printFixed (0, 32, "Playful Technology", STYLE_BOLD);

  // PN5180 setup NSS, BUSY, and RESET
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
  gotIpEventHandler = WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
    Serial.print(F("Connected to "));
    Serial.print(wifiSSID);
    Serial.print(F(", IP:"));
    Serial.println(WiFi.localIP());
    networkState = WLAN_UP_MQTT_DOWN;
  }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP); 
  // Called when disconnected due to WiFi.disconnect() (because Wi-Fi signal is weak, or because the access point is switched off)
  disconnectedEventHandler = WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
    // Fix to ensure WiFi.status() reflects correct status
    // see https://github.com/esp8266/Arduino/issues/7432#issuecomment-895352866
    (void)event;
    WiFi.disconnect();  
    Serial.println("Disconnected from WiFi");
    networkState = WLAN_DOWN_MQTT_DOWN;
    // ESP.restart();
    }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);  

  // Initialise programmable status LED
  FastLED.addLeds<PL9823, ledPin>(leds, 1).setCorrection(TypicalLEDStrip);
  leds[0] = CRGB(255,0,0);
  FastLED.show();

  // Set the device state
  deviceState = DeviceState::Running;
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
      v = beatsin8(100,16, 128);
      break;
    case WLAN_UP_MQTT_DOWN:
      v = beatsin8(75,16, 128);
      break;
    case WLAN_UP_MQTT_STARTED:
      v = beatsin8(50,16, 128);
      break;
    case WLAN_UP_MQTT_UP:
      unsigned long beat = millis() >> 10; // Approx once per second
      v = (beat % 2) ? 128 : 0;
      break;
  }
  // Solid Green
  if(deviceState == DeviceState::Solved) {
    hue = 96; 
    v = 128;
  }
  leds[0] = CHSV(hue, 255, v); 
  FastLED.show();
}

void receiveUpdate(const JsonDocument& jsonDoc) {
  // Act upon command received
  if(jsonDoc["command"] == "SOLVE") { deviceState = DeviceState::Solved; }
  else if(jsonDoc["command"] == "RESET") { deviceState = DeviceState::Running; }
  // Now send refreshed values back
  sendUpdate();
}

void sendUpdate() {
  // Create JSON document reflecting current state - determine size using https://arduinojson.org/v6/assistant/
  StaticJsonDocument<128> jsonDoc;
  jsonDoc["id"] = deviceID;
  jsonDoc["state"] = (deviceState == DeviceState::Solved) ? "SOLVED" : "UNSOLVED";
  char uid[17];
  memset(uid, 0, sizeof uid);
  snprintf(uid, 16, "%02x%02x%02x%02x%02x%02x%02x%02x", lastUid[7], lastUid[6], lastUid[5], lastUid[4], lastUid[3], lastUid[2], lastUid[1], lastUid[0] );
  jsonDoc["input"] = uid;
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
        char buffer[17];
        memset(buffer, 0, sizeof buffer);
        snprintf(buffer, 17, "%02x%02x%02x%02x%02x%02x%02x%02x", currentUid[7], currentUid[6], currentUid[5], currentUid[4], currentUid[3], currentUid[2], currentUid[1], currentUid[0] );
        display.printFixed(0, 16, buffer, STYLE_NORMAL);
        // Output to serial monitor
        Serial.println(buffer);
        // Update the stored value
        memcpy(lastUid, currentUid, 8);
        // If it's the correct ID
        if(memcmp(currentUid, solveUid, 8) == 0) {
          deviceState = DeviceState::Solved;
        }
        // Send status update to Node-RED
        sendUpdate();
      }
    }
    else {
      // If we could previously read a RFID tag
      if(memcmp(lastUid, currentUid, 8) != 0) {
        // Update (i.e. clear) the stored value
        memcpy(lastUid, currentUid, 8);
        // Update LCD display
        display.printFixed(0,  16, "No card in range", STYLE_NORMAL);
        // Output to serial monitor
        Serial.println(F("No card in range"));
        // Update the state
        deviceState = DeviceState::Running;
        // Send status update to Node-RED
        sendUpdate();
      }
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
      if (millis() - timeStamp > 30000) {
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
      if (!mqttClient.connected() && (millis() - timeStamp > 1000)) {
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
  // Process update loops
  inputLoop();
  networkLoop();
  ledLoop();
}
