#define RT_PTD_FX_TAG 0x77D0
#define RT_PTD_PAL_KEY 0x800

static bool8 RtPtdHasArt(u16 move)
{ return move < MOVES_COUNT && sRtPtdMoveArt[move].impact != 0; }

static void RtPtdBody(struct RtFighter *f, s16 *x, s16 *y)
{
    s16 scale=sRt->arena?2:1;
    *x=FROM_Q8(f->x);
    *y=FROM_Q8(f->y)+scale*(8-DsMonAnchorY(f->spriteKey));
}

// Use the authored walking silhouette instead of emitting from the feet or
// screen centre. Side-facing poses use the forward edge of the face band.
static u8 RtPtdEmitter(struct RtFighter *f, s32 dx, s32 dy, s16 *x, s16 *y)
{
    u8 size=DsMonSize(f->spriteKey),dir=RtDirFromVector(dx,dy);
    s16 mx=size/2,my=size/2,scale=sRt->arena?2:1,front;
    if(f->spriteKey && f->spriteKey<ARRAY_COUNT(sPtdMuzzles))
    {mx=sPtdMuzzles[f->spriteKey][dir][0];my=sPtdMuzzles[f->spriteKey][dir][1];}
    else {mx=size/2+dx*10/256;my=size/2+dy*10/256;}
    *x=FROM_Q8(f->x)+scale*(mx-size/2);
    *y=FROM_Q8(f->y)+scale*(8-DsMonAnchorY(f->spriteKey)+my-size/2);
    front=((mx-size/2)*dx+(my-size/2)*dy)/256;
    if(front<6)front=6;if(front>24)front=24;
    return front*scale;
}

static bool8 RtPtdRayClear(s16 x,s16 y,s16 tx,s16 ty)
{
    s32 dx=tx-x,dy=ty-y,dist=RtLength(dx,dy),i;
    s16 mx,my,startX,startY;
    if(sRt->arena)return RtArenaLineClear(x,y,tx,ty);
    if(!dist)return TRUE;
    RtScreenToTile(x,y+4,&startX,&startY);
    for(i=4;i<=dist;i+=4)
    {
        RtScreenToTile(x+dx*i/dist,y+dy*i/dist+4,&mx,&my);
        if(mx==startX && my==startY)continue;
        if(MapGridGetCollisionAt(mx,my)
         && !(TestPlayerAvatarFlags(PLAYER_AVATAR_FLAG_SURFING)&&MetatileBehavior_IsSurfable(MapGridGetMetatileBehaviorAt(mx,my))))return FALSE;
    }
    return TRUE;
}

static s16 RtPtdRayLimit(struct RtFighter *f,s32 dx,s32 dy,s16 reach)
{
    s16 dist,x,y,mx,my,tx,ty,cover=-1,originX=FROM_Q8(f->x),originY=FROM_Q8(f->y);
    if(sRt->arena)cover=RtArenaTrace(originX,originY,originX+dx*reach/256,originY+dy*reach/256);
    RtScreenToTile(originX,originY+4,&tx,&ty);
    for(dist=4;dist<=reach;dist+=4)
    {
        x=originX+dx*dist/256;y=originY+dy*dist/256;
        if(sRt->arena)
        {
            if(x<RT_ARENA_LEFT*2 || x>RT_ARENA_RIGHT*2 || y<RT_ARENA_TOP*2 || y>RT_ARENA_BOTTOM*2)break;
            if(cover>=0 && RtAbs(x-sRt->arenaObjects[cover].x)<16 && RtAbs(y-sRt->arenaObjects[cover].y)<16)break;
        }
        else
        {
            RtScreenToTile(x,y+4,&mx,&my);
            if((mx!=tx || my!=ty) && MapGridGetCollisionAt(mx,my)
             && !(TestPlayerAvatarFlags(PLAYER_AVATAR_FLAG_SURFING)&&MetatileBehavior_IsSurfable(MapGridGetMetatileBehaviorAt(mx,my))))break;
        }
        if(RtFieldBlocks(f,x,y))break;
    }
    return dist-4;
}

