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
 * screens.ino
 *
 *  Created on: 08.07.2021
 *      Author: Marcel Licence
 */

#ifdef SCREEN_ENABLED

struct screen_t
{
    struct screen_t *l_Screen;
    struct screen_t *r_Screen;
    void (*enter)(void);
    void (*btnCallback)(uint8_t key, uint8_t down);
    void (*offsetChanged)(int offset);
};

int scrCurMax = 0;
int scrCurSel = -1;
int scrOffset = 0;

struct screen_t ScrSampleParam =
{
    NULL,
    NULL,
    Screen_SampleEditDraw,
    NULL,
    NULL,
};

struct screen_t ScrStorage =
{
    NULL,
    NULL,
    Screen_StorageAccessDraw,
    Screen_StorageAccessBtn,
    NULL,
};

struct screen_t ScrSampleLoad =
{
    NULL,
    NULL,
    Screen_SampleLoadDraw,
    Screen_SampleLoadBtnCb,
    Screen_SampleLoadOffset,
};

struct screen_t myscreens[] =
{
    {
        NULL,
        NULL,
        Screen_SampleEditDraw,
        NULL,
        NULL,
    },

    {
        NULL,
        NULL,
        Screen_StorageAccessDraw,
        Screen_StorageAccessBtn,
        NULL,
    },

    {
        NULL,
        NULL,
        Screen_SampleLoadDraw,
        Screen_SampleLoadBtnCb,
        Screen_SampleLoadOffset,
    },

    {
        NULL,
        NULL,
        Screen_SampleRecordDraw,
        Screen_SampleRecordBtnCb,
        NULL,
    },

    {
        NULL,
        NULL,
        Screen_EffectsDraw,
        Screen_EffectsBtnCb,
        NULL,
    },
};

struct screen_t *currentScreen = &myscreens[0];

#define ARRAY_SIZE(a)   (sizeof(a)/sizeof(a[0]))

void Screen_Setup()
{
    currentScreen->enter();

    for (int i = 0; i < ARRAY_SIZE(myscreens); i++)
    {
        if (i > 0)
        {
            myscreens[i].l_Screen = &myscreens[i - 1];
        }
        if (i < ARRAY_SIZE(myscreens) - 1)
        {
            myscreens[i].r_Screen = &myscreens[i + 1];
        }
    }
}

char line[8][27];

void Screen_ClrLines(void)
{
    for (int i = 0; i < 8; i++)
    {
        memset(line[i], ' ', 26);
    }
}

void Screen_SetDisplayLines(void)
{
    for (int i = 0; i < 8; i ++)
    {
        Display_SetLine(i, line[i]);
    }
}

#define AUDIBLE_LIMIT   (0.25f/32768.0f)

void Screen_SampleEditDraw()
{
    Screen_ClrLines();

    Display_SetHeader("SampleEdit");

    if (lastActiveRec)
    {
        snprintf(line[0], 26, "attack: %0.3f", log(AUDIBLE_LIMIT) / log(lastActiveRec->attack));
        snprintf(line[1], 26, "decay: %0.3f", log(AUDIBLE_LIMIT) / log(lastActiveRec->decay));
        snprintf(line[2], 26, "sustain: %0.3f", log(AUDIBLE_LIMIT) / log(lastActiveRec->sustain));
        snprintf(line[3], 26, "release: %0.3f", log(AUDIBLE_LIMIT) / log(lastActiveRec->release));
    }
    else
    {
        snprintf(line[0], 26, "attack: N/A");
        snprintf(line[1], 26, "decay: N/A");
        snprintf(line[2], 26, "sustain: N/A");
        snprintf(line[3], 26, "release: N/A");
    }

    Screen_SetDisplayLines();
    Screen_SetLineCnt(4);

    Display_Redraw();
}



void Screen_StorageAccessDraw()
{
    Screen_ClrLines();

    Display_SetHeader("Storage");

    snprintf(line[0], 26, "fs: %s", (patchManagerDest == patch_dest_sd_mmc) ? "SD_MMC" : "LITTLEFS");
    snprintf(line[1], 26, "Save current sample");

    Screen_SetLineCnt(2);

    Screen_SetDisplayLines();
    Display_Redraw();
}

void Screen_StorageAccessBtn(uint8_t key, uint8_t down)
{
    {
        Serial.printf("Screen_StorageAccessBtn!\n");
    }

    switch (scrCurSel)
    {
    case 0:
        if (patchManagerDest == patch_dest_sd_mmc)
        {
            PatchManager_SetDestination(0, 1);
        }
        else
        {
            PatchManager_SetDestination(1, 1);
        }
        break;
    case 1:
        Sampler_SavePatch(0, 1);
        break;
    }
    Screen_StorageAccessDraw();
}

