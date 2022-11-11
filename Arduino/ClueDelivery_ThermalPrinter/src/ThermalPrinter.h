#ifndef Thermal_h
#define Thermal_h

#include <Arduino.h>

namespace PlayfulTechnology {
	class ThermalPrinter {

		public:
			// c'tor
			ThermalPrinter(Stream &s);
			
			void setSerial(Stream &s);
			void init();
			
			void setDefault();
			void setHeatTime(int vHeatTime);
			void setHeatInterval(int vHeatInterval);
			void setPrintDensity(char vPrintDensity);
			void setPrintBreakTime(char vPrintBreakTime);
			void test();
			
			// Text formatting
			void inverseOn();
			void inverseOff();
			void doubleHeightOn();
			void doubleHeightOff();
			void boldOn();
			void boldOff();
			void underlineOn();
			void underlineOff();
			void justify(char value);
			void setCharSpacing(int spacing);
			void setSize(char value);
			void setLineHeight(int val = 32);

			// Commands
			void feed(uint8_t x = 1);
			void sleep();
			void wake();
			void tab();
			void print(char * text);
			void println(char * text);
			
			// Barcodes
			void printBarcode(char * text);
			void printFancyBarcode(char * text);
			void setBarcodeHeight(int val);

			// Images
			void printBitmap(uint16_t w, uint16_t h,  const uint8_t *bitmap);

		private:
			Stream *serial;
		
			boolean linefeedneeded;

			// Helper functions
			void writeBytes(uint8_t a, uint8_t b);
			void writeBytes(uint8_t a, uint8_t b, uint8_t c);
			void writeBytes(uint8_t a, uint8_t b, uint8_t c, uint8_t d);

			int zero;

			int heatTime;
			int heatInterval;
			char printDensity;
			char printBreakTime;
	};
}
#endif