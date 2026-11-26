static void RtTestPsychic(void)
{
    struct RtFighter *a, *b;
    u16 hp, i;
    s32 originalX;
    RtTestReset(); a=&sRt->player; b=&sRt->enemy;
    RtTestCheck(RtProfile(MOVE_PSYCHIC)->startup==15 && RtProfile(MOVE_PSYCHIC)->radius==24);
    RtLaunchCast(a,b,MOVE_PSYCHIC); hp=b->hp;
    for(i=0;i<14;i++) RtUpdateCast(a,b);
    RtTestCheck(b->hp==hp && !RtPsychicHeld(b));
    RtUpdateCast(a,b);
    RtTestCheck(RtPsychicHeld(b) && b->hp==hp && !a->cast.landed);
    RtTestCheck(!RtCanMove(b) && RtBusy(b) && !RtMoveAllowed(b,MOVE_TACKLE));
    originalX=b->x;
    RtTestCheck(!RtMoveFighter(b,Q8(8),0) && b->x==originalX);
    for(i=0;i<12;i++) RtUpdateCast(a,b);
    RtTestCheck(RtPsychicLiftY(b)==-20 && b->hp==hp);
    // Capture already rolled accuracy. Changing stages must not add a second
    // roll at landing (seed 1 would fail the newly reduced accuracy check).
    a->stages[RT_STAT_ACC]=-6;b->stages[RT_STAT_EVASION]=6;SeedRng(1);
    for(i=0;i<7;i++) RtUpdateCast(a,b);
    RtTestCheck(RtPsychicHeld(b) && b->hp==hp && RtPsychicLiftY(b)<0);
    RtUpdateCast(a,b); hp=b->hp;
    RtTestCheck(a->cast.landed && a->cast.hitCount==1 && hp<1000);
    RtTestCheck(!RtPsychicHeld(b) && RtCanMove(b) && RtPsychicLiftY(b)==0);
    for(i=0;i<10;i++) RtUpdateCast(a,b);
    RtTestCheck(b->hp==hp && !a->cast.active);
    // A fast dodge out of the original marker does not move the marker.
    RtTestReset(); a=&sRt->player;b=&sRt->enemy;
    RtLaunchCast(a,b,MOVE_PSYCHIC);originalX=a->cast.targetX;
    b->x+=Q8(30);sRt->frameStep=15;RtUpdateCast(a,b);
    RtTestCheck(!RtPsychicHeld(b) && !a->cast.hit && a->cast.targetX==originalX);
    sRt->frameStep=24;RtUpdateCast(a,b);RtTestCheck(b->hp==1000 && !a->cast.active);
    // Walking at ordinary speed is not an automatic dodge of a point cast.
    RtTestReset(); a=&sRt->player;b=&sRt->enemy;
    RtLaunchCast(a,b,MOVE_PSYCHIC);b->y+=Q8(22);sRt->frameStep=15;RtUpdateCast(a,b);
    RtTestCheck(RtPsychicHeld(b) && a->cast.targetY==b->y);
    // Cancellation, sleep, fainting and switching cannot leave a hold behind.
    RtCancelCast(a);RtTestCheck(!RtPsychicHeld(b) && RtCanMove(b));
    RtLaunchCast(a,b,MOVE_PSYCHIC);RtUpdateCast(a,b);RtSetStatus(a,STATUS1_SLEEP);
    RtTestCheck(!a->cast.active && !RtPsychicHeld(b));
    RtTestReset();a=&sRt->player;b=&sRt->enemy;
    RtLaunchCast(a,b,MOVE_PSYCHIC);sRt->frameStep=15;RtUpdateCast(a,b);a->hp=0;
    RtTestCheck(!RtPsychicHeld(b) && RtCanMove(b));
    // Protect, Dark typing, Wonder Guard and cover are checked before a grab.
    for(i=0;i<4;i++)
    {
        RtTestReset();a=&sRt->player;b=&sRt->enemy;
        if(i==0) b->clocks[RTC_PROTECT]=60;
        if(i==1) b->types[0]=b->types[1]=TYPE_DARK;
        if(i==2) b->ability=ABILITY_WONDER_GUARD;
        if(i==3) {a->x=Q8(120);b->x=Q8(160);sRt->arenaObjectCount=1;sRt->arenaObjects[0].kind=RAO_ROCK;sRt->arenaObjects[0].x=140;sRt->arenaObjects[0].y=80;sRt->arenaObjects[0].hp=3;}
        RtLaunchCast(a,b,MOVE_PSYCHIC);sRt->frameStep=15;RtUpdateCast(a,b);
        RtTestCheck(!RtPsychicHeld(b) && !a->cast.hit && b->hp==1000);
    }
    // Substitute intercepts once and never lifts its owner.
    RtTestReset();a=&sRt->player;b=&sRt->enemy;
    sRt->field[0].active=TRUE;sRt->field[0].enemy=TRUE;sRt->field[0].kind=RTF_SUB;
    sRt->field[0].x=75;sRt->field[0].y=80;sRt->field[0].hp=1000;
    sRt->field[0].fxId=0xFF;sRt->field[0].spriteId=MAX_SPRITES;
    RtLaunchCast(a,b,MOVE_PSYCHIC);sRt->frameStep=15;RtUpdateCast(a,b);
    RtTestCheck(!RtPsychicHeld(b) && b->hp==1000 && sRt->field[0].hp<1000 && a->cast.landed);
    // A dropped update can cross grab and slam without repeated damage.
    RtTestReset();a=&sRt->player;b=&sRt->enemy;
    RtLaunchCast(a,b,MOVE_PSYCHIC);sRt->frameStep=60;RtUpdateCast(a,b);
    RtTestCheck(!a->cast.active && !RtPsychicHeld(b) && b->hp<1000);
}
