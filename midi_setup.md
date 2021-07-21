# MIDI Setup

## Board selection

First, please ensure that the correct board is selected. For that check the config.h.
In case you are using the ESP32 Audio Kit you should find the line:
#define ESP32_AUDIO_KIT
otherwise if you are not using the ESP32 Audio Kit (just ESP32 for example DOIT ESP32 DEVKIT V1) you should change the line to:
//#define ESP32_AUDIO_KIT
In that case the macro will not be active

## MIDI input connection

The MIDI input pin is defined using MIDI_RX_PIN. To be more flexible I've prepared two defines for quicker testing on the ESP32 Audio Kit and also on other hardware.

For example IO18 can be used as MIDI input on the ESP32 Audio Kit using:
#define MIDI_RX_PIN 18

## MIDI baudrate

The MIDI baud rate is specified to be 31250
It can be modified if needed (for example to send messages via computer using another interface).
In that case you can use:
#define MIDI_SERIAL2_BAUDRATE   115200 /* you can use this to change the serial speed */

If you want to use the default MIDI baud rate ensure that the setting is commented-out:
//#define MIDI_SERIAL2_BAUDRATE

## MIDI mapping

### Identify control elements
Standard MIDI messages like noteOn/noteOff/pitchbend are used as specified in MIDI.
In z_config.ino you will find a predefined MIDI map which can be replaced by your own to map functions to your controller.

To identify your different controls go into midi_module.ino and change '//#define DUMP_SERIAL2_TO_SERIAL' to '#define DUMP_SERIAL2_TO_SERIAL'
In that case you will see all received MIDI messages in your console window. Moving a specific controller would create messages like this
">b0 12 37", ">b0 12 38" etc.

Now you can create a line for each control element:
  { 0x0, 0x12, "Pot 1", NULL, NULL, 0},

The first number is the channel (that is the one after the ">b"). The second number is the data1 value, also called the control number. After that you can define a name so you know which knob, slider etc. it is.
When this controller is not mapped to any function you should add ", NULL, NULL, 0}," at the end of the line.

### Map control elements
In my code you will find that there are different functions mapped.

For example Reverb is mapped to a controller which I call "R2":
    { 0x0, 0x12, "R9", NULL, Reverb_SetLevel, 0},
    
So that means that
- channel 0
- control number 0x12
- with the name "R9"
is mapped to
- NULL (a specific type of function)
- Reverb_SetLevel
- with additional parameter value of 0 (the value will also provided to the called function)

For example by setting the destination the additional parameter is used to determine which destination should be used:
    { 0x4, 0x50, "A5", NULL, PatchManager_SetDestination, 0},
    { 0x5, 0x50, "A6", NULL, PatchManager_SetDestination, 1},




