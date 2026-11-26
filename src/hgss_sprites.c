// Pokemon field/battle sprites from PMD Red Rescue Team (private use; see tools/import_pmd_mons.py).
// The function names keep the old Ds* API so the follower and real-time battle code stay unchanged.
// Each pose is assembled from PMD's own sprite pieces: the pieces are unpacked from small LZ77 blocks,
// copied (flipped where needed) into the sprite's tiles, and drawn with FireRed's subsprite tables.
#include "global.h"
#include "gflib.h"
#include "battle_anim.h"
#include "hgss_sprites.h"
#include "pokemon_icon.h"
#include "constants/species.h"
#include "constants/moves.h"
#include "decompress.h"
#include "data/pmd_mon_graphics.h"

#define PMD_SLOTS 6
#define PMD_MAX_PARTS 16
#define PMD_TILE_BYTES 2048
#define B16(p) ((u16)((p)[0] | ((p)[1] << 8)))
#define B32(p) ((u32)((p)[0] | ((p)[1] << 8) | ((p)[2] << 16) | ((u32)(p)[3] << 24)))

struct PmdSlot
{
    u8 spriteId;
    u16 key;
    u16 pose;
    u8 tiles;
    struct SubspriteTable table;
    struct Subsprite subs[PMD_MAX_PARTS];
};

static EWRAM_DATA struct PmdSlot sPmdSlots[PMD_SLOTS] = {0};
// Scratch space borrowed from the shared decompression buffer only while a pose is drawn.
#define sPmdBlock   (&gDecompressionBuffer[0])
#define sPmdGather  (&gDecompressionBuffer[0x1000])
#define sPmdTiles   (&gDecompressionBuffer[0x1800])
static const u8 *sPmdBlockSrc;

static const u8 sTileW[3][4] = {{1, 2, 4, 8}, {2, 4, 4, 8}, {1, 1, 2, 4}};
static const u8 sTileH[3][4] = {{1, 2, 4, 8}, {1, 1, 2, 4}, {2, 4, 4, 8}};
static const u8 sDir4To8[4] = {PMD_DIR_DOWN, PMD_DIR_UP, PMD_DIR_LEFT, PMD_DIR_RIGHT};

static const struct SpriteFrameImage sDsImage32[] = {{NULL, 512}};
static const struct SpriteFrameImage sDsImage64[] = {{NULL, 2048}};
static const struct SpriteTemplate sDsTemplate32 = {
    .tileTag = TAG_NONE, .paletteTag = TAG_NONE, .oam = &gOamData_AffineOff_ObjNormal_32x32,
    .anims = gDummySpriteAnimTable, .images = sDsImage32, .affineAnims = gDummySpriteAffineAnimTable, .callback = SpriteCallbackDummy
};
static const struct SpriteTemplate sDsTemplate64 = {
    .tileTag = TAG_NONE, .paletteTag = TAG_NONE, .oam = &gOamData_AffineOff_ObjNormal_64x64,
    .anims = gDummySpriteAnimTable, .images = sDsImage64, .affineAnims = gDummySpriteAffineAnimTable, .callback = SpriteCallbackDummy
};

static const u8 *PmdBank(u16 key)
{
    if (!key || key >= ARRAY_COUNT(sDsGfx))
        return NULL;
    return (const u8 *)sDsGfx[key].bank;
}

u16 DsMonSpriteKey(u16 species, u32 personality, bool8 female)
{
    if (species == SPECIES_UNOWN) return sDsUnownKeys[GetUnownLetterByPersonality(personality)];
    if (species >= SPECIES_UNOWN_B && species <= SPECIES_UNOWN_QMARK) return sDsUnownKeys[species - SPECIES_UNOWN_B + 1];
    if (species >= NUM_SPECIES) return 0;
    return sDsSpeciesKeys[species];
}
const u16 *DsMonPalette(u16 key, bool8 shiny)
{
    if (!key || key >= ARRAY_COUNT(sDsGfx)) return NULL;
    return shiny ? sDsGfx[key].shiny : sDsGfx[key].normal;
}
u8 DsMonSize(u16 key) { return key < ARRAY_COUNT(sDsGfx) && key ? sDsGfx[key].size : 32; }
u16 DsMonSpriteCount(void) { return ARRAY_COUNT(sDsGfx) - 1; }
s8 DsMonAnchorY(u16 key) { return key < ARRAY_COUNT(sDsGfx) && key ? sDsGfx[key].anchorY : 16; }
// Whole-image access is not available for piece-based sprites; callers use the sprite itself.
const u8 *DsMonFrame(u16 key, u8 direction, u8 phase) { return NULL; }

