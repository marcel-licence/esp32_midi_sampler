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

/*
 * this file should be opened with arduino, this is the main project file
 *
 * shown in: https://youtu.be/7uSobNW7_A4
 *
 * Some features listet here:
 * - loading/saving from/to sd
 * - add patch parameters saving
 * - buffered audio processing -> higher polyphony
 * - sd or littleFs usage (selectable)
 * - global pitchbend and modulation
 * - vu meter (using neopixel lib)
 * - adsr vca control
 * - file system selection -> littleFs support
 * - master reverb effect
 * - precise pitching of samples (just using simple float didn't work well)
 * - sound removal support -> defrag
 * - normalize record
 * - patch selection
 * - save with automatic increment of number in filename
 * - display / vt100 terminal support
 * - support of ST7735 160x80 compatible display -> update of Adafruit-ST7735-Library required
 * - allows using the AS5600 for scratching
 *
 * Author: Marcel Licence
 */

/*
 *
 /$$$$$$$$                     /$$       /$$                 /$$$$$$$   /$$$$$$  /$$$$$$$   /$$$$$$  /$$      /$$ /$$ /$$
| $$_____/                    | $$      | $$                | $$__  $$ /$$__  $$| $$__  $$ /$$__  $$| $$$    /$$$| $$| $$
| $$       /$$$$$$$   /$$$$$$ | $$$$$$$ | $$  /$$$$$$       | $$  \ $$| $$  \__/| $$  \ $$| $$  \ $$| $$$$  /$$$$| $$| $$
| $$$$$   | $$__  $$ |____  $$| $$__  $$| $$ /$$__  $$      | $$$$$$$/|  $$$$$$ | $$$$$$$/| $$$$$$$$| $$ $$/$$ $$| $$| $$
| $$__/   | $$  \ $$  /$$$$$$$| $$  \ $$| $$| $$$$$$$$      | $$____/  \____  $$| $$__  $$| $$__  $$| $$  $$$| $$|__/|__/
| $$      | $$  | $$ /$$__  $$| $$  | $$| $$| $$_____/      | $$       /$$  \ $$| $$  \ $$| $$  | $$| $$\  $ | $$
| $$$$$$$$| $$  | $$|  $$$$$$$| $$$$$$$/| $$|  $$$$$$$      | $$      |  $$$$$$/| $$  | $$| $$  | $$| $$ \/  | $$ /$$ /$$
|________/|__/  |__/ \_______/|_______/ |__/ \_______/      |__/       \______/ |__/  |__/|__/  |__/|__/     |__/|__/|__/


 */

/*
 * todos:
 * - better sound morphing - in progress
 * - fix of auto gain - improved
 * - add tremolo
 * - modulation after time
 * - harmonics?
 * - ignore other tags in wav files
 *
 * done:
 * - add wav saving to sd
 * - add wav loading from sd
 * - add patch parameter saving
 * - buffered audio processing poly from 6 to 14
 * - sd or littleFs usage
 * - pitchbend and modulation
 * - vu meter neopixel
 * - adsr control
 * - file system selection -> littleFs support
 * - add reverb effect
 * - increase of precision required of sample pitch
 * - sound removal support -> defrag
 * - normalize record
 * - patch selection
 * - save with automatic increment
 * - display
 * - simple scratching
 */

#include "config.h"

#include <Arduino.h>
#include <FS.h>
#include <LITTLEFS.h>
#include <SD_MMC.h>
#include <WiFi.h>

#ifdef AS5600_ENABLED
#include <Wire.h>
#endif


/*
 * use this to activate auto loading
 */
#define AUTO_LOAD_PATCHES_FROM_LITTLEFS
//#define AUTO_LOAD_PATCHES_FROM_SD_MMC

/*
 * you can activate this but be careful
 * this will pass trough the mic signal to output during record
 * this may cause heavy feedbacks!
 */
//#define SAMPLER_PASS_TROUGH_DURING_RECORD

/*
 * Chip is ESP32D0WDQ5 (revision 1)
 * Features: WiFi, BT, Dual Core, 240MHz, VRef calibration in efuse, Coding Scheme None

 * ref.: https://www.makerfabs.com/desfile/files/ESP32-A1S%20Product%20Specification.pdf

 * Board: ESP32 Dev Module
 * Flash Size: 32Mbit -> 4MB
 * RAM internal: 520KB SRAM
 * PSRAM: 4M (set to enabled!!!)
 *
 */

