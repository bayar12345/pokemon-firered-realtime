// Themed battle arenas. Geometry is expressed in world pixels; only the OAM
// output is projected to half scale, preserving move ranges, timing and speed.
static u8 RtLegendTheme(u16 species)
{
    switch(species)
    {
    case SPECIES_ARTICUNO:return RATHEME_ICE;
    case SPECIES_ZAPDOS:case SPECIES_RAIKOU:return RATHEME_ELECTRIC;
    case SPECIES_MOLTRES:case SPECIES_ENTEI:case SPECIES_HO_OH:return RATHEME_FIRE;
    case SPECIES_MEWTWO:case SPECIES_DEOXYS:return RATHEME_PSYCHIC;
    case SPECIES_MEW:case SPECIES_CELEBI:return RATHEME_GRASS;
    case SPECIES_LUGIA:case SPECIES_RAYQUAZA:return RATHEME_DRAGON;
    case SPECIES_SUICUNE:case SPECIES_KYOGRE:return RATHEME_WATER;
    case SPECIES_GROUDON:case SPECIES_REGIROCK:return RATHEME_GROUND;
    case SPECIES_REGICE:return RATHEME_ICE;
    case SPECIES_REGISTEEL:return RATHEME_ROCK;
    case SPECIES_JIRACHI:case SPECIES_LATIAS:case SPECIES_LATIOS:return RATHEME_PSYCHIC;
    default:return 0;
    }
}
static u8 RtArenaTheme(u16 trainer)
{
    switch(trainer)
    {
    case TRAINER_LEADER_BROCK:return RATHEME_ROCK;
    case TRAINER_LEADER_MISTY:return RATHEME_WATER;
    case TRAINER_LEADER_LT_SURGE:return RATHEME_ELECTRIC;
    case TRAINER_LEADER_ERIKA:return RATHEME_GRASS;
    case TRAINER_LEADER_KOGA:return RATHEME_POISON;
    case TRAINER_LEADER_SABRINA:return RATHEME_PSYCHIC;
    case TRAINER_LEADER_BLAINE:return RATHEME_FIRE;
    case TRAINER_LEADER_GIOVANNI:return RATHEME_GROUND;
    case TRAINER_ELITE_FOUR_LORELEI:case TRAINER_ELITE_FOUR_LORELEI_2:return RATHEME_ICE;
    case TRAINER_ELITE_FOUR_BRUNO:case TRAINER_ELITE_FOUR_BRUNO_2:return RATHEME_FIGHTING;
    case TRAINER_ELITE_FOUR_AGATHA:case TRAINER_ELITE_FOUR_AGATHA_2:return RATHEME_GHOST;
    case TRAINER_ELITE_FOUR_LANCE:case TRAINER_ELITE_FOUR_LANCE_2:return RATHEME_DRAGON;
    }
    return gTrainers[trainer].trainerClass==TRAINER_CLASS_CHAMPION?RATHEME_CHAMPION:RATHEME_GENERIC;
}

static void RtArenaAdd(u8 kind,s16 x,s16 y,u8 hp)
{
    struct RtArenaObject *o;
    if(sRt->arenaObjectCount>=ARRAY_COUNT(sRt->arenaObjects))return;
    o=&sRt->arenaObjects[sRt->arenaObjectCount++];
    o->kind=kind; o->x=x; o->y=y; o->hp=o->maxHp=hp;
}

