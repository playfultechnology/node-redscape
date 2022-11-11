/**
 * ClueDelivery_LCD
 *
 * A simple sketch that takes a character string supplied over serial connection
 * and prints it to an LCD display, inserting line breaks to avoid splitting words
 *
 */
 
// INCLUDES
// Wire library required for any modules that use I2C communication
#include <Wire.h>
// For LCD displays. See https://github.com/mathertel/LiquidCrystal_PCF8574
#include <LiquidCrystal_PCF8574.h>
// For processing serial commands. See https://github.com/kroimon/Arduino-SerialCommand
#include "src/SerialCommand/SerialCommand.h"

// GLOBALS
// PCF8574 backpack typically uses I2C address of either 0x27 (for TI chips) or  0x3F (NXP chips)
LiquidCrystal_PCF8574 lcd(0x27);
// SerialCommand object helps process commands received over serial connection
SerialCommand sCmd;

void setup() {
  // Serial connection required for incoming commands as well as debug output
  Serial.begin(115200);
	Serial.println(__FILE__ __DATE__);
 
  // Initialise the I2C interface
  Wire.begin();  
  // Initialise the display
  lcd.begin(20,4);
  lcd.setBacklight(255);
  lcd.clear();

  // Register serial commmands and their associated callback functions
  // CLEAR command is straightforward
  sCmd.addCommand("CLEAR", clearDisplay);

  // PRINT command is a bit more complicated since it has additional argument (the text to be printed)
  sCmd.addCommand("PRINT", [](){
    char *arg;
    // Retrieve the text passed after the PRINT command
    arg = sCmd.next();
		// Something was sent
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

void loop(){
  // Read serial input and compare to list of registered commands
  sCmd.readSerial();
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
