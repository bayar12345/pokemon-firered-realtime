// Native map streaming and bounded camera ownership. No connection/warp can run here.
static void RtCacheFieldCollision(void)
{
    s16 x,y;u8 d;static const s8 directions[4][2]={{0,1},{0,-1},{-1,0},{1,0}};
    sRt->collisionReady=FALSE;sRt->collisionLeft=sRt->tileX-16;sRt->collisionTop=sRt->tileY-10;
    for(y=0;y<20;y++)for(x=0;x<32;x++)
    {
        s16 tx=sRt->collisionLeft+x,ty=sRt->collisionTop+y;u16 at=y*32+x;
        sRt->collision[at]=RtReadTileBlocked(tx,ty);sRt->edges[at]=0;
        for(d=0;d<4;d++)if(!RtReadWildEdgeClear(tx,ty,tx+directions[d][0],ty+directions[d][1]))sRt->edges[at]|=1<<d;
    }
    sRt->collisionReady=TRUE;
}
bool8 IsRealtimeScrollingBattle(void){return sRt && sRt->cameraActive;}
void UpdateRealtimeBattleCamera(void)
{
    s16 x,y,dx,dy;
    if(!IsRealtimeScrollingBattle() || IsRealtimeBattlePaused())return;
    if(sRt->player.active && sRt->state==RT_STATE_FIGHT)
    {
        x=FROM_Q8(sRt->player.x)-120;y=FROM_Q8(sRt->player.y)-80;
        if(x<-120)x=-120;if(x>120)x=120;if(y<-80)y=-80;if(y>80)y=80;
        dx=x-sRt->cameraX;dy=y-sRt->cameraY;
        if(dx<-3)dx=-3;if(dx>3)dx=3;if(dy<-3)dy=-3;if(dy>3)dy=3;
        sRt->cameraX+=dx;sRt->cameraY+=dy;
    }
    MoveBattleFieldCamera(sRt->cameraX,sRt->cameraY);
}
static u8 RtCreateWorldSprite(const struct SpriteTemplate *t,s16 x,s16 y,u8 priority)
{
    u8 id=CreateSprite(t,x,y,priority);if(id<MAX_SPRITES)sRt->worldSprites[id]=TRUE;return id;
}
static void RtDestroyWorldSprite(struct Sprite *sprite)
{
    sRt->worldSprites[sprite-gSprites]=FALSE;DestroySprite(sprite);
}
#define CreateSprite RtCreateWorldSprite
#define DestroySprite RtDestroyWorldSprite
