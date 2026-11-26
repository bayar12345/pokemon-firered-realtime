// One authored PTD frame, streamed as connected 64x32 strips. Every strip uses
// the caster's own exact-angle matrix, including in the zoomed arena view.
#define RT_FLAME_TAG 0x77C0

static void RtStopFlame(struct RtFighter *f)
{
    u8 i;struct RtCast *c=&f->cast;
    if(!c->beamLoaded)return;
    for(i=0;i<8;i++)if(c->beamSprites[i]<MAX_SPRITES)DestroySprite(&gSprites[c->beamSprites[i]]);
    if(c->beamMatrix!=0xFF)FreeOamMatrix(c->beamMatrix);
    FreeSpriteTilesByTag(RT_FLAME_TAG+f->isEnemy);RtReleasePal(c->beamPal);c->beamLoaded=FALSE;
}

static bool8 RtLoadFlame(struct RtFighter *f)
{
    struct RtCast *c=&f->cast;
    const struct RtPtdMoveArt *art=&sRtPtdMoveArt[c->move];
    const struct RtPtdAnim *a;
    struct SpriteSheet sheet;struct SpriteTemplate t;u8 i;
    c->beamAnim=sRt->arena?art->beamArena:art->beam;
    if(!c->beamAnim)return FALSE;
    a=&sRtPtdAnims[c->beamAnim];
    for(i=0;i<8;i++)c->beamSprites[i]=MAX_SPRITES;
    c->beamMatrix=0xFF;c->beamPal=0xFF;
    sheet.data=a->pixels;sheet.size=a->frameBytes;sheet.tag=RT_FLAME_TAG+f->isEnemy;
    LoadSpriteSheet(&sheet);if(GetSpriteTileStartByTag(sheet.tag)==0xFFFF)return FALSE;
    c->beamPal=RtAcquirePal(RT_PTD_PAL_KEY+a->paletteKey,a->palette);
    if(c->beamPal==0xFF){FreeSpriteTilesByTag(sheet.tag);return FALSE;}
    c->beamLoaded=TRUE;c->beamFrame=0;
    {s16 x,y;c->beamFront=RtPtdEmitter(f,c->dx,c->dy,&x,&y);c->beamMuzzleX=x-FROM_Q8(f->x);c->beamMuzzleY=y-FROM_Q8(f->y);}
    c->beamMatrix=AllocOamMatrix();
    if(c->beamMatrix==0xFF){RtStopFlame(f);return FALSE;}
    t.tileTag=sheet.tag;t.paletteTag=TAG_NONE;t.oam=&gOamData_AffineOff_ObjNormal_64x32;
    t.anims=gDummySpriteAnimTable;t.images=NULL;t.affineAnims=gDummySpriteAffineAnimTable;t.callback=SpriteCallbackDummy;
    for(i=0;i<a->columns;i++)
    {
        struct Sprite *s;
        c->beamSprites[i]=CreateSprite(&t,0,0,0x24);
        if(c->beamSprites[i]==MAX_SPRITES){RtStopFlame(f);return FALSE;}
        s=&gSprites[c->beamSprites[i]];
        s->oam.paletteNum=c->beamPal;s->oam.priority=1;s->invisible=TRUE;
        s->animPaused=TRUE;s->animBeginning=FALSE;s->affineAnimPaused=TRUE;s->affineAnimBeginning=FALSE;
        s->oam.affineMode=ST_OAM_AFFINE_DOUBLE;s->oam.matrixNum=c->beamMatrix;
        s->oam.tileNum=GetSpriteTileStartByTag(sheet.tag)+i*32;
        CalcCenterToCornerVec(s,s->oam.shape,s->oam.size,s->oam.affineMode);
    }
    return TRUE;
}

static u8 RtPtdBeamFrame(struct RtFighter *f)
{
    struct RtCast *c=&f->cast;const struct RtPtdAnim *a=&sRtPtdAnims[c->beamAnim];
    u16 time=c->elapsed-(c->end-RtProfile(c->move)->active),remain=c->end>c->elapsed?c->end-c->elapsed:0;
    u8 frame;
    if(time<6)frame=time*a->holdStart/6;
    else if(remain<6 && a->holdEnd+1<a->frames)
        frame=a->holdEnd+1+(6-remain)*(a->frames-a->holdEnd-1)/6;
    else frame=a->holdStart+((time-6)*a->fps/60)%(a->holdEnd-a->holdStart+1);
    return frame<a->frames?frame:a->frames-1;
}

static void RtDrawFlame(struct RtFighter *f)
{
    struct RtCast *c=&f->cast;const struct RtMoveDef *d=RtProfile(c->move);
    const struct RtPtdAnim *a;
    s16 ox,oy,front,length=0,scale=sRt->arena?2:1;
    s32 pixels,stretch,local;u8 i,frame;
    if(c->elapsed<c->end-d->active)return;
    if(!c->beamLoaded && !RtLoadFlame(f))return;
    a=&sRtPtdAnims[c->beamAnim];
    front=c->beamFront;ox=FROM_Q8(f->x)+c->beamMuzzleX;oy=FROM_Q8(f->y)+c->beamMuzzleY;
    length=RtPtdRayLimit(f,c->dx,c->dy,d->reach);
    pixels=(length-front)/scale;c->beamPixels=pixels>0?pixels:0;
    if(pixels<4)
    {
        for(i=0;i<a->columns;i++)gSprites[c->beamSprites[i]].invisible=TRUE;
        return;
    }
    stretch=pixels*256/a->extent;
    SetOamMatrix(c->beamMatrix,c->dx*256/stretch,c->dy*256/stretch,-c->dy,c->dx);
    frame=RtPtdBeamFrame(f);
    if(frame!=c->beamFrame)
    {
        RequestSpriteCopy((const u8 *)a->pixels+frame*a->frameBytes,
            (u8 *)OBJ_VRAM0+GetSpriteTileStartByTag(RT_FLAME_TAG+f->isEnemy)*32,a->frameBytes);
        c->beamFrame=frame;
    }
    for(i=0;i<a->columns;i++)
    {
        struct Sprite *s=&gSprites[c->beamSprites[i]];
        local=(i*64+32)*stretch/256;
        s->x=ox+c->dx*local*scale/256;s->y=oy+c->dy*local*scale/256;
        s->invisible=FALSE;
    }
}
