// A cosmetic land follower. It owns one sprite and one allocated OBJ palette;
// it never changes object-event collision, scripts or saved Pokemon data.
#include "global.h"
#include "gflib.h"
#include "hgss_sprites.h"
#include "pokemon.h"
#include "event_object_movement.h"
#include "fieldmap.h"
#include "field_player_avatar.h"
#include "realtime_battle.h"
#include "script.h"
#include "quest_log.h"
#include "constants/quest_log.h"
#include "constants/event_objects.h"
#include "constants/species.h"

#define FOLLOWER_PAL_TAG 0x77F0
static EWRAM_DATA struct {
    s32 x,y;
    s16 playerX,playerY,targetX,targetY;
    u16 key,clock,stepClock,speed;
    u8 spriteId,mapNum,mapGroup,direction;
    bool8 shiny;
} sFollower = {.spriteId=MAX_SPRITES};

static void SpriteCB_MonFollower(struct Sprite *sprite) { }

void HideMonFollower(void)
{
    u8 id=sFollower.spriteId;
    if(id<MAX_SPRITES && gSprites[id].inUse && gSprites[id].callback==SpriteCB_MonFollower) DestroySprite(&gSprites[id]);
    FreeSpritePaletteByTag(FOLLOWER_PAL_TAG);
    sFollower.spriteId=MAX_SPRITES; sFollower.key=0;
}
static bool8 FollowerTileAllowed(s16 x,s16 y,u8 elevation)
{
    u8 occupant=GetObjectEventIdByXY(x,y), height=MapGridGetElevationAt(x,y);
    return !MapGridGetCollisionAt(x,y) && (!height || !elevation || height==elevation)
        && (occupant==OBJECT_EVENTS_COUNT || occupant==gPlayerAvatar.objectEventId);
}
#ifdef PMD_TEST
#include "overworld.h"
#include "main.h"
#include "fly_journey.h"
#include "constants/maps.h"
#include "constants/heal_locations.h"
// Emulator-only trigger (test builds): 1 = load the fixture team and start a wild battle.
extern void RealtimeWorldTestTeam(void);
u8 gPmdTestTrigger;
#endif
void UpdateMonFollower(void)
{
    struct ObjectEvent *player; struct Sprite *sprite; struct Pokemon *mon=NULL;
    struct SpritePalette palette;
    u16 key; u8 i,pal; bool8 shiny,moving; s16 px,py,sx,sy,tx,ty;
    s32 targetX,targetY,dx,dy,step;
#ifdef PMD_TEST
    if(gPmdTestTrigger==1){gPmdTestTrigger=0;RealtimeWorldTestTeam();TryStartRealtimeWildBattle();return;}
    if(gPmdTestTrigger==2){gPmdTestTrigger=0;SetWarpDestination(MAP_GROUP(MAP_PALLET_TOWN),MAP_NUM(MAP_PALLET_TOWN),-1,12,10);WarpIntoMap();SetMainCallback2(CB2_LoadMap);return;}
    if(gPmdTestTrigger==3||gPmdTestTrigger==4){u8 h=gPmdTestTrigger==3?HEAL_LOCATION_LAVENDER_TOWN:HEAL_LOCATION_ONE_ISLAND;gPmdTestTrigger=0;FlyJourney_RecordOrigin();SetWarpDestinationToHealLocation(h);WarpIntoMap();FlyJourney_StartOrLoadMap();return;}
#endif
    if(gRealtimeBattleActive || gMain.inBattle || ArePlayerFieldControlsLocked() || QL_IS_PLAYBACK_STATE
     || gPlayerAvatar.objectEventId>=OBJECT_EVENTS_COUNT
     || TestPlayerAvatarFlags(PLAYER_AVATAR_FLAG_SURFING|PLAYER_AVATAR_FLAG_MACH_BIKE|PLAYER_AVATAR_FLAG_ACRO_BIKE))
    { HideMonFollower(); return; }
    player=&gObjectEvents[gPlayerAvatar.objectEventId];
    if(!player->active || player->invisible || player->spriteId>=MAX_SPRITES || !gSprites[player->spriteId].inUse)
    { HideMonFollower(); return; }
    for(i=0;i<PARTY_SIZE;i++) if(GetMonData(&gPlayerParty[i],MON_DATA_SPECIES) && !GetMonData(&gPlayerParty[i],MON_DATA_IS_EGG) && GetMonData(&gPlayerParty[i],MON_DATA_HP)) { mon=&gPlayerParty[i]; break; }
    if(!mon) { HideMonFollower(); return; }
    key=DsMonSpriteKey(GetMonData(mon,MON_DATA_SPECIES),GetMonData(mon,MON_DATA_PERSONALITY),GetMonGender(mon)==MON_FEMALE);
    shiny=IsMonShiny(mon); px=player->currentCoords.x; py=player->currentCoords.y;
    if(sFollower.spriteId>=MAX_SPRITES || !gSprites[sFollower.spriteId].inUse || gSprites[sFollower.spriteId].callback!=SpriteCB_MonFollower
     || key!=sFollower.key || shiny!=sFollower.shiny || sFollower.mapNum!=gSaveBlock1Ptr->location.mapNum || sFollower.mapGroup!=gSaveBlock1Ptr->location.mapGroup)
    {
        HideMonFollower(); tx=player->previousCoords.x; ty=player->previousCoords.y;
        if((tx==px && ty==py) || tx<px-1 || tx>px+1 || ty<py-1 || ty>py+1)
        {
            tx=px; ty=py;
            if(player->facingDirection==DIR_SOUTH) ty--;
            else if(player->facingDirection==DIR_NORTH) ty++;
            else if(player->facingDirection==DIR_WEST) tx++;
            else tx--;
        }
        if(!key || !FollowerTileAllowed(tx,ty,player->currentElevation)) return;
        palette.data=DsMonPalette(key,shiny); palette.tag=FOLLOWER_PAL_TAG;
        pal=LoadSpritePalette(&palette); if(pal==0xFF) return;
        sFollower.spriteId=DsCreateMonSprite(key,0,0,pal,0x80,SpriteCB_MonFollower);
        if(sFollower.spriteId==MAX_SPRITES) { FreeSpritePaletteByTag(FOLLOWER_PAL_TAG); return; }
        sFollower.key=key; sFollower.shiny=shiny;
        sFollower.mapNum=gSaveBlock1Ptr->location.mapNum; sFollower.mapGroup=gSaveBlock1Ptr->location.mapGroup;
        sFollower.playerX=px; sFollower.playerY=py; sFollower.targetX=tx; sFollower.targetY=ty;
        sFollower.x=(s32)tx*4096; sFollower.y=(s32)ty*4096;
        sFollower.clock=0; sFollower.stepClock=16; sFollower.speed=256;
        sFollower.direction=player->facingDirection==DIR_NORTH?1:player->facingDirection==DIR_WEST?2:player->facingDirection==DIR_EAST?3:0;
    }
    if(sFollower.stepClock<255) sFollower.stepClock++;
    if(px!=sFollower.playerX || py!=sFollower.playerY)
    {
        if(px<sFollower.playerX-1 || px>sFollower.playerX+1 || py<sFollower.playerY-1 || py>sFollower.playerY+1)
        { HideMonFollower(); return; }
        sFollower.targetX=sFollower.playerX; sFollower.targetY=sFollower.playerY;
        sFollower.playerX=px; sFollower.playerY=py;
        sFollower.speed=4096/(sFollower.stepClock?sFollower.stepClock:1);
        if(sFollower.speed<256) sFollower.speed=256;
        if(sFollower.speed>1024) sFollower.speed=1024;
        sFollower.stepClock=0;
    }
    targetX=(s32)sFollower.targetX*4096; targetY=(s32)sFollower.targetY*4096;
    dx=targetX-sFollower.x; dy=targetY-sFollower.y; moving=dx || dy; step=sFollower.speed;
    if(dx) { sFollower.direction=dx<0?2:3; sFollower.x+=dx>step?step:dx<-step?-step:dx; }
    else if(dy) { sFollower.direction=dy<0?1:0; sFollower.y+=dy>step?step:dy<-step?-step:dy; }
    if(moving) sFollower.clock++; else sFollower.clock=0;
    tx=sFollower.x>>12; ty=sFollower.y>>12;
    SetSpritePosToMapCoords(tx,ty,&sx,&sy);
    sprite=&gSprites[sFollower.spriteId];
    sprite->x=sx+8+((sFollower.x>>8)&15);
    sprite->y=sy+16-DsMonAnchorY(key)+((sFollower.y>>8)&15);
    sprite->coordOffsetEnabled=TRUE; sprite->oam.priority=gSprites[player->spriteId].oam.priority;
    // Tall grass drawn over the player's feet sits one subpriority in front of
    // the player. A follower that is below the player on screen must sit in front
    // of that grass too, or the grass cuts through its body.
    sprite->subpriority=gSprites[player->spriteId].subpriority+(ty<py?1:ty>py?-2:0);
    sprite->invisible=FALSE;
    // PMD walk while moving, idle breathing while standing.
    PmdShowMonFrame(sFollower.spriteId,key,moving?PMD_ANIM_WALK:PMD_ANIM_IDLE,PmdDirFromVector(0,0,sFollower.direction),
        moving?sFollower.clock:(u16)gMain.vblankCounter2,TRUE,NULL,NULL);
}
