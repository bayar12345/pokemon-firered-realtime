// Included only by an explicitly instrumented emulator build. No callbacks or
// report memory from this file exist in the production ROM.
#include "load_save.h"
#include "event_data.h"
#include "constants/metatile_behaviors.h"
EWRAM_DATA u32 gRtTestReport[64] = {0};
const u32 gRtTestLayout[] = {
 sizeof(struct RtBattle),sizeof(struct RtFighter),offsetof(struct RtFighter,cast),offsetof(struct RtFighter,spriteKey),
 offsetof(struct RtBattle,fx),sizeof(struct RtFx),offsetof(struct RtBattle,state),offsetof(struct RtBattle,originX),
 offsetof(struct RtBattle,winIds),offsetof(struct RtBattle,msgVisible),offsetof(struct RtBattle,msgCompact),
 offsetof(struct RtBattle,arena),offsetof(struct RtBattle,arenaActive),offsetof(struct RtBattle,arenaObjectCount),
 offsetof(struct RtBattle,arenaObjects),offsetof(struct RtBattle,arenaMatrices),offsetof(struct RtBattle,proj),sizeof(struct RtProjectile),
 offsetof(struct RtBattle,fieldTiles),offsetof(struct RtBattle,statusPalSlot),offsetof(struct RtBattle,aiDifficulty),offsetof(struct RtFx,ptdAnim),
 offsetof(struct RtCast,beamAnim),offsetof(struct RtCast,beamMatrix),offsetof(struct RtCast,beamFrame),offsetof(struct RtCast,beamPixels),
 offsetof(struct RtBattle,weather),offsetof(struct RtFighter,palSlot),offsetof(struct RtFighter,clocks),
 offsetof(struct RtBattle,arenaPortrait),offsetof(struct RtBattle,legendarySpecies),offsetof(struct RtBattle,legendaryPhase)
};
static void RtTestCheck(bool8 pass)
{
    gRtTestReport[1]++;
    if(!pass) { u32 n=++gRtTestReport[2]; if(n<48) gRtTestReport[15+n]=gRtTestReport[1]; }
}
static bool8 RtTestEqual(const void *a,const void *b,u16 size)
{
    const u8 *x=a,*y=b;
    while(size--) if(*x++!=*y++) return FALSE;
    return TRUE;
}
static void RtTestFighter(struct RtFighter *f,struct Pokemon *mon,bool8 enemy)
{
    u8 i;
    f->mon=mon; f->isEnemy=enemy; f->active=TRUE; f->partyIndex=0;
    f->hp=f->maxHp=1000; f->level=50; f->species=GetMonData(mon,MON_DATA_SPECIES);
    f->types[0]=f->types[1]=TYPE_NORMAL;
    f->spriteId=f->glowSprite=f->statusSpriteId=f->confusionSpriteId=MAX_SPRITES;
    f->x=Q8(enemy?86:64); f->y=Q8(80); f->dir=enemy?RT_DIR_LEFT:RT_DIR_RIGHT;
    for(i=0;i<8;i++) f->cast.fx[i]=0xFF;
}
static void RtTestReset(void)
{
    u8 i;
    memset(sRt,0,sizeof(*sRt));
    for(i=0;i<2;i++){sRt->arenaPortrait[i]=MAX_SPRITES;sRt->arenaPortraitPal[i]=0xFF;}
    for(i=0;i<9;i++)sRt->arenaMatrices[i]=0xFF;
    memset(gPlayerParty,0,sizeof(gPlayerParty)); memset(gEnemyParty,0,sizeof(gEnemyParty));
    CreateMon(&gPlayerParty[0],SPECIES_NIDORAN_M,50,31,TRUE,0x12345678,OT_ID_PRESET,0);
    CreateMon(&gEnemyParty[0],SPECIES_NIDORAN_F,50,31,TRUE,0x12345678,OT_ID_PRESET,0);
    gPlayerPartyCount=gEnemyPartyCount=1;
    for(i=0;i<4;i++) { SetMonMoveSlot(&gPlayerParty[0],MOVE_NONE,i); SetMonMoveSlot(&gEnemyParty[0],MOVE_NONE,i); }
    RtTestFighter(&sRt->player,&gPlayerParty[0],FALSE); RtTestFighter(&sRt->enemy,&gEnemyParty[0],TRUE);
    for(i=0;i<RT_MAX_FX;i++) sRt->fx[i].active=TRUE;
    for(i=0;i<12;i++) sRt->weatherSprites[i]=MAX_SPRITES;
    for(i=0;i<RT_WIN_COUNT;i++) sRt->winIds[i]=WINDOW_NONE;
    sRt->statusPalSlot=0xFF; sRt->fieldTiles=0xFFFF;
    sRt->arena=TRUE; sRt->frameStep=1;
}
#include "realtime_wild_space_tests.inc.c"
#include "realtime_psychic_tests.inc.c"
#include "realtime_ptd_tests.inc.c"
#include "realtime_status_ptd_tests.inc.c"
#include "realtime_final_tests.inc.c"

