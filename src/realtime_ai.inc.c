// Easy retains the earlier random decision policy. Normal/Hard evaluate only known moves and visible
// combat state; evaluating a move must never roll damage, spend PP or apply it.
static bool8 RtAiDamaging(u16 move)
{
    u8 effect = gBattleMoves[move].effect;
    return gBattleMoves[move].power || effect == EFFECT_DRAGON_RAGE
        || effect == EFFECT_SONICBOOM || effect == EFFECT_LEVEL_DAMAGE
        || effect == EFFECT_PSYWAVE || effect == EFFECT_SUPER_FANG || effect == EFFECT_OHKO;
}

static bool8 RtAiUsesStat(struct RtFighter *f, u8 stat)
{
    u8 i;
    u16 move;
    if (stat >= RT_STAT_SPEED && stat != RT_STAT_SPATK && stat != RT_STAT_SPDEF) return TRUE;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        move = RtMove(f, i);
        if (!move || !RtPp(f, i) || !RtAiDamaging(move)) continue;
        if ((stat == RT_STAT_ATK || stat == RT_STAT_DEF) && !RtSpecial(move,RtMoveType(f,move))) return TRUE;
        if ((stat == RT_STAT_SPATK || stat == RT_STAT_SPDEF) && !!RtSpecial(move,RtMoveType(f,move))) return TRUE;
    }
    return FALSE;
}

static s32 RtAiAccuracy(struct RtFighter *atk, struct RtFighter *def, u16 move)
{
    return RtAccuracyChance(atk,def,move);
}

static s32 RtAiDamage(struct RtFighter *atk, struct RtFighter *def, u16 move)
{
    u16 mult;
    bool8 crit;
    return RtComputeDamage(atk,def,move,&mult,&crit,FALSE);
}

static bool8 RtAiLineClear(s32 x, s32 y, s32 tx, s32 ty)
{
    s32 dist = RtLength(tx - x, ty - y), i, steps = dist / 8;
    s16 mx, my;
    if (sRt->arena) return RtArenaLineClear(x,y,tx,ty);
    if (steps < 3) return TRUE;
    // Match projectile collision, allowing its initial emergence from the icon.
    for (i = 2; i < steps; i++)
    {
        RtScreenToTile(x + (tx - x) * i / steps, y + (ty - y) * i / steps + 4, &mx, &my);
        if (MapGridGetCollisionAt(mx, my)
         && !(TestPlayerAvatarFlags(PLAYER_AVATAR_FLAG_SURFING) && MetatileBehavior_IsSurfable(MapGridGetMetatileBehaviorAt(mx, my)))) return FALSE;
    }
    return TRUE;
}