static void RtArenaLayout(u8 theme)
{
    sRt->arenaTheme=theme; sRt->arenaObjectCount=0;
    switch(theme)
    {
    case RATHEME_ROCK:
        RtArenaAdd(RAO_ROCK,192,112,4);RtArenaAdd(RAO_ROCK,288,208,4);
        RtArenaAdd(RAO_ROCK,240,160,4);RtArenaAdd(RAO_ROCK,336,96,4);break;
    case RATHEME_WATER:
        RtArenaAdd(RAO_WATER,192,112,1);RtArenaAdd(RAO_WATER,224,112,1);
        RtArenaAdd(RAO_WATER,256,208,1);RtArenaAdd(RAO_WATER,288,208,1);
        RtArenaAdd(RAO_COLUMN,240,160,4);RtArenaAdd(RAO_COLUMN,336,112,4);break;
    case RATHEME_ELECTRIC:
        RtArenaAdd(RAO_ROD,192,112,1);RtArenaAdd(RAO_ROD,288,208,1);
        RtArenaAdd(RAO_ROD,240,160,1);RtArenaAdd(RAO_ROD,336,112,1);break;
    case RATHEME_GRASS:
        RtArenaAdd(RAO_TREE,192,112,3);RtArenaAdd(RAO_TREE,288,208,3);
        RtArenaAdd(RAO_TREE,240,160,3);RtArenaAdd(RAO_TREE,336,112,3);
        RtArenaAdd(RAO_GRASS,160,208,1);RtArenaAdd(RAO_GRASS,320,96,1);
        RtArenaAdd(RAO_GRASS,224,208,1);RtArenaAdd(RAO_GRASS,256,96,1);break;
    case RATHEME_POISON:
        RtArenaAdd(RAO_COLUMN,192,160,4);RtArenaAdd(RAO_COLUMN,288,160,4);
        RtArenaAdd(RAO_POISON,224,96,1);RtArenaAdd(RAO_POISON,256,208,1);
        RtArenaAdd(RAO_POISON,336,208,1);break;
    case RATHEME_PSYCHIC:
        RtArenaAdd(RAO_COLUMN,240,128,4);RtArenaAdd(RAO_COLUMN,240,208,4);
        RtArenaAdd(RAO_PSYCHIC,192,96,1);RtArenaAdd(RAO_PSYCHIC,288,208,1);break;
    case RATHEME_FIRE:
        RtArenaAdd(RAO_ROCK,192,112,4);RtArenaAdd(RAO_ROCK,288,208,4);
        RtArenaAdd(RAO_FIRE,208,208,1);RtArenaAdd(RAO_FIRE,272,96,1);
        RtArenaAdd(RAO_FIRE,240,160,1);break;
    case RATHEME_GROUND:
        RtArenaAdd(RAO_COLUMN,192,112,4);RtArenaAdd(RAO_COLUMN,288,208,4);
        RtArenaAdd(RAO_COLUMN,240,160,4);RtArenaAdd(RAO_SAND,192,208,1);
        RtArenaAdd(RAO_SAND,288,96,1);break;
    case RATHEME_ICE:
        RtArenaAdd(RAO_ICE,192,112,1);RtArenaAdd(RAO_ICE,224,112,1);
        RtArenaAdd(RAO_ICE,256,208,1);RtArenaAdd(RAO_ICE,288,208,1);
        RtArenaAdd(RAO_COLUMN,240,160,4);break;
    case RATHEME_FIGHTING:
        RtArenaAdd(RAO_ROCK,192,112,4);RtArenaAdd(RAO_ROCK,288,208,4);
        RtArenaAdd(RAO_COLUMN,240,160,4);RtArenaAdd(RAO_COLUMN,336,96,4);break;
    case RATHEME_GHOST:
        RtArenaAdd(RAO_COLUMN,192,112,4);RtArenaAdd(RAO_COLUMN,288,208,4);
        RtArenaAdd(RAO_SHADOW,192,208,1);RtArenaAdd(RAO_SHADOW,288,96,1);
        RtArenaAdd(RAO_SHADOW,240,160,1);break;
    case RATHEME_DRAGON:
        RtArenaAdd(RAO_COLUMN,192,112,4);RtArenaAdd(RAO_COLUMN,288,208,4);
        RtArenaAdd(RAO_WIND,224,208,1);RtArenaAdd(RAO_WIND,256,96,1);break;
    case RATHEME_CHAMPION:
        RtArenaAdd(RAO_TREE,192,112,3);RtArenaAdd(RAO_ROCK,288,208,4);break;
    default:break;
    }
}