static void RtTestVBlank(void) { LoadOam(); ProcessSpriteCopyRequests(); TransferPlttBuffer(); }
static void RtTestUiMain(void)
{
    if(sRt->state==RT_STATE_LEARN_MOVE) RtUpdateLearning();
    else if(sRt->state==RT_STATE_MESSAGES && RtUpdateMessages())
    { if(!RtStartLearning()) sRt->state=sRt->afterMsgState; }
    AnimateSprites(); BuildOamBuffer(); UpdatePaletteFade();
    gRtTestReport[4]=sRt->state;
    gRtTestReport[5]=GetMonData(sRt->player.mon,MON_DATA_MOVE1);
    gRtTestReport[6]=GetMonData(sRt->player.mon,MON_DATA_PP_BONUSES);
    gRtTestReport[7]=sRt->learnIndex;
    gRtTestReport[9]=sRt->learnCursor;
    gRtTestReport[10]=sRt->learnConfirm;
}
void RealtimeRegression(void)
{
    struct RtFighter *a,*b,snapshot;
    u32 i,j,iv,rng; u16 mult; bool8 crit; s32 normal,boosted; u8 denied=0;
    static const u8 ids[]={MON_DATA_HP_IV,MON_DATA_ATK_IV,MON_DATA_DEF_IV,MON_DATA_SPEED_IV,MON_DATA_SPATK_IV,MON_DATA_SPDEF_IV};
    static const struct BgTemplate bg[]={{.bg=0,.charBaseIndex=0,.mapBaseIndex=31,.priority=0}};
    static const struct WindowTemplate empty[]={{.bg=0xFF}};
    gRtTestReport[3]=offsetof(struct RtFighter,cast);
    gRtTestReport[11]=offsetof(struct RtBattle,state);
    gRtTestReport[12]=offsetof(struct RtBattle,winIds);
    gRtTestReport[13]=offsetof(struct RtBattle,originX);
    gRtTestReport[14]=offsetof(struct RtBattle,msgVisible);
    gRtTestReport[15]=offsetof(struct RtBattle,msgCompact);
    gRtTestReport[0]=0x52545255;
    SetSaveBlocksPointers(); ResetSpriteData(); FreeAllSpritePalettes();
    sRt=AllocZeroed(sizeof(*sRt));
    if(!sRt) { gRtTestReport[0]=0xDEAD; return; }
    RtTestReset(); a=&sRt->player; b=&sRt->enemy;
    // All 64 possible power-bit patterns, including both formula endpoints.
    for(i=0;i<64;i++)
    {
        for(j=0;j<6;j++) { iv=(i&(1<<j))?2:0; SetMonData(a->mon,ids[j],&iv); }
        RtTestCheck(RtHiddenPower(a)==30+i*40/63);
    }
    // Exact 5/10/20/30/20/10/5 Magnitude buckets over 100 possible rolls.
    { u8 counts[7]={0}; static const u8 powers[]={10,30,50,70,90,110,150}; static const u8 expected[]={5,10,20,30,20,10,5};
      for(i=0;i<100;i++) for(j=0;j<7;j++) if(RtMagnitudePower(i)==powers[j]) counts[j]++;
      for(i=0;i<7;i++) RtTestCheck(counts[i]==expected[i]); }
    RtTestReset();
    RtTestCheck(RtCriticalDenominator(a,b,MOVE_TACKLE)==16);
    a->clocks[RTC_FOCUS]=1800; RtTestCheck(RtCriticalDenominator(a,b,MOVE_TACKLE)==4);
    RtTestCheck(RtCriticalDenominator(a,b,MOVE_SLASH)==3);
    a->item=ITEM_SCOPE_LENS; RtTestCheck(RtCriticalDenominator(a,b,MOVE_SLASH)==2);
    b->ability=ABILITY_BATTLE_ARMOR; RtTestCheck(!RtCriticalDenominator(a,b,MOVE_SLASH));
    b->ability=ABILITY_SHELL_ARMOR; RtTestCheck(!RtCriticalDenominator(a,b,MOVE_SLASH));
    RtTestReset();
    RtLaunchCast(a,b,MOVE_ROLLOUT); RtTestCheck(RtBasePower(a,b,MOVE_ROLLOUT)==30);
    RtApplyImpact(a,b,MOVE_ROLLOUT,0); RtTestCheck(a->rollout==1); RtCancelCast(a);
    RtLaunchCast(a,b,MOVE_ROLLOUT); RtTestCheck(RtBasePower(a,b,MOVE_ROLLOUT)==60); RtCancelCast(a);
    RtLaunchCast(a,b,MOVE_ROLLOUT); RtTestCheck(RtBasePower(a,b,MOVE_ROLLOUT)==30);
    RtRegisterHit(a,MOVE_ROLLOUT); RtRegisterHit(a,MOVE_ROLLOUT); RtTestCheck(a->rollout==1);
    RtCancelCast(a); RtLaunchCast(a,b,MOVE_SCRATCH); RtTestCheck(!a->rollout);
    RtCancelCast(a); a->defenseCurl=TRUE; RtLaunchCast(a,b,MOVE_ROLLOUT); RtTestCheck(RtBasePower(a,b,MOVE_ROLLOUT)==60);
    RtCancelCast(a); RtLaunchCast(a,b,MOVE_TRIPLE_KICK);
    for(i=1;i<=3;i++) { a->cast.hitCount=i; RtTestCheck(RtBasePower(a,b,MOVE_TRIPLE_KICK)==i*10); }
    RtTestReset();
    RtTestCheck(RtAttractEligible(a,b));
    CreateMon(b->mon,SPECIES_NIDORAN_M,50,31,TRUE,1,OT_ID_PRESET,0); RtTestCheck(!RtAttractEligible(a,b));
    CreateMon(b->mon,SPECIES_DITTO,50,31,TRUE,1,OT_ID_PRESET,0); RtTestCheck(!RtAttractEligible(a,b));
    RtTestReset(); b->ability=ABILITY_OBLIVIOUS; RtTestCheck(!RtAttractEligible(a,b)); b->ability=0;
    a->clocks[RTC_ATTRACT]=600;
    for(i=0;i<128;i++) if(!RtCanAct(a,MOVE_TACKLE)) denied++;
    RtTestCheck(denied>35 && denied<95);
    b->partyIndex=1; RtUpdateClocks(a); RtTestCheck(!a->clocks[RTC_ATTRACT]);
    RtTestReset(); RtTestCheck(RtMoveAllowed(a,MOVE_FAKE_OUT));
    RtUseMove(a,b,MOVE_FAKE_OUT); RtTestCheck(a->cast.active && !RtMoveAllowed(a,MOVE_FAKE_OUT));
    RtTestReset(); a->clocks[RTC_TORMENT]=100; a->lastMove=MOVE_SCRATCH;
    RtUseMove(a,b,MOVE_TACKLE); RtTestCheck(a->cast.active);
    RtTestReset(); RtLaunchCast(b,a,MOVE_FOCUS_PUNCH); RtApplyImpact(a,b,MOVE_TACKLE,0); RtTestCheck(!b->cast.active);
    RtTestReset(); b->rage=TRUE; RtApplyImpact(a,b,MOVE_SCRATCH,0); RtTestCheck(b->stages[RT_STAT_ATK]==1);
    RtTestReset(); b->hidden=TRUE; b->cast.move=MOVE_FLY;
    RtTestCheck(!RtCanHitHidden(b,MOVE_TACKLE)); RtTestCheck(RtCanHitHidden(b,MOVE_GUST));
    b->cast.move=MOVE_DIG; RtTestCheck(RtCanHitHidden(b,MOVE_EARTHQUAKE)); RtTestCheck(!RtCanHitHidden(b,MOVE_THUNDER));
    b->cast.move=MOVE_DIVE; RtTestCheck(RtCanHitHidden(b,MOVE_SURF)); RtTestCheck(!RtCanHitHidden(b,MOVE_GUST));
    RtTestCheck(!RtCallAllowed(MOVE_METRONOME,MOVE_PROTECT));
    RtTestCheck(!RtCallAllowed(MOVE_ASSIST,MOVE_FOCUS_PUNCH));
    RtTestCheck(!RtCallAllowed(MOVE_SLEEP_TALK,MOVE_FLY));
    RtTestCheck(RtCallAllowed(MOVE_SKETCH,MOVE_METRONOME));
    RtTestCheck(!RtCallAllowed(MOVE_MIMIC,MOVE_METRONOME));
    RtTestReset(); SetMonMoveSlot(a->mon,MOVE_SLEEP_TALK,0); a->status=STATUS1_SLEEP_TURN(2);
    RtTestCheck(!RtCalledMove(a,b,MOVE_SLEEP_TALK));
    SetMonMoveSlot(a->mon,MOVE_TACKLE,1); RtTestCheck(RtCalledMove(a,b,MOVE_SLEEP_TALK)==MOVE_TACKLE);
    RtTestReset(); normal=RtAiDamage(a,b,MOVE_TACKLE); a->item=ITEM_CHOICE_BAND; boosted=RtAiDamage(a,b,MOVE_TACKLE);
    RtTestCheck(boosted>normal);
    a->choiceMove=MOVE_TACKLE; RtTestCheck(!RtMoveAllowed(a,MOVE_SCRATCH)); RtTestCheck(RtMoveAllowed(a,MOVE_TACKLE));
    RtTestReset(); b->types[0]=b->types[1]=TYPE_GHOST; RtTestCheck(!RtAiDamage(a,b,MOVE_TACKLE));
    b->clocks[RTC_FORESIGHT]=100; RtTestCheck(RtAiDamage(a,b,MOVE_TACKLE)>0);
    RtTestReset(); b->ability=ABILITY_VOLT_ABSORB; b->hp=100; snapshot=*b;
    { u16 ime=REG_IME; REG_IME=0; rng=gRngValue;
      for(i=0;i<50;i++) RtAiDamage(a,b,MOVE_THUNDERBOLT);
      RtTestCheck(RtTestEqual(&snapshot,b,sizeof(snapshot))); RtTestCheck(gRngValue==rng);
      REG_IME=ime; }
    RtCalcDamage(a,b,MOVE_THUNDERBOLT,&mult,&crit); RtTestCheck(b->hp==350 && mult==0);
    RtTestReset(); b->ability=ABILITY_STURDY; RtTestCheck(!RtAiDamage(a,b,MOVE_FISSURE));
    b->ability=ABILITY_FLASH_FIRE; RtCalcDamage(a,b,MOVE_EMBER,&mult,&crit); RtTestCheck(b->flashFire && !mult);
    RtTestReset(); b->status=STATUS1_BURN; b->item=ITEM_LUM_BERRY; RtTryBerry(b); RtTestCheck(!b->status && !b->item && b->recycledItem==ITEM_LUM_BERRY);
    b->confusionTimer=100; b->item=ITEM_LUM_BERRY; RtTryBerry(b); RtTestCheck(!b->confusionTimer && !b->item);
    b->clocks[RTC_ATTRACT]=100; b->item=ITEM_MENTAL_HERB; RtTryBerry(b); RtTestCheck(!b->clocks[RTC_ATTRACT] && !b->item);
    RtTestReset(); a->clocks[RTC_ATTRACT]=100; b->ability=ABILITY_NATURAL_CURE; b->status=STATUS1_BURN; RtLeaveField(b);
    RtTestCheck(!a->clocks[RTC_ATTRACT] && !b->status);
    RtTestReset();
    for(i=0;i<5;i++)
    {
        RtLaunchCast(a,b,MOVE_ROLLOUT); RtTestCheck(RtBasePower(a,b,MOVE_ROLLOUT)==(30<<i));
        RtRegisterHit(a,MOVE_ROLLOUT); RtCancelCast(a);
    }
    RtLaunchCast(a,b,MOVE_ROLLOUT); RtTestCheck(RtBasePower(a,b,MOVE_ROLLOUT)==30);
    RtCancelCast(a);
    for(i=0;i<8;i++) { RtLaunchCast(a,b,MOVE_FURY_CUTTER); RtRegisterHit(a,MOVE_FURY_CUTTER); RtCancelCast(a); }
    RtLaunchCast(a,b,MOVE_FURY_CUTTER); RtTestCheck(RtBasePower(a,b,MOVE_FURY_CUTTER)==160);
    RtTestReset(); RtLaunchCast(b,a,MOVE_FOCUS_PUNCH); b->cast.elapsed=RtProfile(MOVE_FOCUS_PUNCH)->startup;
    RtApplyImpact(a,b,MOVE_SCRATCH,0); RtTestCheck(b->cast.active);
    RtTestReset(); a->stages[RT_STAT_ATK]=6; a->item=ITEM_CHOICE_BAND;
    RtLaunchCast(a,b,MOVE_BEAT_UP); a->cast.hitCount=1; RtTestCheck(RtAiDamage(a,b,MOVE_BEAT_UP)==6);
    CreateMon(&gPlayerParty[1],SPECIES_SNORLAX,10,31,TRUE,123,OT_ID_PRESET,0);
    a->cast.hitCount=2; RtTestCheck(RtAiDamage(a,b,MOVE_BEAT_UP)==4);
    iv=STATUS1_BURN; SetMonData(&gPlayerParty[1],MON_DATA_STATUS,&iv); RtTestCheck(!RtAiDamage(a,b,MOVE_BEAT_UP));
    RtTestReset(); RtLaunchCast(a,b,MOVE_STRING_SHOT); a->cast.elapsed=RtProfile(MOVE_STRING_SHOT)->startup-1;
    RtUpdateCast(a,b); RtTestCheck(sRt->proj[0].active && sRt->proj[0].fieldKind==RTF_WEB+1 && !sRt->field[0].active);
    for(i=0;i<30;i++) RtUpdateProjectiles();
    RtTestCheck(!sRt->proj[0].active && sRt->field[0].active);
    RtUpdateField(); RtTestCheck(b->clocks[RTC_SLOW]==300);
    RtTestReset(); a->ability=ABILITY_DRIZZLE; RtApplyEntryAbilities(a); RtTestCheck(sRt->weather==RTW_RAIN && sRt->weatherTimer==900);
    sRt->weatherTimer=5; RtApplyEntryAbilities(a); RtTestCheck(sRt->weatherTimer==5);
    RtTestReset(); a->ability=ABILITY_INTIMIDATE; RtApplyEntryAbilities(a); RtTestCheck(b->stages[RT_STAT_ATK]==-1);
    RtTestReset(); a->ability=ABILITY_TRACE; b->ability=ABILITY_INTIMIDATE; RtApplyEntryAbilities(a);
    RtTestCheck(a->ability==ABILITY_INTIMIDATE && b->stages[RT_STAT_ATK]==-1);
    RtTestReset(); sRt->isTrainer=TRUE; gTrainerBattleOpponent_A=TRAINER_COOLTRAINER_SAMUEL; b->hp=200;
    RtTestCheck(RtUseTrainerItem() && b->hp==250 && sRt->trainerItemsUsed==1);
    b->cooldown=0; sRt->trainerItemTimer=0; RtTestCheck(!RtUseTrainerItem());
    RtTestReset(); SetMonMoveSlot(a->mon,MOVE_TACKLE,0); b->ability=ABILITY_PRESSURE;
    iv=5; SetMonData(a->mon,MON_DATA_PP1,&iv); RtSpendPp(a,0,MOVE_TACKLE); RtTestCheck(RtPp(a,0)==3);
    iv=1; SetMonData(a->mon,MON_DATA_PP1,&iv); RtSpendPp(a,0,MOVE_TACKLE); RtTestCheck(!RtPp(a,0));
    SetMonMoveSlot(a->mon,MOVE_GROWL,1); a->item=ITEM_CHOICE_BAND; a->choiceMove=MOVE_TACKLE;
    RtTestCheck(!RtHasPp(a));
    SetMonMoveSlot(a->mon,MOVE_SWORDS_DANCE,2); iv=5; SetMonData(a->mon,MON_DATA_PP3,&iv);
    RtSpendPp(a,2,MOVE_SWORDS_DANCE); RtTestCheck(RtPp(a,2)==4);
    RtTestReset(); sRt->weather=RTW_RAIN; RtTestCheck(RtMoveType(a,MOVE_WEATHER_BALL)==TYPE_WATER);
    b->ability=ABILITY_CLOUD_NINE; RtTestCheck(RtMoveType(a,MOVE_WEATHER_BALL)==TYPE_NORMAL);
    b->ability=0; boosted=RtAiDamage(a,b,MOVE_SOLAR_BEAM); sRt->weather=RTW_NONE;
    RtTestCheck(RtAiDamage(a,b,MOVE_SOLAR_BEAM)>boosted);
    // Repeated real sprite/palette/affine allocations must recover fully.
    RtTestReset(); for(i=0;i<RT_MAX_FX;i++) sRt->fx[i].active=FALSE;
    RtScanPalSlots(sRt->palKey); RtInitStatusGraphics();
    RtTestCheck(RtInitFighter(a,&gPlayerParty[0],FALSE,64,80,0));
    RtTestCheck(RtInitFighter(b,&gEnemyParty[0],TRUE,170,80,0));
    for(i=0;i<16;i++)
    {
        u8 matrix; u16 before=0,after=0;
        for(j=0;j<16;j++) before+=sRt->palRef[j];
        RtBeginPresentation(a,RTP_RECALL);
        for(j=0;j<35;j++) { RtUpdatePresentation(); RtSyncSprite(a); }
        RtTestCheck(sRt->presentation.matrix!=0xFF && a->glowSprite!=MAX_SPRITES);
        RtUpdatePresentation(); RtTestCheck(!sRt->presentation.active && a->glowSprite==MAX_SPRITES);
        matrix=AllocOamMatrix(); RtTestCheck(matrix==0); if(matrix!=0xFF) FreeOamMatrix(matrix);
        for(j=0;j<16;j++) after+=sRt->palRef[j];
        RtTestCheck(before==after);
    }
    RtRemoveFighterSprite(a); RtRemoveFighterSprite(b); RtReleasePal(sRt->statusPalSlot);
    FreeSpriteTilesByTag(RT_STATUS_TAG); RtRestoreBorrowedPals();
    // Check every converted sheet through actual OBJ allocation and DMA copies.
    RtTestCheck(DsMonSpriteCount()==419);
    { u16 available=0;
      for(i=1;i<NUM_SPECIES;i++) if(DsMonSpriteKey(i,0,FALSE)) available++;
      RtTestCheck(available==386); }
    for(i=1;i<=DsMonSpriteCount();i++)
    {
        struct SpritePalette palette; u8 pal,id,dir,phase; bool8 pixelsMatch=TRUE;
        u16 size=DsMonSize(i)*DsMonSize(i)/2;
        palette.data=DsMonPalette(i,FALSE); palette.tag=0x77F2;
        pal=LoadSpritePalette(&palette);
        RtTestCheck(pal<16);
        id=DsCreateMonSprite(i,100,80,pal,0,SpriteCallbackDummy);
        RtTestCheck(id<MAX_SPRITES);
        if(id<MAX_SPRITES)
        {
            for(dir=0;dir<4;dir++) for(phase=0;phase<2;phase++)
            {
                DsUpdateMonSprite(id,i,dir,phase); BuildOamBuffer(); ProcessSpriteCopyRequests();
                if(!RtTestEqual(DsMonFrame(i,dir,phase),(const void *)(OBJ_VRAM0+gSprites[id].oam.tileNum*TILE_SIZE_4BPP),size)) pixelsMatch=FALSE;
            }
            DestroySprite(&gSprites[id]);
        }
        RtTestCheck(pixelsMatch);
        FreeSpritePaletteByTag(0x77F2);
        palette.data=DsMonPalette(i,TRUE); pal=LoadSpritePalette(&palette);
        RtTestCheck(pal<16 && RtTestEqual(palette.data,&gPlttBufferUnfaded[OBJ_PLTT_ID(pal)],32));
        FreeSpritePaletteByTag(0x77F2);
    }
    RtTestReset();
    RtTestCheck(RtProfile(MOVE_QUICK_ATTACK)->reach==40);
    RtTestCheck(RtGetMoveCooldown(MOVE_QUICK_ATTACK)==180 && RtGetMoveRecovery(MOVE_QUICK_ATTACK)==75);
    RtTestCheck(RtGetMoveCooldown(MOVE_SPORE)==360 && RtGetMoveCooldown(MOVE_YAWN)==420);
    RtSetStatus(a,STATUS1_SLEEP_TURN(4)); RtTestCheck(a->statusTimer==240);
    sRt->frameStep=60;
    for(i=0;i<4;i++) RtUpdateStatus(a);
    RtTestCheck(!(a->status&STATUS1_SLEEP) && a->clocks[RTC_SLEEP_GUARD]==180);
    RtTestCheck(!RtInflictStatus(a,STATUS1_SLEEP_TURN(3)));
    RtUpdateClocks(a); RtUpdateClocks(a); RtUpdateClocks(a); RtTestCheck(!a->clocks[RTC_SLEEP_GUARD]);
    a->ability=ABILITY_EARLY_BIRD; RtSetStatus(a,STATUS1_SLEEP_TURN(4)); RtTestCheck(a->statusTimer==120);
    // Rematches preserve original teams, evolve complete level chains and retain legal attacks.
    RtTestReset();
    CreateMon(a->mon,SPECIES_CATERPIE,6,31,TRUE,123,OT_ID_PRESET,0);
    ScaleRematchMon(a->mon,0); RtTestCheck(GetMonData(a->mon,MON_DATA_SPECIES)==SPECIES_CATERPIE && GetMonData(a->mon,MON_DATA_LEVEL)==6);
    ScaleRematchMon(a->mon,1); RtTestCheck(GetMonData(a->mon,MON_DATA_SPECIES)==SPECIES_METAPOD && GetMonData(a->mon,MON_DATA_LEVEL)==7);
    CreateMon(a->mon,SPECIES_CATERPIE,6,31,TRUE,123,OT_ID_PRESET,0);
    ScaleRematchMon(a->mon,4); RtTestCheck(GetMonData(a->mon,MON_DATA_SPECIES)==SPECIES_BUTTERFREE && GetMonData(a->mon,MON_DATA_LEVEL)==10);
    { u8 attacks=0; bool8 tackle=FALSE,confusion=FALSE;
      for(i=0;i<4;i++) { u16 move=GetMonData(a->mon,MON_DATA_MOVE1+i); if(RealtimeRosterDamaging(move)) attacks++; if(move==MOVE_TACKLE)tackle=TRUE; if(move==MOVE_CONFUSION)confusion=TRUE; }
      RtTestCheck(attacks>=2 && tackle && confusion); }
    RtTestCheck(GetMonData(a->mon,MON_DATA_HP)==GetMonData(a->mon,MON_DATA_MAX_HP));
    CreateMon(a->mon,SPECIES_CATERPIE,6,31,TRUE,123,OT_ID_PRESET,0); iv=ITEM_EVERSTONE; SetMonData(a->mon,MON_DATA_HELD_ITEM,&iv);
    ScaleRematchMon(a->mon,99); RtTestCheck(GetMonData(a->mon,MON_DATA_SPECIES)==SPECIES_CATERPIE && GetMonData(a->mon,MON_DATA_LEVEL)==100);
    CreateMon(a->mon,SPECIES_PIKACHU,10,31,TRUE,123,OT_ID_PRESET,0); ScaleRematchMon(a->mon,99);
    RtTestCheck(GetMonData(a->mon,MON_DATA_SPECIES)==SPECIES_PIKACHU && GetMonData(a->mon,MON_DATA_LEVEL)==100);
    { extern const u8 Route3_EventScript_Calvin[],EventScript_ScalingRematch[];
      const u8 *root=Route3_EventScript_Calvin;
      memset(gSaveBlock2Ptr->filler_B20,0,sizeof(gSaveBlock2Ptr->filler_B20));
      ClearTrainerFlag(TRAINER_YOUNGSTER_CALVIN); RtTestCheck(TryGetScalingRematchScript(root)==root);
      SetTrainerFlag(TRAINER_YOUNGSTER_CALVIN); RtTestCheck(TryGetScalingRematchScript(root)==EventScript_ScalingRematch);
      RtTestCheck(!GetScalingRematchCount(TRAINER_YOUNGSTER_CALVIN));
      ScalingRematchCommitLevel(); ScalingRematchCommitLevel(); RtTestCheck(GetScalingRematchCount(TRAINER_YOUNGSTER_CALVIN)==1);
      gBattleTypeFlags=BATTLE_TYPE_TRAINER; CreateRealtimeTrainerParty();
      RtTestCheck(GetMonData(&gEnemyParty[0],MON_DATA_LEVEL)==15);
      CreateRealtimeTrainerParty(); RtTestCheck(GetMonData(&gEnemyParty[0],MON_DATA_LEVEL)==15 && GetScalingRematchCount(TRAINER_YOUNGSTER_CALVIN)==1);
      FinishScalingRematch(); TryGetScalingRematchScript(root); ScalingRematchCommitLevel();
      RtTestCheck(GetScalingRematchCount(TRAINER_YOUNGSTER_CALVIN)==2 && !GetScalingRematchCount(TRAINER_LEADER_BROCK));
      for(i=0;i<105;i++) { FinishScalingRematch(); TryGetScalingRematchScript(root); ScalingRematchCommitLevel(); }
      RtTestCheck(GetScalingRematchCount(TRAINER_YOUNGSTER_CALVIN)==99);
      FinishScalingRematch(); ClearTrainerFlag(TRAINER_YOUNGSTER_CALVIN);
    }
    RtTestReset(); SetMonMoveSlot(b->mon,MOVE_GROWL,0); SetMonMoveSlot(b->mon,MOVE_TACKLE,1);
    SetMonMoveSlot(a->mon,MOVE_TACKLE,0); RtTestCheck(RtAiStatusScore(b,a,MOVE_GROWL)>0);
    a->stages[RT_STAT_ATK]=-2; RtTestCheck(!RtAiStatusScore(b,a,MOVE_GROWL));
    a->clocks[RTC_SLEEP_GUARD]=180; RtTestCheck(!RtAiStatusScore(b,a,MOVE_HYPNOSIS));
    RtTestReset(); SetMonMoveSlot(b->mon,MOVE_THUNDER_SHOCK,0); b->clocks[RTC_ROOT]=180; sRt->aiDifficulty=RT_AI_NORMAL;
    RtUpdateSmartEnemy(); RtTestCheck(b->windup && b->pendingMove==MOVE_THUNDER_SHOCK);
    RtTestCheck(!RtAiStartDash(b,100,0));
    RtTestReset(); SetMonMoveSlot(b->mon,MOVE_SNORE,0); b->status=STATUS1_SLEEP_TURN(3); b->statusTimer=180;
    RtUpdateSmartEnemy(); RtTestCheck(b->windup && b->pendingMove==MOVE_SNORE);
    RtTestReset(); b->clocks[RTC_ROOT]=180; RtLaunchCast(b,a,MOVE_TACKLE); b->cast.elapsed=RtProfile(MOVE_TACKLE)->startup-1;
    normal=b->x; RtUpdateCast(b,a); RtTestCheck(b->x==normal);
    RtTestCheck(!RtSpacingForbidden(MB_NORMAL));
    RtTestCheck(RtSpacingForbidden(MB_JUMP_EAST) && RtSpacingForbidden(MB_JUMP_WEST));
    RtTestCheck(RtSpacingForbidden(MB_JUMP_NORTH) && RtSpacingForbidden(MB_JUMP_SOUTH));
    RtTestCheck(RtSpacingForbidden(MB_WARP_DOOR));
    RtTestWildSpace();
    RtTestPsychic();
    // Enlarged world bounds, gym/Elite Four themes, physical cover and staged destruction.
    RtTestReset(); RtTestCheck(RtIsSpotWalkable(460,240) && !RtIsSpotWalkable(461,240));
    RtTestCheck(RtArenaTheme(TRAINER_LEADER_BROCK)==RATHEME_ROCK && RtArenaTheme(TRAINER_LEADER_MISTY)==RATHEME_WATER);
    RtTestCheck(RtArenaTheme(TRAINER_LEADER_LT_SURGE)==RATHEME_ELECTRIC && RtArenaTheme(TRAINER_LEADER_ERIKA)==RATHEME_GRASS);
    RtTestCheck(RtArenaTheme(TRAINER_LEADER_KOGA)==RATHEME_POISON && RtArenaTheme(TRAINER_LEADER_SABRINA)==RATHEME_PSYCHIC);
    RtTestCheck(RtArenaTheme(TRAINER_LEADER_BLAINE)==RATHEME_FIRE && RtArenaTheme(TRAINER_LEADER_GIOVANNI)==RATHEME_GROUND);
    RtTestCheck(RtArenaTheme(TRAINER_ELITE_FOUR_LORELEI)==RATHEME_ICE && RtArenaTheme(TRAINER_ELITE_FOUR_BRUNO)==RATHEME_FIGHTING);
    RtTestCheck(RtArenaTheme(TRAINER_ELITE_FOUR_AGATHA)==RATHEME_GHOST && RtArenaTheme(TRAINER_ELITE_FOUR_LANCE)==RATHEME_DRAGON);
    for(i=1;i<RATHEME_COUNT;i++){RtArenaLayout(i);RtTestCheck(sRt->arenaObjectCount>0);}
    RtArenaLayout(RATHEME_GRASS);RtTestCheck(!RtArenaLineClear(128,160,352,160) && RtArenaBlocked(240,160));
    RtArenaHitCover(128,160,352,160,MOVE_TACKLE);RtTestCheck(sRt->arenaObjects[2].hp==2);
    RtArenaHitCover(128,160,352,160,MOVE_TACKLE);RtTestCheck(sRt->arenaObjects[2].hp==1);
    RtArenaHitCover(128,160,352,160,MOVE_TACKLE);RtTestCheck(!sRt->arenaObjects[2].hp && RtArenaLineClear(128,160,352,160) && !RtArenaBlocked(240,160));
    RtArenaLayout(RATHEME_ROCK);RtArenaHitCover(128,160,352,160,MOVE_TACKLE);RtTestCheck(sRt->arenaObjects[2].hp==4);
    RtArenaHitCover(128,160,352,160,MOVE_ROCK_SMASH);RtTestCheck(sRt->arenaObjects[2].hp==2);
    RtTestReset();RtArenaLayout(RATHEME_ELECTRIC);sRt->arenaActive=TRUE;sRt->arenaClock=90;sRt->arenaArtPhase=3;
    a->x=Q8(224);a->y=Q8(112);b->x=Q8(400);b->y=Q8(224);RtUpdateArena();RtTestCheck(a->hp==1000);
    sRt->arenaClock=125;sRt->arenaArtPhase=4;RtUpdateArena();RtTestCheck(a->hp==969 && a->clocks[RTC_FLINCH]==12);
    RtUpdateArena();RtTestCheck(a->hp==969 && sRt->arenaContactCooldown[0]>0);
    sRt->arenaContactCooldown[0]=0;a->types[0]=TYPE_GROUND;RtUpdateArena();RtTestCheck(a->hp==969);
    sRt->arenaActive=FALSE;
    // The projection keeps native Pokemon pixels while mapping a doubled arena to the display.
    { struct Sprite sprite;struct OamData oam;u8 slots[9];
      memset(&sprite,0,sizeof(sprite));sprite.y=160;sprite.oam=gOamData_AffineOff_ObjNormal_32x32;
      sprite.oam.x=112;sprite.oam.y=144;oam=sprite.oam;
      for(i=0;i<9;i++){slots[i]=AllocOamMatrix();sRt->arenaMatrices[i]=slots[i];RtTestCheck(slots[i]!=0xFF);}
      sRt->arenaActive=TRUE;RealtimeArenaProjectOam(&sprite,&oam);
      RtTestCheck(oam.x==48 && oam.y==64 && oam.affineMode==ST_OAM_AFFINE_NORMAL && sprite.oam.x==112);
      RtFreeArena();for(i=0;i<9;i++)slots[i]=AllocOamMatrix();RtTestCheck(slots[0]==0);for(i=0;i<9;i++)FreeOamMatrix(slots[i]);
    }
    // Continuous ribbon graphics must release all their sheets, palettes and sprites.
    RtTestReset();RtScanPalSlots(sRt->palKey);
    for(j=0;j<8;j++)
    {
        RtLaunchCast(a,b,MOVE_FLAMETHROWER);a->cast.elapsed=30;RtDrawFlame(a);
        RtTestCheck(a->cast.beamLoaded && GetSpriteTileStartByTag(RT_FLAME_TAG)!=0xFFFF);
        RtCancelCast(a);RtTestCheck(GetSpriteTileStartByTag(RT_FLAME_TAG)==0xFFFF);
    }
    RtRestoreBorrowedPals();
    FlagClear(FLAG_DEFEATED_LORELEI);gTrainerBattleOpponent_A=TRAINER_ELITE_FOUR_LORELEI;
    RtTestCheck(ScalingRematchLeagueProgress()==1 && FlagGet(FLAG_DEFEATED_LORELEI));
    RtTestCheck(!ScalingRematchLeagueProgress());
    FlagClear(FLAG_DEFEATED_LANCE);gTrainerBattleOpponent_A=TRAINER_ELITE_FOUR_LANCE_2;
    RtTestCheck(ScalingRematchLeagueProgress()==2 && FlagGet(FLAG_DEFEATED_LANCE));
    gTrainerBattleOpponent_A=TRAINER_LEADER_BROCK;RtTestCheck(!ScalingRematchLeagueProgress());
    // Retreat cleanup must restore padded object-grid coordinates, while the
    // saved camera origin alone uses coordinates without MAP_OFFSET.
    RtTestReset();
    {
        u8 ids[2];u16 key=DsMonSpriteKey(SPECIES_BULBASAUR,0,FALSE);
        sRt->spaceStarted=TRUE;sRt->spaceActors[0]=0;sRt->spaceActors[1]=1;
        sRt->spaceOriginalX[0]=13;sRt->spaceOriginalY[0]=15;
        sRt->spaceOriginalX[1]=14;sRt->spaceOriginalY[1]=15;
        sRt->spaceBack[0]=DIR_WEST;sRt->spaceBack[1]=DIR_EAST;
        for(i=0;i<2;i++)
        {
            ids[i]=DsCreateMonSprite(key,64+i*32,80,0,0,SpriteCallbackDummy);
            memset(&gObjectEvents[i],0,sizeof(gObjectEvents[i]));
            gObjectEvents[i].active=TRUE;gObjectEvents[i].graphicsId=OBJ_EVENT_GFX_RED_NORMAL;
            gObjectEvents[i].spriteId=ids[i];
        }
        RtRestoreSpacing();
        RtTestCheck(gObjectEvents[0].currentCoords.x==13 && gObjectEvents[0].currentCoords.y==15);
        RtTestCheck(gObjectEvents[1].currentCoords.x==14 && gObjectEvents[1].currentCoords.y==15);
        RtTestCheck(gSaveBlock1Ptr->pos.x==6 && gSaveBlock1Ptr->pos.y==8 && !sRt->spaceStarted);
        for(i=0;i<2;i++){DestroySprite(&gSprites[ids[i]]);gObjectEvents[i].active=FALSE;}
    }
    RtTestPtd();
    RtTestStatusPtd();
    RtTestFinalArenas();
    RtProbeStatusMechanics();
    // A real modal menu remains live for controller-driven replacement tests.
    RtTestReset(); for(i=0;i<RT_MAX_FX;i++) sRt->fx[i].active=FALSE;
    ResetBgsAndClearDma3BusyFlags(FALSE); InitBgsFromTemplates(0,bg,1);
    SetBgTilemapBuffer(0,AllocZeroed(0x800)); InitWindows(empty); DeactivateAllTextPrinters();
    SetMonMoveSlot(a->mon,MOVE_TACKLE,0); SetMonMoveSlot(a->mon,MOVE_SCRATCH,1);
    SetMonMoveSlot(a->mon,MOVE_GROWL,2); SetMonMoveSlot(a->mon,MOVE_CUT,3);
    iv=255; SetMonData(a->mon,MON_DATA_PP_BONUSES,&iv);
    RtCreateHud(); RtShowPlayerHud();
    sRt->state=RT_STATE_INTRO; RtShowMessageNow(sText_GoExcl);
    RtStartFight();
    RtTestCheck(sRt->state==RT_STATE_FIGHT && !sRt->msgVisible && !sRt->transientTimer && !sRt->presentation.active);
    RtShowTransient(sText_StatFell,60);
    RtTestCheck(sRt->msgVisible && sRt->msgCompact && sRt->transientTimer==30);
    RtTestCheck(GetWindowAttribute(sRt->winIds[RT_WIN_MSG],WINDOW_TILEMAP_TOP)==2
             && (GetWindowAttribute(sRt->winIds[RT_WIN_MSG],WINDOW_TILEMAP_TOP)+GetWindowAttribute(sRt->winIds[RT_WIN_MSG],WINDOW_HEIGHT))*8<=RT_ARENA_TOP);
    sRt->frameStep=10; RtUpdateHud(); RtUpdateHud(); RtUpdateHud();
    RtTestCheck(!sRt->msgVisible && !sRt->transientTimer);
    RtShowMessageNow(sText_GoExcl);
    RtTestCheck(!sRt->msgCompact && GetWindowAttribute(sRt->winIds[RT_WIN_MSG],WINDOW_TILEMAP_TOP)==3);
    RtHideMessage(); sRt->frameStep=1;
    sRt->state=RT_STATE_MESSAGES; sRt->afterMsgState=RT_STATE_END;
    sRt->learnMoves[0]=MOVE_EMBER; sRt->learnMoves[1]=MOVE_WATER_GUN; sRt->learnCount=2;
    RtTestCheck(RtStartLearning());
    SetGpuReg(REG_OFFSET_DISPCNT,DISPCNT_OBJ_ON|DISPCNT_OBJ_1D_MAP); ShowBg(0);
    SetVBlankCallback(RtTestVBlank); SetMainCallback2(RtTestUiMain);
    gRtTestReport[0]=0x5254444E;
}
