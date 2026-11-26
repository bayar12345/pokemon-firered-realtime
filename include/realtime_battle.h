#ifndef GUARD_REALTIME_BATTLE_H
#define GUARD_REALTIME_BATTLE_H

// Real-time overworld wild battles.
// Wild encounters on land no longer switch to the battle screen: your lead
// Pokemon is summoned next to you and you fight the wild Pokemon right on the map.
//   D-Pad      move your Pokemon
//   A/B/L/R    use move 1/2/3/4
//   SELECT     pause and choose a Poke Ball
//   START      run away

// Options menu indices select one through five seconds. Preserve the old
// seconds-XOR-1 save encoding; former INSTANT and invalid values become 1 sec.
#define RT_START_DELAY_COUNT   5
#define RT_START_DELAY_DEFAULT 0
#define RT_DELAY_TO_SAVE(index)  (((index) + 1) ^ 1)
#define RT_DELAY_FROM_SAVE(saved) ((((saved) ^ 1) >= 1 && ((saved) ^ 1) <= RT_START_DELAY_COUNT) ? (((saved) ^ 1) - 1) : RT_START_DELAY_DEFAULT)

// Zeroed padding in old saves selects NORMAL. Keep SaveBlock2 offsets intact.
#define RT_AI_EASY   0
#define RT_AI_NORMAL 1
#define RT_AI_HARD   2
#define RT_AI_NIGHTMARE 3 // the original HARD: dodges anything it sees
#define RT_AI_COUNT  4
#define RT_AI_DEFAULT RT_AI_NORMAL
#define RT_AI_TO_SAVE(index) ((index) ^ 1)
#define RT_AI_FROM_SAVE(saved) ((((saved) ^ 1) < RT_AI_COUNT) ? ((saved) ^ 1) : RT_AI_DEFAULT)

struct Sprite;
struct OamData;
void RealtimeArenaProjectOam(const struct Sprite *sprite, struct OamData *oam);
extern bool8 gRealtimeBattleActive;

bool8 TryStartRealtimeWildBattle(void);
bool8 IsRealtimeBattlePaused(void);
bool8 TryStartRealtimeTrainerBattle(void);
bool8 TryStartRealtimeLegendaryBattle(void);
bool8 IsRealtimeArenaBattle(void);
bool8 IsRealtimeScrollingBattle(void);
void UpdateRealtimeBattleCamera(void);
u8 GetRealtimeBattleStartDelaySeconds(void);
u8 GetRealtimeBattleAI(void);

#endif // GUARD_REALTIME_BATTLE_H