static bool8 RtArenaSolid(const struct RtArenaObject *o)
{return o->hp && o->kind<=RAO_ROD;}
static void RtLegendLayout(u16 species)
{
    // Dedicated boss geometry keeps a traversable lane through the middle.
    sRt->arenaObjectCount=0;
    switch(species)
    {
    case SPECIES_ARTICUNO:
        RtArenaAdd(RAO_ICE,192,112,1);RtArenaAdd(RAO_ICE,288,208,1);
        RtArenaAdd(RAO_WIND,240,96,1);RtArenaAdd(RAO_WIND,240,224,1);
        RtArenaAdd(RAO_COLUMN,176,208,3);RtArenaAdd(RAO_COLUMN,304,112,3);break;
    case SPECIES_ZAPDOS:case SPECIES_RAIKOU:
        RtArenaAdd(RAO_ROD,208,112,1);RtArenaAdd(RAO_ROD,272,208,1);
        RtArenaAdd(RAO_ROCK,176,208,3);RtArenaAdd(RAO_ROCK,304,112,3);break;
    case SPECIES_MOLTRES:case SPECIES_ENTEI:case SPECIES_HO_OH:
        RtArenaAdd(RAO_ROCK,208,96,3);RtArenaAdd(RAO_ROCK,272,224,3);
        RtArenaAdd(RAO_FIRE,192,208,1);RtArenaAdd(RAO_FIRE,288,112,1);break;
    case SPECIES_MEWTWO:case SPECIES_DEOXYS:
        RtArenaAdd(RAO_COLUMN,224,112,3);RtArenaAdd(RAO_COLUMN,256,208,3);
        RtArenaAdd(RAO_PSYCHIC,176,96,1);RtArenaAdd(RAO_PSYCHIC,304,224,1);break;
    case SPECIES_LUGIA:case SPECIES_RAYQUAZA:
        RtArenaAdd(RAO_WATER,192,112,1);RtArenaAdd(RAO_WATER,288,208,1);
        RtArenaAdd(RAO_WIND,192,208,1);RtArenaAdd(RAO_WIND,288,112,1);break;
    default:RtArenaLayout(RtLegendTheme(species));break;
    }
}
static bool8 RtArenaBlocked(s32 x,s32 y)
{
    u8 i;
    for(i=0;i<sRt->arenaObjectCount;i++)
        if(RtArenaSolid(&sRt->arenaObjects[i]) && RtAbs(x-sRt->arenaObjects[i].x)<21 && RtAbs(y-sRt->arenaObjects[i].y)<21)return TRUE;
    return FALSE;
}
static s16 RtArenaTrace(s32 x,s32 y,s32 tx,s32 ty)
{
    u16 step,steps=RtLength(tx-x,ty-y)/4+1;u8 i;
    for(step=0;step<=steps;step++)for(i=0;i<sRt->arenaObjectCount;i++)
        if(RtArenaSolid(&sRt->arenaObjects[i])
        && RtAbs(x+(tx-x)*step/steps-sRt->arenaObjects[i].x)<16
        && RtAbs(y+(ty-y)*step/steps-sRt->arenaObjects[i].y)<16)return i;
    return -1;
}
static bool8 RtArenaLineClear(s32 x,s32 y,s32 tx,s32 ty)
{return RtArenaTrace(x,y,tx,ty)<0;}
static bool8 RtArenaHitCover(s32 x,s32 y,s32 tx,s32 ty,u16 move)
{
    s16 index;u8 damage=0,type;
    struct RtArenaObject *o;
    if(!sRt->arena)return FALSE;
    index=RtArenaTrace(x,y,tx,ty);if(index<0)return FALSE;
    o=&sRt->arenaObjects[index];type=gBattleMoves[move].type;
    if(RtDamaging(move))
    {
        if(o->kind==RAO_TREE)damage=type==TYPE_FIRE?2:1;
        if((o->kind==RAO_ROCK || o->kind==RAO_COLUMN)
         && (move==MOVE_ROCK_SMASH || type==TYPE_FIGHTING || type==TYPE_GROUND))damage=2;
        if(damage)
        {
            o->hp=o->hp>damage?o->hp-damage:0;
            RtSpawnFx(o->kind==RAO_TREE?FX_LEAF:FX_ROCK,o->x,o->y,18,FALSE);
            if(sRt->arenaActive)RtDrawArena();
        }
    }
    return TRUE;
}