/* this is used to add a task to core 0 */
TaskHandle_t  Core0TaskHnd ;

/* to avoid the high click when turning on the microphone */
static float click_supp_gain = 0.0f;

float *vuInL;
float *vuInR;
float *vuOutL;
float *vuOutR;

extern uint32_t sampleRecordCount;

/* this application starts here */
void setup()
{
    // put your setup code here, to run once:
    delay(500);

    Serial.begin(115200);

    Serial.println();

    Serial.printf("Firmware started successfully\n");

#ifdef AS5600_ENABLED
    //  digitalWrite(TFT_CS, HIGH);
    //  pinMode(TFT_CS, OUTPUT);

    Wire.setClock(I2C_SPEED);
#endif

    click_supp_gain = 0.0f;

#ifdef BLINK_LED_PIN
    Blink_Setup();
#endif
    Status_Setup();

#ifdef ESP32_AUDIO_KIT
#ifdef ES8388_ENABLED
    ES8388_Setup();
#else
    ac101_setup();
    /* using mic as default source */
    ac101_setSourceMic();
#endif
#endif

#ifdef AS5600_ENABLED
    Wire.begin(I2C_SDA, I2C_SCL);
    AS5600_Setup();
#endif

    setup_i2s();
#ifdef ESP32_AUDIO_KIT
    button_setup();
#endif
    Sine_Init();

    Reverb_Setup();

    /*
     * setup midi module / rx port
     */
    Midi_Setup();

#if 0
    setup_wifi();
#else
    WiFi.mode(WIFI_OFF);
#endif

#ifndef ESP8266
    btStop();
    // esp_wifi_deinit();
#endif

    Delay_Init();
    Delay_Reset();

    Sampler_Init();

    Serial.printf("ESP.getFreeHeap() %d\n", ESP.getFreeHeap());
    Serial.printf("ESP.getMinFreeHeap() %d\n", ESP.getMinFreeHeap());
    Serial.printf("ESP.getHeapSize() %d\n", ESP.getHeapSize());
    Serial.printf("ESP.getMaxAllocHeap() %d\n", ESP.getMaxAllocHeap());

    Serial.printf("Total heap: %d\n", ESP.getHeapSize());
    Serial.printf("Free heap: %d\n", ESP.getFreeHeap());

    /* PSRAM will be fully used by the looper */
    Serial.printf("Total PSRAM: %d\n", ESP.getPsramSize());
    Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());

    vuInL = VuMeter_GetPtr(0);
    vuInR = VuMeter_GetPtr(1);
    vuOutL = VuMeter_GetPtr(6);
    vuOutR = VuMeter_GetPtr(7);

#ifdef AUTO_LOAD_PATCHES_FROM_LITTLEFS
    /* finally we can preload some data if available */
    PatchManager_SetDestination(0, 1);
    Sampler_LoadPatchFile("/samples/pet_bottle.wav");
#endif

    /* select sd card */
#ifdef AUTO_LOAD_PATCHES_FROM_SD_MMC
    PatchManager_SetDestination(1, 1);
    Sampler_LoadPatchFile("/samples/PlateDeep.wav");
    Sampler_LoadPatchFile("/samples/76_Pure.wav");
#endif

    /* we need a second task for the terminal output */
    xTaskCreatePinnedToCore(Core0Task, "Core0Task", 8000, NULL, 999, &Core0TaskHnd, 0);

    /* use this to easily test the output */
#if 1
    Sampler_NoteOn(0, 64, 1);
#endif

#ifdef AS5600_ENABLED
    /* starts first loaded sample and activates this for scratching */
    Sampler_SetScratchSample(0, 1);
#endif
}

inline
void Core0TaskSetup()
{
#ifdef DISPLAY_160x80_ENABLED
    Display_Setup();
#ifdef SCREEN_ENABLED
    Screen_Setup();
#endif
    App_SetBrightness(0, 0.25);
#endif

    VuMeter_Init();
}

