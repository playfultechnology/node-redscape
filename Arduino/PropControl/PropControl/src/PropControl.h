#ifndef PropControl_h_
#define PropControl_h_

// DEFINES
/**
* PubSubClient library has a macro to define the callback function signature that is different for
* different hardware. However, this is a "pointer-to-function", and because we're using an instance method
* we need to override this definition with a "point-to-member function" instead.
// https://stackoverflow.com/questions/15841338/c-unresolved-overloaded-function-type
*/
//#define MQTT_CALLBACK_SIGNATURE void (PropControl::*callback)(char*, uint8_t*, unsigned int)
#define PROPCONTROL_VERSION 20190605

// INCLUDES
#include <PubSubClient.h>
#include <Arduino.h>

// CONSTANTS
const int NUM_COMMANDS = 25;
const int COMMAND_MAXLENGTH = 20;

namespace PlayfulTechnology 
{
	typedef struct {
		char commandText[COMMAND_MAXLENGTH];
		#if defined(ESP8266) || defined(ESP32)
		std::function<void()> handler;
		#else
		void(*handler)(void);
		#endif
	} command;
	
	
	class PropControl
	{	
		public:
			PropControl() {
				this->_pubsubclient = new PubSubClient();
			};
			~PropControl() {
				disconnect();
			};
			
			// C'tor with configuration required
			PropControl(Client &client, IPAddress serverIP, uint16_t port) {
				this->_pubsubclient = new PubSubClient(client);
				this->_pubsubclient->setServer(serverIP, port);
				this->_pubsubclient->setClient(client);
			}		

			PropControl(Client &client, IPAddress ip, uint16_t port, char* deviceID) {
				
				_deviceID = deviceID;
				this->_pubsubclient = new PubSubClient(client);
				this->_pubsubclient->setServer(ip, port);
				this->_pubsubclient->setClient(client);
				
				// Subscribe to topics meant for this device
				char topic[32];
				snprintf(topic, 32, "ToDevice/%s", _deviceID);
				this->_pubsubclient->subscribe(topic);
				this->_pubsubclient->subscribe("ToDevice/All");
				
				// Publish a message to the host
				snprintf(topic, 32, "ToHost/%s", _deviceID);
				publish("ToHost/From", "CONNECT");

			}
			
			#if defined(ESP8266) || defined(ESP32)
			void registerCommand(const char* commandText, std::function<void()> handler) {
				// Create a command object
				command c;
				// Populate it with the passed values
				//strcpy(c.commandText, commandText);
				snprintf(c.commandText, COMMAND_MAXLENGTH, commandText);
				c.handler = handler;
				// Add it to the list of registered commands				
				commandList[numRegisteredCommands] = c;
				numRegisteredCommands++;
			}
			#else
			void registerCommand(const char* commandText, void(*handler)(void)) {
				// Create a command object
				command c;
				// Populate it with the passed values
				//strcpy(c.commandText, commandText);
				snprintf(c.commandText, COMMAND_MAXLENGTH, commandText);
				c.handler = handler;
				// Add it to the list of registered commands				
				commandList[numRegisteredCommands] = c;
				numRegisteredCommands++;
			}
			#endif

			bool sendToHost(const char* payload) {
				char topic[32];
				snprintf(topic, 32, "ToHost/%s", _deviceID);
				return this->_pubsubclient->publish(topic, payload);
			}
			bool subscribeToAll() {
				return this->_pubsubclient->subscribe("ToDevice/All");
			}
			bool subscribeToSelf() {
				char topic[32];
				snprintf(topic, 32, "ToDevice/%s", _deviceID);
				return this->_pubsubclient->subscribe(topic);
			}
			void setServer(IPAddress ip, uint16_t port) {
				this->_pubsubclient->setServer(ip, port);
			};
			void setClient(Client& client) {
				this->_pubsubclient->setClient(client);
			};
			void setCallback(MQTT_CALLBACK_SIGNATURE) {
				this->_pubsubclient->setCallback(callback);
			}
			bool connect() {
				return this->_pubsubclient->connect(_deviceID);
			}
			void disconnect() {
				this->_pubsubclient->disconnect();
			}
			bool connect(char* id) {
				_deviceID = id;
				return this->_pubsubclient->connect(id);
			}
			bool subscribe(const char* topic) {
				return this->_pubsubclient->subscribe(topic);
			}
			bool publish(const char* topic, const char* payload) {
				return this->_pubsubclient->publish(topic, payload);
			}
			bool loop() {
				return this->_pubsubclient->loop();
			}

			//__attribute__ ((weak))
			void processCommand(char* topic, byte* payload, unsigned int length) {
				  // A buffer to hold received messages
				char msg[64];
				// The "payload" passed to this function is a byte*
				// Let's first copy its contents to the msg char[] array
				memcpy(msg, payload, length);
				// Add a NULL terminator to the message to make it a correct string
				msg[length] = '\0';
				// Now compare the message received to the set of registered commands
				for(int i=0; i<numRegisteredCommands; i++){
					// If a match is found...
					if(strcmp(msg, commandList[i].commandText) == 0) {
						// ... call the associated handler function
						commandList[i].handler();
						break;
					}
				}
			}
	
		private:
			// Unique name of this device, used as client ID to connect to MQTT server
			// and also topic name for messages published to this device
			char* _deviceID = "Cauldron";
			PubSubClient* _pubsubclient = nullptr;
			// Static variables must be delcared  inside the class, but defined outside.
			command commandList[NUM_COMMANDS];
			byte numRegisteredCommands;
	};
}
#endif