void Screen_EnterSampleLoaderFileInd(char *filename, int offset)
{
    if ((offset >= 0) && (offset < 8))
    {
        memset(line[offset], ' ', 26);
        memcpy(line[offset], filename, min(26, (int)strlen(filename)));
    }
}


bool sampleSel = true;
int sampleSelOffset = 0;

void Screen_SampleLoadDraw()
{
    Screen_ClrLines();

    Display_SetHeader("Sample Load");

    int filecnt = PatchManager_GetFileListExt(Screen_EnterSampleLoaderFileInd, sampleSelOffset) + sampleSelOffset;

    Screen_SetDisplayLines();
    Screen_SetLineCnt(filecnt);

    Display_Redraw();
    sampleSel = true;
}

void Screen_SampleLoadBtnCb(uint8_t key, uint8_t down)
{
    if (sampleSel)
    {
        {
            patch_selectedFileIndex = scrCurSel;
            PatchManager_UpdateFilename();
            Sampler_LoadPatch(0, 1);
            Screen_ClrLines();
            snprintf(line[0], 27, "File loaded to ch %d", sampleRecordCount);
            snprintf(line[1], 27, "  %s", lastSelectedFile);
            if (lastActiveRec != NULL)
            {
                snprintf(line[2], 27, "len: %d (%dms)", lastActiveRec->end - lastActiveRec->start, (lastActiveRec->end - lastActiveRec->start) * 1000 / 44100);
                snprintf(line[3], 27, "loop: %0.3f - %0.3f", lastActiveRec->loop_start, lastActiveRec->loop_end);
                snprintf(line[4], 27, "pitch: %0.3f", lastActiveRec->pitch);
                snprintf(line[5], 27, "a: %0.3f, d: %0.3f", lastActiveRec->attack, lastActiveRec->decay);
                snprintf(line[6], 27, "s: %0.3f, r: %0.3f", lastActiveRec->sustain, lastActiveRec->release);
                snprintf(line[7], 27, "%0.3f %% storage used", (float)sampleStorageInPos * 100.0f / (float)sampleStorageLen);
            }
            Screen_SetDisplayLines();
            Screen_SetLineCnt(0);
            Display_Redraw();
        }
    }
    else
    {
        sampleSel = true;
        Screen_SampleLoadDraw();
    }
}

void Screen_SampleLoadOffset(int offset)
{
    sampleSelOffset = offset;
    Screen_SampleLoadDraw();
}

enum recStateE
{
    recStateIdle,
    recStateWaitPrep,
    recStateWait,
    recStateDone,
};

enum recStateE recState = recStateIdle;

bool waitingRec = false;

void Screen_SampleRecordDraw(void)
{
    Screen_ClrLines();

    Display_SetHeader("Sample Record");

    snprintf(line[0], 27, "Input %s", selSource == acSrcMic ? "LineIn" : "Microphones");

    switch (recState)
    {
    case recStateIdle:
        snprintf(line[1], 27, "Record wait");

        Screen_SetLineCnt(2);
        break;
    case recStateWaitPrep:
        snprintf(line[1], 27, "Preparing for recording");
        snprintf(line[2], 27, "Press button to cancel");
        Screen_SetLineCnt(0);
        break;
    case recStateWait:
        snprintf(line[1], 27, "Waiting for signal");
        snprintf(line[2], 27, "Press button to cancel");
        Screen_SetLineCnt(0);
        break;
    case recStateDone:
        snprintf(line[1], 27, "Recording finished");
        if (lastActiveRec != NULL)
        {
            snprintf(line[2], 27, "len: %d (%dms)", lastActiveRec->end - lastActiveRec->start, (lastActiveRec->end - lastActiveRec->start) * 1000 / 44100);
            snprintf(line[3], 27, "loop: %0.3f - %0.3f", lastActiveRec->loop_start, lastActiveRec->loop_end);
            snprintf(line[4], 27, "pitch: %0.3f", lastActiveRec->pitch);
            snprintf(line[5], 27, "a: %0.3f, d: %0.3f", lastActiveRec->attack, lastActiveRec->decay);
            snprintf(line[6], 27, "s: %0.3f, r: %0.3f", lastActiveRec->sustain, lastActiveRec->release);
            snprintf(line[7], 27, "%0.3f %% storage used", (float)sampleStorageInPos * 100.0f / (float)sampleStorageLen);
        }
        Screen_SetLineCnt(0);
        break;
    }

    Screen_SetDisplayLines();

    Display_Redraw();
}

