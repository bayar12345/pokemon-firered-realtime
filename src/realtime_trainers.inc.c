#include "data/realtime_arena_graphics.h"

// Add individual trainer IDs here to opt additional encounters into arenas.
// Gym Leaders, Elite Four, Champion and rivals are selected by class below.
static const u16 sExtraArenaTrainers[] = {0xFFFF};
static const u8 sText_TrainerIntro[] = _("{STR_VAR_1} challenges you!");
static const u8 sText_TrainerGo[] = _("{STR_VAR_1}: Go, {STR_VAR_2}!");
static const u8 sText_TrainerWon[] = _("You defeated {STR_VAR_1}!");
static const u8 sText_PrizeMoney[] = _("You received {STR_VAR_1} prize money!");
static const u8 sText_CantRunTrainer[] = _("You can't run from a trainer battle!");

static void RtGetTrainerName(u8 *out)
{
    u8 c = gTrainers[gTrainerBattleOpponent_A].trainerClass;
    if (c == TRAINER_CLASS_RIVAL_EARLY || c == TRAINER_CLASS_RIVAL_LATE || c == TRAINER_CLASS_CHAMPION)
        StringCopy(out, gSaveBlock1Ptr->rivalName);
    else StringCopy(out, gTrainers[gTrainerBattleOpponent_A].trainerName);
}

static bool8 RtTrainerNeedsArena(u16 id)
{
    u16 i;
    u8 c = gTrainers[id].trainerClass;
    if (c == TRAINER_CLASS_LEADER || c == TRAINER_CLASS_ELITE_FOUR || c == TRAINER_CLASS_CHAMPION
     || c == TRAINER_CLASS_RIVAL_EARLY || c == TRAINER_CLASS_RIVAL_LATE) return TRUE;
    for (i = 0; sExtraArenaTrainers[i] != 0xFFFF; i++) if (sExtraArenaTrainers[i] == id) return TRUE;
    return FALSE;
}

static u16 RtTrainerGraphics(void)
{
    switch (gTrainerBattleOpponent_A)
    {
    case TRAINER_LEADER_BROCK: return OBJ_EVENT_GFX_BROCK;
    case TRAINER_LEADER_MISTY: return OBJ_EVENT_GFX_MISTY;
    case TRAINER_LEADER_LT_SURGE: return OBJ_EVENT_GFX_LT_SURGE;
    case TRAINER_LEADER_ERIKA: return OBJ_EVENT_GFX_ERIKA;
    case TRAINER_LEADER_KOGA: return OBJ_EVENT_GFX_KOGA;
    case TRAINER_LEADER_BLAINE: return OBJ_EVENT_GFX_BLAINE;
    case TRAINER_LEADER_SABRINA: return OBJ_EVENT_GFX_SABRINA;
    case TRAINER_LEADER_GIOVANNI: return OBJ_EVENT_GFX_GIOVANNI;
    case TRAINER_ELITE_FOUR_LORELEI:
    case TRAINER_ELITE_FOUR_LORELEI_2: return OBJ_EVENT_GFX_LORELEI;
    case TRAINER_ELITE_FOUR_BRUNO:
    case TRAINER_ELITE_FOUR_BRUNO_2: return OBJ_EVENT_GFX_BRUNO;
    case TRAINER_ELITE_FOUR_AGATHA:
    case TRAINER_ELITE_FOUR_AGATHA_2: return OBJ_EVENT_GFX_AGATHA;
    case TRAINER_ELITE_FOUR_LANCE:
    case TRAINER_ELITE_FOUR_LANCE_2: return OBJ_EVENT_GFX_LANCE;
    }
    return OBJ_EVENT_GFX_BLUE;
}

static void RtArenaTrainerSprite(u8 id, s16 x, u8 anim)
{
    if (id == MAX_SPRITES) return;
    gSprites[id].callback = SpriteCallbackDummy;
    gSprites[id].coordOffsetEnabled = FALSE;
    gSprites[id].x = x*2; gSprites[id].y = 160;
    gSprites[id].x2 = gSprites[id].y2 = 0;
    gSprites[id].oam.priority = 2;
    gSprites[id].invisible = FALSE;
    StartSpriteAnim(&gSprites[id], anim);
}

