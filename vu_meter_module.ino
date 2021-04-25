/*
 * This module is only to calculate the vu-meters and drive the ws2812 8x8 led module​
 *
 * Author: Marcel Licence
 *
 * Credits for HSVtoRGB function is unknown
 */

#include <Adafruit_NeoPixel.h>

uint32_t brightness = 4; /* I am using a low value to keep the LED's dark */

/* pixel count of the 8x8 module */
#define NUMPIXELS   (8*8)

#define VU_METER_DECREASE_MULTIPLIER 0.98f

#define VU_METER_COUNT	8

struct pixel_rgb
{
    uint8_t r, g, b, a;
};

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, LED_STRIP_PIN,
                          NEO_GRB + NEO_KHZ800);

struct pixel_rgb pixels[NUMPIXELS] = {0};

/* 2 storages to allow double buffering */
float vuMeterValueInBf[VU_METER_COUNT];
float vuMeterValueDisp[VU_METER_COUNT];

void VuMeter_Init(void)
{
    strip.begin();
    strip.show();
    VuMeter_Process();
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
        }
        colors[0] = r;
        colors[1] = g;
        colors[2] = b;
    }
}

void VuMeter_Process(void)
{
    memcpy(vuMeterValueDisp, vuMeterValueInBf, sizeof(vuMeterValueDisp));
    for (int i = 0; i < 8; i++)
    {
        vuMeterValueInBf[i] *= VU_METER_DECREASE_MULTIPLIER;
    }
}

void VuMeter_Display(void)
{
    for (int i = 0; i < NUMPIXELS; i++)
    {
        int row = (i - (i % 8)) >> 3;
        int col =  (i % 8);

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
            float hue = 120.0f -  ((float)dbVal) * (120.0f / 8.0f); /* from green to red / min to max */
            HSVtoRGB(hue, 255, brightness, colors);

            pixels[i].r = colors[0];
            pixels[i].g = colors[1];
            pixels[i].b = colors[2];
        }
        else
        {
            uint8_t colors[3];
            float hue = 240.0f +  ((float)i) * (120.0f / 64.0f);
            HSVtoRGB(240, 255, brightness / 4, colors);
            pixels[i].r = colors[0];
            pixels[i].g = colors[1];
            pixels[i].b = colors[2];
        }

        strip.setPixelColor(i, pixels[i].r, pixels[i].g, pixels[i].b);
    }
    strip.show();
}

float *VuMeter_GetPtr(uint8_t index)
{
    if (index >= VU_METER_COUNT)
    {
        return NULL;
    }
    return &vuMeterValueInBf[index];
}

void VuMeter_SetBrighness(uint8_t unused, float value)
{
    brightness = (value * 255.0f);
}
