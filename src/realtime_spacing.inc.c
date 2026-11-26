// Make room before capturing the battle's map/screen origin. Use native held
// walking actions so collision, camera tracking and ground effects stay intact.
#define RT_SPACE_MAX_STEPS 3
#define RT_SPACE_GAP 6

static bool8 RtSpacingForbidden(u8 behavior)
{
    return MetatileBehavior_IsJumpEast(behavior) || MetatileBehavior_IsJumpWest(behavior)
        || MetatileBehavior_IsJumpNorth(behavior) || MetatileBehavior_IsJumpSouth(behavior)
        || MetatileBehavior_IsWarpDoor(behavior) || MetatileBehavior_IsNonAnimDoor(behavior)
        || MetatileBehavior_IsLadder(behavior) || MetatileBehavior_IsEscalator(behavior)
        || MetatileBehavior_IsDirectionalStairWarp(behavior) || MetatileBehavior_IsFallWarp(behavior)
        || MetatileBehavior_IsWarpPad(behavior) || MetatileBehavior_IsEastArrowWarp(behavior)
        || MetatileBehavior_IsWestArrowWarp(behavior) || MetatileBehavior_IsNorthArrowWarp(behavior)
        || MetatileBehavior_IsSouthArrowWarp(behavior) || MetatileBehavior_IsDeepSouthWarp(behavior);
}

static bool8 RtCanRetreat(struct ObjectEvent *object, u8 direction)
{
    s16 x = object->currentCoords.x, y = object->currentCoords.y;
    MoveCoords(direction, &x, &y);
    // Do not leave this map, step onto a warp, or jump a ledge in either direction.
    if (x < MAP_OFFSET || y < MAP_OFFSET || x >= gMapHeader.mapLayout->width + MAP_OFFSET
     || y >= gMapHeader.mapLayout->height + MAP_OFFSET) return FALSE;
    if (RtSpacingForbidden(MapGridGetMetatileBehaviorAt(x, y))
     || RtSpacingForbidden(MapGridGetMetatileBehaviorAt(object->currentCoords.x, object->currentCoords.y))) return FALSE;
    // Normal NPC roaming limits do not constrain this temporary scripted step.
    return !(GetCollisionFlagsAtCoords(object, x, y, direction) & (2 | 4 | 8));
}

static u8 RtSpaceDirection(s16 dx, s16 dy)
{
    if (RtAbs(dx) >= RtAbs(dy)) return dx >= 0 ? DIR_EAST : DIR_WEST;
    return dy >= 0 ? DIR_SOUTH : DIR_NORTH;
}

static void RtBeginSpacing(void)
{
    u8 i;
    struct ObjectEvent *player, *trainer;
    if (!RT_ENABLE_ARENAS || !sRt->isTrainer || sRt->arena || gSelectedObjectEvent >= OBJECT_EVENTS_COUNT
     || gSelectedObjectEvent == gPlayerAvatar.objectEventId || !gObjectEvents[gSelectedObjectEvent].active)
    { sRt->state = RT_STATE_SETUP; return; }
    sRt->spaceActors[0] = gPlayerAvatar.objectEventId;
    sRt->spaceActors[1] = gSelectedObjectEvent;
    player = &gObjectEvents[sRt->spaceActors[0]];
    trainer = &gObjectEvents[sRt->spaceActors[1]];
    sRt->spaceBack[1] = RtSpaceDirection(trainer->currentCoords.x - player->currentCoords.x,
                                       trainer->currentCoords.y - player->currentCoords.y);
    sRt->spaceBack[0] = GetOppositeDirection(sRt->spaceBack[1]);
    for (i = 0; i < 2; i++)
    {
        struct ObjectEvent *object = &gObjectEvents[sRt->spaceActors[i]];
        sRt->spaceOriginalX[i] = object->currentCoords.x;
        sRt->spaceOriginalY[i] = object->currentCoords.y;
    }
    sRt->spaceStarted = TRUE;
    sRt->state = RT_STATE_CLEAR_SPACE;
}