static u8 RtArenaPhase(void)
{
    u16 time=sRt->arenaClock%240;
    return time>=120 && time<180?2:time>=60 && time<120?1:0;
}
static u16 RtArenaObjectTile(const struct RtArenaObject *o,u8 phase)
{
    u8 stage=o->maxHp?(o->hp*3+o->maxHp-1)/o->maxHp:0;
    switch(o->kind)
    {
    case RAO_ROCK:return RAT_ROCK_0+stage*4;
    case RAO_TREE:return RAT_TREE_0+stage*4;
    case RAO_COLUMN:return RAT_COLUMN_0+stage*4;
    case RAO_ROD:return RAT_ROD_0+phase*4;
    case RAO_ARC:return RAT_SPARK_0+phase*4;
    case RAO_WATER:return RAT_WATER_0+(sRt->arenaClock/30%3)*4;
    case RAO_GRASS:return RAT_GRASS_0;
    case RAO_POISON:return RAT_POISON_0+phase*4;
    case RAO_PSYCHIC:return RAT_PSYCHIC_0+phase*4;
    case RAO_FIRE:return RAT_FIRE_0+phase*4;
    case RAO_SAND:return RAT_SAND_0;
    case RAO_ICE:return RAT_ICE_0;
    case RAO_SHADOW:return RAT_SHADOW_0+phase*4;
    default:return RAT_WIND_0+phase*4;
    }
}
static void RtDrawArena(void)
{
    static const u8 floors[RATHEME_COUNT]={RAT_FLOOR_NEUTRAL,RAT_FLOOR_SAND,RAT_FLOOR_STONE,RAT_FLOOR_STEEL,
      RAT_FLOOR_GARDEN,RAT_FLOOR_POISON,RAT_FLOOR_PSYCHIC,RAT_FLOOR_LAVA,RAT_FLOOR_SAND,RAT_FLOOR_ICE,
      RAT_FLOOR_STONE,RAT_FLOOR_CRYPT,RAT_FLOOR_DRAGON,RAT_FLOOR_NEUTRAL};
    u16 x,y,tile;u8 i,phase=RtArenaPhase();
    for(y=0;y<32;y++)for(x=0;x<32;x++)
    {
        tile=floors[sRt->arenaTheme];
        if(x==0 || x>=29 || y==3 || y>=15)tile=RAT_WALL;
        gBGTilemapBuffers1[y*32+x]=(RT_ARENA_TILE_BASE+tile)|(12<<12);
        gBGTilemapBuffers2[y*32+x]=(RT_ARENA_TILE_BASE+RAT_CLEAR)|(12<<12);
        gBGTilemapBuffers3[y*32+x]=(RT_ARENA_TILE_BASE+RAT_WALL)|(12<<12);
    }
    for(i=0;i<sRt->arenaObjectCount;i++)
    {
        const struct RtArenaObject *o=&sRt->arenaObjects[i];
        s16 left=o->x/16-1,top=o->y/16-1;
        if(o->kind==RAO_ROD && phase)
        {
            s8 dx,dy;
            for(dx=-2;dx<=2;dx+=2)for(dy=-2;dy<=2;dy+=2)
            {
                if((dx==0)==(dy==0))continue;
                for(y=0;y<2;y++)for(x=0;x<2;x++)
                    if(left+dx+x>=0 && left+dx+x<30 && top+dy+y>=0 && top+dy+y<20)
                        gBGTilemapBuffers2[(top+dy+y)*32+left+dx+x]=(RT_ARENA_TILE_BASE+RAT_SPARK_0+phase*4+y*2+x)|(12<<12);
            }
        }
        tile=RT_ARENA_TILE_BASE+RtArenaObjectTile(o,phase);
        for(y=0;y<2;y++)for(x=0;x<2;x++)
            if(left+x>=0 && left+x<30 && top+y>=0 && top+y<20)
                gBGTilemapBuffers2[(top+y)*32+left+x]=(tile+y*2+x)|(12<<12);
    }
    CopyBgTilemapBufferToVram(1);CopyBgTilemapBufferToVram(2);CopyBgTilemapBufferToVram(3);
}

