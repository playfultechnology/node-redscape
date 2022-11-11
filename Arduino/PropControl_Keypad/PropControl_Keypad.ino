
// INCLUDES
#include <Keypad.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
// Download from https://github.com/knolleary/pubsubclient
#include <PubSubClient.h>

// CONSTANTS
// SSID of the network to join
const char* ssid = "VodafoneConnect53686628";
// Wi-Fi password if required
const char* password = "8p2ty6329x2mk6v";
// The IP address of the MQTT server
const IPAddress server(192,168,1,7);
// Unique name of this device, used as client ID to connect to MQTT server
// and also topic name for messages published to this device
const char* deviceID = "Keypad";
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
WiFiClient networkClient;
PubSubClient MQTTclient(server, 1883, networkClient);
// A buffer to hold messages to be sent/have been received
char msg[128];
// The topic in which to publish a message
char topic[32];

void setup(){
  Serial.begin(9600);
  state = State::Running;

 // Attempt to connect to the specified Wi-Fi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  // Show a simple progress counter while we wait to connect
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

// Give the Wi-FI a couple of seconds to initialize:
  delay(2000);

  // Print debug info about the connection 
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  // Set the puzzle state
  state = State::Running;

  MQTTclient.setCallback([](char* topic, byte* payload, unsigned int length) {

    // For debugging, print the command received to the serial connection
    char msg[64];
    memcpy(msg, payload, length);
    msg[length] = '\0';
    Serial.print("Received:");
    Serial.println(msg);
  });
  
}

void sendUpdate() {
  // Calculate how big the JSON object needs to be to accommodate all the data
  // https://arduinojson.org/v6/assistant/
  const int JsonSize = JSON_OBJECT_SIZE(2) + JSON_ARRAY_SIZE(4);
  // Create a JSON document of the correct size
  StaticJsonDocument<JsonSize> jsonDoc;
  jsonDoc["id"] = "Keypad";
  jsonDoc["state"] = (state == State::Solved) ? "SOLVED" : "UNSOLVED" ;
  jsonDoc["input"] = inputCode;

  // Send to Serial
  serializeJson(jsonDoc, Serial);
  Serial.println("");
  // Or send via MQTT
    snprintf(topic, 32, "FromDevice/%s", deviceID);
  // Declare a buffer to hold the result
  serializeJson(jsonDoc, msg);
  MQTTclient.publish(topic, msg);
  // Or send to HTTP POST (https://randomnerdtutorials.com/decoding-and-encoding-json-with-arduino-or-esp8266/)
}


void reconnect() {
  // Loop until we're reconnected
  while (!MQTTclient.connected()) {
    if (MQTTclient.connect(deviceID)) {
      // ... and resubscribe
      // To receive messages, must have subscribed to at least one topic
  Serial.print(F("Subscribing to topics...."));
  // Subscribe to topics meant for this device
  char topic[32];
  snprintf(topic, 32, "ToDevice/%s", deviceID);
  MQTTclient.subscribe(topic);
  // Subscribe to topics intended for clue delivery systems
  MQTTclient.subscribe("ToDevice/Hint");
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
if (!MQTTclient.connected()) {
      reconnect();
    }
    MQTTclient.loop();
  
}
