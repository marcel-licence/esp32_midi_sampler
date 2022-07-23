/*
 * Copyright (c) 2022 Marcel Licence
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
 * this file contains the implementation of the "patch" manager
 * It allows loading and saving the samples including parameters
 *
 * Saved data will appear as pairs: .wav and .bin
 * The parameters are in the .bin file
 *
 * Hot swap of sd card is supported. SD is busy only during read/write operation
 *
 * Support of loading wav files without parameters
 * - in case no parameters are existing default values are used
 *
 * only signed 16 bit wav files are supported at the moment
 * sampleRate is ignored -> you can pitch it afterwards
 *​
 * Author: Marcel Licence
 */


#ifdef __CDT_PARSER__
#include <cdt.h>
#endif


#include <FS.h>

#ifdef USE_SPIFFS_LEGACY

#include <SPIFFS.h> /* Using library SPIFFS at version 1.0 from https://github.com/espressif/arduino-esp32 */
#define LittleFS SPIFFS

#else /* USE_SPIFFS_LEGACY */

#include <FS.h>
#ifdef ARDUINO_RUNNING_CORE /* tested with arduino esp32 core version 2.0.2 */
#include <LittleFS.h> /* Using library LittleFS at version 2.0.0 from https://github.com/espressif/arduino-esp32 */
#else
#include <LITTLEFS.h> /* Using library LittleFS_esp32 at version 1.0.6 from https://github.com/lorol/LITTLEFS */
#define LittleFS LITTLEFS
#endif

#endif /* USE_SPIFFS_LEGACY */

#include <SD_MMC.h>


#define FST fs::FS


enum patchDst
{
    patch_dest_littlefs,
    patch_dest_sd_mmc,
};

struct patchParamV0_s
{
    float pitch;
    float loop_start;
    float loop_end;
};

struct patchParamV1_s
{
    float attack;
    float decay;
    float sustain;
    float release;
};

struct patchParam_s
{
    union
    {
        struct
        {
            uint32_t version;

            struct patchParamV0_s patchParamV0;
            struct patchParamV1_s patchParamV1;

        };
        uint8_t buffer[512]; /*! raw data */
    };
    char filename[64];
};

/*
 * union is very handy for easy conversion of bytes to the wav header information
 */
union wavHeader
{
    struct
    {
        char riff[4]; /*!< 'RIFF' */
        uint32_t fileSize; /*!< bytes to write containing all data (header + data) */
        char waveType[4]; /*!< 'WAVE' */

        char format[4]; /*!< 'fmt ' */
        uint32_t lengthOfData; /*!< length of the fmt header (16 bytes) */
        uint16_t format_tag; /*!< 0x0001: PCM */
        uint16_t numberOfChannels; /*!< 'WAVE' */
        uint32_t sampleRate;
        uint32_t byteRate;

        uint16_t bytesPerSample;
        uint16_t bitsPerSample;
        char dataStr[4];
        uint32_t dataSize; /* 22052 */
    };
    uint8_t wavHdr[44];
};


static void PatchManager_SaveWavefile(FST &fs, char *filename, int16_t *buffer, uint32_t bufferSize);
static void PatchManager_FilenameFromIdx(FST &fs, const char *dirname, uint8_t index);
static int PatchManager_GetFileList(FST &fs, const char *dirname, void(*fileInd)(char *filename, int offset), int offset);
static uint32_t PatchManager_LoadWavefile(FST &fs, char *filename, int16_t *buffer, uint32_t bufferSize);
static void PatchManager_CreateDir(FST &fs, const char *path);
static void PatchManager_SavePatchParam(FST &fs, char *filename, struct patchParam_s *patchParam);
static void PatchManager_LoadPatchParam(FST &fs, char *filename, struct patchParam_s *patchParam);
static void PatchManager_CreateNewFileNames(FST &fs);


uint32_t patch_selectedFileIndex = 0;
char currentFileNameWav[64] = "/samples/testSample.wav\0";
char currentFileNameBin[64] = "/samples/testSample.bin\0";


enum patchDst patchManagerDest = patch_dest_littlefs;

/*
 * last written files
 */
char wavNewFileName[64];
char parNewFileName[64];


void PatchManager_Init(void)
{
    /* nothing to do */
}