static const u8 *PmdLoadBlock(const u8 *bank, u8 block)
{
    const u8 *src = bank + B32(bank + 12 + 13 * 4 + 4 * block);
    if (sPmdBlockSrc != src)
    {
        LZ77UnCompWram(src, sPmdBlock);
        sPmdBlockSrc = src;
    }
    return sPmdBlock;
}

static void PmdCopyTiles(u8 st, u8 dt, u8 tw, u8 th, u8 flip, u16 avail)
{
    u8 tx, ty, r, c;
    for (ty = 0; ty < th; ty++)
    {
        for (tx = 0; tx < tw; tx++)
        {
            u8 sx = (flip & 1) ? tw - 1 - tx : tx;
            u8 sy = (flip & 2) ? th - 1 - ty : ty;
            u16 so = (st + sy * tw + sx) * 32, d = (dt + ty * tw + tx) * 32;
            if (d + 32 > PMD_TILE_BYTES)
                return;
            if (so + 32 > avail)
            {
                memset(&sPmdTiles[d], 0, 32);
                continue;
            }
            if (!flip)
            {
                memcpy(&sPmdTiles[d], &sPmdGather[so], 32);
                continue;
            }
            for (r = 0; r < 8; r++)
            {
                u8 sr = (flip & 2) ? 7 - r : r;
                for (c = 0; c < 4; c++)
                {
                    u8 b;
                    if (flip & 1)
                    {
                        b = sPmdGather[so + sr * 4 + 3 - c];
                        b = (b >> 4) | (b << 4);
                    }
                    else
                    {
                        b = sPmdGather[so + sr * 4 + c];
                    }
                    sPmdTiles[d + r * 4 + c] = b;
                }
            }
        }
    }
}

static struct PmdSlot *PmdSlotFor(u8 spriteId, bool8 create)
{
    u8 i, free = PMD_SLOTS;
    for (i = 0; i < PMD_SLOTS; i++)
    {
        struct PmdSlot *s = &sPmdSlots[i];
        bool8 live = s->spriteId < MAX_SPRITES && gSprites[s->spriteId].inUse && gSprites[s->spriteId].subspriteTables == &s->table;
        if (live && s->spriteId == spriteId)
            return s;
        if (!live && free == PMD_SLOTS)
            free = i;
    }
    if (!create || free == PMD_SLOTS)
        return NULL;
    sPmdSlots[free].spriteId = spriteId;
    sPmdSlots[free].key = 0;
    sPmdSlots[free].pose = 0xFFFF;
    return &sPmdSlots[free];
}

