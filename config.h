/*
 * Copyright (c) 2021 Marcel Licence
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

/* you can receive MIDI messages via serial-USB connection */
/*
 * you could use for example https://projectgus.github.io/hairless-midiserial/
 * to connect your MIDI device via computer to the serial port
 */
#define MIDI_RECV_FROM_SERIAL

/* activate MIDI via USB (not implemented in this project) */
//#define MIDI_VIA_USB_ENABLED

//#define AS5600_ENABLED /* can be used for scratching */
#define I2C_SPEED 1000000

/*
 * include the board configuration
 * there you will find the most hardware depending pin settings
 */
#include <ml_boards.h>

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