static int PatchManager_GetFileList(FST &fs, const char *dirname, void(*fileInd)(char *filename, int offset), int offset)
{
#ifdef PATCHMANAGER_DEBUG
    Serial.printf("Listing directory: %s\n", dirname);
#endif
    File root = fs.open(dirname);
    if (!root)
    {
        Status_LogMessage("Failed to open directory");
        return 0;
    }
    if (!root.isDirectory())
    {
        Status_LogMessage("Not a directory");
        return 0;
    }

    File file = root.openNextFile();


    int foundFiles = 0;

    while (file)
    {
        if (file.isDirectory())
        {
#if 0
            Serial.print("  DIR : ");
            Serial.println(file.name());
#endif
        }
        else
        {
#if 0
            Serial.printf("%03d - FILE: ", patch_selectedFileIndex);
            Serial.print(file.name());
            Serial.print("  SIZE: ");

            Serial.println(file.size());
#endif
            strcpy(currentFileNameWav, file.name());
            strcpy(currentFileNameBin, file.name());
            strcpy(&currentFileNameBin[strlen(currentFileNameBin) - 3], "bin");

            if (strcmp(".wav", &file.name()[strlen(file.name()) - 4]) == 0)
            {
#if 0
                Serial.printf("ignore %s, %s\n", ".wav", &file.name()[strlen(file.name()) - 4]);
#endif


                if (offset > 0)
                {
                    offset--;
                }
                else
                {
                    fileInd(currentFileNameWav, foundFiles++);
                }
            }
        }
        file = root.openNextFile();
    }
    return foundFiles;
}

int PatchManager_GetFileListExt(void(*fileInd)(char *filename, int offset), int offset)
{
    if (patchManagerDest == patch_dest_sd_mmc)
    {
        if (PatchManager_PrepareSdCard())
        {
            return PatchManager_GetFileList(SD_MMC, "/samples", fileInd, offset);
        }
    }
#ifdef ESP32
    else
    {
        if (PatchManager_PrepareLittleFs())
        {
            return PatchManager_GetFileList(LittleFS, "/samples", fileInd, offset);
        }
    }
#endif
    return 0;
}

static void PatchManager_FilenameFromIdx(FST &fs, const char *dirname, uint8_t index)
{
#ifdef PATCHMANAGER_DEBUG
    Serial.printf("Listing directory: %s\n", dirname);
#endif
    File root = fs.open(dirname);
    if (!root)
    {
        Status_LogMessage("Failed to open directory");
        return;
    }
    if (!root.isDirectory())
    {
        Status_LogMessage("Not a directory");
        return;
    }

    File file = root.openNextFile();
    patch_selectedFileIndex = 0;
    while (file)
    {
        if (file.isDirectory())
        {
#if 0
            Serial.print("  DIR : ");
            Serial.println(file.name());
#endif
        }
        else
        {
#if 0
            Serial.printf("%03d - FILE: ", patch_selectedFileIndex);
            Serial.print(file.name());
            Serial.print("  SIZE: ");

            Serial.println(file.size());
#endif
            strcpy(currentFileNameWav, file.name());
            strcpy(currentFileNameBin, file.name());
            strcpy(&currentFileNameBin[strlen(currentFileNameBin) - 3], "bin");

            if (strcmp(".wav", &file.name()[strlen(file.name()) - 4]) == 0)
            {
#if 0
                Serial.printf("ignore %s, %s\n", ".wav", &file.name()[strlen(file.name()) - 4]);
#endif
                if (index > 0)
                {
                    index--;
                }
                else
                {
                    return;
                }
                patch_selectedFileIndex++;
            }


        }
        file = root.openNextFile();
    }
    patch_selectedFileIndex--;
}

char lastSelectedFile[128] = "";

