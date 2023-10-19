/*
 * Copyright (c) 2023 Marcel Licence
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
 * this file contains the implementation of the sampling core
 * a big PSRAM buffer will be allocated
 * you can record to the buffer and playback samples
 * MIDI ch1 noteOn message will trigger different samples
 * MIDI ch2-16 noteOn will trigger each a certain sample with different pitch
 *
 * setting loop start/end is only a rough and crappy implementation
 * - variable/functions name are confusing​
 *
 * Author: Marcel Licence
 */

#ifdef __CDT_PARSER__
#include <cdt.h>
#endif

/* using exp release curve would never reach 0 a defined limit is required */
#define AUDIBLE_LIMIT   (0.25f/32768.0f)

#define SAMPLE_MAX_RECORDS  32

#define NOTE_NORMAL 69 /* this is an a -> playback a with original recorded speed */

#define SAMPLE_MAX_PLAYERS  8 /* max polyphony, higher values 'may' not be processed in time */

#define MAX_FILENAME_LENGTH 64

/*
 * little helpers
 */
#ifndef absf
#define absf(a) ((a>=0.0f)?(a):(-a))
#endif

#ifndef absI
#define absI(a) ((a >= 0)?(a):(-a))
#endif

#define maxI(a, b)  (a>b)?(a):(b)

enum samplStatusE
{
    sampler_idle,
    sampler_rec,
    sampler_recWait,
    sampler_measureThreshold,
};

enum sampleADSR
{
    adsr_attack,
    adsr_decay,
    adsr_sustain,
    adsr_release,
};

/*
 * parameters for each sample
 */
struct sample_record_s
{
    uint32_t start;
    uint32_t end;
    uint8_t channels;
    bool valid;
    float loop_end;
    float loop_start;
    float pitch;

    float attack;
    float decay;
    float sustain;
    float release;

    char filename[MAX_FILENAME_LENGTH];
};

struct sample_player_s
{
    struct sample_record_s *sample_rec;
    int32_t pos;
    float pos_f;
    float pitch;
    bool playing;
    bool pressed;
    uint8_t ch;
    uint8_t note;
    int normNote;
    float slow;

    float velocity;
    float adsr_gain;
    enum sampleADSR adsr_state;
};


struct sample_record_s sampleRecords[SAMPLE_MAX_RECORDS];
struct sample_player_s samplePlayers[SAMPLE_MAX_PLAYERS];

void (*sampler_recordDoneCb)(void) = NULL;

uint32_t sampleRecordCount = 0; /*!< count of samples in buffer and valid sampleRecords */
uint32_t sampleStorageInPos = 0; /*!< next free sample in sampleStorage */
uint32_t sampleStorageLen = 0; /*!< max len of samples storage */
int16_t *sampleStorage = NULL; /*!< here is were the audio data will be stored */

bool samplerManualRecord = false; /*!< manual record avoids stopping the record by threshold */

enum samplStatusE sampleStatus = sampler_idle;

/* vu meter pointer values */
static float *vuStoreLen;
static float *vuThrInput;
static float *vuAbsInput;
static float *vuSlwInput;

float samplerThreshold = 0.02f;

float inputMonoAbs = 0.0f;
float inputMaxFiltered = 0;

uint32_t lastIn = 0;

bool loop_param_lock = false; /*!< ignore changes of loop start/end when set to true - required for nervous MIDI controllers */

float loop_start_c = 0;
float loop_start_f = 0;

float loop_end_c = 0;
float loop_end_f = 0;
float loop_end_mul = 0.0f;

float modulationDepth = 0.0f;
float modulationSpeed = 5.0f; // 7 maybe better?
float modulationPitch = 1.0f;
float pitchBendValue = 0.0f;

#ifdef AS5600_ENABLED
struct sample_record_s scratchRec;
#endif

struct sample_record_s *lastActiveRec = NULL;

/*
 * used for looped playback
 */

struct sample_player_s *beatPlayer = NULL;

uint8_t sampler_lastCh = 0xFF;
uint8_t sampler_lastNote = 0xFF;

