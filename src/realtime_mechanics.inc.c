// Gen III mechanics shared by live combat and read-only AI evaluation.
// Keep random choices at cast/hit boundaries, never in move scoring.
static u8 RtHiddenPower(struct RtFighter *f)
{
    static const u8 ids[] = {MON_DATA_HP_IV, MON_DATA_ATK_IV, MON_DATA_DEF_IV,
        MON_DATA_SPEED_IV, MON_DATA_SPATK_IV, MON_DATA_SPDEF_IV};
    u8 i, bits = 0;
    for (i = 0; i < 6; i++) bits |= ((GetMonData(f->mon, ids[i]) >> 1) & 1) << i;
    return 30 + bits * 40 / 63;
}

static u8 RtMagnitudePower(u8 roll)
{
    if (roll < 5) return 10;
    if (roll < 15) return 30;
    if (roll < 35) return 50;
    if (roll < 65) return 70;
    if (roll < 85) return 90;
    if (roll < 95) return 110;
    return 150;
}

static bool8 RtRepeatingMove(u16 move)
{
    return move == MOVE_ROLLOUT || move == MOVE_ICE_BALL || move == MOVE_FURY_CUTTER;
}

static void RtBreakSequence(struct RtFighter *f)
{
    f->rollout = 0;
    f->sequenceMove = MOVE_NONE;
}

static void RtRegisterHit(struct RtFighter *f, u16 move)
{
    if (f->cast.active && f->cast.move == move)
    {
        if (RtRepeatingMove(move) && !f->cast.landed && f->rollout < 5) f->rollout++;
        f->cast.landed = TRUE;
    }
}

static bool8 RtAttractEligible(struct RtFighter *f, struct RtFighter *target)
{
    u8 a = GetMonGender(f->mon), b = GetMonGender(target->mon);
    return target->ability != ABILITY_OBLIVIOUS && a != MON_GENDERLESS
        && b != MON_GENDERLESS && a != b;
}

static u8 RtCriticalDenominator(struct RtFighter *atk, struct RtFighter *def, u16 move)
{
    static const u8 chances[] = {16, 8, 4, 3, 2};
    u8 effect = gBattleMoves[move].effect, hold = ItemId_GetHoldEffect(atk->item);
    u8 stage = atk->clocks[RTC_FOCUS] ? 2 : 0;
    if (def->ability == ABILITY_BATTLE_ARMOR || def->ability == ABILITY_SHELL_ARMOR) return 0;
    if (effect == EFFECT_HIGH_CRITICAL || effect == EFFECT_SKY_ATTACK
     || effect == EFFECT_BLAZE_KICK || effect == EFFECT_POISON_TAIL) stage++;
    if (hold == HOLD_EFFECT_SCOPE_LENS) stage++;
    if ((hold == HOLD_EFFECT_LUCKY_PUNCH && atk->species == SPECIES_CHANSEY)
     || (hold == HOLD_EFFECT_STICK && atk->species == SPECIES_FARFETCHD)) stage += 2;
    return chances[stage < 5 ? stage : 4];
}

static s32 RtDamageStat(struct RtFighter *f, u8 stat, bool8 critical, bool8 defending)
{
    static const u8 ids[] = {MON_DATA_ATK, MON_DATA_DEF, MON_DATA_SPEED, MON_DATA_SPATK, MON_DATA_SPDEF};
    s32 value = f->transformed ? f->copiedStats[stat] : GetMonData(f->mon, ids[stat]);
    s8 stage = f->stages[stat];
    if (critical && (defending ? stage > 0 : stage < 0)) stage = 0;
    value = value * sStatStageRatios[stage + 6][0] / sStatStageRatios[stage + 6][1];
    return value > 0 ? value : 1;
}