void PatchManager_UpdateFilename(void)
{
    if (patchManagerDest == patch_dest_sd_mmc)
    {
        if (PatchManager_PrepareSdCard())
        {
            PatchManager_FilenameFromIdx(SD_MMC, "/samples", patch_selectedFileIndex);
#ifdef ESP32
            SD_MMC.end();
#endif
#ifdef PATCHMANAGER_DEBUG
            Serial.printf("Active file: %03d - %s\n", patch_selectedFileIndex, currentFileNameWav);
            Serial.printf("Active file: %03d - %s\n", patch_selectedFileIndex, currentFileNameBin);
#endif
            sprintf(lastSelectedFile, "SD_MMC: %s", currentFileNameWav);
            Status_FileName(lastSelectedFile);
            sprintf(lastSelectedFile, "%s", currentFileNameWav);
        }
    }
#ifdef ESP32
    else
    {
        if (PatchManager_PrepareLittleFs())
        {
            PatchManager_FilenameFromIdx(LittleFS, "/samples", patch_selectedFileIndex);
            LittleFS.end();
#ifdef PATCHMANAGER_DEBUG
            Serial.printf("Active file: %03d - %s\n", patch_selectedFileIndex, currentFileNameWav);
            Serial.printf("Active file: %03d - %s\n", patch_selectedFileIndex, currentFileNameBin);
#endif
            sprintf(lastSelectedFile, "LittleFS: %s", currentFileNameWav);
            Status_FileName(lastSelectedFile);
            sprintf(lastSelectedFile, "%s", currentFileNameWav);
        }
    }
#endif
}

void PatchManager_FileIdxInc(uint8_t unused, float value)
{
    if (value > 0)
    {
        patch_selectedFileIndex++;
        PatchManager_UpdateFilename();
    }
}

void PatchManager_FileIdxDec(uint8_t unused, float value)
{
    if (value > 0)
    {
        if (patch_selectedFileIndex > 0)
        {
            patch_selectedFileIndex--;
        }
        PatchManager_UpdateFilename();
    }
}

static void PatchManager_SaveWavefile(FST &fs, char *filename, int16_t *buffer, uint32_t bufferSize)
{
    File f = fs.open(filename, FILE_WRITE);
    if (!f)
    {
        Status_LogMessage("Could not create new file\n");
        return;
    }

    uint32_t dataSizeOfbuffer = sizeof(int16_t) * bufferSize;
    union wavHeader wavHeader;

    memcpy(wavHeader.riff, "RIFF", 4);
    wavHeader.fileSize = 44 + dataSizeOfbuffer;
    memcpy(wavHeader.waveType, "WAVE", 4);
    memcpy(wavHeader.format, "fmt ", 4);
    wavHeader.lengthOfData = 16; /* length of the fmt header */
    wavHeader.format_tag = 0x0001; /* 0x0001: PCM */
    wavHeader.numberOfChannels = 1;
    wavHeader.sampleRate = 44100;
    wavHeader.byteRate = 44100 * 2;
    wavHeader.bytesPerSample = 2;
    wavHeader.bitsPerSample = 16;

    memcpy(wavHeader.dataStr, "data", 4);
    wavHeader.dataSize = dataSizeOfbuffer;

#ifdef ESP32
    f.seek(0, SeekSet);
#else
    f.seek(0);
#endif
    f.write(wavHeader.wavHdr, 44);

    /* avoid watchdog */
    delay(1);

    f.write((uint8_t *)buffer, dataSizeOfbuffer);
    f.close();

    /* avoid watchdog */
    delay(1);
}

static uint32_t PatchManager_LoadWavefile(FST &fs, char *filename, int16_t *buffer, uint32_t bufferSize)
{
    File f = fs.open(filename, FILE_READ);
    if (!f)
    {
        Status_LogMessage("Could not read file\n");
        return 0;
    }

    union wavHeader wavHeader;

    memset(wavHeader.wavHdr, 0, sizeof(wavHeader));

#ifdef ESP32
    f.seek(0, SeekSet);
#else
    f.seek(0);
#endif
    f.read(wavHeader.wavHdr, 44);

    /* avoid watchdog */
    delay(1);

    uint32_t bufferIn = 0;

    if (wavHeader.numberOfChannels == 1)
    {
#ifdef ESP32
        f.read((uint8_t *)buffer, wavHeader.dataSize);
#else
        uint32_t leftSize = wavHeader.dataSize;
        uint32_t bin = 0;
        while (leftSize > 256)
        {
            f.read((uint8_t *)&buffer[bin], 256);
            bin += 128; // because its int16 takes to bytes
            leftSize -= 256;
        }
        if (leftSize > 0)
        {
            f.read((uint8_t *)&buffer[bin], leftSize);
        }
#endif
        bufferIn += wavHeader.dataSize;
    }
    else
    {
        uint32_t dataLeft = wavHeader.dataSize;
        uint32_t dataRead;

        while (true)
        {
            uint8_t tempBuffer[512];
            if (dataLeft > 512)
            {
                dataRead = 512;
            }
            else
            {
                dataRead = dataLeft;
            }

            f.read(tempBuffer, dataRead);
            dataLeft -= dataRead;

            for (int i = 0; i < dataRead; i++)
            {
                /*
                 * reading only the left channel
                 * right channel will be ignored and data just skipped
                 */
                ((uint8_t *)buffer)[bufferIn] = tempBuffer[i];
                bufferIn += 1;
                i += 1;
                ((uint8_t *)buffer)[bufferIn] = tempBuffer[i];
                bufferIn += 1;
                i += 1;

                i += 1;
            }

            if (dataLeft == 0)
            {
                break;
            }
        }
    }
    f.close();

    /* avoid watchdog */
    delay(1);

    return bufferIn / sizeof(int16_t);
}