void Sampler_Init(int16_t *storage, uint32_t storageLen)
{
    /* remember storage pointer and length */
    sampleStorage = storage;
    sampleStorageLen = storageLen;

    for (int i = 0; i < SAMPLE_MAX_PLAYERS; i++)
    {
        samplePlayers[i].sample_rec = &sampleRecords[0];
        samplePlayers[i].playing = false;
        samplePlayers[i].slow = 0.0f;
    }

    for (int i = 0; i < SAMPLE_MAX_RECORDS; i++)
    {
        sampleRecords[i].pitch = 1.0f;
        sampleRecords[i].release = 1.0f;
        sampleRecords[i].decay = 1.0f;
        sampleRecords[i].attack = 1.0f;
        sampleRecords[i].sustain = 1.0f;
    }

    vuStoreLen = VuMeterMatrix_GetPtr(2);
    vuThrInput = VuMeterMatrix_GetPtr(4);
    vuAbsInput = VuMeterMatrix_GetPtr(5);
    vuSlwInput = VuMeterMatrix_GetPtr(3);
}

float Modulation(void)
{
    float modSpeed = modulationSpeed;
    return modulationDepth * modulationPitch * (SineNorm((modSpeed * ((float)millis()) / 1000.0f)));
}

float FrequencyFromVoice(struct sample_record_s *const voice, float note)
{
#if 0
    float noteA = voice->oldNote;
    uint8_t noteB = voice->activeNote;
    float port = voice->port;
    float note = (((float)(noteA)) * (1.0f - port) + ((float)(noteB)) * port) - 69.0f + pitchBendValue + Modulation();
#endif

    note += pitchBendValue + Modulation();
    float f = ((pow(2.0f, note / 12.0f))); /* no frequency so dont use * 440.0f */
    return f;
}

inline
void Sampler_ProcessADSR(struct sample_player_s *player)
{
    switch (player->adsr_state)
    {
    case adsr_attack:
        /* multiplier sounds bad so we should increase gain linear here */
        player->adsr_gain += player->sample_rec->attack;
        if (player->adsr_gain >= 1.0f)
        {
            player->adsr_gain = 1.0f;
            player->adsr_state = adsr_decay;
        }
        break;
    case adsr_decay:
        player->adsr_gain *= player->sample_rec->decay;
        if (player->adsr_gain <= player->sample_rec->sustain)
        {
            player->adsr_state = adsr_sustain;
            player->adsr_gain = player->sample_rec->sustain; /* avoid undershoot */
        }
        break;
    case adsr_sustain:
        player->adsr_gain = player->sample_rec->sustain;
        break;
    case adsr_release:
        player->adsr_gain *= player->sample_rec->release;
        break;
    }
}

