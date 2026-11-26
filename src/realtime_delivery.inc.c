static void RtAim(struct RtFighter *f, struct RtFighter *target, s32 *dx, s32 *dy)
{
    s32 x,y,fx,fy,dot,cross;
    RtFaceVector(f,&fx,&fy); x=target->x-f->x; y=target->y-f->y;
    dot=x*fx+y*fy; cross=RtAbs(x*fy-y*fx);
    if(fx && fy) cross=cross*181/256;  // diagonal facing vectors are longer
    if(f->isEnemy || (dot>0 && cross*4<dot*7)) RtNormalize(x,y,256,dx,dy);
    else RtNormalize(fx,fy,256,dx,dy);
}
static u16 RtCalledMove(struct RtFighter *f, struct RtFighter *target, u16 move)
{
    u16 called=0, choices[MOVES_COUNT]; u16 n=0, j; u8 i,slot;
    switch(move)
    {
    case MOVE_METRONOME:
        for(j=1;j<MOVES_COUNT;j++) if(RtCallAllowed(move,j)) choices[n++]=j;
        return n?choices[Random()%n]:0;
    case MOVE_SLEEP_TALK:
        if(!(f->status&STATUS1_SLEEP)) return 0;
        for(i=0;i<4;i++) { called=RtMove(f,i); if(RtCallAllowed(move,called) && RtMoveAllowed(f,called)) choices[n++]=called; }
        return n?choices[Random()%n]:0;
    case MOVE_MIMIC: case MOVE_SKETCH:
        called=target->lastMove;
        if(!RtCallAllowed(move,called) || (move==MOVE_SKETCH && f->transformed)) return 0;
        for(i=0;i<4;i++) if(RtMove(f,i)==called) return 0;
        for(i=0;i<4;i++) if(RtMove(f,i)==move)
        {
            if(move==MOVE_SKETCH && !f->transformed)
            { RemoveMonPPBonus(f->mon,i); SetMonMoveSlot(f->mon,called,i); sRt->cacheMon[f->isEnemy?1:0]=NULL; sRt->dirtyMoves=TRUE; }
            else { f->overrideMask|=1<<i; f->overrideMoves[i]=called; f->overridePp[i]=f->overrideMaxPp[i]=gBattleMoves[called].pp; sRt->dirtyMoves=TRUE; }
            return 0;
        } return 0;
    case MOVE_MIRROR_MOVE: called=target->lastMove; if(!RtCallAllowed(move,called)) return 0; break;
    case MOVE_ASSIST:
        { struct Pokemon *party=f->isEnemy?gEnemyParty:gPlayerParty;
          for(i=0;i<PARTY_SIZE;i++) { if(i==f->partyIndex || !GetMonData(&party[i],MON_DATA_SPECIES) || GetMonData(&party[i],MON_DATA_IS_EGG)) continue;
            for(slot=0;slot<4;slot++) { called=GetMonData(&party[i],MON_DATA_MOVE1+slot); if(RtCallAllowed(move,called)) choices[n++]=called; } } }
        return n?choices[Random()%n]:0;
    case MOVE_NATURE_POWER:
        { static const u16 moves[]={MOVE_STUN_SPORE,MOVE_RAZOR_LEAF,MOVE_EARTHQUAKE,MOVE_HYDRO_PUMP,MOVE_SURF,MOVE_BUBBLE_BEAM,MOVE_ROCK_SLIDE,MOVE_SHADOW_BALL,MOVE_SWIFT,MOVE_SWIFT}; u8 terrain=BattleSetup_GetTerrainId(); called=moves[terrain<10?terrain:9]; } break;
    }
    if(!called || called>=MOVES_COUNT) return 0;
    return called;
}
static void RtLaunchCast(struct RtFighter *f, struct RtFighter *target, u16 move)
{
    struct RtCast *c=&f->cast; const struct RtMoveDef *d=RtProfile(move); u8 i;
    RtCancelCast(f);
    if(d->kind==RT_CALL)RtPtdStatusFx(f,target,move,FALSE);
    if(d->flags&RTMF_DISABLED) { RtShowTransient(sText_NothingHappened,40); return; }
    for(i=0;d->kind==RT_CALL && i<4;i++)
    { move=RtCalledMove(f,target,move); if(!move) return; d=RtProfile(move); }
    if(d->kind==RT_CALL || !RtMoveAllowed(f,move)) return;
    if(!RtRepeatingMove(move) || f->sequenceMove!=move || (move!=MOVE_FURY_CUTTER && f->rollout>=5)) RtBreakSequence(f);
    if(RtRepeatingMove(move)) f->sequenceMove=move;
    f->rage=move==MOVE_RAGE;
    c->active=TRUE; c->move=move; c->kind=d->kind;
    if(move==MOVE_MAGNITUDE) c->power=RtMagnitudePower(Random()%100);
    if(move==MOVE_CURSE && !RtTypeIs(f,TYPE_GHOST)) c->kind=RT_SELF;
    c->originX=f->x; c->originY=f->y; c->targetX=target->x; c->targetY=target->y;
    if((d->kind==RT_POINT || d->kind==RT_AIR) && RtLength(FROM_Q8(target->x-f->x),FROM_Q8(target->y-f->y))>d->reach)
    { s32 xx,yy; RtNormalize(target->x-f->x,target->y-f->y,Q8(d->reach),&xx,&yy); c->targetX=f->x+xx; c->targetY=f->y+yy; }
    c->nextHit=d->startup;
    if(move==MOVE_SOLAR_BEAM && sRt->weather==RTW_SUN && RtWeatherWorks())c->nextHit/=2;
    c->end=c->nextHit+d->active;
    if(d->kind==RT_BEAM && RtPtdHasArt(move))c->nextHit+=6;
    c->hits=1;
    if(d->kind==RT_BEAM) c->hits=3;
    if(d->flags&RTMF_MULTI) { u8 roll=Random()%8; c->hits=roll<3?2:roll<6?3:roll==6?4:5; }
    if(move==MOVE_DOUBLE_KICK || move==MOVE_TWINEEDLE || move==MOVE_DOUBLE_SLAP) c->hits=2;
    if(move==MOVE_TRIPLE_KICK) c->hits=3;
    if(d->kind==RT_PARTY) c->hits=PARTY_SIZE;
    if(c->hits>1 && d->kind!=RT_BEAM) c->end=c->nextHit+c->hits*(d->kind==RT_PARTY?18:10)+6;
    if(f->clocks[RTC_CHARGE] && RtDamaging(move) && RtMoveType(f,move)==TYPE_ELECTRIC) { c->phase=1; f->clocks[RTC_CHARGE]=0; }
    RtAim(f,target,&c->dx,&c->dy);
    if(RtDamaging(move))f->dir=RtDirFromVector(c->dx,c->dy);
    if(d->kind==RT_AIR) { f->hidden=TRUE; c->end=c->nextHit+24; }
    if((move==MOVE_THRASH || move==MOVE_PETAL_DANCE || move==MOVE_UPROAR) && !f->clocks[RTC_RAMPAGE]) f->clocks[RTC_RAMPAGE]=480;
    if(d->kind==RT_DASH) f->visualGlow=10;
    if(move==MOVE_SWORDS_DANCE) f->visualGlow=0;
    for(i=0;i<8;i++) c->fx[i]=0xFF;
    if(move==MOVE_PSYCHIC) c->fx[0]=RtSpawnFx(FX_PSY_MARK,FROM_Q8(c->targetX),FROM_Q8(c->targetY)+8,0xFF,TRUE);
    PlaySE(d->kind==RT_CLOSE || d->kind==RT_DASH?SE_M_SCRATCH:SE_M_SWIFT);
}
static bool8 RtInCast(struct RtFighter *f, struct RtFighter *target)
{
    struct RtCast *c=&f->cast; const struct RtMoveDef *d=RtProfile(c->move);
    s32 x=FROM_Q8(target->x-f->x),y=FROM_Q8(target->y-f->y),dist=RtLength(x,y),dot,cross;
    if(!target->hp || !RtCanHitHidden(target,c->move)) return FALSE;
    if(c->kind==RT_POINT || c->kind==RT_AIR) return RtLength(FROM_Q8(target->x-c->targetX),FROM_Q8(target->y-c->targetY))<=d->radius;
    if(c->kind==RT_DASH) return dist<22 && (!sRt->arena || RtArenaLineClear(FROM_Q8(f->x),FROM_Q8(f->y),FROM_Q8(target->x),FROM_Q8(target->y)));
    if(c->kind==RT_AREA) return dist<=d->reach;
    if(c->kind==RT_PARTY) return dist<=d->reach;
    dot=(x*c->dx+y*c->dy)/256; cross=RtAbs(x*c->dy-y*c->dx)/256;
    if((d->flags&RTMF_ALWAYS) && dist<=d->reach) return TRUE;
    if(dist>d->reach+4 || dot<0) return FALSE;
    if(c->kind==RT_BEAM) return cross<d->radius+10;
    if(c->kind==RT_CONE) return cross<dot/2+12;
    return cross<22;
}
static void RtCastFx(struct RtFighter *f)
{
    struct RtCast *c=&f->cast; const struct RtMoveDef *d=RtProfile(c->move);
    s16 x=FROM_Q8(f->x),y=FROM_Q8(f->y),xx,yy; u8 i,n,fx=d->fx;
    if(sRtPtdMoveArt[c->move].beam)
    { RtDrawFlame(f); return; }
    if(RtPtdCastFx(f))return;
    if(c->kind==RT_BEAM)
    {
        n=6;
        for(i=0;i<n;i++)
        {
            xx=x+c->dx*(i+1)*d->reach/(256*n); yy=y+c->dy*(i+1)*d->reach/(256*n);
            if(!RtAiLineClear(x,y,xx,yy) || RtFieldBlocks(f,xx,yy)) break;
            if(c->fx[i]==0xFF) c->fx[i]=RtSpawnFx(fx,xx,yy,0xFF,TRUE);
            else if(c->fx[i]<RT_MAX_FX && sRt->fx[c->fx[i]].active)
            { struct Sprite *s=&gSprites[sRt->fx[c->fx[i]].spriteId]; s->x=xx; s->y=yy; }
        }
        for(;i<6;i++) { RtDestroyFx(c->fx[i]); c->fx[i]=0xFF; }
    }
    else if(c->move==MOVE_VINE_WHIP)
    {
        if(c->elapsed>=d->startup && c->elapsed%3==0)
            for(i=1;i<=3;i++)
            {
                s16 side=6+(c->elapsed-d->startup)%6;
                xx=x+c->dx*i*10/256; yy=y+c->dy*i*10/256;
                RtSpawnFx(FX_ROOT,xx-c->dy*side/256,yy+c->dx*side/256,4,FALSE);
                RtSpawnFx(FX_ROOT,xx+c->dy*side/256,yy-c->dx*side/256,4,FALSE);
            }
    }
    else if(c->move==MOVE_SWORDS_DANCE && !sRtPtdStatusArt[c->move].anim)
    {
        i=(c->elapsed/6)%4;
        if(!(c->elapsed%6)) RtSpawnFx(FX_DRAGON,x+((i==0)?18:(i==2)?-18:0),y+((i==1)?14:(i==3)?-14:0),12,FALSE);
    }
    else if(c->kind==RT_POINT || c->kind==RT_AIR)
    {
        if(c->elapsed<c->nextHit)
        {
            if(!(c->elapsed%12)) RtSpawnFx(c->move==MOVE_THUNDER?FX_SMOKE:FX_SPARKLE,FROM_Q8(c->targetX),FROM_Q8(c->targetY)-(c->move==MOVE_THUNDER?24:0),10,FALSE);
        }
    }
}
static void RtUpdateCast(struct RtFighter *f, struct RtFighter *target)
{
    struct RtCast *c=&f->cast; const struct RtMoveDef *d; u16 dt=sRt->frameStep; s32 vx,vy; u8 i;
    if(!c->active)
    {
        if(f->clocks[RTC_RAMPAGE] && f->hp && target->hp && RtCanMove(f))
        {
            RtFace(f,target); RtNormalize(target->x-f->x,target->y-f->y,RtGetMoveSpeed(f),&vx,&vy);
            if(RtLength(FROM_Q8(target->x-f->x),FROM_Q8(target->y-f->y))>24) RtMoveFighter(f,vx,vy);
            else if(!f->cooldown) RtUseMove(f,target,f->lastMove);
        }
        return;
    }
    if(!f->hp) { RtCancelCast(f); return; }
    d=RtProfile(c->move); c->elapsed+=dt;
    if(c->move==MOVE_PSYCHIC) { RtUpdatePsychic(f,target); return; }
    if(c->kind==RT_DASH && c->elapsed>=d->startup && c->elapsed<c->end)
    {
        for(i=0;i<dt*4;i++) { if(RtCanMove(f)) RtMoveFighter(f,c->dx*d->reach/(d->active*4),c->dy*d->reach/(d->active*4)); if(!c->hit && RtInCast(f,target)) { c->hit=TRUE; RtApplyImpact(f,target,c->move,0); } }
    }
    if(c->kind==RT_AIR && c->elapsed>=c->nextHit)
    {
        // The landing point was committed at takeoff; never chase the target.
        if(RtIsSpotWalkable(FROM_Q8(c->targetX),FROM_Q8(c->targetY))) { f->x=c->targetX; f->y=c->targetY; }
        f->hidden=FALSE;
    }
    RtCastFx(f);
    if(c->elapsed>=c->nextHit && c->hitCount<c->hits)
    {
        if(sRt->arena && !c->coverHit && RtDamaging(c->move) && c->kind!=RT_SHOT && c->kind!=RT_SELF && c->kind!=RT_CALL)
        {
            s32 ox=FROM_Q8(f->x),oy=FROM_Q8(f->y);
            RtArenaHitCover(ox,oy,ox+c->dx*d->reach/256,oy+c->dy*d->reach/256,c->move); c->coverHit=TRUE;
        }
        c->hitCount++; c->nextHit+=(c->kind==RT_BEAM?d->active/3:c->kind==RT_PARTY?18:10);
        if(c->kind==RT_SELF) RtApplyStatusMove(f,target,c->move);
        else if(c->kind==RT_SHOT) RtFireProjectile(f,target,c->move);
        else if(c->kind==RT_BLINK)
        {
            for(i=48;i>=16;i-=8) if(RtIsSpotWalkable(FROM_Q8(f->x)+c->dx*i/256,FROM_Q8(f->y)+c->dy*i/256) && !RtFieldBlocks(f,FROM_Q8(f->x)+c->dx*i/256,FROM_Q8(f->y)+c->dy*i/256))
            { RtSpawnFx(FX_PSYCHIC,FROM_Q8(f->x),FROM_Q8(f->y),12,FALSE); f->x+=c->dx*i; f->y+=c->dy*i; RtSpawnFx(FX_PSYCHIC,FROM_Q8(f->x),FROM_Q8(f->y),12,FALSE); break; }
        }
        else if(c->kind==RT_FIELD)
        {
            RtFireProjectile(f,target,c->move);
        }
        else if(c->kind!=RT_DASH)
        {
            s16 x=FROM_Q8(f->x)+c->dx*20/256,y=FROM_Q8(f->y)+c->dy*20/256;
            if(c->kind==RT_POINT || c->kind==RT_AIR) { x=FROM_Q8(c->targetX); y=FROM_Q8(c->targetY); }
            if(c->kind==RT_AREA && !RtPtdHasArt(c->move) && !sRtPtdStatusArt[c->move].anim)
                for(i=0;i<8;i++) RtSpawnFx(d->fx,FROM_Q8(f->x)+(i%3-1)*d->reach/2,FROM_Q8(f->y)+(i/3-1)*d->reach/2,16,FALSE);
            else if(!RtPtdHasArt(c->move) && !sRtPtdStatusArt[c->move].anim) RtSpawnFx(d->fx,x,y,12,FALSE);
            if(c->kind==RT_PARTY)
            {
                struct Pokemon *party=f->isEnemy?gEnemyParty:gPlayerParty;
                if(!RtIsUsableMon(&party[c->hitCount-1]) || GetMonData(&party[c->hitCount-1],MON_DATA_STATUS)) return;
                RtPartyStrike(f,&party[c->hitCount-1],x,y);
            }
            if(RtInCast(f,target) && ((c->kind==RT_BEAM || c->kind==RT_CONE)
                ? RtPtdRayClear(FROM_Q8(f->x),FROM_Q8(f->y),FROM_Q8(target->x),FROM_Q8(target->y))
                : (!sRt->arena || RtAiLineClear(FROM_Q8(f->x),FROM_Q8(f->y),FROM_Q8(target->x),FROM_Q8(target->y))))) RtApplyImpact(f,target,c->move,0);
            else
            {
                if(RtPtdHasArt(c->move) && !sRtPtdMoveArt[c->move].beam && !sRtPtdMoveArt[c->move].cast)
                    RtPtdMiss(c->move,x,y+(sRt->arena?-24:-12));
                if(c->move==MOVE_TRIPLE_KICK)c->hits=c->hitCount;
            }
        }
        if(c->move==MOVE_EXPLOSION || c->move==MOVE_SELF_DESTRUCT) RtSetHp(f,0);
    }
    if(c->elapsed>=c->end)
    {
        if(c->kind==RT_DASH) RtSpawnFx(FX_SPARKLE,FROM_Q8(f->x),FROM_Q8(f->y),8,FALSE);
        if(c->move==MOVE_SPIT_UP) f->stockpile=0;
        RtCancelCast(f);
    }
}