static u8 RtPtdSpawn(u16 anim,s16 x,s16 y,u8 life,bool8 loop)
{
    const struct RtPtdAnim *a;
    struct SpriteSheet sheet;
    struct SpriteTemplate t;
    struct RtFx *fx;
    u8 i,id,pal;
    if(!anim || anim>=ARRAY_COUNT(sRtPtdAnims))return 0xFF;
    a=&sRtPtdAnims[anim];
    for(i=0;i<RT_MAX_FX;i++)if(!sRt->fx[i].active)break;
    if(i==RT_MAX_FX)return 0xFF;
    pal=RtAcquirePal(RT_PTD_PAL_KEY+a->paletteKey,a->palette);
    if(pal==0xFF)return 0xFF;
    sheet.data=a->pixels;sheet.size=a->frameBytes;sheet.tag=RT_PTD_FX_TAG+i;
    LoadSpriteSheet(&sheet);
    if(GetSpriteTileStartByTag(sheet.tag)==0xFFFF){RtReleasePal(pal);return 0xFF;}
    t.tileTag=sheet.tag;t.paletteTag=TAG_NONE;
    t.oam=a->width==64?&gOamData_AffineOff_ObjNormal_64x64:&gOamData_AffineOff_ObjNormal_32x32;
    t.anims=gDummySpriteAnimTable;t.images=NULL;t.affineAnims=gDummySpriteAffineAnimTable;t.callback=SpriteCallbackDummy;
    id=CreateSprite(&t,x,y,0x28);
    if(id==MAX_SPRITES){FreeSpriteTilesByTag(sheet.tag);RtReleasePal(pal);return 0xFF;}
    gSprites[id].oam.paletteNum=pal;gSprites[id].oam.priority=1;
    gSprites[id].animPaused=TRUE;gSprites[id].animBeginning=FALSE;
    gSprites[id].affineAnimPaused=TRUE;gSprites[id].affineAnimBeginning=FALSE;
    fx=&sRt->fx[i];memset(fx,0,sizeof(*fx));
    fx->active=TRUE;fx->spriteId=id;fx->ptdAnim=anim;fx->ptdPal=pal;fx->ptdMatrix=0xFF;
    fx->loop=loop;fx->life=life?life:max(12,min(254,(a->frames*60+a->fps-1)/a->fps));
    return i;
}

static void RtPtdOrient(u8 id,s32 dx,s32 dy,u16 scaleX,u16 scaleY)
{
    struct RtFx *fx;
    struct Sprite *sprite;
    if(id>=RT_MAX_FX || !sRt->fx[id].active || !sRt->fx[id].ptdAnim)return;
    fx=&sRt->fx[id];sprite=&gSprites[fx->spriteId];
    if(fx->ptdMatrix==0xFF)fx->ptdMatrix=AllocOamMatrix();
    if(fx->ptdMatrix==0xFF)return;
    if(scaleX<32)scaleX=32;if(scaleY<32)scaleY=32;
    SetOamMatrix(fx->ptdMatrix,dx*256/scaleX,dy*256/scaleX,-dy*256/scaleY,dx*256/scaleY);
    sprite->oam.affineMode=ST_OAM_AFFINE_DOUBLE;sprite->oam.matrixNum=fx->ptdMatrix;
    CalcCenterToCornerVec(sprite,sprite->oam.shape,sprite->oam.size,sprite->oam.affineMode);
}

static void RtPtdDestroyFx(u8 id)
{
    struct RtFx *fx=&sRt->fx[id];
    if(fx->ptdMatrix!=0xFF)FreeOamMatrix(fx->ptdMatrix);
    DestroySprite(&gSprites[fx->spriteId]);
    FreeSpriteTilesByTag(RT_PTD_FX_TAG+id);RtReleasePal(fx->ptdPal);
    fx->active=FALSE;fx->ptdAnim=0;
}

static bool8 RtPtdConditionActive(struct RtFighter *f,u8 kind)
{
    static const u32 masks[]={STATUS1_BURN,STATUS1_PSN_ANY,STATUS1_PARALYSIS,STATUS1_SLEEP,STATUS1_FREEZE};
    return kind<5 ? (f->status&masks[kind])!=0 : f->confusionTimer!=0;
}