static void RtInitArena(void)
{
    u8 i;
    RtArenaLayout(sRt->legendarySpecies?RtLegendTheme(sRt->legendarySpecies):RtArenaTheme(gTrainerBattleOpponent_A));
    if (sRt->legendarySpecies)
    {
        RtLegendLayout(sRt->legendarySpecies);
        // Clear the centre lane so short-range teams can always reach the boss.
        for(i=0;i<sRt->arenaObjectCount;i++)
            if(sRt->arenaObjects[i].y==160 && RtArenaSolid(&sRt->arenaObjects[i]))sRt->arenaObjects[i].hp=0;
    }
    sRt->arenaClock=0;sRt->arenaArtPhase=0xFF;
    for(i=0;i<9;i++)
    {
        sRt->arenaMatrices[i]=AllocOamMatrix();
        if(sRt->arenaMatrices[i]!=0xFF)SetOamMatrix(sRt->arenaMatrices[i],i&1?-(i<4?512:256):(i<4?512:256),0,0,i&2?-(i<4?512:256):(i<4?512:256));
    }
    SetBgAttribute(1,BG_ATTR_PRIORITY,2);SetBgAttribute(2,BG_ATTR_PRIORITY,3);SetBgAttribute(3,BG_ATTR_PRIORITY,3);
    ShowBg(1);ShowBg(2);ShowBg(3);
    LoadBgTiles(1,sRtArenaTiles,sizeof(sRtArenaTiles),RT_ARENA_TILE_BASE);
    LoadPalette(sRtArenaPalette,BG_PLTT_ID(12),sizeof(sRtArenaPalette));
    RtDrawArena();
}
static void RtFreeArena(void)
{
    u8 i;
    if(!sRt->arenaActive)return;
    for(i=0;i<9;i++)if(sRt->arenaMatrices[i]!=0xFF)FreeOamMatrix(sRt->arenaMatrices[i]);
    for(i=0;i<2;i++) if(sRt->arenaPortrait[i]<MAX_SPRITES)
    {DestroySprite(&gSprites[sRt->arenaPortrait[i]]);FreeSpriteTilesByTag(0x77C4+i);RtReleasePal(sRt->arenaPortraitPal[i]);sRt->arenaPortrait[i]=MAX_SPRITES;}
    FreeSpriteTilesByTag(0x77C3);
    sRt->arenaActive=FALSE;
}

