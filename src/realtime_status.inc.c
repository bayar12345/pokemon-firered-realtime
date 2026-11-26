// Real-time status conditions. A three-second interval represents a residual
// damage turn; menu pauses do not advance any of these clocks.
#define RT_STATUS_TAG 0x7790
#define RT_STATUS_PAL_KEY 0x500
#define RT_STATUS_INTERVAL 180
#include "data/realtime_status_graphics.h"
static const u8 sText_Brn[] = _("BRN");
static const u8 sText_Psn[] = _("PSN");
static const u8 sText_Par[] = _("PAR");
static const u8 sText_Slp[] = _("SLP");
static const u8 sText_Frz[] = _("FRZ");
static const u8 sText_Cfn[] = _("CFN");
static const u8 sText_StatusApplied[] = _("{STR_VAR_1}: {STR_VAR_2}!");

static const u8 *RtStatusLabel(struct RtFighter *f)
{
    if (f->status & STATUS1_BURN) return sText_Brn;
    if (f->status & STATUS1_PSN_ANY) return sText_Psn;
    if (f->status & STATUS1_PARALYSIS) return sText_Par;
    if (f->status & STATUS1_SLEEP) return sText_Slp;
    if (f->status & STATUS1_FREEZE) return sText_Frz;
    return f->confusionTimer ? sText_Cfn : sText_Dash;
}

static void RtSetStatus(struct RtFighter *f, u32 status)
{
    if((f->status&STATUS1_SLEEP) && !(status&STATUS1_SLEEP)) f->clocks[RTC_SLEEP_GUARD]=180;
    f->status = status;
    f->statusTick = RT_STATUS_INTERVAL;
    f->statusTimer = RtSleepFrames(status,f->ability);
    f->toxicCount = 1;
    SetMonData(f->mon, MON_DATA_STATUS, &f->status);
    if (status & (STATUS1_SLEEP | STATUS1_FREEZE))
    {
        f->windup = 0;
        f->lungeTimer = 0;
        f->pendingMove = MOVE_NONE;
        f->aiMove = MOVE_NONE;
        if (f->cast.active) RtCancelCast(f);
    }
    if(status&STATUS1_BURN)RtPtdConditionFx(f,0);
    else if(status&STATUS1_PSN_ANY)RtPtdConditionFx(f,1);
    else if(status&STATUS1_PARALYSIS)RtPtdConditionFx(f,2);
    else if(status&STATUS1_SLEEP)RtPtdConditionFx(f,3);
    else if(status&STATUS1_FREEZE)RtPtdConditionFx(f,4);
    if (f->isEnemy) sRt->dirtyEnemy = TRUE; else sRt->dirtyPlayer = TRUE;
}

static bool8 RtTypeIs(struct RtFighter *f, u8 type)
{
    return f->types[0] == type || f->types[1] == type;
}

static bool8 RtInflictStatus(struct RtFighter *f, u32 status)
{
    if (f->hp == 0 || (f->status & STATUS1_ANY) || f->clocks[RTC_SAFEGUARD]) return FALSE;
    if ((status & STATUS1_SLEEP) && (f->ability==ABILITY_INSOMNIA || f->ability==ABILITY_VITAL_SPIRIT)) return FALSE;
    if ((status & STATUS1_SLEEP) && f->clocks[RTC_SLEEP_GUARD]) return FALSE;
    if ((status & STATUS1_PARALYSIS) && f->ability==ABILITY_LIMBER) return FALSE;
    if ((status & STATUS1_PSN_ANY) && f->ability==ABILITY_IMMUNITY) return FALSE;
    if ((status & STATUS1_BURN) && f->ability==ABILITY_WATER_VEIL) return FALSE;
    if ((status & STATUS1_FREEZE) && (f->ability==ABILITY_MAGMA_ARMOR || sRt->weather==RTW_SUN)) return FALSE;
    if ((status & STATUS1_PSN_ANY) && (RtTypeIs(f, TYPE_POISON) || RtTypeIs(f, TYPE_STEEL))) return FALSE;
    if ((status & STATUS1_BURN) && RtTypeIs(f, TYPE_FIRE)) return FALSE;
    if ((status & STATUS1_FREEZE) && RtTypeIs(f, TYPE_ICE)) return FALSE;
    RtSetStatus(f, status);
    RtGetNickname(f->mon, gStringVar1);
    StringCopy(gStringVar2, RtStatusLabel(f));
    RtShowTransientExpanded(sText_StatusApplied, 45);
    return TRUE;
}

