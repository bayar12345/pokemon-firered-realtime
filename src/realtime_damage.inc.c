// One common impact path for melee, finite projectiles and cast geometry.
static bool8 RtMoveAllowed(struct RtFighter *f, u16 move)
{
    struct RtFighter *other=RtOther(f); u8 i;
    if (RtPsychicHeld(f) || !move || move>=MOVES_COUNT || (RtProfile(move)->flags&RTMF_DISABLED) || f->clocks[RTC_FLINCH]) return FALSE;
    if (f->clocks[RTC_TAUNT] && !RtDamaging(move)) return FALSE;
    if (f->clocks[RTC_DISABLE] && move==f->disabledMove) return FALSE;
    if (f->clocks[RTC_ENCORE] && move!=f->encoredMove && move!=MOVE_STRUGGLE) return FALSE;
    if (f->clocks[RTC_TORMENT] && move==f->lastMove) return FALSE;
    if (other->clocks[RTC_IMPRISON] && RtLength(FROM_Q8(other->x-f->x),FROM_Q8(other->y-f->y))<=72)
        for(i=0;i<4;i++) if (RtMove(other,i)==move) return FALSE;
    if (move==MOVE_DREAM_EATER && !(other->status&STATUS1_SLEEP)) return FALSE;
    if (move==MOVE_SNORE && !(f->status&STATUS1_SLEEP)) return FALSE;
    if (move==MOVE_FAKE_OUT && f->actionsUsed) return FALSE;
    if (move==MOVE_SPIT_UP && !f->stockpile) return FALSE;
    if (ItemId_GetHoldEffect(f->item)==HOLD_EFFECT_CHOICE_BAND && f->choiceMove && move!=f->choiceMove && move!=MOVE_STRUGGLE) return FALSE;
    return TRUE;
}
static void RtApplyImpact(struct RtFighter *f, struct RtFighter *target, u16 move, u16 fixed)
{
    s32 damage,kx,ky; u16 mult; bool8 crit=FALSE,special=RtSpecial(move,RtMoveType(f,move));
    u8 effect=gBattleMoves[move].effect,i; u16 oldHp;
    if (!target->active || !target->hp || !RtCanHitHidden(target,move) || !f->hp) return;
    if (target->clocks[RTC_PROTECT] || (!special && target->clocks[RTC_BLOCK]))
    { RtSpawnFx(FX_SPARKLE,FROM_Q8(target->x),FROM_Q8(target->y),12,FALSE); return; }
    if (RtSoundMove(move) && target->ability==ABILITY_SOUNDPROOF) return;
    if(!fixed)
    {
        bool8 accurate;
        if(f->cast.active && move==MOVE_PSYCHIC && f->cast.move==move && f->cast.accuracyChecked)
            accurate=f->cast.accuracyPassed;
        else if(f->cast.active && f->cast.move==move && f->cast.hits>1 && f->cast.kind!=RT_BEAM && move!=MOVE_TRIPLE_KICK)
        {
            if(!f->cast.accuracyChecked) { f->cast.accuracyChecked=TRUE; f->cast.accuracyPassed=RtRollAccuracy(f,target,move); }
            accurate=f->cast.accuracyPassed;
        }
        else accurate=RtRollAccuracy(f,target,move);
        if(!accurate)
        {
            if(RtPtdHasArt(move) || sRtPtdStatusArt[move].anim){s16 x,y;RtPtdBody(target,&x,&y);if(RtPtdHasArt(move))RtPtdMiss(move,x,y);else RtPtdStatusAt(move,x,y);}
            if(move==MOVE_TRIPLE_KICK && f->cast.active && f->cast.move==move) f->cast.hits=f->cast.hitCount;
            return;
        }
    }
    if (!RtDamaging(move) && !fixed)
    {
        if (RtSubInLine(f,target) && RtProfile(move)->kind!=RT_SELF && move!=MOVE_PERISH_SONG) return;
        RtApplyStatusMove(f,target,move); return;
    }
    damage=fixed ? fixed : RtCalcDamage(f,target,move,&mult,&crit);
    if(fixed) mult=100;
    if(f->cast.active && f->cast.kind==RT_BEAM && !fixed) damage=(damage+2)/3;
    if (!damage || !mult)
    { if(RtPtdHasArt(move)){s16 x,y;RtPtdBody(target,&x,&y);RtPtdMiss(move,x,y);} return; }
    if (RtSubInLine(f,target))
    { struct RtField *sub=RtSubstitute(target); RtHitSubstitute(f,sub->x,sub->y,damage); RtRegisterHit(f,move); return; }
    oldHp=target->hp; if(damage>oldHp) damage=oldHp;
    if ((effect==EFFECT_FALSE_SWIPE || target->clocks[RTC_ENDURE]) && damage>=oldHp) damage=oldHp-1;
    if(damage>=oldHp && ItemId_GetHoldEffect(target->item)==HOLD_EFFECT_FOCUS_BAND && Random()%100<ItemId_GetHoldEffectParam(target->item)) damage=oldHp-1;
    RtSetHp(target,target->hp-damage);
    if(damage) RtRegisterHit(f,move);
    if(damage && target->rage) RtStage(target,RT_STAT_ATK,1);
    if(damage && target->cast.active && target->cast.move==MOVE_FOCUS_PUNCH && target->cast.elapsed<RtProfile(MOVE_FOCUS_PUNCH)->startup) RtCancelCast(target);
    if(!target->isEnemy) gBattleResults.playerMonWasDamaged=TRUE;
    target->recentDamage[target->historySlot]+=damage;
    if(!RtPtdImpact(f,target,move)) RtSpawnFx(RtProfile(move)->fx,FROM_Q8(target->x),FROM_Q8(target->y),12,FALSE);
    target->hurtTimer=10;
    if (RtProfile(move)->flags&RTMF_ROOT) target->clocks[RTC_ROOT]=90;
    if (move==MOVE_BIND || move==MOVE_WRAP || effect==EFFECT_TRAP) target->clocks[RTC_TRAP]=720;
    if (effect==EFFECT_RAPID_SPIN)
    {
        f->clocks[RTC_TRAP]=f->clocks[RTC_ROOT]=f->clocks[RTC_SEED]=0;
        for(i=0;i<8;i++) if(sRt->field[i].active && sRt->field[i].enemy!=f->isEnemy && sRt->field[i].kind!=RTF_SUB) RtRemoveField(&sRt->field[i]);
    }
    if(move==MOVE_BRICK_BREAK) target->clocks[RTC_REFLECT]=target->clocks[RTC_LIGHTSCREEN]=0;
    if(move==MOVE_SMELLING_SALT && target->status&STATUS1_PARALYSIS) RtSetStatus(target,0);
    if(!RtPtdHasArt(move) && (move==MOVE_FIRE_PUNCH || move==MOVE_ICE_PUNCH || move==MOVE_THUNDER_PUNCH)) RtSpawnFx(move==MOVE_FIRE_PUNCH?FX_EMBER:move==MOVE_ICE_PUNCH?FX_ICE:FX_SPARK,FROM_Q8(target->x),FROM_Q8(target->y),18,FALSE);
    RtSecondaryStatus(f,target,move);
    if (target->ability!=ABILITY_SHIELD_DUST && Random()%100 < gBattleMoves[move].secondaryEffectChance*(f->ability==ABILITY_SERENE_GRACE?2:1))
    {
        switch(effect) {
        case EFFECT_FLINCH_HIT: case EFFECT_TWISTER: case EFFECT_SNORE: case EFFECT_FAKE_OUT:
            if(target->ability!=ABILITY_INNER_FOCUS) target->clocks[RTC_FLINCH]=30; break;
        case EFFECT_ATTACK_DOWN_HIT: RtStage(target,RT_STAT_ATK,-1); break;
        case EFFECT_DEFENSE_DOWN_HIT: RtStage(target,RT_STAT_DEF,-1); break;
        case EFFECT_SPEED_DOWN_HIT: RtStage(target,RT_STAT_SPEED,-1); break;
        case EFFECT_SPECIAL_ATTACK_DOWN_HIT: RtStage(target,RT_STAT_SPATK,-1); break;
        case EFFECT_SPECIAL_DEFENSE_DOWN_HIT: RtStage(target,RT_STAT_SPDEF,-1); break;
        case EFFECT_ACCURACY_DOWN_HIT: RtStage(target,RT_STAT_ACC,-1); break;
        case EFFECT_ALL_STATS_UP_HIT: for(i=0;i<5;i++) RtStage(f,i,1); break;
        case EFFECT_DEFENSE_UP_HIT: RtStage(f,RT_STAT_DEF,1); break;
        case EFFECT_ATTACK_UP_HIT: RtStage(f,RT_STAT_ATK,1); break;
        }
    }
    if (effect==EFFECT_SUPERPOWER) { RtStage(f,RT_STAT_ATK,-1); RtStage(f,RT_STAT_DEF,-1); }
    if (effect==EFFECT_OVERHEAT) { f->stages[RT_STAT_SPATK]-=2; if(f->stages[RT_STAT_SPATK]<-6) f->stages[RT_STAT_SPATK]=-6; }
    if (effect==EFFECT_ABSORB || effect==EFFECT_DREAM_EATER) RtSetHp(f,f->hp+(damage/2?damage/2:1));
    if ((effect==EFFECT_RECOIL && f->ability!=ABILITY_ROCK_HEAD) || move==MOVE_STRUGGLE) RtSetHp(f,f->hp-(damage/4?damage/4:1));
    if (effect==EFFECT_DOUBLE_EDGE && f->ability!=ABILITY_ROCK_HEAD) RtSetHp(f,f->hp-(damage/3?damage/3:1));
    if(effect==EFFECT_PAY_DAY && !f->isEnemy) sRt->payday+=f->level*5;
    if(effect==EFFECT_THIEF && !f->item && target->item && target->ability!=ABILITY_STICKY_HOLD && target->item!=ITEM_ENIGMA_BERRY) { RtSetHeldItem(f,target->item); RtSetHeldItem(target,0); }
    if(effect==EFFECT_KNOCK_OFF && target->item && target->ability!=ABILITY_STICKY_HOLD) RtSetHeldItem(target,0);
    if(ItemId_GetHoldEffect(f->item)==HOLD_EFFECT_SHELL_BELL && damage) RtSetHp(f,f->hp+(damage/8?damage/8:1));
    if ((gBattleMoves[move].flags&FLAG_MAKES_CONTACT) && target->ability==ABILITY_ROUGH_SKIN) RtSetHp(f,f->hp-(f->maxHp/16?f->maxHp/16:1));
    if(target->windup) { target->windup=0; if(target->cooldown<30) target->cooldown=30; }
    if(target->cast.active && target->cast.move==MOVE_SWORDS_DANCE) RtCancelCast(target);
    RtNormalize(target->x-f->x,target->y-f->y,Q8(1),&kx,&ky);
    target->kbX=kx; target->kbY=ky;
    if (target->clocks[RTC_COUNTER] && !special && RtLength(FROM_Q8(f->x-target->x),FROM_Q8(f->y-target->y))<=32)
    { target->clocks[RTC_COUNTER]=0; RtSetHp(f,f->hp-damage*2); if(!RtPtdImpact(target,f,MOVE_COUNTER))RtSpawnFx(FX_FIST,FROM_Q8(f->x),FROM_Q8(f->y),12,FALSE); }
    if (target->clocks[RTC_COAT] && special)
    { target->clocks[RTC_COAT]=0; RtFixedShot(target,f,MOVE_MIRROR_COAT,damage*2); }
    if (!target->hp)
    {
        if(target->clocks[RTC_GRUDGE]) for(i=0;i<4;i++) if(RtMove(f,i)==move) RtSetPp(f,i,0);
        if(f->clocks[RTC_DESTINY] && f->perishOwner==RtSide(target)) RtSetHp(f,0);
    }
    RtTryBerry(target); RtTryBerry(f);
    PlaySE(mult>100?SE_SUPER_EFFECTIVE:mult<100?SE_NOT_EFFECTIVE:SE_EFFECTIVE);
}
static void RtUpdateClocks(struct RtFighter *f)
{
    u8 i; u16 old,dt=sRt->frameStep,damage; struct RtFighter *other=RtOther(f);
    if(!f->hp) return;
    for(i=0;i<RTC_COUNT;i++)
    {
        old=f->clocks[i]; if(!old) continue;
        if(i==RTC_ATTRACT && (!other->hp || other->partyIndex!=f->attractParty)) { f->clocks[i]=0; continue; }
        if(i==RTC_PERISH && !(f->perishOwner ? sRt->enemy.hp : sRt->player.hp)) { f->clocks[i]=0; continue; }
        f->clocks[i]=RtDec(old,dt);
        if(!f->clocks[i])
        {
            if(i==RTC_PERISH) RtSetHp(f,0);
            if(i==RTC_WISH && f->hp) RtSetHp(f,f->hp+f->maxHp/2);
            if(i==RTC_BIDE && f->bideDamage) { RtFixedShot(f,other,MOVE_BIDE,f->bideDamage); f->bideDamage=0; }
            if(i==RTC_RAMPAGE) RtInflictConfusion(f);
        }
    }
    f->noEscapeTimer=RtDec(f->noEscapeTimer,dt);
    f->historyClock+=dt;
    while(f->historyClock>=60) { f->historyClock-=60; f->historySlot=(f->historySlot+1)%8; f->recentDamage[f->historySlot]=0; }
    f->residualClock=RtDec(f->residualClock,dt);
    if(!f->residualClock && f->hp)
    {
        f->residualClock=180;
        if(f->clocks[RTC_SEED]) { damage=f->maxHp/8?f->maxHp/8:1; RtSetHp(f,f->hp-damage); if(other->hp) RtSetHp(other,other->hp+damage); }
        if(f->clocks[RTC_CURSE] || (f->clocks[RTC_NIGHTMARE] && f->status&STATUS1_SLEEP)) RtSetHp(f,f->hp-(f->maxHp/4?f->maxHp/4:1));
        if(f->hp && (f->clocks[RTC_INGRAIN] || ItemId_GetHoldEffect(f->item)==HOLD_EFFECT_LEFTOVERS)) RtSetHp(f,f->hp+(f->maxHp/16?f->maxHp/16:1));
        if(f->clocks[RTC_TRAP]) RtSetHp(f,f->hp-(f->maxHp/16?f->maxHp/16:1));
        if(f->hp && f->ability==ABILITY_RAIN_DISH && sRt->weather==RTW_RAIN && RtWeatherWorks()) RtSetHp(f,f->hp+(f->maxHp/16?f->maxHp/16:1));
        if(f->ability==ABILITY_SHED_SKIN && Random()%3==0) RtSetStatus(f,0);
    }
    RtTryBerry(f);
}
