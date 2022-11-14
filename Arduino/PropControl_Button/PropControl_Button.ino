/**
 * Node-Redscape example - MQTT over WiFi
 * Copyright (c)2022 Alastair Aitchison, Playful Technology
 * 
 * Example code for a simple button escape room puzzle integrated with Node-RED GM control.
 */

// INCLUDES
// Include appropriate WiFi system library depending on target platform
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  // ESP32 or Arduino
#elif defined(ESP32) || defined(__AVR__)
  #include <WiFi.h>
#endif
// MQTT client, see https://github.com/knolleary/pubsubclient
#include <PubSubClient.h>
// JSON serialisation, see https://arduinojson.org/
#include <ArduinoJson.h>
// Button input, see https://github.com/LennartHennigs/Button2
#include <Button2.h>

// CONSTANTS
// Define the pin to which button is attached
const byte buttonPin = D3;
// Unique name of this device, used as client ID to connect to MQTT server
// and also topic name for messages published to this device
const char* deviceID = "Button";
// SSID of the network to join
const char* wifiSSID = "Hyrule";
// Wi-Fi password if required
const char* wifiPassword = "molly1869";
// IP address of remote MQTT server
const IPAddress remoteServer(192, 168, 0, 136);
const int remotePort = 1883;
// Instance of the WiFi client object
WiFiClient networkClient;
// MQTT client based on the chosen network
PubSubClient mqttClient(server, port, networkClient);
// A re-usable buffer to hold MQTT messages to be sent/have been received
char mqttMsg[128];
// The MQTT topic in which to publish a message
char mqttTopic[32];

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

  // Start the WiFi connection
  Serial.print(F("Connecting to WiFi"));
  #if defined(ESP8266)
    WiFi.mode(WIFI_STA);
  #endif
  WiFi.begin(wifiSSID, wifiPassword);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println(F("done."));
  Serial.print(F("IP Address: "));
  Serial.println(WiFi.localIP());

  mqttClient.setCallback([](char* topic, byte* payload, unsigned int length) {
    // Note best practice is NOT to have a reusable JSON document, but create a new one each time it is needed
    // https://arduinojson.org/v6/assistant/
    StaticJsonDocument<64> jsonDoc;      
    deserializeJson(jsonDoc, payload, length);
    pc_receiveUpdate(jsonDoc);
  });
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

  // MQTT connection must be regularly serviced to process incoming/outgoing messages
  if(!mqttClient.connected()) { 
    // Loop until we're reconnected
    while(!mqttClient.connected()) {
      Serial.print(F("Connecting to MQTT server..."));
      if(mqttClient.connect(deviceID)) {
        // Subscribe to topics specifically for this device
        snprintf(topic, 32, "ToDevice/%s", deviceID);
        mqttClient.subscribe(topic);
        // Subscribe to topics intended for ALL devices on network
        mqttClient.subscribe("ToDevice/All");
      }
      else {
        char buffer[30];
        Serial.print(F("Failed to connect to MQTT server at IP %s:port %d\n", remoteServer.toString().c_str(), remotePort);
        Serial.print(buffer);
        Serial.print(F(", reason="));
        Serial.print(MQTTclient.state());
        Serial.println(F(" . Trying again in 5 seconds");
        delay(5000);
      }
    }
    Serial.println(F("Connected!"));
  }
  mqttClient.loop();
}
