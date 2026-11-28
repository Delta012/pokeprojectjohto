#include "options_app.h"

#include <nitro/spi/ARM9/pm.h>

#include "global.h"

#include "data/resdat.naix"
#include "msgdata/msg.naix"
#include "msgdata/msg/msg_0045.h"

#include "bg_window.h"
#include "font.h"
#include "gf_gfx_loader.h"
#include "menu_input_state.h"
#include "msgdata.h"
#include "options.h"
#include "render_text.h"
#include "render_window.h"
#include "screen_fade.h"
#include "sound.h"
#include "sprite.h"
#include "sprite_system.h"
#include "system.h"
#include "touchscreen.h"
#include "sound_playback.h"
#include "wireless_status_icon.h"
#include "vram_transfer_manager.h"



typedef struct OptionsApp_MenuEntry {
    u16 numStrings;
    u16 value;
    String *strings[20];
} OptionsApp_MenuEntry;

enum {
    MENU_ENTRY_TEXT_SPEED,
    MENU_ENTRY_BATTLE_SCENE,
    MENU_ENTRY_BATTLE_STYLE,
    MENU_ENTRY_SOUND_METHOD,
    MENU_ENTRY_FRAME,
    // Added settings; inert for now, they only drive the carousels.
    // Button Mode was removed entirely from this overlay.
    MENU_ENTRY_3D_CAMERA,
    MENU_ENTRY_AUTOSAVE,
    MENU_ENTRY_SEND_TO_BOXES,
    MENU_ENTRY_GIVE_NICKNAMES,
    MENU_ENTRY_EXP_SHARE,
    MENU_ENTRY_EXP_MODE,
    MENU_ENTRY_BGM_VOLUME,
    MENU_ENTRY_SFX_VOLUME,
    MENU_ENTRY_CRIES_VOLUME,
    MENU_ENTRY_6,

    MENU_ENTRY_COUNT,
};

// Fixed row slots: window rows, backdrop bands, arrow sprite pairs.
// Not the menu entry count (MENU_ENTRY_6 is just the button-row sentinel).
#define OPTION_ROW_COUNT 6

typedef struct OptionsApp_Data {
    enum HeapID heapID;
    u32 exitState;
    u32 setupAndFreeState;
    u32 fadeUnused; // unused, game writes 0 here when it's about to start a fade, but never reads from here
    u32 exitChoice : 2;
    u32 currentMenuEntryId : 4;
    u32 optionPage : 2; // active category tab page
    u32 unk10_5 : 13; // unused
    u32 reloadFrameGfx : 1;
    u32 unk10_22 : 10; // unused
    BgConfig *bgConfig;
    Options options;
    Options *playerOptionsUnused; // unused copy of playerOptions
    MenuInputStateMgr *menuInputPtr;
    Options *playerOptions;
    MsgData *msgData;
    u8 filler2C[0x8];
    union {
        Window asArray[7];
        struct {
            Window optionsTitle;
            Window selectedOption;
            Window frameAndTextSpeedTest;
            Window quitButton;
            Window confirmButton;
            Window categoryTabs;
            Window resetButton;
        };
    } windows;
    OptionsApp_MenuEntry menuEntries[MENU_ENTRY_COUNT];
    SpriteSystem *spriteRenderer;
    SpriteManager *spriteGfxHandler;
    Sprite *sprites[16];
    u8 filler308[20];
    u32 menuInputState;
    String *frameNumText;
    u8 textPrinter;
} OptionsApp_Data; // size: 0x628