void Sampler_Process(float *signal_l, float *signal_r, const int buffLen)
{
    *vuStoreLen = ((float)sampleStorageInPos) / ((float)sampleStorageLen);
    *vuThrInput = samplerThreshold;

    for (int n = 0; n < buffLen; n++)
    {

        inputMonoAbs = max(absf(signal_l[n]), absf(signal_r[n]));

        *vuAbsInput = inputMonoAbs;

        switch (sampleStatus)
        {
        case sampler_measureThreshold:
            if (inputMonoAbs > samplerThreshold)
            {
                samplerThreshold = inputMonoAbs;
            }

            break;
        case sampler_rec:
        case sampler_recWait:
        case sampler_idle:
            /* no action */
            break;
        }

        if (sampleStatus == sampler_recWait)
        {
            if ((inputMonoAbs > samplerThreshold))
            {
                Sampler_RecordStart();
                inputMaxFiltered = 1.0f;
            }
        }

        inputMaxFiltered = max(inputMonoAbs, inputMaxFiltered);
        inputMaxFiltered *= 0.9996843825158444074f; /* same as 0.98 every 64 frames = (pow(1/64) */

        *vuSlwInput = inputMaxFiltered;

        if (sampleStatus == sampler_rec)
        {
            static uint32_t recEndTimeout = 0;
            int16_t s16 = 0U;
            s16 += (int16_t)(((float)0x8000) * signal_l[n]);
            //s16 += (((float)0x8000) * signal_r[n]);
            sampleStorage[sampleStorageInPos] = s16;
            sampleStorageInPos++;
            if (sampleStorageInPos >= sampleStorageLen)
            {
                Status_TestMsg("PSRAM memory full!");
                Sampler_RecordStop();
            }

            if ((inputMaxFiltered < samplerThreshold) && (samplerManualRecord == false))
            {
                recEndTimeout ++;
            }
            else
            {
                recEndTimeout = 0;
            }

            if (recEndTimeout > 11025) /* record 250ms after signal went under the threshold */
            {
                Status_TestMsg("Stopped by low threshold!");
                Sampler_RecordStop();
            }
        }

#if 0
        /* no pass through */
        signal_l[n] = slowL;
        signal_r[n] = slowR;

        slowL *= 0.99f;
        slowR *= 0.99f;
#endif

#ifdef SAMPLER_PASS_TROUGH_DURING_RECORD
        if (sampleStatus == sampler_idle)
#endif
#ifndef SAMPLER_ALWAYS_PASS_THROUGH
        {
            /* make quiet to prepare for next step */
            signal_l[n] = 0.0f;
            signal_r[n] = 0.0f;
        }
#endif
    }

    for (int i = 0; i < SAMPLE_MAX_PLAYERS; i++)
    {
        struct sample_player_s *player = &samplePlayers[i];

        /* calc pitch only once per buffer */
        player->pitch = player->sample_rec->pitch * FrequencyFromVoice(player->sample_rec, player->normNote);

        for (int n = 0; n < buffLen; n++)
        {
            signal_l[n] += player->slow;
            signal_r[n] += player->slow;
            player->slow *= 0.99f;

            if (player->playing)
            {
                float f2 = player->pos_f;
                float f1 = 1.0f - f2;

                float sample_f = 0;
                sample_f += f1 * ((float)sampleStorage[player->pos]) / ((float)0x8000);
                sample_f += f2 * ((float)sampleStorage[player->pos + 1]) / ((float)0x8000);

                sample_f *= player->velocity;
                /*
                 * adsr
                 */
                sample_f *= player->adsr_gain;

                Sampler_ProcessADSR(player);

                if ((player->pos_f == 0.0f) && (player->pos == 0))
                {
                    player->slow -= sample_f;
                }

                signal_l[n] += sample_f;
                signal_r[n] += sample_f;

                /* move to next sample */
                int32_t pitch_u = player->pitch;
                player->pos_f += player->pitch - pitch_u; /* does not work great when pos_f is bigger */
                player->pos += pitch_u;

                int posI = player->pos_f;
                player->pos += posI;
                player->pos_f -= posI;

                if (player->pressed)
                {
                    float sampleLen = player->sample_rec->loop_end - player->sample_rec->loop_start;
                    /*
                     * sampleLen = 34896
                     *
                     */
                    if (player->pos - ((float)player->sample_rec->start) >= (player->sample_rec->loop_end))
                    {
                        uint32_t sampleLenU = sampleLen;
                        player->pos -= sampleLenU;
                        player->pos_f -= sampleLen - sampleLenU;
                    }
#ifdef AS5600_ENABLED
                    if (player->pos - ((float)player->sample_rec->start) < player->sample_rec->loop_start)
                    {
                        uint32_t sampleLenU = sampleLen;
                        player->pos += sampleLenU;
                        player->pos_f += sampleLen - sampleLenU;
                    }
#endif
                }

                /* stop playback when end has been reached */
                /* stop playback when signal is not audible anymore */
                if ((player->pos >= player->sample_rec->end) || (player->adsr_gain < AUDIBLE_LIMIT))
                {
                    player->playing = false;
                    player->slow += sample_f;
                }
            }
        }
    }

    for (int n = 0; n < buffLen; n++)
    {
        /*
         * make it a bit quieter to avoid distortion in next stage
         */
        signal_l[n] *= 0.25f;
        signal_r[n] *= 0.25f;
    }
}

struct sample_player_s *getFreeSamplePlayer(void)
{
    for (int i = 0; i < SAMPLE_MAX_PLAYERS; i++)
    {
        if (samplePlayers[i].playing == false)
        {
            return &samplePlayers[i];
        }
    }

    struct sample_player_s *quietestPlayer = NULL;
    float lowestGain = 1.0f;

    for (int i = 0; i < SAMPLE_MAX_PLAYERS; i++)
    {
        if (samplePlayers[i].adsr_gain <= lowestGain)
        {
            quietestPlayer = &samplePlayers[i];
            lowestGain = quietestPlayer->adsr_gain;
        }
    }

