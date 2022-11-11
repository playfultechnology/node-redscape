/**
 * "Toggle Switch State" puzzle 
 * 
 * This puzzle requires players to set a series of toggle switches to the correct on/off state. 
 * The solution can be set by pressing a "save" button, which stores the current state
 * of the switches to EEPROM, restored every time the puzzle is started.
 */

// INCLUDES
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
const char* deviceID = "ToggleSwitch";
// The number of switches. Can be as many as you have digital pins available
const byte numSwitches = 5;
// The pins to which switches are connected
//const byte switchPins[numSwitches] = {5,4,14,12,13};
const byte switchPins[numSwitches] = {D3,D4,D5,D6,D7};
// The desired solution state
bool solution[numSwitches] = {true, true, true, true, true};

// GLOBALS
// An array to record the last known state of every switch
bool lastSwitchState[numSwitches];
// The current state of the puzzle
enum State {Initialising, Active, Solved};
State state = Initialising;
WiFiClient networkClient;
PubSubClient MQTTclient(server, 1883, networkClient);
// A buffer to hold messages to be sent/have been received
char msg[128];
// The topic in which to publish a message
char topic[32];

// Callback function each time a message is published in any of
// the topics to which this client is subscribed
void MQTTcallback(char* topic, byte* payload, unsigned int length) {
  // The message "payload" passed to this function is a byte*
  // Let's first copy its contents to the msg char[] array
  memcpy(msg, payload, length);
  // Add a NULL terminator to the message to make it a correct string
  msg[length] = '\0';

  // Debug
  Serial.print("Message received in topic [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(msg);

  // Act upon the message received
  if(strcmp(msg, "SOLVE") == 0) {
    onSolve();
  }
  else if(strcmp(msg, "RESET") == 0) {
    onReset();
  }
}




void readState() {
  bool changed = false;
  bool solved = true;
  for(int i=0; i<numSwitches; i++){
    int switchValue = digitalRead(switchPins[i]);
    // Compare to previous state
    if(switchValue != lastSwitchState[i]){
      // Update stored value  
      lastSwitchState[i] = (bool)switchValue;
      // Set flag
      changed = true;
    }
    // Compare to correct state
    if(switchValue != solution[i]) { 
      // ... then the puzzle has not been solved
      solved = false;
    }
  }
  state = (solved ? State::Solved : State::Active);
  if(changed) { sendUpdate(); delay(100); }
}

void sendUpdate() {
  // Calculate how big the JSON object needs to be to accommodate all the data
  // https://arduinojson.org/v6/assistant/
  const int JsonSize = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(numSwitches);
  // Create a JSON document of the correct size
  StaticJsonDocument<JsonSize> jsonDoc;
  jsonDoc["id"] = "ToggleSwitch)";
  jsonDoc["state"] = (state == State::Solved) ? "SOLVED" : "UNSOLVED" ;
  JsonArray inputs = jsonDoc.createNestedArray("inputs");
  for(int i=0; i<numSwitches; i++){
    inputs.add(lastSwitchState[i]);
  }
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


void setup() {
  
  // Start the serial connection
  Serial.begin(9600);
  Serial.println("hello");
  
  // Initialise the input pins
  for(int i=0; i<numSwitches; i++){
    pinMode(switchPins[i], INPUT_PULLUP);
  }

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
  state = Active;

  MQTTclient.setCallback([](char* topic, byte* payload, unsigned int length) {

    memcpy(msg, payload, length);
    msg[length] = '\0';

  // Debug
  Serial.print("Message received in topic [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(msg);

  // Act upon the message received
  if(strcmp(msg, "SOLVE") == 0) {
    onSolve();
  }
  else if(strcmp(msg, "RESET") == 0) {
    onReset();
  }



  });


  readState();
  sendUpdate();
  
}

void loop() {
  // Get the current state of the switches
  readState();

while (!MQTTclient.connected()) {
MQTTreconnect();
}

  MQTTclient.loop();
}


void MQTTreconnect() {
    // Debug info
    Serial.print("Attempting to connect to MQTT broker at ");
    Serial.println(server);

    // Attempt to connect
    if (MQTTclient.connect(deviceID)) {
    
      // Debug info
      Serial.println("Connected to MQTT broker");

      // Once connected, publish an announcement to the ToHost/#deviceID# topic
      snprintf(topic, 32, "FromDevice/%s", deviceID);
      snprintf(msg, 64, "{'status' : 'connected'}", deviceID);
      MQTTclient.publish(topic, msg);
      
      // Subscribe to topics meant for this device
      snprintf(topic, 32, "ToDevice/%s", deviceID);
      MQTTclient.subscribe(topic);
      
      // Subscribe to topics meant for all devices
      MQTTclient.subscribe("ToDevice/All");
    }
    else {
      // Debug info why connection could not be made
      Serial.print("Connection to MQTT server failed, rc=");
      Serial.print(MQTTclient.state());
      Serial.println(" trying again in 5 seconds");
      
      // Wait 5 seconds before retrying
      delay(5000);
    }
}


void onSolve(){
  // Print debugging message
  Serial.println(F("Puzzle Solved!"));
  
  // Update the global puzzle state
  state = Solved;
  
  delay(2000);
}

void onUnsolve(){

  // Print debugging message
  Serial.println(F("Puzzle no longer solved!"));
  
  // Update the global puzzle state
  state = Initialising;
  
  // Slight delay
  delay(1000);
}

void onReset(){

  // Print debugging message
  Serial.println(F("Puzzle Reset!"));
  
  // Update the global puzzle state
  state = Initialising;
  
  // Slight delay
  delay(1000);
}