// How far left each row's value erase band reaches, from the stock base x 108.
// Labels are centred on 181 and start no earlier than x 140, so only Frame
// still needs its wider stock band.
static const s8 sOptionsApp_ClearRectXOffsets[MENU_ENTRY_COUNT] = {
    0,
    0,
    0,
    0,
    -0x10, // Frame: wider stock erase band
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

static const u32 sOptionsAppBgLayers[5] = {
    GF_BG_LYR_MAIN_0,
    GF_BG_LYR_MAIN_1,
    GF_BG_LYR_MAIN_2,
    GF_BG_LYR_SUB_0,
    GF_BG_LYR_SUB_1,
};

static const int sNumChoicesPerMenuEntry[MENU_ENTRY_COUNT] = {
    6, 2, 2, 2, 20,
    2, // 3D Camera
    2, // Autosave
    2, // Send to Boxes
    2, // Give Nicknames
    2, // Exp Share
    2, // Exp Mode
    11, // Background Music volume, 0-10
    11, // Sound Effects volume, 0-10
    11, // Pokémon Cries volume, 0-10
    2,  // button-row carousel: Reset / Confirm / Quit
};

// Cursor border (GF_BG_LYR_MAIN_0 vertical scroll) per row slot, top to
// bottom; pages index by slot, not by menu entry. Stock offsets were
// -8..-128; +1 here matches the screen-wide 1 px shift.
static const int sOptionRowBorderYCoords[OPTION_ROW_COUNT] = {
    -7, -31, -55, -79, -103, -127
};

// Centre x of a row's carousel value label, window-local pixels: the stock
// value position from the original FRAME row, which every added row shares.
#define OPTION_CHOICE_LABEL_CENTER_X 174

// The options backdrop (NARC_a_0_7_2 member 17 -> GF_BG_LYR_MAIN_2) bakes
// three empty tab squares into the title band right of "Settings", at screen
// x 64-119, 128-183 and 192-247 (y 0-23). The categoryTabs window starts at
// screen x 64 (tile 8), so the square centres are window-local x 28, 92, 156.
static const u16 sCategoryTabLabelXCoords[3] = { 28, 92, 156 };
static const u16 sCategoryTabMsgIds[3] = {
    msg_0045_00063, // General
    msg_0045_00064, // Battle
    msg_0045_00065, // Audio
};

// Row-label msg ids, indexed by menu entry id (MENU_ENTRY_6 excluded).
static const u16 sOptionEntryLabelMsgIds[MENU_ENTRY_6] = {
    msg_0045_00001, // Text Speed
    msg_0045_00002, // Battle Animations
    msg_0045_00003, // Battle Style
    msg_0045_00004, // Sound Output
    msg_0045_00006, // Frame Type
    msg_0045_00066, // 3D Camera
    msg_0045_00067, // Autosave
    msg_0045_00068, // Send to Boxes
    msg_0045_00069, // Give Nicknames
    msg_0045_00070, // Exp Share
    msg_0045_00071, // Exp Mode
    msg_0045_00072, // Background Music
    msg_0045_00073, // Sound Effects
    msg_0045_00074, // Pokémon Cries
};

// Option pages, one per title-band tab; tapping a tab shows that page's rows
// in the top-down row slots. The Confirm/Quit buttons are sprite-based and
// sit below the row window, so they are visible on every page.
static const u8 sOptionPageRowCounts[NELEMS(sCategoryTabMsgIds)] = { 6, 4, 4 };
static const u8 sOptionPageEntries[NELEMS(sCategoryTabMsgIds)][OPTION_ROW_COUNT] = {
    { MENU_ENTRY_TEXT_SPEED, MENU_ENTRY_3D_CAMERA, MENU_ENTRY_AUTOSAVE,
      MENU_ENTRY_SEND_TO_BOXES, MENU_ENTRY_GIVE_NICKNAMES, MENU_ENTRY_FRAME },
    { MENU_ENTRY_BATTLE_SCENE, MENU_ENTRY_BATTLE_STYLE,
      MENU_ENTRY_EXP_SHARE, MENU_ENTRY_EXP_MODE },
    { MENU_ENTRY_SOUND_METHOD, MENU_ENTRY_BGM_VOLUME,
      MENU_ENTRY_SFX_VOLUME, MENU_ENTRY_CRIES_VOLUME },
};

// Text Speed's carousel slots in display order. The stored option value keeps
// the stock meanings (0 = slow, 1 = medium, 2 = fast) so old saves load
// unchanged, with 3 = very slow, 4 = very fast and 5 = ultra fast added.
static const u16 sTextSpeedChoiceMsgIds[6] = {
    msg_0045_00060, // VERY SLOW
    msg_0045_00009, // SLOW
    msg_0045_00010, // MEDIUM
    msg_0045_00011, // FAST
    msg_0045_00061, // VERY FAST
    msg_0045_00062, // ULTRA FAST
};
static const u8 sTextSpeedChoiceRawValues[6] = { 3, 0, 1, 2, 4, 5 };
static const u8 sTextSpeedRawToChoice[6] = { 1, 2, 3, 0, 4, 5 };

// Value strings for the added settings, indexed by (entry - first added
// entry). Stock rows are reused where possible: On/Off, Exp Mode's
// "Normal", volumes' "1".."10"; the new rows supply "Deferred" and "0".
static const u16 sExtraEntryChoiceMsgIds[MENU_ENTRY_CRIES_VOLUME - MENU_ENTRY_3D_CAMERA + 1][11] = {
    { msg_0045_00012, msg_0045_00013 }, // 3D Camera: On, Off
    { msg_0045_00012, msg_0045_00013 }, // Autosave: On, Off
    { msg_0045_00012, msg_0045_00013 }, // Send to Boxes: On, Off
    { msg_0045_00012, msg_0045_00013 }, // Give Nicknames: On, Off
    { msg_0045_00012, msg_0045_00013 }, // Exp Share: On, Off
    { msg_0045_00018, msg_0045_00075 }, // Exp Mode: Normal, Deferred
    { msg_0045_00076, msg_0045_00020, msg_0045_00021, msg_0045_00022, msg_0045_00023, msg_0045_00024, msg_0045_00025, msg_0045_00026, msg_0045_00027, msg_0045_00028, msg_0045_00029 }, // 0..10
    { msg_0045_00076, msg_0045_00020, msg_0045_00021, msg_0045_00022, msg_0045_00023, msg_0045_00024, msg_0045_00025, msg_0045_00026, msg_0045_00027, msg_0045_00028, msg_0045_00029 }, // 0..10
    { msg_0045_00076, msg_0045_00020, msg_0045_00021, msg_0045_00022, msg_0045_00023, msg_0045_00024, msg_0045_00025, msg_0045_00026, msg_0045_00027, msg_0045_00028, msg_0045_00029 }, // 0..10
};

static const TouchscreenHitbox sOptionsAppTouchscreenHitboxes[20] = {
    { .rect = { .top = 26, .bottom = 46, .left = 114, .right = 153 } },
    { .rect = { .top = 26, .bottom = 46, .left = 160, .right = 200 } },
    { .rect = { .top = 26, .bottom = 46, .left = 209, .right = 249 } },
    { .rect = { .top = 50, .bottom = 70, .left = 114, .right = 153 } },
    { .rect = { .top = 50, .bottom = 70, .left = 209, .right = 249 } },
    { .rect = { .top = 74, .bottom = 93, .left = 114, .right = 153 } },
    { .rect = { .top = 74, .bottom = 93, .left = 209, .right = 249 } },
    { .rect = { .top = 98, .bottom = 117, .left = 114, .right = 153 } },
    { .rect = { .top = 98, .bottom = 117, .left = 209, .right = 249 } },
    { .rect = { .top = 122, .bottom = 142, .left = 114, .right = 153 } },
    { .rect = { .top = 122, .bottom = 142, .left = 209, .right = 249 } },
    { .rect = { .top = 146, .bottom = 166, .left = 112, .right = 152 } },
    { .rect = { .top = 146, .bottom = 166, .left = 209, .right = 254 } },
    { .rect = { .top = 172, .bottom = 191, .left = 128, .right = 180 } },
    { .rect = { .top = 172, .bottom = 191, .left = 183, .right = 255 } },
    { .rect = { .top = 0, .bottom = 23, .left = 64, .right = 119 } },   // General tab
    { .rect = { .top = 0, .bottom = 23, .left = 128, .right = 183 } },  // Battle tab
    { .rect = { .top = 0, .bottom = 23, .left = 192, .right = 247 } },  // Audio tab
    { .rect = { .top = 172, .bottom = 191, .left = 4, .right = 68 } },  // Reset button
    { TOUCHSCREEN_RECTLIST_END },
};

// Hitboxes 0-12 (carousel arrows) map to {row slot, side}; the entry behind
// a slot depends on the active page. Indexes 13-19 (Confirm, Quit, the three
// tabs, Reset) are matched directly in OptionsApp_HandleInput.
static const u32 sOptionsAppHitboxRowSides[13][2] = {
    { 0, 3 },
    { 0, 4 },
    { 0, 4 },
    { 1, 3 },
    { 1, 4 },
    { 2, 3 },
    { 2, 4 },
    { 3, 3 },
    { 3, 4 },
    { 4, 3 },
    { 4, 4 },
    { 5, 3 },
    { 5, 4 },
};

static const UnmanagedSpriteTemplate sOptionsAppSpriteTemplates[16] = {
    {
     .resourceSet = 0,
     .x = 112,
     .y = 23,
     .z = 0,
     .animation = 0,
     .drawPriority = 1,
     .pal = 0,
     .vram = NNS_G2D_VRAM_TYPE_2DMAIN,
     .paletteMode = 0,
     .unk_1C = 0,
     .unk_20 = 0,
     .unk_24 = 0,
     },
    {
     .resourceSet = 1,
     .x = 117,
     .y = 47,
     .z = 0,
     .animation = 0,
     .drawPriority = 1,
     .pal = 0,
     .vram = NNS_G2D_VRAM_TYPE_2DMAIN,
     .paletteMode = 0,
     .unk_1C = 0,
     .unk_20 = 0,
     .unk_24 = 0,
     },
    {
     .resourceSet = 1,
     .x = 117,
     .y = 71,
     .z = 0,
     .animation = 0,
     .drawPriority = 1,
     .pal = 0,
     .vram = NNS_G2D_VRAM_TYPE_2DMAIN,
     .paletteMode = 0,
     .unk_1C = 0,
     .unk_20 = 0,
     .unk_24 = 0,
     },
    {
     .resourceSet = 1,
     .x = 117,
     .y = 95,
     .z = 0,
     .animation = 0,
     .drawPriority = 1,
     .pal = 0,
     .vram = NNS_G2D_VRAM_TYPE_2DMAIN,
     .paletteMode = 0,
     .unk_1C = 0,
     .unk_20 = 0,
     .unk_24 = 0,
     },
    {
     .resourceSet = 1,
     .x = 117,
     .y = 119,
     .z = 0,
     .animation = 0,
     .drawPriority = 1,
     .pal = 0,
     .vram = NNS_G2D_VRAM_TYPE_2DMAIN,
     .paletteMode = 0,
     .unk_1C = 0,
     .unk_20 = 0,
     .unk_24 = 0,
     },
    {
     .resourceSet = 1,
     .x = 117,
     .y = 143,
     .z = 0,
     .animation = 0,
     .drawPriority = 1,
     .pal = 0,
     .vram = NNS_G2D_VRAM_TYPE_2DMAIN,
     .paletteMode = 0,
     .unk_1C = 0,
     .unk_20 = 0,
     .unk_24 = 0,
     },
    {
     .resourceSet = 2,
     .x = 214,
     .y = 143,
     .z = 0,
     .animation = 0,
     .drawPriority = 1,
     .pal = 0,
     .vram = NNS_G2D_VRAM_TYPE_2DMAIN,
     .paletteMode = 0,
     .unk_1C = 0,
     .unk_20 = 0,
     .unk_24 = 0,
     },
    {
     .resourceSet = 3,
     .x = 188,
     .y = 169,
     .z = 0,
     .animation = 0,
     .drawPriority = 1,
     .pal = 1,
     .vram = NNS_G2D_VRAM_TYPE_2DMAIN,
     .paletteMode = 0,
     .unk_1C = 0,
     .unk_20 = 0,
     .unk_24 = 0,
     },
    {
     .resourceSet = 3,
     .x = 116,
     .y = 169,
     .z = 0,
     .animation = 0,
     .drawPriority = 1,
     .pal = 1,
     .vram = NNS_G2D_VRAM_TYPE_2DMAIN,
     .paletteMode = 0,
     .unk_1C = 0,
     .unk_20 = 0,
     .unk_24 = 0,
    },
    {
     .resourceSet = 1,
     .x = 117,
     .y = 23,
     .z = 0,
     .animation = 0,
     .drawPriority = 1,
     .pal = 0,
     .vram = NNS_G2D_VRAM_TYPE_2DMAIN,
     .paletteMode = 0,
     .unk_1C = 0,
     .unk_20 = 0,
     .unk_24 = 0,
    },
    {
     .resourceSet = 2,
     .x = 214,
     .y = 23,
     .z = 0,
     .animation = 0,
     .drawPriority = 1,
     .pal = 0,
     .vram = NNS_G2D_VRAM_TYPE_2DMAIN,
     .paletteMode = 0,
     .unk_1C = 0,
     .unk_20 = 0,
     .unk_24 = 0,
    },
    {
     .resourceSet = 2,
     .x = 214,
     .y = 47,
     .z = 0,
     .animation = 0,
     .drawPriority = 1,
     .pal = 0,
     .vram = NNS_G2D_VRAM_TYPE_2DMAIN,
     .paletteMode = 0,
     .unk_1C = 0,
     .unk_20 = 0,
     .unk_24 = 0,
    },
    {
     .resourceSet = 2,
     .x = 214,
     .y = 71,
     .z = 0,
     .animation = 0,
     .drawPriority = 1,
     .pal = 0,
     .vram = NNS_G2D_VRAM_TYPE_2DMAIN,
     .paletteMode = 0,
     .unk_1C = 0,
     .unk_20 = 0,
     .unk_24 = 0,
    },
    {
     .resourceSet = 2,
     .x = 214,
     .y = 95,
     .z = 0,
     .animation = 0,
     .drawPriority = 1,
     .pal = 0,
     .vram = NNS_G2D_VRAM_TYPE_2DMAIN,
     .paletteMode = 0,
     .unk_1C = 0,
     .unk_20 = 0,
     .unk_24 = 0,
    },
    {
     .resourceSet = 2,
     .x = 214,
     .y = 119,
     .z = 0,
     .animation = 0,
     .drawPriority = 1,
     .pal = 0,
     .vram = NNS_G2D_VRAM_TYPE_2DMAIN,
     .paletteMode = 0,
     .unk_1C = 0,
     .unk_20 = 0,
     .unk_24 = 0,
    },
    {
     .resourceSet = 3,
     .x = 4,
     .y = 169,
     .z = 0,
     .animation = 0,
     .drawPriority = 1,
     .pal = 1,
     .vram = NNS_G2D_VRAM_TYPE_2DMAIN,
     .paletteMode = 0,
     .unk_1C = 0,
     .unk_20 = 0,
     .unk_24 = 0,
    },
};

// Carousel arrow sprite slots per row slot (templates sit at fixed screen
// y 23, 47, ... 143): {left, right}. Row slot < page row count is visible;
// empty rows hide their arrows.
static const s8 sRowArrowSprites[OPTION_ROW_COUNT][2] = {
    { 9,  10 }, // row y 23
    { 1,  11 }, // row y 47
    { 2,  12 }, // row y 71
    { 3,  13 }, // row y 95
    { 4,  14 }, // row y 119
    { 5,  6 },  // row y 143
};

static void OptionsApp_SetupGraphicsBanks(void);
static void OptionsApp_OnVBlank(OptionsApp_Data *data);
static BOOL OptionsApp_Setup(OptionsApp_Data *data);
static BOOL OptionsApp_Free(OptionsApp_Data *data);
static void OptionsApp_SetupBgConfig(OptionsApp_Data *data);
static void OptionsApp_FreeBgConfig(OptionsApp_Data *data);
static void OptionsApp_SetupGraphicsData(OptionsApp_Data *data);
static void OptionsApp_FreeGraphicsDummy(OptionsApp_Data *data);
static void OptionsApp_SetupWindows(OptionsApp_Data *data);
static void OptionsApp_FreeWindows(OptionsApp_Data *data);
static void OptionsApp_PrintTextFrameString(OptionsApp_Data *data, String *frameNumText, BOOL instant);
static void OptionsApp_SetupInterfaceText(OptionsApp_Data *data);
static void OptionsApp_LoadMenuEntriesData(OptionsApp_Data *data);
static void OptionsApp_UpdateMenuEntry(OptionsApp_Data *data, u16 menuEntryId);
static void OptionsApp_UpdateMenuEntryCarousel(OptionsApp_Data *data, u32 menuEntryId, OptionsApp_MenuEntry *menuEntry, s32 offset);
static void OptionsApp_HandleKeyInput(OptionsApp_Data *data, OptionsApp_MenuEntry *menuEntry);
static void OptionsApp_HandleInput(OptionsApp_Data *data);
static void OptionsApp_UpdateSelectionHighlight(OptionsApp_Data *data, u32 menuEntryId);
static void OptionsApp_HideBorderOnConfirmRow(OptionsApp_Data *data);
static int OptionsApp_GetEntrySlot(const OptionsApp_Data *data, u32 menuEntryId);
static void OptionsApp_PrintOptionRowLabels(OptionsApp_Data *data);
static void OptionsApp_UpdateCategoryTabLabels(OptionsApp_Data *data);
static void OptionsApp_UpdateRowArrowVisibility(OptionsApp_Data *data);
static void OptionsApp_MoveSelectionCursor(OptionsApp_Data *data, s32 direction);
static void OptionsApp_SwitchOptionPage(OptionsApp_Data *data, u32 page);
static void OptionsApp_ResetToDefaults(OptionsApp_Data *data);
static void OptionsApp_SetupSpriteRenderer(OptionsApp_Data *data);
static void OptionsApp_FreeSpriteRenderer(OptionsApp_Data *data);
static void OptionsApp_SetupSprites(OptionsApp_Data *data);
static BOOL OptionsApp_ConfirmAndQuitButtonsAreDoneAnimating(OptionsApp_Data *data);

BOOL OptionsMenu_Init(OverlayManager *manager, int *state) {
    OptionsApp_Args *args = OverlayManager_GetArgs(manager);
    Heap_Create(HEAP_ID_3, HEAP_ID_OPTIONS_APP, 0x30000);

    OptionsApp_Data *data = OverlayManager_CreateAndGetData(manager, sizeof(OptionsApp_Data), HEAP_ID_OPTIONS_APP);
    memset(data, 0, sizeof(OptionsApp_Data));

    data->options.textSpeed = Options_GetTextSpeed(args->options);
    data->options.battleScene = Options_GetBattleScene(args->options);
    data->options.battleStyle = Options_GetBattleStyle(args->options);
    data->options.soundMethod = Options_GetSoundMethod(args->options);
    data->options.frame = Options_GetFrame(args->options);
    data->options.camera3D = Options_GetCamera3D(args->options);
    data->options.autosave = Options_GetAutosave(args->options);
    data->options.sendToBoxes = Options_GetSendToBoxes(args->options);
    data->options.giveNicknames = Options_GetGiveNicknames(args->options);
    data->options.expShare = Options_GetExpShare(args->options);
    data->options.expMode = Options_GetExpMode(args->options);
    data->options.bgmVolume = Options_GetBgmVolume(args->options);
    data->options.sfxVolume = Options_GetSfxVolume(args->options);
    data->options.criesVolume = Options_GetCriesVolume(args->options);

    data->menuInputPtr = args->menuInputStateMgr;
    data->playerOptionsUnused = args->options;
    data->heapID = HEAP_ID_OPTIONS_APP;
    data->playerOptions = args->options;
    data->menuInputState = MenuInputStateMgr_GetState(data->menuInputPtr);
    data->frameNumText = String_New(40, data->heapID);

    TextFlags_SetCanABSpeedUpPrint(FALSE);
    TextFlags_SetCanTouchSpeedUpPrint(FALSE);

    SetMasterBrightnessByColor(PM_LCD_TOP, RGB_BLACK);
    SetMasterBrightnessByColor(PM_LCD_BOTTOM, RGB_BLACK);

    return TRUE;
}

BOOL OptionsMenu_Exit(OverlayManager *manager, int *state) {
    OptionsApp_Data *data = OverlayManager_GetData(manager);

    if (data->exitChoice == 1) {
        data->options.textSpeed = sTextSpeedChoiceRawValues[data->menuEntries[MENU_ENTRY_TEXT_SPEED].value];
        data->options.battleScene = data->menuEntries[MENU_ENTRY_BATTLE_SCENE].value;
        data->options.battleStyle = data->menuEntries[MENU_ENTRY_BATTLE_STYLE].value;
        data->options.soundMethod = data->menuEntries[MENU_ENTRY_SOUND_METHOD].value;
        data->options.frame = data->menuEntries[MENU_ENTRY_FRAME].value;

        data->options.camera3D = data->menuEntries[MENU_ENTRY_3D_CAMERA].value;
        data->options.autosave = data->menuEntries[MENU_ENTRY_AUTOSAVE].value;
        data->options.sendToBoxes = data->menuEntries[MENU_ENTRY_SEND_TO_BOXES].value;
        data->options.giveNicknames = data->menuEntries[MENU_ENTRY_GIVE_NICKNAMES].value;
        data->options.expShare = data->menuEntries[MENU_ENTRY_EXP_SHARE].value;
        data->options.expMode = data->menuEntries[MENU_ENTRY_EXP_MODE].value;
        data->options.bgmVolume = data->menuEntries[MENU_ENTRY_BGM_VOLUME].value;
        data->options.sfxVolume = data->menuEntries[MENU_ENTRY_SFX_VOLUME].value;
        data->options.criesVolume = data->menuEntries[MENU_ENTRY_CRIES_VOLUME].value;

        Options_SetTextSpeed(data->playerOptions, data->options.textSpeed);
        Options_SetBattleScene(data->playerOptions, data->options.battleScene);
        Options_SetBattleStyle(data->playerOptions, data->options.battleStyle);
        Options_SetSoundMethod(data->playerOptions, data->options.soundMethod);
        Options_SetFrame(data->playerOptions, data->options.frame);
        Options_SetCamera3D(data->playerOptions, data->options.camera3D);
        Options_SetAutosave(data->playerOptions, data->options.autosave);
        Options_SetSendToBoxes(data->playerOptions, data->options.sendToBoxes);
        Options_SetGiveNicknames(data->playerOptions, data->options.giveNicknames);
        Options_SetExpShare(data->playerOptions, data->options.expShare);
        Options_SetExpMode(data->playerOptions, data->options.expMode);
        Options_SetBgmVolume(data->playerOptions, data->options.bgmVolume);
        Options_SetSfxVolume(data->playerOptions, data->options.sfxVolume);
        Options_SetCriesVolume(data->playerOptions, data->options.criesVolume);
    } else if (data->exitChoice == 2) {
        GF_SndSetMonoFlag(data->options.soundMethod);
        Options_SetTextSpeed(data->playerOptions, data->options.textSpeed);
    }

    String_Delete(data->frameNumText);

    TextFlags_SetCanABSpeedUpPrint(TRUE);
    TextFlags_SetCanTouchSpeedUpPrint(TRUE);

    OverlayManager_FreeData(manager);
    Heap_Destroy(data->heapID);

    return TRUE;
}

BOOL OptionsMenu_Main(OverlayManager *manager, int *state) {
    OptionsApp_Data *data = OverlayManager_GetData(manager);
    switch (data->exitState) {
    case 0:
        if (!OptionsApp_Setup(data)) {
            return FALSE;
        }

        data->fadeUnused = 0;
        BeginNormalPaletteFade(FADE_BOTH_SCREENS, FADE_TYPE_BRIGHTNESS_IN, FADE_TYPE_BRIGHTNESS_IN, RGB_BLACK, 6, 1, data->heapID);
        SpriteSystem_DrawSprites(data->spriteGfxHandler);
        break;
    case 1:
        SpriteSystem_DrawSprites(data->spriteGfxHandler);
        if (!IsPaletteFadeFinished()) {
            return FALSE;
        }
        break;
    case 2:
        if (data->exitChoice != 0) {
            SpriteSystem_DrawSprites(data->spriteGfxHandler);
            break;
        }
        OptionsApp_HandleInput(data);
        SpriteSystem_DrawSprites(data->spriteGfxHandler);
        return FALSE;
    case 3:
        SpriteSystem_DrawSprites(data->spriteGfxHandler);
        if (!OptionsApp_ConfirmAndQuitButtonsAreDoneAnimating(data)) {
            data->fadeUnused = 0;
            BeginNormalPaletteFade(FADE_BOTH_SCREENS, FADE_TYPE_BRIGHTNESS_OUT, FADE_TYPE_BRIGHTNESS_OUT, RGB_BLACK, 6, 1, data->heapID);
            break;
        }
        return FALSE;
    case 4:
        if (TextPrinterCheckActive(data->textPrinter)) {
            RemoveTextPrinter(data->textPrinter);
        }
        SpriteSystem_DrawSprites(data->spriteGfxHandler);
        if (!IsPaletteFadeFinished()) {
            return FALSE;
        }
        break;
    case 5:
        if (OptionsApp_Free(data)) {
            return TRUE;
        }
        return FALSE;
    }

    data->exitState++;
    return FALSE;
}

static void OptionsApp_SetupGraphicsBanks(void) {
    GraphicsBanks banks = {
        .bg = GX_VRAM_BG_128_A,
        .subbg = GX_VRAM_SUB_BG_128_C,
        .obj = GX_VRAM_OBJ_16_G,
        .subobj = GX_VRAM_SUB_OBJ_16_I,
    };
    GfGfx_SetBanks(&banks);
}

static void OptionsApp_OnVBlank(OptionsApp_Data *data) {
    if (data->reloadFrameGfx) {
        LoadUserFrameGfx2(data->bgConfig, GF_BG_LYR_SUB_1, 0x6D, 15, data->menuEntries[MENU_ENTRY_FRAME].value, data->heapID);
        data->reloadFrameGfx = FALSE;
    }

    SpriteSystem_TransferOam();
    NNS_GfdDoVramTransfer();
    DoScheduledBgGpuUpdates(data->bgConfig);
    OS_SetIrqCheckFlag(OS_IE_VBLANK);
}

static BOOL OptionsApp_Setup(OptionsApp_Data *data) {
    switch (data->setupAndFreeState) {
    case 0:
        Main_SetVBlankIntrCB(NULL, NULL);
        HBlankInterruptDisable();

        GfGfx_DisableEngineAPlanes();
        GfGfx_DisableEngineBPlanes();
        GX_SetVisiblePlane(GX_PLANEMASK_NONE);
        GXS_SetVisiblePlane(GX_PLANEMASK_NONE);

        OptionsApp_SetupGraphicsBanks();

        GX_SetDispSelect(GX_DISP_SELECT_SUB_MAIN);

        ResetVisibleHardwareWindows(PM_LCD_TOP);
        ResetVisibleHardwareWindows(PM_LCD_BOTTOM);

        OptionsApp_SetupBgConfig(data);
        OptionsApp_SetupSpriteRenderer(data);
        break;

    case 1:
        OptionsApp_SetupGraphicsData(data);
        data->msgData = NewMsgDataFromNarc(MSGDATA_LOAD_LAZY, NARC_msgdata_msg, NARC_msg_msg_0045_bin, data->heapID);
        OptionsApp_LoadMenuEntriesData(data);
        break;

    case 2:
        OptionsApp_SetupWindows(data);
        OptionsApp_SetupInterfaceText(data);
        GF_CreateVramTransferManager(32, data->heapID);
        GfGfx_EngineATogglePlanes(GX_PLANEMASK_OBJ, GF_PLANE_TOGGLE_ON);
        WirelessStatusIcon_CreateIfConnected();
        OptionsApp_SetupSprites(data);
        OptionsApp_UpdateRowArrowVisibility(data);

        Main_SetVBlankIntrCB((GFIntrCB)OptionsApp_OnVBlank, data);
        data->setupAndFreeState = 0;
        ToggleBgLayer(GF_BG_LYR_MAIN_0, GF_PLANE_TOGGLE_ON);
        return TRUE;
    }

    data->setupAndFreeState++;
    return FALSE;
}

static BOOL OptionsApp_Free(OptionsApp_Data *data) {
    switch (data->setupAndFreeState) {
    case 0:
        GF_DestroyVramTransferManager();
        OptionsApp_FreeWindows(data);

        for (int i = 0; i < MENU_ENTRY_COUNT - 1; i++) {
            for (int j = 0; j < data->menuEntries[i].numStrings; j++) {
                String_Delete(data->menuEntries[i].strings[j]);
            }
        }

        DestroyMsgData(data->msgData);
        OptionsApp_FreeGraphicsDummy(data);
        OptionsApp_FreeBgConfig(data);
        OptionsApp_FreeSpriteRenderer(data);
        break;
    case 1:
        Main_SetVBlankIntrCB(NULL, NULL);
        HBlankInterruptDisable();
        GfGfx_DisableEngineAPlanes();
        GfGfx_DisableEngineBPlanes();
        GX_SetVisiblePlane(GX_PLANEMASK_NONE);
        GXS_SetVisiblePlane(GX_PLANEMASK_NONE);
        data->setupAndFreeState = 0;
        return TRUE;
    }

    data->setupAndFreeState++;
    return FALSE;
}

static void OptionsApp_SetupBgConfig(OptionsApp_Data *data) {
    data->bgConfig = BgConfig_Alloc(data->heapID);
    GraphicsModes modes = {
        .dispMode = GX_DISPMODE_GRAPHICS,
        .bgMode = GX_BGMODE_0,
        .subMode = GX_BGMODE_0,
        ._2d3dMode = GX_BG0_AS_2D,
    };
    SetBothScreensModesAndDisable(&modes);

    BgTemplate templates[5] = {
        {
         .x = 0,
         .y = 0,
         .bufferSize = 0x800,
         .baseTile = 0,
         .size = GF_BG_SCR_SIZE_256x256,
         .colorMode = GX_BG_COLORMODE_16,
         .screenBase = GX_BG_SCRBASE_0xf800,
         .charBase = GX_BG_CHARBASE_0x00000,
         .bgExtPltt = GX_BG_EXTPLTT_01,
         .priority = 0,
         .areaOver = GX_BG_AREAOVER_XLU,
         .mosaic = FALSE,
         },
        {
         .x = 0,
         .y = 0,
         .bufferSize = 0x800,
         .baseTile = 0,
         .size = GF_BG_SCR_SIZE_256x256,
         .colorMode = GX_BG_COLORMODE_16,
         .screenBase = GX_BG_SCRBASE_0xf000,
         .charBase = GX_BG_CHARBASE_0x04000,
         .bgExtPltt = GX_BG_EXTPLTT_01,
         .priority = 1,
         .areaOver = GX_BG_AREAOVER_XLU,
         .mosaic = FALSE,
         },
        {
         .x = 0,
         .y = 0,
         .bufferSize = 0x800,
         .baseTile = 0,
         .size = GF_BG_SCR_SIZE_256x256,
         .colorMode = GX_BG_COLORMODE_16,
         .screenBase = GX_BG_SCRBASE_0xe800,
         .charBase = GX_BG_CHARBASE_0x00000,
         .bgExtPltt = GX_BG_EXTPLTT_01,
         .priority = 2,
         .areaOver = GX_BG_AREAOVER_XLU,
         .mosaic = FALSE,
         },
        {
         .x = 0,
         .y = 0,
         .bufferSize = 0x800,
         .baseTile = 0,
         .size = GF_BG_SCR_SIZE_256x256,
         .colorMode = GX_BG_COLORMODE_16,
         .screenBase = GX_BG_SCRBASE_0xf800,
         .charBase = GX_BG_CHARBASE_0x00000,
         .bgExtPltt = GX_BG_EXTPLTT_01,
         .priority = 1,
         .areaOver = GX_BG_AREAOVER_XLU,
         .mosaic = FALSE,
         },
        {
         .x = 0,
         .y = 0,
         .bufferSize = 0x800,
         .baseTile = 0,
         .size = GF_BG_SCR_SIZE_256x256,
         .colorMode = GX_BG_COLORMODE_16,
         .screenBase = GX_BG_SCRBASE_0xf000,
         .charBase = GX_BG_CHARBASE_0x08000,
         .bgExtPltt = GX_BG_EXTPLTT_01,
         .priority = 0,
         .areaOver = GX_BG_AREAOVER_XLU,
         .mosaic = FALSE,
         },
    };

    for (int i = 0; i < 5; i++) {
        InitBgFromTemplate(data->bgConfig, sOptionsAppBgLayers[i], &templates[i], GF_BG_TYPE_TEXT);
        BgClearTilemapBufferAndCommit(data->bgConfig, sOptionsAppBgLayers[i]);
    }

    BG_ClearCharDataRange(GF_BG_LYR_MAIN_0, 32, 0, data->heapID);
    BG_ClearCharDataRange(GF_BG_LYR_MAIN_1, 32, 0, data->heapID);
    BG_ClearCharDataRange(GF_BG_LYR_SUB_0, 32, 0, data->heapID);
    BG_ClearCharDataRange(GF_BG_LYR_SUB_1, 32, 0, data->heapID);
}

static void OptionsApp_FreeBgConfig(OptionsApp_Data *data) {
    FreeBgTilemapBuffer(data->bgConfig, GF_BG_LYR_SUB_1);
    FreeBgTilemapBuffer(data->bgConfig, GF_BG_LYR_SUB_0);
    FreeBgTilemapBuffer(data->bgConfig, GF_BG_LYR_MAIN_2);
    FreeBgTilemapBuffer(data->bgConfig, GF_BG_LYR_MAIN_1);
    FreeBgTilemapBuffer(data->bgConfig, GF_BG_LYR_MAIN_0);
    Heap_Free(data->bgConfig);
}

static void OptionsApp_SetupGraphicsData(OptionsApp_Data *data) {
    GfGfxLoader_GXLoadPal(NARC_a_0_7_2, 3, GF_PAL_LOCATION_SUB_BG, GF_PAL_SLOT_0_OFFSET, 0x40, data->heapID);
    GfGfxLoader_LoadCharData(NARC_a_0_7_2, 8, data->bgConfig, GF_BG_LYR_SUB_0, 0, 0, FALSE, data->heapID);
    GfGfxLoader_LoadScrnData(NARC_a_0_7_2, 19, data->bgConfig, GF_BG_LYR_SUB_0, 0, 0, FALSE, data->heapID);
    GfGfxLoader_GXLoadPal(NARC_a_0_7_2, 2, GF_PAL_LOCATION_MAIN_BG, GF_PAL_SLOT_0_OFFSET, 0x40, data->heapID);
    GfGfxLoader_LoadCharData(NARC_a_0_7_2, 7, data->bgConfig, GF_BG_LYR_MAIN_0, 0, 0, FALSE, data->heapID);
    // The backdrop art is 1 px too tall for the screen: scroll it, and the
    // text layer with it, up 1 px (yofs 1 samples the map from y=1). The
    // sprites and the cursor-band offsets (sOptionRowBorderYCoords) carry
    // the same -1 px so everything stays aligned.
    GfGfxLoader_LoadScrnData(NARC_a_0_7_2, 17, data->bgConfig, GF_BG_LYR_MAIN_2, 0, 0, FALSE, data->heapID);
    BgSetPosTextAndCommit(data->bgConfig, GF_BG_LYR_MAIN_2, BG_POS_OP_SET_Y, 1);
    BgSetPosTextAndCommit(data->bgConfig, GF_BG_LYR_MAIN_1, BG_POS_OP_SET_Y, 1);
    GfGfxLoader_LoadScrnData(NARC_a_0_7_2, 18, data->bgConfig, GF_BG_LYR_MAIN_0, 0, 0, FALSE, data->heapID);

    // The cursor always starts on the first row of the first page.
    BgSetPosTextAndCommit(data->bgConfig, GF_BG_LYR_MAIN_0, BG_POS_OP_SET_Y, sOptionRowBorderYCoords[0]);
}

static void OptionsApp_FreeGraphicsDummy(OptionsApp_Data *data) {
    // empty, maybe would've been used to free graphics data?
}

static void OptionsApp_SetupWindows(OptionsApp_Data *data) {
    AddWindowParameterized(data->bgConfig, &data->windows.optionsTitle, GF_BG_LYR_MAIN_1, 1, 0, 12, 3, 13, 0xA);
    AddWindowParameterized(data->bgConfig, &data->windows.selectedOption, GF_BG_LYR_MAIN_1, 1, 3, 30, 18, 13, 0x2E);
    AddWindowParameterized(data->bgConfig, &data->windows.quitButton, GF_BG_LYR_MAIN_1, 24, 21, 7, 3, 13, 0x24A);
    AddWindowParameterized(data->bgConfig, &data->windows.frameAndTextSpeedTest, GF_BG_LYR_SUB_1, 2, 19, 27, 4, 12, 0x1);
    AddWindowParameterized(data->bgConfig, &data->windows.confirmButton, GF_BG_LYR_MAIN_1, 15, 21, 7, 3, 13, 0x25F);
    AddWindowParameterized(data->bgConfig, &data->windows.categoryTabs, GF_BG_LYR_MAIN_1, 8, 0, 24, 3, 13, 0x274);
    // Bottom-left corner: same size/style as Confirm & Quit, 1 tile in from
    // the left edge to mirror Quit's margin on the right.
    AddWindowParameterized(data->bgConfig, &data->windows.resetButton, GF_BG_LYR_MAIN_1, 1, 21, 7, 3, 13, 0x2BC);

    LoadUserFrameGfx2(data->bgConfig, GF_BG_LYR_SUB_1, 0x6D, 15, data->options.frame, data->heapID);
    LoadFontPal0(GF_PAL_LOCATION_MAIN_BG, GF_PAL_SLOT_13_OFFSET, data->heapID);
    LoadFontPal0(GF_PAL_LOCATION_SUB_BG, GF_PAL_SLOT_13_OFFSET, data->heapID);
    LoadFontPal1(GF_PAL_LOCATION_MAIN_BG, GF_PAL_SLOT_12_OFFSET, data->heapID);
    LoadFontPal1(GF_PAL_LOCATION_SUB_BG, GF_PAL_SLOT_12_OFFSET, data->heapID);

    FillWindowPixelBuffer(&data->windows.optionsTitle, 0x00);
    FillWindowPixelBuffer(&data->windows.selectedOption, 0x00);
    FillWindowPixelBuffer(&data->windows.quitButton, 0x00);
    FillWindowPixelBuffer(&data->windows.confirmButton, 0x00);
    FillWindowPixelBuffer(&data->windows.categoryTabs, 0x00);
    FillWindowPixelBuffer(&data->windows.resetButton, 0x00);
    FillWindowPixelBuffer(&data->windows.frameAndTextSpeedTest, 0xFF);

    ClearWindowTilemap(&data->windows.frameAndTextSpeedTest);
    ClearWindowTilemap(&data->windows.selectedOption);
    ClearWindowTilemap(&data->windows.optionsTitle);
    ClearWindowTilemap(&data->windows.categoryTabs);
    ClearWindowTilemap(&data->windows.resetButton);

    DrawFrameAndWindow2(&data->windows.frameAndTextSpeedTest, TRUE, 0x6D, 15);
}

static void OptionsApp_FreeWindows(OptionsApp_Data *data) {
    EraseFrameAndWindow1(&data->windows.selectedOption, FALSE);
    ClearFrameAndWindow2(&data->windows.frameAndTextSpeedTest, FALSE);

    for (u16 i = 0; i < NELEMS(data->windows.asArray); i++) {
        ClearWindowTilemapAndCopyToVram(&data->windows.asArray[i]);
        FillWindowPixelBuffer(&data->windows.asArray[i], 0x00);
        ClearWindowTilemap(&data->windows.asArray[i]);
        RemoveWindow(&data->windows.asArray[i]);
    }
}

static void OptionsApp_PrintTextFrameString(OptionsApp_Data *data, String *frameNumText, BOOL instant) {
    u32 textFrameDelay = Options_GetTextFrameDelay(data->playerOptions);

    if (TextPrinterCheckActive(data->textPrinter)) {
        RemoveTextPrinter(data->textPrinter);
    }

    ReadMsgDataIntoString(data->msgData, msg_0045_00040 + data->menuEntries[MENU_ENTRY_FRAME].value, frameNumText); // WINDOW TYPE XX

    FillWindowPixelBuffer(&data->windows.frameAndTextSpeedTest, 0xFF);

    if (instant) {
        AddTextPrinterParameterizedWithColor(&data->windows.frameAndTextSpeedTest, 1, frameNumText, 4, 0, TEXT_SPEED_INSTANT, MAKE_TEXT_COLOR(1, 2, 15), NULL);
    } else {
        // the sample line types itself at the currently selected text speed
        data->textPrinter = AddTextPrinterParameterizedWithColor(&data->windows.frameAndTextSpeedTest, 1, frameNumText, 4, 0, textFrameDelay, MAKE_TEXT_COLOR(1, 2, 15), NULL);
    }
}

static void OptionsApp_SetupInterfaceText(OptionsApp_Data *data) {
    u16 i;
    String *tmpString = String_New(40, data->heapID);

    ReadMsgDataIntoString(data->msgData, msg_0045_00000, tmpString); // Settings
    AddTextPrinterParameterizedWithColor(&data->windows.optionsTitle, 0, tmpString, 0, 6, TEXT_SPEED_INSTANT, MAKE_TEXT_COLOR(15, 2, 0), NULL);

    String_SetEmpty(tmpString);
    OptionsApp_PrintTextFrameString(data, tmpString, TRUE);

    // Setting name labels for the active page's row slots
    OptionsApp_PrintOptionRowLabels(data);

    String_SetEmpty(tmpString);
    ReadMsgDataIntoString(data->msgData, msg_0045_00008, tmpString); // QUIT
    AddTextPrinterParameterizedWithColor(&data->windows.quitButton, 0, tmpString, 0, 6, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(15, 2, 0), NULL);
    String_SetEmpty(tmpString);
    ReadMsgDataIntoString(data->msgData, msg_0045_00007, tmpString); // CONFIRM
    AddTextPrinterParameterizedWithColor(&data->windows.confirmButton, 0, tmpString, 0, 6, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(15, 2, 0), NULL);
    String_SetEmpty(tmpString);
    ReadMsgDataIntoString(data->msgData, msg_0045_00077, tmpString); // RESET
    AddTextPrinterParameterizedWithColor(&data->windows.resetButton, 0, tmpString, 0, 6, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(15, 2, 0), NULL);

    for (i = 0; i < MENU_ENTRY_COUNT; i++) {
        OptionsApp_UpdateMenuEntry(data, i);
    }

    CopyWindowToVram(&data->windows.optionsTitle);
    CopyWindowToVram(&data->windows.selectedOption);
    CopyWindowToVram(&data->windows.quitButton);
    CopyWindowToVram(&data->windows.confirmButton);
    CopyWindowToVram(&data->windows.resetButton);
    CopyWindowToVram(&data->windows.frameAndTextSpeedTest);
    OptionsApp_UpdateCategoryTabLabels(data);

    String_Delete(tmpString);
}

static void OptionsApp_LoadMenuEntriesData(OptionsApp_Data *data) {
    u16 i, j;
    u16 msgNum = 3; // TEXT_SPEED labels come from sTextSpeedChoiceMsgIds; the shared pool continues after its 3 stock rows
    for (i = 0; i < MENU_ENTRY_COUNT - 1; i++) {
        if (i == MENU_ENTRY_FRAME) {
            msgNum += 2; // skip the removed Button Mode rows ("Normal", "L=A")
        }
        data->menuEntries[i].numStrings = sNumChoicesPerMenuEntry[i];
        for (j = 0; j < sNumChoicesPerMenuEntry[i]; j++) {
            if (i == MENU_ENTRY_TEXT_SPEED) {
                data->menuEntries[i].strings[j] = NewString_ReadMsgData(data->msgData, sTextSpeedChoiceMsgIds[j]);
            } else if (i > MENU_ENTRY_FRAME) {
                data->menuEntries[i].strings[j] = NewString_ReadMsgData(data->msgData, sExtraEntryChoiceMsgIds[i - (MENU_ENTRY_FRAME + 1)][j]); // Added settings
            } else {
                data->menuEntries[i].strings[j] = NewString_ReadMsgData(data->msgData, msg_0045_00009 + msgNum++); // Option values
            }
        }
    }

    data->menuEntries[MENU_ENTRY_TEXT_SPEED].value = (data->options.textSpeed < NELEMS(sTextSpeedRawToChoice)) ? sTextSpeedRawToChoice[data->options.textSpeed] : 2;
    data->menuEntries[MENU_ENTRY_BATTLE_SCENE].value = data->options.battleScene;
    data->menuEntries[MENU_ENTRY_BATTLE_STYLE].value = data->options.battleStyle;
    data->menuEntries[MENU_ENTRY_SOUND_METHOD].value = data->options.soundMethod;

    data->menuEntries[MENU_ENTRY_FRAME].value = data->options.frame;
    data->menuEntries[MENU_ENTRY_6].value = 0;
    // Added settings: seed from the save. Old saves hold garbage in the new
    // words, so clamp the volume rows into the carousel's 0-10 range.
    data->menuEntries[MENU_ENTRY_3D_CAMERA].value = data->options.camera3D;
    data->menuEntries[MENU_ENTRY_AUTOSAVE].value = data->options.autosave;
    data->menuEntries[MENU_ENTRY_SEND_TO_BOXES].value = data->options.sendToBoxes;
    data->menuEntries[MENU_ENTRY_GIVE_NICKNAMES].value = data->options.giveNicknames;
    data->menuEntries[MENU_ENTRY_EXP_SHARE].value = data->options.expShare;
    data->menuEntries[MENU_ENTRY_EXP_MODE].value = data->options.expMode;
    data->menuEntries[MENU_ENTRY_BGM_VOLUME].value = (data->options.bgmVolume < 11) ? data->options.bgmVolume : 10;
    data->menuEntries[MENU_ENTRY_SFX_VOLUME].value = (data->options.sfxVolume < 11) ? data->options.sfxVolume : 10;
    data->menuEntries[MENU_ENTRY_CRIES_VOLUME].value = (data->options.criesVolume < 11) ? data->options.criesVolume : 10;
}

// Row slot the entry occupies on the active page, or -1 when the entry lives
// on another page. MENU_ENTRY_6 (the button row) has no row slot.
static int OptionsApp_GetEntrySlot(const OptionsApp_Data *data, u32 menuEntryId) {
    u16 slot;

    for (slot = 0; slot < sOptionPageRowCounts[data->optionPage]; slot++) {
        if (sOptionPageEntries[data->optionPage][slot] == menuEntryId) {
            return slot;
        }
    }
    return -1;
}

// Setting name labels, printed top-down in the active page's row slots.
static void OptionsApp_PrintOptionRowLabels(OptionsApp_Data *data) {
    u16 i;
    String *tmpString = String_New(40, data->heapID);

    for (i = 0; i < sOptionPageRowCounts[data->optionPage]; i++) {
        u16 entryId = sOptionPageEntries[data->optionPage][i];
        String_SetEmpty(tmpString);
        ReadMsgDataIntoString(data->msgData, sOptionEntryLabelMsgIds[entryId], tmpString); // Setting name
        AddTextPrinterParameterizedWithColor(&data->windows.selectedOption, 0, tmpString, 0, i * 24 + 4, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(15, 2, 0), NULL);
    }

    String_Delete(tmpString);
}

// Category labels, centred in the title band's three tab squares. Same font
// and color as the "Settings" title; the active page's label is raised 1 px
// as the selection marker (keeps the effect free of new artwork).
static void OptionsApp_UpdateCategoryTabLabels(OptionsApp_Data *data) {
    u16 i;
    u16 x;
    String *tmpString = String_New(40, data->heapID);

    FillWindowPixelBuffer(&data->windows.categoryTabs, 0x00);
    for (i = 0; i < NELEMS(sCategoryTabMsgIds); i++) {
        String_SetEmpty(tmpString);
        ReadMsgDataIntoString(data->msgData, sCategoryTabMsgIds[i], tmpString);
        x = sCategoryTabLabelXCoords[i] - FontID_String_GetWidth(0, tmpString, 0) / 2;
        AddTextPrinterParameterizedWithColor(&data->windows.categoryTabs, 0, tmpString, x, (i == data->optionPage) ? 5 : 6, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(15, 2, 0), NULL);
    }
    CopyWindowToVram(&data->windows.categoryTabs);

    String_Delete(tmpString);
}

// Arrows only exist for the active page's rows; empty rows hide theirs.
static void OptionsApp_UpdateRowArrowVisibility(OptionsApp_Data *data) {
    u16 slot;
    u16 dir;

    for (slot = 0; slot < OPTION_ROW_COUNT; slot++) {
        BOOL visible = (slot < sOptionPageRowCounts[data->optionPage]);
        for (dir = 0; dir < 2; dir++) {
            Sprite *sprite = data->sprites[sRowArrowSprites[slot][dir]];
            Sprite_SetDrawFlag(sprite, visible);
            if (visible) {
                Sprite_SetAnimCtrlSeq(sprite, 0);
            }
        }
    }
}

// D-pad up/down walks the active page's rows, then the always-present
// button-row entry, wrapping.
static void OptionsApp_MoveSelectionCursor(OptionsApp_Data *data, s32 direction) {
    u32 count = sOptionPageRowCounts[data->optionPage] + 1;
    u32 pos;

    if (data->currentMenuEntryId == MENU_ENTRY_6) {
        pos = count - 1;
    } else {
        pos = (u32)OptionsApp_GetEntrySlot(data, data->currentMenuEntryId);
    }
    pos = (pos + (u32)(direction + (s32)count)) % count;
    data->currentMenuEntryId = (pos + 1 == count) ? MENU_ENTRY_6 : sOptionPageEntries[data->optionPage][pos];
    OptionsApp_UpdateSelectionHighlight(data, data->currentMenuEntryId);
}

static void OptionsApp_SwitchOptionPage(OptionsApp_Data *data, u32 page) {
    u16 i;

    if (page == data->optionPage || page >= NELEMS(sOptionPageRowCounts)) {
        return;
    }
    data->optionPage = page;

    // Keep the cursor on its entry when that entry is on the new page,
    // otherwise drop it onto the new page's first row.
    if (data->currentMenuEntryId != MENU_ENTRY_6 && OptionsApp_GetEntrySlot(data, data->currentMenuEntryId) < 0) {
        data->currentMenuEntryId = sOptionPageEntries[page][0];
    }

    FillWindowPixelBuffer(&data->windows.selectedOption, 0x00);
    OptionsApp_PrintOptionRowLabels(data);
    for (i = 0; i < sOptionPageRowCounts[page]; i++) {
        OptionsApp_UpdateMenuEntry(data, sOptionPageEntries[page][i]);
    }
    OptionsApp_UpdateRowArrowVisibility(data);
    OptionsApp_UpdateCategoryTabLabels(data);
    OptionsApp_UpdateSelectionHighlight(data, data->currentMenuEntryId);
    PlaySE(SEQ_SE_DP_SELECT);
}

// Reset button: snap every option back to its Options_Init default and
// persist immediately (same write path as Confirm), then repaint.
static void OptionsApp_ResetToDefaults(OptionsApp_Data *data) {
    u16 i;

    data->menuEntries[MENU_ENTRY_TEXT_SPEED].value = 2;     // Medium
    data->menuEntries[MENU_ENTRY_BATTLE_SCENE].value = 0;   // On
    data->menuEntries[MENU_ENTRY_BATTLE_STYLE].value = 1;   // Set
    data->menuEntries[MENU_ENTRY_SOUND_METHOD].value = 0;   // Stereo
    data->menuEntries[MENU_ENTRY_FRAME].value = 0;          // frame type 1
    data->menuEntries[MENU_ENTRY_3D_CAMERA].value = 0;      // On
    data->menuEntries[MENU_ENTRY_AUTOSAVE].value = 0;       // On
    data->menuEntries[MENU_ENTRY_SEND_TO_BOXES].value = 0;  // On
    data->menuEntries[MENU_ENTRY_GIVE_NICKNAMES].value = 0; // On
    data->menuEntries[MENU_ENTRY_EXP_SHARE].value = 0;      // On
    data->menuEntries[MENU_ENTRY_EXP_MODE].value = 1;       // Deferred
    data->menuEntries[MENU_ENTRY_BGM_VOLUME].value = 10;
    data->menuEntries[MENU_ENTRY_SFX_VOLUME].value = 10;
    data->menuEntries[MENU_ENTRY_CRIES_VOLUME].value = 10;

    data->options.textSpeed = sTextSpeedChoiceRawValues[data->menuEntries[MENU_ENTRY_TEXT_SPEED].value];
    data->options.battleScene = data->menuEntries[MENU_ENTRY_BATTLE_SCENE].value;
    data->options.battleStyle = data->menuEntries[MENU_ENTRY_BATTLE_STYLE].value;
    data->options.soundMethod = data->menuEntries[MENU_ENTRY_SOUND_METHOD].value;
    data->options.frame = data->menuEntries[MENU_ENTRY_FRAME].value;
    data->options.camera3D = data->menuEntries[MENU_ENTRY_3D_CAMERA].value;
    data->options.autosave = data->menuEntries[MENU_ENTRY_AUTOSAVE].value;
    data->options.sendToBoxes = data->menuEntries[MENU_ENTRY_SEND_TO_BOXES].value;
    data->options.giveNicknames = data->menuEntries[MENU_ENTRY_GIVE_NICKNAMES].value;
    data->options.expShare = data->menuEntries[MENU_ENTRY_EXP_SHARE].value;
    data->options.expMode = data->menuEntries[MENU_ENTRY_EXP_MODE].value;
    data->options.bgmVolume = data->menuEntries[MENU_ENTRY_BGM_VOLUME].value;
    data->options.sfxVolume = data->menuEntries[MENU_ENTRY_SFX_VOLUME].value;
    data->options.criesVolume = data->menuEntries[MENU_ENTRY_CRIES_VOLUME].value;

    Options_SetTextSpeed(data->playerOptions, data->options.textSpeed);
    Options_SetBattleScene(data->playerOptions, data->options.battleScene);
    Options_SetBattleStyle(data->playerOptions, data->options.battleStyle);
    Options_SetSoundMethod(data->playerOptions, data->options.soundMethod);
    Options_SetFrame(data->playerOptions, data->options.frame);
    Options_SetCamera3D(data->playerOptions, data->options.camera3D);
    Options_SetAutosave(data->playerOptions, data->options.autosave);
    Options_SetSendToBoxes(data->playerOptions, data->options.sendToBoxes);
    Options_SetGiveNicknames(data->playerOptions, data->options.giveNicknames);
    Options_SetExpShare(data->playerOptions, data->options.expShare);
    Options_SetExpMode(data->playerOptions, data->options.expMode);
    Options_SetBgmVolume(data->playerOptions, data->options.bgmVolume);
    Options_SetSfxVolume(data->playerOptions, data->options.sfxVolume);
    Options_SetCriesVolume(data->playerOptions, data->options.criesVolume);

    // Side effects (text-speed sample retype, mono flag, frame GFX reload)
    // run through the entries' update path, same as at setup.
    for (i = 0; i < MENU_ENTRY_COUNT; i++) {
        OptionsApp_UpdateMenuEntry(data, i);
    }

    Sprite_SetAnimCtrlSeq(data->sprites[15], 3); // press animation
    PlaySE(SEQ_SE_GS_GEARCANCEL);
}

static void OptionsApp_UpdateMenuEntry(OptionsApp_Data *data, u16 menuEntryId) {
    String *choice;
    u16 x;
    int slot = OptionsApp_GetEntrySlot(data, menuEntryId);

    // A slot of -1 (entry on another page, or the button row) skips the
    // redraw; its side effects at the bottom still apply. Each row shows a
    // single carousel value inside the backdrop band, centred between its
    // arrow sprites.
    if (slot >= 0) {
        FillWindowPixelRect(&data->windows.selectedOption, 0, 108 + sOptionsApp_ClearRectXOffsets[menuEntryId], slot * 24 + 4, 384, 24);
        choice = data->menuEntries[menuEntryId].strings[data->menuEntries[menuEntryId].value];
        x = OPTION_CHOICE_LABEL_CENTER_X - FontID_String_GetWidth(0, choice, 0) / 2;
        AddTextPrinterParameterizedWithColor(&data->windows.selectedOption, 0, choice, x, slot * 24 + 4, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(15, 2, 0), NULL);
        CopyWindowToVram(&data->windows.selectedOption);
    }

    switch (menuEntryId) {
    case MENU_ENTRY_FRAME:
        OptionsApp_PrintTextFrameString(data, data->frameNumText, TRUE);
        data->reloadFrameGfx = TRUE;
        break;
    case MENU_ENTRY_TEXT_SPEED:
        Options_SetTextSpeed(data->playerOptions, sTextSpeedChoiceRawValues[data->menuEntries[menuEntryId].value]);
        OptionsApp_PrintTextFrameString(data, data->frameNumText, FALSE);
        break;
    case MENU_ENTRY_SOUND_METHOD:
        GF_SndSetMonoFlag(data->menuEntries[menuEntryId].value);
        break;
    }
}

static void OptionsApp_UpdateMenuEntryCarousel(OptionsApp_Data *data, u32 menuEntryId, OptionsApp_MenuEntry *menuEntry, s32 offset) {
    if (offset == 0) {
        return;
    }

    int slot = OptionsApp_GetEntrySlot(data, menuEntryId);
    if (slot >= 0) {
        Sprite_SetAnimCtrlSeq(data->sprites[sRowArrowSprites[slot][offset > 0]], 1);
    }

    if (offset > 0) {
        menuEntry->value = (menuEntry->value + offset) % menuEntry->numStrings;
    } else {
        menuEntry->value = (menuEntry->value + menuEntry->numStrings - 1) % menuEntry->numStrings;
    }
}

static void OptionsApp_HandleKeyInput(OptionsApp_Data *data, OptionsApp_MenuEntry *menuEntry) {
    // L/R cycle the category pages in a loop. The shoulder buttons only show
    // up in newKeysRaw (newKeys is masked to dpad + face buttons).
    // SwitchOptionPage ignores the no-op case and plays its own SE.
    if (gSystem.newKeysRaw & PAD_BUTTON_R) {
        OptionsApp_SwitchOptionPage(data, (data->optionPage + 1) % NELEMS(sOptionPageRowCounts));
    } else if (gSystem.newKeysRaw & PAD_BUTTON_L) {
        OptionsApp_SwitchOptionPage(data, (data->optionPage + NELEMS(sOptionPageRowCounts) - 1) % NELEMS(sOptionPageRowCounts));
    }

    if (data->currentMenuEntryId != MENU_ENTRY_6) {
        if (gSystem.newKeys & PAD_KEY_RIGHT) {
            OptionsApp_UpdateMenuEntryCarousel(data, data->currentMenuEntryId, menuEntry, 1);
            OptionsApp_UpdateMenuEntry(data, data->currentMenuEntryId);
            PlaySE(SEQ_SE_DP_SELECT);
        } else if (gSystem.newKeys & PAD_KEY_LEFT) {
            OptionsApp_UpdateMenuEntryCarousel(data, data->currentMenuEntryId, menuEntry, -1);
            OptionsApp_UpdateMenuEntry(data, data->currentMenuEntryId);
            PlaySE(SEQ_SE_DP_SELECT);
        }
    } else {
        // Button-row carousel: on screen left-to-right Reset, Confirm,
        // Quit; values 2, 1, 0. Left walks left, wrapping.
        if (gSystem.newKeys & PAD_KEY_LEFT) {
            data->menuEntries[data->currentMenuEntryId].value = (data->menuEntries[data->currentMenuEntryId].value + 1) % 3;
            OptionsApp_UpdateSelectionHighlight(data, data->currentMenuEntryId);
            PlaySE(SEQ_SE_DP_SELECT);
        } else if (gSystem.newKeys & PAD_KEY_RIGHT) {
            data->menuEntries[data->currentMenuEntryId].value = (data->menuEntries[data->currentMenuEntryId].value + 2) % 3;
            OptionsApp_UpdateSelectionHighlight(data, data->currentMenuEntryId);
            PlaySE(SEQ_SE_DP_SELECT);
        }
    }

    if (gSystem.newKeys & PAD_KEY_UP) {
        OptionsApp_MoveSelectionCursor(data, -1);
        PlaySE(SEQ_SE_DP_SELECT);
    } else if (gSystem.newKeys & PAD_KEY_DOWN) {
        OptionsApp_MoveSelectionCursor(data, 1);
        PlaySE(SEQ_SE_DP_SELECT);
    } else if ((gSystem.newKeys & PAD_BUTTON_A) && data->currentMenuEntryId == MENU_ENTRY_6) {
        if (data->menuEntries[data->currentMenuEntryId].value == 2) {
            OptionsApp_ResetToDefaults(data);
        } else if (data->menuEntries[data->currentMenuEntryId].value == 1) {
            MenuInputStateMgr_SetState(data->menuInputPtr, MENU_INPUT_STATE_BUTTONS);
            PlaySE(SEQ_SE_DP_SAVE);
            Sprite_SetAnimCtrlSeq(data->sprites[8], 3);
            data->exitChoice = 1;
        } else {
            MenuInputStateMgr_SetState(data->menuInputPtr, MENU_INPUT_STATE_BUTTONS);
            PlaySE(SEQ_SE_GS_GEARCANCEL);
            Sprite_SetAnimCtrlSeq(data->sprites[7], 3);
            data->exitChoice = 2;
        }
    } else if (gSystem.newKeys & PAD_BUTTON_B) {
        MenuInputStateMgr_SetState(data->menuInputPtr, MENU_INPUT_STATE_BUTTONS);
        PlaySE(SEQ_SE_GS_GEARCANCEL);
        if (data->currentMenuEntryId == MENU_ENTRY_6 && data->menuEntries[data->currentMenuEntryId].value == 0) {
            Sprite_SetAnimCtrlSeq(data->sprites[7], 3);
        } else {
            Sprite_SetAnimCtrlSeq(data->sprites[7], 2);
        }
        data->exitChoice = 2;
    }
}

static void OptionsApp_HandleInput(OptionsApp_Data *data) {
    if (gSystem.touchNew != 0) {
        const int hitboxIndex = TouchscreenHitbox_FindRectAtTouchNew(sOptionsAppTouchscreenHitboxes);
        switch (hitboxIndex) {
        case -1:
            break;

        case 15: // General tab
            OptionsApp_SwitchOptionPage(data, 0);
            break;

        case 16: // Battle tab
            OptionsApp_SwitchOptionPage(data, 1);
            break;

        case 17: // Audio tab
            OptionsApp_SwitchOptionPage(data, 2);
            break;

        case 18: // Reset button
            OptionsApp_ResetToDefaults(data);
            break;

        case 13: // Confirm button
            data->currentMenuEntryId = MENU_ENTRY_6;
            OptionsApp_HideBorderOnConfirmRow(data);
            data->exitChoice = 1;
            PlaySE(SEQ_SE_DP_SAVE);
            data->menuInputState = 1;
            MenuInputStateMgr_SetState(data->menuInputPtr, MENU_INPUT_STATE_TOUCH);
            data->menuEntries[data->currentMenuEntryId].value = 1;
            OptionsApp_UpdateSelectionHighlight(data, data->currentMenuEntryId);
            Sprite_SetAnimCtrlSeq(data->sprites[8], 3);
            break;

        case 14: // Quit button
            data->currentMenuEntryId = MENU_ENTRY_6;
            OptionsApp_HideBorderOnConfirmRow(data);
            data->exitChoice = 2;
            PlaySE(SEQ_SE_GS_GEARCANCEL);
            data->menuInputState = 1;
            MenuInputStateMgr_SetState(data->menuInputPtr, MENU_INPUT_STATE_TOUCH);
            data->menuEntries[data->currentMenuEntryId].value = 0;
            OptionsApp_UpdateSelectionHighlight(data, data->currentMenuEntryId);
            Sprite_SetAnimCtrlSeq(data->sprites[7], 3);
            break;

        default: {
            // every remaining hitbox is a carousel arrow: 3 = left, 4 = right;
            // it picks the entry the active page shows in that row slot
            u32 row = sOptionsAppHitboxRowSides[hitboxIndex][0];
            if (row >= sOptionPageRowCounts[data->optionPage]) {
                break; // row slot empty on this page
            }
            data->currentMenuEntryId = sOptionPageEntries[data->optionPage][row];
            OptionsApp_MenuEntry *entry = &data->menuEntries[data->currentMenuEntryId];

            u32 value = sOptionsAppHitboxRowSides[hitboxIndex][1];
            OptionsApp_UpdateMenuEntryCarousel(data, data->currentMenuEntryId, entry, (value == 3) ? -1 : 1);
            OptionsApp_UpdateMenuEntry(data, data->currentMenuEntryId);
            OptionsApp_UpdateSelectionHighlight(data, data->currentMenuEntryId);
            OptionsApp_HideBorderOnConfirmRow(data);
            data->menuInputState = 1;
            PlaySE(SEQ_SE_DP_SELECT);
            break;
        }
        }
    } else if (gSystem.newKeys != 0 || gSystem.newKeysRaw != 0) {
        // newKeysRaw additionally covers L/R; unused keys fall through.
        OptionsApp_HandleKeyInput(data, &data->menuEntries[data->currentMenuEntryId]);
    }
}

static void OptionsApp_UpdateSelectionHighlight(OptionsApp_Data *data, u32 menuEntryId) {
    if (menuEntryId == MENU_ENTRY_6) {
        ToggleBgLayer(GF_BG_LYR_MAIN_0, GF_PLANE_TOGGLE_OFF);
        if (data->menuEntries[menuEntryId].value == 0) {  // Quit
            Sprite_SetAnimCtrlSeq(data->sprites[7], 1);
            Sprite_SetAnimCtrlSeq(data->sprites[8], 0);
            Sprite_SetAnimCtrlSeq(data->sprites[15], 0);
        } else if (data->menuEntries[menuEntryId].value == 1) {  // Confirm
            Sprite_SetAnimCtrlSeq(data->sprites[7], 0);
            Sprite_SetAnimCtrlSeq(data->sprites[8], 1);
            Sprite_SetAnimCtrlSeq(data->sprites[15], 0);
        } else {  // Reset
            Sprite_SetAnimCtrlSeq(data->sprites[7], 0);
            Sprite_SetAnimCtrlSeq(data->sprites[8], 0);
            Sprite_SetAnimCtrlSeq(data->sprites[15], 1);
        }
    } else {
        BgSetPosTextAndCommit(data->bgConfig, GF_BG_LYR_MAIN_0, BG_POS_OP_SET_Y, sOptionRowBorderYCoords[OptionsApp_GetEntrySlot(data, menuEntryId)]);
        Sprite_SetAnimCtrlSeq(data->sprites[7], 0);
        Sprite_SetAnimCtrlSeq(data->sprites[8], 0);
        Sprite_SetAnimCtrlSeq(data->sprites[15], 0);
        ToggleBgLayer(GF_BG_LYR_MAIN_0, GF_PLANE_TOGGLE_ON);
    }
}

static void OptionsApp_HideBorderOnConfirmRow(OptionsApp_Data *data) {
    if (data->currentMenuEntryId == MENU_ENTRY_6) {
        ToggleBgLayer(GF_BG_LYR_MAIN_0, GF_PLANE_TOGGLE_OFF);
    }
}

static void OptionsApp_SetupSpriteRenderer(OptionsApp_Data *data) {
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_OBJ, GF_PLANE_TOGGLE_ON);
    GfGfx_EngineBTogglePlanes(GX_PLANEMASK_OBJ, GF_PLANE_TOGGLE_ON);

    data->spriteRenderer = SpriteSystem_Alloc(data->heapID);
    data->spriteGfxHandler = SpriteManager_New(data->spriteRenderer);

    const OamManagerParam oamManagerParam = {
        .fromOBJmain = 0,
        .numOBJmain = 128,
        .fromAffineMain = 0,
        .numAffineMain = 32,
        .fromOBJsub = 0,
        .numOBJsub = 128,
        .fromAffineSub = 0,
        .numAffineSub = 32,
    };
    const OamCharTransferParam oamTransferParam = {
        .maxTasks = 9,
        .sizeMain = 0x400,
        .sizeSub = 0x400,
        .charModeMain = GX_OBJVRAMMODE_CHAR_1D_32K,
        .charModeSub = GX_OBJVRAMMODE_CHAR_1D_32K,
    };
    SpriteSystem_Init(data->spriteRenderer, &oamManagerParam, &oamTransferParam, 32);
    SpriteSystem_InitSprites(data->spriteRenderer, data->spriteGfxHandler, NELEMS(data->sprites));

    ResdatIdList fileIdList = {
        .charRes = NARC_resdat_resdat_00000022_bin,
        .plttRes = NARC_resdat_resdat_00000023_bin,
        .cellRes = NARC_resdat_resdat_00000021_bin,
        .animRes = NARC_resdat_resdat_00000020_bin,
        .mcelRes = 0xFFFF,
        .manmRes = 0xFFFF,
        .headerId = NARC_resdat_resdat_00000077_bin,
    };
    SpriteSystem_LoadResourceDataFromFileIdList(data->spriteRenderer, data->spriteGfxHandler, &fileIdList);

    G2dRenderer_SetSubSurfaceCoords(SpriteSystem_GetRenderer(data->spriteRenderer), FX32_CONST(0), FX32_CONST(256));
}