static bool8 RtInflictConfusion(struct RtFighter *f)
{
    if (f->hp == 0 || f->confusionTimer || f->ability==ABILITY_OWN_TEMPO) return FALSE;
    f->confusionTimer = (2 + Random() % 4) * RT_STATUS_INTERVAL;
    RtPtdConditionFx(f,5);
    if (f->isEnemy) sRt->dirtyEnemy = TRUE; else sRt->dirtyPlayer = TRUE;
    RtGetNickname(f->mon, gStringVar1);
    StringCopy(gStringVar2, sText_Cfn);
    RtShowTransientExpanded(sText_StatusApplied, 45);
    return TRUE;
}

static bool8 RtStatusMove(struct RtFighter *atk, struct RtFighter *def, u16 move)
{
    u8 effect = gBattleMoves[move].effect;
    u32 status = 0;
    bool8 confused = FALSE;
    switch (effect)
    {
    case EFFECT_SLEEP: status = STATUS1_SLEEP_TURN(2 + Random() % 3); break;
    case EFFECT_POISON: status = STATUS1_POISON; break;
    case EFFECT_TOXIC: status = STATUS1_TOXIC_POISON; break;
    case EFFECT_PARALYZE: status = STATUS1_PARALYSIS; break;
    case EFFECT_WILL_O_WISP: status = STATUS1_BURN; break;
    case EFFECT_CONFUSE:
    case EFFECT_SWAGGER:
    case EFFECT_FLATTER:
    case EFFECT_TEETER_DANCE: confused = TRUE; break;
    case EFFECT_YAWN:
        if (!def->status && !def->yawnTimer) { def->yawnTimer = 120; return TRUE; }
        return FALSE;
    default: return FALSE;
    }
    if (move == MOVE_THUNDER_WAVE && !RtGetTypeMultiplier(TYPE_ELECTRIC, def->species)) return TRUE;
    if (confused)
    {
        if (effect == EFFECT_SWAGGER) RtStage(def,RT_STAT_ATK,2);
        if (effect == EFFECT_FLATTER) RtStage(def,RT_STAT_SPATK,1);
        RtInflictConfusion(def);
    }
    else RtInflictStatus(def, status);
    return TRUE;
}

