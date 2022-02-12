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
 * @file display_module.ino
 * @author Marcel Licence
 * @date 23.06.2021
 *
 * @brief  this file includes some implementation to drive a display (experimental code)
 */


#ifdef __CDT_PARSER__
#include <cdt.h>
#endif

#ifdef DISPLAY_160x80_ENABLED

#include <Adafruit_GFX.h> /* requires library Adafruit-GFX-Library from https://github.com/adafruit/Adafruit-GFX-Library */
#include <Adafruit_ST7735.h> /* requires library Adafruit-ST7735-Library from https://github.com/adafruit/Adafruit-ST7735-Library */
//#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>


#include "160x80_demo.h"


#define DW 160
#define DH 80

static SPIClass DispSpi;
static Adafruit_ST7735 tft = Adafruit_ST7735(&DispSpi, TFT_CS, TFT_DC, TFT_RST);

struct screen_s
{
    const char *head;
    void(*button_up)(void);
    void(*button_down)(void);
    void(*button_left)(void);
    void(*button_right)(void);
};

struct screen_s main =
{
    "Main",
    NULL,
    NULL,
    NULL,
    NULL,
};

static char display_header[27];
static char display_fulltext[27];

volatile struct screen_s *activeScreen = &main;
volatile struct screen_s *lastScreen = NULL;
static volatile int curSel = -1;
static volatile int curOffset = 0;

#define PWM_Ch 1



void drawRGBBitmapP(int16_t x, int16_t y, PGM_VOID_P bitmap,
                    int16_t w, int16_t h)
{
    uint16_t pixel_data_temp[160];
    for (int i = 0; i < h; i++)
    {
        memcpy_P(pixel_data_temp, &((struct gimpImg_s *)bitmap)->pixel_data[160 * 2 * i], sizeof(pixel_data_temp));
        tft.drawRGBBitmap(x, y + i, pixel_data_temp, w, 1);
    }
}



void Display_Setup()
{
    display_header[0] = 0;
    display_fulltext[0] = 0;

    DispSpi.begin(TFT_SCLK, -1, TFT_MOSI);

    tft.initR(INITR_096_160x80_IPS);  // Init ST7735S mini display
    //tft.setRotation(3); // set display orientation
    tft.setRotation(1); // set display orientation

#if 1
    drawRGBBitmapP(0, 0, &gimp_image_ml, 160, 80);
    delay(250);
    tft.invertDisplay(true);
    delay(250);
    tft.invertDisplay(false);
    delay(250);
    tft.invertDisplay(true);
    delay(250);
    tft.invertDisplay(false);
    delay(250);
#endif
    tft.fillScreen(ST77XX_WHITE);

#if 0
    tft.fillScreen(ST77XX_WHITE);
    char ttt[] = "1234567890123456789012345#"; // # 26
    testdrawtext(ttt, ST77XX_BLACK, ST77XX_WHITE);
    tft.drawFastHLine(0, 11, tft.width(), 0);

    for (int i = 0; i < 8; i++)
    {
        char testStr[] = "###4567890123456789012345#";
        testdrawtextline(testStr, ST77XX_BLACK, ST77XX_WHITE, i);
    }

    delay(50);

    tft.fillScreen(ST77XX_BLACK);
    char bk[] = "black";
    testdrawtext(bk, 0xFFFF, ST77XX_BLACK);
    delay(50);

    tft.fillScreen(0xF800);
    char red[] = "red";
    testdrawtext(red, 0xFFFF, 0xF800);
    delay(50);

    tft.fillScreen(0x07C0);
    char gr[] = "green";
    testdrawtext(gr, 0xFFFF, 0x07C0);
    delay(50);

    tft.fillScreen(0x003F);
    char bl[] = "blue";
    testdrawtext(bl, 0xFFFF, 0x003F);
    delay(50);

    tft.fillScreen(ST77XX_WHITE);
    char wr[] = "white";
    testdrawtext(wr, ST77XX_BLACK, ST77XX_WHITE);
    delay(50);
#endif

#ifdef TFT_BLK_PIN
    ledcAttachPin(TFT_BLK_PIN, PWM_Ch);
    ledcSetup(PWM_Ch, 800, 8);
#endif
}