    return quietestPlayer;
}


void Sampler_StartSamplePlayer(struct sample_player_s *player, struct sample_record_s *rec)
{
    player->sample_rec = rec;
    player->pos = player->sample_rec->start;
    player->pos_f = 0.0f;
    player->playing = true;
    player->pressed = true;
    player->pitch = player->sample_rec->pitch;


    player->normNote = 0;

    player->adsr_gain = AUDIBLE_LIMIT;
    player->adsr_state = adsr_attack;
    Sampler_ProcessADSR(player);
}

inline struct sample_player_s *Sampler_NoteOnInt(uint8_t ch, uint8_t note, float vel)
{
    struct sample_player_s *freePlayer = NULL;
    if (ch == 0)
    {
        freePlayer = getFreeSamplePlayer();
        struct sample_record_s *rec = &sampleRecords[note % sampleRecordCount];

        if (freePlayer != NULL)
        {
            Sampler_StartSamplePlayer(freePlayer, rec);

            freePlayer->ch = ch;
            freePlayer->note = note;
            freePlayer->velocity = vel;

            lastActiveRec = freePlayer->sample_rec;
        }
    }
    else
    {
        freePlayer = getFreeSamplePlayer();
        struct sample_record_s *rec = &sampleRecords[(ch - 1) % sampleRecordCount]; /* decrease by one because we want to start with the first sample here */

        if (freePlayer != NULL)
        {
            Sampler_StartSamplePlayer(freePlayer, rec);

            freePlayer->ch = ch;
            freePlayer->note = note;
            freePlayer->velocity = vel;
            freePlayer->normNote = note - NOTE_NORMAL;
            freePlayer->pitch *= pow(2.0f, 1.0f / 12.0f * (note - NOTE_NORMAL)); /* this would be the a as middle */

            lastActiveRec = freePlayer->sample_rec;
        }
    }

    return freePlayer;
}

void Sampler_NoteOn(uint8_t ch, uint8_t note, float vel)
{
    if (sampleRecordCount == 0)
    {
        return;
    }

    sampler_lastCh = ch;
    sampler_lastNote = note;

    (void *)Sampler_NoteOnInt(ch, note, vel);
}

void Sampler_NoteOff(uint8_t ch, uint8_t note)
{
    for (int i = 0; i < SAMPLE_MAX_PLAYERS; i++)
    {
        if ((samplePlayers[i].ch == ch) && (samplePlayers[i].note == note))
        {
            samplePlayers[i].pressed = false;
            samplePlayers[i].adsr_state = adsr_release;
        }
    }
}

void Sampler_RecordStop(void)
{
    if (sampleStatus == sampler_rec)
    {
        if (sampleStorageInPos > sampleRecords[sampleRecordCount].start)
        {
            sampleRecords[sampleRecordCount].valid = true;
            sampleRecords[sampleRecordCount].end = sampleStorageInPos - 1; /* pointing to last sample */
            sampleRecordCount += 1;
        }
        sampleStatus = sampler_idle;
        Status_TestMsg("Recording done!");

        struct sample_record_s *tempRec = lastActiveRec;
        lastActiveRec = &sampleRecords[sampleRecordCount - 1];
        Sampler_NormalizeActiveRecording(0, 1);
        lastActiveRec = tempRec;

        if (sampler_recordDoneCb != NULL)
        {
            sampler_recordDoneCb();
        }
    }
    else if (sampleStatus == sampler_recWait)
    {
        Status_TestMsg("Recording cancelled!");
        sampleStatus = sampler_idle;
    }
}

void Sampler_RecordStart(void)
{
    sampleRecords[sampleRecordCount].start = sampleStorageInPos;
    sampleStatus = sampler_rec;
    Status_TestMsg("Recording started..");
}

void Sampler_MeasureThreshold(uint8_t quarter, float value)
{
    if ((value > 0) && (sampleStatus == sampler_idle))
    {
        sampleStatus = sampler_measureThreshold;
        samplerThreshold = 0.0f;
        Status_TestMsg("Measuring threshold started..");
    }

    if ((value == 0) && (sampleStatus == sampler_measureThreshold))
    {
        sampleStatus = sampler_idle;
        Status_ValueChangedFloat("SamplerThreshold", samplerThreshold);
    }
}

