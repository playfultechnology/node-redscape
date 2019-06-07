# propcontrol
## Introduction
PropControl is a framework for managing a network of "props" - physical, microprocessor-controlled devices, such as might be found in a live theatrical performance, escape room, or similar. These props are typically designed to operate standalone - with internal logic running on a local Arduino controller which responds to inputs and controls outputs. PropControl creates a network interface between these devices - allowing them to report on their internal state, to be monitored remotely, to respond to events triggered by a server or other devices, or to be overridden or reset by a remote command.
PropControl consists of:
* Host software - the host controller software runs on a PC and provides a graphical interface to monitor and control all the props in the network. It is written in C# and uses the Unity engine. 
* Client library - the client library provides functionality for prop controllers to join and interact with the host and other props on the propcontrol network. Currently available for Arduino/ESP8266/ESP32-based props, though other controllers (e.g. Raspberry Pi) may be added at a future date. This library should be added and reference by the main sketch running on the device.

All network communication is based on the MQTT protocol, and an MQTT broker must be accessible to all devices. The recommended method to do so is to install and run the [Mosquitto](https://http://www.mosquitto.org/) broker on the same PC as is running the PropControl host controller software.

## Design Principles
PropControl is designed based on the principles of encapsulation and loose-coupling between devices. This makes it easy to create a flexible, expandable, adhoc network between a range of heterogenous devices, with a minimal amount of overhead. There is no standard set of commands and no strict contract between devices - each prop is free to define its own set of commands relevant to its function, and register these with the server so that other devices are aware of its capabilities.
For example, 
* ER puzzle devices might register the commands "RESET" and "FORCE_SOLVE"
* an ER clue delivery system might register the command "PRINT_CLUE"
* a door might register the commands "OPEN" and "CLOSE"

In addition to sending commands registered by individual props, the host software can broadcast common events, e.g. "GAME_START", "GAME_END", which do not need to be explicitly registered by any props. The host does not know (nor care) how many or what devices listen to such events.

## The problem with existing Escape Room control software
Suppose you had an escape room in which players open a cursed book. When they do so, the atmosphere of the room changes and becomes spookier - perhaps the lights change to take on a reddish tinge. Typically, the way this would occur in most escape room software is that a central control sofware would continuously poll all the inputs in the room and, when the relevant sensor (let's say it's sensor 0) indicated the book had been opened, the control software would send a message instructing the lights to change, as follows:
```
loop() {
  // Read the sensor value
  boolean bookHasBeenOpened = readInput(bookSensor(0));
  // If the sensor shows the book has been opened
  if(bookHasBeenOpened) {
    // Send a command to set the RGB value of the lights
    lights.setRGB(255,0,0);
  }
}
```
Now let's say you wanted to enhance the spooky atmosphere of the room with some sound. So you add an audio device to your network, and modify your room controller code to set the background music when the book is opened too. Perhaps you also want to play a "knock" sound effect randomly every 10-30 seconds, so your room control script now becomes something like this:
```
uint_32 timeOfLastKnockSFX;

loop() {
  // Read the sensor value
  boolean bookHasBeenOpened = readInput(bookSensor(0));
  // If the sensor shows the book has been opened
  if(bookHasBeenOpened) {
    // Send a command to set the RGB value of the lights
    light.setRGB(255,0,0);
    // Play some spooky music
    sound.playBackgroundMusic("spooky.mp3");
    sound.setVolume(100);
    if(now > timeOfLastKnock + random(10, 30)){
      sound.playSFX("knock.mp3");
      timeOfLastKnockSFX = now;
    }
  }
}
```
What about making a door open a slight amount, then slamming shut again? Now our code looks something like this:
```
uint_32 timeOfLastKnockSFX;
uint_32 timeDoorLastOpened;

loop() {
  // Read the sensor value
  boolean bookHasBeenOpened = readInput(bookSensor);
  // If the sensor shows the book has been opened
  if(bookHasBeenOpened) {
    // Send a command to set the RGB value of the lights
    light.setRGB(255,0,0);
    // Play some spooky music
    sound.playBackgroundMusic("spooky.mp3");
    sound.setVolume(100);
    if(now > timeOfLastKnock + random(10, 30)){
      sound.playSFX("knock.mp3");
      timeOfLastKnockSFX = now;
    }
    // Open the door
    door.Open(15, SLOW);
    // Wait a bit
    delay(3000);
    // Slam it shut again
    door.Close(-15, FAST);
  }
}
```
And so on... now, this code works, but it is monolithic and cumbersome to edit. Pretty soon, the list of commands will be pages long and pretty complex. It is *tightly coupled* - in that the central room control script contains details that are specific to the implementation of each individual prop. That makes it very hard to test any component in isolation - if you wanted to swap out a different door controller that needed slightly different values, you'd have to edit the entire room script. Likewise, if you added another light to the room, you'd have to duplicate the lighting code.

PropControl is different. Using PropControl, the central room script *only* deals with changes that relate to the room. In this case, that is that is has become spookier. 

```
pc.BroadcastEvent("GETSPOOKY");
```
Exactly how each individual prop reacts to that event is of no concern to the room controller. It is up to each prop to decide if, and how, they respond to being told this fact. One light might decide to turn red, another might go greenish-blue. The sound controller might make the sound of bats. But these are prop-specific decisions, and therefore set on the prop itself:
```
pc.RegisterCommand("GETSPOOKY", [](){
   // It's my light, and *I* get to decide how to make it spooky
   myLight.RGB(255,0,0);
   Serial.println("Thunderbolts and lightning, very very frightning indeed!");
  });
```
This encapsulation of behaviour means that the network structure can be completely dynamic - devices of any sort can be added or removed with no impact or configuration changes required.






