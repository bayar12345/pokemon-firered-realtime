// Optional playtest tools. Saved in seven formerly unused SaveBlock2 bytes;
// all actions are gated by an explicit, default-off switch.
#include "global.h"
#include "gflib.h"
#include "god_mode.h"
#if PLAYTEST_TOOLS
#include "battle.h"
#include "data.h"
#include "event_data.h"
#include "menu.h"
#include "new_menu_helpers.h"
#include "overworld.h"
#include "pokedex.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "scanline_effect.h"
#include "sound.h"
#include "task.h"
#include "constants/moves.h"
#include "constants/opponents.h"
#include "constants/pokedex.h"
#include "constants/songs.h"

#define GOD_MAGIC 0x32444F47 // GOD2
#define GOD_ROWS 10
#define GOD_VISIBLE_MOVES 8

enum { GOD_MAIN, GOD_CONFIRM, GOD_FILL, GOD_PARTY, GOD_SLOT, GOD_MOVES, GOD_NOTICE, GOD_EXIT };
enum { GOD_ENABLE, GOD_LEVEL, GOD_PC, GOD_PARTY_LEVEL, GOD_RELEARN, GOD_HMS, GOD_BADGES, GOD_CHAMPION, GOD_DEX, GOD_BACK };

struct GodMenu
{
    u16 tilemap[32 * 32];
    bool8 seen[NUM_SPECIES];
    u16 moves[64];
    u16 moveCount;
    u16 moveCursor;
    u16 nextDex;
    u16 pcCursor;
    u16 added;
    u8 state, cursor, party, slot, action;
    u8 level;
    bool8 enabled;
    bool8 allMoves;
    bool8 hmUnlock;
};

static EWRAM_DATA struct GodMenu *sGod = NULL;
static const struct BgTemplate sBg[] = {{.bg = 0, .charBaseIndex = 0, .mapBaseIndex = 31, .priority = 0}};
static const struct WindowTemplate sWindows[] = {
    {.bg = 0, .tilemapLeft = 1, .tilemapTop = 1, .width = 28, .height = 18, .paletteNum = 0, .baseBlock = 1},
    DUMMY_WIN_TEMPLATE
};
static const u16 sPal[] = {RGB(2,3,5), RGB(29,30,31), RGB(12,13,16), RGB(31,23,6)};
static const u8 sWhite[] = {0,1,2};
static const u8 sGold[] = {0,3,2};
static const u8 sText_Title[] = _("GOD MODE - TEST TOOLS");
static const u8 sText_Off[] = _("OFF");
static const u8 sText_On[] = _("ON");
static const u8 sText_Level[] = _("TEST LEVEL");
static const u8 sText_Enable[] = _("GOD MODE");
static const u8 sText_Pc[] = _("ADD MISSING POKéMON TO PC");
static const u8 sText_PartyLevel[] = _("SET PARTY LEVEL + HEAL");
static const u8 sText_Moves[] = _("RELEARN / TEST MOVES");
static const u8 sText_Hms[] = _("HM UNLOCK");
static const u8 sText_Badges[] = _("ALL BADGES");
static const u8 sText_Champion[] = _("CHAMPION / E4 FLAGS");
static const u8 sText_Dex[] = _("NATIONAL POKéDEX");
static const u8 sText_Back[] = _("BACK");
static const u8 sText_Cursor[] = _(">");
static const u8 sText_Controls[] = _("UP/DOWN: choose  A: select  B: back");
static const u8 sText_LevelHelp[] = _("LEFT/RIGHT: 1 level   L/R: 10 levels");
static const u8 sText_Locked[] = _("Turn GOD MODE ON first.");
static const u8 sText_Confirm[] = _("Apply this change?\nA: confirm   B: cancel");
static const u8 sText_PcConfirm[] = _("Add missing species at TEST LEVEL?\nExisting POKéMON stay unchanged.\nA: confirm   B: cancel");
static const u8 sText_PartyConfirm[] = _("Change party levels and heal them?\nSpecies and moves stay unchanged.\nA: confirm   B: cancel");
static const u8 sText_NoRoom[] = _("Not enough empty PC slots.\nNo POKéMON were changed.");
static const u8 sText_Done[] = _("Done. Save normally to keep changes.\nA/B: return");
static const u8 sText_Filling[] = _("Adding missing POKéMON...\nPlease wait.");
static const u8 sText_EmptyParty[] = _("You need a POKéMON in your party.");
static const u8 sText_PickParty[] = _("CHOOSE A PARTY POKéMON");
static const u8 sText_PickSlot[] = _("CHOOSE A MOVE SLOT TO REPLACE");
static const u8 sText_MoveHelp[] = _("L/R: page   SELECT: learnset/all moves\nA: teach selected move   B: back");
static const u8 sText_Learnset[] = _("RELEARN - SPECIES LEARNSET");
static const u8 sText_AllMoves[] = _("TEST LAB - ALL MOVES");
static const u8 sText_Taught[] = _("Move taught. PP restored for that slot.\nA/B: return");
static const u8 sText_Empty[] = _("---");
static const u8 *const sLabels[GOD_ROWS] = {sText_Enable,sText_Level,sText_Pc,sText_PartyLevel,sText_Moves,sText_Hms,sText_Badges,sText_Champion,sText_Dex,sText_Back};

