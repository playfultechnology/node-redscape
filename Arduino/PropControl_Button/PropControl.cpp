/**
 * Node-Redscape Prop Control,  Copyright (c) 2019-2022 Playful Technology
 * 
 * This code is designed to be run on an Arduino-type device (Uno/Nano/ESP32/ESP8266 etc.) to integrate 
 * an escape room prop via a serial, wired ethernet, or Wi-Fi connection to a Node-RED server, 
 * using JSON-formatted messages over Serial/MQTT/HTTP. 
 * 
 * This allows a gamesmaster to use the Node-RED dashboard to monitor all inputs the players make into 
 * a prop, integrating it to trigger events as part of a wider gameflow, and to reset, and override the 
 * puzzle state remotely.
 * 
 * Usage:
  jsonDoc["id"] = "Keypad";
  jsonDoc["state"] = (state == State::Solved) ? "SOLVED" : "UNSOLVED" ;
  jsonDoc["input"] = inputCode;
 *
 * Tested on:
 * Arduino Nano / Arduino UNO / Arduino Mega / Wemos D1 Mini / Devkit ESP32 v1 (via USB Serial)
 * Arduino UNO (via Ethernet Shield)
 * Wemos D1 Mini / Devkit ESP32 v1 (via Wi-Fi)
 */
 
// DEFINES
// Define the physical layer by which the device is connected to Node-RED. Valid values are: 
// USE_ETHERNET (e.g. an Arduino UNO with a W5100/5500 ethernet shield)
// USE_WIFI (e.g. an ESP8266 or ESP32 with built-in Wi-Fi)
// USE_SERIAL (e.g. RS232 over USB cable or RS485 transceiver)
// USE_SERIAL1
// USE_SERIAL2
// USE_SERIAL3
// USE_SOFTWARESERIAL 
#define PHYSICAL_LAYER USE_WIFI

// Define the transport/application protocol layer
// Valid values are USE_TCP, USE_UDP, USE_SERIAL, USE_MQTT (over TCP)
#define PROTOCOL USE_MQTT
 
// INCLUDES
#include "Arduino.h"
#include "PropControl.h"
// Include appropriate networking library depending on target board architecture
#if (PHYSICAL_LAYER == USE_ETHERNET)
  #include <Ethernet.h>
  #if (PROTOCOL == USE_UDP)
    #include <EthernetUdp.h>
  #endif
// Wi-Fi connection
#elif (PHYSICAL_LAYER == USE_WIFI)
  // ESP8266
  #if defined(ESP8266)
    #include <ESP8266WiFi.h>
  // ESP32 or Arduino
  #elif defined(ESP32) || defined(__AVR__)
    #include <WiFi.h>
  #endif
  #if (PROTOCOL == USE_UDP)
    #include <WiFiUdp.h>
  #endif
#elif (PHYSICAL_LAYER == USE_SERIAL) 
  // No additional libraries needed for built-in hardware serial
#elif (PHYSICAL_LAYER == USE_SOFTWARESERIAL)
  // AltSoftSerial is a more robust SW serial library, but only supported on AVR boards (UNO, Nano, etc.)
  #if defined(__AVR__)
    #include <AltSoftSerial.h>
  #else
    #include <SoftwareSerial.h>
  #endif
#endif
#if (PROTOCOL == USE_MQTT)
	// MQTT client, see https://github.com/knolleary/pubsubclient
	#include <PubSubClient.h>
#endif
// JSON serialisation, see https://arduinojson.org/
#include <ArduinoJson.h>

// CONSTANTS  
// Unique name of this device, used as client ID to connect to MQTT server
// and also topic name for messages published to this device
const char* deviceID = "Button";
#if (PHYSICAL_LAYER == USE_ETHERNET)
  // Enter a MAC address for your controller below.
  // Newer Ethernet shields have a MAC address printed on a sticker on the shield
  byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
  // Set the static IP address to use. Or, set (0, 0, 0, 0) to use DHCP, but
  // be aware that increases code size.
  const IPAddress ip(192, 168, 0, 31);
#elif (PHYSICAL_LAYER == USE_WIFI)
  // SSID of the network to join
  const char* ssid = "Hyrule";
  // Wi-Fi password if required
  const char* password = "molly1869";
#endif

#if (PROTOCOL == USE_TCP)
  const IPAddress server(192, 168, 0, 136);
  const int port = 80;
#elif (PROTOCOL == USE_UDP)
  const IPAddress server(192, 168, 0, 136);
  const int port = 161;
#elif (PROTOCOL == USE_MQTT)
  const IPAddress server(192, 168, 0, 136);
  const int port = 1883;
#endif

// GLOBALS
#if (PHYSICAL_LAYER == USE_ETHERNET)
  EthernetClient networkClient;   // Client to send data to Node-RED
