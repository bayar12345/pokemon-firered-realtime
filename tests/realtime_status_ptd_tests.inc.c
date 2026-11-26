static void RtStatusTestClearFx(void)
{
    u8 i;for(i=0;i<RT_MAX_FX;i++)RtDestroyFx(i);
}
static u8 RtStatusTestFind(u8 kind)
{
    u8 i;for(i=0;i<RT_MAX_FX;i++)if(sRt->fx[i].active && sRt->fx[i].ptdAnim && sRt->fx[i].kind==kind)return i;
    return 0xFF;
}
static void RtTestStatusPtd(void)
{
    struct RtFighter *f,*e;u16 move,count=0;u8 i,id;u8 tiles[128],refs[16];u32 matrices;
    RtTestReset();ResetSpriteData();FreeAllSpritePalettes();RtScanPalSlots(sRt->palKey);
    f=&sRt->player;e=&sRt->enemy;f->spriteKey=e->spriteKey=DsMonSpriteKey(SPECIES_BULBASAUR,0,FALSE);
    for(i=0;i<RT_MAX_FX;i++)sRt->fx[i].active=FALSE;
    memcpy(tiles,gSpriteTileAllocBitmap,128);memcpy(refs,sRt->palRef,16);matrices=gOamMatrixAllocBitmap;
    for(move=1;move<MOVES_COUNT;move++)
    {
        const struct RtPtdStatusArt *a=&sRtPtdStatusArt[move];
        if(!a->anim)continue;
        count++;RtTestCheck(!RtDamaging(move) && a->anim<ARRAY_COUNT(sRtPtdAnims) && a->recipient<=2 && a->clock<=RTC_COUNT);
        RtPtdStatusFx(f,e,move,FALSE);RtTestCheck(RtStatusTestFind(0)<RT_MAX_FX);RtStatusTestClearFx();
    }
    RtTestCheck(count==138);
    for(i=0;i<RT_STAT_COUNT;i++)RtTestCheck(sRtPtdStageArt[i][0] && sRtPtdStageArt[i][1]);
    for(i=0;i<6;i++)RtTestCheck(sRtPtdConditionArt[i]>0);
    RtTestCheck(RtApplyStatusMove(f,e,MOVE_SMOKESCREEN) && e->stages[RT_STAT_ACC]==-1);
    id=RtStatusTestFind(0);RtTestCheck(id<RT_MAX_FX && sRt->fx[id].followSide==2);
    if(id<RT_MAX_FX)
    {
        s16 x,y;e->x+=Q8(20);e->y+=Q8(10);RtPtdBody(e,&x,&y);RtPtdUpdateFx(id);
        RtTestCheck(gSprites[sRt->fx[id].spriteId].x==x && gSprites[sRt->fx[id].spriteId].y==y);
    }
    RtStatusTestClearFx();e->clocks[RTC_MAGICCOAT]=90;
    RtApplyStatusMove(f,e,MOVE_SMOKESCREEN);id=RtStatusTestFind(0);
    RtTestCheck(f->stages[RT_STAT_ACC]==-1 && e->stages[RT_STAT_ACC]==-1 && id<RT_MAX_FX && sRt->fx[id].followSide==1);
    RtStatusTestClearFx();e->clocks[RTC_SNATCH]=90;RtApplyStatusMove(f,e,MOVE_SWORDS_DANCE);id=RtStatusTestFind(0);
    RtTestCheck(e->stages[RT_STAT_ATK]==2 && !f->stages[RT_STAT_ATK] && id<RT_MAX_FX && sRt->fx[id].followSide==2);
    RtStatusTestClearFx();RtApplyStatusMove(f,e,MOVE_REFLECT);id=RtStatusTestFind(128+RTC_REFLECT);
    RtTestCheck(f->clocks[RTC_REFLECT]==900 && id<RT_MAX_FX);
    if(id<RT_MAX_FX)
    {
        RtTestCheck(sRt->fx[id].life==0xFF && sRt->fx[id].loop && gSprites[sRt->fx[id].spriteId].oam.priority==2);
        RtPtdStatusFx(f,e,MOVE_REFLECT,TRUE);RtTestCheck(RtStatusTestFind(128+RTC_REFLECT)==id);
        f->clocks[RTC_REFLECT]=0;RtPtdUpdateFx(id);RtTestCheck(!sRt->fx[id].active);
    }
    RtStatusTestClearFx();f->stages[RT_STAT_ATK]=6;RtStage(f,RT_STAT_ATK,1);RtTestCheck(RtStatusTestFind(32+RT_STAT_ATK)==0xFF);
    f->ability=ABILITY_KEEN_EYE;RtStage(f,RT_STAT_ACC,-1);RtTestCheck(RtStatusTestFind(32+RT_STAT_ACC)==0xFF);
    f->ability=0;RtSetStatus(f,STATUS1_BURN);id=RtStatusTestFind(64);RtTestCheck(id<RT_MAX_FX);
    if(id<RT_MAX_FX){RtSetStatus(f,0);RtPtdUpdateFx(id);RtTestCheck(!sRt->fx[id].active);}
    RtPtdConditionFx(e,0);RtStatusTestClearFx();RtPtdStatusFx(f,e,MOVE_BARRIER,TRUE);id=RtStatusTestFind(0);
    if(id<RT_MAX_FX){f->partyIndex=1;RtPtdUpdateFx(id);RtTestCheck(!sRt->fx[id].active);f->partyIndex=0;}
    RtStatusTestClearFx();RtTestCheck(RtTestEqual(tiles,gSpriteTileAllocBitmap,128) && RtTestEqual(refs,sRt->palRef,16) && gOamMatrixAllocBitmap==matrices);
    RtRestoreBorrowedPals();ResetSpriteData();
}
