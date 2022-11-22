/**
 * Node-Redscape Prop Control - Arduino Serial Keypad
 * 
 * Example code for a keypad entry escape room puzzle integrated with Node-RED GM control.
 *
 */

// INCLUDES
// Keypad input (tested with v3.1.1), see https://github.com/Chris--A/Keypad
#include <Keypad.h>
// JSON serialization (tested with v6.19.4), see https://arduinojson.org
#include <ArduinoJson.h>
// 4-Digit LED display (tested with v0.3.4), see https://github.com/RobTillaart/TM1637_RT
#include <TM1637.h>
// Programmable LED display (tested with v3.5), see https://github.com/FastLED/FastLED
#include <FastLED.h>
// Software UART for communication with Node-RED
#include <SoftwareSerial.h>

// CONSTANTS
// Unique name used to identify this device on the network
const char* deviceID = "Keypad-1";
// Define the characters in the matrix
const char keys[4][3] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
// Be careful of pinout order! Some 3x4 keypads are mental. https://learn.adafruit.com/matrix-keypad/pinouts
byte rowPins[4] = {7, 2, 3, 5}; // Row pinouts of the keypad
byte colPins[3] = {6, 8, 4}; // Column pinouts of the keypad
char solutionCode[5] = "1234";
// Clock pin for the display
const byte displayClockPin = A5;
// Data pin for the display
const byte displayDataPin = A4;
const byte displayNumDigits = 4;
// Connected to the DIN of the programmable LED
const byte ledPin = A0;

// GLOBALS
// Create a new keypad based on the paremeters defined above
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, 4, 3 );
// Record the code which the user has entered
char inputCode[] = "    ";
// What position through the code is being entered?
byte sequenceNum = 0;
// Create a display object, specifying pin parameters (Clock pin, Data pin)
TM1637 display;
CRGB leds[1];
// Track state of overall puzzle
enum DeviceState {Initialising, Running, Solved};
DeviceState deviceState = DeviceState::Initialising;

void setup(){
  // Initialise serial connection
  Serial.begin(115200);
  Serial.println("");
	Serial.println(__FILE__ __DATE__);

  // Set brightness
  display.begin(displayClockPin, displayDataPin, displayNumDigits);

  // Tell FastLED about the LED strip configuration
  FastLED.addLeds<PL9823, ledPin>(leds, 1).setCorrection(TypicalLEDStrip);
  leds[0] = CRGB(255,0,0);
  FastLED.show();

  // Set the puzzle state
  deviceState = DeviceState::Running;	
}

// Set the LED colour/pattern to indicate the status of the device
void ledLoop() {
  uint8_t hue = 0; // colour
  uint8_t v = 128; // brightness
  if(deviceState == DeviceState::Initialising) {
    hue = 0;
    v = beatsin8(10,0, 128);
  }
  else if(deviceState == DeviceState::Solved) {
    hue = 96; 
    v =128;
  }
  else {
    hue = 160;
    v = beatsin8(10,0, 128);
  }
  leds[0] = CHSV(hue, 255, v); 
  FastLED.show();
}

// Updates the display to show the current code entered by the user
void updateDisplay(){
  // Initialise a new blank display array
  uint8_t displayData[] = { 0x10, 0x10, 0x10, 0x10 };
  // Loop over each character in the code
  for(int i=0; i<(sizeof(inputCode)/sizeof(inputCode[0])); i++){
    // Digits can be displayed directly using the encodeDigit() function
    if(inputCode[i] >= '0' && inputCode[i] <= '9') {
      displayData[displayNumDigits-i-1] = (int)inputCode[i] - 48;
    }
    // For the * character, display a single line in the middle of the digit display
    else if(inputCode[i] == '*'){
      displayData[displayNumDigits-i-1] = 0b01000000;
    }
    // For the # character, display parallel lines at the top and bottom of the digit
    else if(inputCode[i] == '#'){
      displayData[displayNumDigits-i-1] = 0b00001001;
    }
  }
  // Pass the data array to the display
  display.displayRaw(displayData, -1);
}

// Flash the current value of the display on and off
void flashDisplay(){
  uint8_t displayData[] = { 0x10, 0x10, 0x10, 0x10 };
  // Toggle between the current value and the empty value
  for(int i=0; i<4; i++){
    delay(250);
    updateDisplay();    
    delay(250);
    display.displayRaw(displayData, -1);
  }
}

void receiveUpdate(const JsonDocument& jsonDoc) {
  if(jsonDoc["command"] == "SOLVE") { deviceState = DeviceState::Solved; }
  else if(jsonDoc["command"] == "RESET") { 
    deviceState = DeviceState::Running; 
    memset(inputCode, 0, sizeof(inputCode));
    sequenceNum = 0;
    // Update the display
    updateDisplay();    
  }
  // Now send refreshed values back
  sendUpdate();
}

void sendUpdate() {
  // Create JSON document - determine size using https://arduinojson.org/v6/assistant/
  StaticJsonDocument<128> jsonDoc;
  jsonDoc["id"] = deviceID;
  jsonDoc["state"] = (deviceState == DeviceState::Solved) ? "SOLVED" : "UNSOLVED";
  jsonDoc["input"] = inputCode;
  serializeJson(jsonDoc, Serial);
  Serial.println("");
}

void networkLoop() {
  if(Serial.available()) {
    // This one must be bigger than the sender's because it must store the strings
    StaticJsonDocument<512> jsonDoc;
    // Read the JSON document from the "link" serial port
    DeserializationError err = deserializeJson(jsonDoc, Serial);
    if (err == DeserializationError::Ok) {
      receiveUpdate(jsonDoc);
    }
    else {
      // Flush all bytes in the "link" serial port buffer
      while (Serial.available() > 0) { Serial.read(); }
    }
  }
}

void inputLoop() {
  char key = keypad.getKey();
  if (key){
    // Set the current position of the code sequence to the key pressed
    inputCode[sequenceNum] = key;
    // Increment the counter
    sequenceNum++;
    // Update the display
    updateDisplay();    
    // If the player has entered the correct 4 digits of a code
    if(strcmp(inputCode, solutionCode) == 0) {
      deviceState = DeviceState::Solved;
    }
    else {
      deviceState = DeviceState::Running;
    }
    // Update Node-RED
    sendUpdate();
    
    // If the player has entered incorrect 4 digit code
    if(sequenceNum == 4) {
      if(strcmp(inputCode, solutionCode) != 0) {
        // Flash the display
        flashDisplay();
      }
      // Reset
      memset(inputCode, 0, sizeof(inputCode));
      sequenceNum = 0;  
    }
  }
}

void loop(){
  inputLoop();
  networkLoop();
  ledLoop();
}