// Rendering changes only the copied OAM entry, never the physics coordinates or
// a sprite's animation state. Shared affine slots keep the per-frame cost small.
void RealtimeArenaProjectOam(const struct Sprite *sprite,struct OamData *oam)
{
    static const u8 widths[3][4]={{8,16,32,64},{16,32,32,64},{8,8,16,32}};
    static const u8 heights[3][4]={{8,16,32,64},{8,8,16,32},{16,32,32,64}};
    s16 x,y,w,h,bias=0,biasX=0;u8 matrix,flip=0,i,j;bool8 native=FALSE;
    if(sRt && sRt->cameraActive && sRt->worldSprites[sprite-gSprites])
    {
        s16 cx=sprite->x+sprite->x2,cy=sprite->y+sprite->y2;
        x=oam->x;y=oam->y;while(x-cx>256)x-=512;while(cx-x>256)x+=512;while(y-cy>128)y-=256;while(cy-y>128)y+=256;
        x-=sRt->cameraX;y-=sRt->cameraY;
        if(x<-96 || x>336 || y<-96 || y>224){oam->affineMode=0;oam->x=240;oam->y=160;return;}
        oam->x=x;oam->y=y;return;
    }
    if(!sRt || !sRt->arenaActive || oam->shape>=3)return;
    for(j=0;j<2;j++)
    {
        struct RtFighter *f=j?&sRt->enemy:&sRt->player;
        if(!f->active)continue;
        if(sprite==&gSprites[f->spriteId] || (f->glowSprite<MAX_SPRITES && sprite==&gSprites[f->glowSprite]))
        {native=TRUE;bias=(8-DsMonAnchorY(f->spriteKey))/2;}
        if(f->arenaShadowLive && sprite==&gSprites[f->arenaShadow])native=TRUE;
        if((f->statusSpriteId<MAX_SPRITES && sprite==&gSprites[f->statusSpriteId])
         || (f->confusionSpriteId<MAX_SPRITES && sprite==&gSprites[f->confusionSpriteId]))
        {native=TRUE;biasX=(sprite->x-FROM_Q8(f->x))/2;bias=(sprite->y-FROM_Q8(f->y))/2;}
        if(f->cast.beamLoaded)for(i=0;i<8;i++)
            if(f->cast.beamSprites[i]<MAX_SPRITES && sprite==&gSprites[f->cast.beamSprites[i]])native=TRUE;
    }
    for(i=0;i<RT_MAX_FX;i++)if(sRt->fx[i].active && sRt->fx[i].ptdAnim
        && sprite==&gSprites[sRt->fx[i].spriteId])native=TRUE;
    w=widths[oam->shape][oam->size];h=heights[oam->shape][oam->size];
    // Unwrap against the actual sprite centre. Large rotated effects can
    // cross -32 or 256, where the old fixed cutoffs wrapped them to the far edge.
    {s16 cx=sprite->x+sprite->x2,cy=sprite->y+sprite->y2;
     if(sprite->coordOffsetEnabled){cx+=gSpriteCoordOffsetX;cy+=gSpriteCoordOffsetY;}
     x=oam->x;y=oam->y;
     while(x-cx>256)x-=512;while(cx-x>256)x+=512;
     while(y-cy>128)y-=256;while(cy-y>128)y+=256;}

    if(oam->affineMode&ST_OAM_AFFINE_ON_MASK)
    {
        struct OamMatrix *m=&gOamMatrices[oam->matrixNum];
        if(native)
        {
            // Keep each attack's private rotation matrix. Sharing the old
            // composition slot made simultaneous beams overwrite each other.
            s16 halfW=oam->affineMode==ST_OAM_AFFINE_DOUBLE?w:w/2;
            s16 halfH=oam->affineMode==ST_OAM_AFFINE_DOUBLE?h:h/2;
            oam->x=(x+halfW)/2-halfW+biasX;oam->y=(y+halfH)/2-halfH+bias;
            return;
        }
        matrix=sRt->arenaMatrices[8];if(matrix==0xFF)return;
        if(oam->affineMode==ST_OAM_AFFINE_DOUBLE){x+=w/2;y+=h/2;}
        SetOamMatrix(matrix,m->a*(native?1:2),m->b*(native?1:2),m->c*(native?1:2),m->d*(native?1:2));
    }
    else
    {
        flip=((oam->matrixNum>>3)&1)|((oam->matrixNum>>3)&2);
        matrix=sRt->arenaMatrices[flip+(native?4:0)];if(matrix==0xFF)return;
    }
    oam->x=(x+w/2)/2-w/2+biasX;oam->y=(y+h/2)/2-h/2+bias;
    oam->affineMode=ST_OAM_AFFINE_NORMAL;oam->matrixNum=matrix;
}