static s32 RtAiStatusScore(struct RtFighter *atk, struct RtFighter *def, u16 move)
{
    u8 effect = gBattleMoves[move].effect, i;
    s32 score = 0;
    struct RtFighter *target;
    const struct RtStatEffect *e;
    switch(move)
    {
    case MOVE_ATTRACT: return !def->clocks[RTC_ATTRACT] && RtAttractEligible(atk,def)?85:0;
    case MOVE_FOCUS_ENERGY: return !atk->clocks[RTC_FOCUS] && def->hp*2>def->maxHp?65:0;
    case MOVE_PROTECT: case MOVE_DETECT: case MOVE_BLOCK: case MOVE_COUNTER: case MOVE_MIRROR_COAT:
        if(!def->cast.active || def->cast.elapsed+36<def->cast.nextHit) return 0;
        if(move==MOVE_BLOCK || move==MOVE_COUNTER) return !RtSpecial(def->cast.move,RtMoveType(def,def->cast.move))?150:0;
        if(move==MOVE_MIRROR_COAT) return RtSpecial(def->cast.move,RtMoveType(def,def->cast.move))?150:0;
        return 120;
    case MOVE_RAIN_DANCE: return sRt->weather!=RTW_RAIN && RtTypeIs(atk,TYPE_WATER)?70:0;
    case MOVE_SUNNY_DAY: return sRt->weather!=RTW_SUN && (RtTypeIs(atk,TYPE_FIRE) || atk->ability==ABILITY_CHLOROPHYLL)?70:0;
    case MOVE_REFLECT: return !atk->clocks[RTC_REFLECT] && RtAiUsesStat(def,RT_STAT_ATK)?65:0;
    case MOVE_LIGHT_SCREEN: return !atk->clocks[RTC_LIGHTSCREEN] && RtAiUsesStat(def,RT_STAT_SPATK)?65:0;
    case MOVE_SUBSTITUTE: return !RtSubstitute(atk) && atk->hp*2>atk->maxHp?65:0;
    case MOVE_DISABLE: return def->lastMove && !def->clocks[RTC_DISABLE]?75:0;
    case MOVE_TAUNT: return !def->clocks[RTC_TAUNT]?45:0;
    case MOVE_LEECH_SEED: return !def->clocks[RTC_SEED] && !RtTypeIs(def,TYPE_GRASS)?80:0;
    case MOVE_MUD_SPORT: return !atk->clocks[RTC_MUDSPORT] && def->cast.active && RtMoveType(def,def->cast.move)==TYPE_ELECTRIC?80:0;
    case MOVE_WATER_SPORT: return !atk->clocks[RTC_WATERSPORT] && def->cast.active && RtMoveType(def,def->cast.move)==TYPE_FIRE?80:0;
    }
    switch (effect)
    {
    case EFFECT_SLEEP: return !def->status && !def->clocks[RTC_SLEEP_GUARD] ? 110 : 0;
    case EFFECT_YAWN: return !def->status && !def->yawnTimer && !def->clocks[RTC_SLEEP_GUARD] ? 85 : 0;
    case EFFECT_POISON:
    case EFFECT_TOXIC:
        return !def->status && !RtTypeIs(def, TYPE_POISON) && !RtTypeIs(def, TYPE_STEEL)
            && def->hp * 3 > def->maxHp ? (effect == EFFECT_TOXIC ? 100 : 75) : 0;
    case EFFECT_WILL_O_WISP:
        return !def->status && !RtTypeIs(def, TYPE_FIRE) ? (RtAiUsesStat(def, RT_STAT_ATK) ? 120 : 65) : 0;
    case EFFECT_PARALYZE:
        return !def->status && !(move == MOVE_THUNDER_WAVE && !RtGetTypeMultiplier(TYPE_ELECTRIC, def->species)) ? 100 : 0;
    case EFFECT_CONFUSE:
    case EFFECT_SWAGGER:
    case EFFECT_FLATTER:
    case EFFECT_TEETER_DANCE: return !def->confusionTimer ? 80 : 0;
    case EFFECT_RESTORE_HP:
    case EFFECT_SOFTBOILED:
    case EFFECT_MORNING_SUN:
    case EFFECT_SYNTHESIS:
    case EFFECT_MOONLIGHT:
    case EFFECT_REST:
        return atk->hp * 2 <= atk->maxHp ? 300 + (atk->maxHp - atk->hp) * 400 / atk->maxHp : 0;
    case EFFECT_REFRESH:
        return atk->status & (STATUS1_PSN_ANY | STATUS1_BURN | STATUS1_PARALYSIS) ? 150 : 0;
    case EFFECT_HEAL_BELL: return atk->status ? 150 : 0;
    case EFFECT_HAZE:
        for (i = 0; i < RT_STAT_COUNT; i++) score += def->stages[i] - atk->stages[i];
        return score >= 4 ? 100 : 0;
    }
    if (atk->hp * 3 < atk->maxHp) return 0;
    for (i = 0; i < ARRAY_COUNT(sStatEffects); i++)
    {
        e = &sStatEffects[i];
        if (e->effect != effect) continue;
        target = e->self ? atk : def;
        // Prefer fighting over repeatedly stacking setup/debuffs. Do not use
        // physical/special stages that neither side's known attacks can use.
        if (e->amount > 0 && target->stages[e->stat] >= 2) continue;
        if (e->amount < 0 && target->stages[e->stat] <= -2) continue;
        if (e->stat == RT_STAT_ATK || e->stat == RT_STAT_SPATK)
        { if (!RtAiUsesStat(target, e->stat)) continue; }
        if (e->stat == RT_STAT_DEF || e->stat == RT_STAT_SPDEF)
        { if (!RtAiUsesStat(e->self ? def : atk, e->stat == RT_STAT_DEF ? RT_STAT_ATK : RT_STAT_SPATK)) continue; }
        score += (e->stat == RT_STAT_SPEED ? 115 : 90) * RtAbs(e->amount);
        if (target->stages[e->stat] != 0) score = score * 2 / 3;
    }
    return score;
}

