Cyber G Midi aims to add midi to the Enya Cyber G shitty guitar. 

Just needs a Teensy 4.1 and some soldering and you can now add MIDI to the device.

Keyboard = will send the notes on/off based on the octave setting of the device

Guitar neck - default is only 1 button at a time can be pressed since pressing multiple isn't detected well by the device (currently disabled via ifdef. It sends a specific note for each press and letting go turns note off.
Rocker sends change program on up or down. No return to middle action is sent by device, so couldn't use it as a trigger.

Change Command is set for left, right, circle on, circle off buttons on the keyboard.

I might consider adding analog  stick for pitch bend but it's not really supported on the device, so I might not do it.
