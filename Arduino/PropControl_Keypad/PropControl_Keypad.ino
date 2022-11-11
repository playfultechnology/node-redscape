/**
 * Node-Redscape Prop Control - Keypad
 * 
 * Example code for a keypad entry escape room puzzle integrated with Node-RED GM control.
 *
 * This code is designed to be run on an Arduino-type device (Uno/Nano/ESP32/ESP8266 etc.) with 
 * a keypad in an escape room. The controller is connected, via a serial, wired ethernet, or Wi-Fi
 * connection to a Node-RED server, using JSON-formatted messages delivered over Serial/MQTT/HTTP. 
 * 
 * A gamesmaster can use the Node-RED dashboard to monitor all inputs the players make on the keypad, 
 * reset, and override the puzzle state remotely.
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
// Define the method by which the controller is connected to Node-RED. Valid values are: 
// USE_ETHERNET (e.g. an Arduino UNO with a W5100/5500 ethernet shield)
// USE_WIFI (e.g. an ESP8266 or ESP32 with built-in Wi-Fi)
// USE_SERIAL (e.g. an Arduino Nano via the USB cable)
#define USE_WIFI
// Define the transport medium
// USE_MQTT
// USE_HTTP
#define USE_MQTT
 
// INCLUDES
#include <Keypad.h>
// Include appropriate networking library depending on target board architecture
#if defined(USE_ETHERNET)
	#include <Ethernet.h>
// Wi-Fi connection
#elif defined(USE_WIFI)
  // ESP8266
  #if defined(ESP8266)
    #include <ESP8266WiFi.h>
  // ESP32 or Arduino
  #elif defined(ESP32) || defined(__AVR__)
    #include <WiFi.h>
  #endif
#endif
#if defined(USE_MQTT)
	// MQTT client, see https://github.com/knolleary/pubsubclient
	#include <PubSubClient.h>
#endif
// JSON serialisation, see https://arduinojson.org/
#include <ArduinoJson.h>

// CONSTANTS
#ifdef USE_WIFI
  // SSID of the network to join
  const char* ssid = "";
  // Wi-Fi password if required
  const char* password = "";
#elif defined(USE_ETHERNET)
  // Enter a MAC address for your controller below.
  // Newer Ethernet shields have a MAC address printed on a sticker on the shield
  const byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
  // Set the static IP address to use. Or, set (0, 0, 0, 0) to use DHCP, but
  // be aware that increases code size.
  IPAddress ip(192, 168, 1, 31);
#endif
#if defined(USE_WIFI) || defined(USE_ETHERNET)
  // The IP address of the MQTT server
  const IPAddress server(192,168,0,18);
  // The port on which the MQTT server is running
  const int port = 1883;
	// Unique name of this device, used as client ID to connect to MQTT server
	// and also topic name for messages published to this device
	const char* deviceID = "Keypad";
#endif
// Define the characters in the matrix
const byte ROWS = 4;
const byte COLS = 3;
const char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[ROWS] = {D4, D3, D2, D1}; // Row pinouts of the keypad
byte colPins[COLS] = {D5, D6, D7}; // Column pinouts of the keypad
char solutionCode[5] = "1234";

// GLOBALS
// Create a new keypad based on the paremeters defined above
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS );
// Record the code which the user has entered
char inputCode[] = "    ";
// What position through the code is being entered?
byte sequenceNum = 0;
// Track state of overall puzzle
enum State {Initialising, Running, Solved};
State state = Initialising;
#ifdef USE_ETHERNET 
  EthernetClient networkClient;
  #if defined(USE_HTTP)
    // Create a local webserver to receive HTTP requests from Node-RED
    EthernetServer networkServer(80);
  #endif    
#elif defined(USE_WIFI)  
  WiFiClient networkClient;
  #if defined(USE_HTTP)
    // Create a local webserver to receive HTTP requests from Node-RED
    WiFiServer networkServer(80);
  #endif    
#endif
#if defined(USE_MQTT)
  // Create a propcontrol object based on the chosen network
  PubSubClient MQTTclient(server, port, networkClient);
  // A re-usable buffer to hold messages to be sent/have been received
  char msg[128];
  // The topic in which to publish a message
  char topic[32];
#endif

void setup(){
  // Initialise serial connection
  Serial.begin(115200);
	Serial.println(__FILE__ __DATE__);

  // Initialise network layer
  #ifdef USE_WIFI
    // Start the WiFi connection
    Serial.print(F("Connecting to WiFi..."));
    #if defined(ESP8266)
      WiFi.mode(WIFI_STA);
    #endif
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println(F("done."));
    Serial.print(F("IP Address: "));
    Serial.println(WiFi.localIP());      
  #elif defined(USE_ETHERNET)
    // Use specified IP address
		if(ip != INADDR_NONE) { Ethernet.begin(mac, ip); }
    // Use DHCP if no IP address defined
		else { Ethernet.begin(mac); }
    Serial.print(F("LAN IP: "));
    Serial.println(Ethernet.localIP());
  #endif

  // Set the puzzle state
  state = State::Running;

  // Set up communication layer
  #if defined(USE_HTTP)
    // Start the local web server on port 80
    networkServer.begin();
  #endif
	#if defined(USE_MQTT)
    MQTTclient.setCallback([](char* topic, byte* payload, unsigned int length) {
      // Note best practice is NOT to have a reusable JSON document, but create a new one each time it is needed
      // Calculate how big the JSON object needs to be to accommodate all the data
      // https://arduinojson.org/v6/assistant/
      const int JsonSize = JSON_OBJECT_SIZE(2) + JSON_ARRAY_SIZE(4);
      StaticJsonDocument<JsonSize> jsonDoc;      
      deserializeJson(jsonDoc, payload, length);
      receiveUpdate(jsonDoc);
    });
	#endif
}

void sendUpdate() {
  // Note best practice is NOT to have a reusable JSON document, but create a new one each time it is needed
  // Calculate how big the JSON object needs to be to accommodate all the data
  // https://arduinojson.org/v6/assistant/
  const int JsonSize = JSON_OBJECT_SIZE(2) + JSON_ARRAY_SIZE(4);
  StaticJsonDocument<JsonSize> jsonDoc;
  jsonDoc["id"] = "Keypad";
  jsonDoc["state"] = (state == State::Solved) ? "SOLVED" : "UNSOLVED" ;
  jsonDoc["input"] = inputCode;
	#if defined(USE_SERIAL)
		serializeJson(jsonDoc, Serial);
		Serial.println("");
	#elif (defined(USE_WIFI) || defined(USE_ETHERNET))
		#if defined(USE_MQTT)
			// Set the topic
			snprintf(topic, 32, "FromDevice/%s", deviceID);
      serializeJson(jsonDoc, msg);
			// Publish to MQTT
			MQTTclient.publish(topic, msg);
		#endif
		#if defined(USE_HTTP)
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
		#endif
  #endif
}

// Note function signature from https://github.com/bblanchon/ArduinoJson/issues/989
void receiveUpdate(const JsonDocument& jsonDoc) {
  // TESTING! Send the received JSON to serial
  serializeJson(jsonDoc, Serial);
  Serial.println("");
}


void loop(){
  char key = keypad.getKey();
  if (key){
		// Set the current position of the code sequence to the key pressed
    inputCode[sequenceNum] = key;
    // Increment the counter
    sequenceNum++;

    if(strcmp(inputCode, solutionCode) == 0){
      state = State::Solved;
    }
    else{
      state = State::Running;
    }
    sendUpdate();

    if(sequenceNum == 4) { sequenceNum = 0; // Clear the data array
      memset(inputCode, 0, sizeof(inputCode)); }
  }

  #if defined(USE_HTTP)
    // Check if there is an incoming client  
    #if defined(USE_ETHERNET)
      EthernetClient client = networkServer.available();
    #elif defined(USE_WIFI)
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
            receiveUpdate();
          }
        }
      }
    }
  #endif

	#if defined(USE_MQTT)
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
