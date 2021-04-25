/*
 * this file is used to precalculate a sine
 * sine is very slow and this can be used to speed up the execution
 *
 * Author: Marcel Licence
 */

#define SINE_BIT	8UL
#define SINE_CNT	(1<<SINE_BIT)
#define SINE_MSK	((1<<SINE_BIT)-1)
#define SINE_I(i)	((i) >> (32 - SINE_BIT)) /* & SINE_MSK */


float *sine = NULL;

void Sine_Init(void)
{
    sine = (float *)malloc(sizeof(float) * SINE_CNT);
    if (sine == NULL)
    {
        Serial.printf("not enough heap memory for sine buffer!\n");
    }
    for (int i = 0; i < SINE_CNT; i++)
    {
        sine[i] = (float)sin(i * 2.0 * PI / SINE_CNT);
    }
}

/*
 * returns sin(value * 2.0f * PI) from lookup
 */
float SineNorm(float alpha_div2pi)
{
    uint32_t index = ((uint32_t)(alpha_div2pi * ((float)SINE_CNT))) % SINE_CNT;
    return sine[index];
}