static void GodDraw(void);

static void GodSaveSettings(void)
{
    u32 magic = GOD_MAGIC;
    CpuCopy32(&magic, gSaveBlock2Ptr->filler_90, sizeof(magic));
    gSaveBlock2Ptr->filler_90[4] = sGod->enabled;
    gSaveBlock2Ptr->filler_90[5] = sGod->level;
    gSaveBlock2Ptr->filler_90[6] = sGod->hmUnlock ? 0xA5 : 0;
}

static bool8 GodAllBadges(void)
{
    u8 i;
    for (i = 0; i < NUM_BADGES; i++)
        if (!FlagGet(FLAG_BADGE01_GET + i))
            return FALSE;
    return TRUE;
}

static void GodPrint(u16 x, u16 y, const u8 *text, bool8 gold)
{
    AddTextPrinterParameterized3(0, FONT_SMALL, x, y, gold ? sGold : sWhite, TEXT_SKIP_DRAW, text);
}

static void GodNotice(const u8 *message)
{
    sGod->state = GOD_NOTICE;
    FillWindowPixelBuffer(0, PIXEL_FILL(0));
    GodPrint(3, 4, sText_Title, TRUE);
    GodPrint(3, 42, message, FALSE);
    PutWindowTilemap(0);
    CopyWindowToVram(0, COPYWIN_FULL);
}

static u16 GodMoveAt(u16 index)
{
    return sGod->allMoves ? index + 1 : sGod->moves[index];
}