char display_line[8][27];

static bool redraw = false;

void Display_Draw()
{
#if 0
    if (activeScreen != lastScreen)
    {
        //for (int n = 0; n<7; n++)
        {
            //  curSel = n;



            Serial.printf("redraw %d\n", curSel);
            lastScreen = activeScreen;
            //tft.fillScreen(ST77XX_WHITE);
            if (display_header[0] == 0)
            {
                testdrawtext(activeScreen->head, ST77XX_BLACK, ST77XX_WHITE);
            }
            else
            {
                testdrawtext(display_header, ST77XX_BLACK, ST77XX_WHITE);
            }
            tft.drawFastHLine(0, 11, tft.width(), 0);

            for (int i = 0; i < 8; i++)
            {
                uint16_t bg = ST77XX_WHITE;
                if (i == curSel)
                {
                    //tft.fillRect(0, 14 + (8 * i), tft.width(), 8, ST77XX_GREEN);
                    bg = ST77XX_GREEN;
                }
                //char longLine[64];
                sprintf(display_line[i], "#%d#45678901234A67890123#%d#", i, curSel + 1);
                testdrawtextline(display_line[i], ST77XX_BLACK, bg, i);
            }
            //delay(250);
        }

    }
#endif
    if (redraw)
    {
        if (display_fulltext[0] != 0)
        {
            tft.setTextSize(3);
            testdrawtext(display_fulltext, ST77XX_BLACK, ST77XX_WHITE);
        }
        else
        {
            {
                uint16_t bg = ST77XX_WHITE;
                if (curSel == -1)
                {
                    bg = ST77XX_GREEN;
                }
                testdrawtext(display_header, ST77XX_BLACK, bg);
            }
            tft.drawFastHLine(0, 11, tft.width(), 0);

            for (int i = 0; i < 8; i++)
            {
                uint16_t bg = ST77XX_WHITE;
                if (i == curSel)
                {
                    bg = ST77XX_GREEN;
                }
                testdrawtextline(display_line[i], ST77XX_BLACK, bg, i);
            }
        }
        redraw = false;
    }
}

bool Display_Busy()
{
    return redraw;
}

void Display_Redraw()
{
    redraw = true;
}

void Display_SetLine(int i, char *text)
{
    memset(display_line[i], ' ', 26);
    memcpy(display_line[i], text, strlen(text));
}

#define min(a,b)  ((a>b)?(b):(a))

void Display_SetHeader(const char *headerText)
{
    memset(display_header, ' ', 26);
    memcpy(display_header, headerText, min(26, strlen(headerText)));
    //lastScreen = NULL;
}

void Display_SetFullText(const char *headerText)
{
    memset(display_fulltext, ' ', 26);
    memcpy(display_fulltext, headerText, min(26, strlen(headerText)));
    //lastScreen = NULL;
}

void Display_SetCurSel(int sel)
{
    curSel = sel;
}

void Display_SetBacklight(uint8_t DutyCycle)
{
#ifdef TFT_BLK_PIN
    ledcWrite(PWM_Ch, DutyCycle);
#else
    Serial.printf("Cannot change backlight, TFT_BLK_PIN not set!\n");
#endif
}

void testdrawtext(const char *text, uint16_t color, uint16_t bg)
{
    tft.setCursor(2, 3);
    tft.setTextColor(color, bg);
    tft.setTextWrap(true);

    char textLine[]  = "                          ";
    memcpy(textLine, text, strlen(text));

    tft.print(textLine);
}

void testdrawtextline(char *text, uint16_t color, uint16_t bg, uint8_t line)
{
    tft.setCursor(2, 14 + 8 * line);
    tft.setTextColor(color, bg);
    tft.setTextWrap(true);

    char textLine[]  = "                          ";
    memcpy(textLine, text, strlen(text));

    tft.print(textLine);
}

#endif
