static void RtTestWildSpace(void)
{
    const struct MapLayout *savedLayout = gMapHeader.mapLayout;
    struct BackupMapLayout savedMap = VMap;
    static const u32 attributes[] = {MB_NORMAL, MB_TALL_GRASS, MB_WARP_DOOR, MB_POND_WATER, MB_JUMP_EAST, MB_IMPASSABLE_EAST};
    struct Tileset tileset = {0};
    struct MapLayout layout = {0};
    u16 *map = Alloc(32*32*sizeof(u16));
    u16 i;
    s16 tx, ty;
    RtTestCheck(map != NULL);
    if (!map) return;
    RtTestReset(); sRt->arena = FALSE;
    memset(gObjectEvents, 0, sizeof(gObjectEvents));
    memset(&gPlayerAvatar, 0, sizeof(gPlayerAvatar));
    gObjectEvents[0].active = TRUE;
    gObjectEvents[0].currentElevation = 3;
    gObjectEvents[0].currentCoords.x = gObjectEvents[0].previousCoords.x = 15;
    gObjectEvents[0].currentCoords.y = gObjectEvents[0].previousCoords.y = 15;
    gObjectEvents[0].facingDirection = DIR_EAST;
    sRt->tileX = sRt->tileY = 15;
    sRt->originX = 120; sRt->originY = 80;
    tileset.metatileAttributes = attributes;
    layout.width = layout.height = 16;
    layout.primaryTileset = layout.secondaryTileset = &tileset;
    gMapHeader.mapLayout = &layout;
    VMap.Xsize = VMap.Ysize = 32; VMap.map = map;
    for (i=0;i<32*32;i++) map[i] = 0x3001;
    RtTestCheck(RtPlanWildSpawns());
    RtTestCheck(RtAbs(sRt->wildPlayerDx)+RtAbs(sRt->wildPlayerDy)==1);
    RtTestCheck(RtAbs(sRt->wildEnemyDx-sRt->wildPlayerDx)+RtAbs(sRt->wildEnemyDy-sRt->wildPlayerDy)>=2);
    RtScreenToTile(sRt->originX+sRt->wildEnemyDx*16,sRt->originY+sRt->wildEnemyDy*16+6,&tx,&ty);
    RtTestCheck(MetatileBehavior_IsPokeGrass(MapGridGetMetatileBehaviorAt(tx,ty)));
    // A tempting grass tile beyond a tree wall is never a spawn candidate.
    for(i=0;i<32*32;i++) map[i]=0x3C00;
    map[15*32+15]=0x3001; map[15*32+14]=0x3000;
    map[15*32+13]=0x3000; map[15*32+12]=0x3001;
    map[15*32+18]=0x3001;
    RtTestCheck(RtPlanWildSpawns() && sRt->wildEnemyDx==-3 && sRt->wildEnemyDy==0);
    map[15*32+12]=0x3000;
    RtTestCheck(!RtPlanWildSpawns()); // no reachable grass other than trainer tile
    map[15*32+13]=0x3C00;
    RtTestCheck(!RtPlanWildSpawns()); // insufficient connected room
    // A real, visible detour around cover is allowed.
    for(i=14;i<=18;i++) map[14*32+i]=0x3000;
    map[15*32+18]=0x3001;
    RtTestCheck(RtPlanWildSpawns() && sRt->wildEnemyDx==3 && sRt->wildEnemyDy==0);
    map[14*32+16]=0x3C00;
    RtTestCheck(!RtPlanWildSpawns());
    // A detour outside the visible fight area cannot connect two spawns.
    for(i=0;i<32*32;i++) map[i]=0x3C00;
    map[15*32+15]=0x3001;
    for(i=11;i<=15;i++) {map[i*32+14]=0x3000; map[i*32+18]=0x3000;}
    for(i=14;i<=18;i++) map[11*32+i]=0x3000;
    map[15*32+18]=0x3001;
    RtTestCheck(!RtPlanWildSpawns());
    // Connections/border buffers, warps, water, ledges, elevations and NPCs.
    layout.width=9;
    RtTestCheck(RtIsTileBlocked(16,15));
    layout.width=16;
    for(i=2;i<=4;i++) { map[15*32+14]=0x3000+i; RtTestCheck(RtIsTileBlocked(14,15)); }
    map[15*32+14]=0x4000; RtTestCheck(RtIsTileBlocked(14,15));
    map[15*32+14]=0x3000;
    gObjectEvents[1].active=TRUE; gObjectEvents[1].currentCoords.x=14; gObjectEvents[1].currentCoords.y=15;
    RtTestCheck(RtIsTileBlocked(14,15)); gObjectEvents[1].active=FALSE;
    map[15*32+15]=0x3005; map[15*32+16]=0x3000;
    RtTestCheck(!RtWildStepClear(120,80,136,80));
    map[15*32+15]=0x3000;
    RtTestCheck(RtWildStepClear(120,80,136,80));
    // The production movement path also rejects tunnelling through a tile.
    map[15*32+16]=0x3C00; map[15*32+17]=0x3000;
    sRt->player.x=Q8(120); sRt->player.y=Q8(80);
    RtTestCheck(!RtMoveFighter(&sRt->player,Q8(32),0) && sRt->player.x==Q8(120));
    VMap=savedMap; gMapHeader.mapLayout=savedLayout; Free(map);
    memset(gObjectEvents,0,sizeof(gObjectEvents));
    // Legacy INSTANT and invalid saved options migrate to one second.
    for(i=0;i<8;i++)
    {
        gSaveBlock2Ptr->optionsBattleStartDelay=i;
        RtTestCheck(GetRealtimeBattleStartDelaySeconds()>=1 && GetRealtimeBattleStartDelaySeconds()<=5);
    }
    for(i=0;i<RT_START_DELAY_COUNT;i++)
    {
        gSaveBlock2Ptr->optionsBattleStartDelay=RT_DELAY_TO_SAVE(i);
        RtTestCheck(GetRealtimeBattleStartDelaySeconds()==i+1);
    }
    gSaveBlock2Ptr->optionsBattleStartDelay=1;
    RtTestCheck(GetRealtimeBattleStartDelaySeconds()==1);
}
