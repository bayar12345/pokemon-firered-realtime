// Fighters must fit inside one connected, bounded part of the current map.
// Use the same feet collision and directional edge checks as fighter movement.
static void RtReadMapOrigin(void)
{
    struct ObjectEvent *object = &gObjectEvents[gPlayerAvatar.objectEventId];
    struct Sprite *sprite = &gSprites[object->spriteId];
    sRt->tileX = object->currentCoords.x;
    sRt->tileY = object->currentCoords.y;
    sRt->originX = sprite->x + sprite->x2;
    sRt->originY = sprite->y + sprite->y2 + 8;
    if (sprite->coordOffsetEnabled)
    {
        sRt->originX += gSpriteCoordOffsetX;
        sRt->originY += gSpriteCoordOffsetY;
    }
}

static bool8 RtReadWildEdgeClear(s16 x, s16 y, s16 nx, s16 ny)
{
    struct ObjectEvent probe;
    u8 direction;
    if (x == nx && y == ny) return TRUE;
    probe = gObjectEvents[gPlayerAvatar.objectEventId];
    direction = nx > x ? DIR_EAST : nx < x ? DIR_WEST : ny > y ? DIR_SOUTH : DIR_NORTH;
    probe.currentCoords.x = x;
    probe.currentCoords.y = y;
    probe.currentMetatileBehavior = MapGridGetMetatileBehaviorAt(x, y);
    probe.trackedByCamera = FALSE;
    // Tile occupancy is checked separately, allowing the human's origin tile.
    // Native directional walls and elevation rules still apply.
    return !(GetCollisionFlagsAtCoords(&probe, nx, ny, direction) & (2 | 4));
}

static bool8 RtWildEdgeClear(s16 x,s16 y,s16 nx,s16 ny)
{
    s16 cx=x-sRt->collisionLeft,cy=y-sRt->collisionTop;u8 d;
    if(x==nx && y==ny)return TRUE;
    d=nx>x?3:nx<x?2:ny>y?0:1;
    if(sRt->collisionReady && cx>=0 && cx<32 && cy>=0 && cy<20)return !(sRt->edges[cy*32+cx]&(1<<d));
    return RtReadWildEdgeClear(x,y,nx,ny);
}
static bool8 RtWildStepClear(s16 x, s16 y, s16 nx, s16 ny)
{
    s16 px, py, tx, ty, ux, uy;
    s8 foot;
    while (x != nx || y != ny)
    {
        px = x + (nx > x ? min(4, nx - x) : -min(4, x - nx));
        py = y + (ny > y ? min(4, ny - y) : -min(4, y - ny));
        if (!RtIsSpotWalkable(px, py)) return FALSE;
        for (foot = -5; foot <= 5; foot += 10)
        {
            RtScreenToTile(x + foot, y + 6, &tx, &ty);
            RtScreenToTile(px + foot, py + 6, &ux, &uy);
            if (!RtWildEdgeClear(tx, ty, ux, uy)) return FALSE;
        }
        x = px;
        y = py;
    }
    return TRUE;
}

#define RT_WILD_GRID_SIZE (30 * 17)
static bool8 RtPlanWildSpawns(void)
{
    u8 seen[RT_WILD_GRID_SIZE] = {0};u16 queue[RT_WILD_GRID_SIZE];
    u16 head = 0, tail = 0, width, height, node, next, p, e;u8 d;
    s16 left = sRt->originX, top = sRt->originY;
    s16 x, y, nx, ny, px, py, ex, ey, tx, ty;
    s32 dx, dy, score, best = 0x7FFFFFFF;
    bool8 grass = !sRt->isTrainer && MetatileBehavior_IsPokeGrass(MapGridGetMetatileBehaviorAt(sRt->tileX, sRt->tileY));
    static const s8 offsets[4][2] = {{0,1},{0,-1},{-1,0},{1,0}};

    if (!RtIsSpotWalkable(left, top)) return FALSE;
    while (left - 16 >= RT_FIELD_LEFT) left -= 16;
    while (top - 16 >= RT_FIELD_TOP) top -= 16;
    width = (RT_FIELD_RIGHT - left) / 16 + 1;
    height = (RT_FIELD_BOTTOM - top) / 16 + 1;
    if (width * height > RT_WILD_GRID_SIZE) return FALSE;
    node = (sRt->originY - top) / 16 * width + (sRt->originX - left) / 16;
    seen[node] = TRUE;
    queue[tail++] = node;
    while (head < tail)
    {
        node = queue[head++];
        x = left + node % width * 16;
        y = top + node / width * 16;
        for (d = 0; d < 4; d++)
        {
            nx = x + offsets[d][0] * 16;
            ny = y + offsets[d][1] * 16;
            if (nx < left || ny < top || nx >= left + width * 16 || ny >= top + height * 16) continue;
            next = (ny - top) / 16 * width + (nx - left) / 16;
            if (seen[next] || !RtWildStepClear(x,y,nx,ny) || !RtWildStepClear(nx,ny,x,y)) continue;
            seen[next] = TRUE;
            queue[tail++] = next;
        }
    }
    switch (GetPlayerFacingDirection())
    {
    case DIR_NORTH: dx = 0; dy = -1; break;
    case DIR_WEST: dx = -1; dy = 0; break;
    case DIR_EAST: dx = 1; dy = 0; break;
    default: dx = 0; dy = 1; break;
    }
    // Select a pair together, rather than two unrelated walkable islands.
    for (e = 0; e < tail; e++)
    {
        node = queue[e];
        ex = left + node % width * 16;
        ey = top + node / width * 16;
        if (RtAbs(ex-sRt->originX)+RtAbs(ey-sRt->originY) < 32) continue;
        RtScreenToTile(ex, ey + 6, &tx, &ty);
        if (grass && !MetatileBehavior_IsPokeGrass(MapGridGetMetatileBehaviorAt(tx,ty))) continue;
        for (p = 0; p < 4; p++)
        {
            px = sRt->originX + offsets[p][0]*16;
            py = sRt->originY + offsets[p][1]*16;
            if(px<left || py<top || px>=left+width*16 || py>=top+height*16)continue;
            node=(py-top)/16*width+(px-left)/16;if(!seen[node])continue;
            if (RtAbs(px-sRt->originX)+RtAbs(py-sRt->originY) != 16
             || RtAbs(ex-px)+RtAbs(ey-py) < 32) continue;
            score = 2*(RtAbs(ex-sRt->originX-dx*48)+RtAbs(ey-sRt->originY-dy*48))
                  + RtAbs(px-sRt->originX-dx*16)+RtAbs(py-sRt->originY-dy*16);
            if (score >= best) continue;
            best = score;
            // Store tile offsets: the popup/StopPlayerAvatar can settle the
            // sprite before setup, but cannot change this frozen map position.
            sRt->wildPlayerDx = (px-sRt->originX)/16;
            sRt->wildPlayerDy = (py-sRt->originY)/16;
            sRt->wildEnemyDx = (ex-sRt->originX)/16;
            sRt->wildEnemyDy = (ey-sRt->originY)/16;
        }
    }
    if(best==0x7FFFFFFF && tail>=2)
    {
        node=queue[tail-1];sRt->wildPlayerDx=sRt->wildPlayerDy=0;
        sRt->wildEnemyDx=(left+node%width*16-sRt->originX)/16;sRt->wildEnemyDy=(top+node/width*16-sRt->originY)/16;
        return TRUE;
    }
    return best != 0x7FFFFFFF;
}