static void GodDraw(void)
{
    u8 i, count;
    u16 start, move;
    u8 text[POKEMON_NAME_LENGTH + 8];
    FillWindowPixelBuffer(0, PIXEL_FILL(0));
    if (sGod->state == GOD_MAIN || sGod->state == GOD_CONFIRM)
    {
        GodPrint(3, 0, sText_Title, TRUE);
        for (i = 0; i < GOD_ROWS; i++)
        {
            GodPrint(12, 16 + i * 11, sLabels[i], i == sGod->cursor);
            if (i == sGod->cursor)
                GodPrint(0, 16 + i * 11, sText_Cursor, TRUE);
            if (i == GOD_ENABLE || i == GOD_HMS || i == GOD_BADGES || i == GOD_CHAMPION || i == GOD_DEX)
            {
                bool8 on = i == GOD_ENABLE ? sGod->enabled : i == GOD_HMS ? sGod->enabled && sGod->hmUnlock : i == GOD_BADGES ? GodAllBadges() : i == GOD_CHAMPION ? FlagGet(FLAG_SYS_GAME_CLEAR) : IsNationalPokedexEnabled();
                GodPrint(195, 16 + i * 11, on ? sText_On : sText_Off, TRUE);
            }
            if (i == GOD_LEVEL)
            {
                ConvertIntToDecimalStringN(text, sGod->level, STR_CONV_MODE_LEFT_ALIGN, 3);
                GodPrint(195, 16 + i * 11, text, TRUE);
            }
        }
        if (sGod->state == GOD_CONFIRM)
        {
            // Replace the options with an explicit confirmation, avoiding
            // accidental persistent PC or story edits from a single press.
            FillWindowPixelBuffer(0, PIXEL_FILL(0));
            GodPrint(3, 4, sLabels[sGod->action], TRUE);
            GodPrint(3, 35, sGod->action == GOD_PC ? sText_PcConfirm : sGod->action == GOD_PARTY_LEVEL ? sText_PartyConfirm : sText_Confirm, FALSE);
        }
        else
        {
            GodPrint(0, 128, sGod->cursor == GOD_LEVEL ? sText_LevelHelp : sText_Controls, FALSE);
        }
    }
    else if (sGod->state == GOD_PARTY || sGod->state == GOD_SLOT)
    {
        GodPrint(0, 0, sGod->state == GOD_PARTY ? sText_PickParty : sText_PickSlot, TRUE);
        count = sGod->state == GOD_PARTY ? gPlayerPartyCount : MAX_MON_MOVES;
        for (i = 0; i < count; i++)
        {
            if (sGod->state == GOD_PARTY)
            {
                GetMonData(&gPlayerParty[i], MON_DATA_NICKNAME, text);
                StringGet_Nickname(text);
            }
            else
            {
                move = GetMonData(&gPlayerParty[sGod->party], MON_DATA_MOVE1 + i);
                StringCopy(text, move == MOVE_NONE ? sText_Empty : gMoveNames[move]);
            }
            GodPrint(13, 20 + i * 15, text, i == sGod->cursor);
            if (i == sGod->cursor)
                GodPrint(0, 20 + i * 15, sText_Cursor, TRUE);
        }
        GodPrint(0, 128, sText_Controls, FALSE);
    }
    else if (sGod->state == GOD_MOVES)
    {
        GodPrint(0, 0, sGod->allMoves ? sText_AllMoves : sText_Learnset, TRUE);
        start = sGod->moveCursor / GOD_VISIBLE_MOVES * GOD_VISIBLE_MOVES;
        for (i = 0; i < GOD_VISIBLE_MOVES && start + i < sGod->moveCount; i++)
        {
            move = GodMoveAt(start + i);
            GodPrint(13, 17 + i * 12, gMoveNames[move], start + i == sGod->moveCursor);
            if (start + i == sGod->moveCursor)
                GodPrint(0, 17 + i * 12, sText_Cursor, TRUE);
        }
        GodPrint(0, 118, sText_MoveHelp, FALSE);
    }
    PutWindowTilemap(0);
    CopyWindowToVram(0, COPYWIN_FULL);
}

static void GodBuildMoves(void)
{
    u16 species = GetMonData(&gPlayerParty[sGod->party], MON_DATA_SPECIES);
    u16 i, j, move;
    sGod->moveCount = 0;
    sGod->moveCursor = 0;
    if (sGod->allMoves)
    {
        sGod->moveCount = MOVES_COUNT - 1;
        return;
    }
    // All levels in this species' learnset are available in the test menu,
    // including on low-level evolved forms. Nothing changes outside this menu.
    for (i = 0; gLevelUpLearnsets[species][i] != 0xFFFF; i++)
    {
        move = gLevelUpLearnsets[species][i] & 0x1FF;
        for (j = 0; j < sGod->moveCount && sGod->moves[j] != move; j++);
        if (j == sGod->moveCount && sGod->moveCount < ARRAY_COUNT(sGod->moves))
            sGod->moves[sGod->moveCount++] = move;
    }
}

static void GodBeginPc(void)
{
    u16 i, species, freeSlots = 0, needed = 0;
    memset(sGod->seen, 0, sizeof(sGod->seen));
    for (i = 0; i < TOTAL_BOXES_COUNT * IN_BOX_COUNT; i++)
    {
        species = GetBoxMonDataAt(i / IN_BOX_COUNT, i % IN_BOX_COUNT, MON_DATA_SPECIES);
        if (species == SPECIES_NONE)
            freeSlots++;
        else if (species < NUM_SPECIES && !GetBoxMonDataAt(i / IN_BOX_COUNT, i % IN_BOX_COUNT, MON_DATA_IS_EGG))
            sGod->seen[species] = TRUE;
    }
    for (i = 1; i <= NATIONAL_DEX_COUNT; i++)
        if (!sGod->seen[NationalPokedexNumToSpecies(i)])
            needed++;
    if (needed > freeSlots)
    {
        GodNotice(sText_NoRoom);
        return;
    }
    sGod->nextDex = 1;
    sGod->pcCursor = 0;
    sGod->added = 0;
    GodNotice(sText_Filling);
    sGod->state = GOD_FILL;
}