static s32 RtAiMoveScore(struct RtFighter *atk, struct RtFighter *def, u16 move, bool8 useRange)
{
    s32 damage, score, travel = 0, dist, speed;
    u16 recovery;
    if (!RtAiDamaging(move)) return RtAiStatusScore(atk, def, move) * RtAiAccuracy(atk, def, move) / 100;
    damage = RtAiDamage(atk, def, move);
    if (!damage) return 0;
    if (useRange && (RtIsContactMove(move) || move == MOVE_STRUGGLE))
    {
        dist = RtLength(FROM_Q8(def->x - atk->x), FROM_Q8(def->y - atk->y));
        speed = RtGetMoveSpeed(atk);
        if (dist > 24) travel = (dist - 24) * 256 / speed;
    }
    recovery = RtGetMoveRecovery(move);
    score = 12 + (damage < def->hp ? damage : def->hp) * 800 / def->maxHp;
    score = score * 150 / (recovery + 60 + travel);
    if (damage * 92 / 100 >= def->hp) score += 170 * 60 / (60 + travel);
    score = score * RtAiAccuracy(atk, def, move) / 100;
    if (useRange && !RtIsContactMove(move) && !RtAiLineClear(FROM_Q8(atk->x), FROM_Q8(atk->y), FROM_Q8(def->x), FROM_Q8(def->y))) score /= 3;
    if (gBattleMoves[move].effect == EFFECT_OVERHEAT && damage < def->hp) score = score * 4 / 5;
    if ((gBattleMoves[move].effect == EFFECT_RECOIL || gBattleMoves[move].effect == EFFECT_DOUBLE_EDGE || move == MOVE_STRUGGLE)
     && damage / 3 >= atk->hp && damage < def->hp) score /= 3;
    return score > 0 ? score : 1;
}

static u16 RtAiChooseMove(struct RtFighter *atk, struct RtFighter *def, u8 *slot)
{
    u8 i;
    u16 move, bestMove = MOVE_NONE;
    s32 score, best = 0;
    bool8 anyPp = FALSE;
    *slot = MAX_MON_MOVES;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        move = RtMove(atk, i);
        if (!move || !RtPp(atk, i)) continue;
        if(!RtMoveAllowed(atk,move)) continue;
        if ((atk->status & STATUS1_SLEEP) && move != MOVE_SNORE && move != MOVE_SLEEP_TALK) continue;
        anyPp = TRUE;
        if (atk->moveCooldown[i]) continue;
        score = RtAiMoveScore(atk, def, move, TRUE);
        if (score > best) { best = score; bestMove = move; *slot = i; }
    }
    if (!anyPp && !atk->struggleCooldown) return MOVE_STRUGGLE;
    return bestMove;
}

static bool8 RtAiRangedRole(struct RtFighter *atk, struct RtFighter *def)
{
    u8 i;
    u16 move;
    s32 score, melee = 0, ranged = 0;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        move = RtMove(atk, i);
        if (!move || !RtPp(atk, i) || !RtAiDamaging(move)) continue;
        score = RtAiMoveScore(atk, def, move, FALSE);
        if (RtIsContactMove(move)) { if (score > melee) melee = score; }
        else if (score > ranged) ranged = score;
    }
    return ranged > 0 && ranged * 10 >= melee * 9;
}

static bool8 RtAiSteer(struct RtFighter *f, struct RtFighter *target, s32 desired, s32 speed)
{
    static const s8 dirs[8][2] = {{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1},{0,-1},{1,-1}};
    s32 dx = FROM_Q8(target->x - f->x), dy = FROM_Q8(target->y - f->y);
    s32 dist = RtLength(dx, dy), gx, gy, vx, vy, x, y, cost, best = -100000;
    s32 bestX = 0, bestY = 0;
    u8 i;
    if (!f->aiStrafe) f->aiStrafe = Random() & 1 ? 1 : -1;
    if (dist > desired + 8) { gx = dx; gy = dy; }
    else if (dist < desired - 8) { gx = -dx; gy = -dy; }
    else { gx = -dy * f->aiStrafe; gy = dx * f->aiStrafe; }
    RtNormalize(gx, gy, 16, &gx, &gy);
    for (i = 0; i < 8; i++)
    {
        RtNormalize(dirs[i][0], dirs[i][1], 8, &vx, &vy);
        x = FROM_Q8(f->x) + vx; y = FROM_Q8(f->y) + vy;
        if (!RtIsSpotWalkable(x, y) || !RtIsSpotWalkable(FROM_Q8(f->x) + vx / 2, FROM_Q8(f->y) + vy / 2)) continue;
        cost = (vx * gx + vy * gy) / 8 - RtAbs(RtLength(FROM_Q8(target->x) - x, FROM_Q8(target->y) - y) - desired) * 3;
        if (f->aiRanged && RtAiLineClear(x, y, FROM_Q8(target->x), FROM_Q8(target->y))) cost += 12;
        cost-=RtArenaDangerAt(f,x,y);
        if (cost > best) { best = cost; bestX = dirs[i][0]; bestY = dirs[i][1]; }
    }
    if (!bestX && !bestY) { f->aiStrafe = -f->aiStrafe; return FALSE; }
    RtNormalize(bestX, bestY, speed, &vx, &vy);
    return RtMoveFighter(f, vx, vy);
}