static bool8 RtArenaHazardImmune(const struct RtFighter *f,u8 kind)
{
    if(kind==RAO_WATER)return RtTypeIs((struct RtFighter *)f,TYPE_WATER);
    if(kind==RAO_SAND)return RtTypeIs((struct RtFighter *)f,TYPE_GROUND)||RtTypeIs((struct RtFighter *)f,TYPE_ROCK);
    if(kind==RAO_ICE)return RtTypeIs((struct RtFighter *)f,TYPE_ICE);
    if(kind==RAO_ROD || kind==RAO_ARC)return RtTypeIs((struct RtFighter *)f,TYPE_GROUND);
    if(kind==RAO_FIRE)return RtTypeIs((struct RtFighter *)f,TYPE_FIRE);
    if(kind==RAO_POISON)return RtTypeIs((struct RtFighter *)f,TYPE_POISON)||RtTypeIs((struct RtFighter *)f,TYPE_STEEL);
    if(kind==RAO_SHADOW)return RtTypeIs((struct RtFighter *)f,TYPE_GHOST)||RtTypeIs((struct RtFighter *)f,TYPE_DARK);
    return FALSE;
}
static void RtUpdateArena(void)
{
    u8 i,j,phase;u16 damage;struct RtFighter *f;struct RtArenaObject *o;
    if(!sRt->arenaActive)return;
    sRt->arenaClock+=sRt->frameStep;phase=RtArenaPhase();
    if(sRt->legendarySpecies && !sRt->legendaryPhase && sRt->enemy.active
      && sRt->enemy.hp && sRt->enemy.hp*2<=sRt->enemy.maxHp)
    {
        u8 kind=sRt->arenaTheme==RATHEME_FIRE?RAO_FIRE:sRt->arenaTheme==RATHEME_ELECTRIC?RAO_ROD:
          sRt->arenaTheme==RATHEME_ICE?RAO_WIND:sRt->arenaTheme==RATHEME_PSYCHIC?RAO_SHADOW:RAO_WIND;
        sRt->legendaryPhase=1;sRt->arenaClock=0;sRt->arenaArtPhase=0xFF;
        // New phase hazards occupy the side lanes, never the fighters' centre lane.
        // Rods remain in their original solid positions; phase two uses ground sparks.
        if(kind==RAO_ROD)kind=RAO_ARC;
        RtArenaAdd(kind,160,96,1);RtArenaAdd(kind,320,224,1);
        phase=0;
    }
    if(sRt->arenaArtPhase!=sRt->arenaClock/30%8)
    {sRt->arenaArtPhase=sRt->arenaClock/30%8;RtDrawArena();}
    for(j=0;j<2;j++)
    {
        f=j?&sRt->enemy:&sRt->player;
        sRt->arenaContactCooldown[j]=RtDec(sRt->arenaContactCooldown[j],sRt->frameStep);
        if(!f->active || !f->hp || f->hidden)continue;
        for(i=0;i<sRt->arenaObjectCount;i++)
        {
            o=&sRt->arenaObjects[i];
            if(!o->hp || RtLength(FROM_Q8(f->x)-o->x,FROM_Q8(f->y)-o->y)>(o->kind==RAO_ROD?44:20))continue;
            if(o->kind==RAO_WATER || o->kind==RAO_SAND || o->kind==RAO_ICE)
            {
                if(!(o->kind==RAO_WATER && RtTypeIs(f,TYPE_WATER))
                 && !(o->kind==RAO_SAND && (RtTypeIs(f,TYPE_GROUND)||RtTypeIs(f,TYPE_ROCK)))
                 && !(o->kind==RAO_ICE && RtTypeIs(f,TYPE_ICE)))f->clocks[RTC_SLOW]=45;
                continue;
            }
            if(phase!=2 || sRt->arenaContactCooldown[j] || o->kind<=RAO_COLUMN || o->kind==RAO_GRASS || RtArenaHazardImmune(f,o->kind))continue;
            if(o->kind==RAO_PSYCHIC)
            {
                u8 k;
                for(k=0;k<sRt->arenaObjectCount;k++)
                    if(k!=i && sRt->arenaObjects[k].kind==RAO_PSYCHIC
                     && RtIsSpotWalkable(sRt->arenaObjects[k].x,sRt->arenaObjects[k].y)
                     && RtLength(FROM_Q8((j?sRt->player.x:sRt->enemy.x))-sRt->arenaObjects[k].x,FROM_Q8((j?sRt->player.y:sRt->enemy.y))-sRt->arenaObjects[k].y)>24)
                    {f->x=Q8(sRt->arenaObjects[k].x);f->y=Q8(sRt->arenaObjects[k].y);break;}
                sRt->arenaContactCooldown[j]=240;RtSpawnFx(FX_PSYCHIC,FROM_Q8(f->x),FROM_Q8(f->y),18,FALSE);break;
            }
            if(o->kind==RAO_WIND)
            {
                RtMoveFighter(f,Q8(j?-8:8),0);sRt->arenaContactCooldown[j]=60;RtSpawnFx(FX_WIND,o->x,o->y,18,FALSE);continue;
            }
            damage=f->maxHp/32;if(!damage)damage=1;
            if((o->kind==RAO_ROD || o->kind==RAO_ARC) && RtTypeIs(f,TYPE_ELECTRIC))damage=damage>1?damage/2:1;
            RtSetHp(f,f->hp>damage?f->hp-damage:0);
            if(o->kind==RAO_ROD || o->kind==RAO_ARC)f->clocks[RTC_FLINCH]=12;
            sRt->arenaContactCooldown[j]=90;
            RtSpawnFx(o->kind==RAO_ROD || o->kind==RAO_ARC?FX_SPARK:o->kind==RAO_FIRE?FX_EMBER:FX_POISON,FROM_Q8(f->x),FROM_Q8(f->y),18,FALSE);
        }
    }
}