void Sampler_Record(uint8_t quarter, float value)
{
    if (value > 0)
    {
        if (sampleRecordCount < SAMPLE_MAX_RECORDS)
        {
            samplerManualRecord = true;
            if (sampleStatus != sampler_rec)
            {
                Sampler_RecordStart();
            }
        }
        else
        {
            Status_TestMsg("No free sample handlers available!");
        }
    }
    else
    {
        samplerManualRecord = false;
        Sampler_RecordStop();
    }
}

void Sampler_Stop(uint8_t quarter, float value)
{
    if (value > 0)
    {
        samplerManualRecord = false;
        Sampler_RecordStop();

        if (beatPlayer != NULL)
        {
            Sampler_NoteOff(beatPlayer->ch, beatPlayer->note);
        }
    }
}

void Sampler_Play(uint8_t unused, float value)
{
    if (value > 0)
    {
        if (sampler_lastCh != 0xFF)
        {
            struct sample_record_s *tempActiveRec = lastActiveRec;
            beatPlayer = Sampler_NoteOnInt(sampler_lastCh, sampler_lastNote, 1);
            lastActiveRec = tempActiveRec;
        }
    }
}

#ifdef AS5600_ENABLED
struct sample_player_s *scratchPlayer = NULL;

void Sampler_SetScratchSample(uint8_t selSample, float value)
{
    if (value > 0)
    {
        Sampler_NoteOff(0xEE, 0xEE);

        if (selSample == 0xFF)
        {
            /* use last sample for scratching */
            memcpy(&scratchRec, lastActiveRec, sizeof(scratchRec));
        }
        else
        {
            /* use selected sample for scratching */
            memcpy(&scratchRec, &sampleRecords[selSample % sampleRecordCount], sizeof(scratchRec));
        }

        float sampleLen = (scratchRec.end - scratchRec.start);

        scratchRec.loop_start = 0;
        scratchRec.loop_end = sampleLen - 1;
        scratchRec.release = 0;
        scratchRec.pitch = 0;

        {
            scratchPlayer = getFreeSamplePlayer();
            struct sample_record_s *rec = &scratchRec;

            if (scratchPlayer != NULL)
            {
                Sampler_StartSamplePlayer(scratchPlayer, rec);

                scratchPlayer->ch = 0xEE;
                scratchPlayer->note = 0xEE;
                scratchPlayer->velocity = 1;
#ifdef DISPLAY_160x80_ENABLED
                Display_SetFullText(scratchRec.filename);
                Display_Redraw();
#endif
            }
        }
    }
}

void Sampler_ScratchFader(uint8_t unused, float value)
{
    float vol = /*1.0f -*/ value;
    vol *= 5;
    if (vol > 1)
    {
        vol = 1;
    }
    if (scratchPlayer != NULL)
    {
        scratchPlayer->velocity = vol;
    }

    vol = 1.0f - value;
    vol *= 5;
    if (vol > 1)
    {
        vol = 1;
    }

    if (beatPlayer != NULL)
    {
        beatPlayer->velocity = vol;
    }
}
#endif

void Sampler_LoopStartC(uint8_t quarter, float value)
{
    if (loop_param_lock)
    {
        return;
    }

    loop_start_c = (10000.0f * value);
    Sampler_UpdateLoopRange();
    if (lastActiveRec != NULL)
    {
        Status_ValueChangedFloat("loop_start", lastActiveRec->loop_start);
    }
}

void Sampler_LoopStartF(uint8_t quarter, float value)
{
    if (loop_param_lock)
    {
        return;
    }

    loop_start_f = value * 100.f;
    Sampler_UpdateLoopRange();
    if (lastActiveRec != NULL)
    {
        Status_ValueChangedFloat("loop_start", lastActiveRec->loop_start);
    }
}

void Sampler_LoopEndC(uint8_t quarter, float value)
{
    if (loop_param_lock)
    {
        return;
    }

    /*
     * length range one octave
     * 0 -> 1
     * 0.5 -> 1.4142... = sqrt(2)
     * 1 -> 2
     */
    float samples_a440 = (1.0f / 440.0f) * 44100.0f;

    samples_a440 *= 1.0f / pow(2.0f, (value * 8.0) - 4.0f);

    loop_end_c = samples_a440;
    loop_end_c = 1.0f / pow(2.0f, (value / 0.5f)); /* two octaves */
    Sampler_UpdateLoopRange();
    if (lastActiveRec != NULL)
    {
        Status_ValueChangedFloat("loop_end", lastActiveRec->loop_end);
    }
}