inline
void Core0TaskLoop()
{
    Status_Process();
#ifndef AS5600_ENABLED /* does not work together */
    VuMeter_Display();
#endif
#ifdef SCREEN_ENABLED
    Screen_Loop();
#endif
#ifdef DISPLAY_160x80_ENABLED
    Display_Draw();
#endif
}

void Core0Task(void *parameter)
{
    Core0TaskSetup();

    while (true)
    {
        Core0TaskLoop();

        /* this seems necessary to trigger the watchdog */
        delay(1);
        yield();
    }
}

float master_output_gain = 1.0f;

/* little enum to make switching more clear */
enum acSource
{
    acSrcLine,
    acSrcMic
};

/* line in is used by default, so it should not be changed here */
enum acSource selSource = acSrcLine;

/* be carefull when calling this function, microphones can cause very bad feedback!!! */
void App_ToggleSource(uint8_t channel, float value)
{
    if (value > 0)
    {
        switch (selSource)
        {
        case acSrcLine:
            click_supp_gain = 0.0f;
#ifdef AC101_ENABLED
            ac101_setSourceMic();
#endif
            selSource = acSrcMic;
            Status_TestMsg("Input: Microphone");
            break;
        case acSrcMic:
            click_supp_gain = 0.0f;
#ifdef AC101_ENABLED
            ac101_setSourceLine();
#endif
            selSource = acSrcLine;
            Status_TestMsg("Input: LineIn");
            break;
        }
    }
}

void App_SetOutputLevel(uint8_t not_used, float value)
{
    master_output_gain = value;
}

/*
 * this should avoid having a constant offset on our signal
 * I am not sure if that is required, but in case it can avoid early clipping
 */
static float fl_offset = 0.0f;
static float fr_offset = 0.0f;


static float fl_sample[SAMPLE_BUFFER_SIZE];
static float fr_sample[SAMPLE_BUFFER_SIZE];

#ifndef absf
#define absf(a) ((a>=0.0f)?(a):(-a))
#endif

/*
 * the main audio task
 */
inline void audio_task()
{
    memset(fl_sample, 0, sizeof(fl_sample));
    memset(fr_sample, 0, sizeof(fr_sample));

    i2s_read_stereo_samples_buff(fl_sample, fr_sample, SAMPLE_BUFFER_SIZE);

    for (int n = 0; n < SAMPLE_BUFFER_SIZE; n++)
    {
        /*
         * this avoids the high peak coming over the mic input when switching to it
         */
        fl_sample[n] *= click_supp_gain;
        fr_sample[n] *= click_supp_gain;

        if (click_supp_gain < 1.0f)
        {
            click_supp_gain += 0.00001f;
        }
        else
        {
            click_supp_gain = 1.0f;
        }

        /* make it a bit quieter */
        fl_sample[n] *= 0.5f;
        fr_sample[n] *= 0.5f;

        /*
         * this removes dc from signal
         */
        fl_offset = fl_offset * 0.99 + fl_sample[n] * 0.01;
        fr_offset = fr_offset * 0.99 + fr_sample[n] * 0.01;

        fl_sample[n] -= fl_offset;
        fr_sample[n] -= fr_offset;

        /*
         * put vu values to vu meters
         */
        *vuInL = max(*vuInL, absf(fl_sample[n]));
        *vuInR = max(*vuInR, absf(fr_sample[n]));
    }

    /*
     * main loop core
     */
    Sampler_Process(fl_sample, fr_sample, SAMPLE_BUFFER_SIZE);

    /*
     * little simple delay effect
     */
    Delay_Process_Buff(fl_sample, fr_sample, SAMPLE_BUFFER_SIZE);

    /*
     * add also some reverb
     */
    Reverb_Process(fl_sample, fr_sample, SAMPLE_BUFFER_SIZE);

    /*
     * apply master output gain
     */
    for (int n = 0; n < SAMPLE_BUFFER_SIZE; n++)
    {
        /* apply master_output_gain */
        fl_sample[n] *= master_output_gain;
        fr_sample[n] *= master_output_gain;

        /* output signal to vu meter */
        *vuOutL = max(*vuOutL, absf(fl_sample[n]));
        *vuOutR = max(*vuOutR, absf(fr_sample[n]));
    }

    /* function blocks and returns when sample is put into buffer */
    if (i2s_write_stereo_samples_buff(fl_sample, fr_sample, SAMPLE_BUFFER_SIZE))
    {
        /* nothing for here */
    }

    Status_Process_Sample(SAMPLE_BUFFER_SIZE);
}

