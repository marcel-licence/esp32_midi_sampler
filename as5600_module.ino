/*
 * as5600_module.ino
 *
 *  Created on: 25.07.2021
 *      Author: PC
 *
 *      Datasheet: https://ams.com/documents/20143/36005/AS5600_DS000365_5-00.pdf
 */

#ifdef __CDT_PARSER__
#include <cdt.h>
#endif

#ifdef AS5600_ENABLED

//#include <Wire.h>

#define AS5600_ADDR 0x36

int sumCh = 0;

inline
void AS5600_WriteReg(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(AS5600_ADDR);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

inline
void AS5600_WriteRegU16(uint8_t reg, uint16_t value)
{
    Wire.beginTransmission(AS5600_ADDR);
    Wire.write(reg);
    Wire.write((value >> 8) & 0xFF);
    Wire.write(value & 0xFF);
    Wire.endTransmission();
}

inline
uint8_t AS5600_ReadReg(uint8_t reg)
{
    Wire.beginTransmission(AS5600_ADDR);
    Wire.write(reg);
    Wire.endTransmission();

    Wire.requestFrom(AS5600_ADDR, 1);
    return Wire.read();
}

inline
uint16_t AS5600_ReadReg_u16(uint8_t reg)
{
    Wire.beginTransmission(AS5600_ADDR);
    Wire.write(reg);
    Wire.endTransmission();

    Wire.requestFrom(AS5600_ADDR, 2);
    return (((uint16_t)Wire.read()) << 8) + (uint16_t)Wire.read();
}

#define REG_CONF 		0x07
#define REG_RAW_ANLGE   0x0C

void AS5600_Setup()
{
    // Serial.printf("CONF: 0x%08x\n", AS5600_ReadReg_u16(REG_CONF));
    AS5600_WriteRegU16(REG_CONF, 0x08); // hyst to 2 LSB to avoid static noise
    // Serial.printf("CONF: 0x%08x\n", AS5600_ReadReg_u16(REG_CONF));
}

void AS5600_Loop()
{
    static uint16_t lastVal = 0;
    uint16_t newVal = AS5600_ReadReg_u16(REG_RAW_ANLGE);
    bool valUpdate = false;

    if (newVal > lastVal)
    {
        if (newVal - lastVal > 0)
        {
            valUpdate = true;
        }
    }
    else
    {
        if (lastVal -  newVal > 0)
        {
            valUpdate = true;
        }
    }
    if (valUpdate)
    {
        int ch = ((int)newVal) - ((int)lastVal);
        if (ch > 2048)
        {
            ch -= 4096;
        }
        if (ch < -2048)
        {
            ch += 4096;
        }
        //Serial.printf("RAW_AGNLE: %d, %d\n", newVal, ch);
        lastVal = newVal;
        sumCh += ch;
    }
}

static float lastPitch = 0;
static float pitchOffset = -100.0f;

float AS5600_GetPitch(uint8_t oversample)
{
    float pitch = sumCh;

    sumCh = 0;
    pitch *= 0.25f;
    pitch /= oversample;
    if (pitch > 50)
    {
        pitch = 50;
    }
    if (pitch < -50)
    {
        pitch  = -50;
    }

    for (int i = 0; i < oversample; i++)
    {
        lastPitch = pitch * 0.05 + lastPitch * 0.95;
    }
    return max(lastPitch, pitchOffset);
}

void AS5600_SetPitchOffset(float offset)
{
    pitchOffset = offset;
}

#endif
