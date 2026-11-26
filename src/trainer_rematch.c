#include "global.h"
#include "trainer_rematch.h"
#include "battle.h"
#include "battle_setup.h"
#include "event_data.h"
#include "quest_log.h"
#include "script.h"
#include "constants/items.h"
#include "constants/flags.h"
#include "constants/moves.h"
#include "constants/species.h"
#include "constants/opponents.h"
#include "constants/quest_log.h"

// Occupy existing unused save padding, preserving every native save offset.
// One independent counter per trainer. Old saves migrate lazily on acceptance.
struct ScalingRematchSave {
    u32 magic;
    u8 version;
    u8 reserved[3];
    u8 counts[MAX_TRAINERS_COUNT];
};
#define REMATCH_MAGIC 0x374D5452
STATIC_ASSERT(sizeof(struct ScalingRematchSave) <= sizeof(((struct SaveBlock2 *)0)->filler_B20), RematchesFitSavePadding);
STATIC_ASSERT(offsetof(struct SaveBlock2, encryptionKey) == 0xF20, RematchesPreserveSaveLayout);

struct ScalingRematchEntry {
    const u8 *script;
    const u8 *command;
    u16 trainer;
};
#include "data/scaling_rematch_trainers.h"

extern const u8 EventScript_ScalingRematch[];
extern const struct Evolution gEvolutionTable[NUM_SPECIES][EVOS_PER_MON];
static EWRAM_DATA const u8 *sOriginalScript = NULL;
static EWRAM_DATA u16 sRematchTrainer = 0;
static EWRAM_DATA u8 sRematchBonus = 0;
static EWRAM_DATA bool8 sRematchAccepted = FALSE;

static struct ScalingRematchSave *RematchSave(void)
{
    return (struct ScalingRematchSave *)gSaveBlock2Ptr->filler_B20;
}

u8 GetScalingRematchCount(u16 trainer)
{
    struct ScalingRematchSave *save = RematchSave();
    if (trainer >= MAX_TRAINERS_COUNT || save->magic != REMATCH_MAGIC || save->version != 1)
        return 0;
    return save->counts[trainer] < MAX_LEVEL ? save->counts[trainer] : MAX_LEVEL - 1;
}

void FinishScalingRematch(void)
{
    sOriginalScript = NULL;
    sRematchTrainer = 0;
    sRematchBonus = 0;
    sRematchAccepted = FALSE;
}

const u8 *TryGetScalingRematchScript(const u8 *original)
{
    u16 i;
    const struct ScalingRematchEntry *entry = NULL;
    if (QL_IS_PLAYBACK_STATE) return original;
    FinishScalingRematch();
    for (i = 0; i < ARRAY_COUNT(sScalingRematches); i++)
    {
        const struct ScalingRematchEntry *candidate = &sScalingRematches[i];
        // The same League NPC owns both native teams. Only intercept the team
        // selected by story progress; its first upgraded fight stays native.
        if (candidate->trainer>=TRAINER_ELITE_FOUR_LORELEI && candidate->trainer<=TRAINER_ELITE_FOUR_LANCE
            && FlagGet(FLAG_SYS_CAN_LINK_WITH_RS)) continue;
        if (candidate->trainer>=TRAINER_ELITE_FOUR_LORELEI_2 && candidate->trainer<=TRAINER_ELITE_FOUR_LANCE_2
            && !FlagGet(FLAG_SYS_CAN_LINK_WITH_RS)) continue;
        if (candidate->script == original && HasTrainerBeenFought(candidate->trainer))
            entry = candidate;
    }
    if (!entry) return original;
    sOriginalScript = original;
    sRematchTrainer = entry->trainer;
    ConfigureScalingRematchBattle(entry->command);
    return EventScript_ScalingRematch;
}