#elif (PHYSICAL_LAYER == USE_WIFI)
  WiFiClient networkClient; // Client to send data to Node-RED
#elif (PHYSICAL_LAYER == USE_SERIAL)
  #define pcSerial Serial
#elif (PHYSICAL_LAYER == USE_HWSERIAL1)
  #if defined(HAVE_HWSERIAL1)
    #define pcSerial Serial1
  #else
    #error "Target device does not have SERIAL1 hardware"
  #endif
#elif (PHYSICAL_LAYER == USE_HWSERIAL2)
  #if defined(HAVE_HWSERIAL2)
    #define pcSerial Serial2
  #else
    #error "Target device does not have SERIAL2 hardware"
  #endif
#elif (PHYSICAL_LAYER == USE_HWSERIAL3)
  #if defined(HAVE_HWSERIAL3)
    #define pcSerial Serial3
  #else
    #error "Target device does not have SERIAL3 hardware"
  #endif     
#elif (PHYSICAL_LAYER == USE_SWSERIAL)
  #if defined(__AVR__)
    AltSoftSerial pcSerial;
  #else
    SoftwareSerial pcSerial(8,9);
  #endif
#endif
#if (PROTOCOL == USE_TCP)
  #if (PHYSICAL_LAYER == USE_ETHERNET)
    // And a server instance to receive requests from Node-RED
    EthernetServer networkServer(80);
  #elif (PHYSICAL_LAYER == USE_WIFI)
    // Create a local webserver to receive HTTP requests from Node-RED
    WiFiServer networkServer(80);
  #endif
#elif (PROTOCOL == USE_UDP)
  #if (PHYSICAL_LAYER == USE_ETHERNET)
    // https://docs.arduino.cc/tutorials/ethernet-shield-rev2/udp-send-receive-string
    EthernetServer networkServer(80);    
    EthernetUDP UDP;
  #elif (PHYSICAL_LAYER == USE_WIFI)
    // https://siytek.com/esp8266-udp-send-receive/
    WiFiUDP UDP;
  #endif
#elif (PROTOCOL == USE_MQTT)
  // Create a propcontrol object based on the chosen network
  PubSubClient MQTTclient(server, port, networkClient);
  // A re-usable buffer to hold messages to be sent/have been received
  char msg[128];
  // The topic in which to publish a message
  char topic[32];
#endif

void pc_setup(){
  // Initialise network layer
  #if (PHYSICAL_LAYER == USE_ETHERNET)
    // Use specified IP address
		if(ip != INADDR_NONE) { Ethernet.begin(mac, ip); }
    // Use DHCP if no IP address defined
		else { Ethernet.begin(mac); }
    Serial.print(F("LAN IP: "));
    Serial.println(Ethernet.localIP());
  #elif (PHYSICAL_LAYER == USE_WIFI)
    // Start the WiFi connection
    Serial.print(F("Connecting to WiFi"));
    #if defined(ESP8266)
      WiFi.mode(WIFI_STA);
    #endif
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.print(".");
    }
    Serial.println(F("done."));
    Serial.print(F("IP Address: "));
    Serial.println(WiFi.localIP());
  #elif (PHYSICAL_LAYER == USE_SERIAL)
    // Initialise the appropriate chosen serial connection
    pcSerial.begin(9600);
  #endif

  // Initialise the transport/application protocol layer 
  #if (PROTOCOL == USE_TCP || PROTOCOL == USE_UDP)
    networkServer.begin();
    #if (PROTOCOL == USE_UDP)
      UDP.begin(port);
    #endif
	#elif (PROTOCOL == USE_MQTT)
    MQTTclient.setCallback([](char* topic, byte* payload, unsigned int length) {
      // Note best practice is NOT to have a reusable JSON document, but create a new one each time it is needed
      // https://arduinojson.org/v6/assistant/
      StaticJsonDocument<64> jsonDoc;      
      deserializeJson(jsonDoc, payload, length);
      pc_receiveUpdate(jsonDoc);
    });
	#endif
}

void pc_sendUpdate(const JsonDocument& jsonDoc) {
  #if (PROTOCOL == USE_TCP)
    // POST to HTTP
    if(networkClient.connect(server, port)) {
      networkClient.print("POST /");
      networkClient.println(" HTTP/1.1");
      networkClient.print("Host: ");
      //networkClient.println(ip.ToString());
      networkClient.println("Connection: close\r\nContent-Type: application/json");
      networkClient.print("Content-Length: ");
      networkClient.print(measureJson(jsonDoc));
      networkClient.print("\r\n");
      networkClient.println();
      serializeJson(jsonDoc, networkClient);
    }
  #elif (PROTOCOL == USE_UDP)
    /*
    UDP.beginPacket(Udp.remoteIP(), Udp.remotePort());
    UDP.write(ReplyBuffer);
    UDP.endPacket();
    */
	#elif (PROTOCOL == USE_SERIAL)
		serializeJson(jsonDoc, pcSerial);
		Serial.println("");
	#elif (PROTOCOL == USE_MQTT)
	  // Set the topic
		snprintf(topic, 32, "FromDevice/%s", deviceID);
    serializeJson(jsonDoc, msg);
		MQTTclient.publish(topic, msg);
	#endif
}

