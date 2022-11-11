// INCLUDES
// Wire library required for any modules that use I2C communication
#include <Wire.h>
// Library for LCD displays
#include <LiquidCrystal_PCF8574.h>
#include "src/SerialCommand/SerialCommand.h"

// CONSTANTS
char str[] ="Hello World! Here is some text. I need them to split into multiple lines...";

// GLOBALS
// PCF8574 backpack typically uses I2C address of 0x27
LiquidCrystal_PCF8574 lcd(0x27);
// SerialCommand object helps process commands received over serial connection
SerialCommand sCmd;

void setup() {
  // Serial connection required for incoming commands as well as debug output
  Serial.begin(9600);
 
  // Initialise the I2C interface
  Wire.begin();  
  // Initialise the display
  lcd.begin(20,4);
  lcd.setBacklight(2128);
  lcd.clear();

  // Register commmands and their associated callback functions

  // CLEAR command is straightforward
  sCmd.addCommand("CLEAR", clearDisplay);

  // PRINT command is a bit more complicated since it has additional argument (the text to be printed)
  sCmd.addCommand("PRINT", [](){
    char *arg;
    // Retrieve the text passed after the PRINT command
    arg = sCmd.next();
    if(arg != NULL) { 
      printMessage(arg);
      Serial.print("Received PRINT command: ");
      Serial.println(arg);
    }
    else {
      Serial.println("Nothing to print");
    }
  }
  );
}

/*
 * Splits a message and displays it on separate lines 
 * of a 4x20 LCD display, avoiding breaking mid-word  
 */
void printMessage(char *msg) {
  int i=0;
  // Loop until we run out of text
  while(*msg) {
    lcd.setCursor(0, i++);
    // Find the position of the space nearest the end of this line
    const char* endOfLine = split(msg, 20); 
    // Display message up to that point
    while(msg != endOfLine) {
      lcd.print(*msg++);
    }
    // If the line break was a word break anyway, move on - don't start new line with a space
    if(*msg == ' ') {
      msg++;
    }
  }
}

void clearDisplay() {
  lcd.clear();
}

void loop(){
  // Read serial input and compare to list of registered commands
  sCmd.readSerial();
}

// Helper function to split string avoiding word breaks
const char* split (const char* s, const int length) {
  // If the supplied string is shorter than the max allowed length, return the whole thing
  if (strlen(s) <= length)
    return s + strlen(s);
  // Search backwards to find the last space
  for (const char* space=&s[length]; space!=s; space--) {
    if (*space == ' ') {
      return space;
    }
  }
  // If no space character found, return the whole line
  return &s[length];       
} 
