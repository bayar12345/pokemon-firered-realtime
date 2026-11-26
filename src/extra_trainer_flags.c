// Extended trainer flags. Uses the same save area and layout as the earlier world build,
// so existing saves keep their defeated-trainer state.
#include "global.h"
#include "extra_trainer_flags.h"
#include "constants/opponents.h"

#define EXTRA_MAGIC 0x31574750
#define EXTRA_SAVE_OFFSET (8 + MAX_TRAINERS_COUNT)
struct ExtraSave {
    u32 magic, seed; u16 version; u8 state[16]; u8 reserved[2];
    u8 parcelItems[69], gymWins[4], gymRewards[4], mentorRewards[3];
    u8 extraTrainerFlags[16], visitedTowns[4];
};
STATIC_ASSERT(EXTRA_SAVE_OFFSET + sizeof(struct ExtraSave) <= sizeof(((struct SaveBlock2 *)0)->filler_B20), ExtraFlagsFitUnusedRematchTail);

static struct ExtraSave *Save(void) { return (struct ExtraSave *)(gSaveBlock2Ptr->filler_B20 + EXTRA_SAVE_OFFSET); }
static void InitState(void)
{
    struct ExtraSave *s = Save();
    u32 i;
    if (s->magic != EXTRA_MAGIC || s->version < 1 || s->version > 3)
    {
        u8 *b = (u8 *)s;
        for (i = 0; i < sizeof(*s); i++) b[i] = 0;
        s->seed = 0x5FA61AC2; s->magic = EXTRA_MAGIC; s->version = 3;
    }
}
bool8 ExtraTrainerFlagGet(u16 trainer)
{
    u16 i = trainer - MAX_TRAINERS_COUNT;
    if (trainer < MAX_TRAINERS_COUNT || i >= 128) return FALSE;
    InitState();
    return (Save()->extraTrainerFlags[i / 8] >> (i % 8)) & 1;
}
void ExtraTrainerFlagSet(u16 trainer, bool8 value)
{
    u16 i = trainer - MAX_TRAINERS_COUNT;
    if (trainer < MAX_TRAINERS_COUNT || i >= 128) return;
    InitState();
    if (value) Save()->extraTrainerFlags[i / 8] |= 1 << (i % 8);
    else Save()->extraTrainerFlags[i / 8] &= ~(1 << (i % 8));
}
