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
 * z_config.ino
 *
 * Put all your project configuration here (no defines etc)
 * This file will be included at the and can access all
 * declarations and type definitions
 *
 *  Created on: 12.05.2021
 *      Author: Marcel Licence
 */

#ifdef AUDIO_KIT_BUTTON_ANALOG
#ifdef AS5600_ENABLED
audioKitButtonCb audioKitButtonCallback = App_ButtonCbPlaySample;
#else
#ifdef SCREEN_ENABLED
audioKitButtonCb audioKitButtonCallback = Screen_ButtonCallback;
#else
audioKitButtonCb audioKitButtonCallback = App_ButtonCb;
#endif
#endif
#endif

/*
 * this mapping is used for the edirol pcr-800
 * this should be changed when using another controller
 */
struct midiControllerMapping edirolMapping[] =
{
    /* transport buttons */
    { 0x8, 0x52, "back", NULL, Sampler_RecordWait, 0},
    { 0xD, 0x52, "stop", NULL, Sampler_Stop, 0},
    { 0xe, 0x52, "start", NULL, Sampler_Play, 0},
    { 0xa, 0x52, "rec", NULL, Sampler_Record, 0},

    /* upper row of buttons */
    { 0x0, 0x50, "A1", NULL, Sampler_LoopUnlock, 0},
    { 0x1, 0x50, "A2", NULL, Sampler_LoopAll, 1},
#ifdef AS5600_ENABLED
    { 0x2, 0x50, "A3", NULL, Sampler_ScratchFader, 2},
    { 0x3, 0x50, "A4", NULL, Sampler_SetScratchSample, 0xFF},
#else
    { 0x2, 0x50, "A3", NULL, NULL, 2},
    { 0x3, 0x50, "A4", NULL, NULL, 3},
#endif

    { 0x4, 0x50, "A5", NULL, PatchManager_SetDestination, 0},
    { 0x5, 0x50, "A6", NULL, PatchManager_SetDestination, 1},
    { 0x6, 0x50, "A7", NULL, Sampler_RemoveActiveRecording, 2},
    { 0x7, 0x50, "A8", NULL, Sampler_LoadPatch, 0},

    { 0x0, 0x53, "A9", NULL, Sampler_SavePatch, 0},

    /* lower row of buttons */
    { 0x0, 0x51, "B1", NULL, Sampler_LoopLock, 0},
    { 0x1, 0x51, "B2", NULL, Sampler_LoopRemove, 1},
    { 0x2, 0x51, "B3", NULL, Sampler_Panic, 2},
    { 0x3, 0x51, "B4", NULL, Sampler_NormalizeActiveRecording, 3},

    { 0x4, 0x51, "B5", NULL, PatchManager_FileIdxInc, 0},
    { 0x5, 0x51, "B6", NULL, PatchManager_FileIdxDec, 1},
    { 0x6, 0x51, "B7", NULL, Sampler_Panic, 2},
    { 0x7, 0x51, "B8", NULL, Sampler_MeasureThreshold, 3},

    { 0x1, 0x53, "B9", NULL, App_ToggleSource, 0},

    /* pedal */
    { 0x0, 0x0b, "VolumePedal", NULL, NULL, 0},

    /* slider */
    { 0x0, 0x11, "S1", NULL, Sampler_SetADSR_Attack, 0},
    { 0x1, 0x11, "S2", NULL, Sampler_SetADSR_Decay, 1},
    { 0x2, 0x11, "S3", NULL, Sampler_SetADSR_Sustain, 2},
    { 0x3, 0x11, "S4", NULL, Sampler_SetADSR_Release, 3},

    { 0x4, 0x11, "S5", NULL, Delay_SetInputLevel, 0},
    { 0x5, 0x11, "S6", NULL, Delay_SetFeedback, 0},
    { 0x6, 0x11, "S7", NULL, Delay_SetOutputLevel, 0},
    { 0x7, 0x11, "S8", NULL, Delay_SetLength, 0},

    { 0x1, 0x12, "S9", NULL, App_SetOutputLevel, 0},

    /* rotary */
    { 0x0, 0x10, "R1", NULL, Sampler_LoopStartC, 0},
    { 0x1, 0x10, "R2", NULL, Sampler_LoopStartF, 1},
    { 0x2, 0x10, "R3", NULL, Sampler_LoopEndC, 2},
    { 0x3, 0x10, "R4", NULL, Sampler_LoopEndF, 3},

    { 0x4, 0x10, "R5", NULL, Sampler_SetLoopEndMultiplier, 0},
    { 0x5, 0x10, "R6", NULL, Sampler_SetPitch, 0},
    { 0x6, 0x10, "R7", NULL, Sampler_ModulationSpeed, 0},
    { 0x7, 0x10, "R8", NULL, Sampler_ModulationPitch, 0},

    { 0x0, 0x12, "R9", NULL, Reverb_SetLevel, 0},

    /* Central slider */
#ifndef AS5600_ENABLED
    { 0x0, 0x13, "H1", NULL, App_SetBrightness, 0},
#else
    { 0x0, 0x13, "H1", NULL, Sampler_ScratchFader, 0},
#endif
};

struct midiMapping_s midiMapping =
{
    NULL,
    Sampler_NoteOn,
    Sampler_NoteOff,
    Sampler_PitchBend,
    Sampler_ModulationWheel,
    Synth_RealTimeMsg,
    Synth_SongPosition,
    edirolMapping,
    sizeof(edirolMapping) / sizeof(edirolMapping[0]),
};

#ifdef MIDI_VIA_USB_ENABLED
struct usbMidiMappingEntry_s usbMidiMappingEntries[] =
{
    {
        NULL,
        App_UsbMidiShortMsgReceived,
        NULL,
        NULL,
        0xFF,
    },
};

struct usbMidiMapping_s usbMidiMapping =
{
    NULL,
    NULL,
    usbMidiMappingEntries,
    sizeof(usbMidiMappingEntries) / sizeof(usbMidiMappingEntries[0]),
};
#endif /* MIDI_VIA_USB_ENABLED */
