#ifndef GUARD_EXTRA_TRAINER_FLAGS_H
#define GUARD_EXTRA_TRAINER_FLAGS_H
// "Defeated" flags for trainer IDs beyond FireRed's native range (used by GBA Engine-added trainers).
bool8 ExtraTrainerFlagGet(u16 trainer);
void ExtraTrainerFlagSet(u16 trainer, bool8 value);
#endif