static void PatchManager_CreateDir(FST &fs, const char *path)
{
    Serial.printf("Creating Dir: %s\n", path);
    if (fs.mkdir(path))
    {
        Serial.println("Dir created");
    }
    else
    {
        Status_LogMessage("mkdir failed");
    }
}

bool PatchManager_PrepareSdCard(void)
{
#ifdef ESP32
    if (!SD_MMC.begin("/sdcard", true)) /* makes less noise on recording! */
#else
    if (!card.init(SD_DETECT_NONE))
#endif
    {
        Status_LogMessage("Card Mount Failed");
        delay(1000);
        return false;
    }

#ifdef ESP32
    uint8_t cardType = SD_MMC.cardType();

    if (cardType == CARD_NONE)
    {
        Status_LogMessage("No SD card attached");

        delay(1000);
        return false;
    }

    if (cardType == CARD_MMC)
    {
        Status_LogMessage("Card Access: MMC");
    }
    else if (cardType == CARD_SD)
    {
        Status_LogMessage("Card Access: SDSC");
    }
    else if (cardType == CARD_SDHC)
    {
        Status_LogMessage("Card Access: SDHC");
    }
    else
    {
        Status_LogMessage("Card Access: UNKNOWN");
    }
#endif

    return true;
}

#ifdef ESP32
bool PatchManager_PrepareLittleFs(void)
{
    if (!LittleFS.begin())
    {
        Status_LogMessage("LittleFS Mount Failed");
        return false;
    }

    return true;
}
#endif

static void PatchManager_SavePatchParam(FST &fs, char *filename, struct patchParam_s *patchParam)
{
    File f = fs.open(filename, FILE_WRITE);
    if (!f)
    {
        Status_LogMessage("Could not create new file\n");
        return;
    }

    f.write((uint8_t *)patchParam, sizeof(*patchParam));

    f.close();
}

static void PatchManager_LoadPatchParam(FST &fs, char *filename, struct patchParam_s *patchParam)
{
    File f = fs.open(filename, FILE_READ);
    if (!f)
    {
#ifdef PATCHMANAGER_DEBUG
        Serial.printf("No patch parameter\nUsing default values\n");
#endif
        patchParam->version = 0;
        patchParam->patchParamV0.pitch = 1;
        patchParam->patchParamV0.loop_start = 0;
        patchParam->patchParamV0.loop_end = 9999999; /* using sample length would be better */

        patchParam->patchParamV1.attack = 1;
        patchParam->patchParamV1.decay = 1;
        patchParam->patchParamV1.release = 0;
        patchParam->patchParamV1.sustain = 1;
        return;
    }

    f.read((uint8_t *)patchParam, sizeof(*patchParam));

#ifdef PATCHMANAGER_DEBUG
    Serial.printf("patchParam:\n");
    Serial.printf("    version: %d\n", patchParam->version);

    Serial.printf("        pitch: %0.06f\n", patchParam->patchParamV0.pitch);
    Serial.printf("        loop_start: %0.06f\n", patchParam->patchParamV0.loop_start);
    Serial.printf("        loop_end: %0.06f\n", patchParam->patchParamV0.loop_end);

    if (patchParam->version >= 1)
    {
        Serial.printf("        attack: %0.06f\n", patchParam->patchParamV1.attack);
        Serial.printf("        decay: %0.06f\n", patchParam->patchParamV1.decay);
        Serial.printf("        sustain: %0.06f\n", patchParam->patchParamV1.sustain);
        Serial.printf("        release: %0.06f\n", patchParam->patchParamV1.release);
    }
#endif

    f.close();
}

