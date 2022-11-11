/**
 * Thermal Printer - Escape Room Clue Delivery System
 * 
 * This code is designed to be run on an Arduino-type device (Uno/Nano/ESP32/ESP8266 etc.) connected to 
 * a thermal printer positioned in an escape room. The controller is connected, via a serial connection,
 * wired ethernet connection, or Wi-Fi connection, to a Node-RED control system. 
 * 
 * A gamesmaster can send pre-defined hints or type custom messages to send to the printer from the
 * Node-RED dashboard, or any arbitrary text or graphics could be printed when some other controller
 * in the room is triggered
 * 
 * Usage:
 * via Serial:
 * PRINT "This is a message sent via serial connection"
 * FEED 3
 * LED ON
 * via MQTT:
 * Topic:ToDevice/100/PRINT Payload:"This is message sent via MQTT"
 * Topic:ToDevice/100/FEED Payload:3
 * Topic:ToDevice/100/LED Payload:ON
 *
 * Tested on:
 * Arduino Nano / Arduino UNO / Arduino Mega / Wemos D1 Mini / Devkit ESP32 v1 (via USB Serial)
 * Arduino UNO (via Ethernet Shield)
 * Wemos D1 Mini / Devkit ESP32 v1 (via Wi-Fi)
 */

// DEFINES
// This value determines the method by which the controller is connected to Node-RED. Valid values are: 
// USE_ETHERNET (e.g. an Arduino UNO with an ethernet shield)
// USE_WIFI (e.g. an ESP8266 or ESP32 with built-in Wi-Fi)
// USE_SERIAL (e.g. an Arduino Nano via the USB cable)
#define USE_SERIAL

// INCLUDES
// Some devices (e.g. ESP32 or Arduino MEGA) have multiple dedicated hardware serial interfaces (UARTs)
// (e.g. they have built-in Serial, Serial1, Serial2 etc.). 
// On an Arduino MEGA, these are on GPIO pins Serial1: 19(RX),18(TX), Serial2: 17(RX),16(TX), Serial3: 15(RX),14(TX)
// On an ESP32, these are Serial1: 9(RX),10(TX) , Serial2: 16(RX),17(TX) (But note that Serial1 pins can't be used as shared with SPI flash memory)
// However, Nano/Uno etc. only have one serial, so we can emulate another one on any pair of GPIO pins instead
// Whether using hardware or software interface, printer should then be connected to Tx/Rx pins as follows:
// Printer Tx -> Arduino Rx = Blue wire
// Arduino Tx -> Printer Rx = Green wire
#if !defined(HAVE_HWSERIAL2) && !defined (ESP32)
  // The SoftwareSerial library allows software emulation of a serial port
  #include <SoftwareSerial.h>
    // Create a Serial interface on the following Rx/Tx pins to communicate between Arduino and printer
    SoftwareSerial Serial2(2,3);  // For Wemos ESP8266 boards, remember to use "D" prefix with pin numbers, e.g. D2/D3
#endif
// Thermal printer library, see: https://github.com/playfultechnology/arduino-thermal-printer
#include "src/ThermalPrinter.h"
// Include the appropriate library for the chosen communication method
#ifdef USE_SERIAL
  // CmdBuffer library receives and parses commands received over serial connection
  #include "src/CmdBuffer/CmdBuffer.hpp"
  #include "src/CmdBuffer/CmdCallback.hpp"
  #include "src/CmdBuffer/CmdParser.hpp"
#else
  // Hard-wired ethernet connection (e.g. Arduino ethernet shield)
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
  // PubSubClient used for MQTT message brokering over either ethernet or Wi-Fi
  // See from https://github.com/knolleary/pubsubclient
  #include <PubSubClient.h>
#endif
// Additional file contains image data
#include "Image.h"

// CONSTANTS
#ifdef USE_WIFI
  // SSID of the network to join
  const char* ssid = "VodafoneConnect53686628";
  // Wi-Fi password if required
  const char* password = "8p2ty6329x2mk6v";
#endif
#if defined(USE_WIFI) || defined(USE_ETHERNET)
  // The IP address of the MQTT server
  const IPAddress server(192,168,1,18);
  // The port on which the MQTT server is running
  const int port = 1883;
  // Unique ID used to identify this device on the MQTT network
  char* deviceID = "100";
#endif

// GLOBALS
// Initialise printer object on specified serial interface (which can be a hardware interface
// or software interface simulated using SoftwareSerial above)
PlayfulTechnology::ThermalPrinter printer(Serial2); 
// Network connection object - WiFi/Ethernet/Serial as appropriate
#ifdef USE_ETHERNET 
  EthernetClient networkClient;
  // Create a propcontrol object based on the chosen network
  PubSubClient pc(server, port, networkClient);
#elif defined(USE_WIFI)  
  WiFiClient networkClient;
  // Create a propcontrol object based on the chosen network
  PubSubClient pc(server, port, networkClient);
