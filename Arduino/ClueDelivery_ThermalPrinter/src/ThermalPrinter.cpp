/**
 * Arduino library implementing the ESC/POS interface for serial
 * control of thermal receipt printer. For list of commands, see
 * https://reference.epson-biz.com/modules/ref_escpos/index.php?content_id=52
 */

#include <Arduino.h>
#include "ThermalPrinter.h"

namespace PlayfulTechnology {
	ThermalPrinter::ThermalPrinter(Stream &s) {
		serial = &s;
	}

	void ThermalPrinter::init() {
		// Define zero terminating character
		int zero = 0;
		// Define thermal settings
		heatTime = 80; // Controls speed of printing and darkness
		heatInterval = 2; // Controls speed of printing and darkness
		printDensity = 15; // Testing shows the max helps darken text
		printBreakTime = 15; // Testing shows the max helps darken text
		setHeatTime(heatTime);
		setPrintDensity(printDensity);
		// Set default printer settings 
		setDefault();
	}


	void ThermalPrinter::setSerial(Stream &s){
		serial = &s;
	}

	void ThermalPrinter::setDefault() {
		wake();
		justify('L');
		inverseOff();
		doubleHeightOff();
		setLineHeight(32);
		boldOff();
		underlineOff();
		setBarcodeHeight(50);
		setSize('s');
	}

	void ThermalPrinter::test() {
		println("Hello World!");
		feed(2);
	}

	void ThermalPrinter::setBarcodeHeight(int val) {
		// Default is 50
		writeBytes(29, 104, val);
	}

	void ThermalPrinter::printBarcode(char * text){
		writeBytes(29, 107, 0); // GS, K, m!
		for(int i = 0; i < strlen(text); i ++){
			serial->write(text[i]); // Data
		}
		serial->write(zero); // Terminator
		delay(3000); // For some reason we can't immediately have line feeds here
		feed(2);
	}

	void ThermalPrinter::printFancyBarcode(char * text){
		writeBytes(29, 107, 4); // GS, K, Fancy!
		for(int i = 0; i < strlen(text); i ++){
			serial->write(text[i]); //Data
		}
		serial->write(zero); //Terminator
		delay(3000); //For some reason we can't immediately have line feeds here
		feed(2);
	}

	void ThermalPrinter::writeBytes(uint8_t a, uint8_t b) {
		serial->write(a);
		serial->write(b);
	}

	void ThermalPrinter::writeBytes(uint8_t a, uint8_t b, uint8_t c) {
		serial->write(a);
		serial->write(b);
		serial->write(c);
	}

	void ThermalPrinter::writeBytes(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
		serial->write(a);
		serial->write(b);
		serial->write(c);
		serial->write(d);
	}

	void ThermalPrinter::inverseOn() {
		writeBytes(29, 'B', 1);
	}

	void ThermalPrinter::inverseOff() {
		writeBytes(29, 'B', 0, 10);
	}

	void ThermalPrinter::doubleHeightOn() {
		writeBytes(27, 14);
	}

	void ThermalPrinter::doubleHeightOff() {
		writeBytes(27, 20);
	}

	void ThermalPrinter::boldOn() {
		writeBytes(27, 69, 1);
	}

	void ThermalPrinter::boldOff() {
		writeBytes(27, 69, 0);
		if (linefeedneeded) {
			feed();
		}
		linefeedneeded = false;
	}

	void ThermalPrinter::justify(char value) {
		uint8_t pos = 0;
		if(value == 'l' || value == 'L') { pos = 0; }
		if(value == 'c' || value == 'C') { pos = 1; }
		if(value == 'r' || value == 'R') { pos = 2; }
		writeBytes(0x1B, 0x61, pos);
	}

	void ThermalPrinter::feed(uint8_t x) {
		while (x--) {
			serial->write(10);
		}
	}

	void ThermalPrinter::setSize(char value) {
		int size = 0;
		if(value == 's' || value == 'S') { size = 0; }
		if(value == 'm' || value == 'M') { size = 10; }
		if(value == 'l' || value == 'L') { size = 25; }
		writeBytes(29, 33, size, 10);
	}