/*
 * this function will be called once a second
 * call can be delayed when one operation needs more time (> 1/44100s)
 */
void loop_1Hz(void)
{
#ifdef BLINK_LED_PIN
    Blink_Process();
#endif
#if 0 /* use this line to to show analog button values to setup their values */
    printf("ADC: %d\n", analogRead(36));
#endif
}

void loop_100Hz(void)
{
    static uint32_t loop_cnt;

    loop_cnt += 1;
    if ((loop_cnt) >= 100)
    {
        loop_cnt = 0;
        loop_1Hz();
    }

    /*
     * Put your functions here which should be called 100 times a second
     */
#ifdef ESP32_AUDIO_KIT
    button_loop();
#endif
}

#ifdef AS5600_ENABLED
inline
void loop_4th()
{
#ifdef AS5600_ENABLED
#ifdef DISPLAY_160x80_ENABLED
    if (Display_Busy() == false)
#endif
    {
        AS5600_Loop();
        Sampler_SetPitchAbs(AS5600_GetPitch(4));
    }
#endif
}
#endif

/*
 * this is the main loop
 */
void loop()
{
    audio_task(); /* audio tasks blocks for one sample -> 1/44100s */

#ifdef AS5600_ENABLED
    static uint32_t prec4 = 0;
    prec4++;
    if ((prec4 % 4) == 0)
    {
        loop_4th();
    }
#endif

    VuMeter_Process(); /* calculates slow falling bars */

    static uint32_t loop_cnt;

    loop_cnt += SAMPLE_BUFFER_SIZE;
    if ((loop_cnt) >= (SAMPLE_RATE / 100))
    {
        loop_cnt = 0;
        loop_100Hz();
    }

    /*
     * doing midi only 64 times per sample cycle
     */
    Midi_Process();
}

uint8_t baseKey = 64 + 12 + 7; // c

#ifdef KEYS_TO_MIDI_NOTES

void App_Key1Down()
{
    Sampler_NoteOn(1, baseKey + 0, 1);
}

void App_Key2Down()
{
    Sampler_NoteOn(1, baseKey + 2, 1);
}

void App_Key3Down()
{
    Sampler_NoteOn(1, baseKey + 4, 1);
}

void App_Key4Down()
{
    Sampler_NoteOn(1, baseKey + 5, 1);
}

void App_Key5Down()
{
    Sampler_NoteOn(1, baseKey + 7, 1);
}

void App_Key6Down()
{
    Sampler_NoteOn(1, baseKey + 9, 1);
}

void App_Key1Up()
{

    Sampler_NoteOff(1, baseKey + 0);
}

void App_Key2Up()
{
    Sampler_NoteOff(1, baseKey + 2);
}

void App_Key3Up()
{
    Sampler_NoteOff(1, baseKey + 4);
}

void App_Key4Up()
{
    Sampler_NoteOff(1, baseKey + 5);
}

void App_Key5Up()
{
    Sampler_NoteOff(1, baseKey + 7);
}

void App_Key6Up()
{
    Sampler_NoteOff(1, baseKey + 9);
}
#else

enum keyMode_e
{
    keyMode_playbackChrom,
    keyMode_record,
    keyMode_playbackSample,
    keyMode_storage
};

enum keyMode_e currentKeyMode = keyMode_playbackChrom;

#ifdef AS5600_ENABLED

uint8_t scratchNote = 0;

void App_ButtonCbPlaySample(uint8_t key, uint8_t down)
{
    if (down > 0)
    {
        switch (key)
        {
        case 0:
            scratchNote++;
            Sampler_SetScratchSample(scratchNote, 1);
            break;

        case 1:
            if (scratchNote > 0)
            {
                scratchNote--;
            }
            Sampler_SetScratchSample(scratchNote, 1);
            break;
        }
    }
    switch (key)
    {
    case 2:
        delay(250); /* to avoid recording the noise of the button */
        Sampler_RecordWait(0, down);
        break;

    case 3:
        //Sampler_Record(0, down);
        if (down > 0)
        {
            AS5600_SetPitchOffset(1.0f);
        }
        else
        {
            AS5600_SetPitchOffset(-100.0f);
        }
        break;
    }
}