static void RtSecondaryStatus(struct RtFighter *atk, struct RtFighter *def, u16 move)
{
    u8 effect = gBattleMoves[move].effect;
    u32 status = 0;
    if ((def->status & STATUS1_FREEZE) && gBattleMoves[move].type == TYPE_FIRE) RtSetStatus(def, STATUS1_NONE);
    if (!def->hp || def->ability==ABILITY_SHIELD_DUST || Random() % 100 >= gBattleMoves[move].secondaryEffectChance*(atk->ability==ABILITY_SERENE_GRACE?2:1)) return;
    if(move==MOVE_SECRET_POWER)
    {
        switch(BattleSetup_GetTerrainId()) {
        case BATTLE_TERRAIN_GRASS: RtInflictStatus(def,STATUS1_POISON); break;
        case BATTLE_TERRAIN_LONG_GRASS: RtInflictStatus(def,STATUS1_SLEEP_TURN(2+Random()%3)); break;
        case BATTLE_TERRAIN_SAND: RtStage(def,RT_STAT_ACC,-1); break;
        case BATTLE_TERRAIN_UNDERWATER: RtStage(def,RT_STAT_DEF,-1); break;
        case BATTLE_TERRAIN_WATER: RtStage(def,RT_STAT_ATK,-1); break;
        case BATTLE_TERRAIN_POND: RtStage(def,RT_STAT_SPEED,-1); break;
        case BATTLE_TERRAIN_MOUNTAIN: RtInflictConfusion(def); break;
        case BATTLE_TERRAIN_CAVE: def->clocks[RTC_FLINCH]=30; break;
        default: RtInflictStatus(def,STATUS1_PARALYSIS); }
        return;
    }
    switch (effect)
    {
    case EFFECT_POISON_HIT:
    case EFFECT_TWINEEDLE:
    case EFFECT_POISON_TAIL: status = STATUS1_POISON; break;
    case EFFECT_POISON_FANG: status = STATUS1_TOXIC_POISON; break;
    case EFFECT_BURN_HIT:
    case EFFECT_BLAZE_KICK: status = STATUS1_BURN; break;
    case EFFECT_FREEZE_HIT: status = STATUS1_FREEZE; break;
    case EFFECT_PARALYZE_HIT:
    case EFFECT_THUNDER: status = STATUS1_PARALYSIS; break;
    case EFFECT_CONFUSE_HIT: RtInflictConfusion(def); return;
    case EFFECT_TRI_ATTACK:
        switch (Random() % 3) { case 0: status = STATUS1_BURN; break; case 1: status = STATUS1_FREEZE; break; default: status = STATUS1_PARALYSIS; }
        break;
    }
    if (status) RtInflictStatus(def, status);
}

static void RtUpdateStatus(struct RtFighter *f)
{
    u16 dt = sRt->frameStep, damage;
    if (f->confusionTimer)
    {
        f->confusionTimer = f->confusionTimer > dt ? f->confusionTimer - dt : 0;
        if (!f->confusionTimer) { if (f->isEnemy) sRt->dirtyEnemy = TRUE; else sRt->dirtyPlayer = TRUE; }
    }
    if (f->yawnTimer)
    {
        f->yawnTimer = f->yawnTimer > dt ? f->yawnTimer - dt : 0;
        if (!f->yawnTimer) RtInflictStatus(f, STATUS1_SLEEP_TURN(2 + Random() % 3));
    }
    if (f->status & STATUS1_SLEEP)
    {
        f->statusTimer = f->statusTimer > dt ? f->statusTimer - dt : 0;
        if (!f->statusTimer) RtSetStatus(f, STATUS1_NONE);
        else
        {
            u32 status = (f->status & ~STATUS1_SLEEP) | ((f->statusTimer + 59) / 60);
            if (status != f->status) { f->status = status; SetMonData(f->mon, MON_DATA_STATUS, &f->status); }
        }
    }
    f->statusTick = f->statusTick > dt ? f->statusTick - dt : 0;
    if (!f->statusTick)
    {
        f->statusTick = RT_STATUS_INTERVAL;
        if(f->status&STATUS1_BURN)RtPtdConditionFx(f,0);
        else if(f->status&STATUS1_PSN_ANY)RtPtdConditionFx(f,1);
        if (f->status & (STATUS1_BURN | STATUS1_PSN_ANY))
        {
            if (f->status & STATUS1_TOXIC_POISON)
            {
                damage = f->maxHp * f->toxicCount / 16;
                if (f->toxicCount < 15) f->toxicCount++;
            }
            else damage = f->maxHp / 8;
            RtSetHp(f, (s32)f->hp - (damage ? damage : 1));
        }
        if ((f->status & STATUS1_FREEZE) && Random() % 5 == 0) RtSetStatus(f, STATUS1_NONE);
    }
}

