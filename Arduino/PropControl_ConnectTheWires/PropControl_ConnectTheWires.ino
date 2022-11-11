/**
* Connect the Wires Puzzle
* 
* This puzzle requires the player to use a number of wires to
* join correct pairs of terminals together.
* Once all terminals have been correctly connected, the puzzle is solved.  
* 
* Demonstrates:
* - Use of inbuilt pull-up resistors
* - pinMode, digitalWrite, digitalRead
*/

// INCLUDES
#include <ArduinoJson.h>

// CONSTANTS
// The total number of possible sockets from which connections can be made
const byte numSockets = 8;
// The array of pins to which each socket is connected
const byte signalPins[numSockets] = { 2, 3, 4, 5, 6, 7, 8, 9 };
// The total number of connections required to be made
const byte numConnections = 4;
// The connections that need to be made between pins, referenced by their index in the signalPins array
const byte connections[numConnections][2] = { {0,1}, {2,5}, {3,4}, {6,7} };

// GLOBAL VARIABLES
// Track state of which output pins are correctly connected
bool lastState[numConnections] = { false, false, false };
// Track state of overall puzzle
enum State {Initialising, Running, Solved};
State state = Initialising;

void setup() {

  Serial.begin(9600);
  
    // Initialise the pins. When wires are disconnected, the input pins would be
    // floating and digitalRead would be unpredictable, so we'll initialise them
    // as INPUT_PULLUP and use the wires to connect them to ground.
    for (int i=0; i<numSockets; i++){	
      // The default state of all pins will be INPUT_PULLUP, though they will be reassigned throughout the duration of the program in order to test connection between pins
      pinMode(signalPins[i], INPUT_PULLUP);
    }
    
    state = Running;


    sendUpdate();
    
}

void loop() {
	
  // Assume that all wires are correct
  bool AllWiresCorrect = true;
  
  // Assume that the puzzle state has not changed since last reading
  bool stateChanged = false;
  
	// Check each connection in turn  
	for (int i=0; i<numConnections; i++){

    // Get the pin numbers that should be connected by this wire
    byte pin1 = signalPins[connections[i][0]];
    byte pin2 = signalPins[connections[i][1]];

    // Test whether the appropriate pins are correctly connected
    bool currentState = isConnected(pin1, pin2);
   
    // Has the connection state changed since last reading?
    if (currentState != lastState[i]) {
      
      // Set the flag to show the state of the puzzle has changed
      stateChanged = true;
      
      // Update the stored connection state
      lastState[i] = currentState;                  
    }
        
    // If any connection is incorrect, puzzle is not solved
    if(currentState == false) {
      AllWiresCorrect = false;
    }        
  }

	// If the state of the puzzle has changed
	if (AllWiresCorrect && state == Running){
		onSolve();
	}
  // If the state of the puzzle has changed
  else if (!AllWiresCorrect && state == Solved){
    onUnsolve();
  }

  // If a connection has been made/broken since last time we checked
  if(stateChanged) {
    sendUpdate();
  }

  delay(100);
}

/**
 * Called when the puzzle is solved
 */
void onSolve() {
  state = Solved;
}

/**
 * Called when the (previously solved) puzzle becomes unsolved
 */
void onUnsolve(){
  state = Running;
}

void sendUpdate() {
  // Calculate how big the JSON object needs to be to accommodate all the data
  // https://arduinojson.org/v6/assistant/
  const int JsonSize = 7*JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(numConnections);
  // Create a JSON document of the correct size
  StaticJsonDocument<JsonSize> jsonDoc;
  jsonDoc["id"] = "ConnectWires";
  jsonDoc["state"] = (state == State::Solved) ? "SOLVED" : "UNSOLVED" ;
  JsonArray inputs = jsonDoc.createNestedArray("inputs");
  for(int i=0; i<numConnections; i++){
    JsonObject connection = inputs.createNestedObject();
    connection["from"] = connections[i][0];
    connection["to"] = connections[i][1];
    connection["connected"] = lastState[i];
  }
  // Send to Serial
  serializeJson(jsonDoc, Serial);
  Serial.println("");
  // Or send via MQTT
  // Or send to HTTP POST (https://randomnerdtutorials.com/decoding-and-encoding-json-with-arduino-or-esp8266/)
}


/**
 * Tests whether an output pin is connected to a given INPUT_PULLUP pin
 */
bool isConnected(byte OutputPin, byte InputPin) {

  // To test whether the pins are connected, set the first as output and the second as input
  pinMode(OutputPin, OUTPUT);
  pinMode(InputPin, INPUT_PULLUP);
  
  // Set the output pin LOW
	digitalWrite (OutputPin, LOW);
  delay(10);
  // If connected, the LOW signal should be detected on the input pin
  // (Remember, we're using LOW not HIGH, because an INPUT_PULLUP will read HIGH by default)
	bool isConnected = !digitalRead(InputPin);

  // Set the output pin back to its default state
  pinMode(OutputPin, INPUT_PULLUP);
    
  return isConnected;
}
