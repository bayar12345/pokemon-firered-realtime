// Native party data stays intact during temporary move/stat transformations.
// Every GetMonData call decrypts and checksums the whole party slot, and the
// AI and HUD ask for moves and PP many times per frame, so read them once per frame.
static u8 RtMonCacheSlot(struct RtFighter *f)
{
    u8 c = f->isEnemy ? 1 : 0, i;
    if (sRt->cacheMon[c] != f->mon || sRt->cacheStamp[c] != gMain.vblankCounter2)
    {
        sRt->cacheMon[c] = f->mon;
        sRt->cacheStamp[c] = gMain.vblankCounter2;
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            sRt->cacheMoves[c][i] = f->mon ? GetMonData(f->mon, MON_DATA_MOVE1 + i) : MOVE_NONE;
            sRt->cachePp[c][i] = f->mon ? GetMonData(f->mon, MON_DATA_PP1 + i) : 0;
        }
    }
    return c;
}
static u16 RtMove(struct RtFighter *f, u8 slot)
{
    return (f->overrideMask & (1 << slot)) ? f->overrideMoves[slot] : sRt->cacheMoves[RtMonCacheSlot(f)][slot];
}
static u8 RtPp(struct RtFighter *f, u8 slot)
{
    return (f->overrideMask & (1 << slot)) ? f->overridePp[slot] : sRt->cachePp[RtMonCacheSlot(f)][slot];
}
static u8 RtMaxPp(struct RtFighter *f, u8 slot)
{
    u16 move = RtMove(f, slot);
    if (!move) return 0;
    return (f->overrideMask & (1 << slot)) ? f->overrideMaxPp[slot] : CalculatePPWithBonus(move, GetMonData(f->mon, MON_DATA_PP_BONUSES), slot);
}
static void RtSetPp(struct RtFighter *f, u8 slot, u8 pp)
{
    if (f->overrideMask & (1 << slot)) f->overridePp[slot] = pp;
    else { SetMonData(f->mon, MON_DATA_PP1 + slot, &pp); sRt->cacheMon[f->isEnemy ? 1 : 0] = NULL; }
    if (!f->isEnemy) sRt->dirtyMoves = TRUE;
}
static bool8 RtHasPp(struct RtFighter *f)
{
    u8 i;
    for (i = 0; i < 4; i++) if (RtMove(f, i) && RtPp(f, i) && RtMoveAllowed(f,RtMove(f,i))) return TRUE;
    return FALSE;
}
static u16 RtDec(u16 value, u16 dt) { return value > dt ? value - dt : 0; }
static u16 RtSleepFrames(u32 status,u8 ability)
{
    u16 seconds=status&STATUS1_SLEEP;
    if(seconds>4) seconds=4;
    return seconds*(ability==ABILITY_EARLY_BIRD?30:60);
}
static bool8 RtDamaging(u16 move)
{
    u8 e = gBattleMoves[move].effect;
    return gBattleMoves[move].power || e == EFFECT_DRAGON_RAGE || e == EFFECT_SONICBOOM
        || e == EFFECT_LEVEL_DAMAGE || e == EFFECT_PSYWAVE || e == EFFECT_SUPER_FANG || e == EFFECT_OHKO;
}
static const struct RtMoveDef *RtProfile(u16 move)
{
    return &sRtMoves[move < MOVES_COUNT ? move : MOVE_NONE];
}
static void RtSpendPp(struct RtFighter *f,u8 slot,u16 move)
{
    struct RtFighter *other=f->isEnemy?&sRt->player:&sRt->enemy;
    u8 pp=RtPp(f,slot), cost=1;
    if(other->ability==ABILITY_PRESSURE && !(gBattleMoves[move].target&MOVE_TARGET_USER)) cost=2;
    RtSetPp(f,slot,pp>cost?pp-cost:0);
}
static bool8 RtSpecial(u16 move, u8 type) { return move == MOVE_EARTH_POWER || !IS_TYPE_PHYSICAL(type); }
static bool8 RtPsychicHeld(struct RtFighter *f)
{
    struct RtFighter *caster = RtOther(f);
    struct RtCast *c = &caster->cast;
    return f->active && f->hp && caster->hp && c->active
        && c->move == MOVE_PSYCHIC && c->hit && !c->landed;
}
static bool8 RtBusy(struct RtFighter *f) { return f->cast.active || f->clocks[RTC_FLINCH] || RtPsychicHeld(f); }
static bool8 RtCanMove(struct RtFighter *f)
{
    return !RtPsychicHeld(f) && !f->hidden && !(f->status & (STATUS1_SLEEP | STATUS1_FREEZE)) && !f->clocks[RTC_ROOT] && !f->clocks[RTC_FLINCH];
}
static u8 RtMoveType(struct RtFighter *f, u16 move)
{
    u8 type = gBattleMoves[move].type;
    u32 iv;
    if (move == MOVE_WEATHER_BALL && RtWeatherWorks())
    {
        if (sRt->weather == RTW_SUN) type = TYPE_FIRE;
        if (sRt->weather == RTW_RAIN) type = TYPE_WATER;
        if (sRt->weather == RTW_SAND) type = TYPE_ROCK;
        if (sRt->weather == RTW_HAIL) type = TYPE_ICE;
    }
    if (move == MOVE_HIDDEN_POWER)
    {
        iv = (GetMonData(f->mon, MON_DATA_HP_IV) & 1)
          | ((GetMonData(f->mon, MON_DATA_ATK_IV) & 1) << 1)
          | ((GetMonData(f->mon, MON_DATA_DEF_IV) & 1) << 2)
          | ((GetMonData(f->mon, MON_DATA_SPEED_IV) & 1) << 3)
          | ((GetMonData(f->mon, MON_DATA_SPATK_IV) & 1) << 4)
          | ((GetMonData(f->mon, MON_DATA_SPDEF_IV) & 1) << 5);
        type = 1 + iv * 15 / 63;
        if (type >= TYPE_MYSTERY) type++;
    }
    return type;
}
