/**
 * Node-Redscape example - ESP32 UDP over Ethernet
 * Copyright (c)2022 Alastair Aitchison, Playful Technology
 * 
 * Example code for a simple RFID escape room puzzle integrated with Node-RED GM control.
 */
// REQUIREMENT CHECKS
#ifndef ESP32
  // Example uses features not present in WiFi.h implementation used in Arduino boards
  #error "This code is designed for ESP32 architecture"
#endif

// DEFINES
#ifndef LED_BUILTIN
  // If not defined otherwise, assume LED attached to pin 2
  #define LED_BUILTIN 2 
#endif

// INCLUDES
// Networking library (tested with v2.0.1)
#include <Ethernet.h>
#include <EthernetUdp.h>
// JSON serialization (tested with v6.19.4), see https://arduinojson.org
#include <ArduinoJson.h>
// Button input, (tested with v2.1.0)see https://github.com/LennartHennigs/Button2
#include <Button2.h>

// CONSTANTS
// Unique name used to identify this device on the network
const char* deviceID = "Button-1";
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
const IPAddress localIPAddress(192, 168, 1, 177);
// IP address of remote MQTT server
const char* remoteUDPServer = "192.168.0.114";
const int remoteUDPPort = 161;
const int localUDPPort = 161;
// Define the pin to which button is attached
const byte buttonPin = 16;
// GPIO pin connected to SS pin of ethernet module
const byte ssPin = 5;

// GLOBALS
// UDP client
EthernetUDP Udp;
// A re-usable buffer to hold messages to be sent/have been received
char udpPacket[256];
// Keep track of connection state of both WiFi and UDP server
enum : byte { LAN_DOWN_UDP_DOWN, LAN_UP_UDP_DOWN, LAN_UP_UDP_UP } connectionState;
byte networkState = LAN_DOWN_UDP_DOWN;
// Track state of overall puzzle
enum State {Initialising, Running, Solved};
State state = Initialising;
Button2 button;

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
    //Serial.println(F("Button Pressed"));
    // Toggle State
    if(state != State::Solved) { state = State::Solved; }
    else { state = State::Running; }
    // Update Node-RED with new state
    sendUpdate();
  });
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
  jsonDoc["ip"] = Ethernet.localIP();
  jsonDoc["port"] = localUDPPort;
  if(networkState == LAN_UP_UDP_UP) {
    // Send UDP packet
    Udp.beginPacket(remoteUDPServer, remoteUDPPort);
    serializeJson(jsonDoc, Udp);
    Udp.println();
    Udp.endPacket();
  }
}

void sendUpdate() {
  // Create JSON document - determine size using https://arduinojson.org/v6/assistant/
  StaticJsonDocument<128> jsonDoc;
  jsonDoc["id"] = deviceID;
  jsonDoc["state"] = (state == State::Solved) ? "SOLVED" : "UNSOLVED";
  // Debug - send update to serial connection
  serializeJson(jsonDoc, Serial);
  Serial.println("");
  if(networkState == LAN_UP_UDP_UP) {
    // Send UDP packet
    Udp.beginPacket(remoteUDPServer, remoteUDPPort);
    serializeJson(jsonDoc, Udp);
    Udp.println();
    Udp.endPacket();
  }
  else {
    Serial.println(F("Can't send update"));
  }
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
    case LAN_DOWN_UDP_DOWN:
      // Start the connection
      Serial.println("Starting LAN connection");
      Ethernet.init(ssPin);
      if(Ethernet.begin(mac) == 0) {
        Serial.println(F("Failed to configure Ethernet using DHCP"));
        // Try to configure using IP address instead of DHCP:
        Ethernet.begin(mac, localIPAddress);
        Serial.print(F("  Using IP "));
        Serial.println(Ethernet.localIP());
        // Set the timer
        timeStamp = millis();
        // And advance the state machine to the next state
        networkState = LAN_UP_UDP_DOWN;
      }
      else {
        Serial.print(F("  DHCP assigned IP "));
        Serial.println(Ethernet.localIP());
        // Set the timer
        timeStamp = millis();
        // And advance the state machine to the next state
        networkState = LAN_UP_UDP_DOWN;
      }
      break;

    // If the WLAN router connection was established
    case LAN_UP_UDP_DOWN:
      Serial.print(F("Starting UDP Server on port "));
      Serial.println(localUDPPort);
      if(Udp.begin(localUDPPort)) {
        networkState = LAN_UP_UDP_UP;
        registerWithNodeRED();
      }
      else {
        Serial.println(F("Failed."));
      }
      break;

    case LAN_UP_UDP_UP:
      if(millis() - timeStamp > 500) {
        // https://www.arduino.cc/reference/en/libraries/ethernet/ethernet.maintain/
        Ethernet.maintain();
        timeStamp = millis();
      }
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
