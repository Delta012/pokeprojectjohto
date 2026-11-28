#include "options.h"

#include "global.h"

#include "player_data.h"
#include "system.h"

Options *Options_New(enum HeapID heapID) {
    Options *ret = Heap_Alloc(heapID, sizeof(Options));
    Options_Init(ret);
    return ret;
}

void Options_Copy(Options *src, Options *dest) {
    MI_CpuCopy8(src, dest, sizeof(Options));
}

void Options_Init(Options *options) {
    MI_CpuClear8(options, sizeof(Options));

    options->textSpeed = 1;   // medium speed
    options->soundMethod = 0; // stereo
    options->battleStyle = 1; // set
    options->battleScene = 0; // on
    options->buttonMode = 0;  // normal
    options->frame = 0;       // frame 1
    // Added settings; On/Off flags use 0 = On (first menu carousel choice).
    options->camera3D = 0;     // on
    options->autosave = 0;     // on
    options->sendToBoxes = 0;  // on
    options->giveNicknames = 0; // on
    options->expShare = 0;     // on
    options->expMode = 1;      // deferred
    options->bgmVolume = 10;   // 0-10
    options->sfxVolume = 10;
    options->criesVolume = 10;
}

void Options_SetButtonModeOnMain(SaveData *saveData, u32 buttonMode) {
    if (saveData != NULL) {
        buttonMode = Options_GetButtonMode(Save_PlayerData_GetOptionsAddr(saveData));
    }

    switch (buttonMode) {
    case 1:
        gSystem.buttonMode = 3;
        break;
    case 0:
    default:
        gSystem.buttonMode = 0;
        break;
    }
}

u32 Options_GetTextSpeed(Options *options) {
    return options->textSpeed;
}

void Options_SetTextSpeed(Options *options, u32 textSpeed) {
    options->textSpeed = textSpeed;
}

u8 Options_GetTextFrameDelay(Options *options) {
    switch (Options_GetTextSpeed(options)) {
    case 0: // slow: 4 frames/char
        return 4;
    case 1: // medium: 2 frames/char
        return 2;
    case 2: // fast: 1 frame/char (stock fast)
        return 1;
    case 3: // very slow: 8 frames/char
        return 8;
    case 4: // very fast: 2 characters per frame
        return 0xFE;
    default: // ultra fast: 4 characters per frame
        return 0xFD;
    }
}

u32 Options_GetSoundMethod(Options *options) {
    return options->soundMethod;
}

void Options_SetSoundMethod(Options *options, u32 soundMethod) {
    options->soundMethod = soundMethod;
}

u32 Options_GetBattleScene(Options *options) {
    return options->battleScene;
}

void Options_SetBattleScene(Options *options, u32 battleScene) {
    options->battleScene = battleScene;
}

u32 Options_GetBattleStyle(Options *options) {
    return options->battleStyle;
}

void Options_SetBattleStyle(Options *options, u32 battleStyle) {
    options->battleStyle = battleStyle;
}

u32 Options_GetButtonMode(Options *options) {
    return options->buttonMode;
}

void Options_SetButtonMode(Options *options, u32 buttonMode) {
    options->buttonMode = buttonMode;
}

u32 Options_GetFrame(Options *options) {
    return options->frame;
}

void Options_SetFrame(Options *options, u32 frame) {
    options->frame = frame;
}

u32 Options_GetCamera3D(Options *options) {
    return options->camera3D;
}

void Options_SetCamera3D(Options *options, u32 value) {
    options->camera3D = value;
}

u32 Options_GetAutosave(Options *options) {
    return options->autosave;
}

void Options_SetAutosave(Options *options, u32 value) {
    options->autosave = value;
}

u32 Options_GetSendToBoxes(Options *options) {
    return options->sendToBoxes;
}

void Options_SetSendToBoxes(Options *options, u32 value) {
    options->sendToBoxes = value;
}

u32 Options_GetGiveNicknames(Options *options) {
    return options->giveNicknames;
}

void Options_SetGiveNicknames(Options *options, u32 value) {
    options->giveNicknames = value;
}

u32 Options_GetExpShare(Options *options) {
    return options->expShare;
}

void Options_SetExpShare(Options *options, u32 value) {
    options->expShare = value;
}

u32 Options_GetExpMode(Options *options) {
    return options->expMode;
}

void Options_SetExpMode(Options *options, u32 value) {
    options->expMode = value;
}

u32 Options_GetBgmVolume(Options *options) {
    return options->bgmVolume;
}

void Options_SetBgmVolume(Options *options, u32 value) {
    options->bgmVolume = value;
}

u32 Options_GetSfxVolume(Options *options) {
    return options->sfxVolume;
}

void Options_SetSfxVolume(Options *options, u32 value) {
    options->sfxVolume = value;
}

u32 Options_GetCriesVolume(Options *options) {
    return options->criesVolume;
}

void Options_SetCriesVolume(Options *options, u32 value) {
    options->criesVolume = value;
}
