// Fly journey: after the native Fly take-off, the player glides above a picture of Kanto and the
// Sevii Islands (built by tools/flyatlas from the real maps) through drifting clouds, then the
// native landing plays at the destination. The picture is streamed into a 32x32-tile ring, so its
// size is limited only by ROM space. Nothing here changes maps, saves or Fly rules.
#include "global.h"
#include "gflib.h"
#include "main.h"
#include "overworld.h"
#include "palette.h"
#include "field_effect.h"
#include "fly_journey.h"
#include "constants/maps.h"
#include "constants/field_effects.h"
#include "data/fly_atlas.h"

extern const struct SpriteTemplate *const gFieldEffectObjectTemplatePointers[];
extern const u16 gObjectEventPal_Player[];

#define ATLAS_CHAR      0       // 1024 tile slots at 0x06000000
#define CLOUD_CHAR      2       // cloud tiles at 0x06008000
#define ATLAS_SCREEN    28
#define CLOUD_SCREEN_A  29
#define CLOUD_SCREEN_B  30
#define ATLAS_PIXEL_W   (FLY_ATLAS_TW * 8)
#define ATLAS_PIXEL_H   (FLY_ATLAS_TH * 8)

enum { FJ_FADE_IN, FJ_GLIDE, FJ_FADE_OUT, FJ_DONE };

struct FlyJourney
{
    bool8 haveOrigin, active;
    u8 state, birdId, shadowId;
    s16 sx, sy, tx, ty;         // start / target centre in atlas pixels
    s16 cx, cy;                 // current top-left of the view
    s16 col0, row0;             // first loaded tile column / row (32 of each are resident)
    u16 time, duration, cloudT;
};
static EWRAM_DATA struct FlyJourney sFJ = {0};

static bool8 AtlasPosOf(u8 group, u8 num, s16 x, s16 y, s16 *ox, s16 *oy)
{
    u16 key = (group << 8) | num, i;
    for (i = 0; i < ARRAY_COUNT(sFlyAtlasMaps); i++)
    {
        if (sFlyAtlasMaps[i].mapGroupNum == key)
        {
            *ox = sFlyAtlasMaps[i].x + x * FLY_ATLAS_MT + FLY_ATLAS_MT / 2;
            *oy = sFlyAtlasMaps[i].y + y * FLY_ATLAS_MT + FLY_ATLAS_MT / 2;
            return TRUE;
        }
    }
    return FALSE;
}

// Called by the Fly task just before it warps (location is still the take-off map).
void FlyJourney_RecordOrigin(void)
{
    struct WarpData *loc = &gSaveBlock1Ptr->location;
    sFJ.haveOrigin = AtlasPosOf(loc->mapGroup, loc->mapNum, gSaveBlock1Ptr->pos.x, gSaveBlock1Ptr->pos.y, &sFJ.sx, &sFJ.sy);
}

static void LoadAtlasTile(s16 tx, s16 ty)
{
    u16 slot = (tx & 31) + (ty & 31) * 32, n, entry;
    u8 pal;
    if (tx < 0) tx = 0;
    if (ty < 0) ty = 0;
    if (tx >= FLY_ATLAS_TW) tx = FLY_ATLAS_TW - 1;
    if (ty >= FLY_ATLAS_TH) ty = FLY_ATLAS_TH - 1;
    n = ty * FLY_ATLAS_TW + tx;
    pal = (sFlyAtlasPals[n >> 1] >> ((n & 1) * 4)) & 15;
    entry = sFlyAtlasMap[n];
    CpuCopy32(&sFlyAtlasTiles[entry * 8], ((u8 *)BG_CHAR_ADDR(ATLAS_CHAR) + slot * 32), 32);
    ((u16 *)BG_SCREEN_ADDR(ATLAS_SCREEN))[slot] = slot | (pal << 12);
}

static s16 AbsDiff(s16 a, s16 b) { return a > b ? a - b : b - a; }

static void LoadAtlasColumn(s16 col) { s16 r; for (r = sFJ.row0; r < sFJ.row0 + 32; r++) LoadAtlasTile(col, r); }
static void LoadAtlasRow(s16 row)    { s16 c; for (c = sFJ.col0; c < sFJ.col0 + 32; c++) LoadAtlasTile(c, row); }

