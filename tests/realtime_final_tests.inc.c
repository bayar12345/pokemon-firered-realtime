static void RtTestFinalArenas(void)
{
    extern const u8 PokemonLeague_LoreleisRoom_EventScript_Lorelei[];
    extern const u8 EventScript_ScalingRematch[];
    static const u16 bosses[]={SPECIES_ARTICUNO,SPECIES_ZAPDOS,SPECIES_MOLTRES,SPECIES_MEWTWO,SPECIES_LUGIA,SPECIES_HO_OH,SPECIES_DEOXYS};
    static const u16 trainers[]={TRAINER_LEADER_BROCK,TRAINER_LEADER_MISTY,TRAINER_LEADER_LT_SURGE,TRAINER_LEADER_ERIKA,TRAINER_LEADER_KOGA,TRAINER_LEADER_SABRINA,TRAINER_LEADER_BLAINE,TRAINER_LEADER_GIOVANNI,TRAINER_ELITE_FOUR_LORELEI,TRAINER_ELITE_FOUR_BRUNO,TRAINER_ELITE_FOUR_AGATHA,TRAINER_ELITE_FOUR_LANCE};
    u16 i,x;u8 j;
    FlagClear(FLAG_SYS_CAN_LINK_WITH_RS);SetTrainerFlag(TRAINER_ELITE_FOUR_LORELEI);SetTrainerFlag(TRAINER_ELITE_FOUR_LORELEI_2);
    RtTestCheck(TryGetScalingRematchScript(PokemonLeague_LoreleisRoom_EventScript_Lorelei)==EventScript_ScalingRematch && gTrainerBattleOpponent_A==TRAINER_ELITE_FOUR_LORELEI);
    FlagSet(FLAG_SYS_CAN_LINK_WITH_RS);
    RtTestCheck(TryGetScalingRematchScript(PokemonLeague_LoreleisRoom_EventScript_Lorelei)==EventScript_ScalingRematch && gTrainerBattleOpponent_A==TRAINER_ELITE_FOUR_LORELEI_2);
    ClearTrainerFlag(TRAINER_ELITE_FOUR_LORELEI_2);
    RtTestCheck(TryGetScalingRematchScript(PokemonLeague_LoreleisRoom_EventScript_Lorelei)==PokemonLeague_LoreleisRoom_EventScript_Lorelei);
    FinishScalingRematch();FlagClear(FLAG_SYS_CAN_LINK_WITH_RS);ClearTrainerFlag(TRAINER_ELITE_FOUR_LORELEI);
    RtTestReset();RtTestCheck(RtApplyStatusMoveEffect(&sRt->enemy,&sRt->player,MOVE_MEAN_LOOK));
    RtTestCheck(sRt->player.noEscapeTimer==600 && !sRt->player.clocks[RTC_TRAP]);
    gMain.newKeysRaw=START_BUTTON;RtTestCheck(RtTryFlee() && sRt->outcome==RT_OUTCOME_NONE);
    RtLeaveField(&sRt->enemy);RtTestCheck(!sRt->player.noEscapeTimer);
    RtTestCheck(RtTryFlee() && sRt->outcome==RT_OUTCOME_RAN);gMain.newKeysRaw=0;
    for(i=0;i<ARRAY_COUNT(bosses);i++)
    {
        RtTestReset();sRt->legendarySpecies=bosses[i];sRt->arenaTheme=RtLegendTheme(bosses[i]);RtLegendLayout(bosses[i]);
        RtTestCheck(sRt->arenaObjectCount>=4 && sRt->arenaObjectCount<=16);
        for(x=32;x<=448;x+=8)RtTestCheck(!RtArenaBlocked(x,160));
        for(j=0;j<sRt->arenaObjectCount;j++)RtTestCheck(sRt->arenaObjects[j].x>=32 && sRt->arenaObjects[j].x<=448 && sRt->arenaObjects[j].y>=64 && sRt->arenaObjects[j].y<=240);
    }
    RtTestReset();sRt->player.types[0]=TYPE_GROUND;
    RtTestCheck(RtArenaHazardImmune(&sRt->player,RAO_ARC));
    sRt->player.types[0]=TYPE_NORMAL;RtTestCheck(!RtArenaHazardImmune(&sRt->player,RAO_ARC));
    for(i=0;i<ARRAY_COUNT(trainers);i++)
    {
        RtTestReset();RtScanPalSlots(sRt->palKey);
        RtCreateTrainerPortrait(0,TRAINER_PIC_RED,20);
        RtCreateTrainerPortrait(1,gTrainers[trainers[i]].trainerPic,220);
        RtTestCheck(sRt->arenaPortrait[0]<MAX_SPRITES && sRt->arenaPortrait[1]<MAX_SPRITES);
        RtTestCheck(sRt->arenaPortraitPal[0]!=sRt->arenaPortraitPal[1]);
        for(j=0;j<2;j++)
        {
            struct Sprite *sprite=&gSprites[sRt->arenaPortrait[j]];
            RtTestCheck(sprite->oam.size==SPRITE_SIZE(64x64));
            RtTestCheck(sprite->oam.paletteNum==sRt->arenaPortraitPal[j]);
        }
        sRt->arenaActive=TRUE;RtFreeArena();
        RtTestCheck(GetSpriteTileStartByTag(0x77C4)==0xFFFF && GetSpriteTileStartByTag(0x77C5)==0xFFFF);
        RtRestoreBorrowedPals();
    }
}

// Record successful status applications under useful conditions. A failure is
// reviewed separately: several moves intentionally fail in a single battle.
static void RtProbeStatusMechanics(void)
{
    u16 move;
    for(move=1;move<MOVES_COUNT;move++)if(!RtDamaging(move))
    {
        gRtTestReport[47]=move;
        gRtTestReport[46]=1;
        RtTestReset();sRt->player.hp=sRt->player.maxHp/2;
        gRtTestReport[46]=2;
        sRt->player.stockpile=2;sRt->enemy.lastMove=MOVE_TACKLE;
        SetMonMoveSlot(sRt->player.mon,MOVE_TACKLE,0);
        SetMonMoveSlot(sRt->enemy.mon,MOVE_TACKLE,0);
        gRtTestReport[46]=3;
        if(RtApplyStatusMoveEffect(&sRt->player,&sRt->enemy,move))gRtTestReport[48+move/32]|=1u<<(move%32);
        gRtTestReport[46]=4;
    }
}
