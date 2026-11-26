// PTD status artwork is separate from damaging-move art and changes no clocks,
// hit checks, stat values or damage. Follow the actual recipient after reflection.
static u8 RtPtdFollow(u16 anim,struct RtFighter *f,u8 life,bool8 loop,u8 kind,s16 offsetX,s16 offsetY)
{
    u8 i,id; s16 x,y;
    if(!anim || !f->active)return 0xFF;
    for(i=0;i<RT_MAX_FX;i++)if(sRt->fx[i].active && sRt->fx[i].ptdAnim==anim
        && sRt->fx[i].followSide==(f->isEnemy?2:1) && sRt->fx[i].followParty==f->partyIndex
        && sRt->fx[i].kind==kind)return i;
    RtPtdBody(f,&x,&y);id=RtPtdSpawn(anim,x+offsetX,y+offsetY,life,loop);
    if(id<RT_MAX_FX)
    {
        struct RtFx *fx=&sRt->fx[id];fx->followSide=f->isEnemy?2:1;fx->followParty=f->partyIndex;
        fx->kind=kind;fx->offsetX=offsetX;fx->offsetY=offsetY;
    }
    return id;
}

static void RtPtdStatusFx(struct RtFighter *atk,struct RtFighter *def,u16 move,bool8 applied)
{
    const struct RtPtdStatusArt *art=&sRtPtdStatusArt[move];
    struct RtFighter *f;u8 i,id,clock;
    if(!art->anim)return;
    for(i=0;i<(art->recipient==2?2:1);i++)
    {
        f=art->recipient==2?(i?def:atk):(art->recipient?def:atk);
        if(move==MOVE_CURSE && RtTypeIs(atk,TYPE_GHOST))f=def;
        clock=applied?art->clock:0;
        id=RtPtdFollow(art->anim,f,clock?0xFF:0,clock!=0,clock?128+clock-1:0,0,move==MOVE_INGRAIN?(DsMonSize(f->spriteKey)/3)*(sRt->arena?2:1):0);
        if(id<RT_MAX_FX && art->clock)
        {
            // Put solid PTD shield panels behind the body, leaving the creature
            // readable while the panel's edges remain visible around it.
            struct Sprite *sp=&gSprites[sRt->fx[id].spriteId];
            sp->oam.priority=2;sp->subpriority=f->spriteId<MAX_SPRITES?gSprites[f->spriteId].subpriority+1:0x30;
        }
    }
}

static void RtPtdStatusAt(u16 move,s16 x,s16 y)
{
    if(sRtPtdStatusArt[move].anim)RtPtdSpawn(sRtPtdStatusArt[move].anim,x,y,0,FALSE);
}

static void RtPtdStageFx(struct RtFighter *f,u8 stat,s8 change)
{
    s16 scale=sRt->arena?2:1;
    if(stat>=RT_STAT_COUNT || !change)return;
    RtPtdFollow(sRtPtdStageArt[stat][change>0],f,30,FALSE,32+stat,
        (stat%3-1)*8*scale,-(DsMonSize(f->spriteKey)/2+10)*scale);
}

static void RtPtdConditionFx(struct RtFighter *f,u8 kind)
{
    s16 scale=sRt->arena?2:1;
    if(kind<6)RtPtdFollow(sRtPtdConditionArt[kind],f,0,FALSE,64+kind,0,
        (kind>=2?-(DsMonSize(f->spriteKey)/2+4)*scale:0));
}
