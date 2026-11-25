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
    options->battleStyle = 0; // shift
    options->battleScene = 0; // on
    options->buttonMode = 0;  // normal
    options->frame = 0;       // frame 1
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
