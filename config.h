/*
 * Copyright (c) 2023 Marcel Licence
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Dieses Programm ist Freie Software: Sie können es unter den Bedingungen
 * der GNU General Public License, wie von der Free Software Foundation,
 * Version 3 der Lizenz oder (nach Ihrer Wahl) jeder neueren
 * veröffentlichten Version, weiter verteilen und/oder modifizieren.
 *
 * Dieses Programm wird in der Hoffnung bereitgestellt, dass es nützlich sein wird, jedoch
 * OHNE JEDE GEWÄHR,; sogar ohne die implizite
 * Gewähr der MARKTFÄHIGKEIT oder EIGNUNG FÜR EINEN BESTIMMTEN ZWECK.
 * Siehe die GNU General Public License für weitere Einzelheiten.
 *
 * Sie sollten eine Kopie der GNU General Public License zusammen mit diesem
 * Programm erhalten haben. Wenn nicht, siehe <https://www.gnu.org/licenses/>.
 */

/**
 * @file config.h
 * @author Marcel Licence
 * @date 12.05.2021
 *
 * @brief This file contains the project configuration
 *
 * All definitions are visible in the entire project
 *
 * Put all your project settings here (defines, numbers, etc.)
 * configurations which are requiring knowledge of types etc.
 * shall be placed in z_config.ino (will be included at the end)
 */


#ifndef CONFIG_H_
#define CONFIG_H_


#ifdef __CDT_PARSER__
#include <cdt.h>
#endif


#define SERIAL_BAUDRATE 115200

//#define NOTE_ON_AFTER_SETUP /* used to get a test tone without MIDI input. Can be deactivated */

/*
 * you can select one of the pre-defined boards
 * look into ML_SynthTools in ml_boards.h for more information
 * @see https://github.com/marcel-licence/ML_SynthTools
 */
#define BOARD_ML_V1 /* activate this when using the ML PCB V1 */
//#define BOARD_ESP32_AUDIO_KIT_AC101 /* activate this when using the ESP32 Audio Kit v2.2 with the AC101 codec */
//#define BOARD_ESP32_AUDIO_KIT_ES8388 /* activate this when using the ESP32 Audio Kit v2.2 with the ES8388 codec */
//#define BOARD_ESP32_DOIT /* activate this when using the DOIT ESP32 DEVKIT V1 board */

/* can be used to pass line in through audio processing to output */
//#define AUDIO_PASS_THROUGH

/* this changes latency but also speed of processing */
#define SAMPLE_BUFFER_SIZE 64

/* enable the following to get a vt100 compatible output which can be displayed for example with teraterm pro */
//#define VT100_ENABLED

/* following can be activated to output the key function in the status output */
#define STATUS_SHOW_BUTTON_TEXT

/* this will force using const velocity for all notes, remove this to get dynamic velocity */
//#define MIDI_USE_CONST_VELOCITY

/* this variable defines the max length of the delay and also the memory consumption */
#define MAX_DELAY   (SAMPLE_RATE/4) /* 1/2s -> @ 44100 samples */

/* you can receive MIDI messages via serial-USB connection */
/*
 * you could use for example https://projectgus.github.io/hairless-midiserial/
 * to connect your MIDI device via computer to the serial port
 */
#define MIDI_RECV_FROM_SERIAL

/*
 * you can use this to use the sampler without PSRAM
 * keep in mind that the there is just a small buffer available to store samples
 */
//#define SAMPLER_USE_HEAP_ONLY   (128*1024)

/*
 * activate MIDI via USB (not implemented in this project)
 *
 * This requires the MAX3421E connected via SPI to the ESP32
 *
 * @see https://youtu.be/Mt3rT-SVZww
 */
//#define MIDI_VIA_USB_ENABLED

/*
 * use this to display a scope on the oled display
 * It shares the I2C with the AS5600
 */
//#define OLED_OSC_DISP_ENABLED

//#define MIDI_STREAM_PLAYER_ENABLED /* activate this to use the midi stream playback module */

/*
 * Following define enables the AS5600 processing (for scratching)
 * It should be connected to I2C_SDA, I2C_SCL
 * It may be defined by the selected board (e.g. ESP32 Audio Kit with ES8388)
 * In case it is not defined please make your own defines
 * Ensure that only unused pins are used for I2C
 */
//#define AS5600_ENABLED


//#define I2C_SDA 18
//#define I2C_SCL 23

#define I2C_SPEED 1000000

/*
 * include the board configuration
 * there you will find the most hardware depending pin settings
 */
#include <ml_boards.h> /* requires the ML_SynthTools library:  https://github.com/marcel-licence/ML_SynthTools */

#ifdef BOARD_ML_V1
#elif (defined BOARD_ESP32_AUDIO_KIT_AC101)
#elif (defined BOARD_ESP32_AUDIO_KIT_ES8388)
#elif (defined BOARD_ESP32_DOIT)
#else
/* there is room left for other configurations */
#endif

/*
 * Some additional stuff when using the audio kit
 */
#ifdef ESP32_AUDIO_KIT

/*
 * use the following when you've modified the audio kit like shown in video: https://youtu.be/r0af0DB1R68
 * move R66-70 to R60-R64 (0 Ohm resistors, you can also put a solder bridge)
 * place at R55-R59 a 1.8k resistor
 */
#define AUDIO_KIT_BUTTON_ANALOG


//#define DISPLAY_160x80_ENABLED /* activate this when a 160x80 ST7735 compatible display is connected */


#ifdef DISPLAY_160x80_ENABLED
#define SCREEN_ENABLED

#ifndef AS5600_ENABLED /* this will conflict with I2C */
#define TFT_MOSI    0
#define TFT_SCLK    19
#define TFT_CS        23
#define TFT_RST       -1 // 5 // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC         21

#else
#define TFT_MOSI    23
#define TFT_SCLK    18
#define TFT_CS        5
#define MCP_CS      5 // 15
#define TFT_RST        -1 // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC         0 //12 is for SD usally, 21

#endif

#define TFT_BLK_PIN 14  // do not connect it would cause the Audio Kit to get stuck
#endif

#endif /* ESP32_AUDIO_KIT */

/*
 * You can modify the sample rate as you want
 */
#ifdef ESP32_AUDIO_KIT
#define SAMPLE_RATE 44100
#define SAMPLE_SIZE_16BIT
#else
#define SAMPLE_RATE 44100
#define SAMPLE_SIZE_16BIT
#endif


#endif /* CONFIG_H_ */