#elif defined(USE_SERIAL)
  // Buffer must be large enough to store the number of characters in the text message to print
  CmdBuffer<200> serialCmdBuffer;
  // We create a callback for each command that can be received over the serial connection
  // We'll have two required print actions: PRINT or FEED and also one for toggling the LED for testing  
  CmdCallback<4> serialCmdCallback;
  // The CmdParser object extracts and processes commands from the buffer
  CmdParser serialCmdParser;
#endif

void setup(){
  // Serial connection used to output info to Serial Monitor and, if USE_SERIAL
  // specified, to receive print commands
  Serial.begin(9600);
  // Secondary serial connection used to communicate between the Arduino and the printer
  Serial2.begin(9600);
  // Initialise the printer settings
  printer.init();
  // Initialise the LED pin as an output
  pinMode(LED_BUILTIN, OUTPUT);

  #ifdef USE_SERIAL
    // Add commands to process, and the callback functions we'd like them to fire
    serialCmdCallback.addCmd("PRINT", &printOut);
    serialCmdCallback.addCmd("FEED", &paperFeed);
    serialCmdCallback.addCmd("LED", &led);
    serialCmdCallback.addCmd("PRINTIMAGE", &printImage);
  #else
    #ifdef USE_WIFI
      // Start the WiFi connection
      Serial.print(F("Connecting to WiFi..."));
      #if defined(ESP8266)
        WiFi.mode(WIFI_STA);
      #endif
      WiFi.begin(ssid, password);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        #ifdef DEBUG
          Serial.print(".");
        #endif
      }
      Serial.println(F("done."));
      Serial.print(F("IP Address: "));
      Serial.println(WiFi.localIP());
    #elif defined(USE_ETHERNET)
      // Enter a MAC address for your controller below.
      // Newer Ethernet shields have a MAC address printed on a sticker on the shield
      const byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
      // Set the static IP address to use if the DHCP fails to assign
      IPAddress ip(192, 168, 1, 31);
      // Note that you can leave IP blank to have it assigned by DHCP, but that makes the Arduino sketch occupy more memory
      Ethernet.begin(mac, ip);
      Serial.print(F("LAN IP: "));
      Serial.println(Ethernet.localIP());
    #endif
    // Specify the callback that processes any messages received on an MQTT topic to which we are subscribed
    pc.setCallback([](char* topic, byte* payload, unsigned int length) {
      // Copy the payload received to a character string
      char msg[200];
      memcpy(msg, payload, length);
      msg[length] = '\0';
      Serial.print(F("Received over MQTT:"));
      Serial.println(msg);
      // Perform the appropriate action depending on the topic in which the message was published
      if(strstr(topic, "PRINT") != NULL) {
        printer.println(msg); 
        Serial.print(F("Received MQTT PRINT "));
        Serial.println(msg);
      }
      else if(strstr(topic, "FEED")  != NULL) {
        int numLineFeeds = atoi(msg);
        if(numLineFeeds > 0) { printer.feed(numLineFeeds); }
        else { printer.feed(); }
        Serial.print(F("Received MQTT FEED "));
        Serial.println(msg);
      }
      else if(strstr(topic, "LED") != NULL) {
        if(strcmp(msg, "ON") == 0) {
          Serial.print(F("Turning LED on"));
          digitalWrite(LED_BUILTIN, HIGH);
        }
        else if(strcmp(msg, "OFF") == 0) {
          Serial.print(F("Turning LED off"));
          digitalWrite(LED_BUILTIN, LOW);
        }
      }
    });
  #endif

  // Set font size [S,M,L]
  printer.setSize('S');

  Serial.println(F("Setup Complete"));
}

void loop(){
  #ifdef USE_SERIAL
    // Check for new char on serial and call function if command was entered
    serialCmdCallback.updateCmdProcessing(&serialCmdParser, &serialCmdBuffer, &Serial);
  #else
    // Ensure there's a connection to the MQTT server
    while(!pc.connected()) {
      if(pc.connect(deviceID)) {
        // Subscribe to topics meant for this device
        char topic[32];
        snprintf(topic, 32, "ToDevice/%s/#", deviceID);
        pc.subscribe(topic);
        Serial.println(F("Connected to MQTT server"));
      } else {
        Serial.print(F("Could not connect to MQTT server, rc="));
        Serial.println(pc.state());
        // Wait 5 seconds before retrying
        delay(5000);
      }
    }
    // Call the main MQTT loop to check for and publish messages
    pc.loop();
  #endif

  // Delay a moment
  delay(10);
}

#ifdef USE_SERIAL
// The following callbacks are used only for instructions received over serial connection
void led(CmdParser *serialCmdParser) {
  if(serialCmdParser->equalCmdParam(1, "ON")) { digitalWrite(LED_BUILTIN, HIGH); }
  else { digitalWrite(LED_BUILTIN, LOW); }
}
void printOut(CmdParser *serialCmdParser) { printer.println(serialCmdParser->getCmdParam(1)); }
void paperFeed(CmdParser *serialCmdParser) { printer.feed(atoi(serialCmdParser->getCmdParam(1))); }
void printImage(CmdParser *serialCmdParser) {   
  // Print a bitmap image (defined in image.h):
  printer.printBitmap(image_width, image_height, image_data);
}
#endif