void Sampler_LoopEndF(uint8_t quarter, float value)
{
    if (loop_param_lock)
    {
        return;
    }

    loop_end_f = 1.0f / pow(2.0f, (value / 24.0f)); /* quarter note */
    Sampler_UpdateLoopRange();
    if (lastActiveRec != NULL)
    {
        Status_ValueChangedFloat("loop_end", lastActiveRec->loop_end);
    }
}

void Sampler_LoopAll(uint8_t index, float value)
{
    if ((value > 0) && (lastActiveRec != NULL))
    {
        float sampleLen = (lastActiveRec->end - lastActiveRec->start);

        lastActiveRec->loop_start = 0;
        lastActiveRec->loop_end = sampleLen - 1;

        Status_ValueChangedFloat("len", lastActiveRec->end - lastActiveRec->start);
        Status_ValueChangedFloat("loop_start", lastActiveRec->loop_start);
        Status_ValueChangedFloat("loop_end", lastActiveRec->loop_end);
    }
}

void Sampler_LoopLock(uint8_t not_used, float value)
{
    if (value > 0)
    {
        Status_TestMsg("Parameters now locked!");
        loop_param_lock = true;
    }
}

void Sampler_LoopUnlock(uint8_t not_used, float value)
{
    if (value > 0)
    {
        Status_TestMsg("Parameters now unlocked!");
        loop_param_lock = false;
    }
}

void Sampler_LoopRemove(uint8_t index, float value)
{
    if ((value > 0) && (lastActiveRec != NULL))
    {
        lastActiveRec->loop_start = lastActiveRec->end;
        lastActiveRec->loop_end = lastActiveRec->end;
    }
}

void Sampler_SetLoopEndMultiplier(uint8_t quarter, float value)
{
    int loop_end_mul_i = 1 + ((value) * 127.0f); /* 128 octaves */
    loop_end_mul = loop_end_mul_i;
    Sampler_UpdateLoopRange();
    if (lastActiveRec != NULL)
    {
        Status_ValueChangedFloat("loop_end", lastActiveRec->loop_end);
    }
}

void Sampler_SetPitch(uint8_t quarter, float value)
{
    if (lastActiveRec != NULL)
    {
        lastActiveRec->pitch = 0.5f * pow(2.0f, 2.0f * value);
        Status_ValueChangedFloat("pitch", lastActiveRec->pitch);
    }
}

#ifdef AS5600_ENABLED
void Sampler_SetPitchAbs(float value)
{
    scratchRec.pitch = value;
}
#endif

void Sampler_UpdateLoopRange(void)
{
    if (lastActiveRec != NULL)
    {
        float loop_len = 440 * loop_end_mul * loop_end_c * loop_end_f;
        if (loop_len >= lastActiveRec->end - lastActiveRec->start)
        {
            loop_len = lastActiveRec->end - lastActiveRec->start;
        }
        lastActiveRec->loop_start = loop_start_c + loop_start_f;
        lastActiveRec->loop_end = loop_len + lastActiveRec->loop_start;
        lastActiveRec->loop_start = min(lastActiveRec->loop_start, lastActiveRec->loop_end);
    }
}

void Sampler_RecordWait(uint8_t quarter, float value)
{
    if (value > 0)
    {
        if (sampleRecordCount < SAMPLE_MAX_RECORDS)
        {
            Status_TestMsg("Wait to record...");
            sampleStatus = sampler_recWait;
        }
        else
        {
            Status_TestMsg("No free sample handlers available!");
        }
    }
}

void Sampler_ModulationWheel(uint8_t ch, float value)
{
    modulationDepth = value;
}

void Sampler_ModulationSpeed(uint8_t ch, float value)
{
    modulationSpeed = value * 10;
    Status_ValueChangedFloat("ModulationSpeed", modulationSpeed);
}

