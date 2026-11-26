// Timed battle effects, damage routing and persistent field objects.
static struct RtFighter *RtOther(struct RtFighter *f) { return f->isEnemy ? &sRt->player : &sRt->enemy; }
static u8 RtSide(struct RtFighter *f) { return f->isEnemy ? 1 : 0; }
static bool8 RtWeatherWorks(void)
{
    return sRt->player.ability != ABILITY_CLOUD_NINE && sRt->enemy.ability != ABILITY_CLOUD_NINE
        && sRt->player.ability != ABILITY_AIR_LOCK && sRt->enemy.ability != ABILITY_AIR_LOCK;
}

static void RtApplyEntryAbilities(struct RtFighter *f)
{
    struct RtFighter *other=RtOther(f);
    if(f->entryApplied || !f->active || !f->hp || !other->active) return;
    f->entryApplied=TRUE;
    if(f->ability==ABILITY_TRACE && other->ability!=ABILITY_TRACE && other->ability!=ABILITY_NONE) f->ability=other->ability;
    if(f->ability==ABILITY_INTIMIDATE) RtStage(other,RT_STAT_ATK,-1);
    if(f->ability==ABILITY_DRIZZLE) RtSetBattleWeather(RTW_RAIN,900);
    if(f->ability==ABILITY_DROUGHT) RtSetBattleWeather(RTW_SUN,900);
    if(f->ability==ABILITY_SAND_STREAM) RtSetBattleWeather(RTW_SAND,900);
}

static bool8 RtUseTrainerItem(void)
{
    struct RtFighter *f=&sRt->enemy;
    u8 i; u16 item,heal;
    if(!sRt->isTrainer || !f->hp || f->cast.active || f->cooldown || sRt->trainerItemTimer) return FALSE;
    for(i=0;i<4;i++)
    {
        if(sRt->trainerItemsUsed&(1<<i)) continue;
        item=gTrainers[gTrainerBattleOpponent_A].items[i]; heal=0;
        if(item==ITEM_FULL_RESTORE && (f->hp*4<=f->maxHp || f->status || f->confusionTimer)) heal=f->maxHp;
        if(item==ITEM_HYPER_POTION && f->hp*4<=f->maxHp) heal=200;
        if(item==ITEM_SUPER_POTION && f->hp*4<=f->maxHp) heal=50;
        if(item==ITEM_POTION && f->hp*4<=f->maxHp) heal=20;
        if(!heal && !(item==ITEM_FULL_HEAL && (f->status || f->confusionTimer))) continue;
        if(item==ITEM_FULL_HEAL || item==ITEM_FULL_RESTORE) { RtSetStatus(f,0); f->confusionTimer=0; }
        if(heal) RtSetHp(f,f->hp+heal);
        sRt->trainerItemsUsed|=1<<i;
        sRt->trainerItemTimer=300;
        f->cooldown=f->recoveryMax=90;
        f->windup=0; f->pendingMove=f->aiMove=MOVE_NONE;
        RtSpawnFx(FX_SPARKLE,FROM_Q8(f->x),FROM_Q8(f->y),20,FALSE);
        PlaySE(SE_USE_ITEM);
        return TRUE;
    }
    return FALSE;
}

