/**
 * A remote-controlled clock whose time can be set by an HTTP GET Request made by any web browser.
 *
 * Uses a two-shaft stepper motor (e.g. BKA30D-R5) attached to 
 * the hour and minute hands of an analogue clock, controlled by an
 * ESP8266-powered WeMos D1 Mini microcontroller. Can easily be adapted to any
 * network-enabled Arduino-esque device.
 * 
 * USAGE:
 * Time can be set by making HTTP request in the format
 * http://192.168.1.4?h=10&m=45 (where 192.168.1.4 is the IP address assigned to this device)
 */
// DEFINES
// The total number of steps required for one complete revolution of the stepper shaft
// This should be stated on the motor datasheet, or can be derived empirically
#define NUM_STEPS 720
// There are 12 hours on the clock, so each hour requires NUM_STEPS / 12 = 60 steps
#define STEPS_PER_HOUR 60
// There are 60 minutes, so each minute requires NUM_STEPS / 60 = 12 steps
#define STEPS_PER_MINUTE 12
// This buffer will store the incoming HTTP request.
// If you were to visit the following URL in your browser (passing two parameters):
// http://192.168.1.18/?h=10&m=45
// The request as received by the Arduino would look like:
// GET /?h=10&m=45 HTTP/1.1  (25 chars)
// Keep the buffer size small to reduce memory consumption, but increase if necessary
// to accommodate whatever parameters you pass in the URL.
#define BUFFER_SIZE 32

// LIBRARIES
// AccelStepper library for stepper motor control is available from
// https://www.airspayce.com/mikem/arduino/AccelStepper/
#include "src/AccelStepper/AccelStepper.h"
// Inlcude code for WiFi client and server
#include <ESP8266WiFi.h>

// GLOBALS
// I'm using BKA30D-R5 dual stepper module, designed for vehicle instrument panels
// It has two bipolar steppers, which are wired to pins as follows:
// "O" is the location of the spindle
//    ______________
//   / D1 D2    D5 D6 \
//  |    O            |
//   \_D3 D4____D7 D8_/
//
// Based on wiring above, declare a stepper object for each hand
AccelStepper hourStepper(AccelStepper::FULL4WIRE, D5, D6, D7, D8);
AccelStepper minuteStepper(AccelStepper::FULL4WIRE, D1, D2, D3, D4);
// Authentication details for your WiFi network
// SSID of the network to join
const char* ssid = "VodafoneConnect53686628";
// Wi-Fi password if required
const char* password = "8p2ty6329x2mk6v";
// The webserver object
WiFiServer server(80);

void setup() { 
  // Serial is the H/W serial used to interface Arduino to PC
  Serial.begin(9600);
 
  // Attempt to connect to the specified Wi-Fi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  // Show a simple progress counter while we wait to connect
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Confirm SSID of connected network
  Serial.print(F("Connected to: "));
  Serial.println(WiFi.SSID());
  // Print the assigned IP address
  Serial.print(F("IP Address: "));
  Serial.println(WiFi.localIP());
  
  // Start the web server on port 80
  server.begin();
  
  // Configure each stepper
  hourStepper.setMaxSpeed(500);
  minuteStepper.setMaxSpeed(500);
  hourStepper.setAcceleration(200);
  minuteStepper.setAcceleration(200);
}

/**
 * Sets the target position of each hand based on supplied hours and minutes values
 * Note that this only sets the desired target - the actual movement is handled
 * in the main loop()
 */
void SetTime(long hours, long minutes){
  Serial.print("Setting time to ");
  Serial.print(hours);
  Serial.print(":");
  Serial.println(minutes);
  hourStepper.moveTo((hours + minutes/60) * STEPS_PER_HOUR);
  minuteStepper.moveTo(minutes * STEPS_PER_MINUTE);
}

/**
 * Checks and acts upon any input received over Wi-Fi HTTP connection
 */
