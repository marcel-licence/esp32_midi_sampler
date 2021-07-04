/*
 * this file contains basic stuff to work with the ESP32 Audio Kit V2.2 module
 *
 * Author: Marcel Licence
 */


/*
 * Instructions: http://myosuploads3.banggood.com/products/20210306/20210306011116instruction.pdf
 *
 * Schematic: https://docs.ai-thinker.com/_media/esp32-audio-kit_v2.2_sch.pdf
 */

#include "AC101.h" /* only compatible with forked repo: https://github.com/marcel-licence/AC101 */

//#define BUTTON_DEBUG_MSG


/* AC101 pins */
#define IIS_SCLK                    27
#define IIS_LCLK                    26
#define IIS_DSIN                    25
#define IIS_DSOUT                   35

#define IIC_CLK                     32
#define IIC_DATA                    33

#define GPIO_PA_EN                  GPIO_NUM_21
#define GPIO_SEL_PA_EN              GPIO_SEL_21


#define PIN_LED4    (22)
#define PIN_LED5    (19)


#ifdef AUDIO_KIT_BUTTON_DIGITAL
/*
 * when not modified and R66-R70 are placed on the board
 */
#define PIN_KEY_1                   (36)
#define PIN_KEY_2                   (13)
#define PIN_KEY_3                   (19)
#define PIN_KEY_4                   (23)
#define PIN_KEY_5                   (18)
#define PIN_KEY_6                   (5)

#define PIN_PLAY                    PIN_KEY_4
#define PIN_VOL_UP                  PIN_KEY_5
#define PIN_VOL_DOWN                PIN_KEY_6
#endif

#ifdef AUDIO_KIT_BUTTON_ANALOG
/*
 * modification required:
 * - remove R66-R70
 * - insert R60-R64 (0 Ohm or solder bridge)
 * - insert R55-R59 using 1.8kOhm (recommended but other values might be possible with tweaking the code)
 */
#define PIN_KEY_ANALOG              (36)

uint32_t keyMin[7] = {4095 - 32, 0, 462 - 32, 925 - 32, 1283 - 32, 1570 - 32, 1800 - 32 };
uint32_t keyMax[7] = {4095 + 32, 0 + 32, 462 + 32, 925 + 32, 1283 + 32, 1570 + 32, 1800 + 32 };
#endif

#define OUTPUT_PIN 0
#define MCLK_CH 0
#define PWM_BIT 1

AC101 ac;

/* actually only supporting 16 bit */
#define SAMPLE_SIZE_16BIT
//#define SAMPLE_SIZE_24BIT
//#define SAMPLE_SIZE_32BIT

#ifndef SAMPLE_RATE
#define SAMPLE_RATE 44100
#endif
#define CHANNEL_COUNT   2
#define WORD_SIZE   16
#define I2S1CLK (512*SAMPLE_RATE)
#define BCLK    (SAMPLE_RATE*CHANNEL_COUNT*WORD_SIZE)
#define LRCK    (SAMPLE_RATE*CHANNEL_COUNT)


typedef void(*audioKitButtonCb)(uint8_t, uint8_t);
extern audioKitButtonCb audioKitButtonCallback;

/*
 * this function could be used to set up the masterclock
 * it is not necessary to use the ac101
 */
void ac101_mclk_setup()
{
    // Put a signal out on pin
    uint32_t freq = SAMPLE_RATE * 512; /* The maximal frequency is 80000000 / 2^bit_num */
    Serial.printf("Output frequency: %d\n", freq);
    ledcSetup(MCLK_CH, freq, PWM_BIT);
    ledcAttachPin(OUTPUT_PIN, MCLK_CH);
    ledcWrite(MCLK_CH, 1 << (PWM_BIT - 1)); /* 50% duty -> The available duty levels are (2^bit_num)-1, where bit_num can be 1-15. */
}

/*
 * complete setup of the ac101 to enable in/output
 */