static void RtLeaveField(struct RtFighter *f)
{
    struct RtFighter *other=RtOther(f);
    if(f->ability==ABILITY_NATURAL_CURE && f->hp) RtSetStatus(f,0);
    if(other->clocks[RTC_ATTRACT] && other->attractParty==f->partyIndex) other->clocks[RTC_ATTRACT]=0;
    other->clocks[RTC_TRAP]=0;
    other->noEscapeTimer=0;
    f->clocks[RTC_ATTRACT]=0;
}
static bool8 RtSoundMove(u16 move)
{
    switch (move) {
    case MOVE_GROWL: case MOVE_ROAR: case MOVE_SING: case MOVE_SUPERSONIC:
    case MOVE_SCREECH: case MOVE_SNORE: case MOVE_PERISH_SONG: case MOVE_UPROAR:
    case MOVE_HYPER_VOICE: case MOVE_METAL_SOUND: case MOVE_GRASS_WHISTLE: return TRUE;
    } return FALSE;
}
static u16 RtMultiplier(u8 type, struct RtFighter *f)
{
    u16 i = 0, mult = 100;
    while (gTypeEffectiveness[i] != TYPE_ENDTABLE)
    {
        if (gTypeEffectiveness[i] == TYPE_FORESIGHT) { i+=3; continue; }
        if (gTypeEffectiveness[i] == type)
        {
            if(f->clocks[RTC_FORESIGHT] && (type==TYPE_NORMAL || type==TYPE_FIGHTING) && gTypeEffectiveness[i+1]==TYPE_GHOST) { i+=3; continue; }
            if (gTypeEffectiveness[i+1] == f->types[0]) mult = mult * gTypeEffectiveness[i+2] / 10;
            if (gTypeEffectiveness[i+1] == f->types[1] && f->types[0] != f->types[1]) mult = mult * gTypeEffectiveness[i+2] / 10;
        }
        i += 3;
    }
    return mult;
}
static void RtStage(struct RtFighter *f, u8 stat, s8 change)
{
    s8 next;
    if (change < 0 && (f->ability == ABILITY_CLEAR_BODY || f->ability == ABILITY_WHITE_SMOKE
     || (stat == RT_STAT_ATK && f->ability == ABILITY_HYPER_CUTTER)
     || (stat == RT_STAT_ACC && f->ability == ABILITY_KEEN_EYE) || f->clocks[RTC_MIST])) return;
    next = f->stages[stat] + change;
    next=next < -6 ? -6 : next > 6 ? 6 : next;
    if(next!=f->stages[stat])RtPtdStageFx(f,stat,next-f->stages[stat]);
    f->stages[stat]=next;
}
static void RtSetHeldItem(struct RtFighter *f, u16 item)
{
    f->item = item;
    SetMonData(f->mon, MON_DATA_HELD_ITEM, &item);
}
static void RtConsumeItem(struct RtFighter *f)
{
    f->recycledItem = f->item;
    RtSetHeldItem(f, ITEM_NONE);
    RtSpawnFx(FX_SPARKLE, FROM_Q8(f->x), FROM_Q8(f->y), 16, FALSE);
}
static void RtTryBerry(struct RtFighter *f)
{
    u8 effect = ItemId_GetHoldEffect(f->item), param = ItemId_GetHoldEffectParam(f->item), i;
    bool8 used = FALSE;
    if (!f->item || !f->hp) return;
    if (effect == HOLD_EFFECT_RESTORE_HP && f->hp * 2 <= f->maxHp) { RtSetHp(f, f->hp + param); used = TRUE; }
    if (effect == HOLD_EFFECT_CURE_STATUS && (f->status || f->confusionTimer)) { RtSetStatus(f, 0); f->confusionTimer = 0; used = TRUE; }
    if (effect == HOLD_EFFECT_CURE_ATTRACT && f->clocks[RTC_ATTRACT]) { f->clocks[RTC_ATTRACT]=0; used=TRUE; }
    if ((effect == HOLD_EFFECT_CURE_PAR && f->status & STATUS1_PARALYSIS)
     || (effect == HOLD_EFFECT_CURE_SLP && f->status & STATUS1_SLEEP)
     || (effect == HOLD_EFFECT_CURE_PSN && f->status & STATUS1_PSN_ANY)
     || (effect == HOLD_EFFECT_CURE_BRN && f->status & STATUS1_BURN)
     || (effect == HOLD_EFFECT_CURE_FRZ && f->status & STATUS1_FREEZE)) { RtSetStatus(f, 0); used = TRUE; }
    if (effect == HOLD_EFFECT_CURE_CONFUSION && f->confusionTimer) { f->confusionTimer = 0; used = TRUE; }
    if (effect == HOLD_EFFECT_RESTORE_PP)
    {
        for (i = 0; i < 4; i++) if (RtMove(f,i) && !RtPp(f,i)) { RtSetPp(f,i, param < RtMaxPp(f,i) ? param : RtMaxPp(f,i)); used = TRUE; break; }
    }
    if (effect == HOLD_EFFECT_RESTORE_STATS)
    {
        for (i = 0; i < RT_STAT_COUNT; i++) if (f->stages[i] < 0) { f->stages[i] = 0; used = TRUE; }
    }
    if (used) RtConsumeItem(f);
}
static struct RtField *RtSubstitute(struct RtFighter *f)
{
    u8 i;
    for (i = 0; i < 8; i++) if (sRt->field[i].active && sRt->field[i].kind == RTF_SUB && sRt->field[i].enemy == f->isEnemy) return &sRt->field[i];
    return NULL;
}
static void RtRemoveField(struct RtField *field)
{
    if (field->fxId < RT_MAX_FX) RtDestroyFx(field->fxId);
    if (field->spriteId != MAX_SPRITES) DestroySprite(&gSprites[field->spriteId]);
    field->active = FALSE;
}
static bool8 RtFieldBlocks(struct RtFighter *f, s32 x, s32 y)
{
    u8 i;
    for (i = 0; i < 8; i++)
    {
        struct RtField *field = &sRt->field[i];
        if (field->active && field->kind == RTF_SUB && field->enemy != f->isEnemy
         && RtAbs(x-field->x) < 13 && RtAbs(y-field->y) < 14) return TRUE;
    }
    return FALSE;
}
static bool8 RtHitSubstitute(struct RtFighter *f, s32 x, s32 y, u16 damage)
{
    struct RtField *sub = RtSubstitute(RtOther(f));
    if (!sub || RtAbs(x-sub->x) > 17 || RtAbs(y-sub->y) > 18) return FALSE;
    if (damage >= sub->hp) { RtSpawnFx(FX_SMOKE,sub->x,sub->y,20,FALSE); RtRemoveField(sub); }
    else { sub->hp -= damage; RtSpawnFx(FX_IMPACT,sub->x,sub->y,8,FALSE); }
    return TRUE;
}
static bool8 RtSubInLine(struct RtFighter *f, struct RtFighter *target)
{
    struct RtField *sub = RtSubstitute(target);
    s32 dx,dy,sx,sy,dot,cross,len;
    if (!sub) return FALSE;
    dx = FROM_Q8(target->x-f->x); dy = FROM_Q8(target->y-f->y);
    sx = sub->x-FROM_Q8(f->x); sy=sub->y-FROM_Q8(f->y);
    dot = sx*dx+sy*dy; len=RtLength(dx,dy); cross=RtAbs(sx*dy-sy*dx);
    return dot > 0 && dot < dx*dx+dy*dy && cross < len*18;
}
static bool8 RtPlaceField(struct RtFighter *f, u8 kind, s16 x, s16 y)
{
    u8 i;
    struct RtField *field;
    if (!RtIsSpotWalkable(x,y)) return FALSE;
    if (kind == RTF_SUB && RtSubstitute(f)) return FALSE;
    for (i=0;i<8;i++) if (!sRt->field[i].active) break;
    if (i==8) return FALSE;
    field=&sRt->field[i]; memset(field,0,sizeof(*field));
    field->active=TRUE; field->enemy=f->isEnemy; field->kind=kind; field->x=x; field->y=y;
    field->spriteId=MAX_SPRITES; field->fxId=0xFF;
    field->hp=f->maxHp/4 ? f->maxHp/4 : 1;
    field->life=kind==RTF_SUB ? 0xFFFF : 1200;
    if (kind == RTF_SUB)
    {
        if (f->hp <= field->hp) { field->active=FALSE; return FALSE; }
        RtSetHp(f,f->hp-field->hp);
    }
    RtDrawField(field);
    return TRUE;
}
static void RtUpdateField(void)
{
    u8 i;
    struct RtField *field;
    struct RtFighter *target;
    for (i=0;i<8;i++)
    {
        field=&sRt->field[i]; if (!field->active) continue;
        if (field->life != 0xFFFF) { field->life=RtDec(field->life,sRt->frameStep); if (!field->life) { RtRemoveField(field); continue; } }
        field->hitTimer=RtDec(field->hitTimer,sRt->frameStep);
        target=field->enemy ? &sRt->player : &sRt->enemy;
        if (field->kind != RTF_SUB && target->active && target->hp && !target->hidden
         && RtAbs(FROM_Q8(target->x)-field->x)<15 && RtAbs(FROM_Q8(target->y)-field->y)<15 && !field->hitTimer)
        {
            field->hitTimer=60;
            if (field->kind==RTF_WEB) { target->clocks[RTC_SLOW]=300; RtRemoveField(field); }
            else if (field->kind==RTF_SPIKES && target->ability != ABILITY_LEVITATE && !RtTypeIs(target,TYPE_FLYING)) RtSetHp(target,target->hp-(target->maxHp/8 ? target->maxHp/8 : 1));
        }
    }
}
static void RtCancelCast(struct RtFighter *f)
{
    u8 i;
    RtStopFlame(f);
    if(f->cast.active && RtRepeatingMove(f->cast.move) && !f->cast.landed) RtBreakSequence(f);
    for (i=0;i<8;i++) if (f->cast.fx[i] < RT_MAX_FX) RtDestroyFx(f->cast.fx[i]);
    memset(&f->cast,0,sizeof(f->cast));
    for (i=0;i<8;i++) f->cast.fx[i]=0xFF;
    f->hidden=FALSE;
    if (f->glowSprite != MAX_SPRITES) { DestroySprite(&gSprites[f->glowSprite]); f->glowSprite=MAX_SPRITES; FreeSpriteTilesByTag(0x77B0+f->isEnemy); }
}
static void RtChangeIcon(struct RtFighter *f, u16 species)
{
    u8 old=f->spriteId;
    RtReleasePal(f->palSlot);
    if (old != MAX_SPRITES) DestroySprite(&gSprites[old]);
    f->species=species;
    f->palSlot=RtAcquirePal(RT_PAL_KEY_DS(f->spriteKey,f->visualShiny),DsMonPalette(f->spriteKey,f->visualShiny));
    f->spriteId=DsCreateMonSprite(f->spriteKey,FROM_Q8(f->x),FROM_Q8(f->y),f->palSlot,0x40,SpriteCallbackDummy);
    if (f->spriteId != MAX_SPRITES) { gSprites[f->spriteId].oam.paletteNum=f->palSlot; gSprites[f->spriteId].oam.priority=2; }
}
static bool8 RtExtendedStatus(struct RtFighter *f, struct RtFighter *target, u16 move)
{
    u8 i,slot,pp; s32 vx,vy; u16 value; u32 clear=0;
    struct RtFighter *thief;
    switch (move)
    {
    case MOVE_MEAN_LOOK: target->noEscapeTimer=600; return TRUE;
    case MOVE_FOLLOW_ME: case MOVE_HELPING_HAND: return FALSE;
    case MOVE_COUNTER: f->clocks[RTC_COUNTER]=24; return TRUE;
    case MOVE_MIRROR_COAT: f->clocks[RTC_COAT]=24; return TRUE;
    case MOVE_BLOCK: f->clocks[RTC_BLOCK]=24; return TRUE;
    case MOVE_PROTECT: case MOVE_DETECT: f->clocks[RTC_PROTECT]=60; return TRUE;
    case MOVE_ENDURE: f->clocks[RTC_ENDURE]=120; return TRUE;
    case MOVE_GRUDGE: f->clocks[RTC_GRUDGE]=600; return TRUE;
    case MOVE_DESTINY_BOND: target->clocks[RTC_DESTINY]=600; target->perishOwner=RtSide(f); return TRUE;
    case MOVE_BIDE:
        value=0; for(i=0;i<8;i++) value+=f->recentDamage[i];
        memset(f->recentDamage,0,sizeof(f->recentDamage));
        f->bideDamage=value*2; f->clocks[RTC_BIDE]=60; return TRUE;
    case MOVE_MUD_SPORT: f->clocks[RTC_MUDSPORT]=900; return TRUE;
    case MOVE_WATER_SPORT: f->clocks[RTC_WATERSPORT]=900; return TRUE;
    case MOVE_REFLECT: f->clocks[RTC_REFLECT]=900; return TRUE;
    case MOVE_LIGHT_SCREEN: f->clocks[RTC_LIGHTSCREEN]=900; return TRUE;
    case MOVE_SAFEGUARD: f->clocks[RTC_SAFEGUARD]=900; return TRUE;
    case MOVE_MIST: f->clocks[RTC_MIST]=900; return TRUE;
    case MOVE_FOCUS_ENERGY: f->clocks[RTC_FOCUS]=1800; return TRUE;
    case MOVE_DEFENSE_CURL: f->defenseCurl=TRUE; RtStage(f,RT_STAT_DEF,1); return TRUE;
    case MOVE_MAGIC_COAT: f->clocks[RTC_MAGICCOAT]=90; return TRUE;
    case MOVE_SNATCH: f->clocks[RTC_SNATCH]=180; return TRUE;
    case MOVE_IMPRISON: f->clocks[RTC_IMPRISON]=600; return TRUE;
    case MOVE_RAIN_DANCE: RtSetBattleWeather(RTW_RAIN,900); return TRUE;
    case MOVE_SUNNY_DAY: RtSetBattleWeather(RTW_SUN,900); return TRUE;
    case MOVE_SANDSTORM: RtSetBattleWeather(RTW_SAND,900); return TRUE;
    case MOVE_HAIL: RtSetBattleWeather(RTW_HAIL,900); return TRUE;
    case MOVE_SUBSTITUTE:
        RtFaceVector(f,&vx,&vy);
        return RtPlaceField(f,RTF_SUB,FROM_Q8(f->x)+vx*24,FROM_Q8(f->y)+vy*24);
    case MOVE_TRANSFORM:
        if (f->transformed || target->transformed || target->hidden || RtSubstitute(target)) return FALSE;
        f->transformed=TRUE; f->types[0]=target->types[0]; f->types[1]=target->types[1]; f->ability=target->ability;
        f->spriteKey=target->spriteKey; f->visualShiny=target->visualShiny;
        for (i=0;i<5;i++) { static const u8 ids[]={MON_DATA_ATK,MON_DATA_DEF,MON_DATA_SPEED,MON_DATA_SPATK,MON_DATA_SPDEF}; f->copiedStats[i]=GetMonData(target->mon,ids[i]); }
        for(i=0;i<RT_STAT_COUNT;i++) f->stages[i]=target->stages[i];
        f->overrideMask=15;
        for(i=0;i<4;i++) { f->overrideMoves[i]=RtMove(target,i); f->overridePp[i]=f->overrideMaxPp[i]=gBattleMoves[f->overrideMoves[i]].pp<5 ? gBattleMoves[f->overrideMoves[i]].pp : 5; }
        RtChangeIcon(f,target->species); sRt->dirtyMoves=TRUE; sRt->dirtyPlayer=TRUE; sRt->dirtyEnemy=TRUE; return TRUE;
    case MOVE_ROLE_PLAY: if(target->ability==ABILITY_WONDER_GUARD || target->ability==ABILITY_NONE || target->ability==ABILITY_TRACE) return FALSE; f->ability=target->ability; return TRUE;
    case MOVE_SKILL_SWAP: if(f->ability==ABILITY_WONDER_GUARD || target->ability==ABILITY_WONDER_GUARD) return FALSE; i=f->ability; f->ability=target->ability; target->ability=i; return TRUE;
    case MOVE_TRICK: if(target->ability==ABILITY_STICKY_HOLD || f->item==ITEM_ENIGMA_BERRY || target->item==ITEM_ENIGMA_BERRY || (!f->item && !target->item)) return FALSE; value=f->item; RtSetHeldItem(f,target->item); RtSetHeldItem(target,value); f->choiceMove=target->choiceMove=0; return TRUE;
    case MOVE_RECYCLE: if(!f->item && f->recycledItem) { RtSetHeldItem(f,f->recycledItem); f->recycledItem=0; return TRUE; } return FALSE;
    case MOVE_PSYCH_UP: memcpy(f->stages,target->stages,sizeof(f->stages)); return TRUE;
    case MOVE_CONVERSION: f->types[0]=f->types[1]=RtMoveType(f,RtMove(f,0)); return TRUE;
    case MOVE_CONVERSION_2:
        for(i=0;i<32;i++) { u8 t=Random()%NUMBER_OF_MON_TYPES; if(t!=TYPE_MYSTERY) { f->types[0]=f->types[1]=t; if(RtMultiplier(gBattleMoves[target->lastMove].type,f)<100) return TRUE; } }
        return TRUE;
    case MOVE_CAMOUFLAGE: { static const u8 types[]={TYPE_GRASS,TYPE_GRASS,TYPE_GROUND,TYPE_WATER,TYPE_WATER,TYPE_WATER,TYPE_ROCK,TYPE_ROCK,TYPE_NORMAL,TYPE_NORMAL}; i=BattleSetup_GetTerrainId(); f->types[0]=f->types[1]=types[i<10?i:9]; return TRUE; }
    case MOVE_PERISH_SONG: target->clocks[RTC_PERISH]=1200; target->perishOwner=RtSide(f); return TRUE;
    case MOVE_CURSE:
        if(RtTypeIs(f,TYPE_GHOST)) { RtSetHp(f,f->hp-f->maxHp/2); target->clocks[RTC_CURSE]=1800; }
        else { RtStage(f,RT_STAT_ATK,1); RtStage(f,RT_STAT_DEF,1); RtStage(f,RT_STAT_SPEED,-1); }
        return TRUE;
    case MOVE_LEECH_SEED: if(RtTypeIs(target,TYPE_GRASS)) return FALSE; target->clocks[RTC_SEED]=1800; return TRUE;
    case MOVE_NIGHTMARE: if(!(target->status&STATUS1_SLEEP)) return FALSE; target->clocks[RTC_NIGHTMARE]=1800; return TRUE;
    case MOVE_WISH: f->clocks[RTC_WISH]=360; return TRUE;
    case MOVE_INGRAIN: f->clocks[RTC_INGRAIN]=1800; return TRUE;
    case MOVE_STOCKPILE: if(f->stockpile<3) { f->stockpile++; return TRUE; } return FALSE;
    case MOVE_SWALLOW: if(!f->stockpile) return FALSE; RtSetHp(f,f->hp+f->maxHp*f->stockpile/4); f->stockpile=0; return TRUE;
    case MOVE_PAIN_SPLIT: value=(f->hp+target->hp)/2; RtSetHp(f,value); RtSetHp(target,value); return TRUE;
    case MOVE_BELLY_DRUM: if(f->hp<=f->maxHp/2) return FALSE; RtSetHp(f,f->hp-f->maxHp/2); f->stages[RT_STAT_ATK]=6; return TRUE;
    case MOVE_TAUNT: target->clocks[RTC_TAUNT]=360; return TRUE;
    case MOVE_DISABLE: target->disabledMove=target->lastMove; target->clocks[RTC_DISABLE]=480; return target->lastMove!=0;
    case MOVE_ENCORE: target->encoredMove=target->lastMove; target->clocks[RTC_ENCORE]=360; return target->lastMove!=0;
    case MOVE_TORMENT: target->clocks[RTC_TORMENT]=480; return TRUE;
    case MOVE_ATTRACT: if(target->clocks[RTC_ATTRACT] || !RtAttractEligible(f,target)) return FALSE; target->clocks[RTC_ATTRACT]=600; target->attractParty=f->partyIndex; return TRUE;
    case MOVE_FORESIGHT: case MOVE_ODOR_SLEUTH: target->clocks[RTC_FORESIGHT]=1800; target->stages[RT_STAT_EVASION]=0; return TRUE;
    case MOVE_CHARGE: f->clocks[RTC_CHARGE]=360; return TRUE;
    case MOVE_LOCK_ON: case MOVE_MIND_READER: f->clocks[RTC_LOCKON]=180; return TRUE;
    case MOVE_SPITE:
        for(i=0;i<4;i++) if(RtMove(target,i)==target->lastMove) { pp=RtPp(target,i); RtSetPp(target,i,pp>4?pp-4:0); return TRUE; }
        return FALSE;
    case MOVE_BATON_PASS:
        if(!f->isEnemy)
        {
            for(i=0;i<gPlayerPartyCount;i++) if(i!=f->partyIndex && RtIsUsableMon(&gPlayerParty[i]))
            { memcpy(sRt->batonStages,f->stages,sizeof(f->stages)); sRt->batonPass=TRUE; RtOpenPicker(); return TRUE; }
        }
        else for(i=0;i<gEnemyPartyCount;i++) if(i!=f->partyIndex && RtIsUsableMon(&gEnemyParty[i])) { RtForceEnemySwitch(i,TRUE); return TRUE; }
        return FALSE;
    case MOVE_TELEPORT: return TRUE;
    case MOVE_MEMENTO: RtStage(target,RT_STAT_ATK,-2); RtStage(target,RT_STAT_SPATK,-2); RtSetHp(f,0); return TRUE;
    case MOVE_ROAR: case MOVE_WHIRLWIND:
        if(target->ability==ABILITY_SUCTION_CUPS || target->clocks[RTC_INGRAIN]) return FALSE;
        if(!sRt->isTrainer) { sRt->outcome=RT_OUTCOME_RAN; gBattleOutcome=B_OUTCOME_RAN; RtStartMessages(RT_STATE_END); return TRUE; }
        if(!target->isEnemy) { for(i=0;i<gPlayerPartyCount;i++) if(i!=target->partyIndex && RtIsUsableMon(&gPlayerParty[i])) { RtSwitchPlayer(i); return TRUE; } }
        else for(i=0;i<gEnemyPartyCount;i++) if(i!=target->partyIndex && RtIsUsableMon(&gEnemyParty[i])) { RtForceEnemySwitch(i,FALSE); return TRUE; }
        return FALSE;
    case MOVE_SPLASH: return TRUE;
    }
    return FALSE;
}
