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
 * @file vu_meter_module.ino
 * @author Marcel Licence
 * @date 25.04.2021
 *
 * @brief  This module is only to calculate the vu-meters and drive the ws2812 8x8 led module​
 * Credits for HSVtoRGB function is unknown
 */



#define VU_METER_COUNT  8

#define VU_METER_DECREASE_MULTIPLIER 0.98f


#ifdef LED_STRIP_PIN

#include <Adafruit_NeoPixel.h>

uint32_t brightness = 4; /* I am using a low value to keep the LED's dark */

/* pixel count of the 8x8 module */
#define NUMPIXELS   (8*8)





struct pixel_rgb
{
    uint8_t r, g, b, a;
};

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, LED_STRIP_PIN,
                          NEO_GRB + NEO_KHZ800);

struct pixel_rgb pixels[NUMPIXELS] = {0};
#endif

/* 2 storages to allow double buffering */
float vuMeterValueInBf[VU_METER_COUNT];
float vuMeterValueDisp[VU_METER_COUNT];

void VuMeterMatrix_Init(void)
{
#ifdef LED_STRIP_PIN
    strip.begin();
    strip.show();
#endif
    VuMeterMatrix_Process();
}

/* found this function somewhere in some repo. source cannot be identified anymore */
void HSVtoRGB(int hue, int sat, int val, uint8_t colors[3])
{
    // hue: 0-359, sat: 0-255, val (lightness): 0-255
    int r, g, b, base;
    if (sat == 0)
    {
        // Achromatic color (gray).
        colors[0] = val;
        colors[1] = val;
        colors[2] = val;
    }
    else
    {
        base = ((255 - sat) * val) >> 8;
        switch (hue / 60)
        {
        case 0:
            r = val;
            g = (((val - base) * hue) / 60) + base;
            b = base;
            break;
        case 1:
            r = (((val - base) * (60 - (hue % 60))) / 60) + base;
            g = val;
            b = base;
            break;
        case 2:
            r = base;
            g = val;
            b = (((val - base) * (hue % 60)) / 60) + base;
            break;
        case 3:
            r = base;
            g = (((val - base) * (60 - (hue % 60))) / 60) + base;
            b = val;
            break;
        case 4:
            r = (((val - base) * (hue % 60)) / 60) + base;
            g = base;
            b = val;
            break;
        case 5:
            r = val;
            g = base;
            b = (((val - base) * (60 - (hue % 60))) / 60) + base;
            break;
        default:
            r = 0;
            g = 0;
            b = 0;
            break;
        }
        colors[0] = r;
        colors[1] = g;
        colors[2] = b;
    }
}

void VuMeterMatrix_Process(void)
{
    memcpy(vuMeterValueDisp, vuMeterValueInBf, sizeof(vuMeterValueDisp));
    for (int i = 0; i < 8; i++)
    {
        vuMeterValueInBf[i] *= VU_METER_DECREASE_MULTIPLIER;
    }
}

void VuMeterMatrix_Display(void)
{
#ifdef LED_STRIP_PIN
    for (int i = 0; i < NUMPIXELS; i++)
    {
        int row = (i - (i % 8)) >> 3;
        int col = (i % 8);

        float dbVal = 0;
        if (vuMeterValueDisp[row] > 1.0f)
        {
            dbVal = 8.0f;
        }
        else if (vuMeterValueDisp[row] > 0.0f)   /* log would crash if you put in zero */
        {
            dbVal = 8 + (3.32f * log10(vuMeterValueDisp[row]));
        }

        if (dbVal > col)
        {
            uint8_t colors[3];
            float hue = 120.0f - ((float)dbVal) * (120.0f / 8.0f);  /* from green to red / min to max */
            HSVtoRGB(hue, 255, brightness, colors);

            pixels[i].r = colors[0];
            pixels[i].g = colors[1];
            pixels[i].b = colors[2];
        }
        else
        {
            uint8_t colors[3];
#if 0 /* may be useful when doing some color effects */
            float hue = 240.0f + ((float)i) * (120.0f / 64.0f);
#endif
            HSVtoRGB(240, 255, brightness / 4, colors);
            pixels[i].r = colors[0];
            pixels[i].g = colors[1];
            pixels[i].b = colors[2];
        }

        strip.setPixelColor(i, pixels[i].r, pixels[i].g, pixels[i].b);
    }
    strip.show();
#endif
}

float *VuMeterMatrix_GetPtr(uint8_t index)
{
    if (index >= VU_METER_COUNT)
    {
        return NULL;
    }
    return &vuMeterValueInBf[index];
}

void VuMeterMatrix_SetBrighness(uint8_t unused, float value)
{
#ifdef LED_STRIP_PIN
    brightness = (value * 255.0f);
#endif
}