void Sampler_ModulationPitch(uint8_t ch, float value)
{
    modulationPitch = value * 5;
    Status_ValueChangedFloat("ModulationDepth", modulationPitch);
}

void Sampler_PitchBend(uint8_t ch, float bend)
{
    pitchBendValue = bend;
}

void Sampler_Panic(uint8_t ch, float value)
{
    for (int i = 0; i < SAMPLE_MAX_PLAYERS; i++)
    {
        if ((samplePlayers[i].ch != 0xEE) && (samplePlayers[i].note != 0xEE)) /* do not kill scratch sample */
        {
            if (samplePlayers[i].playing)
            {
                /* show information about stuck notes */
                Serial.printf("KillNote %d\n", i);
            }
            samplePlayers[i].pressed = false;
            samplePlayers[i].playing = false;
            Status_TestMsg("Panic! All notes off...");
        }
    }
}

void Sampler_NormalizeActiveRecording(uint8_t unused, float value)
{
    if ((value > 0) && (lastActiveRec != NULL))
    {
        int16_t maxLvl = 0;

        Status_TestMsg("Normalizing sample...");

        for (int i = lastActiveRec->start; i < lastActiveRec->end; i++)
        {
            maxLvl = maxI(absI(sampleStorage[i]), maxLvl);
        }

        float gainMultiplier = ((float)32767) / ((float)maxLvl);

        for (int i = lastActiveRec->start; i < lastActiveRec->end; i++)
        {
            sampleStorage[i] = ((float)sampleStorage[i]) * gainMultiplier;
        }

        Status_TestMsg("Sample normalized");
    }
}

void Sampler_RemoveActiveRecording(uint8_t unused, float value)
{
    if ((value > 0) && (lastActiveRec != NULL))
    {
        if (lastActiveRec->valid == false)
        {
            Status_TestMsg("No record selected for removal");
            return;
        }
        Status_TestMsg("Erasing record...");
        uint32_t recordLength = lastActiveRec->end - lastActiveRec->start;
        for (int i = lastActiveRec->start; i + recordLength < sampleStorageLen; i++)
        {
            sampleStorage[i] = sampleStorage[i + recordLength];
        }
        sampleStorageInPos -= recordLength;

        for (int i = 0; i < sampleRecordCount; i++)
        {
            if (sampleRecords[i].start > lastActiveRec->start)
            {
                sampleRecords[i].start -= recordLength;
                sampleRecords[i].end -= recordLength;
            }
        }

        lastActiveRec->valid = false;

        if (sampleRecordCount > 1)
        {
            memcpy(lastActiveRec, &sampleRecords[sampleRecordCount - 1], sizeof(struct sample_record_s));
            sampleRecords[sampleRecordCount - 1].valid = false;
            sampleRecordCount -= 1;
        }

        lastActiveRec = NULL;
        Status_TestMsg("Record removed...");
    }
}

void Sampler_SavePatch(uint8_t quarter, float value)
{
    if (value > 0)
    {
        if (lastActiveRec != NULL)
        {
            struct patchParam_s patchParam;
            memset(&patchParam, 0, sizeof(patchParam));
            patchParam.version = 1;

            patchParam.patchParamV0.pitch = lastActiveRec->pitch;
            patchParam.patchParamV0.loop_start = lastActiveRec->loop_start;
            patchParam.patchParamV0.loop_end = lastActiveRec->loop_end;

            patchParam.patchParamV1.attack = lastActiveRec->attack;
            patchParam.patchParamV1.decay = lastActiveRec->decay;
            patchParam.patchParamV1.sustain = lastActiveRec->sustain;
            patchParam.patchParamV1.release = lastActiveRec->release;

            PatchManager_SaveNewPatch(&patchParam, &sampleStorage[lastActiveRec->start], lastActiveRec->end - lastActiveRec->start);
            //PatchManager_SaveNewPatch(lastActiveRec, sampleStorage);
        }
    }
}

void Sampler_LoadPatchFile(const char *filename)
{
    PatchManager_SetFilename(filename);
    Sampler_LoadPatch(0, 1);
}