static void RtUpdateSpacing(void)
{
    u8 i;
    struct ObjectEvent *player = &gObjectEvents[sRt->spaceActors[0]];
    struct ObjectEvent *trainer = &gObjectEvents[sRt->spaceActors[1]];
    bool8 moved = FALSE;
    // Let each in-progress tile step and the camera finish before summoning.
    for (i = 0; i < 2; i++)
        if (ObjectEventIsHeldMovementActive(&gObjectEvents[sRt->spaceActors[i]]))
            if (!ObjectEventClearHeldMovementIfFinished(&gObjectEvents[sRt->spaceActors[i]])) return;
    if (sRt->spaceSettle) { sRt->spaceSettle--; return; }
    if (RtAbs(trainer->currentCoords.x - player->currentCoords.x)
      + RtAbs(trainer->currentCoords.y - player->currentCoords.y) < RT_SPACE_GAP)
    {
        for (i = 0; i < 2; i++)
        {
            struct ObjectEvent *object = &gObjectEvents[sRt->spaceActors[i]];
            if (sRt->spaceSteps[i] >= RT_SPACE_MAX_STEPS || !RtCanRetreat(object, sRt->spaceBack[i])) continue;
            if (!ObjectEventSetHeldMovement(object, GetWalkNormalMovementAction(sRt->spaceBack[i])))
            { sRt->spaceSteps[i]++; moved = TRUE; }
        }
    }
    if (moved) { sRt->spaceSettle = 2; return; }
    for (i = 0; i < 2; i++)
    {
        struct ObjectEvent *object = &gObjectEvents[sRt->spaceActors[i]];
        SetObjectEventDirection(object, GetOppositeDirection(sRt->spaceBack[i]));
        StartSpriteAnim(&gSprites[object->spriteId], GetFaceDirectionAnimNum(object->facingDirection));
        FreezeObjectEvent(object);
    }
    sRt->state = RT_STATE_SETUP;
}

static void RtRestoreSpacing(void)
{
    u8 i;
    if (!sRt->spaceStarted) return;
    for (i = 0; i < 2; i++)
    {
        struct ObjectEvent *object = &gObjectEvents[sRt->spaceActors[i]];
        ObjectEventClearHeldMovementIfActive(object);
        // This low-level API takes padded map-grid coordinates. Only the
        // saved camera origin below removes MAP_OFFSET.
        MoveObjectEventToMapCoords(object, sRt->spaceOriginalX[i], sRt->spaceOriginalY[i]);
        SetObjectEventDirection(object, GetOppositeDirection(sRt->spaceBack[i]));
        UnfreezeObjectEvent(object);
    }
    gSaveBlock1Ptr->pos.x = sRt->spaceOriginalX[0] - MAP_OFFSET;
    gSaveBlock1Ptr->pos.y = sRt->spaceOriginalY[0] - MAP_OFFSET;
    sRt->spaceStarted = FALSE;
}

// Select a pair together; never let both Pokemon choose the same fallback tile.
// Favor positions one third/two thirds of the way between the human trainers.
static bool8 RtFindTrainerSpawns(s32 *px, s32 *py, s32 *ex, s32 *ey)
{
    struct { s16 x,y; } spots[96];
    struct ObjectEvent *trainer;
    s16 i,j,n=0,x,y;
    s32 tx,ty,ax,ay,bx,by,dot,length,score,best=0x7FFFFFFF;
    bool8 found=FALSE;
    if (gSelectedObjectEvent >= OBJECT_EVENTS_COUNT || !gObjectEvents[gSelectedObjectEvent].active) return FALSE;
    trainer = &gObjectEvents[gSelectedObjectEvent];
    tx = sRt->originX + (trainer->currentCoords.x - sRt->tileX) * 16;
    ty = sRt->originY + (trainer->currentCoords.y - sRt->tileY) * 16;
    ax = (sRt->originX * 2 + tx) / 3; ay = (sRt->originY * 2 + ty) / 3;
    bx = (sRt->originX + tx * 2) / 3; by = (sRt->originY + ty * 2) / 3;
    length=(tx-sRt->originX)*(tx-sRt->originX)+(ty-sRt->originY)*(ty-sRt->originY);
    for (i=-6;i<=7;i++) for (j=-4;j<=4;j++)
    {
        x=sRt->originX+i*16; y=sRt->originY+j*16;
        dot=(x-sRt->originX)*(tx-sRt->originX)+(y-sRt->originY)*(ty-sRt->originY);
        if (dot<=0 || dot>=length) continue;
        if (n>=ARRAY_COUNT(spots) || !RtIsSpotWalkable(x,y)) continue;
        if (RtLength(x-sRt->originX,y-sRt->originY)<24 || RtLength(x-tx,y-ty)<24) continue;
        spots[n].x=x; spots[n++].y=y;
    }
    for (i=0;i<n;i++) for (j=0;j<n;j++)
    {
        if (RtLength(spots[i].x-spots[j].x,spots[i].y-spots[j].y)<32) continue;
        score=RtAbs(spots[i].x-ax)+RtAbs(spots[i].y-ay)+RtAbs(spots[j].x-bx)+RtAbs(spots[j].y-by);
        if (score<best)
        { best=score; *px=spots[i].x; *py=spots[i].y; *ex=spots[j].x; *ey=spots[j].y; found=TRUE; }
    }
    return found;
}