void ScalingRematchCommitLevel(void)
{
    struct ScalingRematchSave *save = RematchSave();
    u16 i;
    if (!sOriginalScript || !sRematchTrainer || sRematchAccepted) return;
    if (save->magic != REMATCH_MAGIC || save->version != 1)
    {
        u8 *bytes = (u8 *)save;
        for (i = 0; i < sizeof(*save); i++) bytes[i] = 0;
        save->magic = REMATCH_MAGIC;
        save->version = 1;
    }
    sRematchBonus = GetScalingRematchCount(sRematchTrainer);
    if (sRematchBonus < MAX_LEVEL - 1) sRematchBonus++;
    save->counts[sRematchTrainer] = sRematchBonus;
    sRematchAccepted = TRUE;
}

void ScalingRematchResumeOriginal(void)
{
    const u8 *original = sOriginalScript;
    FinishScalingRematch();
    if (original) ScriptContext_SetupScript(original);
}

static void AddLevelMoves(bool8 *available, u16 species, u8 level)
{
    u16 i, entry;
    for (i = 0; (entry = gLevelUpLearnsets[species][i]) != LEVEL_UP_END; i++)
        if ((entry >> 9) <= level) available[entry & 0x1FF] = TRUE;
}

static void ChooseRematchMoves(struct Pokemon *mon, bool8 *available)
{
    u16 selected[MAX_MON_MOVES] = {0};
    u16 move, bestMove;
    u8 slot;
    s32 score, best;
    for (slot = 0; slot < MAX_MON_MOVES; slot++)
    {
        best = 0; bestMove = MOVE_NONE;
        for (move = 1; move < MOVES_COUNT; move++)
        {
            if (!available[move]) continue;
            // Reserve the first two slots for attacks whenever the legal pool permits.
            if (slot < 2 && !RealtimeRosterDamaging(move)) continue;
            score = RealtimeRosterMoveScore(mon, move, selected[0]);
            if (slot >= 2 && RealtimeRosterDamaging(move)) score = score * 2 / 3;
            if (score > best) { best = score; bestMove = move; }
        }
        if (!bestMove && slot < 2)
        {
            for (move = 1; move < MOVES_COUNT; move++)
            {
                if (!available[move]) continue;
                score = RealtimeRosterMoveScore(mon, move, selected[0]);
                if (score > best) { best = score; bestMove = move; }
            }
        }
        selected[slot] = bestMove;
        available[bestMove] = FALSE;
        // Penalize redundant support roles, including same-effect alternatives.
        if (bestMove && !RealtimeRosterDamaging(bestMove))
            for (move = 1; move < MOVES_COUNT; move++)
                if (!RealtimeRosterDamaging(move) && gBattleMoves[move].effect == gBattleMoves[bestMove].effect)
                    available[move] = FALSE;
    }
    for (slot = 0; slot < MAX_MON_MOVES; slot++) SetMonMoveSlot(mon, selected[slot], slot);
    slot = 0;
    SetMonData(mon, MON_DATA_PP_BONUSES, &slot);
}

static void SetRematchLevel(struct Pokemon *mon, u16 species, u8 level)
{
    SetMonData(mon, MON_DATA_SPECIES, &species);
    SetMonData(mon, MON_DATA_EXP, &gExperienceTables[gSpeciesInfo[species].growthRate][level]);
    SetMonData(mon, MON_DATA_LEVEL, &level);
    CalculateMonStats(mon);
}