static s32 RtArenaDangerAt(struct RtFighter *f,s32 x,s32 y)
{
    u8 i; s32 danger=0; struct RtArenaObject *o;
    if(!sRt->arenaActive)return 0;
    for(i=0;i<sRt->arenaObjectCount;i++)
    {
        o=&sRt->arenaObjects[i];
        if(RtArenaHazardImmune(f,o->kind))continue;
        if(o->kind==RAO_WATER || o->kind==RAO_SAND || o->kind==RAO_ICE)
        {if(RtLength(x-o->x,y-o->y)<32)danger+=30;continue;}
        if(!RtArenaPhase() || o->kind<=RAO_COLUMN || o->kind==RAO_GRASS)continue;
        if(RtLength(x-o->x,y-o->y)<(o->kind==RAO_ROD?60:36))danger+=110;
    }
    return danger;
}
static bool8 RtArenaCanBreakLine(struct RtFighter *f,struct RtFighter *target,u16 move)
{
    s16 index;u8 type;
    if(!sRt->arenaActive || !RtDamaging(move))return FALSE;
    index=RtArenaTrace(FROM_Q8(f->x),FROM_Q8(f->y),FROM_Q8(target->x),FROM_Q8(target->y));
    if(index<0)return FALSE;type=gBattleMoves[move].type;
    return sRt->arenaObjects[index].kind==RAO_TREE || (sRt->arenaObjects[index].kind!=RAO_ROD && (move==MOVE_ROCK_SMASH || type==TYPE_FIGHTING || type==TYPE_GROUND));
}

static const u32 sRtArenaShadowTiles[] = {0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x20000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x11112220,0x11111112,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000002,0x22211111,0x11111111,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000022,0x22000000,0x20000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x11111111,0x33331112,0x11112220,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x11111111,0x11133333,0x22211111,0x00000002,0x00000000,0x00000000,0x00000000,0x00000000,0x00000221,0x00000022,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x20000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x11112220,0x11111112,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000002,0x22211111,0x11111111,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000022,0x22000000,0x20000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x11111111,0x44441112,0x11112220,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x11111111,0x11144444,0x22211111,0x00000002,0x00000000,0x00000000,0x00000000,0x00000000,0x00000221,0x00000022,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000};
static const u16 sRtArenaShadowPal[16] = {0,RGB(3,4,6),RGB(29,29,23),RGB(6,18,31),RGB(31,14,5)};

static void RtCreateArenaShadow(struct RtFighter *f)
{
    struct SpriteSheet sheet={sRtArenaShadowTiles,sizeof(sRtArenaShadowTiles),0x77C3};
    struct SpriteTemplate t;u8 pal;
    if(!sRt->arenaActive)return;
    if(GetSpriteTileStartByTag(sheet.tag)==0xFFFF)LoadSpriteSheet(&sheet);
    if(GetSpriteTileStartByTag(sheet.tag)==0xFFFF)return;
    pal=RtAcquirePal(0x601,sRtArenaShadowPal);if(pal==0xFF)return;
    t.tileTag=sheet.tag;t.paletteTag=TAG_NONE;t.oam=&gOamData_AffineOff_ObjNormal_32x16;
    t.anims=gDummySpriteAnimTable;t.images=NULL;t.affineAnims=gDummySpriteAffineAnimTable;t.callback=SpriteCallbackDummy;
    f->arenaShadow=CreateSprite(&t,FROM_Q8(f->x),FROM_Q8(f->y)+4,0x78);
    if(f->arenaShadow>=MAX_SPRITES){RtReleasePal(pal);return;}
    f->arenaShadowLive=TRUE;if(f->isEnemy){gSprites[f->arenaShadow].sheetTileStart+=8;gSprites[f->arenaShadow].oam.tileNum+=8;}gSprites[f->arenaShadow].oam.paletteNum=pal;gSprites[f->arenaShadow].oam.priority=2;
}
static void RtRemoveArenaShadow(struct RtFighter *f)
{
    if(!f->arenaShadowLive)return;
    RtReleasePal(gSprites[f->arenaShadow].oam.paletteNum);DestroySprite(&gSprites[f->arenaShadow]);f->arenaShadowLive=FALSE;
}
