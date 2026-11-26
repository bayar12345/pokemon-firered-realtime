// Deterministic team construction, independent of the live battle allocation.
bool8 RealtimeRosterDamaging(u16 move)
{
    return move && move < MOVES_COUNT && RtAiDamaging(move);
}

s32 RealtimeRosterMoveScore(struct Pokemon *mon, u16 move, u16 firstAttack)
{
    u16 species = GetMonData(mon, MON_DATA_SPECIES);
    u8 effect = gBattleMoves[move].effect, type = gBattleMoves[move].type, i;
    u16 attack = GetMonData(mon, MON_DATA_ATK), spAtk = GetMonData(mon, MON_DATA_SPATK);
    s32 score = 0, power = gBattleMoves[move].power, accuracy = gBattleMoves[move].accuracy;
    const struct RtMoveDef *profile = RtProfile(move);
    if (!move || move >= MOVES_COUNT || profile->flags & RTMF_DISABLED) return 0;
    if (!accuracy || profile->flags & RTMF_ALWAYS) accuracy = 100;
    if (RtAiDamaging(move))
    {
        switch (effect)
        {
        case EFFECT_DRAGON_RAGE: power = 65; break;
        case EFFECT_SONICBOOM: power = 35; break;
        case EFFECT_LEVEL_DAMAGE: case EFFECT_PSYWAVE: power = 55; break;
        case EFFECT_SUPER_FANG: power = 65; break;
        case EFFECT_OHKO: power = 100; break;
        }
        if (profile->flags & RTMF_MULTI) power *= (effect == EFFECT_DOUBLE_HIT || effect == EFFECT_TWINEEDLE) ? 2 : 3;
        if (move == MOVE_RETURN || move == MOVE_FRUSTRATION) power = 70;
        score = power * (RtSpecial(move, type) ? spAtk : attack) * 5 / (attack + spAtk + 1);
        if (type == gSpeciesInfo[species].types[0] || type == gSpeciesInfo[species].types[1]) score = score * 3 / 2;
        score = score * accuracy / 100;
        score = score * 180 / (90 + profile->startup + RtGetMoveRecovery(move) + RtGetMoveCooldown(move) / 4);
        if (!RtIsContactMove(move)) score = score * 6 / 5;
        if (profile->flags & RTMF_CHARGE) score = score * 3 / 4;
        if (effect == EFFECT_RECOIL || effect == EFFECT_DOUBLE_EDGE || effect == EFFECT_OVERHEAT) score = score * 4 / 5;
        if (effect == EFFECT_EXPLOSION || effect == EFFECT_DREAM_EATER || effect == EFFECT_SPIT_UP) score /= 4;
        if (firstAttack)
        {
            if (type == gBattleMoves[firstAttack].type) score = score * 2 / 3;
            if (profile->kind == RtProfile(firstAttack)->kind) score = score * 4 / 5;
        }
        return score > 0 ? score : 1;
    }
    // Speed affects real movement and is a useful support role alongside damage.
    if (move == MOVE_STRING_SHOT) return 145;
    switch (effect)
    {
    case EFFECT_RESTORE_HP: case EFFECT_SOFTBOILED: case EFFECT_MORNING_SUN:
    case EFFECT_SYNTHESIS: case EFFECT_MOONLIGHT: return 170;
    case EFFECT_REST: return 110;
    case EFFECT_SLEEP: return 150;
    case EFFECT_YAWN: return 115;
    case EFFECT_PARALYZE: return 155;
    case EFFECT_TOXIC: return 135;
    case EFFECT_POISON: return 85;
    case EFFECT_WILL_O_WISP: return 145;
    case EFFECT_CONFUSE: case EFFECT_SWAGGER: case EFFECT_FLATTER: return 110;
    case EFFECT_LEECH_SEED: return 145;
    case EFFECT_PROTECT: return 105;
    case EFFECT_REFLECT: case EFFECT_LIGHT_SCREEN: return 105;
    case EFFECT_SUBSTITUTE: return 110;
    }
    for (i = 0; i < ARRAY_COUNT(sStatEffects); i++)
    {
        const struct RtStatEffect *e = &sStatEffects[i];
        if (e->effect != effect) continue;
        if (e->self && e->stat == RT_STAT_ATK && attack * 4 < spAtk * 3) continue;
        if (e->self && e->stat == RT_STAT_SPATK && spAtk * 4 < attack * 3) continue;
        score += (e->stat == RT_STAT_SPEED ? 135 : 75) + (RtAbs(e->amount) - 1) * 35;
    }
    return score * accuracy / 100;
}
