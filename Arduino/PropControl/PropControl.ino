// DEFINES
// valid values are USE_ETHERNET or USE_WIFI
#define USE_WIFI

// INCLUDES
#if defined(USE_ETHERNET)
  #include <Ethernet.h>
#elif defined(USE_WIFI)
  #if defined(ESP8266)
    #include <ESP8266WiFi.h>
  #elif defined(ESP32) || defined(__AVR__)
    #include <WiFi.h>
  #endif
#endif
#include "PropControl/src/PropControl.h"

// CONSTANTS
// SSID of the network to join
const char* ssid = "VodafoneConnect53686628";
// Wi-Fi password if required
const char* password = "8p2ty6329x2mk6v";
// The IP address of the MQTT server
const IPAddress server(192,168,1,7);
char* deviceID = "clientTest";

// GLOBALS
// Network connection object - WiFi/Ethernet as appropriate
#if(USE_ETHERNET) 
  EthernetClient networkClient;
#elif defined(USE_WIFI)  
  WiFiClient networkClient;
#endif
// PropControl object
PlayfulTechnology::PropControl pc(networkClient, server, 1883, deviceID);


void onTest() {
  Serial.println("FLiiping Heck!");
}
void onPing() {
  Serial.println("Pong!");
}


void defaultCallback(char* topic, byte* payload, unsigned int length) {
  pc.processCommand(topic, payload, length);
    Serial.println("buggrits");

  if(strcmp((char*)payload, "TEST") == 0) {
            // ... call the associated handler function
            Serial.print("OOOOOOO");
            
          }

    
  }

void setup() {
  pc.setCallback(defaultCallback);

  pc.registerCommand("Test", onTest);
  pc.registerCommand("Ping", [](){Serial.println("Pong!");});

  Serial.begin(9600);

  Serial.print(F("Connecting to WiFi..."));
  // Start the WiFi connection
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    #ifdef DEBUG
    Serial.print(".");
    #endif
  }
  Serial.print(F("done! IP Address:"));
  Serial.println(WiFi.localIP());

  if (pc.connect()) {
    Serial.println(F("Connected to Prop Control Server"));
  }

  // To receive messages, must have subscribed to at least one topic, and also defined a callback function.
  pc.subscribeToSelf();
  pc.subscribeToAll();
}

void loop() {
  // put your main code here, to run repeatedly:
  pc.loop();
  delay(500);
}
