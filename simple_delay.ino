/*
 * This is a simple implementation of a stereo s16 delay line
 * - level adjustable
 * - feedback
 * - length adjustable
 *
 * here is some magic the module also uses some PSRAM for the second channel!!!
 *
 * Author: Marcel Licence
 */


/* max delay can be changed but changes also the memory consumption */
#define MAX_DELAY	(SAMPLE_RATE / 2) /* 0.5s */

/*
 * module variables
 */
int16_t *delayLine_l;
int16_t *delayLine_r;
float delayToMix = 0;
float delayInLvl = 1.0f;
float delayFeedback = 0;
uint32_t delayLen = 11098;
uint32_t delayIn = 0;
uint32_t delayOut = 0;

void Delay_Init(void)
{
    psramInit();
    Serial.printf("Total PSRAM: %d\n", ESP.getPsramSize());
    Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());

    delayLine_l = (int16_t *)malloc(sizeof(int16_t) * MAX_DELAY);
    if (delayLine_l == NULL)
    {
        Serial.printf("Not enough heap memory!\n");
    }
    delayLine_r = (int16_t *)ps_malloc(sizeof(int16_t) * MAX_DELAY);
    if (delayLine_r == NULL)
    {
        Serial.printf("Not enough PSRAM memory!\n");
    }
    Delay_Reset();
}

void Delay_Reset(void)
{
    for (int i = 0; i < MAX_DELAY; i++)
    {
        delayLine_l[i] = 0;
        delayLine_r[i] = 0;
    }
}

void Delay_Process(float *signal_l, float *signal_r)
{
#if 0
    *signal_l *= (1.0f - delayFeedback);
    *signal_r *= (1.0f - delayFeedback);
#endif

    delayLine_l[delayIn] = (((float)0x8000) * *signal_l * delayInLvl);
    delayLine_r[delayIn] = (((float)0x8000) * *signal_r * delayInLvl);

    delayOut = delayIn + (1 + MAX_DELAY - delayLen);

    if (delayOut >= MAX_DELAY)
    {
        delayOut -= MAX_DELAY;
    }

    *signal_l += ((float)delayLine_l[delayOut]) * delayToMix / ((float)0x8000);
    *signal_r += ((float)delayLine_r[delayOut]) * delayToMix / ((float)0x8000);

    delayLine_l[delayIn] += (((float)delayLine_l[delayOut]) * delayFeedback);
    delayLine_r[delayIn] += (((float)delayLine_r[delayOut]) * delayFeedback);

    delayIn ++;

    if (delayIn >= MAX_DELAY)
    {
        delayIn = 0;
    }
}

void Delay_Process_Buff(float *signal_l, float *signal_r, int buffLen)
{
#if 0
    *signal_l *= (1.0f - delayFeedback);
    *signal_r *= (1.0f - delayFeedback);
#endif

    for (int n = 0; n < buffLen; n++)
    {
        delayLine_l[delayIn] = (((float)0x8000) * signal_l[n] * delayInLvl);
        delayLine_r[delayIn] = (((float)0x8000) * signal_r[n] * delayInLvl);

        delayOut = delayIn + (1 + MAX_DELAY - delayLen);

        if (delayOut >= MAX_DELAY)
        {
            delayOut -= MAX_DELAY;
        }

        signal_l[n] += ((float)delayLine_l[delayOut]) * delayToMix / ((float)0x8000);
        signal_r[n] += ((float)delayLine_r[delayOut]) * delayToMix / ((float)0x8000);

        delayLine_l[delayIn] += (((float)delayLine_l[delayOut]) * delayFeedback);
        delayLine_r[delayIn] += (((float)delayLine_r[delayOut]) * delayFeedback);

        delayIn ++;

        if (delayIn >= MAX_DELAY)
        {
            delayIn = 0;
        }
    }
}

void Delay_SetInputLevel(uint8_t unused, float value)
{
    delayInLvl = value;
    Status_ValueChangedFloat("DelayInputLevel", value);
}

void Delay_SetFeedback(uint8_t unused, float value)
{
    delayFeedback = value;
    Status_ValueChangedFloat("DelayFeedback", value);
}

void Delay_SetLevel(uint8_t unused, float value)
{
    delayToMix = value;
    Status_ValueChangedFloat("DelayOutputLevel", value);
}

void Delay_SetLength(uint8_t unused, float value)
{
    delayLen = (uint32_t)(((float)MAX_DELAY - 1.0f) * value);
    Status_ValueChangedFloat("DelayLenMs", delayLen * (1000.0f / ((float)SAMPLE_RATE)));
}