void Screen_SampleRecordBtnCb(uint8_t key, uint8_t down)
{
    if (down > 0)
    {
        switch (recState)
        {
        case recStateIdle:
            switch (scrCurSel)
            {
            case 0:
                App_ToggleSource(0, 1);
                Screen_SampleRecordDraw();
                break;
            case 1:
                Sampler_SetRecordDoneCallback(Screen_SampleRecordDoneDraw);
                recState = recStateWaitPrep;
                Screen_SampleRecordDraw();
                delay(3000);
                recState = recStateWait;
                Screen_SampleRecordDraw();
                Sampler_RecordWait(0, 1);
                break;
            }
            break;
        default:
            Sampler_Stop(0, 1);
            recState = recStateIdle;
            Screen_SampleRecordDraw();
            break;
        }
    }
}

float rev_level = 0; /* todo interface required */

#if 0 /* actually not availble */
extern float delayToMix;
extern float delayInLvl;
extern uint32_t delayLen;
extern float delayFeedback;
#else
float delayToMix = 0;
float delayInLvl = 0;
uint32_t delayLen = 0;
float delayFeedback = 0;
#endif

void Screen_EffectsDraw(void)
{
    Screen_ClrLines();

    Display_SetHeader("Effects");

    snprintf(line[0], 26, "reverb level: %0.3f", rev_level);
    snprintf(line[1], 26, "delay level: %0.3f", delayToMix);
    snprintf(line[2], 26, "delay input: %0.3f", delayInLvl);
    snprintf(line[3], 26, "delay time: %0.3fms", delayLen * (1000.0f / ((float)SAMPLE_RATE)));
    snprintf(line[4], 26, "delay feedback: %0.3f", delayFeedback);


    Screen_SetDisplayLines();
    Screen_SetLineCnt(4);

    Display_Redraw();
}

void Screen_EffectsBtnCb(uint8_t key, uint8_t down)
{

}

void Screen_SampleRecordDoneDraw(void)
{
    recState = recStateDone;
    Screen_SampleRecordDraw();
    Sampler_SetRecordDoneCallback(NULL);
}

#define BTN_D 0
#define BTN_U 2
#define BTN_L 3
#define BTN_R 1

void Screen_SetLineCnt(int cnt)
{
    scrCurMax = cnt;
}

void Screen_ButtonCallback(uint8_t key, uint8_t down)
{
    if (down > 0)
    {
        switch (key)
        {
        case BTN_U:
            if (scrCurSel > -1)
            {
                scrCurSel --;
            }
            scrOffset = max(0, scrCurSel - 7);
            Serial.printf("cur: %d, %d\n", scrCurSel, scrOffset);
            if (currentScreen->offsetChanged)
            {
                currentScreen->offsetChanged(scrOffset);
            }

            Display_SetCurSel(scrCurSel - scrOffset);
            Display_Redraw();
            break;
        case BTN_D:
            if (scrCurSel < scrCurMax - 1)
            {
                scrCurSel ++;
            }
            scrOffset = max(0, scrCurSel - 7);
            Serial.printf("cur: %d, %d\n", scrCurSel, scrOffset);

            if (currentScreen->offsetChanged)
            {
                currentScreen->offsetChanged(scrOffset);
            }

            Display_SetCurSel(scrCurSel - scrOffset);
            Display_Redraw();
            break;
        case BTN_R:
            if (scrCurSel == -1)
            {
                if (currentScreen->r_Screen)
                {
                    currentScreen = currentScreen->r_Screen;
                    currentScreen->enter();
                }
            }
            else
            {
                if (currentScreen->btnCallback)
                {
                    currentScreen->btnCallback(key, down);
                }
                else
                {
                    Serial.printf("no btn cb!\n");
                }
            }
            break;
        case BTN_L:
            if (scrCurSel == -1)
            {
                if (currentScreen->l_Screen)
                {
                    currentScreen = currentScreen->l_Screen;
                    currentScreen->enter();
                }
            }
            else
            {
                if (currentScreen->btnCallback)
                {
                    currentScreen->btnCallback(key, down);
                }
                else
                {
                    Serial.printf("no btn cb!\n");
                }
            }
            break;
        }
    }
}

static bool updateScreen = true;

void Screen_Update()
{
    updateScreen = true;
}

void Screen_Loop()
{
    if (updateScreen)
    {
        currentScreen->enter();
        updateScreen = false;
    }
}

#endif

