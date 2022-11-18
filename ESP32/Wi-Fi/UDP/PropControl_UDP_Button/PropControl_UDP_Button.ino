/**
 * Node-Redscape example - ESP32 UDP over Wi-Fi
 * Copyright (c)2022 Alastair Aitchison, Playful Technology
 * 
 * Example code for a simple RFID escape room puzzle integrated with Node-RED GM control.
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
// ESP32 WiFi library
#include <WiFi.h>
#include <WiFiUdp.h>
// JSON serialization (tested with 6.19.4), see https://github.com/knolleary/pubsubclient
#include <ArduinoJson.h>
// Button input (tested with v2.1.0), see https://github.com/LennartHennigs/Button2
#include <Button2.h>

// CONSTANTS
// Unique name of this device, used as client ID to connect to MQTT server
// and also topic name for messages published to this device
const char* deviceID = "Button-1";
// SSID of the network to join
const char* wifiSSID = "Hyrule";
// Wi-Fi password if required
const char* wifiPassword = "molly1869";
// IP address of remote UDP server
const char* remoteUDPServer = "192.168.0.136";
const int remoteUDPPort = 161;
const int localUDPPort = 161;
// Define the pin to which button is attached
const byte buttonPin = 5;

// GLOBALS
// Instance of the WiFi client object
WiFiClient networkClient;
// UDP client based on the chosen network
WiFiUDP Udp;
// A re-usable buffer to hold messages to be sent/have been received
char udpPacket[256];
// Keep track of connection state of both Wi-Fi and UDP server
enum : byte { WLAN_DOWN_UDP_DOWN, WLAN_STARTING_UDP_DOWN, WLAN_UP_UDP_DOWN, WLAN_UP_UDP_UP } connectionState;
byte networkState = WLAN_DOWN_UDP_DOWN;
// Track state of overall puzzle
enum State {Initialising, Running, Solved};
State state = Initialising;
Button2 button;
// Wi-Fi network event handlers, see https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WiFiClientEvents/WiFiClientEvents.ino
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

  // Callback when assigned an IP address  
  gotIpEventHandler = WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
    Serial.print(F("Connected to "));
    Serial.print(wifiSSID);
    Serial.print(F(", IP:"));
    Serial.println(WiFi.localIP());
    networkState = WLAN_UP_UDP_DOWN;
  }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP); 
  // Called when disconnected due to WiFi.disconnect(), because Wi-Fi signal is weak, or because the access point is switched off.
  disconnectedEventHandler = WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
    // Fix to ensure WiFi.status() reflects correct
    // see https://github.com/esp8266/Arduino/issues/7432#issuecomment-895352866
    (void)event;
    WiFi.disconnect();  
    Serial.println("Disconnected from WiFi");
    networkState = WLAN_DOWN_UDP_DOWN;
    }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED); 
}

void receiveUpdate(const JsonDocument& jsonDoc) {
  // Debug - send output to serial monitor
  // serializeJsonPretty(jsonDoc, Serial);
  // Act upon command received
  if(jsonDoc["command"] == "SOLVE") { state = Solved; }
  else if(jsonDoc["command"] == "RESET") { state = Running; }
  // Now send refreshed values back
  sendUpdate();
}

void registerWithNodeRED() {
  Serial.println(F("Registering with Node-RED"));
  // Create JSON document - determine size using https://arduinojson.org/v6/assistant/
  StaticJsonDocument<128> jsonDoc;
  jsonDoc["id"] = deviceID;
  jsonDoc["ip"] = WiFi.localIP();
  jsonDoc["port"] = localUDPPort;
  // Send UDP packet
  Udp.beginPacket(remoteUDPServer, remoteUDPPort);
  serializeJson(jsonDoc, Udp);
  Udp.println();
  Udp.endPacket();
}

void sendUpdate() {
  // Create JSON document - determine size using https://arduinojson.org/v6/assistant/
  StaticJsonDocument<128> jsonDoc;
  jsonDoc["id"] = deviceID;
  jsonDoc["state"] = (state == State::Solved) ? "SOLVED" : "UNSOLVED";
  // Debug - send update to serial connection
  serializeJson(jsonDoc, Serial);
  // Send UDP packet
  Udp.beginPacket(remoteUDPServer, remoteUDPPort);
  serializeJson(jsonDoc, Udp);
  Udp.println();
  Udp.endPacket();
}

void udpLoop() {
  int packetSize = Udp.parsePacket();
  if(packetSize) {
    // Serial.printf("Received %d bytes from %s, port %d\n", packetSize, Udp.remoteIP().toString().c_str(), Udp.remotePort());
    int len = Udp.read(udpPacket, 255);
    if (len > 0){ udpPacket[len] = '\0'; }
    // Serial.printf("UDP packet contents: %s\n", udpPacket);
    // Create a JSON document from the UDP packetreceived. Note best practice is NOT to have a reusable 
    // JSON document, but create a new one each time it is needed.  https://arduinojson.org/v6/assistant/
    StaticJsonDocument<128> jsonDoc;      
    deserializeJson(jsonDoc, udpPacket);
    // Process the contents of the update received
    receiveUpdate(jsonDoc);
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
    case WLAN_DOWN_UDP_DOWN:
     // Set ESP32 Wi-Fi Configuration. See https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/src/WiFiGeneric.h
      WiFi.disconnect();
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
      networkState = WLAN_STARTING_UDP_DOWN;
      break;

    // If the WLAN router connection was started
    case WLAN_STARTING_UDP_DOWN:
      // Allow 30 seconds since attempting to join the Wi-Fi
      if (millis() - timeStamp >= 30000) {
        // Otherwise, if the WLAN router connection was not established
        Serial.println("Failed to start WiFi. Restarting...");
        // Clear the connection for the next attempt
        // WiFi stack corruption is the biggest reason for WiFi connection failures
        WiFi.disconnect();
        // And reset the state machine to its initial state (restart)
        networkState = WLAN_DOWN_UDP_DOWN;
      }
      break;

    // If the WLAN router connection was established
    case WLAN_UP_UDP_DOWN:
      Serial.print(F("Starting UDP Server on port "));
      Serial.println(localUDPPort);
      if(Udp.begin(localUDPPort)) {
        registerWithNodeRED();
        networkState = WLAN_UP_UDP_UP;
      }
      else {
        Serial.println(F("Failed."));
      }
      break;

    case WLAN_UP_UDP_UP:
      udpLoop();
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