static const struct OamData sRtTrainerPortraitOam={.shape=SPRITE_SHAPE(64x64),.size=SPRITE_SIZE(64x64),.priority=2};
static void RtCreateTrainerPortrait(u8 side,u16 pic,s16 x)
{
    struct SpriteSheet sheet;
    struct SpriteTemplate template={0};
    u8 id,pal; u8 *pixels=Alloc(0x800);
    if (!pixels) return;
    pal=RtAcquireCompressedPal(0x700+pic,gTrainerFrontPicPaletteTable[pic].data);
    if (pal==0xFF) { Free(pixels); return; }
    LZDecompressWram(gTrainerFrontPicTable[pic].data,pixels);
    sheet.data=pixels;sheet.size=0x800;sheet.tag=0x77C4+side;
    if (LoadSpriteSheet(&sheet)==0xFFFF) { RtReleasePal(pal); Free(pixels); return; }
    Free(pixels);
    template.tileTag=sheet.tag;template.paletteTag=TAG_NONE;
    template.oam=&sRtTrainerPortraitOam;template.anims=gDummySpriteAnimTable;
    template.affineAnims=gDummySpriteAffineAnimTable;template.callback=SpriteCallbackDummy;
    id=CreateSprite(&template,x*2,160,0);
    if(id==MAX_SPRITES) { FreeSpriteTilesByTag(sheet.tag); RtReleasePal(pal); return; }
    gSprites[id].oam.paletteNum=pal;
    sRt->arenaPortrait[side]=id;sRt->arenaPortraitPal[side]=pal;
}

static void RtLegendAfterEvolution(void)
{
    MainCallback cb=sRtLegendEnd;sRtLegendEnd=NULL;SetMainCallback2(cb);
}
static void RtFinishLegend(u8 taskId)
{
    MainCallback cb=sRtLegendEnd;sRtLegendEnd=NULL;
    RtCleanup();DestroyTask(taskId);CleanupOverworldWindowsAndTilemaps();SetMainCallback2(cb);
}

static void RtSetupArena(void)
{
    u8 i, player = gObjectEvents[gPlayerAvatar.objectEventId].spriteId, opponent = MAX_SPRITES;
    if (gSelectedObjectEvent < OBJECT_EVENTS_COUNT && gObjectEvents[gSelectedObjectEvent].active
     && gSelectedObjectEvent != gPlayerAvatar.objectEventId)
        opponent = gObjectEvents[gSelectedObjectEvent].spriteId;
    for (i = 0; i < MAX_SPRITES; i++)
    {
        if (gSprites[i].inUse) { gSprites[i].invisible = TRUE; gSprites[i].callback = SpriteCallbackDummy; }
    }
    if (opponent == MAX_SPRITES) opponent = CreateObjectGraphicsSprite(RtTrainerGraphics(), SpriteCallbackDummy, 224, 80, 0);
    RtArenaTrainerSprite(player, 16, ANIM_STD_FACE_EAST);
    RtArenaTrainerSprite(opponent, 224, ANIM_STD_FACE_WEST);
    if (!sRt->isTrainer && opponent<MAX_SPRITES) gSprites[opponent].invisible=TRUE;
    RtInitArena();
    sRt->arenaActive = TRUE;
    // The trainer sprite may have introduced a palette; rescan before any
    // battle palettes are acquired, so no overworld actor is recolored.
    RtScanPalSlots(sRt->palKey);
    RtCreateTrainerPortrait(0,gSaveBlock2Ptr->playerGender?TRAINER_PIC_LEAF:TRAINER_PIC_RED,20);
    if(sRt->isTrainer) RtCreateTrainerPortrait(1,gTrainers[gTrainerBattleOpponent_A].trainerPic,220);
    if(sRt->arenaPortrait[0]<MAX_SPRITES) gSprites[player].invisible=TRUE;
    if(sRt->arenaPortrait[1]<MAX_SPRITES && opponent<MAX_SPRITES) gSprites[opponent].invisible=TRUE;
}

static s8 RtNextEnemy(void)
{
    u8 i;
    for (i = 0; i < gEnemyPartyCount; i++) if (RtIsUsableMon(&gEnemyParty[i])) return i;
    return -1;
}

