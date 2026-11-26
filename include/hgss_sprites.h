#ifndef GUARD_HGSS_SPRITES_H
#define GUARD_HGSS_SPRITES_H
#include "sprite.h"

// Pokemon field/battle sprites (PMD Red Rescue Team art; name kept for history).
enum {
    PMD_ANIM_WALK, PMD_ANIM_ATTACK, PMD_ANIM_STRIKE, PMD_ANIM_SHOOT, PMD_ANIM_DANCE, PMD_ANIM_SLEEP, PMD_ANIM_HURT,
    PMD_ANIM_IDLE, PMD_ANIM_SWING, PMD_ANIM_DOUBLE, PMD_ANIM_HOP, PMD_ANIM_CHARGE, PMD_ANIM_ROTATE, PMD_ANIM_COUNT
};
enum {
    PMD_DIR_DOWN, PMD_DIR_DOWN_RIGHT, PMD_DIR_RIGHT, PMD_DIR_UP_RIGHT, PMD_DIR_UP, PMD_DIR_UP_LEFT, PMD_DIR_LEFT, PMD_DIR_DOWN_LEFT
};
#define PMD_FRAME_HIT    2
#define PMD_FRAME_RETURN 1

struct PmdFrame { u16 pose; u8 duration, flags; s8 dx, dy; u16 length; };

u16 DsMonSpriteKey(u16 species, u32 personality, bool8 female);
const u16 *DsMonPalette(u16 key, bool8 shiny);
const u8 *DsMonFrame(u16 key, u8 direction, u8 phase);
u8 DsMonSize(u16 key);
u16 DsMonSpriteCount(void);
s8 DsMonAnchorY(u16 key);
u8 DsCreateMonSprite(u16 key, s16 x, s16 y, u8 palette, u8 priority, SpriteCallback callback);
void DsUpdateMonSprite(u8 id, u16 key, u8 direction, u8 phase);
void DsRecolorMonSprite(u8 id, u8 color);
bool8 PmdGetFrame(u16 key, u8 anim, u8 dir, u16 tick, bool8 loop, struct PmdFrame *out);
u16 PmdAnimHitTime(u16 key, u8 anim);
u8 PmdMoveBodyAnim(u16 move, u16 species);
u8 PmdDirFromVector(s32 dx, s32 dy, u8 fallback4);
void PmdShowMonFrame(u8 id, u16 key, u8 anim, u8 dir, u16 tick, bool8 loop, s16 *dx, s16 *dy);
void UpdateMonFollower(void);
void HideMonFollower(void);

#endif