static u16 RtBasePower(struct RtFighter *atk, struct RtFighter *def, u16 move)
{
    u32 power = gBattleMoves[move].power;
    u32 ratio = atk->hp * 48 / atk->maxHp;
    if (move == MOVE_HIDDEN_POWER) return RtHiddenPower(atk);
    if (move == MOVE_MAGNITUDE) return atk->cast.active && atk->cast.move == move ? atk->cast.power : 71;
    if (move == MOVE_RETURN) power = GetMonData(atk->mon, MON_DATA_FRIENDSHIP) * 10 / 25;
    if (move == MOVE_FRUSTRATION) power = (255 - GetMonData(atk->mon, MON_DATA_FRIENDSHIP)) * 10 / 25;
    if (move == MOVE_ERUPTION || move == MOVE_WATER_SPOUT) power = 150 * atk->hp / atk->maxHp;
    if (move == MOVE_FACADE && atk->status & (STATUS1_PSN_ANY | STATUS1_BURN | STATUS1_PARALYSIS)) power *= 2;
    if (move == MOVE_SMELLING_SALT && def->status & STATUS1_PARALYSIS) power *= 2;
    if (move == MOVE_REVENGE && atk->recentDamage[atk->historySlot]) power *= 2;
    if (move == MOVE_WEATHER_BALL && sRt->weather && RtWeatherWorks()) power *= 2;
    if (move == MOVE_REVERSAL || move == MOVE_FLAIL)
        power = ratio <= 1 ? 200 : ratio <= 4 ? 150 : ratio <= 9 ? 100 : ratio <= 16 ? 80 : ratio <= 32 ? 40 : 20;
    if (move == MOVE_SPIT_UP) return atk->stockpile * 100;
    if (move == MOVE_PURSUIT && def->cast.active && def->cast.move == MOVE_TELEPORT) power *= 2;
    if (move == MOVE_LOW_KICK)
    {
        u16 weight = GetPokedexHeightWeight(SpeciesToNationalPokedexNum(def->species), 1);
        power = weight < 100 ? 20 : weight < 250 ? 40 : weight < 500 ? 60 : weight < 1000 ? 80 : weight < 2000 ? 100 : 120;
    }
    if (RtRepeatingMove(move))
    {
        power <<= atk->rollout > 4 ? 4 : atk->rollout;
        if (move != MOVE_FURY_CUTTER && atk->defenseCurl) power *= 2;
    }
    if (move == MOVE_TRIPLE_KICK) power *= atk->cast.hitCount ? atk->cast.hitCount : 1;
    return power ? power : 1;
}

static bool8 RtCanHitHidden(struct RtFighter *target, u16 move)
{
    u16 hiddenMove = target->cast.move;
    if (!target->hidden) return TRUE;
    if (hiddenMove == MOVE_FLY || hiddenMove == MOVE_BOUNCE)
        return move == MOVE_GUST || move == MOVE_TWISTER || move == MOVE_THUNDER || move == MOVE_SKY_UPPERCUT;
    if (hiddenMove == MOVE_DIG) return move == MOVE_EARTHQUAKE || move == MOVE_MAGNITUDE;
    if (hiddenMove == MOVE_DIVE) return move == MOVE_SURF || move == MOVE_WHIRLPOOL;
    return FALSE;
}

static bool8 RtCallAllowed(u16 caller, u16 move)
{
    if (!move || move >= MOVES_COUNT || move == MOVE_STRUGGLE) return FALSE;
    if (caller == MOVE_SKETCH) return move != MOVE_SKETCH;
    if (caller == MOVE_MIMIC)
        return move != MOVE_METRONOME && move != MOVE_SKETCH && move != MOVE_MIMIC;
    if (caller == MOVE_MIRROR_MOVE) return (gBattleMoves[move].flags & FLAG_MIRROR_MOVE_AFFECTED) != 0;
    if (move == MOVE_SLEEP_TALK || move == MOVE_ASSIST || move == MOVE_MIRROR_MOVE || move == MOVE_METRONOME) return FALSE;
    if (caller == MOVE_SLEEP_TALK)
        return move != MOVE_FOCUS_PUNCH && move != MOVE_UPROAR
            && !(RtProfile(move)->flags & RTMF_CHARGE) && RtProfile(move)->kind != RT_AIR;
    switch (move)
    {
    case MOVE_SKETCH: case MOVE_MIMIC: case MOVE_COUNTER: case MOVE_MIRROR_COAT:
    case MOVE_PROTECT: case MOVE_DETECT: case MOVE_ENDURE: case MOVE_DESTINY_BOND:
    case MOVE_THIEF: case MOVE_FOLLOW_ME: case MOVE_SNATCH: case MOVE_HELPING_HAND:
    case MOVE_COVET: case MOVE_TRICK: case MOVE_FOCUS_PUNCH: return FALSE;
    }
    return !(RtProfile(move)->flags & RTMF_DISABLED);
}