static bool8 RtAiStartDash(struct RtFighter *f, s32 dx, s32 dy)
{
    s32 vx, vy;
    if (!RtCanMove(f) || f->dashCooldown || f->hurtTimer || f->status & STATUS1_PARALYSIS) return FALSE;
    RtNormalize(dx, dy, RtGetMoveSpeed(f) * 3, &vx, &vy);
    if ((!vx && !vy) || !RtIsSpotWalkable(FROM_Q8(f->x + vx * 2), FROM_Q8(f->y + vy * 2))) return FALSE;
    f->dashVx = vx; f->dashVy = vy;
    f->dashFrames = 12; f->dashCooldown = 150;
    f->windup = 0; f->pendingMove = MOVE_NONE; f->aiMove = MOVE_NONE;
    RtSpawnFx(FX_SMOKE, FROM_Q8(f->x), FROM_Q8(f->y), 12, FALSE);
    return TRUE;
}

static bool8 RtAiShotIncoming(void)
{
    u8 i;
    for (i = 0; i < RT_MAX_PROJ; i++)
        if (sRt->proj[i].active && !sRt->proj[i].fromEnemy && sRt->proj[i].aiDecision != 2) return TRUE;
    return FALSE;
}

static bool8 RtAiDodge(struct RtFighter *f)
{
    u8 i;
    s32 dx, dy, vx, vy, dot, cross, length;
    struct RtProjectile *p;
    if (f->dashCooldown) return FALSE;
    for (i = 0; i < RT_MAX_PROJ; i++)
    {
        p = &sRt->proj[i];
        if (!p->active || p->fromEnemy) continue;
        dx = FROM_Q8(f->x - p->x); dy = FROM_Q8(f->y - p->y);
        vx = p->vx; vy = p->vy;
        length = RtLength(vx, vy);
        dot = dx * vx + dy * vy;
        cross = RtAbs(dx * vy - dy * vx);
        if (!length || dot <= 0 || dot > length * 65 || cross > length * (RT_HIT_RADIUS + 5)) continue;
        if (sRt->aiDifficulty == RT_AI_HARD)
        {
            // HARD reacts like a quick player rather than a machine: it needs
            // about a twelfth of a second to notice a shot, cannot dodge one that
            // is already close (about one tile away), and only commits to
            // dodging some of the shots it does see. NIGHTMARE skips all this.
            if (p->age < 5) continue;
            if (dot < length * 18) { p->aiDecision = 2; continue; }
            if (!p->aiDecision) p->aiDecision = (Random() % 100) < 70 ? 1 : 2;
            if (p->aiDecision != 1) continue;
        }
#ifdef PMD_TEST
        gRtCnt[7]++;
        if (RtAiStartDash(f, -vy * f->aiStrafe, vx * f->aiStrafe) || RtAiStartDash(f, vy * f->aiStrafe, -vx * f->aiStrafe)) { gRtCnt[9]++; return TRUE; }
#endif
        if (RtAiStartDash(f, -vy * f->aiStrafe, vx * f->aiStrafe)) return TRUE;
        if (RtAiStartDash(f, vy * f->aiStrafe, -vx * f->aiStrafe)) return TRUE;
    }
    return FALSE;
}