static void GodFillBatch(void)
{
    u8 batch = 0;
    u16 species;
    u8 fateful = TRUE;
    struct BoxPokemon *mon;
    while (sGod->nextDex <= NATIONAL_DEX_COUNT && batch < 8)
    {
        species = NationalPokedexNumToSpecies(sGod->nextDex++);
        if (sGod->seen[species])
            continue;
        while (sGod->pcCursor < TOTAL_BOXES_COUNT * IN_BOX_COUNT && GetBoxMonDataAt(sGod->pcCursor / IN_BOX_COUNT, sGod->pcCursor % IN_BOX_COUNT, MON_DATA_SPECIES) != SPECIES_NONE)
            sGod->pcCursor++;
        if (sGod->pcCursor >= TOTAL_BOXES_COUNT * IN_BOX_COUNT) { GodNotice(sText_NoRoom); return; }
        mon = GetBoxedMonPtr(sGod->pcCursor / IN_BOX_COUNT, sGod->pcCursor % IN_BOX_COUNT);
        CreateBoxMon(mon, species, sGod->level, USE_RANDOM_IVS, FALSE, 0, OT_ID_PLAYER_ID, 0);
        if (species == SPECIES_MEW || species == SPECIES_DEOXYS)
            SetBoxMonData(mon, MON_DATA_MODERN_FATEFUL_ENCOUNTER, &fateful);
        GetSetPokedexFlag(SpeciesToNationalPokedexNum(species), FLAG_SET_SEEN);
        GetSetPokedexFlag(SpeciesToNationalPokedexNum(species), FLAG_SET_CAUGHT);
        sGod->seen[species] = TRUE;
        sGod->pcCursor++;
        sGod->added++;
        batch++;
    }
    if (sGod->nextDex > NATIONAL_DEX_COUNT)
        GodNotice(sText_Done);
}

static void GodApply(void)
{
    u8 i;
    u16 species, hp;
    u32 exp, status = 0;
    bool8 on;
    if (!sGod->enabled)
    {
        GodNotice(sText_Locked);
        return;
    }
    switch (sGod->action)
    {
    case GOD_PC:
        GodBeginPc();
        return;
    case GOD_PARTY_LEVEL:
        for (i = 0; i < gPlayerPartyCount; i++)
        {
            if (GetMonData(&gPlayerParty[i], MON_DATA_IS_EGG))
                continue;
            species = GetMonData(&gPlayerParty[i], MON_DATA_SPECIES);
            exp = gExperienceTables[gSpeciesInfo[species].growthRate][sGod->level];
            SetMonData(&gPlayerParty[i], MON_DATA_EXP, &exp);
            CalculateMonStats(&gPlayerParty[i]);
            hp = GetMonData(&gPlayerParty[i], MON_DATA_MAX_HP);
            SetMonData(&gPlayerParty[i], MON_DATA_HP, &hp);
            SetMonData(&gPlayerParty[i], MON_DATA_STATUS, &status);
            MonRestorePP(&gPlayerParty[i]);
        }
        break;
    case GOD_BADGES:
        on = !GodAllBadges();
        for (i = 0; i < NUM_BADGES; i++)
            if (on) FlagSet(FLAG_BADGE01_GET + i); else FlagClear(FLAG_BADGE01_GET + i);
        break;
    case GOD_CHAMPION:
        on = !FlagGet(FLAG_SYS_GAME_CLEAR);
        if (on) FlagSet(FLAG_SYS_GAME_CLEAR); else FlagClear(FLAG_SYS_GAME_CLEAR);
        for (i = 0; i < 4; i++)
            if (on) FlagSet(TRAINER_FLAGS_START + TRAINER_ELITE_FOUR_LORELEI + i); else FlagClear(TRAINER_FLAGS_START + TRAINER_ELITE_FOUR_LORELEI + i);
        for (i = 0; i < 3; i++)
            if (on) FlagSet(TRAINER_FLAGS_START + TRAINER_CHAMPION_FIRST_SQUIRTLE + i); else FlagClear(TRAINER_FLAGS_START + TRAINER_CHAMPION_FIRST_SQUIRTLE + i);
        break;
    case GOD_DEX:
        if (IsNationalPokedexEnabled()) DisableNationalPokedex();
        else { FlagSet(FLAG_SYS_POKEDEX_GET); EnableNationalPokedex(); }
        break;
    }
    GodNotice(sText_Done);
}

static void GodExit(void)
{
    GodSaveSettings();
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    sGod->state = GOD_EXIT;
}