static void RtPtdUpdateFx(u8 id)
{
    struct RtFx *fx=&sRt->fx[id];
    const struct RtPtdAnim *a=&sRtPtdAnims[fx->ptdAnim];
    u16 frame;
    if(fx->followSide)
    {
        struct RtFighter *f=fx->followSide==2?&sRt->enemy:&sRt->player;
        s16 x,y;
        if(!f->active || f->partyIndex!=fx->followParty
         || (fx->kind>=128 && (!f->hp || !f->clocks[fx->kind-128]))
         || (fx->kind>=64 && fx->kind<70 && (!f->hp || !RtPtdConditionActive(f,fx->kind-64)))){RtDestroyFx(id);return;}
        RtPtdBody(f,&x,&y);
        gSprites[fx->spriteId].x=x+fx->offsetX;gSprites[fx->spriteId].y=y+fx->offsetY;
    }
    fx->ptdAge+=sRt->frameStep;frame=fx->ptdAge*a->fps/60;
    if(frame>=a->frames)frame=fx->loop?frame%a->frames:a->frames-1;
    if(frame!=fx->frame)
    {
        fx->frame=frame;
        RequestSpriteCopy((const u8 *)a->pixels+frame*a->frameBytes,
            (u8 *)OBJ_VRAM0+gSprites[fx->spriteId].sheetTileStart*32,a->frameBytes);
    }
    if(fx->life!=0xFF)
    {
        fx->life=fx->life>sRt->frameStep?fx->life-sRt->frameStep:0;
        if(!fx->life)RtDestroyFx(id);
    }
}

static bool8 RtPtdImpact(struct RtFighter *f,struct RtFighter *target,u16 move)
{
    s16 x,y;u8 id;
    if(!RtPtdHasArt(move))return FALSE;
    RtPtdBody(target,&x,&y);
    id=RtPtdSpawn(sRtPtdMoveArt[move].impact,x,y,0,FALSE);
    if(id==0xFF)return FALSE;
    sRt->fx[id].followSide=target->isEnemy?2:1;sRt->fx[id].followParty=target->partyIndex;
    return TRUE;
}

static void RtPtdMiss(u16 move,s16 x,s16 y)
{
    if(RtPtdHasArt(move))RtPtdSpawn(sRtPtdMoveArt[move].impact,x,y,0,FALSE);
}

static bool8 RtPtdCastFx(struct RtFighter *f)
{
    struct RtCast *c=&f->cast;
    const struct RtMoveDef *d=RtProfile(c->move);
    u16 anim=sRtPtdMoveArt[c->move].cast;
    s16 x,y,front,dist,limit=d->reach,scale=sRt->arena?2:1;
    u8 i,id;
    if(c->move==MOVE_EARTH_POWER)
    {
        if(c->elapsed<d->startup)return TRUE;
        for(i=0;i<3;i++)if(c->fx[i]==0xFF)
        {
            dist=(i+1)*d->reach/3;
            x=FROM_Q8(f->x)+c->dx*dist/256;y=FROM_Q8(f->y)+c->dy*dist/256;
            if(!RtPtdRayClear(FROM_Q8(f->x),FROM_Q8(f->y),x,y))break;
            c->fx[i]=RtPtdSpawn(sRtPtdMoveArt[c->move].impact,x,y-16*scale,0xFF,FALSE);
        }
        return TRUE;
    }
    if(!anim)return FALSE;
    if(c->elapsed<d->startup)return TRUE;
    if(c->fx[0]!=0xFF)return TRUE;
    if(d->kind==RT_AREA)
    {
        RtPtdBody(f,&x,&y);id=RtPtdSpawn(anim,x,y,0xFF,FALSE);
        RtPtdOrient(id,256,0,min(512,d->reach*2*256/(scale*64)),min(512,d->reach*2*256/(scale*64)));
    }
    else
    {
        front=RtPtdEmitter(f,c->dx,c->dy,&x,&y);
        limit=RtPtdRayLimit(f,c->dx,c->dy,limit)-front;if(limit<4)return TRUE;
        x+=c->dx*limit/512;y+=c->dy*limit/512;
        id=RtPtdSpawn(anim,x,y,0xFF,FALSE);
        RtPtdOrient(id,c->dx,c->dy,min(512,limit*256/(scale*sRtPtdAnims[anim].extent)),256);
    }
    c->fx[0]=id;
    return TRUE;
}