static bool8 RtCanAct(struct RtFighter *f, u16 move)
{
    s32 attack, defense, damage;
    if (!f->hp || !RtMoveAllowed(f,move)) return FALSE;
    if ((f->status & STATUS1_SLEEP) && move != MOVE_SNORE && move != MOVE_SLEEP_TALK) return FALSE;
    if (f->status & STATUS1_FREEZE)
    {
        if (move == MOVE_FLAME_WHEEL || move == MOVE_SACRED_FIRE) RtSetStatus(f, STATUS1_NONE);
        else return FALSE;
    }
    if ((f->status & STATUS1_PARALYSIS) && Random() % 4 == 0)
    {
        RtBreakSequence(f);
        f->cooldown = f->recoveryMax = 45;
        return FALSE;
    }
    if (f->confusionTimer && Random() % 2 == 0)
    {
        RtBreakSequence(f);
        attack = RtGetStat(f, RT_STAT_ATK);
        if (f->status & STATUS1_BURN) attack /= 2;
        defense = RtGetStat(f, RT_STAT_DEF);
        damage = ((2 * f->level / 5 + 2) * 40 * attack / defense) / 50 + 2;
        RtSetHp(f, (s32)f->hp - damage);
        f->cooldown = f->recoveryMax = 60;
        f->hurtTimer = 20;
        RtSpawnFx(FX_IMPACT, FROM_Q8(f->x), FROM_Q8(f->y), 0, FALSE);
        return FALSE;
    }
    if (f->clocks[RTC_ATTRACT] && Random()%2==0)
    {
        RtBreakSequence(f);
        f->cooldown=f->recoveryMax=45;
        RtSpawnFx(FX_HEART,FROM_Q8(f->x),FROM_Q8(f->y)-12,20,FALSE);
        return FALSE;
    }
    return TRUE;
}

static void RtInitStatusGraphics(void)
{
    struct SpriteSheet sheet = {sRtStatusTiles, sizeof(sRtStatusTiles), RT_STATUS_TAG};
    sRt->statusTileStart = LoadSpriteSheet(&sheet);
    sRt->statusPalSlot = RtAcquirePal(RT_STATUS_PAL_KEY, sRtStatusPalette);
}

static void RtStatusSprite(struct RtFighter *f, u8 *id, u8 kind, bool8 above)
{
    struct SpriteTemplate t;
    struct Sprite *sp;
    if (kind == 0xFF || !f->active || !f->hp || sRt->statusTileStart == 0xFFFF || sRt->statusPalSlot == 0xFF)
    {
        if (*id != MAX_SPRITES) DestroySprite(&gSprites[*id]);
        *id = MAX_SPRITES;
        return;
    }
    if (*id == MAX_SPRITES)
    {
        t.tileTag = RT_STATUS_TAG; t.paletteTag = TAG_NONE;
        t.oam = &gOamData_AffineOff_ObjNormal_16x16; t.anims = gDummySpriteAnimTable;
        t.images = NULL; t.affineAnims = gDummySpriteAffineAnimTable; t.callback = SpriteCallbackDummy;
        *id = CreateSprite(&t, 0, 0, 0);
        if (*id == MAX_SPRITES) return;
    }
    sp = &gSprites[*id];
    sp->animPaused = TRUE;
    sp->oam.tileNum = sRt->statusTileStart + kind * 8 + ((sRt->fightFrames / 12) & 1) * 4;
    sp->oam.paletteNum = sRt->statusPalSlot;
    sp->oam.priority = 1; // visible on the field, behind modal BG0 panels
    sp->x = FROM_Q8(f->x) + (above ? 9 : 0);
    sp->y = FROM_Q8(f->y) + (above ? -13 : 1);
    sp->invisible = FALSE;
}

static void RtUpdateStatusVisual(struct RtFighter *f)
{
    u8 kind = 0xFF;
    if (f->status & STATUS1_BURN) kind = 0;
    else if (f->status & STATUS1_PSN_ANY) kind = 1;
    else if (f->status & STATUS1_PARALYSIS) kind = 2;
    else if (f->status & STATUS1_SLEEP) kind = 3;
    else if (f->status & STATUS1_FREEZE) kind = 4;
    RtStatusSprite(f, &f->statusSpriteId, kind, FALSE);
    RtStatusSprite(f, &f->confusionSpriteId, f->confusionTimer ? 5 : 0xFF, TRUE);
}