static void RtUpdateSmartEnemy(void)
{
    struct RtFighter *f = &sRt->enemy, *target = &sRt->player;
    u16 elapsed = sRt->frameStep, move;
    u8 pp, i;
    s32 dist, dx, dy, desired;
    bool8 contact;
    RtUpdateCommon(f);
    f->dashCooldown = f->dashCooldown > elapsed ? f->dashCooldown - elapsed : 0;
    if (!f->hp || f->status & STATUS1_FREEZE) { f->dashFrames = 0; return; }
    RtUpdateLunge(f, target);
    if (RtBusy(f) || !target->active || !target->hp || f->clocks[RTC_RAMPAGE]) return;
    if (f->dashFrames)
    {
        if (f->hurtTimer || !RtCanMove(f)) { f->dashFrames = 0; return; }
        // Small collision-checked steps prevent a burst from crossing walls.
        for (i = 0; i < elapsed && f->dashFrames; i++)
        {
            if (!RtMoveFighter(f, f->dashVx, f->dashVy)) { f->dashFrames = 0; break; }
            f->dashFrames--;
        }
        RtFace(f, target);
        return;
    }
    dx = FROM_Q8(target->x - f->x); dy = FROM_Q8(target->y - f->y); dist = RtLength(dx, dy);
    if (!f->aiStrafe) f->aiStrafe = Random() & 1 ? 1 : -1;
    if (sRt->aiDifficulty >= RT_AI_HARD && RtAiDodge(f)) return;
    if (f->windup)
    {
        RtFace(f, target);
        f->windup = f->windup > elapsed ? f->windup - elapsed : 0;
        if (f->windup) return;
        move = f->pendingMove;
        f->aiMove = MOVE_NONE; f->aiTimer = 0;
        contact = RtIsContactMove(move) || move == MOVE_STRUGGLE;
        if ((contact && dist >= RtProfile(move)->reach) || (!contact && RtAiDamaging(move) && !RtAiLineClear(FROM_Q8(f->x), FROM_Q8(f->y), FROM_Q8(target->x), FROM_Q8(target->y)) && !RtArenaCanBreakLine(f,target,move))) return;
        if (f->aiSlot < MAX_MON_MOVES)
        {
            if (f->moveCooldown[f->aiSlot]) return;
            pp = RtPp(f, f->aiSlot);
            if (!pp || !RtCanAct(f, move)) return;
            RtSpendPp(f,f->aiSlot,move);
        }
        else if (!RtCanAct(f, move)) return;
        RtUseMove(f, target, move);
        return;
    }
    f->aiTimer = f->aiTimer > elapsed ? f->aiTimer - elapsed : 0;
    if (!f->aiTimer)
    {
        f->aiTimer = sRt->aiDifficulty == RT_AI_NIGHTMARE ? 8 : sRt->aiDifficulty == RT_AI_HARD ? 12 : 16;
        f->aiRanged = RtAiRangedRole(f, target);
        f->aiMove = RtAiChooseMove(f, target, &f->aiSlot);
    }
    move = f->aiMove;
    if (move && !f->cooldown && f->hurtTimer <= 6)
    {
        contact = RtIsContactMove(move) || move == MOVE_STRUGGLE;
        if (RtProfile(move)->kind==RT_SELF || (dist < RtProfile(move)->reach && (contact || (RtAiLineClear(FROM_Q8(f->x), FROM_Q8(f->y), FROM_Q8(target->x), FROM_Q8(target->y)) || RtArenaCanBreakLine(f,target,move)))))
        {
            f->pendingMove = move;
            f->windup = (contact ? 24 : 30) - (sRt->aiDifficulty >= RT_AI_HARD ? 6 : 0);
            if (move == MOVE_PROTECT || move == MOVE_DETECT || move == MOVE_BLOCK || move == MOVE_COUNTER || move == MOVE_MIRROR_COAT)
                f->windup = 1;
            RtFace(f, target);
            return;
        }
    }
    desired = f->aiRanged ? (move && RtProfile(move)->reach<100 ? 48 : 88) : (f->cooldown > 30 ? 38 : 20);
    // A ready melee attack takes precedence over the general ranged role.
    if (move && RtAiDamaging(move) && RtIsContactMove(move) && !f->cooldown) desired = 20;
    // HARD keeps its dash for dodging while one of the player's shots is in the air.
    if (sRt->aiDifficulty >= RT_AI_HARD && !f->windup && !(sRt->aiDifficulty == RT_AI_HARD && RtAiShotIncoming()))
    {
        if (desired == 20 && dist > 65 && RtAiStartDash(f, dx, dy)) return;
        if (f->aiRanged && dist < 42 && RtAiStartDash(f, -dx, -dy)) return;
    }
    if (!f->hurtTimer && RtCanMove(f)) RtAiSteer(f, target, desired, RtGetMoveSpeed(f));
    RtFace(f, target);
}

static void RtUpdateEnemy(void)
{
    if (sRt->aiDifficulty == RT_AI_EASY) RtUpdateEasyEnemy();
    else RtUpdateSmartEnemy();
}