static void StreamAtlas(void)
{
    s16 wantCol = (sFJ.cx >> 3), wantRow = (sFJ.cy >> 3) - 5;
    if (AbsDiff(wantCol, sFJ.col0) >= 8 || AbsDiff(wantRow, sFJ.row0) >= 8)
    {
        s16 c;
        sFJ.col0 = wantCol; sFJ.row0 = wantRow;
        for (c = sFJ.col0; c < sFJ.col0 + 32; c++) LoadAtlasColumn(c);
        return;
    }
    while (sFJ.col0 < wantCol) { sFJ.col0++; LoadAtlasColumn(sFJ.col0 + 31); }
    while (sFJ.col0 > wantCol) { sFJ.col0--; LoadAtlasColumn(sFJ.col0); }
    while (sFJ.row0 < wantRow) { sFJ.row0++; LoadAtlasRow(sFJ.row0 + 31); }
    while (sFJ.row0 > wantRow) { sFJ.row0--; LoadAtlasRow(sFJ.row0); }
}

static void VBlankCB_FlyJourney(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
    SetGpuReg(REG_OFFSET_BG2HOFS, sFJ.cx & 0x1FF);
    SetGpuReg(REG_OFFSET_BG2VOFS, sFJ.cy & 0x1FF);
    SetGpuReg(REG_OFFSET_BG1HOFS, (sFJ.cx * 3 / 2 + sFJ.cloudT / 2) & 0x1FF);
    SetGpuReg(REG_OFFSET_BG1VOFS, (sFJ.cy * 3 / 2) & 0x1FF);
    SetGpuReg(REG_OFFSET_BG0HOFS, (sFJ.cx * 2 + 96 + sFJ.cloudT) & 0x1FF);
    SetGpuReg(REG_OFFSET_BG0VOFS, (sFJ.cy * 2 + 160) & 0x1FF);
}

static void CB2_FlyJourney(void);
static void CB2_FlyJourneyInit(void)
{
    struct Sprite *bird;
    SetVBlankCallback(NULL);
    SetHBlankCallback(NULL);
    SetMainCallback1(NULL);
    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    DmaFill16(3, 0, (void *)VRAM, VRAM_SIZE);
    DmaFill32(3, 0, (void *)OAM, OAM_SIZE);
    ResetSpriteData();
    FreeAllSpritePalettes();
    ResetPaletteFade();
    LoadPalette(sFlyAtlasPalettes, BG_PLTT_ID(0), 15 * PLTT_SIZE_4BPP);
    LoadPalette(sFlyCloudPal, BG_PLTT_ID(15), PLTT_SIZE_4BPP);
    LoadPalette(gObjectEventPal_Player, OBJ_PLTT_ID(0), PLTT_SIZE_4BPP);
    CpuCopy32(sFlyCloudTiles, (void *)BG_CHAR_ADDR(CLOUD_CHAR), sizeof(sFlyCloudTiles));
    CpuCopy16(sFlyCloudMap, (void *)BG_SCREEN_ADDR(CLOUD_SCREEN_A), 0x800);
    CpuCopy16(sFlyCloudMap, (void *)BG_SCREEN_ADDR(CLOUD_SCREEN_B), 0x800);

    sFJ.time = 0; sFJ.cloudT = 0; sFJ.state = FJ_FADE_IN;
    {
        s32 dx = sFJ.tx - sFJ.sx, dy = sFJ.ty - sFJ.sy, d = Sqrt(dx * dx + dy * dy);
        sFJ.duration = d / 2;                       // ~2 pixels per frame
        if (sFJ.duration < 120) sFJ.duration = 120;
        if (sFJ.duration > 420) sFJ.duration = 420;
    }
    sFJ.cx = sFJ.sx - DISPLAY_WIDTH / 2; sFJ.cy = sFJ.sy - DISPLAY_HEIGHT / 2;
    sFJ.col0 = -1000; sFJ.row0 = -1000;
    StreamAtlas();

    sFJ.birdId = CreateSprite(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_BIRD], DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2 - 8, 0);
    if (sFJ.birdId != MAX_SPRITES)
    {
        bird = &gSprites[sFJ.birdId];
        bird->oam.paletteNum = 0;
        bird->oam.priority = 0;
        StartSpriteAnim(bird, gSaveBlock2Ptr->playerGender == MALE ? 2 : 4);
    }

    SetGpuReg(REG_OFFSET_BG2CNT, BGCNT_PRIORITY(3) | BGCNT_CHARBASE(ATLAS_CHAR) | BGCNT_SCREENBASE(ATLAS_SCREEN) | BGCNT_16COLOR | BGCNT_TXT256x256);
    SetGpuReg(REG_OFFSET_BG1CNT, BGCNT_PRIORITY(2) | BGCNT_CHARBASE(CLOUD_CHAR) | BGCNT_SCREENBASE(CLOUD_SCREEN_A) | BGCNT_16COLOR | BGCNT_TXT256x256);
    SetGpuReg(REG_OFFSET_BG0CNT, BGCNT_PRIORITY(1) | BGCNT_CHARBASE(CLOUD_CHAR) | BGCNT_SCREENBASE(CLOUD_SCREEN_B) | BGCNT_16COLOR | BGCNT_TXT256x256);
    SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_BG0 | BLDCNT_TGT1_BG1 | BLDCNT_EFFECT_BLEND | BLDCNT_TGT2_BG1 | BLDCNT_TGT2_BG2 | BLDCNT_TGT2_OBJ);
    SetGpuReg(REG_OFFSET_BLDALPHA, BLDALPHA_BLEND(9, 9));
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0 | DISPCNT_OBJ_1D_MAP | DISPCNT_BG0_ON | DISPCNT_BG1_ON | DISPCNT_BG2_ON | DISPCNT_OBJ_ON);
    BeginNormalPaletteFade(PALETTES_ALL, 1, 16, 0, RGB_WHITE);
    SetVBlankCallback(VBlankCB_FlyJourney);
    SetMainCallback2(CB2_FlyJourney);
}