void ac101_setup()
{
    Serial.printf("Connect to AC101 codec... ");
    while (not ac.begin(IIC_DATA, IIC_CLK))
    {
        Serial.printf("Failed!\n");
        delay(1000);
    }
    Serial.printf("OK\n");
#ifdef SAMPLE_SIZE_24BIT
    ac.SetI2sWordSize(AC101::WORD_SIZE_24_BITS);
#endif
#ifdef SAMPLE_SIZE_16BIT
    ac.SetI2sWordSize(AC101::WORD_SIZE_16_BITS);
#endif

#if (SAMPLE_RATE==44100)&&(defined(SAMPLE_SIZE_16BIT))
    ac.SetI2sSampleRate(AC101::SAMPLE_RATE_44100);
    /*
     * BCLK: 44100 * 2 * 16 = 1411200 Hz
     * SYSCLK: 512 * fs = 512* 44100 = 22579200 Hz
     *
     * I2S1CLK/BCLK1 -> 512 * 44100 / 44100*2*16
     * BCLK1/LRCK -> 44100*2*16 / 44100 Obacht ... ein clock cycle goes high and low
     * means 32 when 32 bits are in a LR word channel * word_size
     */
    ac.SetI2sClock(AC101::BCLK_DIV_16, false, AC101::LRCK_DIV_32, false);
    ac.SetI2sMode(AC101::MODE_SLAVE);
    ac.SetI2sWordSize(AC101::WORD_SIZE_16_BITS);
    ac.SetI2sFormat(AC101::DATA_FORMAT_I2S);
#endif

    ac.SetVolumeSpeaker(3);
    ac.SetVolumeHeadphone(99);

#if 1
    ac.SetLineSource();
#else
    ac.SetMicSource(); /* handle with care: mic is very sensitive and might cause feedback using amp!!! */
#endif

#if 0
    ac.DumpRegisters();
#endif

    // Enable amplifier
    pinMode(GPIO_PA_EN, OUTPUT);
    digitalWrite(GPIO_PA_EN, HIGH);
}

/*
 * pullup required to enable reading the buttons (buttons will connect them to ground if pressed)
 */
void button_setup()
{
#ifdef AUDIO_KIT_BUTTON_DIGITAL
    // Configure keys on ESP32 Audio Kit board
    pinMode(PIN_PLAY, INPUT_PULLUP);
    pinMode(PIN_VOL_UP, INPUT_PULLUP);
    pinMode(PIN_VOL_DOWN, INPUT_PULLUP);
#endif
#ifdef AUDIO_KIT_BUTTON_ANALOG_OLD
    adcAttachPin(PIN_KEY_ANALOG);
    analogReadResolution(10);
    analogSetAttenuation(ADC_11db);
#endif
}

/*
 * selects the microphone as audio source
 * handle with care: mic is very sensitive and might cause feedback using amp!!!
 */
void ac101_setSourceMic(void)
{
    ac.SetMicSource();
}

/*
 * selects the line in as input
 */
void ac101_setSourceLine(void)
{
    ac.SetLineSource();
}

/*
 * very bad implementation checking the button state
 * there is some work required for a better functionality
 */
void button_loop()
{
#ifdef AUDIO_KIT_BUTTON_DIGITAL
    if (digitalRead(PIN_PLAY) == LOW)
    {
        Serial.println("PIN_PLAY pressed");
        if (buttonMapping.key4_pressed != NULL)
        {
            buttonMapping.key4_pressed();
        }
    }
    if (digitalRead(PIN_VOL_UP) == LOW)
    {
        Serial.println("PIN_VOL_UP pressed");
        if (buttonMapping.key5_pressed != NULL)
        {
            buttonMapping.key5_pressed();
        }
    }
    if (digitalRead(PIN_VOL_DOWN) == LOW)
    {
        Serial.println("PIN_VOL_DOWN pressed");
        if (buttonMapping.key6_pressed != NULL)
        {
            buttonMapping.key6_pressed();
        }
    }
#endif
#ifdef AUDIO_KIT_BUTTON_ANALOG
    static uint32_t lastKeyAD = 0xFFFF;
    static uint32_t keyAD = 0;
    static uint8_t pressedKey = 0;
    static uint8_t newPressedKey = 0;
    static uint8_t pressedKeyLast = 0;
    static uint8_t keySettle = 0;

    keyAD = analogRead(PIN_KEY_ANALOG);
    if (keyAD != lastKeyAD)
    {
        //Serial.printf("keyAd: %d\n", keyAD);
        lastKeyAD = keyAD;

        //pressedKey = 0;
        for (int i = 0; i < 7; i ++)
        {
            if ((keyAD >= keyMin[i]) && (keyAD < keyMax[i]))
            {
                newPressedKey = i;
            }
        }
        if (newPressedKey != pressedKey)
        {
            keySettle = 3;
            pressedKey = newPressedKey;
        }
    }

    if (keySettle > 0)
    {
        keySettle--;
        if (keySettle == 0)
        {
            if (pressedKey != pressedKeyLast)
            {
                if (pressedKeyLast > 0)
                {
#ifdef BUTTON_DEBUG_MSG
                    Serial.printf("Key %d up\n", pressedKeyLast);
#endif
                    if (audioKitButtonCallback != NULL)
                    {
                        audioKitButtonCallback(pressedKeyLast - 1, 0);
                    }
                }
                if (pressedKey > 0)
                {
#ifdef BUTTON_DEBUG_MSG
                    Serial.printf("Key %d down\n", pressedKey);
#endif
                    if (audioKitButtonCallback != NULL)
                    {
                        audioKitButtonCallback(pressedKey - 1, 1);
                    }
                }
                pressedKeyLast = pressedKey;
            }
        }
    }
#endif
}