static void PmdDrawPose(struct PmdSlot *s, u16 key, u16 pose)
{
    const u8 *bank = PmdBank(key), *stab, *poff, *pr;
    struct Sprite *sprite = &gSprites[s->spriteId];
    u16 gathered = 0, npz;
    u8 nb, nsrc, nparts, ntiles, i, used = 0;

    if (!bank)
        return;
    sPmdBlockSrc = NULL;  // the shared buffer may have been used by other code since the last pose
    nb = bank[3];
    npz = B16(bank + 4);
    stab = bank + 12 + 13 * 4 + 4 * nb;
    poff = stab + 6 * B16(bank + 6);
    if (pose >= npz)
        pose = 0;
    pr = bank + B16(bank + 10) + B16(poff + 2 * pose);
    nsrc = pr[0];
    nparts = pr[1];
    ntiles = pr[2];
    for (i = 0; i < nsrc; i++)
    {
        const u8 *e = stab + 6 * B16(pr + 4 + 2 * i);
        const u8 *blk = PmdLoadBlock(bank, e[0]);
        u16 len = B16(e + 4);
        if (gathered + len > PMD_TILE_BYTES)
            len = PMD_TILE_BYTES - gathered;
        memcpy(&sPmdGather[gathered], blk + B16(e + 2), len);
        gathered += len;
    }
    pr += 4 + 2 * nsrc;
    for (i = 0; i < nparts; i++, pr += 6)
    {
        u8 shape = pr[2] >> 2, size = pr[2] & 3;
        if (shape > 2 || used >= PMD_MAX_PARTS)
            continue;
        PmdCopyTiles(pr[3], pr[4], sTileW[shape][size], sTileH[shape][size], pr[5], gathered);
        s->subs[used].x = (s8)pr[0];
        s->subs[used].y = (s8)pr[1];
        s->subs[used].shape = shape;
        s->subs[used].size = size;
        s->subs[used].tileOffset = pr[4];
        s->subs[used].priority = sprite->oam.priority;
        used++;
    }
    s->table.subspriteCount = used;
    s->table.subsprites = s->subs;
    if (ntiles * 32 > (DsMonSize(key) == 64 ? 2048 : 512))
        ntiles = (DsMonSize(key) == 64 ? 64 : 16);
    if (ntiles)
        CpuCopy16(sPmdTiles, (void *)(OBJ_VRAM0 + sprite->oam.tileNum * TILE_SIZE_4BPP), ntiles * 32);
    s->tiles = ntiles;
    s->key = key;
    s->pose = pose;
    sprite->subspriteTables = &s->table;
    sprite->subspriteTableNum = 0;
    sprite->subspriteMode = SUBSPRITES_IGNORE_PRIORITY;
    sprite->hFlip = FALSE;
    sprite->vFlip = FALSE;
}

static const u8 *PmdAnim(const u8 *bank, u8 anim)
{
    u32 o;
    if (anim >= 13)
        return NULL;
    o = B32(bank + 12 + 4 * anim);
    return o ? bank + o : NULL;
}

// Finds the frame of an animation at a given time (60 fps ticks).
bool8 PmdGetFrame(u16 key, u8 anim, u8 dir, u16 tick, bool8 loop, struct PmdFrame *out)
{
    const u8 *bank = PmdBank(key), *a, *p;
    u8 n, hasOff, mask, d, i, pw;
    u16 total = 0, t = 0;

    if (!bank)
        return FALSE;
    a = PmdAnim(bank, anim);
    if (!a) a = PmdAnim(bank, PMD_ANIM_IDLE);
    if (!a) a = PmdAnim(bank, PMD_ANIM_WALK);
    if (!a) return FALSE;
    n = a[0];
    hasOff = a[1] & 1;
    mask = a[2];
    pw = bank[8];
    if (!n || !mask)
        return FALSE;
    for (i = 0; i < n; i++)
        total += a[4 + 2 * i];
    dir &= 7;
    if (!(mask & (1 << dir)))
        for (dir = 0; !(mask & (1 << dir)); dir++)
            ;
    if (loop)
        tick %= total;
    else if (tick >= total)
        tick = total - 1;
    for (i = 0; i < n - 1; i++)
    {
        if (tick < t + a[4 + 2 * i])
            break;
        t += a[4 + 2 * i];
    }
    p = a + 4 + 2 * n;
    for (d = 0; d < dir; d++)
        if (mask & (1 << d))
            p += n * pw + (hasOff ? 2 * n : 0);
    out->pose = pw == 2 ? B16(p + 2 * i) : p[i];
    out->duration = a[4 + 2 * i];
    out->flags = a[5 + 2 * i];
    out->dx = hasOff ? (s8)p[n * pw + 2 * i] : 0;
    out->dy = hasOff ? (s8)p[n * pw + 2 * i + 1] : 0;
    out->length = total;
    return TRUE;
}

// Ticks from the start of the animation to its damage frame (half its length if it has none).
u16 PmdAnimHitTime(u16 key, u8 anim)
{
    const u8 *bank = PmdBank(key), *a;
    u16 t = 0, total = 0;
    u8 i, n;
    if (!bank) return 0;
    a = PmdAnim(bank, anim);
    if (!a) return 0;
    n = a[0];
    for (i = 0; i < n; i++)
        total += a[4 + 2 * i];
    for (i = 0; i < n; i++)
    {
        if (a[5 + 2 * i] & 2)
            return t;
        t += a[4 + 2 * i];
    }
    return total / 2;
}