// Note function signature from https://github.com/bblanchon/ArduinoJson/issues/989
void pc_receiveUpdate(const JsonDocument& jsonDoc) {
  // TESTING! Send the received JSON to serial
  serializeJson(jsonDoc, Serial);
  Serial.println("");
}


void pc_loop(){
  #if (PROTOCOL == USE_TCP)
    // Check if there is an incoming client  
    #if (PHYSICAL_LAYER == USE_ETHERNET)
      EthernetClient client = networkServer.available();
    #elif (PHYSICAL_LAYER == USE_WIFI)
      WiFiClient client = networkServer.available();
    #endif
    // If there is a client...
    if(client) {
      // As long as the client is still connected
      while (client.connected()) {
        // And there are bytes to read from the request
        if (client.available()) {
          // Read the very first line of the request
          char line[64];
          int l = client.readBytesUntil('\n', line, sizeof(line));
          line[l] = 0;
          // Check that the first line begins with "POST"
          if (strncmp_P(line, PSTR("POST"), strlen("POST")) == 0) {
            // If so, skip until we get to the blank line that separates the header from the body
            client.find((char*) "\r\n\r\n");
            // And then deserialize the rest into a 
            // https://arduinojson.org/v6/assistant/
            const int JsonSize = JSON_OBJECT_SIZE(2) + JSON_ARRAY_SIZE(4);
            StaticJsonDocument<JsonSize> jsonDoc;
            // See https://arduinojson.org/v5/faq/can-i-parse-data-from-a-stream/
            deserializeJson(jsonDoc, client);
            // Act upon update received
            pc_receiveUpdate(jsonDoc);
          }
        }
      }
    }
  #elif (PROTOCOL == USE_UDP)
    // Check for incoming packets
    int packetSize = UDP.parsePacket();
    if (packetSize) {
      Serial.print("Received packet of size ");
      Serial.println(packetSize);
      Serial.print("From ");
      IPAddress remote = UDP.remoteIP();
      for (int i=0; i < 4; i++) {
        Serial.print(remote[i], DEC);
        if (i < 3) { Serial.print("."); }
      }
      Serial.print(", port ");
      Serial.println(UDP.remotePort());
      // read the packet into packetBufffer
      char packetBuffer[40];
      UDP.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
      Serial.println("Contents:");
      Serial.println(packetBuffer);

      // Act upon update received
      pc_receiveUpdate(jsonDoc); 
    }
  #elif (PROTOCOL == USE_SERIAL)
    if(pcSerial.available()){
      // Allocate the JSON document
      // This one must be bigger than the sender's because it must store the strings
      StaticJsonDocument<300> doc;
  
      // Read the JSON document from the "link" serial port
      DeserializationError err = deserializeJson(doc, pcSerial);
  
      if (err == DeserializationError::Ok) {
        // Print the values
        // (we must use as<T>() to resolve the ambiguity)
        Serial.print("timestamp = ");
        Serial.println(doc["timestamp"].as<long>());
        Serial.print("value = ");
        Serial.println(doc["value"].as<int>());
      } 
      else {
        // Print error to the "debug" serial port
        Serial.print("deserializeJson() returned ");
        Serial.println(err.c_str());
    
        // Flush all bytes in the "link" serial port buffer
        while (pcSerial.available() > 0)
          pcSerial.read();
      }
    }
	#elif (PROTOCOL == USE_MQTT)
    // MQTT connection must be regularly serviced to process incoming/outgoing messages
		if (!MQTTclient.connected()) { 
      // Loop until we're reconnected
      while (!MQTTclient.connected()) {
        if (MQTTclient.connect(deviceID)) {
          // ... and resubscribe
          // To receive messages, must have subscribed to at least one topic
      Serial.print(F("Subscribing to topics...."));
      snprintf(topic, 32, "ToDevice/%s", deviceID);
      MQTTclient.subscribe(topic);
      // Subscribe to topics intended for ALL devices on network
      MQTTclient.subscribe("ToDevice/All");
        } else {
          Serial.print("failed, rc=");
          Serial.print(MQTTclient.state());
          Serial.println(" try again in 5 seconds");
          // Wait 5 seconds before retrying
          delay(5000);
        }
      }
     }
    MQTTclient.loop();
  #endif
}