static void CB2_FlyJourney(void)
{
    s32 t, e;
    sFJ.cloudT++;
    switch (sFJ.state)
    {
    case FJ_FADE_IN:
        if (!gPaletteFade.active) sFJ.state = FJ_GLIDE;
        break;
    case FJ_GLIDE:
        sFJ.time++;
        // ease in/out along the straight line from take-off to destination
        t = (sFJ.time << 12) / sFJ.duration;
        e = (t * t * (3 * 4096 - 2 * t)) >> 24;           // smoothstep, Q12
        sFJ.cx = sFJ.sx + (((s32)(sFJ.tx - sFJ.sx) * e) >> 12) - DISPLAY_WIDTH / 2;
        sFJ.cy = sFJ.sy + (((s32)(sFJ.ty - sFJ.sy) * e) >> 12) - DISPLAY_HEIGHT / 2;
        if (sFJ.birdId != MAX_SPRITES)
        {
            gSprites[sFJ.birdId].hFlip = sFJ.tx < sFJ.sx;
            gSprites[sFJ.birdId].y2 = ((sFJ.cloudT >> 3) & 3) == 0 ? -1 : 0;   // gentle wing bob
        }
        if (sFJ.time >= sFJ.duration)
        {
            BeginNormalPaletteFade(PALETTES_ALL, 1, 0, 16, RGB_WHITE);
            sFJ.state = FJ_FADE_OUT;
        }
        break;
    case FJ_FADE_OUT:
        if (!gPaletteFade.active)
        {
            sFJ.state = FJ_DONE;
            SetVBlankCallback(NULL);
            SetGpuReg(REG_OFFSET_BLDCNT, 0);
            SetGpuReg(REG_OFFSET_BLDALPHA, 0);
            ResetSpriteData();
            FreeAllSpritePalettes();
            SetMainCallback2(CB2_LoadMap);
            return;
        }
        break;
    }
    StreamAtlas();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

// Called after the warp has been set up. Plays the journey if both ends are on the picture.
void FlyJourney_StartOrLoadMap(void)
{
    struct WarpData *loc = &gSaveBlock1Ptr->location;
    if (sFJ.haveOrigin && AtlasPosOf(loc->mapGroup, loc->mapNum, gSaveBlock1Ptr->pos.x, gSaveBlock1Ptr->pos.y, &sFJ.tx, &sFJ.ty))
    {
        sFJ.haveOrigin = FALSE;
        SetMainCallback2(CB2_FlyJourneyInit);
    }
    else
    {
        sFJ.haveOrigin = FALSE;
        SetMainCallback2(CB2_LoadMap);
    }
}
