#ifndef GUARD_TRAINER_REMATCH_H
#define GUARD_TRAINER_REMATCH_H

#include "global.h"
#include "pokemon.h"

const u8 *TryGetScalingRematchScript(const u8 *original);
void ConfigureScalingRematchBattle(const u8 *command);
void ScalingRematchCommitLevel(void);
void ScalingRematchResumeOriginal(void);
void FinishScalingRematch(void);
u16 ScalingRematchLeagueProgress(void);
u8 GetScalingRematchCount(u16 trainer);
void ApplyScalingRematchParty(struct Pokemon *party, u16 trainer, u8 count);
void ScaleRematchMon(struct Pokemon *mon, u8 bonus);
bool8 RealtimeRosterDamaging(u16 move);
s32 RealtimeRosterMoveScore(struct Pokemon *mon, u16 move, u16 firstAttack);

#endif