static void Task_GodMenu(u8 taskId)
{
    s16 change = 0;
    u8 count;
    u16 move;
    if (gPaletteFade.active)
        return;
    if (sGod->state == GOD_EXIT)
    {
        FreeAllWindowBuffers();
        UnsetBgTilemapBuffer(0);
        FREE_AND_SET_NULL(sGod);
        DestroyTask(taskId);
        SetMainCallback2(CB2_ReturnToFieldWithOpenMenu);
        return;
    }
    if (sGod->state == GOD_FILL) { GodFillBatch(); return; }
    if (sGod->state == GOD_NOTICE)
    {
        if (JOY_NEW(A_BUTTON | B_BUTTON)) { sGod->state = GOD_MAIN; sGod->cursor = 0; GodDraw(); }
        return;
    }
    if (sGod->state == GOD_CONFIRM)
    {
        if (JOY_NEW(B_BUTTON)) { sGod->state = GOD_MAIN; GodDraw(); }
        else if (JOY_NEW(A_BUTTON)) GodApply();
        return;
    }
    if (sGod->state == GOD_MOVES)
    {
        if (JOY_REPT(DPAD_UP)) change = -1;
        if (JOY_REPT(DPAD_DOWN)) change = 1;
        if (JOY_NEW(L_BUTTON)) change = -GOD_VISIBLE_MOVES;
        if (JOY_NEW(R_BUTTON)) change = GOD_VISIBLE_MOVES;
        if (change && sGod->moveCount)
        {
            sGod->moveCursor = ((sGod->moveCursor + change) % sGod->moveCount + sGod->moveCount) % sGod->moveCount;
            GodDraw();
        }
        if (JOY_NEW(SELECT_BUTTON)) { sGod->allMoves ^= 1; GodBuildMoves(); GodDraw(); }
        if (JOY_NEW(B_BUTTON)) { sGod->state = GOD_SLOT; sGod->cursor = sGod->slot; GodDraw(); }
        else if (JOY_NEW(A_BUTTON) && sGod->enabled && sGod->moveCount)
        {
            move = GodMoveAt(sGod->moveCursor);
            SetMonMoveSlot(&gPlayerParty[sGod->party], move, sGod->slot);
            // PP Up bonuses belong to the replaced move, not its replacement.
            count = GetMonData(&gPlayerParty[sGod->party], MON_DATA_PP_BONUSES);
            count &= ~(3 << (sGod->slot * 2));
            SetMonData(&gPlayerParty[sGod->party], MON_DATA_PP_BONUSES, &count);
            GodNotice(sText_Taught);
        }
        return;
    }
    count = sGod->state == GOD_MAIN ? GOD_ROWS : sGod->state == GOD_PARTY ? gPlayerPartyCount : MAX_MON_MOVES;
    if (JOY_REPT(DPAD_UP)) change = -1;
    if (JOY_REPT(DPAD_DOWN)) change = 1;
    if (change)
    {
        sGod->cursor = (sGod->cursor + count + change) % count;
        PlaySE(SE_SELECT);
        GodDraw();
    }
    if (JOY_NEW(B_BUTTON))
    {
        if (sGod->state == GOD_MAIN) GodExit();
        else { sGod->state = GOD_MAIN; sGod->cursor = GOD_RELEARN; GodDraw(); }
        return;
    }
    if (sGod->state == GOD_MAIN)
    {
        if (sGod->cursor == GOD_LEVEL)
        {
            change = 0;
            if (JOY_REPT(DPAD_LEFT)) change = -1;
            if (JOY_REPT(DPAD_RIGHT)) change = 1;
            if (JOY_NEW(L_BUTTON)) change = -10;
            if (JOY_NEW(R_BUTTON)) change = 10;
            if (change)
            {
                s16 level = sGod->level + change;
                sGod->level = level < 1 ? 1 : level > MAX_LEVEL ? MAX_LEVEL : level;
                GodSaveSettings();
                GodDraw();
            }
        }
        if ((JOY_NEW(A_BUTTON) || (JOY_NEW(DPAD_LEFT | DPAD_RIGHT) && sGod->cursor == GOD_ENABLE)))
        {
            if (sGod->cursor == GOD_ENABLE) { sGod->enabled ^= 1; GodSaveSettings(); GodDraw(); }
            else if (sGod->cursor == GOD_BACK) GodExit();
            else if (sGod->cursor == GOD_LEVEL) return;
            else if (!sGod->enabled) GodNotice(sText_Locked);
            else if ((sGod->cursor == GOD_RELEARN || sGod->cursor == GOD_PARTY_LEVEL) && !gPlayerPartyCount) GodNotice(sText_EmptyParty);
            else if (sGod->cursor == GOD_HMS) { sGod->hmUnlock ^= 1; GodSaveSettings(); GodDraw(); }
            else if (sGod->cursor == GOD_RELEARN) { sGod->state = GOD_PARTY; sGod->cursor = 0; GodDraw(); }
            else { sGod->action = sGod->cursor; sGod->state = GOD_CONFIRM; GodDraw(); }
        }
    }
    else if (JOY_NEW(A_BUTTON))
    {
        if (sGod->state == GOD_PARTY)
        {
            if (GetMonData(&gPlayerParty[sGod->cursor], MON_DATA_IS_EGG)) return;
            sGod->party = sGod->cursor;
            sGod->cursor = 0;
            sGod->state = GOD_SLOT;
        }
        else
        {
            sGod->slot = sGod->cursor;
            sGod->allMoves = FALSE;
            GodBuildMoves();
            sGod->state = GOD_MOVES;
        }
        GodDraw();
    }
}