void Sampler_LoadPatch(uint8_t unused, float value)
{
    if (value > 0)
    {
        struct patchParam_s patchParam;

        uint32_t newSampleLen = PatchManager_LoadPatch(&patchParam, &sampleStorage[sampleStorageInPos], sampleStorageLen - sampleStorageInPos);

        if (newSampleLen > 0)
        {
            struct sample_record_s *newPatch = &sampleRecords[sampleRecordCount];
            sampleRecordCount++;

            memcpy(newPatch->filename, patchParam.filename, sizeof(newPatch->filename));

            newPatch->pitch = patchParam.patchParamV0.pitch;
            newPatch->loop_start = patchParam.patchParamV0.loop_start;
            newPatch->loop_end = patchParam.patchParamV0.loop_end;

            if (patchParam.version >= 1)
            {
                newPatch->attack = patchParam.patchParamV1.attack;
                newPatch->decay = patchParam.patchParamV1.decay;
                newPatch->sustain = patchParam.patchParamV1.sustain;
                newPatch->release = patchParam.patchParamV1.release;
            }
            else
            {
                newPatch->attack = 1.0f;
                newPatch->decay = 1.0f;
                newPatch->sustain = 1.0f;
                newPatch->release = 1.0f;
            }

            newPatch->start = sampleStorageInPos;
            newPatch->end = newPatch->start + newSampleLen - 1; /* pointing to last sample */
            newPatch->valid = true;
            lastIn = sampleStorageInPos;
            sampleStorageInPos += newSampleLen;

            Status_ValueChangedInt("Loaded sample", newPatch->end - newPatch->start);

            /* select new recorded sample */
            lastActiveRec = newPatch;
        }
        else
        {
            lastActiveRec = NULL;
        }
    }
}

void Sampler_SetADSR_Attack(uint8_t not_used, float value)
{
    if (lastActiveRec != NULL)
    {
        float attackInv = pow(2.0f, value * 4) - 1.0f;
        Status_ValueChangedFloat("attackInv", attackInv);
        if (attackInv > 0.0f)
        {
            lastActiveRec->attack = 1 / (attackInv * 44100.0f);
        }
        else
        {
            lastActiveRec->attack = 1 / AUDIBLE_LIMIT;
        }
        Status_ValueChangedFloat("attack", lastActiveRec->attack);
    }
}

void Sampler_SetADSR_Decay(uint8_t not_used, float value)
{
    if (lastActiveRec != NULL)
    {
        lastActiveRec->decay = pow(value, 10.0f / 44100.0f); /* value controls the level after one tenth of a second */
        Status_ValueChangedFloat("decay", lastActiveRec->decay);
    }
}

void Sampler_SetADSR_Release(uint8_t not_used, float value)
{
    if (lastActiveRec != NULL)
    {
        lastActiveRec->release = pow(value, 10.0f / 44100.0f); /* value controls the level after one tenth of a second */
        Status_ValueChangedFloat("release", lastActiveRec->release);
    }
}

void Sampler_SetADSR_Sustain(uint8_t not_used, float value)
{
    if (lastActiveRec != NULL)
    {
        lastActiveRec->sustain = value;
        Status_ValueChangedFloat("sustain", lastActiveRec->sustain);
    }
}

void Sampler_SetRecordDoneCallback(void(*callback)(void))
{
    sampler_recordDoneCb = callback;
}

void Sampler_AddSection(float pitch_keycenter, uint32_t offset, uint32_t end, uint32_t loop_start, uint32_t loop_end, const char *soundName)
{
    struct sample_record_s *newPatch = &sampleRecords[sampleRecordCount];
    sampleRecordCount++;

    memset(newPatch->filename, 0, MAX_FILENAME_LENGTH);
    memcpy(newPatch->filename, soundName, strnlen(soundName, MAX_FILENAME_LENGTH));

    newPatch->pitch = ((pow(2.0f, (69.0f - ((float)pitch_keycenter)) / 12.0f)));
    newPatch->start = lastIn + offset;
    newPatch->end = lastIn + end;

    if ((offset == loop_start) && (end == (loop_end + 1)))
    {
        newPatch->loop_start = 0;
        newPatch->loop_end = 0xFFFFFFFF;
    }
    else
    {
        newPatch->loop_start = loop_start - offset;
        newPatch->loop_end = loop_end - offset;
    }

    newPatch->sustain = 1;
    newPatch->attack = 1;
    newPatch->decay = 1;
    newPatch->release = 0.99;

    newPatch->valid = true;
}

