Cyber G Midi aims to add midi to the Enya Cyber G shitty guitar. 

Just needs a Teensy 4.1 and some soldering and you can now add MIDI to the device.

Keyboard = will send the notes on/off based on the octave setting of the device

Guitar neck - default is only 1 button at a time can be pressed since pressing multiple isn't detected well by the device (currently disabled via ifdef. It sends a specific note for each press and letting go turns note off.
Rocker sends change program on up or down. No return to middle action is sent by device, so couldn't use it as a trigger.

Change Command is set for left, right, circle on, circle off buttons on the keyboard.

I might consider adding analog  stick for pitch bend but it's not really supported on the device, so I might not do it.

How to actually use for you, the end user.

1. Open up your Cyber G Guitar. Start by removing the rubber cover where the logo is lit
2. Unscrew that lone screw and then remove the 5 screws under (disconnect the ribbon cable behind the metal buttons
3. Remove all the screws under the keyboard
4. You can now remove the top but be careful of the ribbon cable
5. Look at the wiring diagram and prepare to solder GND, Guitar to CyberG, Keyboard to CyberG, Silence and change logo color
6. Program the Teensy 4.1 with the ino I provided
7. Connect Neck to CyberG to pin 1
8. Connect Keyboard to CyberG to pin 7
9. Connect Gnd to Gnd
10. Connect Silence to Pin 10
11. Connect logo color change to Pin 11
12. Add a USB port to the device by making a hole and adding a USB-C breakout
13. Solder the 4 USB points of the breakout to the Teensy
14. Reassemble and test
15. You should now have a CyberG that outputs the correct midi for the piano and it plays specific notes for the neck. Other buttons have unique midi signals which can be assigned via VSTs.


Why does this mod exist?

1. Cyber G requires an app to work
2. The App requires online, meaning this is ewaste in a few years
3. Only the keyboard has MIDI but you literally have to remove it from the device to use it (unless you are crazy and put a USB-C connector in front like I did)
4. The sounds are really bad and they want me to pay $60 to unlock 12 sounds in the Chinese app
5. I got the Chinese version, which is Chinese only