void CheckHTTPInput() {
    // Listen if there is an incoming client
  WiFiClient client = server.available();
  // If there is a client...
  if(client) {
    // Set up some variables which will be used to parse the incoming request
    // HTTP requests end with a blank line. i.e. a double carriage return then a line feed (\r\n\r\n),
    // so to tell when the request ends we need to keep track whether the current line is empty
    bool currentLineIsBlank = true;
    // Counter to loop over the request
    int i = 0;
    // A char buffer to hold the contents of the HTTP request
    char requestBuffer[BUFFER_SIZE];
    // A pointer to each argument extracted from the URL. e.g. http://192.168.0.1?arg1=value1&arg2=value2
    char *pch;
    // Initialise the hours and minutes with an invalid value
    long hours, minutes = -1;
    // As long as the client is still connected
    while (client.connected()) {
      // And there are bytes to read from the request
      if (client.available()) {
        // Read in the next byte
        char c = client.read();
        // Fill the buffer
        if(i < BUFFER_SIZE-1) {
          requestBuffer[i] = c;
          i++;
          // terminate the buffer
          requestBuffer[i] = 0;         
        }
        // If the current line is blank and we see a newline character, we've reached the end of the request
        if (c == '\n' && currentLineIsBlank) {
          // Tokenise the contents of the request buffer
          // to extract the first parameter (which follows the ? character in the URL)
          pch = strtok(requestBuffer,"?");
          // If we've been able to find a parameter
          while(pch != NULL) {
            // Does this token assign a value to the hours variable, h? (i.e. it begins "h=")
            if(strncmp(pch,"h=",2) == 0) {
              // Try to parse the remainder of this token as an integer and assign to hours variable
              hours = strtol(pch+2, NULL, 10);
            }
            // Does this token assign a value to the minutes variable, m? (i.e. it begins "m=")
            else if(strncmp(pch,"m=",2) == 0) {
              // Try to parse the remainder of this token as an integer and assign to minutes variable
              minutes = strtol(pch+2, NULL, 10);
            }
            // Does this token pass the "r" (reset) parameter?
            else if(strncmp(pch,"r=",2) == 0) {
              // Try to parse the remainder of this token as an integer and assign to minutes variable
              int r = atoi(pch+2);
              if(r == 1) {
                hourStepper.setCurrentPosition(0);
                minuteStepper.setCurrentPosition(0);
              }
            }
            // Continue to tokenise the remainder of the request buffer
            pch = strtok(NULL,"& ");
          }
          // All arguments have been processed, so send a response to the client
          // and end the connection
          sendHttpResponse(client);
          client.stop();

          // If both hours and minutes have been set to valid values
          if(hours != -1 && minutes != -1) {
            SetTime(hours,minutes);
          }
        }
        // If a newline character is received, this line of the request is blank
        if (c == '\n') {
          currentLineIsBlank = true;
        }
        // Anything other than a carriage return means this line is not blank
        else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }
    }
  }
}

/**
 * Serves a simple HTML webpage back to the client with instructions how to
 * set time from a web browser
 */
void sendHttpResponse(WiFiClient client) {
  // HTTP header specification requires a response code (e.g. HTTP/1.1 200 OK)
  // and then a content-type declaration, followed by a blank line:
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-type:text/html"));
  client.println();
  // Specify any content to be returned to the client
  client.println(F("Set a time by calling this page with h and m parameters, e.g."));
  client.print(WiFi.localIP());
  client.println(F("?h=12&m=45"));
  // The HTTP response ends with another blank line:
  client.println();
}

/**
 * Main Program Loop
 */
void loop() {
  // Check for input via Wi-Fi HTTP request in format http://192.168.1.4?h=10&m=45
  CheckHTTPInput();
 
  // We call the run() function on both steppers every frame, which processes any movement
  // required to reach their target position
  hourStepper.run();
  minuteStepper.run();
}