static void OptionsApp_FreeSpriteRenderer(OptionsApp_Data *data) {
    SpriteSystem_DestroySpriteManager(data->spriteRenderer, data->spriteGfxHandler);
    SpriteSystem_Free(data->spriteRenderer);
    data->spriteGfxHandler = NULL;
}

static void OptionsApp_SetupSprites(OptionsApp_Data *data) {
    for (u16 i = 0; i < NELEMS(data->sprites); i++) {
        // Slot 0 is the retired Text Speed cursor button: every row is a
        // carousel now, so it is never created and nothing can redraw it.
        if (i == 0) {
            data->sprites[i] = NULL;
            continue;
        }
        data->sprites[i] = SpriteSystem_CreateSpriteFromResourceHeader(data->spriteRenderer, data->spriteGfxHandler, &sOptionsAppSpriteTemplates[i]);
        thunk_Sprite_SetPriority(data->sprites[i], 2);
        Sprite_SetAnimActiveFlag(data->sprites[i], TRUE);
    }

    Sprite_SetDrawFlag(data->sprites[7], TRUE);
}

static BOOL OptionsApp_ConfirmAndQuitButtonsAreDoneAnimating(OptionsApp_Data *data) {
    if (Sprite_IsAnimated(data->sprites[7]) == 0 && Sprite_IsAnimated(data->sprites[8]) == 0) {
        return FALSE;
    }

    return TRUE;
}