u8 PmdMoveBodyAnim(u16 move, u16 species)
{
    s16 lo = 0, hi = ARRAY_COUNT(gPmdMoveAnimOverrides) - 2, mid;
    if (move >= MOVES_COUNT)
        return PMD_ANIM_ATTACK;
    while (lo <= hi)
    {
        mid = (lo + hi) / 2;
        if (gPmdMoveAnimOverrides[mid][0] < move) lo = mid + 1;
        else hi = mid - 1;
    }
    for (; lo < (s16)ARRAY_COUNT(gPmdMoveAnimOverrides) - 1 && gPmdMoveAnimOverrides[lo][0] == move; lo++)
        if (gPmdMoveAnimOverrides[lo][1] == species)
            return gPmdMoveAnimOverrides[lo][2];
    return gPmdMoveBodyAnim[move];
}

u8 PmdDirFromVector(s32 dx, s32 dy, u8 fallback4)
{
    s32 ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    if (!dx && !dy)
        return sDir4To8[fallback4 & 3];
    if (ax > ay * 2)
        return dx < 0 ? PMD_DIR_LEFT : PMD_DIR_RIGHT;
    if (ay > ax * 2)
        return dy < 0 ? PMD_DIR_UP : PMD_DIR_DOWN;
    if (dy > 0)
        return dx > 0 ? PMD_DIR_DOWN_RIGHT : PMD_DIR_DOWN_LEFT;
    return dx > 0 ? PMD_DIR_UP_RIGHT : PMD_DIR_UP_LEFT;
}

void PmdShowMonFrame(u8 id, u16 key, u8 anim, u8 dir, u16 tick, bool8 loop, s16 *dx, s16 *dy)
{
    struct PmdFrame frame;
    struct PmdSlot *s;
    if (dx) *dx = 0;
    if (dy) *dy = 0;
    if (id >= MAX_SPRITES || !gSprites[id].inUse || !PmdGetFrame(key, anim, dir, tick, loop, &frame))
        return;
    s = PmdSlotFor(id, TRUE);
    if (!s)
        return;
    if (s->key != key || s->pose != frame.pose || gSprites[id].subspriteTables != &s->table)
        PmdDrawPose(s, key, frame.pose);
    if (dx) *dx = frame.dx;
    if (dy) *dy = frame.dy;
}

// Recolours the current pose into a one-colour silhouette (used for glows).
void DsRecolorMonSprite(u8 id, u8 color)
{
    struct PmdSlot *s = PmdSlotFor(id, FALSE);
    u16 i, n, *vram;
    if (!s)
        return;
    vram = (u16 *)(OBJ_VRAM0 + gSprites[id].oam.tileNum * TILE_SIZE_4BPP);
    n = s->tiles * 16;
    for (i = 0; i < n; i++)
    {
        u16 v = vram[i], o = 0;
        if (v & 0x000F) o |= color;
        if (v & 0x00F0) o |= color << 4;
        if (v & 0x0F00) o |= color << 8;
        if (v & 0xF000) o |= color << 12;
        vram[i] = o;
    }
}

u8 DsCreateMonSprite(u16 key, s16 x, s16 y, u8 palette, u8 priority, SpriteCallback callback)
{
    u8 id;
    if (!key || key >= ARRAY_COUNT(sDsGfx) || palette >= 16) return MAX_SPRITES;
    id = CreateSprite(DsMonSize(key) == 64 ? &sDsTemplate64 : &sDsTemplate32, x, y, priority);
    if (id == MAX_SPRITES) return id;
    gSprites[id].animPaused = TRUE; gSprites[id].animBeginning = FALSE;
    gSprites[id].affineAnimPaused = TRUE; gSprites[id].affineAnimBeginning = FALSE;
    gSprites[id].oam.paletteNum = palette; gSprites[id].callback = callback;
    gSprites[id].data[7] = -1;
    gSprites[id].subspriteTables = NULL;
    PmdShowMonFrame(id, key, PMD_ANIM_IDLE, PMD_DIR_DOWN, 0, TRUE, NULL, NULL);
    return id;
}

void DsUpdateMonSprite(u8 id, u16 key, u8 direction, u8 phase)
{
    PmdShowMonFrame(id, key, PMD_ANIM_WALK, sDir4To8[direction & 3], phase * 8, TRUE, NULL, NULL);
}
