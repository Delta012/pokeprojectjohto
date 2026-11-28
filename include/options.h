#ifndef POKEHEARTGOLD_OPTIONS_H
#define POKEHEARTGOLD_OPTIONS_H

#include "heap.h"
#include "save.h"

typedef struct Options {
    u16 textSpeed : 4;
    u16 soundMethod : 2;
    u16 battleStyle : 1;
    u16 battleScene : 1;
    u16 buttonMode : 2;
    u16 frame : 5;
    u16 dummy : 1;
    // Added settings. On/Off flags store 0 = On, matching the options menu's
    // first carousel choice; expMode: 0 = Normal, 1 = Deferred.
    u16 camera3D : 1;
    u16 autosave : 1;
    u16 sendToBoxes : 1;
    u16 giveNicknames : 1;
    u16 expShare : 1;
    u16 expMode : 1;
    u16 dummy2 : 10;
    u16 bgmVolume : 4;
    u16 sfxVolume : 4;
    u16 criesVolume : 4;
    u16 dummy3 : 4;
    u16 dummy4 : 16; // pad to 8 bytes; Options_Copy copies via MI_CpuCopy8
} Options; // size: 0x8

Options *Options_New(enum HeapID heapID);
void Options_Copy(Options *src, Options *dest);
void Options_Init(Options *options);
void Options_SetButtonModeOnMain(SaveData *saveData, u32 buttonMode);
u32 Options_GetTextSpeed(Options *options);
void Options_SetTextSpeed(Options *options, u32 textSpeed);
u8 Options_GetTextFrameDelay(Options *options);
u32 Options_GetSoundMethod(Options *options);
void Options_SetSoundMethod(Options *options, u32 soundMethod);
u32 Options_GetBattleScene(Options *options);
void Options_SetBattleScene(Options *options, u32 battleScene);
u32 Options_GetBattleStyle(Options *options);
void Options_SetBattleStyle(Options *options, u32 battleStyle);
u32 Options_GetButtonMode(Options *options);
void Options_SetButtonMode(Options *options, u32 buttonMode);
u32 Options_GetFrame(Options *options);
void Options_SetFrame(Options *options, u32 frame);
u32 Options_GetCamera3D(Options *options);
void Options_SetCamera3D(Options *options, u32 value);
u32 Options_GetAutosave(Options *options);
void Options_SetAutosave(Options *options, u32 value);
u32 Options_GetSendToBoxes(Options *options);
void Options_SetSendToBoxes(Options *options, u32 value);
u32 Options_GetGiveNicknames(Options *options);
void Options_SetGiveNicknames(Options *options, u32 value);
u32 Options_GetExpShare(Options *options);
void Options_SetExpShare(Options *options, u32 value);
u32 Options_GetExpMode(Options *options);
void Options_SetExpMode(Options *options, u32 value);
u32 Options_GetBgmVolume(Options *options);
void Options_SetBgmVolume(Options *options, u32 value);
u32 Options_GetSfxVolume(Options *options);
void Options_SetSfxVolume(Options *options, u32 value);
u32 Options_GetCriesVolume(Options *options);
void Options_SetCriesVolume(Options *options, u32 value);

#endif // POKEHEARTGOLD_OPTIONS_H