static void GodMain(void)
{
    RunTasks();
    UpdatePaletteFade();
}

static void GodVBlank(void)
{
    TransferPlttBuffer();
}

// Read existing save padding only. No badges, items or learned moves are granted.
bool8 GodModeHmUnlocked(void)
{
    u32 magic;
    CpuCopy32(gSaveBlock2Ptr->filler_90, &magic, sizeof(magic));
    return magic == GOD_MAGIC && gSaveBlock2Ptr->filler_90[4] == 1
        && gSaveBlock2Ptr->filler_90[6] == 0xA5;
}

bool8 GodModeIsHm(u16 move)
{
    return move == MOVE_CUT || move == MOVE_FLY || move == MOVE_SURF
        || move == MOVE_STRENGTH || move == MOVE_FLASH
        || move == MOVE_ROCK_SMASH || move == MOVE_WATERFALL;
}

void GodModeCheckHmAccess(void)
{
    gSpecialVar_Result = GodModeHmUnlocked();
}

bool8 IsGodModeMenuActive(void)
{
    return sGod != NULL;
}

void CB2_GodModeMenu(void)
{
    u32 magic;
    SetVBlankCallback(NULL);
    SetHBlankCallback(NULL);
    ScanlineEffect_Stop();
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    ResetPaletteFade();
    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    ResetBgsAndClearDma3BusyFlags(FALSE);
    InitBgsFromTemplates(0, sBg, ARRAY_COUNT(sBg));
    sGod = AllocZeroed(sizeof(*sGod));
    if (sGod == NULL)
    {
        SetMainCallback2(CB2_ReturnToFieldWithOpenMenu);
        return;
    }
    CpuCopy32(gSaveBlock2Ptr->filler_90, &magic, sizeof(magic));
    sGod->enabled = magic == GOD_MAGIC && gSaveBlock2Ptr->filler_90[4] == 1;
    sGod->hmUnlock = magic == GOD_MAGIC && gSaveBlock2Ptr->filler_90[6] == 0xA5;
    sGod->level = magic == GOD_MAGIC ? gSaveBlock2Ptr->filler_90[5] : 5;
    if (sGod->level < 1 || sGod->level > MAX_LEVEL) sGod->level = 5;
    SetBgTilemapBuffer(0, sGod->tilemap);
    InitWindows(sWindows);
    DeactivateAllTextPrinters();
    LoadPalette(sPal, 0, sizeof(sPal));
    ChangeBgX(0, 0, 0);
    ChangeBgY(0, 0, 0);
    SetGpuReg(REG_OFFSET_BLDCNT, 0);
    SetGpuReg(REG_OFFSET_WININ, 0);
    SetGpuReg(REG_OFFSET_WINOUT, 0);
    GodDraw();
    ShowBg(0);
    CreateTask(Task_GodMenu, 0);
    BlendPalettes(PALETTES_ALL, 16, RGB_BLACK);
    BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
    SetVBlankCallback(GodVBlank);
    SetMainCallback2(GodMain);
}

#endif // PLAYTEST_TOOLS
