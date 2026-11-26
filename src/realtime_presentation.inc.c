// Copies icon pixels into private OBJ tiles. Never recolor a shared map palette.
#include "data/realtime_field_graphics.h"
#include "data/realtime_weather_graphics.h"
#define RT_GLOW_TAG 0x77B0
#define RT_FIELD_TAG 0x77B4
static void RtMakeGlow(struct RtFighter *f,u8 color)
{
    // A second copy of the current pose, recoloured to one colour.
    struct PmdFrame frame;
    if(f->glowSprite!=MAX_SPRITES || sRt->statusPalSlot==0xFF) return;
    f->glowSprite=DsCreateMonSprite(f->spriteKey,FROM_Q8(f->x),FROM_Q8(f->y),sRt->statusPalSlot,0,SpriteCallbackDummy);
    if(f->glowSprite==MAX_SPRITES) return;
    if(PmdGetFrame(f->spriteKey,PMD_ANIM_IDLE,PmdDirFromVector(0,0,f->dir),0,TRUE,&frame))
        PmdShowMonFrame(f->glowSprite,f->spriteKey,PMD_ANIM_IDLE,PmdDirFromVector(0,0,f->dir),0,TRUE,NULL,NULL);
    DsRecolorMonSprite(f->glowSprite,color);
    gSprites[f->glowSprite].oam.priority=1;
}
static void RtRenderCast(struct RtFighter *f,struct Sprite *sprite)
{
    struct RtCast *c=&f->cast;
    struct RtPresentation *p=&sRt->presentation;
    bool8 red=p->active && p->enemy==f->isEnemy && p->kind!=RTP_WILD && (p->kind==RTP_RECALL || p->time>=24);
    bool8 white=c->active && c->kind==RT_DASH;
    if(white || red)
    {
        RtMakeGlow(f,red?1:10);
        if(f->glowSprite!=MAX_SPRITES)
        {
            struct Sprite *glow=&gSprites[f->glowSprite]; glow->x=sprite->x; glow->y=sprite->y;
            glow->hFlip=sprite->hFlip; glow->invisible=FALSE;
            if(red && p->kind==RTP_RECALL)
            {
                u16 time=p->time<36?p->time:35, scale=256*36/(36-time);
                glow->x+=(p->originX-glow->x)*time/36; glow->y+=(p->originY-glow->y)*time/36;
                if(p->matrix==0xFF) p->matrix=AllocOamMatrix();
                if(p->matrix!=0xFF) { glow->oam.affineMode=ST_OAM_AFFINE_NORMAL; glow->oam.matrixNum=p->matrix; glow->affineAnimPaused=TRUE; SetOamMatrix(p->matrix,scale,0,0,scale); }
            }
            sprite->invisible=TRUE;
        }
    }
    else if(f->glowSprite!=MAX_SPRITES)
    { DestroySprite(&gSprites[f->glowSprite]); f->glowSprite=MAX_SPRITES; }
    if(f->hidden) sprite->invisible=TRUE;
    if(p->active && p->enemy==f->isEnemy)
    {
        if(p->kind==RTP_SEND && p->time<24) sprite->invisible=TRUE;
        if(p->kind==RTP_WILD) sprite->y2=(36-p->time)*(36-p->time)/64;
    }
    if(c->active && c->kind==RT_AIR && (c->move==MOVE_FLY || c->move==MOVE_BOUNCE))
    {
        u16 start=RtProfile(c->move)->startup;
        if(c->elapsed<12) { sprite->invisible=FALSE; sprite->y2=-c->elapsed*4; }
        else if(c->elapsed>=start-12 && c->elapsed<start)
        { sprite->invisible=FALSE; sprite->x=FROM_Q8(c->targetX); sprite->y=FROM_Q8(c->targetY); sprite->y2=-(start-c->elapsed)*4; }
    }
    if(c->active && c->move==MOVE_BODY_SLAM) sprite->y2=-(c->end-c->elapsed)*c->elapsed/16;
    // Psychic holds a caught target only through its short rise and fall.
    if(RtPsychicHeld(f))
    {
        sprite->y2=RtPsychicLiftY(f);
        sprite->invisible=FALSE;
    }
    // Other lifting moves retain their authored cosmetic motion.
    c=&RtOther(f)->cast;
    if(c->active && c->move!=MOVE_PSYCHIC && (RtProfile(c->move)->flags&RTMF_LIFT) && c->hitCount && RtInCast(RtOther(f),f)) sprite->y2=-((c->elapsed&15)<8?(c->elapsed&7)*2:(15-(c->elapsed&15))*2);
}
static void RtStopPresentation(bool8 reveal)
{
    struct RtPresentation *p=&sRt->presentation; struct RtFighter *f;
    if(!p->active) return;
    f=p->enemy?&sRt->enemy:&sRt->player;
    if(p->ballSprite!=MAX_SPRITES) DestroySprite(&gSprites[p->ballSprite]);
    if(p->matrix!=0xFF) FreeOamMatrix(p->matrix);
    if(p->ballPal!=0xFF) { FreeSpriteTilesByTag(gBallSpriteSheets[p->ballId].tag); RtReleasePal(p->ballPal); }
    p->active=FALSE;
    if(f->glowSprite!=MAX_SPRITES) { DestroySprite(&gSprites[f->glowSprite]); f->glowSprite=MAX_SPRITES; }
    if(f->spriteId!=MAX_SPRITES && f->active) { gSprites[f->spriteId].invisible=!reveal; gSprites[f->spriteId].y2=0; }
}
static void RtBeginPresentation(struct RtFighter *f,u8 kind)
{
    struct RtPresentation *p=&sRt->presentation; struct SpriteTemplate t; u16 ball;
    RtStopPresentation(TRUE); memset(p,0,sizeof(*p)); p->active=TRUE; p->kind=kind; p->enemy=f->isEnemy;
    p->ballSprite=MAX_SPRITES; p->ballPal=0xFF; p->matrix=0xFF;
    p->originX=f->isEnemy?(sRt->arena?224:FROM_Q8(f->x)+24):sRt->originX;
    p->originY=f->isEnemy?80:sRt->originY-8;
    if(kind==RTP_WILD)
    {
        s16 tx,ty; u8 fx=FX_SMOKE;
        RtScreenToTile(FROM_Q8(f->x),FROM_Q8(f->y)+4,&tx,&ty);
        if(MetatileBehavior_IsTallGrass(MapGridGetMetatileBehaviorAt(tx,ty))) fx=FX_LEAF;
        if(MetatileBehavior_IsSurfable(MapGridGetMetatileBehaviorAt(tx,ty))) fx=FX_WATER;
        RtSpawnFx(fx,FROM_Q8(f->x)-7,FROM_Q8(f->y)+9,20,FALSE); RtSpawnFx(fx,FROM_Q8(f->x)+7,FROM_Q8(f->y)+9,20,FALSE); return;
    }
    ball=GetMonData(f->mon,MON_DATA_POKEBALL); if(ball<ITEM_MASTER_BALL || ball>ITEM_PREMIER_BALL) ball=ITEM_POKE_BALL;
    p->ballId=ItemIdToBallId(ball);
    if(LoadCompressedSpriteSheetUsingHeap(&gBallSpriteSheets[p->ballId])) return;
    p->ballPal=RtAcquireCompressedPal(RT_PAL_KEY_BALL(p->ballId),gBallSpritePalettes[p->ballId].data);
    if(p->ballPal==0xFF) { FreeSpriteTilesByTag(gBallSpriteSheets[p->ballId].tag); return; }
    t.tileTag=gBallSpriteSheets[p->ballId].tag; t.paletteTag=TAG_NONE; t.oam=&gOamData_AffineOff_ObjNormal_16x16;
    t.anims=gDummySpriteAnimTable; t.images=NULL; t.affineAnims=gDummySpriteAffineAnimTable; t.callback=SpriteCallbackDummy;
    p->ballSprite=CreateSprite(&t,p->originX,p->originY,0);
    if(p->ballSprite!=MAX_SPRITES) { gSprites[p->ballSprite].oam.paletteNum=p->ballPal; gSprites[p->ballSprite].oam.priority=1; }
    PlaySE(kind==RTP_SEND?SE_BALL_THROW:SE_BALL_OPEN);
}
static void RtUpdatePresentation(void)
{
    struct RtPresentation *p=&sRt->presentation; struct RtFighter *f; s16 t;
    if(!p->active) return;
    f=p->enemy?&sRt->enemy:&sRt->player; p->time+=sRt->frameStep;
    if(p->kind==RTP_SEND && p->ballSprite!=MAX_SPRITES)
    {
        struct Sprite *ball=&gSprites[p->ballSprite]; t=p->time>24?24:p->time;
        ball->x=p->originX+(FROM_Q8(f->x)-p->originX)*t/24;
        ball->y=p->originY+(FROM_Q8(f->y)-p->originY)*t/24-t*(24-t)/8;
        ball->invisible=p->time>=24;
        if(p->time>=24 && p->time-sRt->frameStep<24) { PlaySE(SE_BALL_OPEN); RtSpawnFx(FX_SPARKLE,FROM_Q8(f->x),FROM_Q8(f->y),12,FALSE); }
    }
    if(p->time>= (p->kind==RTP_SEND?48:36)) RtStopPresentation(p->kind!=RTP_RECALL);
}
static void RtForceEnemySwitch(u8 index,bool8 pass)
{
    if(index>=gEnemyPartyCount || index==sRt->enemy.partyIndex || !RtIsUsableMon(&gEnemyParty[index])) return;
    sRt->forcedEnemy=index; sRt->enemyBaton=pass;
    RtLeaveField(&sRt->enemy);
    if(!pass && RtSubstitute(&sRt->enemy)) RtRemoveField(RtSubstitute(&sRt->enemy));
    if(pass) memcpy(sRt->batonStages,sRt->enemy.stages,sizeof(sRt->batonStages));
    RtClearProjectiles(); RtCancelCast(&sRt->player); RtCancelCast(&sRt->enemy);
    RtBeginPresentation(&sRt->enemy,RTP_RECALL); sRt->afterPresentation=RT_STATE_ENEMY_SWITCH; sRt->state=RT_STATE_PRESENTATION;
}
static void RtSwitchPlayer(u8 index)
{
    struct RtFighter *f=&sRt->player; u8 i;
    if(index>=gPlayerPartyCount || index==f->partyIndex || !RtIsUsableMon(&gPlayerParty[index])) return;
    if(!f->active) { f->partyIndex=index; f->mon=&gPlayerParty[index]; return; }
    RtLeaveField(f);
    if(!sRt->batonPass && RtSubstitute(f)) RtRemoveField(RtSubstitute(f));
    sRt->switchTarget=index; sRt->switchRecovery=f->cooldown>60?f->cooldown:60;
    for(i=0;i<4;i++) sRt->switchMoveCd[f->partyIndex][i]=f->moveCooldown[i];
    RtClearProjectiles(); RtCancelCast(f); RtCancelCast(&sRt->enemy); sRt->enemy.windup=0;
    RtBeginPresentation(f,RTP_RECALL); sRt->timer=0; sRt->state=RT_STATE_MANUAL_SWITCH;
}
static void RtUpdateSwitch(void)
{
    s16 x,y; u8 i;
    RtUpdatePresentation(); RtSyncSprite(&sRt->player); RtSyncSprite(&sRt->enemy); RtUpdateFx();
    if(sRt->presentation.active) return;
    if(!sRt->timer)
    {
        x=FROM_Q8(sRt->player.x); y=FROM_Q8(sRt->player.y); RtRemoveFighterSprite(&sRt->player);
        RtSummonPlayerMon(sRt->switchTarget,x,y); sRt->player.cooldown=sRt->player.recoveryMax=sRt->switchRecovery;
        for(i=0;i<4;i++) sRt->player.moveCooldown[i]=sRt->player.moveCooldownMax[i]=sRt->switchMoveCd[sRt->switchTarget][i];
        if(sRt->batonPass) { memcpy(sRt->player.stages,sRt->batonStages,sizeof(sRt->batonStages)); sRt->batonPass=FALSE; }
        sRt->timer=1;
    }
    else { RtResumeFight(); sRt->timer=0; }
    RtUpdateHud();
}
static void RtPartyStrikeCallback(struct Sprite *s)
{
    if(++s->data[0]>16) { RtReleasePal(s->data[2]); DestroySprite(s); }
    else s->x2=(s->data[0]<8?s->data[0]:16-s->data[0]);
}
static void RtPartyStrike(struct RtFighter *f,struct Pokemon *mon,s16 x,s16 y)
{
    u8 id,pal; bool8 shiny=IsMonShiny(mon);
    u16 key=DsMonSpriteKey(GetMonData(mon,MON_DATA_SPECIES),GetMonData(mon,MON_DATA_PERSONALITY),GetMonGender(mon)==MON_FEMALE);
    pal=RtAcquirePal(RT_PAL_KEY_DS(key,shiny),DsMonPalette(key,shiny));
    if(pal==0xFF) return;
    id=DsCreateMonSprite(key,x,y+8-DsMonAnchorY(key),pal,0,RtPartyStrikeCallback);
    if(id<MAX_SPRITES)sRt->worldSprites[id]=TRUE;
    if(id!=MAX_SPRITES) { gSprites[id].data[2]=pal; gSprites[id].oam.priority=2; DsUpdateMonSprite(id,key,f->dir,0); }
    else RtReleasePal(pal);
}
static void RtDrawField(struct RtField *field)
{
    struct SpriteTemplate t;
    if(field->spriteId!=MAX_SPRITES || sRt->fieldTiles==0xFFFF || sRt->statusPalSlot==0xFF) return;
    t.tileTag=RT_FIELD_TAG; t.paletteTag=TAG_NONE; t.oam=&gOamData_AffineOff_ObjNormal_32x32;
    t.anims=gDummySpriteAnimTable; t.images=NULL; t.affineAnims=gDummySpriteAffineAnimTable; t.callback=SpriteCallbackDummy;
    field->spriteId=CreateSprite(&t,field->x,field->y,0x60);
    if(field->spriteId!=MAX_SPRITES) { gSprites[field->spriteId].oam.tileNum=sRt->fieldTiles+field->kind*16; gSprites[field->spriteId].oam.paletteNum=sRt->statusPalSlot; gSprites[field->spriteId].oam.priority=2; }
}
static void RtClearWeatherSprites(void)
{ u8 i; for(i=0;i<12;i++) if(sRt->weatherSprites[i]!=MAX_SPRITES) { DestroySprite(&gSprites[sRt->weatherSprites[i]]); sRt->weatherSprites[i]=MAX_SPRITES; } }
static void RtSetBattleWeather(u8 weather,u16 duration)
{ RtClearWeatherSprites(); sRt->weather=weather; sRt->weatherTimer=duration; sRt->weatherTick=180; }
static void RtUpdateWeather(void)
{
    u8 i,kind; struct RtFighter *f; struct SpriteTemplate t; struct Sprite *sp;
    if(!sRt->weather) return;
    sRt->weatherTimer=RtDec(sRt->weatherTimer,sRt->frameStep);
    if(!sRt->weatherTimer) { RtClearWeatherSprites(); sRt->weather=RTW_NONE; return; }
    if(sRt->weather!=RTW_SUN && sRt->weatherTiles!=0xFFFF && sRt->statusPalSlot!=0xFF)
    {
        for(i=0;i<12;i++)
        {
            if(sRt->weatherSprites[i]==MAX_SPRITES)
            {
                t.tileTag=0x77B6; t.paletteTag=TAG_NONE; t.oam=sRt->weather==RTW_RAIN?&gOamData_AffineOff_ObjNormal_16x32:sRt->weather==RTW_HAIL?&gOamData_AffineOff_ObjNormal_8x8:&gOamData_AffineOff_ObjNormal_32x32;
                t.anims=gDummySpriteAnimTable; t.images=NULL; t.affineAnims=gDummySpriteAffineAnimTable; t.callback=SpriteCallbackDummy;
                sRt->weatherSprites[i]=CreateSprite(&t,10+Random()%220,32+Random()%88,0x70);
                if(sRt->weatherSprites[i]==MAX_SPRITES) continue;
                sRt->worldSprites[sRt->weatherSprites[i]]=FALSE;
                sp=&gSprites[sRt->weatherSprites[i]]; if(sRt->arena){sp->x*=2;sp->y*=2;} sp->oam.paletteNum=sRt->statusPalSlot; sp->oam.priority=2;
                sp->oam.tileNum=sRt->weatherTiles+(sRt->weather==RTW_RAIN?0:sRt->weather==RTW_HAIL?8:9);
            }
            sp=&gSprites[sRt->weatherSprites[i]];
            sp->x+=(sRt->weather==RTW_SAND?4:-1)*sRt->frameStep;
            sp->y+=(sRt->weather==RTW_RAIN?4:1)*sRt->frameStep;
            if(sp->y>(sRt->arena?248:124)) sp->y=sRt->arena?60:30;
            if(sp->x<(sRt->arena?16:8)) sp->x=sRt->arena?460:230;
            if(sp->x>(sRt->arena?464:232)) sp->x=sRt->arena?20:10;
        }
    }
    else if(sRt->weather==RTW_SUN)
    {
        sRt->weatherClock+=sRt->frameStep;
        if(sRt->weatherClock>=24) { sRt->weatherClock=0; RtSpawnFx(FX_SPARKLE,(12+Random()%216)*(sRt->arena?2:1),(32+Random()%88)*(sRt->arena?2:1),20,FALSE); }
    }
    sRt->weatherTick=RtDec(sRt->weatherTick,sRt->frameStep);
    if(!sRt->weatherTick && RtWeatherWorks())
    {
        sRt->weatherTick=180;
        for(i=0;i<2;i++)
        {
            f=i?&sRt->enemy:&sRt->player;
            if(sRt->weather==RTW_SAND && !RtTypeIs(f,TYPE_ROCK) && !RtTypeIs(f,TYPE_GROUND) && !RtTypeIs(f,TYPE_STEEL)) RtSetHp(f,f->hp-(f->maxHp/16?f->maxHp/16:1));
            if(sRt->weather==RTW_HAIL && !RtTypeIs(f,TYPE_ICE)) RtSetHp(f,f->hp-(f->maxHp/16?f->maxHp/16:1));
        }
    }
}
static void RtCleanupEngine(void)
{
    u8 i;
    RtStopPresentation(TRUE); RtCancelCast(&sRt->player); RtCancelCast(&sRt->enemy);
    for(i=0;i<8;i++) if(sRt->field[i].active) RtRemoveField(&sRt->field[i]);
    RtClearWeatherSprites(); FreeSpriteTilesByTag(0x77B6);
    FreeSpriteTilesByTag(RT_FIELD_TAG);
    for(i=0;i<MAX_SPRITES;i++) if(gSprites[i].inUse && gSprites[i].callback==RtPartyStrikeCallback) { RtReleasePal(gSprites[i].data[2]); DestroySprite(&gSprites[i]); }
}
