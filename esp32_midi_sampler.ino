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
 *
 * Author: Marcel Licence
 */

/*
 * todos:
 * - better sound morphing - in progress
 * - fix of auto gain - improved
 * - add tremolo
 * - modulation after time
 * - harmonics?
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
 */

#include <Arduino.h>
#include <FS.h>
#include <LITTLEFS.h>
#include <SD_MMC.h>
#include <WiFi.h>

#define VT100_ENABLED

/* on board led */
#define LED_PIN 	19 // IO19 -> D5

// set to pin connected to data input of WS8212 (NeoPixel) strip
#define LED_STRIP_PIN         21


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

/* our samplerate */
#define SAMPLE_RATE 44100

/* this is used to add a task to core 0 */
TaskHandle_t  Core0TaskHnd ;

/* to avoid the high click when turning on the microphone */
static float click_supp_gain = 0.0f;

float *vuInL;
float *vuInR;
float *vuOutL;
float *vuOutR;

/* this application starts here */
void setup()
{
    // put your setup code here, to run once:
    delay(500);

    Serial.begin(115200);

    Serial.println();

    Serial.printf("Loading data\n");

    Serial.printf("Firmware started successfully\n");

    click_supp_gain = 0.0f;

    Blink_Setup();

    Status_Setup();

    ac101_setup();

    setup_i2s();

    button_setup();

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

    btStop();
    // esp_wifi_deinit();

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
    xTaskCreatePinnedToCore(CoreTask0, "terminalTask", 8000, NULL, 999, &Core0TaskHnd, 0);

    /* using mic as default source */
    ac101_setSourceMic();
}

void CoreTask0( void *parameter )
{
    VuMeter_Init();

    while (true)
    {
        Status_Process();
        VuMeter_Display();

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
            ac101_setSourceMic();
            selSource = acSrcMic;
            Status_TestMsg("Input: Microphone");
            break;
        case acSrcMic:
            click_supp_gain = 0.0f;
            ac101_setSourceLine();
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

#define SAMPLE_BUFFER_SIZE	64

static float fl_sample[SAMPLE_BUFFER_SIZE];
static float fr_sample[SAMPLE_BUFFER_SIZE];

#ifndef absf
#define absf(a)	((a>=0.0f)?(a):(-a))
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
    static uint32_t cycl = ESP.getCycleCount();
    static uint32_t lastCycl;

    lastCycl = cycl;

    button_loop();
    Blink_Process();
}

static uint32_t midi_pre_scaler = 0;

/*
 * this is the main loop
 */
void loop()
{
    audio_task(); /* audio tasks blocks for one sample -> 1/44100s */

    VuMeter_Process(); /* calculates slow falling bars */

    static uint32_t loop_cnt;

    loop_cnt += SAMPLE_BUFFER_SIZE;
    if ((loop_cnt) >= SAMPLE_RATE)
    {
        loop_cnt = 0;
        loop_1Hz();
    }

    //midi_pre_scaler++;
    //if (midi_pre_scaler > 64)
    {
        /*
         * doing midi only 64 times per sample cycle
         */
        Midi_Process();
        midi_pre_scaler = 0;
    }
}
