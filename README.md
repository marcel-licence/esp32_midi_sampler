# esp32_midi_sampler
ESP32 Audio Kit Sampling MIDI Module - A little DIY Arduino based audio/synthesizer project

The project can be seen in my video https://youtu.be/7uSobNW7_A4

The project is written for the ESP32 Audio Kit

Configuration of the Audio Kit:
- Set DIP 2, 3 to on and all other DIP switches to off (required for SD card access)

To store samples on the ESP32 you can upload them using the Arduino plugin:
- ESP32 LittleFS Data Upload (src: https://github.com/lorol/arduino-esp32littlefs-plugin)

A WS2812 8x8 led matrix can be connected to IO21, 3V3, GND

MIDI should be connected to IO22

A controller mapping can be found in midi_interface.ino. 
---
If you have questions or ideas please feel free to use the discussion area!

---
External ressources:
reverb_module.ino is a ported from stm32 code (src. https://github.com/YetAnotherElectronicsChannel/STM32_DSP_Reverb/blob/master/code/Src/main.c)
A great video explaining the reverb can be found on his channel: https://youtu.be/nRLXNmLmHqM