#else

uint8_t lastCh = 1;

void App_ButtonCb(uint8_t key, uint8_t down)
{
    if ((key == 0) && (down > 0))
    {
        switch (currentKeyMode)
        {
        case keyMode_storage:
            currentKeyMode = keyMode_record;
#ifdef DISPLAY_160x80_ENABLED
            Display_SetHeader("Record");
#endif
            Status_LogMessage("Record");

            Status_SetKeyText(0, "1:select");
            Status_SetKeyText(1, "2:rWait");
            Status_SetKeyText(2, "3:record");
            Status_SetKeyText(3, "4:littFs");
            Status_SetKeyText(4, "5:sdMmc");
            Status_SetKeyText(5, "6:save");

            break;
        case keyMode_record:
            currentKeyMode = keyMode_playbackSample;
#ifdef DISPLAY_160x80_ENABLED
            Display_SetHeader("Sample playback");
#endif
            Status_LogMessage("Sample playback");

            Status_SetKeyText(0, "1:select");
            Status_SetKeyText(1, "2:smpl1");
            Status_SetKeyText(2, "3:smpl2");
            Status_SetKeyText(3, "4:smpl3");
            Status_SetKeyText(4, "5:smpl4");
            Status_SetKeyText(5, "6:smpl5");
            break;
        case keyMode_playbackSample:
            currentKeyMode = keyMode_playbackChrom;
#ifdef DISPLAY_160x80_ENABLED
            Display_SetHeader("Chromatic playback");
#endif
            Status_LogMessage("Chromatic playback");

            Status_SetKeyText(0, "1:select");
            Status_SetKeyText(1, "2:c");
            Status_SetKeyText(2, "3:c#");
            Status_SetKeyText(3, "4:d");
            Status_SetKeyText(4, "5:d#");
            Status_SetKeyText(5, "6:e");
            break;

        case keyMode_playbackChrom:
            currentKeyMode = keyMode_storage;
#ifdef DISPLAY_160x80_ENABLED
            Display_SetHeader("Sample loader");
#endif
            Status_LogMessage("Sample loader");

            Status_SetKeyText(0, "1:select");
            Status_SetKeyText(1, "2:next");
            Status_SetKeyText(2, "3:prev");
            Status_SetKeyText(3, "4:littFs");
            Status_SetKeyText(4, "5:sdMmc");
            Status_SetKeyText(5, "6:load");
            break;
        }
    }
    else
    {
        switch (currentKeyMode)
        {
        case keyMode_playbackChrom:
            if (down > 0)
            {
                Sampler_NoteOn(lastCh, baseKey + key, 1);
            }
            else
            {
                Sampler_NoteOff(lastCh, baseKey + key);
            }
            break;
        case keyMode_record:
            switch (key)
            {
            case 1:
                delay(250); /* to avoid recording the noise of the button */
                Sampler_RecordWait(0, down);
                break;
            case 2:
                Sampler_Record(0, down);
                break;
            case 3:
                PatchManager_SetDestination(0, 0);
                break;
            case 4:
                PatchManager_SetDestination(0, 1);
                break;
            case 5:
                Sampler_SavePatch(0, down);
                break;

            }
            break;
        case keyMode_playbackSample:
            if (down > 0)
            {
                Sampler_NoteOn(0, key - 1, 1);
                lastCh = key;
            }
            else
            {
                Sampler_NoteOff(0, key - 1);
            }
            break;
        case keyMode_storage:
            if (down > 0)
            {
                switch (key)
                {
                case 1:
                    PatchManager_FileIdxInc(0, 1);
                    break;
                case 2:
                    PatchManager_FileIdxDec(0, 1);
                    break;
                case 3:
                    PatchManager_SetDestination(0, 0);
                    break;
                case 4:
                    PatchManager_SetDestination(0, 1);
                    break;
                case 5:
                    Sampler_LoadPatch(0, 1);
                    break;
                }
            }
            break;
        }
    }
}

#endif
#endif

void App_SetBrightness(uint8_t unused, float value)
{
#ifdef DISPLAY_160x80_ENABLED
    Display_SetBacklight(value * 255.0f);
#endif
    VuMeter_SetBrighness(unused, value / 8.0f);
}