	void ThermalPrinter::underlineOff() {
		writeBytes(27, 45, 0, 10);
	}

	void ThermalPrinter::underlineOn() {
		writeBytes(27, 45, 1);
	}

	void ThermalPrinter::print(char * text) {
		serial->print(text);
	}
	void ThermalPrinter::println(char * text) {
		serial->println(text);
	}


	void ThermalPrinter::printBitmap(uint16_t w, uint16_t h, const uint8_t *bitmap) {
		int rowBytes, rowBytesClipped, rowStart, chunkHeight, chunkHeightLimit, x, y, i;
		// Round to the nearest whole byte boundary
		rowBytes = (w + 7) / 8;
		// Printer has a max width of 384 pixels, i.e. 48 bytes of data in each row
		rowBytesClipped = (rowBytes >= 48) ? 48 : rowBytes;
		// To decide how many rows to send in each chunk, assume that printer has a buffer size of 256 bytes
		// So, how many rows of data can fit into 256 bytes?
		chunkHeightLimit = 256 / rowBytesClipped;
		// Assume 256 byte printer buffer
		if(chunkHeightLimit > 255) { chunkHeightLimit = 255; }
		// Ensure we send at least one row in each chunk
		else if(chunkHeightLimit < 1) { chunkHeightLimit = 1; }
		// Loop over all the image data
		for(i=rowStart=0; rowStart < h; rowStart += chunkHeightLimit) {
			// Take a "chunk" of all the remaining rows
			chunkHeight = h - rowStart;
			// If there are too many rows in our chunk, reduce it down to the max chunk size
			if(chunkHeight > chunkHeightLimit) { chunkHeight = chunkHeightLimit; }
			// Write device control command
			writeBytes(18, '*', chunkHeight, rowBytesClipped);
			// Loop over each row in the chunk we've taken
			for(y=0; y < chunkHeight; y++) {
				// Loop over each column in the row
				for(x=0; x < rowBytesClipped; x++, i++) {
					// Retrieve the corresponding byte of data from the image array
					serial->write(pgm_read_byte(bitmap + i));
				}
				// When we get to the end of the line, add on however many pixels were skipped off the end before the 8-byte boundary
				i += rowBytes - rowBytesClipped;
			}
			// Delay at the end of each "chunk" of lines to give the printer buffer chance to catch up
			delay(100);
		}
	}

	void ThermalPrinter::wake() {
		writeBytes(27, 61, 1);
	}

	void ThermalPrinter::sleep() {
		writeBytes(27, 61, 0);
	}

	void ThermalPrinter::tab() {
		serial->write(9);
	}

	void ThermalPrinter::setCharSpacing(int spacing) {
		writeBytes(27, 32, 0, 10);
	}

	void ThermalPrinter::setLineHeight(int val) {
		writeBytes(27, 51, val); // Default is 32
	}

	void ThermalPrinter::setHeatTime(int vHeatTime) {
		heatTime = vHeatTime;
		serial->write(27);
		serial->write(55);
		serial->write(7); // Default 64 dots = 8*('7'+1)
		serial->write(heatTime); // Default 80 or 800us
		serial->write(heatInterval); // Default 2 or 20us	
	}

	void ThermalPrinter::setHeatInterval(int vHeatInterval) {
		heatInterval = vHeatInterval;
		serial->write(27);
		serial->write(55);
		serial->write(7); // Default 64 dots = 8*('7'+1)
		serial->write(heatTime); // Default 80 or 800us
		serial->write(heatInterval); // Default 2 or 20us	
	}

	void ThermalPrinter::setPrintDensity(char vPrintDensity) {
		// Modify the print density and timeout
		printDensity = vPrintDensity;
		serial->write(18);
		serial->write(35);
		int printSetting = (printDensity<<4) | printBreakTime;
		serial->write(printSetting); // Combination of printDensity and printBreakTime
	}

	void ThermalPrinter::setPrintBreakTime(char vPrintBreakTime) {
		// Modify the print density and timeout
		printBreakTime = vPrintBreakTime;
		serial->write(18);
		serial->write(35);
		int printSetting = (printDensity<<4) | printBreakTime;
		serial->write(printSetting); // Combination of printDensity and printBreakTime
	}
}