# DmxSimple

![](extras/lights.jpg)

DMX controlled lights and visual effects are easily available from DJ or theatrical suppliers. This library gives you a simple way to drive spot lamps, floods, wall washers, lasers, smoke machines and more from a normal Arduino using DMX. If you can use analogWrite() to dim an LED, you will have no problems using DmxSimple to control something much bigger and brighter.

The software output is compatible with the Tinker.it! DMX shield, or DIY DMX circuits.

DmxSimple does some fancy stuff behind the scenes, so there is no need to worry about DMX frames, retransmissions and channel orders. Specify which channels you want at which value, and the library will handle everything else for you.

## Compatibility

DmxSimple is compatible with all known Arduinos including the Mega. The following Arduinos are recommended, as they support both DmxSimple and the Tinker.it! DMX shield: Arduino Mega, Arduino Duemilanove 328, Arduino Duemilanove, Arduino Diecimila, Arduino Bluetooth, Arduino Pro (5V version only), Arduino NG, Arduino Serial. 

## Installation

Download the archive. Extract to (arduino install)/hardware/libraries/DmxSimple . Restart Arduino so it recognises the library. 

## Example code

FadeUp
SerialToDmx 

The example code is also built into the library. Open Arduino, and look under File > Sketchbook > Examples > Library-DmxSimple. 

## Function calls

### DmxSimple.write(channel, value);

Set DMX channel to value. Channel has the range 1 to 512 (128 on old Arduinos with a 168 or Mega 8). Value is in the range 0 (off) to 255 (full brightness). 

### DmxSimple.maxChannel(channel);

Set the number of DMX channels in the DMX system. The DMX system sends out channels in order, starting at 1. So it takes longer to send channels 1-512 than it does to send only channels 1-10. DmxSimple defaults to sending all channels, but by reducing this you can increase the lamp update rate for even smoother animation. Using DmxSimple.maxChannel(0) turns off the DMX system, so you can use pin 3 as a regular pin. It will revert to a DMX output next time you call DmxSimple.write() or DmxSimple.maxChannel(nonzero). 

### DmxSimple.usePin(pin);

Set the output pin. DmxSimple defaults to using pin 3, making it compatible with the Tinker.it! DMX shield. But if you use this function, you can output on any pin capable of digital output. 

## DmxSimple community

We now have a group on Google Groups. Get assistance starting with DMX, or show off your installations. 

## Library limitations

Timer 2 is used. Due to the small amount of RAM on 168 and Mega8 Arduinos, only the first 128 channels are supported. Arduinos with processor sockets can easily be upgraded to a 328 to control all 512 channels. 

## Upgrade notes

The original version 1 of DmxSimple used a file "DmxInterrupt.h". This has since been removed. To upgrade old sketches, remove the #include "DmxInterrupt.h" line. When upgrading the library from v1, delete all the old files before copying the contents of the new archive. 