void PatchManager_SetDestination(uint8_t destination, float value)
{
    if (value > 0)
    {
        switch (destination)
        {
        case 0:
            patchManagerDest = patch_dest_littlefs;
            Status_TestMsg("Patch storage: little fs");
            break;
        case 1:
            patchManagerDest = patch_dest_sd_mmc;
            Status_TestMsg("Patch storage: sd mmc");
            break;
        }
        PatchManager_UpdateFilename();
    }
}

static void PatchManager_CreateNewFileNames(FST &fs)
{

    int i = 0;

    while (true)
    {
        sprintf(wavNewFileName, "/samples/newSample%03d.wav", i);
        sprintf(parNewFileName, "/samples/newSample%03d.bin", i);

        if (fs.exists(wavNewFileName))
        {
            i++;
            continue;
        }

        if (fs.exists(parNewFileName))
        {
            i++;
            continue;
        }

        break;
    }
}

void PatchManager_SaveNewPatch(struct patchParam_s *patchParam, int16_t *buffer, int bufferSize)
{
    if (patchManagerDest == patch_dest_sd_mmc)
    {
        if (PatchManager_PrepareSdCard())
        {
            PatchManager_CreateDir(SD_MMC, "/samples");
            PatchManager_CreateNewFileNames(SD_MMC);
            PatchManager_SavePatchParam(SD_MMC, parNewFileName, patchParam);
            PatchManager_SaveWavefile(SD_MMC, wavNewFileName, buffer, bufferSize);
#ifdef ESP32
            SD_MMC.end();
#endif
#ifdef PATCHMANAGER_DEBUG
            Serial.printf("Written %d to %s on SD_MMC\n", bufferSize, wavFileName);
#else
            Status_ValueChangedInt("Written to SD_MMC",  bufferSize);
#endif
        }
    }
#ifdef ESP32
    else
    {
        if (PatchManager_PrepareLittleFs())
        {
            PatchManager_CreateDir(LittleFS, "/samples");
            PatchManager_CreateNewFileNames(LittleFS);
            PatchManager_SavePatchParam(LittleFS, parNewFileName, patchParam);
            PatchManager_SaveWavefile(LittleFS, wavNewFileName, buffer, bufferSize);
            LittleFS.end();
#ifdef PATCHMANAGER_DEBUG
            Serial.printf("Written %d to %s on LittleFS\n", bufferSize, wavFileName);
#else
            Status_ValueChangedInt("Written to LittleFS",  bufferSize);
#endif
        }
    }
#endif
}

void PatchManager_SetFilename(const char *filename)
{
    strcpy(currentFileNameWav, filename);
    strcpy(currentFileNameBin, filename);
    strcpy(&currentFileNameBin[strlen(currentFileNameBin) - 3], "bin");
}

uint32_t PatchManager_LoadPatch(struct patchParam_s *patchParam, int16_t *buffer, int bufferSize)
{
    memset(patchParam, 0, sizeof(*patchParam));

    uint32_t readBufferBytes = 0 ;

    if (patchManagerDest == patch_dest_sd_mmc)
    {
        if (PatchManager_PrepareSdCard())
        {
            PatchManager_LoadPatchParam(SD_MMC, currentFileNameBin, patchParam);

            readBufferBytes = PatchManager_LoadWavefile(SD_MMC, currentFileNameWav, buffer, bufferSize);
#ifdef ESP32
            SD_MMC.end();
#endif
#ifdef PATCHMANAGER_DEBUG
            Serial.printf("Read %d from %s on SD_MMC\n", readBufferBytes, currentFileNameWav);
#else
            Status_ValueChangedInt("Read from SD_MMC",  readBufferBytes);
#endif
        }
    }
#ifdef ESP32
    else
    {
        if (PatchManager_PrepareLittleFs())
        {
            PatchManager_LoadPatchParam(LittleFS, currentFileNameBin, patchParam);

            readBufferBytes = PatchManager_LoadWavefile(LittleFS, currentFileNameWav, buffer, bufferSize);
            LittleFS.end();
#ifdef PATCHMANAGER_DEBUG
            Serial.printf("Read %d from %s on LittleFS\n", readBufferBytes, currentFileNameWav);
#else
            Status_ValueChangedInt("Read from LittleFS",  readBufferBytes);
#endif
        }
    }
#endif

    memcpy(patchParam->filename, currentFileNameBin, sizeof(patchParam->filename));

    return readBufferBytes;
}
