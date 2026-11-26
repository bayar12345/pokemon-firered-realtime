extern u8 gSpriteTileAllocBitmap[128];
extern u32 gOamMatrixAllocBitmap;

static void RtTestPtd(void)
{
    u16 move,i,count=0;
    const struct RtPtdMoveArt *art;
    const struct RtPtdAnim *a;
    struct RtFighter *f,*enemy;
    static const u16 species[]={SPECIES_BULBASAUR,SPECIES_CHARIZARD,SPECIES_BLASTOISE,SPECIES_PIKACHU,SPECIES_ONIX,SPECIES_GYARADOS,SPECIES_DRAGONITE,SPECIES_SNORLAX};
    static const s16 aim[4][2]={{256,0},{-256,0},{0,-256},{0,256}};
    for(move=1;move<MOVES_COUNT;move++)
    {
        art=&sRtPtdMoveArt[move];
        if(RtDamaging(move))
        {
            count++;RtTestCheck(art->impact>0 && art->impact<ARRAY_COUNT(sRtPtdAnims));
            if(RtProfile(move)->kind==RT_SHOT)RtTestCheck(art->shot>0);
            if(RtProfile(move)->kind==RT_BEAM)RtTestCheck(art->beam>0 || move==MOVE_EARTH_POWER);
        }
        RtTestCheck(art->shot<ARRAY_COUNT(sRtPtdAnims) && art->impact<ARRAY_COUNT(sRtPtdAnims)
                 && art->cast<ARRAY_COUNT(sRtPtdAnims) && art->beam<ARRAY_COUNT(sRtPtdAnims) && art->beamArena<ARRAY_COUNT(sRtPtdAnims));
    }
    RtTestCheck(count==218);
    RtTestCheck(!RtPtdHasArt(MOVE_SWORDS_DANCE) && !RtPtdHasArt(MOVE_GROWL) && !RtPtdHasArt(MOVE_SLEEP_POWDER));
    for(i=1;i<ARRAY_COUNT(sRtPtdAnims);i++)
    {
        a=&sRtPtdAnims[i];
        RtTestCheck(a->frames>0 && (a->fps==21 || a->fps==30) && a->frameBytes==a->width*a->height/2);
        RtTestCheck(((u32)a->pixels&3)==0 && (u32)a->pixels>=0x08000000
                 && (u32)a->pixels+a->frames*a->frameBytes<=0x0A000000);
        RtTestCheck(a->holdStart<=a->holdEnd && a->holdEnd<a->frames);
    }
    RtTestCheck(sRtPtdAnims[sRtPtdMoveArt[MOVE_ICE_BEAM].beam].pixels==sRtPtdAnims[sRtPtdMoveArt[MOVE_HYPER_BEAM].beam].pixels);
    RtTestCheck(sRtPtdAnims[sRtPtdMoveArt[MOVE_ICE_BEAM].beam].palette!=sRtPtdAnims[sRtPtdMoveArt[MOVE_HYPER_BEAM].beam].palette);
    // Mouth/front positions use actual pose pixels. Arena positions compensate
    // for native-size Pokemon so the emitter remains attached after zooming.
    RtTestReset();f=&sRt->player;f->x=Q8(128);f->y=Q8(160);
    for(i=0;i<ARRAY_COUNT(species);i++)
    {
        u8 d;
        f->spriteKey=DsMonSpriteKey(species[i],0,FALSE);
        for(d=0;d<4;d++)
        {
            s16 x,y,ax,ay;u8 front,afront;
            sRt->arena=FALSE;front=RtPtdEmitter(f,aim[d][0],aim[d][1],&x,&y);
            RtTestCheck(front>=6 && front<=24 && (d!=0 || x>128) && (d!=1 || x<128) && y<=168 && (d==3 || y<160));
            sRt->arena=TRUE;afront=RtPtdEmitter(f,aim[d][0],aim[d][1],&ax,&ay);
            RtTestCheck(ax==128+(x-128)*2 && ay==160+(y-160)*2 && afront==front*2);
        }
    }
    // Animation instances and simultaneous beams must return every tile,
    // palette reference and private rotation matrix they acquire.
    RtTestReset();ResetSpriteData();FreeAllSpritePalettes();RtScanPalSlots(sRt->palKey);
    f=&sRt->player;enemy=&sRt->enemy;
    for(i=0;i<RT_MAX_FX;i++)sRt->fx[i].active=FALSE;
    {
        u8 before[128],palBefore[16];u32 matrixBefore=gOamMatrixAllocBitmap;
        memcpy(before,gSpriteTileAllocBitmap,sizeof(before));memcpy(palBefore,sRt->palRef,16);
        for(i=1;i<ARRAY_COUNT(sRtPtdAnims);i++)if(sRtPtdAnims[i].columns==1 && sRtPtdAnims[i].height==sRtPtdAnims[i].width)
        {
            u8 id=RtPtdSpawn(i,64,80,0,FALSE);
            RtTestCheck(id<RT_MAX_FX);
            if(id<RT_MAX_FX)
            {
                RtTestCheck(sRt->fx[id].life>=12);
                RtPtdOrient(id,181,181,256,256);sRt->frameStep=2;RtPtdUpdateFx(id);
                RtDestroyFx(id);
            }
        }
        sRt->frameStep=1;
        RtTestCheck(RtTestEqual(before,gSpriteTileAllocBitmap,sizeof(before)) && RtTestEqual(palBefore,sRt->palRef,16) && gOamMatrixAllocBitmap==matrixBefore);
        f->spriteKey=enemy->spriteKey=DsMonSpriteKey(SPECIES_CHARIZARD,0,FALSE);
        RtLaunchCast(f,enemy,MOVE_FLAMETHROWER);f->cast.elapsed=30;RtDrawFlame(f);
        RtLaunchCast(enemy,f,MOVE_ICE_BEAM);enemy->cast.elapsed=30;RtDrawFlame(enemy);
        RtTestCheck(f->cast.beamLoaded && enemy->cast.beamLoaded && f->cast.beamMatrix!=enemy->cast.beamMatrix);
        if(f->cast.beamLoaded && enemy->cast.beamLoaded)
        {
            struct OamData first=gSprites[f->cast.beamSprites[0]].oam,second=gSprites[enemy->cast.beamSprites[0]].oam;
            sRt->arenaActive=TRUE;
            RealtimeArenaProjectOam(&gSprites[f->cast.beamSprites[0]],&first);
            RealtimeArenaProjectOam(&gSprites[enemy->cast.beamSprites[0]],&second);
            RtTestCheck(first.matrixNum==f->cast.beamMatrix && second.matrixNum==enemy->cast.beamMatrix
                     && first.affineMode==ST_OAM_AFFINE_DOUBLE && second.affineMode==ST_OAM_AFFINE_DOUBLE);
        }
        RtCancelCast(f);RtCancelCast(enemy);
        RtTestCheck(RtTestEqual(before,gSpriteTileAllocBitmap,sizeof(before)) && RtTestEqual(palBefore,sRt->palRef,16) && gOamMatrixAllocBitmap==matrixBefore);
    }
    RtTestReset();f=&sRt->player;enemy=&sRt->enemy;
    RtArenaLayout(RATHEME_ROCK);f->x=Q8(160);f->y=Q8(160);enemy->x=Q8(280);enemy->y=Q8(160);
    RtTestCheck(!RtPtdRayClear(160,160,280,160) && RtPtdRayLimit(f,256,0,144)<80);
    RtArenaLayout(RATHEME_GENERIC);
    RtTestCheck(RtPtdRayClear(160,160,280,160) && RtPtdRayLimit(f,256,0,144)==144);
    sRt->weather=RTW_SUN;RtLaunchCast(f,enemy,MOVE_SOLAR_BEAM);
    RtTestCheck(f->cast.nextHit==RtProfile(MOVE_SOLAR_BEAM)->startup/2+6);
    RtTestCheck(f->cast.end-RtProfile(MOVE_SOLAR_BEAM)->active==RtProfile(MOVE_SOLAR_BEAM)->startup/2);
    RtCancelCast(f);
    RtRestoreBorrowedPals();ResetSpriteData();
}
