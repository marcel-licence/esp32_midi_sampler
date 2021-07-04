/*
 * this file includes all required function to setup and drive the i2s interface
 *
 * Author: Marcel Licence
 */

#include <driver/i2s.h>


/*
 * no dac not tested within this code
 * - it has the purpose to generate a quasy analog signal without a DAC
 */
//#define I2S_NODAC


const i2s_port_t i2s_port_number = I2S_NUM_0;


bool i2s_write_sample_32ch2(uint8_t *sample);

bool i2s_write_sample_32ch2(uint8_t *sample)
{
    static size_t bytes_written = 0;
    static size_t bytes_read = 0;
    i2s_read(i2s_port_number, (char *)sample, 8, &bytes_read, portMAX_DELAY);

    i2s_write(i2s_port_number, (const char *)sample, 8, &bytes_written, portMAX_DELAY);

    if (bytes_written > 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

#ifdef SAMPLE_SIZE_24BIT

bool i2s_write_sample_24ch2(uint8_t *sample);

bool i2s_write_sample_24ch2(uint8_t *sample)
{
    static size_t bytes_written1 = 0;
    static size_t bytes_written2 = 0;
    i2s_write(i2s_port_number, (const char *)&sample[1], 3, &bytes_written1, portMAX_DELAY);
    i2s_write(i2s_port_number, (const char *)&sample[5], 3, &bytes_written2, portMAX_DELAY);

    if ((bytes_written1 + bytes_written2) > 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

#endif

bool i2s_write_stereo_samples(float *fl_sample, float *fr_sample)
{

#ifdef SAMPLE_SIZE_24BIT
#if 0
    static union sampleTUNT
    {
        uint8_t sample[8];
        int32_t ch[2];
    } sampleDataU;
#else
    static union sampleTUNT
    {
        int32_t ch[2];
        uint8_t bytes[8];
    } sampleDataU;
#endif
#endif
#ifdef SAMPLE_SIZE_16BIT
    static union sampleTUNT
    {
        uint32_t sample;
        int16_t ch[2];
    } sampleDataU;
#endif

    /*
     * using RIGHT_LEFT format
     */
    sampleDataU.ch[0] = int16_t(*fr_sample * 16383.0f);
    sampleDataU.ch[1] = int16_t(*fl_sample * 16383.0f);

    static size_t bytes_written = 0;

    i2s_write(i2s_port_number, (const char *)&sampleDataU.sample, 4, &bytes_written, portMAX_DELAY);

    if (bytes_written > 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool i2s_write_stereo_samples_buff(float *fl_sample, float *fr_sample, const int buffLen)
{
#ifdef SAMPLE_SIZE_24BIT
#if 0
    static union sampleTUNT
    {
        uint8_t sample[8];
        int32_t ch[2];
    } sampleDataU[SAMPLE_BUFFER_SIZE];
#else
    static union sampleTUNT
    {
        int32_t ch[2];
        uint8_t bytes[8];
    } sampleDataU[SAMPLE_BUFFER_SIZE];
#endif
#endif
#ifdef SAMPLE_SIZE_16BIT
    static union sampleTUNT
    {
        uint32_t sample;
        int16_t ch[2];
    } sampleDataU[SAMPLE_BUFFER_SIZE];
#endif

    for (int n = 0; n < buffLen; n++)
    {
        /*
         * using RIGHT_LEFT format
         */
        sampleDataU[n].ch[0] = int16_t(fr_sample[n] * 16383.0f);
        sampleDataU[n].ch[1] = int16_t(fl_sample[n] * 16383.0f);
    }

    static size_t bytes_written = 0;

    i2s_write(i2s_port_number, (const char *)&sampleDataU[0].sample, 4 * buffLen, &bytes_written, portMAX_DELAY);

    if (bytes_written > 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void i2s_read_stereo_samples(float *fl_sample, float *fr_sample)
{
    static size_t bytes_read = 0;

    static union
    {
        uint32_t sample;
        int16_t ch[2];
    } sampleData;


    i2s_read(i2s_port_number, (char *)&sampleData.sample, 4, &bytes_read, portMAX_DELAY);

    //sampleData.ch[0] &= 0xFFFE;
    //sampleData.ch[1] &= 0;

    /*
     * using RIGHT_LEFT format
     */
    *fr_sample = ((float)sampleData.ch[0] * (5.5f / 65535.0f));
    *fl_sample = ((float)sampleData.ch[1] * (5.5f / 65535.0f));
}

void i2s_read_stereo_samples_buff(float *fl_sample, float *fr_sample, const int buffLen)
{
    static size_t bytes_read = 0;

    static union
    {
        uint32_t sample;
        int16_t ch[2];
    } sampleData[SAMPLE_BUFFER_SIZE];

    i2s_read(i2s_port_number, (char *)&sampleData[0].sample, 4 * SAMPLE_BUFFER_SIZE, &bytes_read, portMAX_DELAY);

    //sampleData.ch[0] &= 0xFFFE;
    //sampleData.ch[1] &= 0;

    for (int n = 0; n < SAMPLE_BUFFER_SIZE; n++)
    {
        /*
         * using RIGHT_LEFT format
         */
        fr_sample[n] = ((float)sampleData[n].ch[0] * (5.5f / 65535.0f));
        fl_sample[n] = ((float)sampleData[n].ch[1] * (5.5f / 65535.0f));
    }
}


i2s_config_t i2s_configuration =
{
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX), // | I2S_MODE_DAC_BUILT_IN
    .sample_rate = SAMPLE_RATE,
#ifdef SAMPLE_SIZE_24BIT
    .bits_per_sample = I2S_BITS_PER_SAMPLE_24BIT, /* the DAC module will only take the 8bits from MSB */
#endif
#ifdef SAMPLE_SIZE_16BIT
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, /* the DAC module will only take the 8bits from MSB */
#endif
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = 0, // default interrupt priority
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = 0
};


i2s_pin_config_t pins =
{
    .bck_io_num = IIS_SCLK,
    .ws_io_num =  IIS_LCLK,
    .data_out_num = IIS_DSIN,
    .data_in_num = IIS_DSOUT
};


void setup_i2s()
{
    i2s_driver_install(i2s_port_number, &i2s_configuration, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pins);
    i2s_set_sample_rates(i2s_port_number, SAMPLE_RATE);
    i2s_start(i2s_port_number);
}