static u16 LevelEvolution(struct Pokemon *mon, u16 species, u8 level)
{
    u8 i;
    u16 branch = (GetMonData(mon, MON_DATA_PERSONALITY) >> 16) % 10;
    u16 attack = GetMonData(mon, MON_DATA_ATK), defense = GetMonData(mon, MON_DATA_DEF);
    if (GetMonData(mon, MON_DATA_HELD_ITEM) == ITEM_EVERSTONE) return SPECIES_NONE;
    for (i = 0; i < EVOS_PER_MON; i++)
    {
        const struct Evolution *e = &gEvolutionTable[species][i];
        if (e->param > level) continue;
        switch (e->method)
        {
        case EVO_LEVEL: case EVO_LEVEL_NINJASK: return e->targetSpecies;
        case EVO_LEVEL_ATK_GT_DEF: if (attack > defense) return e->targetSpecies; break;
        case EVO_LEVEL_ATK_EQ_DEF: if (attack == defense) return e->targetSpecies; break;
        case EVO_LEVEL_ATK_LT_DEF: if (attack < defense) return e->targetSpecies; break;
        case EVO_LEVEL_SILCOON: if (branch <= 4) return e->targetSpecies; break;
        case EVO_LEVEL_CASCOON: if (branch > 4) return e->targetSpecies; break;
        }
    }
    return SPECIES_NONE;
}

void ScaleRematchMon(struct Pokemon *mon, u8 bonus)
{
    bool8 available[MOVES_COUNT] = {FALSE};
    u16 species = GetMonData(mon, MON_DATA_SPECIES), target, ancestor, i;
    u16 level = GetMonData(mon, MON_DATA_LEVEL) + bonus, hp;
    u8 depth, j;
    if (!bonus || !species || species >= NUM_SPECIES) return;
    if (level > MAX_LEVEL) level = MAX_LEVEL;
    for (j = 0; j < MAX_MON_MOVES; j++) available[GetMonData(mon, MON_DATA_MOVE1 + j)] = TRUE;
    // Include ancestral level-up moves a normally trained Pokemon could retain.
    ancestor = species;
    for (depth = 0; depth < 8; depth++)
    {
        AddLevelMoves(available, ancestor, level);
        target = SPECIES_NONE;
        for (i = 1; i < NUM_SPECIES && !target; i++)
            for (j = 0; j < EVOS_PER_MON; j++)
                if (gEvolutionTable[i][j].targetSpecies == ancestor) { target = i; break; }
        if (!target) break;
        ancestor = target;
    }
    SetRematchLevel(mon, species, level);
    for (depth = 0; depth < 8; depth++)
    {
        target = LevelEvolution(mon, species, level);
        if (!target || target == species) break;
        EvolutionRenameMon(mon, species, target);
        species = target;
        SetRematchLevel(mon, species, level);
        AddLevelMoves(available, species, level);
    }
    ChooseRematchMoves(mon, available);
    hp = GetMonData(mon, MON_DATA_MAX_HP);
    SetMonData(mon, MON_DATA_HP, &hp);
}

void ApplyScalingRematchParty(struct Pokemon *party, u16 trainer, u8 count)
{
    u8 i;
    if (!sRematchAccepted || trainer != sRematchTrainer) return;
    for (i = 0; i < count && i < PARTY_SIZE; i++) ScaleRematchMon(&party[i], sRematchBonus);
}

// League room flags reset between challenges even though trainer defeat flags
// persist. Reopen only the progression door, without replaying reward scripts.
u16 ScalingRematchLeagueProgress(void)
{
    u16 flag=0,door=1;
    switch(gTrainerBattleOpponent_A)
    {
    case TRAINER_ELITE_FOUR_LORELEI:case TRAINER_ELITE_FOUR_LORELEI_2:flag=FLAG_DEFEATED_LORELEI;break;
    case TRAINER_ELITE_FOUR_BRUNO:case TRAINER_ELITE_FOUR_BRUNO_2:flag=FLAG_DEFEATED_BRUNO;break;
    case TRAINER_ELITE_FOUR_AGATHA:case TRAINER_ELITE_FOUR_AGATHA_2:flag=FLAG_DEFEATED_AGATHA;break;
    case TRAINER_ELITE_FOUR_LANCE:case TRAINER_ELITE_FOUR_LANCE_2:flag=FLAG_DEFEATED_LANCE;door=2;break;
    }
    if(!flag || FlagGet(flag))return 0;
    FlagSet(flag);FlagClear(FLAG_TEMP_5);return door;
}