static void RtSwitchEnemy(void)
{
    s8 next = sRt->forcedEnemy!=0xFF?sRt->forcedEnemy:RtNextEnemy();
    sRt->forcedEnemy=0xFF;
    if (next < 0) { sRt->state = RT_STATE_END; return; }
    if(sRt->enemy.active) RtRemoveFighterSprite(&sRt->enemy);
    RtInitFighter(&sRt->enemy, &gEnemyParty[next], TRUE, sRt->enemySpawnX, sRt->enemySpawnY, next);
    sRt->expParticipantsMask=1<<sRt->player.partyIndex;
    if(sRt->enemyBaton) { memcpy(sRt->enemy.stages,sRt->batonStages,sizeof(sRt->batonStages)); sRt->enemyBaton=FALSE; }
    sRt->enemy.cooldown = sRt->enemy.recoveryMax = 45;
    RtFace(&sRt->enemy, &sRt->player);
    sRt->dirtyEnemy = TRUE;
    RtGetTrainerName(gStringVar1);
    RtGetNickname(sRt->enemy.mon, gStringVar2);
    RtShowTransientExpanded(sText_TrainerGo, 60);
    RtBeginPresentation(&sRt->enemy,RTP_SEND);
    sRt->afterPresentation=RT_STATE_FIGHT;
    sRt->state = RT_STATE_PRESENTATION;
}

static void RtTrainerReward(void)
{
    u32 reward;
    u16 i;
    u8 value = 1, lastLevel = GetMonData(&gEnemyParty[gEnemyPartyCount - 1], MON_DATA_LEVEL), multiplier = 1;
    for (i = 0; gTrainerMoneyTable[i].classId != 0xFF; i++)
        if (gTrainerMoneyTable[i].classId == gTrainers[gTrainerBattleOpponent_A].trainerClass) { value = gTrainerMoneyTable[i].value; break; }
    for (i = 0; i < PARTY_SIZE; i++)
        if ((sRt->participantsMask & (1 << i)) && GetMonData(&gPlayerParty[i], MON_DATA_HELD_ITEM) == ITEM_AMULET_COIN) multiplier = 2;
    reward = 4 * lastLevel * value * multiplier * ((gBattleTypeFlags & BATTLE_TYPE_DOUBLE) ? 2 : 1);
    AddMoney(&gSaveBlock1Ptr->money, reward);
    RtGetTrainerName(gStringVar1);
    RtQueueMessage(sText_TrainerWon);
    ConvertIntToDecimalStringN(gStringVar1, reward, STR_CONV_MODE_LEFT_ALIGN, 6);
    RtQueueMessage(sText_PrizeMoney);
}

static void RtTrainerAfterEvolution(void)
{
    MainCallback cb = sRtTrainerEnd;
    sRtTrainerEnd = NULL;
    SetMainCallback2(cb);
}

static void RtFinishTrainer(u8 taskId, u8 outcome)
{
    MainCallback cb = sRtTrainerEnd;
    gBattleOutcome = outcome;
    sRtTrainerEnd = NULL;
    RtCleanup();
    DestroyTask(taskId);
    CleanupOverworldWindowsAndTilemaps();
    SetMainCallback2(cb);
}

// Preserve the original trainer's defeat/victory dialogue with the native
// field printer, including its page breaks. Combat has already ended here.
static bool8 RtBeginFinalTrainerSpeech(u8 nextState)
{
    const u8 *text;
    u8 *copy;
    bool8 shown;
    if (sRt->trainerSpeechShown) return FALSE;
    text = nextState == RT_STATE_WHITEOUT ? GetTrainerWonSpeech() : GetTrainerALoseText();
    if (*text == EOS) { sRt->trainerSpeechShown = TRUE; return FALSE; }
    copy = Alloc(StringLength(text) + 1);
    if (copy == NULL) { sRt->trainerSpeechShown = TRUE; return FALSE; }
    StringCopy(copy, text);
    shown = ShowFieldMessage(copy);
    Free(copy);
    if (!shown) return TRUE;
    sRt->trainerSpeechShown = TRUE;
    sRt->afterSpeechState = nextState;
    sRt->state = RT_STATE_TRAINER_SPEECH;
    return TRUE;
}
