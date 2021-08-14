# esp32_midi_sampler
ESP32 Audio Kit Sampling MIDI Module - A little DIY Arduino based audio/synthesizer project

The project can be seen in my video https://youtu.be/7uSobNW7_A4

The project is written for the ESP32 Audio Kit V2.2
Arduino Settings:
- Board: ESP32 Dev Module
- PSRAM: Enabled

ESP32 LittleFS Data Upload is required to upload samples into flash.

Compile information:
- board 'esp32' Version 1.0.6
- core 'esp32' Version 1.0.6

Following libraries are used:
- FS in Version 1.0
- LittleFS_esp32 in Version 1.0.6
- SD_MMC in Version 1.0
- WiFi in Version 1.0
- AC101 in Version 0.0.1
- Adafruit_NeoPixel in Version 1.7.0
- Wire in Version 1.0.1

Configuration of the Audio Kit:
- Set DIP 2, 3 to on and all other DIP switches to off (required for SD card access)

To store samples on the ESP32 you can upload them using the Arduino plugin:
- ESP32 LittleFS Data Upload (src: https://github.com/lorol/arduino-esp32littlefs-plugin)

A WS2812 8x8 led matrix can be connected to IO12, 3V3, GND (defined by LED_STRIP_PIN in config.h)
ref: https://hackaday.com/2017/01/20/cheating-at-5v-ws2812-control-to-use-a-3-3v-data-line/

MIDI should be connected to IO18  (you can modify the define MIDI_RX_PIN in config.h)

AS5600 can be now used for scratching. To use it, set the define "AS5600_ENABLED" in config.h as active.
The connection is defined below:
- SDA: IO21
- SCL: IO22
Note: MIDI_RX_PIN changed from IO18 to IO19 on the ESP32 Audio Kit.
In that case you can attach a 160x80 display but its connection will change to:
- MOS: IO23
- SCLK: IO18
- CS: IO5
- DC: IO0
- RST -> should be connected to RST / EN of the ESP32


A controller mapping can be found in z_config.ino.
---
Keys can be used with only one IO by a little modification of the ESP32 Audio Kit (ref: https://youtu.be/r0af0DB1R68)
- move R66-70 to R60-R64 (0 Ohm resistors, you can also put a solder bridge)
- place at R55-R59 a 1.8k resistor
Finally the define AUDIO_KIT_BUTTON_ANALOG in config.h is required

---
If you have questions or ideas please feel free to use the discussion area!

---
External resources:
reverb_module.ino is a ported from stm32 code (src. https://github.com/YetAnotherElectronicsChannel/STM32_DSP_Reverb/blob/master/code/Src/main.c)
A great video explaining the reverb can be found on his channel: https://youtu.be/nRLXNmLmHqM



