// Commit the target position before the tell. A caught opponent is lifted in
// place, then damaged exactly once on landing. No world-coordinate teleport.
#define RT_PSYCHIC_RISE 12
#define RT_PSYCHIC_FALL 8

static s16 RtPsychicLiftY(struct RtFighter *f)
{
    s16 time;
    if (!RtPsychicHeld(f)) return 0;
    time=RtOther(f)->cast.elapsed-RtProfile(MOVE_PSYCHIC)->startup;
    if (time<0 || time>=RT_PSYCHIC_RISE+RT_PSYCHIC_FALL) return 0;
    return time<=RT_PSYCHIC_RISE ? -time*20/RT_PSYCHIC_RISE
         : -(RT_PSYCHIC_RISE+RT_PSYCHIC_FALL-time)*20/RT_PSYCHIC_FALL;
}

static void RtUpdatePsychic(struct RtFighter *f, struct RtFighter *target)
{
    struct RtCast *c = &f->cast;
    const struct RtMoveDef *d = RtProfile(MOVE_PSYCHIC);
    if (!c->released && c->elapsed >= d->startup)
    {
        u16 mult = RtMultiplier(RtMoveType(f, MOVE_PSYCHIC), target);
        c->released = TRUE;
        // Missing the committed circle never retargets a moving opponent.
        if (RtInCast(f,target) && target->active && !target->clocks[RTC_PROTECT]
         && mult && !(target->ability == ABILITY_WONDER_GUARD && mult <= 100)
         && RtAiLineClear(FROM_Q8(f->x),FROM_Q8(f->y),FROM_Q8(target->x),FROM_Q8(target->y)))
        {
            c->accuracyChecked = TRUE;
            c->accuracyPassed = RtRollAccuracy(f,target,MOVE_PSYCHIC);
            if (c->accuracyPassed)
            {
                if (RtSubInLine(f,target))
                {
                    // The substitute takes the hit; its owner is not lifted.
                    RtApplyImpact(f,target,MOVE_PSYCHIC,0);
                    c->landed = TRUE;
                    c->hitCount = 1;
                }
                else
                {
                    c->hit = TRUE;
                    c->targetX = target->x;
                    c->targetY = target->y;
                    RtCancelCast(target);
                    target->windup = target->dashFrames = target->hurtTimer = 0;
                    target->kbX = target->kbY = 0;
                }
            }
        }
    }
    if (c->hit && !c->landed)
    {
        if (!target->active || !target->hp) { RtCancelCast(f); return; }
        if (c->fx[0] < RT_MAX_FX && sRt->fx[c->fx[0]].active)
        {
            struct Sprite *ring = &gSprites[sRt->fx[c->fx[0]].spriteId];
            ring->x = FROM_Q8(c->targetX);
            ring->y = FROM_Q8(c->targetY) + 8;
        }
        if (c->elapsed >= d->startup + RT_PSYCHIC_RISE + RT_PSYCHIC_FALL)
        {
            c->landed = TRUE;
            c->hitCount = 1;
            RtApplyImpact(f,target,MOVE_PSYCHIC,0);
            RtSpawnFx(FX_SMOKE,FROM_Q8(target->x)-8,FROM_Q8(target->y)+8,12,FALSE);
            RtSpawnFx(FX_SMOKE,FROM_Q8(target->x)+8,FROM_Q8(target->y)+8,12,FALSE);
            PlaySE(SE_M_SCRATCH);
        }
    }
    if (c->elapsed >= c->end) RtCancelCast(f);
}
