// Real-time overworld wild battles.
//
// When a wild Pokemon appears on a land tile, the game no longer switches to the
// classic battle screen. Instead the wild Pokemon appears on the map, your lead
// Pokemon is summoned next to you, a short countdown runs (Options > BATTLE START)
// and then you fight in real time:
//
//   D-Pad      move your Pokemon
//   A/B/L/R    use move 1/2/3/4
//   SELECT     pause and choose a Poke Ball
//   START      run away
//
// Damage uses the Gen 3 formula (stats, STAB, type chart, crits, random roll).
// Contact moves are short lunges, other damaging moves fire projectiles that
// can be dodged, stat moves change stat stages (Speed stages also change how
// fast a Pokemon moves). EXP, EVs, level ups, new moves and evolution all work.

#include "global.h"
#include "trainer_rematch.h"
#include "party_exp_share.h"
#include "characters.h"
#include "gflib.h"
#include "realtime_battle.h"
#include "battle.h"
#include "battle_anim.h"
#include "battle_main.h"
#include "battle_setup.h"
#include "decompress.h"
#include "data.h"
#include "event_object_movement.h"
#include "evolution_scene.h"
#include "field_fadetransition.h"
#include "field_player_avatar.h"
#include "fieldmap.h"
#include "field_camera.h"
#include "money.h"
#include "field_message_box.h"
#include "constants/trainers.h"
#include "constants/battle_setup.h"
#include "constants/opponents.h"
#include "constants/event_objects.h"
#include "constants/event_object_movement.h"
#include "item.h"
#include "map_name_popup.h"
#include "menu.h"
#include "field_screen_effect.h"
#include "metatile_behavior.h"
#include "overworld.h"
#include "pokeball.h"
#include "pokedex.h"
#include "pokemon.h"
#include "pokemon_icon.h"
#include "hgss_sprites.h"
#include "pokemon_storage_system.h"
#include "quest_log.h"
#include "random.h"
#include "script.h"
#include "sound.h"
#include "strings.h"
#include "task.h"
#include "constants/battle_anim.h"
#include "constants/battle_move_effects.h"
#include "constants/game_stat.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/songs.h"
#include "constants/species.h"
#include "constants/abilities.h"
#include "constants/hold_effects.h"
#include "constants/weather.h"
#include "field_weather.h"
#include "field_effect_helpers.h"
#include "text_window.h"
#include "text_window_graphics.h"

STATIC_ASSERT(offsetof(struct SaveBlock2, optionsBattleAI) == 0x16, RealtimeAIUsesPadding);
STATIC_ASSERT(offsetof(struct SaveBlock2, pokedex) == 0x18, RealtimeAIPreservesPokedex);
STATIC_ASSERT(sizeof(struct SaveBlock2) == 0xF24, RealtimeAIPreservesSaveSize);

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define Q8(n) ((s32)(n) << 8)
#define FROM_Q8(n) ((n) >> 8)

enum {
    RT_DIR_DOWN,
    RT_DIR_UP,
    RT_DIR_LEFT,
    RT_DIR_RIGHT,
};

enum {
    RT_STAT_ATK,
    RT_STAT_DEF,
    RT_STAT_SPEED,
    RT_STAT_SPATK,
    RT_STAT_SPDEF,
    RT_STAT_ACC,
    RT_STAT_EVASION,
    RT_STAT_COUNT
};

enum {
    RT_STATE_WAIT_POPUP,
    RT_STATE_SETUP,
    RT_STATE_INTRO,
    RT_STATE_COUNTDOWN,
    RT_STATE_FIGHT,
    RT_STATE_PICKER,
    RT_STATE_BALL,
    RT_STATE_ENEMY_FAINT,
    RT_STATE_ENEMY_SWITCH,
    RT_STATE_PLAYER_FAINT,
    RT_STATE_SWITCH,
    RT_STATE_MESSAGES,
    RT_STATE_END,
    RT_STATE_WHITEOUT,
    RT_STATE_EVOLVE,
    RT_STATE_TRAINER_SPEECH,
    RT_STATE_PRESENTATION,
    RT_STATE_MANUAL_SWITCH,
    RT_STATE_LEARN_MOVE,
    RT_STATE_CLEAR_SPACE,
};

enum {
    RT_OUTCOME_NONE,
    RT_OUTCOME_WON,
    RT_OUTCOME_CAUGHT,
    RT_OUTCOME_RAN,
    RT_OUTCOME_LOST,
};

// Effect graphics, all taken from the game's own battle animation sprites.
enum {
    FX_IMPACT,
    FX_EMBER,
    FX_WATER,
    FX_SPARK,
    FX_LEAF,
    FX_ICE,
    FX_POISON,
    FX_SHADOW,
    FX_ROCK,
    FX_PSYCHIC,
    FX_DRAGON,
    FX_FEATHER,
    FX_FIST,
    FX_SCRATCH,
    FX_SPARKLE,
    FX_SMOKE,
    FX_ORB,
    FX_CHOP, FX_KICK, FX_COIN, FX_NEEDLE, FX_ROOT, FX_BONE, FX_WIND, FX_HEART, FX_PSY_MARK, FX_COUNT
};

enum {
    FX_SHAPE_16x16,
    FX_SHAPE_32x32,
    FX_SHAPE_16x32,
    FX_SHAPE_8x8,
    FX_SHAPE_32x16,
};

enum {
    RT_WIN_ENEMY,
    RT_WIN_PLAYER,
    RT_WIN_MOVES,
    RT_WIN_MSG,
    RT_WIN_HINT,
    RT_WIN_MENU,
    RT_WIN_COUNT
};

#define RT_MAX_PROJ     6
#define RT_MAX_FX       24
#define RT_MSG_MAX      24
#define RT_MSG_LEN      128

#define RT_HUD_PAL      13
#define RT_FRAME_PAL    14
#define RT_FRAME_TILES  0x060
#define RT_MENU_FRAME_TILES 0x1E0

// Sprite palette slots. The overworld reserves 12 of the 16 OBJ palettes for
// object events, so the battle borrows the ones no visible sprite is using.
#define RT_PAL_UNAVAILABLE 0xFFFF
#define RT_PAL_FREE        0xFFFE
#define RT_PAL_TAG_BASE    0x7700
#define RT_PAL_KEY_FX(kind)   (0x100 + (kind))
#define RT_PAL_KEY_ICON(idx)  (0x200 + (idx))
#define RT_PAL_KEY_BALL(ball) (0x300 + (ball))
#define RT_PAL_KEY_DS(key,shiny) (0x1000+(key)*2+(shiny))
#define RT_MIN_PAL_SLOTS   4 // two Pokemon icons + a Poke Ball
#define RT_ENABLE_ARENAS FALSE
#define RT_FIELD_LEFT (-110)
#define RT_FIELD_RIGHT 350
#define RT_FIELD_TOP (-48)
#define RT_FIELD_BOTTOM 184
#define RT_ARENA_LEFT   10
#define RT_ARENA_RIGHT  230
#define RT_ARENA_TOP    32
#define RT_ARENA_BOTTOM 120

#define RT_PLAYER_SPEED (Q8(1) + 128) // 1.5 px per frame
#define RT_PROJ_SPEED   (Q8(3) + 64)
#define RT_PROJ_LIFE    70
#define RT_CONTACT_RANGE 30
#define RT_HIT_RADIUS   14
#define RT_LUNGE_FRAMES 10


// HUD palette (BG palette 13 is saved and restored around the battle).
#define C_TRANSPARENT 0
#define C_WHITE       1
#define C_DARK        2
#define C_SHADOW      3
#define C_HP_GREEN    4
#define C_HP_YELLOW   5
#define C_HP_RED      6
#define C_BAR_BG      7
#define C_EXP         8
#define C_PANEL_LIGHT 9
#define C_BLACK       10
#define C_PANEL_DARK  11
#define C_GRAY_TEXT   12
#define C_ACCENT      13
#define C_LOW_PP      14
#define C_BORDER      15

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

#include "realtime_engine_types.inc.c"

struct RtFighter
{
    struct Pokemon *mon;
    s32 x, y;          // icon centre, Q8 pixels relative to the initial field view
    s32 kbX, kbY;      // knockback velocity, Q8
    u16 species;
    u16 hp;
    u16 maxHp;
    u16 shownHp;
    u16 aiMove;
    u16 pendingMove;
    u8 level;
    u8 spriteId;
    u8 palSlot;
    u8 dir;
    u8 lastHorizDir;
    u16 cooldown; // recovery shared by all actions
    u16 recoveryMax;
    u16 moveCooldown[MAX_MON_MOVES];
    u16 moveCooldownMax[MAX_MON_MOVES];
    u16 struggleCooldown;
    u8 hurtTimer;
    u8 lungeTimer;
    u8 windup;
    u8 aiTimer;
    u8 aiSlot, dashFrames;
    s8 aiStrafe;
    bool8 aiRanged;
    u16 dashCooldown;
    s16 dashVx, dashVy;
    u8 partyIndex;
    s8 aiDx, aiDy;
    s8 stages[RT_STAT_COUNT];
    bool8 isEnemy;
    bool8 active;
    u32 status;
    u16 statusTick, statusTimer, confusionTimer, yawnTimer;
    u8 toxicCount, statusSpriteId, confusionSpriteId;
    u16 clocks[RTC_COUNT];
    struct RtCast cast;
    u16 overrideMoves[4];
    u8 overridePp[4], overrideMaxPp[4], overrideMask;
    u16 copiedStats[5];
    u8 types[2], ability, originalAbility;
    u16 item, recycledItem;
    bool8 transformed, hidden;
    u16 lastMove, disabledMove, encoredMove, choiceMove;
    u8 lastSlot, stockpile, rollout, perishOwner;
    u16 sequenceMove, actionsUsed;
    u8 attractParty;
    bool8 defenseCurl, rage, flashFire, entryApplied;
    u16 recentDamage[8], historyClock, bideDamage;
    u8 historySlot;
    u16 residualClock;
    u8 visualGlow, glowSprite;
    u16 spriteKey, walkClock;
    s32 renderedX, renderedY;
    bool8 visualShiny;
    u8 arenaShadow;
    bool8 arenaShadowLive;
    u16 noEscapeTimer;
    u16 pmdClock, pmdCastMove;
    u8 pmdCastAnim;
    s8 faceX, faceY;   // eight-way facing (0,0 = use dir)
};

struct RtFx
{
    bool8 active;
    u8 kind;
    u8 spriteId;
    u8 frame;
    u8 frameTimer;
    u8 life;
    bool8 loop;
    u16 ptdAnim, ptdAge;
    u8 ptdPal, ptdMatrix, followSide, followParty;
    s16 offsetX, offsetY;
};

struct RtProjectile
{
    bool8 active;
    bool8 fromEnemy;
    u16 move;
    s32 x, y, vx, vy;
    u8 life;
    u8 fxId;
    u16 distance, maxDistance, fixedDamage;
    u8 sourceParty;
    u8 fieldKind, spriteId;
    s16 artOffsetX, artOffsetY, artEndY;
    u16 artRange;
    u8 artFront;
    u8 age;        // updates since fired, for AI reaction time
    u8 aiDecision; // 0 not judged yet, 1 dodge it, 2 let it through
};

struct RtBattle
{
    struct RtFighter player;
    struct RtFighter enemy;
    struct RtProjectile proj[RT_MAX_PROJ];
    struct RtFx fx[RT_MAX_FX];
    u8 fxRefCount[FX_COUNT];
    u8 fxPalSlot[FX_COUNT];
    u16 palKey[16];
    u8 palRef[16];
    bool8 palFromAlloc[16];
    bool8 palSaved[16];
    u16 palSavedUnfaded[16][16];
    u16 palSavedFaded[16][16];
    u16 statusTileStart;
    u8 statusPalSlot;
    u8 winIds[RT_WIN_COUNT];
    u16 savedPalUnfaded[16];
    u16 savedPalFaded[16];
    u16 savedFramePal[16], savedFramePalFaded[16];
    u32 lastVblank;
    u16 frameStep;
    u8 state;
    u8 afterMsgState;
    u16 timer;
    u16 countdown;
    u8 delaySeconds;
    u8 aiDifficulty;
    u8 shownNumber;
    u8 outcome;
    u8 leveledUpMask;
    u8 participantsMask;
    u8 expParticipantsMask;
    // messages
    u8 msgQueue[RT_MSG_MAX][RT_MSG_LEN];
    u8 msgHead;
    u8 msgCount;
    u16 msgTimer;
    bool8 msgVisible, msgCompact;
    u16 transientTimer;
    // HUD state
    bool8 dirtyEnemy;
    bool8 dirtyPlayer;
    bool8 dirtyMoves;
    bool8 dirtyEnemyBar;
    u8 hudSlot;
    bool8 halfRowReady;
    u8 halfRowClass[256];
    struct Pokemon *cacheMon[2];
    u32 cacheStamp[2];
    u16 cacheMoves[2][MAX_MON_MOVES];
    u8 cachePp[2][MAX_MON_MOVES];
    u16 shownTenths[MAX_MON_MOVES];
    bool8 dirtyPlayerBar;
    u8 hudTick;
    u16 moveSlotRecovery;
    u32 fightFrames;
    u16 pickerBalls[12];
    u8 pickerCount, pickerCursor, pickerTab, pickerReturnState;
    u16 pickerReturnTimer;
    bool8 masterConfirm;
    bool8 spriteAnimPaused[MAX_SPRITES];
    bool8 spriteAffinePaused[MAX_SPRITES];
    u8 moveSlotState[MAX_MON_MOVES];
    u8 moveSlotPp[MAX_MON_MOVES];
    // ball throw
    u8 ballSpriteId;
    u8 ballPalSlot;
    u8 ballId;
    u16 ballItem;
    u8 ballState;
    u8 ballShakesTotal;
    u8 ballShakesDone;
    bool8 ballSuccess;
    s32 ballX, ballY, ballStartX, ballStartY, ballEndX, ballEndY;
    s16 cameraX,cameraY;
    bool8 cameraActive,worldSprites[MAX_SPRITES];
    bool8 collisionReady;
    s16 collisionLeft,collisionTop;
    u8 collision[32*20],edges[32*20];
    // map origin
    s16 originX, originY; // screen pixel centre of the player's tile
    s16 tileX, tileY;     // player's map tile
    bool8 isTrainer, arena, arenaActive, trainerSpeechShown;
    u8 arenaPortrait[2], arenaPortraitPal[2];
    u16 legendarySpecies;
    u8 legendaryPhase;
    u8 afterSpeechState;
    s16 enemySpawnX, enemySpawnY;
    s8 wildPlayerDx, wildPlayerDy, wildEnemyDx, wildEnemyDy;
    struct RtField field[8];
    struct RtPresentation presentation;
    u8 weather, savedWeather, weatherFx, afterPresentation;
    u16 weatherTimer, weatherTick, fieldTiles;
    u8 weatherSprites[12];
    u16 weatherTiles;
    u32 weatherClock;
    u8 forcedEnemy;
    bool8 enemyBaton;
    bool8 partyPicker;
    bool8 playerReturned;
    u8 switchTarget;
    u16 switchRecovery, switchMoveCd[PARTY_SIZE][4];
    s8 batonStages[RT_STAT_COUNT];
    bool8 batonPass;
    u32 payday;
    u8 trainerItemsUsed;
    u16 trainerItemTimer;
    u16 learnMoves[64];
    u8 learnParty[64];
    u8 learnCount, learnIndex, learnCursor;
    bool8 learnConfirm;
    u8 spaceActors[2], spaceBack[2], spaceSteps[2], spaceSettle;
    s16 spaceOriginalX[2], spaceOriginalY[2];
    bool8 spaceStarted;
    u8 arenaTheme, arenaObjectCount, arenaArtPhase, arenaMatrices[9];
    u32 arenaClock;
    u16 arenaContactCooldown[2];
    struct RtArenaObject arenaObjects[16];

};

struct RtFxDef
{
    u16 tag;
    u8 shape;
    u8 frames;
    u8 frameDur;
    u8 tilesPerFrame;
    u8 tileOffset;
};

struct RtStatEffect
{
    u8 effect;
    u8 stat;
    s8 amount;
    bool8 self;
};

// ---------------------------------------------------------------------------
// Data
// ---------------------------------------------------------------------------

EWRAM_DATA bool8 gRealtimeBattleActive = FALSE;
static EWRAM_DATA struct RtBattle *sRt = NULL;
#ifdef PMD_TEST
u16 gRtPerf[8]; u16 gRtCnt[10];
#endif
static EWRAM_DATA MainCallback sRtTrainerEnd = NULL;
static EWRAM_DATA MainCallback sRtLegendEnd = NULL;

static const struct RtFxDef sFxDefs[FX_COUNT] =
{
    [FX_IMPACT]  = {ANIM_TAG_IMPACT,         FX_SHAPE_32x32, 1, 8, 16},
    [FX_EMBER]   = {ANIM_TAG_SMALL_EMBER,    FX_SHAPE_32x32, 5, 4, 16},
    [FX_WATER]   = {ANIM_TAG_WATER_ORB,      FX_SHAPE_16x16, 4, 4, 4},
    [FX_SPARK]   = {ANIM_TAG_SPARK_2,        FX_SHAPE_16x16, 3, 3, 4},
    [FX_LEAF]    = {ANIM_TAG_LEAF,           FX_SHAPE_16x16, 9, 3, 4},
    [FX_ICE]     = {ANIM_TAG_ICE_CHUNK,      FX_SHAPE_32x32, 5, 4, 16},
    [FX_POISON]  = {ANIM_TAG_POISON_BUBBLE,  FX_SHAPE_16x16, 3, 5, 4},
    [FX_SHADOW]  = {ANIM_TAG_SHADOW_BALL,    FX_SHAPE_32x32, 1, 8, 16},
    [FX_ROCK]    = {ANIM_TAG_ROCKS,          FX_SHAPE_32x32, 6, 4, 16},
    [FX_PSY_MARK] = {ANIM_TAG_PURPLE_RING, FX_SHAPE_32x16, 1, 8, 8},
    [FX_PSYCHIC] = {ANIM_TAG_BLUEGREEN_ORB,  FX_SHAPE_16x16, 1, 8, 4},
    [FX_DRAGON]  = {ANIM_TAG_PURPLE_FLAME,   FX_SHAPE_16x32, 4, 4, 8},
    [FX_FEATHER] = {ANIM_TAG_WHITE_FEATHER,  FX_SHAPE_32x32, 2, 6, 16},
    [FX_FIST]    = {ANIM_TAG_HANDS_AND_FEET, FX_SHAPE_32x32, 1, 8, 16},
    [FX_SCRATCH] = {ANIM_TAG_SCRATCH,        FX_SHAPE_32x32, 5, 3, 16},
    [FX_SPARKLE] = {ANIM_TAG_SPARKLE_2,      FX_SHAPE_32x32, 8, 3, 16},
    [FX_SMOKE]   = {ANIM_TAG_GRAY_SMOKE,     FX_SHAPE_32x32, 5, 5, 16},
    [FX_ORB]     = {ANIM_TAG_RED_ORB,        FX_SHAPE_16x16, 1, 8, 4},
    [FX_CHOP] = {ANIM_TAG_HANDS_AND_FEET, FX_SHAPE_32x32, 1, 8, 16, 48},
    [FX_KICK] = {ANIM_TAG_HANDS_AND_FEET, FX_SHAPE_32x32, 1, 8, 16, 16},
    [FX_COIN] = {ANIM_TAG_COIN, FX_SHAPE_16x16, 4, 3, 4},
    [FX_NEEDLE] = {ANIM_TAG_NEEDLE, FX_SHAPE_16x16, 1, 8, 4},
    [FX_ROOT] = {ANIM_TAG_ROOTS, FX_SHAPE_32x32, 4, 4, 16},
    [FX_BONE] = {ANIM_TAG_BONE, FX_SHAPE_32x32, 1, 8, 16},
    [FX_WIND] = {ANIM_TAG_AIR_WAVE, FX_SHAPE_32x16, 1, 5, 8},
    [FX_HEART] = {ANIM_TAG_RED_HEART, FX_SHAPE_16x16, 1, 8, 4},
};

// Projectile graphic and hit graphic per move type.
static const u8 sTypeFx[NUMBER_OF_MON_TYPES][2] =
{
    [TYPE_NORMAL]   = {FX_ORB,     FX_IMPACT},
    [TYPE_FIGHTING] = {FX_FIST,    FX_FIST},
    [TYPE_FLYING]   = {FX_FEATHER, FX_FEATHER},
    [TYPE_POISON]   = {FX_POISON,  FX_POISON},
    [TYPE_GROUND]   = {FX_ROCK,    FX_SMOKE},
    [TYPE_ROCK]     = {FX_ROCK,    FX_ROCK},
    [TYPE_BUG]      = {FX_LEAF,    FX_SCRATCH},
    [TYPE_GHOST]    = {FX_SHADOW,  FX_SHADOW},
    [TYPE_STEEL]    = {FX_ORB,     FX_IMPACT},
    [TYPE_MYSTERY]  = {FX_ORB,     FX_IMPACT},
    [TYPE_FIRE]     = {FX_EMBER,   FX_EMBER},
    [TYPE_WATER]    = {FX_WATER,   FX_WATER},
    [TYPE_GRASS]    = {FX_LEAF,    FX_LEAF},
    [TYPE_ELECTRIC] = {FX_SPARK,   FX_SPARK},
    [TYPE_PSYCHIC]  = {FX_PSYCHIC, FX_SPARKLE},
    [TYPE_ICE]      = {FX_ICE,     FX_ICE},
    [TYPE_DRAGON]   = {FX_DRAGON,  FX_DRAGON},
    [TYPE_DARK]     = {FX_SHADOW,  FX_SCRATCH},
};

#include "data/realtime_move_profiles.h"
#include "data/realtime_ptd_graphics.h"
#include "data/realtime_ptd_emitters.h"

static const u8 sStatStageRatios[13][2] =
{
    {10, 40}, {10, 35}, {10, 30}, {10, 25}, {10, 20}, {10, 15}, {10, 10},
    {15, 10}, {20, 10}, {25, 10}, {30, 10}, {35, 10}, {40, 10},
};

static const u8 sAccuracyStageRatios[13][2] =
{
    {33, 100}, {36, 100}, {43, 100}, {50, 100}, {60, 100}, {75, 100}, {1, 1},
    {133, 100}, {166, 100}, {2, 1}, {233, 100}, {133, 50}, {3, 1},
};

static const struct RtStatEffect sStatEffects[] =
{
    {EFFECT_ATTACK_UP,              RT_STAT_ATK,      1, TRUE},
    {EFFECT_DEFENSE_UP,             RT_STAT_DEF,      1, TRUE},
    {EFFECT_SPEED_UP,               RT_STAT_SPEED,    1, TRUE},
    {EFFECT_SPECIAL_ATTACK_UP,      RT_STAT_SPATK,    1, TRUE},
    {EFFECT_SPECIAL_DEFENSE_UP,     RT_STAT_SPDEF,    1, TRUE},
    {EFFECT_ACCURACY_UP,            RT_STAT_ACC,      1, TRUE},
    {EFFECT_EVASION_UP,             RT_STAT_EVASION,  1, TRUE},
    {EFFECT_ATTACK_UP_2,            RT_STAT_ATK,      2, TRUE},
    {EFFECT_DEFENSE_UP_2,           RT_STAT_DEF,      2, TRUE},
    {EFFECT_SPEED_UP_2,             RT_STAT_SPEED,    2, TRUE},
    {EFFECT_SPECIAL_ATTACK_UP_2,    RT_STAT_SPATK,    2, TRUE},
    {EFFECT_SPECIAL_DEFENSE_UP_2,   RT_STAT_SPDEF,    2, TRUE},
    {EFFECT_ACCURACY_UP_2,          RT_STAT_ACC,      2, TRUE},
    {EFFECT_EVASION_UP_2,           RT_STAT_EVASION,  2, TRUE},
    {EFFECT_DEFENSE_CURL,           RT_STAT_DEF,      1, TRUE},
    {EFFECT_MINIMIZE,               RT_STAT_EVASION,  1, TRUE},
    {EFFECT_BULK_UP,                RT_STAT_ATK,      1, TRUE},
    {EFFECT_BULK_UP,                RT_STAT_DEF,      1, TRUE},
    {EFFECT_CALM_MIND,              RT_STAT_SPATK,    1, TRUE},
    {EFFECT_CALM_MIND,              RT_STAT_SPDEF,    1, TRUE},
    {EFFECT_DRAGON_DANCE,           RT_STAT_ATK,      1, TRUE},
    {EFFECT_DRAGON_DANCE,           RT_STAT_SPEED,    1, TRUE},
    {EFFECT_COSMIC_POWER,           RT_STAT_DEF,      1, TRUE},
    {EFFECT_COSMIC_POWER,           RT_STAT_SPDEF,    1, TRUE},
    {EFFECT_ATTACK_DOWN,            RT_STAT_ATK,     -1, FALSE},
    {EFFECT_DEFENSE_DOWN,           RT_STAT_DEF,     -1, FALSE},
    {EFFECT_SPEED_DOWN,             RT_STAT_SPEED,   -1, FALSE},
    {EFFECT_SPECIAL_ATTACK_DOWN,    RT_STAT_SPATK,   -1, FALSE},
    {EFFECT_SPECIAL_DEFENSE_DOWN,   RT_STAT_SPDEF,   -1, FALSE},
    {EFFECT_ACCURACY_DOWN,          RT_STAT_ACC,     -1, FALSE},
    {EFFECT_EVASION_DOWN,           RT_STAT_EVASION, -1, FALSE},
    {EFFECT_ATTACK_DOWN_2,          RT_STAT_ATK,     -2, FALSE},
    {EFFECT_DEFENSE_DOWN_2,         RT_STAT_DEF,     -2, FALSE},
    {EFFECT_SPEED_DOWN_2,           RT_STAT_SPEED,   -2, FALSE},
    {EFFECT_SPECIAL_ATTACK_DOWN_2,  RT_STAT_SPATK,   -2, FALSE},
    {EFFECT_SPECIAL_DEFENSE_DOWN_2, RT_STAT_SPDEF,   -2, FALSE},
    {EFFECT_ACCURACY_DOWN_2,        RT_STAT_ACC,     -2, FALSE},
    {EFFECT_EVASION_DOWN_2,         RT_STAT_EVASION, -2, FALSE},
    {EFFECT_TICKLE,                 RT_STAT_ATK,     -1, FALSE},
    {EFFECT_TICKLE,                 RT_STAT_DEF,     -1, FALSE},
};

static const u16 sHudPalette[16] =
{
    RGB(0, 0, 0),     // transparent
    RGB(7, 7, 8),     // foreground on native white windows
    RGB(7, 7, 8),     // dark text
    RGB(21, 21, 22),  // light shadow
    RGB(7, 26, 10),   // HP green
    RGB(30, 25, 4),   // HP yellow
    RGB(29, 7, 6),    // HP red
    RGB(9, 10, 11),   // bar background
    RGB(8, 20, 31),   // EXP blue
    RGB(31, 31, 31),  // native white panel
    RGB(26, 26, 27),  // text shadow
    RGB(31, 31, 31),  // native white panel
    RGB(14, 14, 16),  // greyed text
    RGB(6, 12, 24),   // blue button labels
    RGB(25, 3, 3),    // low PP
    RGB(10, 14, 23),  // blue frame accent
};

// BG0 characters start at VRAM + 0x8000. Map screen blocks start at
// 0xE000 (tile 0x300 relative to BG0). The old message window ended at
// tile 0x304 and overwrote the map. Keep every HUD tile below the field
// dialogue window (0x198), with disjoint, size-derived allocations.
#define RT_TILES_ENEMY  0x080
#define RT_TILES_PLAYER (RT_TILES_ENEMY + 13 * 2)
#define RT_TILES_MOVES  (RT_TILES_PLAYER + 13 * 2)
#define RT_TILES_MSG    (RT_TILES_MOVES + 28 * 5)
#define RT_TILES_HINT   (RT_TILES_MSG + 26 * 3)
typedef char RtHudTilesFit[(RT_TILES_HINT + 4 * 2 <= 0x198) ? 1 : -1];
// The picker is modal: its tiles intentionally replace HUD tiles while the
// simulation is paused. Closing it redraws the HUD from its separate buffers.
typedef char RtPickerTilesFit[(1 + 28 * 17 <= RT_MENU_FRAME_TILES && RT_MENU_FRAME_TILES + 9 < 0x200) ? 1 : -1];
typedef char RtLiveFrameTilesFit[(RT_FRAME_TILES+9<=RT_TILES_ENEMY)?1:-1];
static const struct WindowTemplate sRtWindowTemplates[RT_WIN_COUNT] =
{
    [RT_WIN_ENEMY]  = {.bg = 0, .tilemapLeft = 17, .tilemapTop = 0, .width = 13, .height = 2, .paletteNum = RT_HUD_PAL, .baseBlock = RT_TILES_ENEMY},
    [RT_WIN_PLAYER] = {.bg = 0, .tilemapLeft = 0, .tilemapTop = 0, .width = 13, .height = 2, .paletteNum = RT_HUD_PAL, .baseBlock = RT_TILES_PLAYER},
    [RT_WIN_MOVES]  = {.bg = 0, .tilemapLeft = 1, .tilemapTop = 14, .width = 28, .height = 5, .paletteNum = RT_HUD_PAL, .baseBlock = RT_TILES_MOVES},
    [RT_WIN_MSG]    = {.bg = 0, .tilemapLeft = 2, .tilemapTop = 3, .width = 26, .height = 3, .paletteNum = RT_HUD_PAL, .baseBlock = RT_TILES_MSG},
    [RT_WIN_HINT]   = {.bg = 0, .tilemapLeft = 13, .tilemapTop = 0, .width = 4, .height = 2, .paletteNum = RT_HUD_PAL, .baseBlock = RT_TILES_HINT},
    [RT_WIN_MENU]   = {.bg = 0, .tilemapLeft = 1, .tilemapTop = 1, .width = 28, .height = 17, .paletteNum = RT_HUD_PAL, .baseBlock = 1},
};

static const u8 sTextDark[3]   = {C_PANEL_LIGHT, C_DARK, C_SHADOW};
static const u8 sTextWhite[3]  = {C_PANEL_LIGHT, C_DARK, C_SHADOW};
static const u8 sTextGray[3]   = {C_PANEL_LIGHT, C_GRAY_TEXT, C_SHADOW};
static const u8 sTextAccent[3] = {C_PANEL_LIGHT, C_ACCENT, C_SHADOW};
static const u8 sTextRed[3]    = {C_PANEL_LIGHT, C_LOW_PP, C_SHADOW};

static const u8 sText_WildAppeared[]   = _("A wild {STR_VAR_1} appeared!");
static const u8 sText_Go[]             = _("Go! {STR_VAR_1}!");
static const u8 sText_GoExcl[]         = _("GO!");
static const u8 sText_Ready[]          = _("Ready… {STR_VAR_1}");
static const u8 sText_WildFainted[]    = _("Wild {STR_VAR_1} fainted!");
static const u8 sText_GainedExp[]      = _("{STR_VAR_1} gained {STR_VAR_2} EXP. Points!");
static const u8 sText_GainedBoosted[]  = _("{STR_VAR_1} gained a boosted {STR_VAR_2} EXP!");
static const u8 sText_GrewTo[]         = _("{STR_VAR_1} grew to {LV} {STR_VAR_2}!");
static const u8 sText_Learned[]        = _("{STR_VAR_1} learned {STR_VAR_2}!");
static const u8 sText_CantLearn[]      = _("{STR_VAR_1} can't learn {STR_VAR_2} (4 moves).");
static const u8 sText_Fainted[]        = _("{STR_VAR_1} fainted!");
static const u8 sText_OutOfMons[]      = _("{STR_VAR_1} is out of usable POKéMON!");
static const u8 sText_WhitedOut[]      = _("{STR_VAR_1} whited out!");
static const u8 sText_GotAway[]        = _("Got away safely!");
static const u8 sText_Gotcha[]         = _("Gotcha! {STR_VAR_1} was caught!");
static const u8 sText_SentToPc[]       = _("{STR_VAR_1} was sent to the PC.");
static const u8 sText_NoRoom[]         = _("There's no room for {STR_VAR_1}…");
static const u8 sText_BrokeFree[]      = _("Oh no! It broke free!");
static const u8 sText_NoBalls[]        = _("You have no POKé BALLS!");
static const u8 sText_SuperEffective[] = _("It's super effective!");
static const u8 sText_NotVery[]        = _("It's not very effective…");
static const u8 sText_NoEffect[]       = _("It doesn't affect {STR_VAR_1}…");
static const u8 sText_Crit[]           = _("A critical hit!");
static const u8 sText_Missed[]         = _("{STR_VAR_1}'s attack missed!");
static const u8 sText_StatRose[]       = _("{STR_VAR_1}'s stats rose!");
static const u8 sText_StatFell[]       = _("{STR_VAR_1}'s stats fell!");
static const u8 sText_NothingHappened[] = _("But nothing happened!");
static const u8 sText_Healed[]         = _("{STR_VAR_1} regained health!");
static const u8 sText_ButtonA[]        = _("A");
static const u8 sText_ButtonB[]        = _("B");
static const u8 sText_ButtonL[]        = _("L");
static const u8 sText_ButtonR[]        = _("R");
static const u8 sText_Dash[]           = _("-");
static const u8 sText_Slash[]          = _("/");
static const u8 sText_SelectHint[] = _("SEL");
static const u8 sText_BallHint[] = _("BALL");
static const u8 sText_TeamHint[] = _("TEAM");
static const u8 sText_MoveReady[] = _("RDY");
static const u8 sText_Recovery[] = _("REC ");
static const u8 sText_NoPp[] = _("0");
static const u8 sText_Seconds[] = _("s");
static const u8 sText_Decimal[] = _(".");
static const u8 sText_Lv[]             = _("{LV}");

static const u8 *const sButtonLabels[MAX_MON_MOVES] = {sText_ButtonA, sText_ButtonB, sText_ButtonL, sText_ButtonR};
static const u16 sMoveButtons[MAX_MON_MOVES] = {A_BUTTON, B_BUTTON, L_BUTTON, R_BUTTON};

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

bool32 IsMapNamePopupTaskActive(void); // map_name_popup.c

static void Task_RealtimeBattle(u8 taskId);
static void RtCleanup(void);
static const u8 *RtStatusLabel(struct RtFighter *f);
static void RtSetStatus(struct RtFighter *f, u32 status);
static void RtUpdateStatus(struct RtFighter *f);
static void RtUpdateStatusVisual(struct RtFighter *f);
static bool8 RtCanAct(struct RtFighter *f, u16 move);
static void RtInitStatusGraphics(void);
static bool8 RtStartBattleCommon(bool8 trainer);
static void RtResumeFight(void);
static bool8 RtTrainerNeedsArena(u16 id);
static void RtSetupArena(void);
static void RtOpenPicker(void);
static void RtUpdatePicker(void);
static void RtClosePicker(void);
static void RtStartBallThrow(u16 ball);
static void RtDestroyBall(void);
static void RtUpdateCast(struct RtFighter *f, struct RtFighter *target);
static void RtCancelCast(struct RtFighter *f);
static struct RtFighter *RtOther(struct RtFighter *f);
static bool8 RtInCast(struct RtFighter *f, struct RtFighter *target);
static void RtStopFlame(struct RtFighter *f);
static bool8 RtTypeIs(struct RtFighter *f,u8 type);
static void RtPtdDestroyFx(u8 id);
static void RtPtdUpdateFx(u8 id);
static bool8 RtPtdImpact(struct RtFighter *f,struct RtFighter *target,u16 move);
static void RtUpdateClocks(struct RtFighter *f);
static bool8 RtExtendedStatus(struct RtFighter *f, struct RtFighter *target, u16 move);
static void RtApplyImpact(struct RtFighter *f, struct RtFighter *target, u16 move, u16 fixed);
static bool8 RtFieldBlocks(struct RtFighter *f, s32 x, s32 y);
static void RtSeparateFighters(void);
static bool8 RtHitSubstitute(struct RtFighter *f, s32 x, s32 y, u16 damage);
static void RtUpdateField(void);
static void RtCleanupEngine(void);
static void RtBeginPresentation(struct RtFighter *f, u8 kind);
static void RtStopPresentation(bool8 reveal);
static void RtUpdatePresentation(void);
static void RtRenderCast(struct RtFighter *f, struct Sprite *sprite);
static void RtSetBattleWeather(u8 weather, u16 duration);
static void RtSwitchPlayer(u8 index);
static void RtForceEnemySwitch(u8 index,bool8 pass);
static void RtSetHp(struct RtFighter *f, s32 hp);
static s32 RtGetStat(struct RtFighter *f, u8 stat);
static void RtUseMove(struct RtFighter *atk, struct RtFighter *def, u16 move);
static void RtFace(struct RtFighter *f, struct RtFighter *target);
static void RtStage(struct RtFighter *f, u8 stat, s8 change);
static bool8 RtApplyStatusMove(struct RtFighter *atk, struct RtFighter *def, u16 move);
static u16 RtMultiplier(u8 type, struct RtFighter *f);
static bool8 RtWeatherWorks(void);
static bool8 RtSoundMove(u16 move);
static bool8 RtMoveAllowed(struct RtFighter *f, u16 move);
static void RtUpdateWeather(void);
static void RtLaunchCast(struct RtFighter *f, struct RtFighter *target, u16 move);
static void RtFixedShot(struct RtFighter *f, struct RtFighter *target, u16 move, u16 damage);
static bool8 RtAiLineClear(s32 x,s32 y,s32 tx,s32 ty);
static void RtDrawField(struct RtField *field);
static void RtAim(struct RtFighter *f,struct RtFighter *target,s32 *dx,s32 *dy);
static void RtPartyStrike(struct RtFighter *f,struct Pokemon *mon,s16 x,s16 y);
static u8 RtSpawnFx(u8 kind, s16 x, s16 y, u8 life, bool8 loop);
static void RtShowTransient(const u8 *str, u16 frames);
static bool8 RtTypeIs(struct RtFighter *f, u8 type);
static void RtInitArena(void);
static void RtDrawArena(void);
static void RtUpdateArena(void);
static void RtFreeArena(void);
static void RtCreateArenaShadow(struct RtFighter *f);
static void RtRemoveArenaShadow(struct RtFighter *f);
static bool8 RtArenaBlocked(s32 x,s32 y);
static bool8 RtArenaLineClear(s32 x,s32 y,s32 tx,s32 ty);
static s16 RtArenaTrace(s32 x,s32 y,s32 tx,s32 ty);
static bool8 RtArenaHitCover(s32 x,s32 y,s32 tx,s32 ty,u16 move);
static s32 RtArenaDangerAt(struct RtFighter *f,s32 x,s32 y);
static bool8 RtArenaCanBreakLine(struct RtFighter *f,struct RtFighter *target,u16 move);




// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

static s32 RtAbs(s32 v)
{
    return v < 0 ? -v : v;
}

// Cheap vector length (octagonal approximation), input and output in the same units.
static s32 RtLength(s32 dx, s32 dy)
{
    s32 ax = RtAbs(dx), ay = RtAbs(dy);
    if (ax > ay)
        return ax + (ay * 3) / 8;
    return ay + (ax * 3) / 8;
}

static void RtNormalize(s32 dx, s32 dy, s32 speed, s32 *outX, s32 *outY)
{
    s32 len = RtLength(dx, dy);
    if (len == 0)
    {
        *outX = 0;
        *outY = 0;
        return;
    }
    *outX = dx * speed / len;
    *outY = dy * speed / len;
}

static void RtDirVector(u8 dir, s32 *dx, s32 *dy)
{
    *dx = 0;
    *dy = 0;
    switch (dir)
    {
    case RT_DIR_DOWN:  *dy = 1;  break;
    case RT_DIR_UP:    *dy = -1; break;
    case RT_DIR_LEFT:  *dx = -1; break;
    case RT_DIR_RIGHT: *dx = 1;  break;
    }
}

// Eight-way facing: diagonal input is kept as the facing until another direction is chosen.
static void RtFaceVector(struct RtFighter *f, s32 *dx, s32 *dy)
{
    if (f->faceX || f->faceY) { *dx = f->faceX; *dy = f->faceY; }
    else RtDirVector(f->dir, dx, dy);
}

static u8 RtDirFromVector(s32 dx, s32 dy)
{
    if (RtAbs(dx) >= RtAbs(dy))
        return dx < 0 ? RT_DIR_LEFT : RT_DIR_RIGHT;
    return dy < 0 ? RT_DIR_UP : RT_DIR_DOWN;
}

static void RtGetNickname(struct Pokemon *mon, u8 *dest)
{
    GetMonData(mon, MON_DATA_NICKNAME, dest);
    StringGet_Nickname(dest);
}

static bool8 RtIsUsableMon(struct Pokemon *mon)
{
    return GetMonData(mon, MON_DATA_SPECIES) != SPECIES_NONE
        && !GetMonData(mon, MON_DATA_IS_EGG)
        && GetMonData(mon, MON_DATA_HP) != 0;
}

#include "realtime_combat_util.inc.c"
#include "realtime_mechanics.inc.c"

static s32 RtGetStat(struct RtFighter *f, u8 stat)
{
    static const u8 sMonDataIds[] = {MON_DATA_ATK, MON_DATA_DEF, MON_DATA_SPEED, MON_DATA_SPATK, MON_DATA_SPDEF};
    s32 value = f->transformed ? f->copiedStats[stat] : GetMonData(f->mon, sMonDataIds[stat]);
    s8 stage = f->stages[stat] + 6;
    value = value * sStatStageRatios[stage][0] / sStatStageRatios[stage][1];
    return value > 0 ? value : 1;
}

static s32 RtGetMoveSpeed(struct RtFighter *f)
{
    s32 speed;
    s8 stage = f->stages[RT_STAT_SPEED] + 6;

    if (f->isEnemy)
    {
        speed = 150 + (f->transformed?f->copiedStats[RT_STAT_SPEED]:GetMonData(f->mon, MON_DATA_SPEED)) * 2;
        if (speed > 330)
            speed = 330;
    }
    else
    {
        speed = RT_PLAYER_SPEED;
    }
    if (f->isEnemy && sRt->aiDifficulty >= RT_AI_HARD) speed = speed * 5 / 4;
    speed = speed * sStatStageRatios[stage][0] / sStatStageRatios[stage][1];
    if (f->status & STATUS1_PARALYSIS) speed /= 4;
    if (f->clocks[RTC_SLOW]) speed /= 2;
    if (f->clocks[RTC_RAMPAGE]) speed = speed * 6 / 5;
    if(RtWeatherWorks() && ((f->ability==ABILITY_SWIFT_SWIM && sRt->weather==RTW_RAIN) || (f->ability==ABILITY_CHLOROPHYLL && sRt->weather==RTW_SUN))) speed=speed*3/2;
    return speed > 0 ? speed : 1;
}

// ---------------------------------------------------------------------------
// Map collision
// ---------------------------------------------------------------------------

static bool8 RtSpacingForbidden(u8 behavior);

static bool8 RtReadTileBlocked(s16 tx, s16 ty)
{
    u8 behavior;
    bool8 water;

    if (TRUE)
    {
        u8 elevation;
        if (tx < MAP_OFFSET || ty < MAP_OFFSET
         || tx >= gMapHeader.mapLayout->width + MAP_OFFSET
         || ty >= gMapHeader.mapLayout->height + MAP_OFFSET) return TRUE;
        behavior = MapGridGetMetatileBehaviorAt(tx, ty);
        elevation = MapGridGetElevationAt(tx, ty);
        if (RtSpacingForbidden(behavior)
         || (elevation != 0 && elevation != 15
          && gObjectEvents[gPlayerAvatar.objectEventId].currentElevation != 0
          && elevation != gObjectEvents[gPlayerAvatar.objectEventId].currentElevation)) return TRUE;
    }
    behavior = MapGridGetMetatileBehaviorAt(tx, ty);
    // Trainer battles on Surf can use the visible water. Keep obstacle and
    // NPC collision, and leave the base movement-speed rules unchanged.
    water = TestPlayerAvatarFlags(PLAYER_AVATAR_FLAG_SURFING) && MetatileBehavior_IsSurfable(behavior);
    if (MapGridGetCollisionAt(tx, ty) && !water)
        return TRUE;
    if ((MetatileBehavior_IsSurfable(behavior) && !water)
     || MetatileBehavior_IsJumpEast(behavior)
     || MetatileBehavior_IsJumpWest(behavior)
     || MetatileBehavior_IsJumpNorth(behavior)
     || MetatileBehavior_IsJumpSouth(behavior))
        return TRUE;
    if (GetObjectEventIdByXY(tx, ty) != OBJECT_EVENTS_COUNT
     && !(tx == sRt->tileX && ty == sRt->tileY))
        return TRUE;
    return FALSE;
}

static bool8 RtIsTileBlocked(s16 tx,s16 ty)
{
    s16 x=tx-sRt->collisionLeft,y=ty-sRt->collisionTop;
    if(sRt->collisionReady && x>=0 && x<32 && y>=0 && y<20)return sRt->collision[y*32+x];
    return RtReadTileBlocked(tx,ty);
}

// Screen pixel -> map tile.
static void RtScreenToTile(s32 sx, s32 sy, s16 *tx, s16 *ty)
{
    *tx = sRt->tileX + ((sx - (sRt->originX - 8)) >> 4);
    *ty = sRt->tileY + ((sy - (sRt->originY - 8)) >> 4);
}

// Checks the body of a Pokemon centred at (sx, sy) (whole pixels): a box from
// 6 px above to 6 px below the centre, 10 px wide. Only checking the feet let
// the sprite's upper half sink about a tile into trees, fences and people.
static bool8 RtIsSpotWalkable(s32 sx, s32 sy)
{
    s16 tx, ty;

    if (sRt->arena)
        return sx>=RT_ARENA_LEFT*2 && sx<=RT_ARENA_RIGHT*2 && sy>=RT_ARENA_TOP*2 && sy<=RT_ARENA_BOTTOM*2 && !RtArenaBlocked(sx,sy);
    if (sx < RT_FIELD_LEFT || sx > RT_FIELD_RIGHT || sy < RT_FIELD_TOP || sy > RT_FIELD_BOTTOM)
        return FALSE;
    RtScreenToTile(sx - 5, sy + 6, &tx, &ty);
    if (RtIsTileBlocked(tx, ty))
        return FALSE;
    RtScreenToTile(sx + 5, sy + 6, &tx, &ty);
    if (RtIsTileBlocked(tx, ty))
        return FALSE;
    RtScreenToTile(sx - 5, sy - 6, &tx, &ty);
    if (RtIsTileBlocked(tx, ty))
        return FALSE;
    RtScreenToTile(sx + 5, sy - 6, &tx, &ty);
    if (RtIsTileBlocked(tx, ty))
        return FALSE;
    return TRUE;
}

#include "realtime_wild_space.inc.c"
#include "realtime_overworld_camera.inc.c"

// Keep Pokemon bodies apart; never let one walk into the other.
#define RT_BODY_SEP_X 20 // below the 22 px a dash needs to connect
#define RT_BODY_SEP_Y 14
static bool8 RtBodiesOverlap(s32 ax, s32 ay, s32 bx, s32 by)
{
    return RtAbs(ax - bx) < RT_BODY_SEP_X && RtAbs(ay - by) < RT_BODY_SEP_Y;
}

static bool8 RtBodyBlocks(struct RtFighter *f, s32 nx, s32 ny)
{
    struct RtFighter *o = RtOther(f);
    s32 ox, oy, x, y;
    if (!o || !o->active || !o->hp) return FALSE;
    ox = FROM_Q8(o->x); oy = FROM_Q8(o->y); x = FROM_Q8(f->x); y = FROM_Q8(f->y);
    // Moving away from an overlap is always allowed, so nobody gets stuck.
    return RtBodiesOverlap(nx, ny, ox, oy) && RtAbs(nx - ox) + RtAbs(ny - oy) < RtAbs(x - ox) + RtAbs(y - oy);
}

static bool8 RtCanStep(struct RtFighter *f, s32 nx, s32 ny)
{
    // A fighter that somehow starts inside a wall may always move out of it.
    if (!RtIsSpotWalkable(nx, ny) && RtIsSpotWalkable(FROM_Q8(f->x), FROM_Q8(f->y))) return FALSE;
    if (RtFieldBlocks(f, nx, ny) || RtBodyBlocks(f, nx, ny)) return FALSE;
    return sRt->arena || RtWildStepClear(FROM_Q8(f->x), FROM_Q8(f->y), nx, ny);
}


// Moves with sliding along walls. Returns FALSE if fully blocked.
static bool8 RtMoveFighter(struct RtFighter *f, s32 dx, s32 dy)
{
    bool8 moved = FALSE;

    s8 off;

    if (RtPsychicHeld(f)) return FALSE;
    if (dx != 0)
    {
        if (RtCanStep(f, FROM_Q8(f->x + dx), FROM_Q8(f->y))) { f->x += dx; moved = TRUE; }
        else if (dy == 0)
        {
            // Corner sliding: when a straight move is blocked by an edge, slip
            // up to 7 px sideways so 1-tile gaps are easy to walk into.
            for (off = 1; off <= 7; off++)
            {
                if (RtCanStep(f, FROM_Q8(f->x + dx), FROM_Q8(f->y) - off) && RtCanStep(f, FROM_Q8(f->x), FROM_Q8(f->y) - 1)) { f->y -= Q8(1); moved = TRUE; break; }
                if (RtCanStep(f, FROM_Q8(f->x + dx), FROM_Q8(f->y) + off) && RtCanStep(f, FROM_Q8(f->x), FROM_Q8(f->y) + 1)) { f->y += Q8(1); moved = TRUE; break; }
            }
        }
    }
    if (dy != 0)
    {
        if (RtCanStep(f, FROM_Q8(f->x), FROM_Q8(f->y + dy))) { f->y += dy; moved = TRUE; }
        else if (dx == 0)
        {
            for (off = 1; off <= 7; off++)
            {
                if (RtCanStep(f, FROM_Q8(f->x) - off, FROM_Q8(f->y + dy)) && RtCanStep(f, FROM_Q8(f->x) - 1, FROM_Q8(f->y))) { f->x -= Q8(1); moved = TRUE; break; }
                if (RtCanStep(f, FROM_Q8(f->x) + off, FROM_Q8(f->y + dy)) && RtCanStep(f, FROM_Q8(f->x) + 1, FROM_Q8(f->y))) { f->x += Q8(1); moved = TRUE; break; }
            }
        }
    }
    return moved;
}

// ---------------------------------------------------------------------------
// Sprite palettes
// ---------------------------------------------------------------------------

// Marks which OBJ palette slots are borrowable (reserved but unused by any sprite)
// and returns how many palettes the battle can use in total.
static u8 RtScanPalSlots(u16 *palKey)
{
    bool8 used[16];
    u8 i, count = 0;

    for (i = 0; i < 16; i++)
        used[i] = FALSE;
    for (i = 0; i < MAX_SPRITES; i++)
    {
        if (gSprites[i].inUse)
            used[gSprites[i].oam.paletteNum] = TRUE;
    }
    for (i = 0; i < 16; i++)
    {
        if (i < gReservedSpritePaletteCount && !used[i])
        {
            if (palKey != NULL)
                palKey[i] = RT_PAL_FREE;
            count++;
        }
        else
        {
            if (palKey != NULL)
                palKey[i] = RT_PAL_UNAVAILABLE;
            if (i >= gReservedSpritePaletteCount && GetSpritePaletteTagByPaletteNum(i) == TAG_NONE)
                count++;
        }
    }
    return count;
}

// Gets a palette slot holding the given colours (shared by key). Returns 0xFF if none is free.
static u8 RtAcquirePal(u16 key, const u16 *colors)
{
    u8 i, slot = 0xFF;

    for (i = 0; i < 16; i++)
    {
        if (sRt->palKey[i] == key)
        {
            sRt->palRef[i]++;
            return i;
        }
    }

    // Prefer a proper dynamic slot, then borrow a reserved one.
    slot = AllocSpritePalette(RT_PAL_TAG_BASE + key);
    if (slot != 0xFF)
    {
        sRt->palFromAlloc[slot] = TRUE;
    }
    else
    {
        for (i = 0; i < 16; i++)
        {
            if (sRt->palKey[i] == RT_PAL_FREE)
            {
                slot = i;
                break;
            }
        }
        if (slot == 0xFF)
            return 0xFF;
        sRt->palFromAlloc[slot] = FALSE;
        if (!sRt->palSaved[slot])
        {
            CpuCopy16(&gPlttBufferUnfaded[OBJ_PLTT_ID(slot)], sRt->palSavedUnfaded[slot], PLTT_SIZE_4BPP);
            CpuCopy16(&gPlttBufferFaded[OBJ_PLTT_ID(slot)], sRt->palSavedFaded[slot], PLTT_SIZE_4BPP);
            sRt->palSaved[slot] = TRUE;
        }
    }
    LoadPalette(colors, OBJ_PLTT_ID(slot), PLTT_SIZE_4BPP);
    sRt->palKey[slot] = key;
    sRt->palRef[slot] = 1;
    return slot;
}

static u8 RtAcquireCompressedPal(u16 key, const u32 *data)
{
    u16 colors[16];

    LZDecompressWram(data, colors);
    return RtAcquirePal(key, colors);
}

static void RtReleasePal(u8 slot)
{
    if (slot >= 16 || sRt->palRef[slot] == 0)
        return;
    if (--sRt->palRef[slot] != 0)
        return;
    if (sRt->palFromAlloc[slot])
    {
        FreeSpritePaletteByTag(RT_PAL_TAG_BASE + sRt->palKey[slot]);
        sRt->palFromAlloc[slot] = FALSE;
        sRt->palKey[slot] = RT_PAL_UNAVAILABLE;
    }
    else
    {
        sRt->palKey[slot] = RT_PAL_FREE;
    }
}

static void RtRestoreBorrowedPals(void)
{
    u8 i;

    for (i = 0; i < 16; i++)
    {
        if (sRt->palRef[i] != 0 && sRt->palFromAlloc[i])
            FreeSpritePaletteByTag(RT_PAL_TAG_BASE + sRt->palKey[i]);
        if (!sRt->palSaved[i])
            continue;
        CpuCopy16(sRt->palSavedUnfaded[i], &gPlttBufferUnfaded[OBJ_PLTT_ID(i)], PLTT_SIZE_4BPP);
        CpuCopy16(sRt->palSavedFaded[i], &gPlttBufferFaded[OBJ_PLTT_ID(i)], PLTT_SIZE_4BPP);
    }
}

// ---------------------------------------------------------------------------
// Effects
// ---------------------------------------------------------------------------

static bool8 RtLoadFx(u8 kind)
{
    const struct RtFxDef *def = &sFxDefs[kind];
    u16 index = def->tag - ANIM_SPRITES_START;

    if (sRt->fxRefCount[kind] == 0)
    {
        sRt->fxPalSlot[kind] = RtAcquireCompressedPal(RT_PAL_KEY_FX(index), gBattleAnimPaletteTable[index].data);
        if (sRt->fxPalSlot[kind] == 0xFF)
            return FALSE;
        if (GetSpriteTileStartByTag(def->tag) == 0xFFFF)
            LoadCompressedSpriteSheetUsingHeap(&gBattleAnimPicTable[index]);
        if (GetSpriteTileStartByTag(def->tag) == 0xFFFF)
        {
            RtReleasePal(sRt->fxPalSlot[kind]);
            return FALSE;
        }
    }
    sRt->fxRefCount[kind]++;
    return TRUE;
}

static void RtUnloadFx(u8 kind)
{
    u8 i;
    bool8 used = FALSE;
    if (sRt->fxRefCount[kind] == 0)
        return;
    if (--sRt->fxRefCount[kind] == 0)
    {
        for (i = 0; i < FX_COUNT; i++)
            if (sRt->fxRefCount[i] && sFxDefs[i].tag == sFxDefs[kind].tag) used = TRUE;
        if (!used) FreeSpriteTilesByTag(sFxDefs[kind].tag);
        RtReleasePal(sRt->fxPalSlot[kind]);
    }
}

static void RtSetFxFrame(struct RtFx *fx)
{
    struct Sprite *sprite = &gSprites[fx->spriteId];
    sprite->oam.tileNum = sprite->sheetTileStart + sFxDefs[fx->kind].tileOffset + fx->frame * sFxDefs[fx->kind].tilesPerFrame;
}

// Returns the fx slot or 0xFF.
static u8 RtSpawnFx(u8 kind, s16 x, s16 y, u8 life, bool8 loop)
{
    struct SpriteTemplate template;
    const struct RtFxDef *def = &sFxDefs[kind];
    u8 i, spriteId;

    for (i = 0; i < RT_MAX_FX; i++)
    {
        if (!sRt->fx[i].active)
            break;
    }
    if (i == RT_MAX_FX)
        return 0xFF;
    if (!RtLoadFx(kind))
        return 0xFF;

    template.tileTag = def->tag;
    template.paletteTag = TAG_NONE;
    switch (def->shape)
    {
    case FX_SHAPE_32x16:
        template.oam = &gOamData_AffineOff_ObjNormal_32x16;
        break;
    case FX_SHAPE_8x8:
        template.oam = &gOamData_AffineOff_ObjNormal_8x8;
        break;
    case FX_SHAPE_16x16:
        template.oam = &gOamData_AffineOff_ObjNormal_16x16;
        break;
    case FX_SHAPE_16x32:
        template.oam = &gOamData_AffineOff_ObjNormal_16x32;
        break;
    default:
        template.oam = &gOamData_AffineOff_ObjNormal_32x32;
        break;
    }
    template.anims = gDummySpriteAnimTable;
    template.images = NULL;
    template.affineAnims = gDummySpriteAffineAnimTable;
    template.callback = SpriteCallbackDummy;

    spriteId = CreateSprite(&template, x, y, 0);
    if (spriteId == MAX_SPRITES)
    {
        RtUnloadFx(kind);
        return 0xFF;
    }
    gSprites[spriteId].oam.priority = 1;
    gSprites[spriteId].oam.paletteNum = sRt->fxPalSlot[kind];

    sRt->fx[i].ptdAnim = 0;
    sRt->fx[i].active = TRUE;
    sRt->fx[i].kind = kind;
    sRt->fx[i].spriteId = spriteId;
    sRt->fx[i].frame = 0;
    sRt->fx[i].frameTimer = 0;
    sRt->fx[i].life = life != 0 ? life : def->frames * def->frameDur;
    sRt->fx[i].loop = loop;
    RtSetFxFrame(&sRt->fx[i]);
    return i;
}

static void RtDestroyFx(u8 id)
{
    struct RtFx *fx;

    if (id >= RT_MAX_FX || !sRt->fx[id].active)
        return;
    fx = &sRt->fx[id];
    if(fx->ptdAnim){RtPtdDestroyFx(id);return;}
    DestroySprite(&gSprites[fx->spriteId]);
    RtUnloadFx(fx->kind);
    fx->active = FALSE;
}

static void RtUpdateFx(void)
{
    u8 i;
    struct RtFx *fx;
    const struct RtFxDef *def;

    for (i = 0; i < RT_MAX_FX; i++)
    {
        fx = &sRt->fx[i];
        if (!fx->active)
            continue;
        if(fx->ptdAnim){RtPtdUpdateFx(i);continue;}
        def = &sFxDefs[fx->kind];
        if (++fx->frameTimer >= def->frameDur)
        {
            fx->frameTimer = 0;
            if (++fx->frame >= def->frames)
                fx->frame = fx->loop ? 0 : def->frames - 1;
        }
        RtSetFxFrame(fx);
        if (fx->life != 0xFF && --fx->life == 0)
            RtDestroyFx(i);
    }
}

#include "realtime_ptd.inc.c"
#include "realtime_ptd_status.inc.c"

static u8 RtGetHitFx(u16 move)
{
    switch (move)
    {
    case MOVE_SCRATCH:
    case MOVE_SLASH:
    case MOVE_FURY_SWIPES:
    case MOVE_CUT:
    case MOVE_CRUSH_CLAW:
    case MOVE_FALSE_SWIPE:
        return FX_SCRATCH;
    }
    return sTypeFx[gBattleMoves[move].type][1];
}

// ---------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------

static void RtDrawPanel(u8 windowId, u8 fillColor)
{
    u16 w = sRtWindowTemplates[windowId].width * 8;
    u16 h = sRtWindowTemplates[windowId].height * 8;
    u8 win = sRt->winIds[windowId];
    if(windowId==RT_WIN_MOVES || windowId==RT_WIN_MSG || windowId==RT_WIN_MENU)
    {
        FillWindowPixelBuffer(win,PIXEL_FILL(C_PANEL_LIGHT));
        DrawTextBorderOuter(win,windowId==RT_WIN_MENU?RT_MENU_FRAME_TILES:RT_FRAME_TILES,RT_FRAME_PAL);
        return;
    }
    FillWindowPixelBuffer(win, PIXEL_FILL(C_BORDER));
    FillWindowPixelRect(win, PIXEL_FILL(fillColor), 1, 1, w - 2, h - 2);
}

#include "realtime_text_fit.inc.c"
static void RtPrint(u8 windowId,u8 font,s16 x,s16 y,const u8 *color,const u8 *str)
{
    u16 w=GetWindowAttribute(sRt->winIds[windowId],WINDOW_WIDTH)*8,h=GetWindowAttribute(sRt->winIds[windowId],WINDOW_HEIGHT)*8;
    RtPrintFit(windowId,font,x,y,w-x,h-y,color,str);
}

static void RtPrintRight(u8 windowId, u8 font, s16 right, s16 y, const u8 *color, const u8 *str)
{
    s16 width = GetStringWidth(font, str, 0);
    RtPrintFit(windowId,font,max(0,right-width),y,min(right,width),13,color,str);
}

// Word-at-a-time rectangle fill for HUD bars and timer areas. The stock
// FillBitmapRect4Bit computes every pixel separately and dominated battle CPU time.
static void RtFastFill(u8 windowId, u8 color, u16 x, u16 y, u16 w, u16 h)
{
    u8 win = sRt->winIds[windowId];
    u8 *pixels = (u8 *)GetWindowAttribute(win, WINDOW_TILE_DATA);
    u16 tiles = GetWindowAttribute(win, WINDOW_WIDTH), wh = GetWindowAttribute(win, WINDOW_HEIGHT) * 8;
    u8 fill = color | (color << 4);
    u32 fill32 = fill | (fill << 8) | (fill << 16) | (fill << 24);
    u16 xx, yy, end;
    if (x >= tiles * 8 || y >= wh) return;
    end = min(x + w, tiles * 8);
    h = min(h, wh - y);
    for (yy = y; yy < y + h; yy++)
    {
        u8 *row = pixels + (yy >> 3) * tiles * 32 + (yy & 7) * 4;
        xx = x;
        while (xx < end)
        {
            u8 *b = row + (xx >> 3) * 32 + ((xx & 7) >> 1);
            if ((xx & 7) == 0 && xx + 8 <= end) { *(u32 *)b = fill32; xx += 8; }
            else if ((xx & 1) == 0 && xx + 1 < end) { *b = fill; xx += 2; }
            else { if (xx & 1) *b = (*b & 0x0F) | (color << 4); else *b = (*b & 0xF0) | color; xx++; }
        }
    }
}

static void RtDrawBar(u8 windowId, u16 x, u16 y, u16 w, u16 h, u32 value, u32 max, u8 color)
{
    u8 win = sRt->winIds[windowId];
    u16 filled = max != 0 ? (value * w) / max : 0;

    if (value != 0 && filled == 0)
        filled = 1;
    (void)win;
    if (filled != 0)
        RtFastFill(windowId, color, x, y, filled, h);
    if (filled < w)
        RtFastFill(windowId, C_BAR_BG, x + filled, y, w - filled, h);
}

static u8 RtHpColor(u16 hp, u16 maxHp)
{
    if (hp * 5 <= maxHp)
        return C_HP_RED;
    if (hp * 2 <= maxHp)
        return C_HP_YELLOW;
    return C_HP_GREEN;
}

static void RtDrawFighterHeader(u8 windowId,struct RtFighter *f)
{
    u8 name[POKEMON_NAME_LENGTH+2],lvl[8],*ptr;
    RtGetNickname(f->mon,name);ptr=StringCopy(lvl,sText_Lv);ConvertIntToDecimalStringN(ptr,f->level,STR_CONV_MODE_LEFT_ALIGN,3);
    RtPrintFit(windowId,FONT_SMALL,2,0,72,9,sTextDark,name);RtPrintFit(windowId,FONT_SMALL,76,0,26,9,sTextDark,lvl);
    if((f->status&STATUS1_ANY)||f->confusionTimer)RtPrintFit(windowId,FONT_SMALL,79,8,24,8,sTextDark,RtStatusLabel(f));
}

static void RtDrawEnemyHud(void)
{
    struct RtFighter *f = &sRt->enemy;
    RtDrawPanel(RT_WIN_ENEMY, C_PANEL_LIGHT);
    RtDrawFighterHeader(RT_WIN_ENEMY, f);
    RtDrawBar(RT_WIN_ENEMY, 2, 11, 74, 3, f->shownHp, f->maxHp, RtHpColor(f->shownHp, f->maxHp));
    CopyWindowToVram(sRt->winIds[RT_WIN_ENEMY], COPYWIN_GFX);
}

static void RtDrawPlayerHud(void)
{
    struct RtFighter *f = &sRt->player;
    u32 exp, low, high;
    RtDrawPanel(RT_WIN_PLAYER, C_PANEL_LIGHT);
    RtDrawFighterHeader(RT_WIN_PLAYER, f);
    RtDrawBar(RT_WIN_PLAYER, 2, 11, 74, 3, f->shownHp, f->maxHp, RtHpColor(f->shownHp, f->maxHp));
    if (f->level < MAX_LEVEL)
    {
        exp = GetMonData(f->mon, MON_DATA_EXP);
        low = gExperienceTables[gSpeciesInfo[GetMonData(f->mon,MON_DATA_SPECIES)].growthRate][f->level];
        high = gExperienceTables[gSpeciesInfo[GetMonData(f->mon,MON_DATA_SPECIES)].growthRate][f->level + 1];
        RtDrawBar(RT_WIN_PLAYER, 2, 15, 100, 1, exp - low, high - low, C_EXP);
    }
    CopyWindowToVram(sRt->winIds[RT_WIN_PLAYER], COPYWIN_GFX);
}

// HP drain only changes the bar, so do not re-render the name and level text.
static void RtDrawHpBarOnly(u8 windowId, struct RtFighter *f)
{
    RtDrawBar(windowId, 2, 11, 74, 3, f->shownHp, f->maxHp, RtHpColor(f->shownHp, f->maxHp));
    CopyWindowToVram(sRt->winIds[windowId], COPYWIN_GFX);
}

// 0 = empty, 1 = ready, 2 = cooling down, 3 = out of PP
static u8 RtGetMoveSlotState(u8 slot)
{
    struct RtFighter *f = &sRt->player;
    u16 move = RtMove(f, slot);

    if (move == MOVE_NONE)
        return 0;
    if (RtPp(f, slot) == 0 || !RtMoveAllowed(f,move))
        return 3;
    if (f->moveCooldown[slot] != 0 || f->cooldown != 0 || f->lungeTimer != 0 || RtBusy(f))
        return 2;
    return 1;
}

static void RtTinyPrint(u8 window,u16 x,u16 y,const u8 *text,u8 color)
{
    u8 colors[]={C_PANEL_LIGHT,color,C_SHADOW};
    RtPrint(window,FONT_SMALL,x,y,colors,text);
}

static void RtDrawMoveCooldownPart(u8 slot, u8 state, bool8 timerOnly)
{
    struct RtFighter *f = &sRt->player;
    u16 remaining = f->moveCooldown[slot], max = f->moveCooldownMax[slot], tenths;
    u8 ppText[12], *end;
    u8 text[16], digit[4];
    u8 *ptr;
    u16 x = (slot&1)*112+2,y=(slot/2)*20;
    bool8 recovering = f->cooldown > remaining;
    if (recovering) { remaining = f->cooldown; max = f->recoveryMax; }
    if (timerOnly)
        RtFastFill(RT_WIN_MOVES, C_PANEL_LIGHT, x+68, y+9, 40, 10);
    else
    {
        RtFastFill(RT_WIN_MOVES, C_PANEL_LIGHT, x, y+9, 108, 10);
        end = ConvertIntToDecimalStringN(ppText, RtPp(f, slot), STR_CONV_MODE_LEFT_ALIGN, 2);
        end = StringCopy(end, sText_Slash);
        ConvertIntToDecimalStringN(end, RtMaxPp(f, slot), STR_CONV_MODE_LEFT_ALIGN, 2);
        RtPrintFit(RT_WIN_MOVES,FONT_SMALL,x,y+10,54,8,state==3?sTextRed:sTextDark,ppText);
    }
    if (state == 2 && remaining)
    {
        tenths = (remaining + 5) / 6;
        ptr = ConvertIntToDecimalStringN(text, tenths / 10, STR_CONV_MODE_LEFT_ALIGN, 2);
        ptr = StringCopy(ptr, sText_Decimal);
        ConvertIntToDecimalStringN(digit, tenths % 10, STR_CONV_MODE_LEFT_ALIGN, 1);
        StringAppend(text, digit);
        RtPrintFit(RT_WIN_MOVES,FONT_SMALL,x+68,y+10,38,8,sTextGray,text);
    }
    else RtPrintFit(RT_WIN_MOVES,FONT_SMALL,x+68,y+10,38,8,state==3?sTextRed:sTextAccent,state==3?sText_NoPp:sText_MoveReady);
    RtDrawBar(RT_WIN_MOVES, x, y+19, 108, 1, state == 3 ? 0 : max > remaining ? max - remaining : 0, max ? max : 1, C_ACCENT);
    if (state == 1)
        RtFastFill(RT_WIN_MOVES, C_HP_GREEN, x, y+19, 108, 1);
}

static void RtDrawMoveCooldown(u8 slot, u8 state)
{
    RtDrawMoveCooldownPart(slot, state, FALSE);
}

static void RtDrawMovesHud(void)
{
    struct RtFighter *f = &sRt->player;
    u8 i, state;
    u16 move, x,y;
    u8 name[MOVE_NAME_LENGTH + 1];
    RtDrawPanel(RT_WIN_MOVES, C_PANEL_DARK);
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        x = (i&1)*112+2;y=(i/2)*20;
        state = RtGetMoveSlotState(i);
        sRt->moveSlotState[i] = state;
        sRt->moveSlotPp[i] = RtPp(f, i);
        RtPrintFit(RT_WIN_MOVES,FONT_SMALL,x,y,8,8,sTextAccent,sButtonLabels[i]);
        move = RtMove(f, i);
        StringCopy(name, move == MOVE_NONE ? sText_Dash : gMoveNames[move]);
        RtPrintFit(RT_WIN_MOVES,FONT_SMALL,x+10,y,96,8,state==1?sTextDark:sTextGray,name);
        if (state)
            RtDrawMoveCooldown(i, state);
    }
    CopyWindowToVram(sRt->winIds[RT_WIN_MOVES], COPYWIN_GFX);
}

static void RtHideMessage(void);

static void RtShowMessageNow(const u8 *str)
{
    u8 page[RT_MSG_LEN*2];u8 win=sRt->winIds[RT_WIN_MSG];
    RtHideMessage();sRt->msgCompact=FALSE;SetWindowAttribute(win,WINDOW_TILEMAP_TOP,3);SetWindowAttribute(win,WINDOW_HEIGHT,3);
    RtDrawPanel(RT_WIN_MSG,C_PANEL_DARK);str=RtWrapMessage(str,page);
    RtPrintFit(RT_WIN_MSG,FONT_SMALL,4,1,200,22,sTextWhite,str);PutWindowTilemap(win);CopyWindowToVram(win,COPYWIN_FULL);sRt->msgVisible=TRUE;
}

static void RtHideMessage(void)
{
    u8 win = sRt->winIds[RT_WIN_MSG];

    if (!sRt->msgVisible)
        return;
    if (sRt->msgCompact)
    { ClearWindowTilemap(win); CopyWindowToVram(win, COPYWIN_MAP); }
    else ClearStdWindowAndFrameToTransparent(win,TRUE);
    sRt->msgVisible = FALSE;
    sRt->transientTimer = 0;
}

// Short message during the fight; does not pause anything.
static void RtShowTransient(const u8 *str, u16 frames)
{
    if (sRt->state == RT_STATE_FIGHT)
    {
        u8 text[RT_MSG_LEN*2];
        u8 win = sRt->winIds[RT_WIN_MSG];
        RtHideMessage();
        sRt->msgCompact = TRUE;
        // This unframed strip ends at y=32, above the playable field.
        SetWindowAttribute(win, WINDOW_TILEMAP_TOP, 2);
        SetWindowAttribute(win, WINDOW_HEIGHT, 2);
        FillWindowPixelBuffer(win, PIXEL_FILL(C_PANEL_LIGHT));
        str=RtWrapMessage(str,text);
        RtPrintFit(RT_WIN_MSG,FONT_SMALL,3,0,202,15,sTextDark,str);
        PutWindowTilemap(win);
        CopyWindowToVram(win, COPYWIN_FULL);
        sRt->msgVisible = TRUE;
        frames = max(frames, 90);
    }
    else RtShowMessageNow(str);
    sRt->transientTimer = frames;
}

static void RtShowTransientExpanded(const u8 *fmt, u16 frames)
{
    StringExpandPlaceholders(gStringVar4, fmt);
    RtShowTransient(gStringVar4, frames);
}

// Messages that are read one after another (battle paused).
static void RtQueueMessage(const u8 *fmt)
{
    const u8 *read;u8 slot;StringExpandPlaceholders(gStringVar4,fmt);read=gStringVar4;
    do {
        if(sRt->msgCount>=RT_MSG_MAX)return;slot=(sRt->msgHead+sRt->msgCount)%RT_MSG_MAX;
        read=RtTextPage(read,sRt->msgQueue[slot]);sRt->msgCount++;
    }while(*read!=EOS);
}

static void RtStartMessages(u8 nextState)
{
    sRt->afterMsgState = nextState;
    sRt->msgTimer = 0;
    sRt->state = RT_STATE_MESSAGES;
}

static void RtDrawBallHint(void)
{
    RtDrawPanel(RT_WIN_HINT, C_PANEL_DARK);
    RtTinyPrint(RT_WIN_HINT, 7, 1, sText_SelectHint, C_ACCENT);
    PutWindowTilemap(sRt->winIds[RT_WIN_HINT]);
    CopyWindowToVram(sRt->winIds[RT_WIN_HINT], COPYWIN_FULL);
}

static void RtLoadNativeFrame(bool8 modal)
{
    u8 frame=gSaveBlock2Ptr->optionsWindowFrameType;
    const struct TextWindowGraphics *gfx=GetUserWindowGraphics(frame<10?frame:0);
    LoadBgTiles(0,gfx->tiles,0x120,modal?RT_MENU_FRAME_TILES:RT_FRAME_TILES);
    LoadPalette(gfx->palette,BG_PLTT_ID(RT_FRAME_PAL),PLTT_SIZE_4BPP);
}

static void RtCreateHud(void)
{
    u8 i;

    CpuCopy16(&gPlttBufferUnfaded[BG_PLTT_ID(RT_HUD_PAL)], sRt->savedPalUnfaded, PLTT_SIZE_4BPP);
    CpuCopy16(&gPlttBufferFaded[BG_PLTT_ID(RT_HUD_PAL)], sRt->savedPalFaded, PLTT_SIZE_4BPP);
    CpuCopy16(&gPlttBufferUnfaded[BG_PLTT_ID(RT_FRAME_PAL)],sRt->savedFramePal,PLTT_SIZE_4BPP);
    CpuCopy16(&gPlttBufferFaded[BG_PLTT_ID(RT_FRAME_PAL)],sRt->savedFramePalFaded,PLTT_SIZE_4BPP);
    LoadPalette(sHudPalette, BG_PLTT_ID(RT_HUD_PAL), PLTT_SIZE_4BPP);
    RtLoadNativeFrame(FALSE);

    for (i = 0; i < RT_WIN_MENU; i++)
        sRt->winIds[i] = AddWindow(&sRtWindowTemplates[i]);

    RtDrawBallHint();
    RtDrawEnemyHud();
    PutWindowTilemap(sRt->winIds[RT_WIN_ENEMY]);
    CopyWindowToVram(sRt->winIds[RT_WIN_ENEMY], COPYWIN_FULL);
}

static void RtShowPlayerHud(void)
{
    RtDrawPlayerHud();
    PutWindowTilemap(sRt->winIds[RT_WIN_PLAYER]);
    CopyWindowToVram(sRt->winIds[RT_WIN_PLAYER], COPYWIN_FULL);
    RtDrawMovesHud();
    PutWindowTilemap(sRt->winIds[RT_WIN_MOVES]);
    CopyWindowToVram(sRt->winIds[RT_WIN_MOVES], COPYWIN_FULL);
}

static void RtDestroyHud(void)
{
    u8 i;

    for (i = 0; i < RT_WIN_COUNT; i++)
    {
        if (sRt->winIds[i] == WINDOW_NONE)
            continue;
        if(i==RT_WIN_MOVES || i==RT_WIN_MSG || i==RT_WIN_MENU) ClearStdWindowAndFrameToTransparent(sRt->winIds[i],FALSE);
        else ClearWindowTilemap(sRt->winIds[i]);
        CopyWindowToVram(sRt->winIds[i], COPYWIN_MAP);
        RemoveWindow(sRt->winIds[i]);
        sRt->winIds[i] = WINDOW_NONE;
    }
    CpuCopy16(sRt->savedPalUnfaded, &gPlttBufferUnfaded[BG_PLTT_ID(RT_HUD_PAL)], PLTT_SIZE_4BPP);
    CpuCopy16(sRt->savedPalFaded, &gPlttBufferFaded[BG_PLTT_ID(RT_HUD_PAL)], PLTT_SIZE_4BPP);
    CpuCopy16(sRt->savedFramePal,&gPlttBufferUnfaded[BG_PLTT_ID(RT_FRAME_PAL)],PLTT_SIZE_4BPP);
    CpuCopy16(sRt->savedFramePalFaded,&gPlttBufferFaded[BG_PLTT_ID(RT_FRAME_PAL)],PLTT_SIZE_4BPP);
}

static void RtUpdateHud(void)
{
    u8 i;
    struct RtFighter *fighters[2] = {&sRt->player, &sRt->enemy};
    u16 step;
    bool8 refreshCooldowns = FALSE;
    bool8 copiedCooldowns = FALSE;

    // Refresh numeric cooldowns at 10 Hz, rather than rerendering text each frame.
    sRt->hudTick += sRt->frameStep;
    if (sRt->hudTick >= 6)
    {
        sRt->hudTick = 0;
        refreshCooldowns = TRUE;
    }

    // Animate HP bars towards the real value.
    for (i = 0; i < 2; i++)
    {
        struct RtFighter *f = fighters[i];
        if (f->mon == NULL || f->shownHp == f->hp)
            continue;
        step = f->maxHp / 40;
        if (step == 0)
            step = 1;
        if (f->shownHp > f->hp)
            f->shownHp = (f->shownHp - f->hp > step) ? f->shownHp - step : f->hp;
        else
            f->shownHp = (f->hp - f->shownHp > step) ? f->shownHp + step : f->hp;
        if (i == 0)
            sRt->dirtyPlayerBar = TRUE;
        else
            sRt->dirtyEnemyBar = TRUE;
    }

    // Slot state reads the encrypted party data, so check it at the HUD rate.
    if (sRt->player.mon != NULL && refreshCooldowns)
    {
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            if (RtGetMoveSlotState(i) != sRt->moveSlotState[i]
             || (sRt->moveSlotState[i] != 0 && RtPp(&sRt->player,i) != sRt->moveSlotPp[i]))
                sRt->dirtyMoves = TRUE;
        }
    }

    if (sRt->dirtyEnemy)
    {
        RtDrawEnemyHud();
        sRt->dirtyEnemy = FALSE;
        sRt->dirtyEnemyBar = FALSE;
    }
    else if (sRt->dirtyEnemyBar)
    {
        RtDrawHpBarOnly(RT_WIN_ENEMY, &sRt->enemy);
        sRt->dirtyEnemyBar = FALSE;
    }
    if (sRt->dirtyPlayer && sRt->player.mon != NULL)
    {
        RtDrawPlayerHud();
        sRt->dirtyPlayer = FALSE;
        sRt->dirtyPlayerBar = FALSE;
    }
    else if (sRt->dirtyPlayerBar && sRt->player.mon != NULL)
    {
        RtDrawHpBarOnly(RT_WIN_PLAYER, &sRt->player);
        sRt->dirtyPlayerBar = FALSE;
    }
    if (sRt->dirtyMoves && sRt->player.mon != NULL)
    {
        RtDrawMovesHud();
        sRt->dirtyMoves = FALSE;
    }
    else if (sRt->player.mon != NULL)
    {
        // Move names, button labels and PP have not changed. Redraw one timer
        // per frame, and only when its shown tenths change, so no frame does
        // all four text redraws at once.
        i = sRt->hudSlot++ & (MAX_MON_MOVES - 1);
        if (sRt->moveSlotState[i] == 2)
        {
            struct RtFighter *pf = &sRt->player;
            u16 remaining = pf->cooldown > pf->moveCooldown[i] ? pf->cooldown : pf->moveCooldown[i];
            u16 tenths = (remaining + 5) / 6;
            if (tenths != sRt->shownTenths[i])
            {
                sRt->shownTenths[i] = tenths;
                RtDrawMoveCooldownPart(i, 2, TRUE);
                copiedCooldowns = TRUE;
            }
        }
        if (copiedCooldowns)
            CopyWindowToVram(sRt->winIds[RT_WIN_MOVES], COPYWIN_GFX);
    }

    if (sRt->transientTimer != 0)
    {
        sRt->transientTimer = sRt->transientTimer > sRt->frameStep ? sRt->transientTimer - sRt->frameStep : 0;
        if (!sRt->transientTimer) RtHideMessage();
    }
}

// ---------------------------------------------------------------------------
// Fighters
// ---------------------------------------------------------------------------

static void RtSyncSprite(struct RtFighter *f)
{
    struct Sprite *sprite;
    s32 dx, dy;
    u8 t;

    if (!f->active || f->spriteId == MAX_SPRITES)
        return;
    sprite = &gSprites[f->spriteId];
    sprite->x = FROM_Q8(f->x);
    sprite->y = FROM_Q8(f->y)+8-DsMonAnchorY(f->spriteKey);
    sprite->x2 = 0;
    sprite->y2 = 0;

    // Lunge forward then back.
    if (f->lungeTimer != 0)
    {
        RtFaceVector(f, &dx, &dy);
        t = RT_LUNGE_FRAMES - f->lungeTimer;
        if (t > RT_LUNGE_FRAMES / 2)
            t = RT_LUNGE_FRAMES - t;
        sprite->x2 = dx * t * 2;
        sprite->y2 = dy * t * 2;
    }
    // Telegraphing an attack: little shake.
    if (f->windup != 0)
        sprite->x2 += (f->windup & 2) ? 1 : -1;

    // HGSS provides authored poses for all four directions.
    if (f->dir == RT_DIR_LEFT || f->dir == RT_DIR_RIGHT)
        f->lastHorizDir = f->dir;
    // PMD animation: the move's own body animation (damage lands on its hit frame),
    // otherwise hurt, sleep, walk or idle, facing one of eight directions.
    {
        struct RtCast *cast=&f->cast;
        s32 mx=f->x-f->renderedX, my=f->y-f->renderedY;
        s16 adx=0, ady=0;
        u8 anim, dir8;
        u16 tick;
        bool8 loop=TRUE;
        if(mx || my) f->walkClock+=sRt->frameStep; else f->walkClock=0;
        if(cast->active && (cast->dx || cast->dy)) dir8=PmdDirFromVector(cast->dx,cast->dy,f->dir);
        else if(mx || my) dir8=PmdDirFromVector(mx,my,f->dir);
        else if(f->isEnemy && RtOther(f)->active) dir8=PmdDirFromVector(RtOther(f)->x-f->x,RtOther(f)->y-f->y,f->dir);
        else dir8=PmdDirFromVector(f->faceX,f->faceY,f->dir);
        if(cast->active && cast->move)
        {
            s32 t;
            if(f->pmdCastMove!=cast->move) { f->pmdCastMove=cast->move; f->pmdCastAnim=PmdMoveBodyAnim(cast->move,f->species); }
            anim=f->pmdCastAnim;
            t=(s32)cast->elapsed+PmdAnimHitTime(f->spriteKey,anim)-RtProfile(cast->move)->startup;
            tick=t<0?0:t; loop=FALSE;
        }
        else if(f->hurtTimer) { anim=PMD_ANIM_HURT; tick=f->hurtTimer<20?20-f->hurtTimer:0; loop=FALSE; f->pmdCastMove=0; }
        else if(f->status & STATUS1_SLEEP) { anim=PMD_ANIM_SLEEP; tick=f->pmdClock; f->pmdCastMove=0; }
        else if(f->walkClock) { anim=PMD_ANIM_WALK; tick=f->walkClock; f->pmdCastMove=0; }
        else { anim=PMD_ANIM_IDLE; tick=f->pmdClock; f->pmdCastMove=0; }
        f->pmdClock+=sRt->frameStep;
        f->renderedX=f->x; f->renderedY=f->y;
        PmdShowMonFrame(f->spriteId,f->spriteKey,anim,dir8,tick,loop,&adx,&ady);
        if(cast->active && cast->move) { sprite->x2=adx; sprite->y2=ady; }
        else { sprite->x2+=adx; sprite->y2+=ady; }
    }

    // Blink while hurt.
    sprite->invisible = (f->hurtTimer != 0 && (f->hurtTimer & 2));

    // Draw lower Pokemon in front.
    sprite->subpriority = 0x80 - (FROM_Q8(f->y) >> 1);
    if(f->arenaShadowLive)
    {
        gSprites[f->arenaShadow].x=FROM_Q8(f->x);
        gSprites[f->arenaShadow].y=FROM_Q8(f->y)+4;
        gSprites[f->arenaShadow].invisible=!f->hp || f->hidden;
    }
    RtRenderCast(f, sprite);
    RtUpdateStatusVisual(f);
}

static bool8 RtInitFighter(struct RtFighter *f, struct Pokemon *mon, bool8 isEnemy, s32 x, s32 y, u8 partyIndex)
{
    u8 i;

    memset(f, 0, sizeof(*f));
    f->statusSpriteId = MAX_SPRITES;
    f->confusionSpriteId = MAX_SPRITES;
    f->mon = mon;
    f->glowSprite = MAX_SPRITES;
    for (i = 0; i < 8; i++) f->cast.fx[i] = 0xFF;
    f->residualClock = 180;
    f->ability = f->originalAbility = GetMonAbility(mon);
    f->item = GetMonData(mon, MON_DATA_HELD_ITEM);
    f->types[0] = gSpeciesInfo[GetMonData(mon, MON_DATA_SPECIES)].types[0];
    f->types[1] = gSpeciesInfo[GetMonData(mon, MON_DATA_SPECIES)].types[1];
    f->isEnemy = isEnemy;
    f->species = GetMonData(mon, MON_DATA_SPECIES);
    f->hp = GetMonData(mon, MON_DATA_HP);
    f->maxHp = GetMonData(mon, MON_DATA_MAX_HP);
    f->shownHp = f->hp;
    f->level = GetMonData(mon, MON_DATA_LEVEL);
    f->status = GetMonData(mon, MON_DATA_STATUS);
    f->statusTick = 180;
    f->statusTimer = RtSleepFrames(f->status,f->ability);
    f->toxicCount = 1;
    f->x = Q8(x);
    f->y = Q8(y);
    f->dir = isEnemy ? RT_DIR_LEFT : RT_DIR_RIGHT;
    f->lastHorizDir = f->dir;
    f->partyIndex = partyIndex;
    for (i = 0; i < RT_STAT_COUNT; i++)
        f->stages[i] = 0;

    f->spriteKey=DsMonSpriteKey(f->species,GetMonData(mon,MON_DATA_PERSONALITY),GetMonGender(mon)==MON_FEMALE);
    f->visualShiny=IsMonShiny(mon);
    f->palSlot = RtAcquirePal(RT_PAL_KEY_DS(f->spriteKey,f->visualShiny),DsMonPalette(f->spriteKey,f->visualShiny));
    f->spriteId = DsCreateMonSprite(f->spriteKey,x,y,f->palSlot,0x40,SpriteCallbackDummy);
    if(f->spriteId<MAX_SPRITES)sRt->worldSprites[f->spriteId]=TRUE;
    if (f->spriteId == MAX_SPRITES)
    {
        RtReleasePal(f->palSlot);
        return FALSE;
    }
    gSprites[f->spriteId].oam.priority = 2;
    if (f->palSlot != 0xFF)
        gSprites[f->spriteId].oam.paletteNum = f->palSlot;
    f->active = TRUE;
    RtCreateArenaShadow(f);
    RtSyncSprite(f);
    return TRUE;
}

static void RtRemoveFighterSprite(struct RtFighter *f)
{
    RtRemoveArenaShadow(f);
    RtCancelCast(f);
    if (f->statusSpriteId != MAX_SPRITES) DestroySprite(&gSprites[f->statusSpriteId]);
    if (f->confusionSpriteId != MAX_SPRITES) DestroySprite(&gSprites[f->confusionSpriteId]);
    f->statusSpriteId = MAX_SPRITES;
    f->confusionSpriteId = MAX_SPRITES;
    if (f->active && f->spriteId != MAX_SPRITES)
    {
        DestroySprite(&gSprites[f->spriteId]);
        RtReleasePal(f->palSlot);
    }
    f->spriteId = MAX_SPRITES;
    f->active = FALSE;
}

static void RtFace(struct RtFighter *f, struct RtFighter *target)
{
    f->dir = RtDirFromVector(target->x - f->x, target->y - f->y);
    if (target->x != f->x)
        f->lastHorizDir = target->x < f->x ? RT_DIR_LEFT : RT_DIR_RIGHT;
}

static void RtSetHp(struct RtFighter *f, s32 hp)
{
    if (hp < 0)
        hp = 0;
    if (hp > f->maxHp)
        hp = f->maxHp;
    f->hp = hp;
    if(!hp)
    {
        struct RtFighter *other=f->isEnemy?&sRt->player:&sRt->enemy;
        if(other->clocks[RTC_DESTINY] && other->perishOwner==(f->isEnemy?1:0))
        { other->clocks[RTC_DESTINY]=0; if(other->hp) RtSetHp(other,0); }
        if(other->clocks[RTC_PERISH] && other->perishOwner==(f->isEnemy?1:0)) other->clocks[RTC_PERISH]=0;
    }
    SetMonData(f->mon, MON_DATA_HP, &f->hp);
    if (f->isEnemy)
        sRt->dirtyEnemy = TRUE;
    else
        sRt->dirtyPlayer = TRUE;
}

// ---------------------------------------------------------------------------
// Combat
// ---------------------------------------------------------------------------

static u16 RtGetTypeMultiplier(u8 moveType, u16 defSpecies)
{
    u16 mult = 100;
    u8 defType1 = gSpeciesInfo[defSpecies].types[0];
    u8 defType2 = gSpeciesInfo[defSpecies].types[1];
    u32 i = 0;

    while (TYPE_EFFECT_ATK_TYPE(i) != TYPE_ENDTABLE)
    {
        if (TYPE_EFFECT_ATK_TYPE(i) == TYPE_FORESIGHT)
        {
            i += 3;
            continue;
        }
        if (TYPE_EFFECT_ATK_TYPE(i) == moveType)
        {
            if (TYPE_EFFECT_DEF_TYPE(i) == defType1)
                mult = mult * TYPE_EFFECT_MULTIPLIER(i) / TYPE_MUL_NORMAL;
            if (TYPE_EFFECT_DEF_TYPE(i) == defType2 && defType1 != defType2)
                mult = mult * TYPE_EFFECT_MULTIPLIER(i) / TYPE_MUL_NORMAL;
        }
        i += 3;
    }
    return mult;
}

static u16 RtAccuracyChance(struct RtFighter *atk, struct RtFighter *def, u16 move)
{
    u8 accuracy = gBattleMoves[move].accuracy;
    s8 stage;
    u32 chance;

    if (move==MOVE_THUNDER && RtWeatherWorks() && sRt->weather==RTW_RAIN) return 100;
    if (move==MOVE_THUNDER && RtWeatherWorks() && sRt->weather==RTW_SUN) accuracy=50;
    if (gBattleMoves[move].effect==EFFECT_OHKO) return atk->level>=def->level?30+atk->level-def->level:0;
    if (atk->clocks[RTC_LOCKON]) return 100;
    if (move==MOVE_SWIFT || move==MOVE_AERIAL_ACE) return 100;
    if (accuracy == 0 || (RtProfile(move)->flags&RTMF_ALWAYS) || gBattleMoves[move].effect == EFFECT_ALWAYS_HIT)
        return 100;
    stage = atk->stages[RT_STAT_ACC] - def->stages[RT_STAT_EVASION];
    if (stage < -6)
        stage = -6;
    if (stage > 6)
        stage = 6;
    chance = accuracy * sAccuracyStageRatios[stage + 6][0] / sAccuracyStageRatios[stage + 6][1];
    if(atk->ability==ABILITY_COMPOUND_EYES) chance=chance*13/10;
    if(atk->ability==ABILITY_HUSTLE && !RtSpecial(move,RtMoveType(atk,move)) && RtDamaging(move)) chance=chance*4/5;
    if(def->ability==ABILITY_SAND_VEIL && sRt->weather==RTW_SAND && RtWeatherWorks()) chance=chance*4/5;
    if(ItemId_GetHoldEffect(def->item)==HOLD_EFFECT_EVASION_UP) chance=chance*(100-ItemId_GetHoldEffectParam(def->item))/100;
    return chance>100?100:chance;
}

static bool8 RtRollAccuracy(struct RtFighter *atk, struct RtFighter *def, u16 move)
{
    u16 chance=RtAccuracyChance(atk,def,move);
    if(atk->clocks[RTC_LOCKON]) atk->clocks[RTC_LOCKON]=0;
    return chance>=100 || Random()%100<chance;
}

#include "realtime_status.inc.c"

static void RtShowStatFx(struct RtFighter *target)
{
    RtSpawnFx(FX_SPARKLE, FROM_Q8(target->x), FROM_Q8(target->y), 0, FALSE);
}

// Applies a status move. Returns TRUE if anything happened.
static bool8 RtApplyStatusMoveEffect(struct RtFighter *atk, struct RtFighter *def, u16 move)
{
    u8 effect = gBattleMoves[move].effect;
    u8 i;
    bool8 any = FALSE;
    bool8 rose = FALSE;
    struct RtFighter *target = NULL;
    s32 heal;

    if (RtExtendedStatus(atk, def, move)) return TRUE;
    if (RtStatusMove(atk, def, move)) return TRUE;

    for (i = 0; i < ARRAY_COUNT(sStatEffects); i++)
    {
        if (sStatEffects[i].effect != effect)
            continue;
        target = sStatEffects[i].self ? atk : def;
        RtStage(target, sStatEffects[i].stat, sStatEffects[i].amount);
        if (target->stages[sStatEffects[i].stat] > 6)
            target->stages[sStatEffects[i].stat] = 6;
        if (target->stages[sStatEffects[i].stat] < -6)
            target->stages[sStatEffects[i].stat] = -6;
        rose = sStatEffects[i].amount > 0;
        any = TRUE;
    }
    if (any)
    {
        // RtStage emits the matching PTD stat indicator only on an actual change.
        RtGetNickname(target->mon, gStringVar1);
        if (target->isEnemy)
        {
            // "Wild X" is too long next to the message, the name is enough.
        }
        RtShowTransientExpanded(rose ? sText_StatRose : sText_StatFell, 50);
        PlaySE(rose ? SE_M_STAT_INCREASE : SE_M_STAT_DECREASE);
        return TRUE;
    }

    switch (effect)
    {
    case EFFECT_RESTORE_HP:
    case EFFECT_SOFTBOILED:
    case EFFECT_MORNING_SUN:
    case EFFECT_SYNTHESIS:
    case EFFECT_MOONLIGHT:
    case EFFECT_REST:
        heal = (effect == EFFECT_REST) ? atk->maxHp : atk->maxHp / 2;
        if(effect==EFFECT_MORNING_SUN || effect==EFFECT_SYNTHESIS || effect==EFFECT_MOONLIGHT)
        { if(RtWeatherWorks() && sRt->weather) heal=sRt->weather==RTW_SUN?atk->maxHp*2/3:atk->maxHp/4; }
        if (atk->hp == atk->maxHp && !(effect == EFFECT_REST && atk->status))
            break;
        if (effect == EFFECT_REST) RtSetStatus(atk, STATUS1_SLEEP_TURN(2));
        RtSetHp(atk, atk->hp + heal);
        if(!sRtPtdStatusArt[move].anim)RtShowStatFx(atk);
        RtGetNickname(atk->mon, gStringVar1);
        RtShowTransientExpanded(sText_Healed, 50);
        PlaySE(SE_USE_ITEM);
        return TRUE;
    case EFFECT_HEAL_BELL:
        {
            struct Pokemon *party = atk->isEnemy ? gEnemyParty : gPlayerParty;
            u32 clear = 0;
            for (i = 0; i < PARTY_SIZE; i++) SetMonData(&party[i], MON_DATA_STATUS, &clear);
            RtSetStatus(atk, STATUS1_NONE);
            return TRUE;
        }
    case EFFECT_REFRESH:
        if (atk->status & (STATUS1_PSN_ANY | STATUS1_BURN | STATUS1_PARALYSIS)) { RtSetStatus(atk, STATUS1_NONE); return TRUE; }
        break;
    case EFFECT_HAZE:
        for (i = 0; i < RT_STAT_COUNT; i++)
        {
            atk->stages[i] = 0;
            def->stages[i] = 0;
        }
        if(!sRtPtdStatusArt[move].anim)RtShowStatFx(atk);
        if(!sRtPtdStatusArt[move].anim)RtShowStatFx(def);
        return TRUE;
    }
    // Effects outside this version still fail explicitly.
    if(!sRtPtdStatusArt[move].anim)RtSpawnFx(FX_SPARKLE, FROM_Q8(def->x), FROM_Q8(def->y), 0, FALSE);
    RtShowTransient(sText_NothingHappened, 40);
    return FALSE;
}

static bool8 RtApplyStatusMove(struct RtFighter *atk,struct RtFighter *def,u16 move)
{
    bool8 applied;
    if((gBattleMoves[move].flags&FLAG_SNATCH_AFFECTED) && def->clocks[RTC_SNATCH]
        && RtLength(FROM_Q8(def->x-atk->x),FROM_Q8(def->y-atk->y))<=72)
    {def->clocks[RTC_SNATCH]=0;return RtApplyStatusMove(def,atk,move);}
    if(def->clocks[RTC_MAGICCOAT] && atk!=def && (gBattleMoves[move].flags&FLAG_MAGIC_COAT_AFFECTED))
    {def->clocks[RTC_MAGICCOAT]=0;return RtApplyStatusMove(def,atk,move);}
    applied=RtApplyStatusMoveEffect(atk,def,move);
    RtPtdStatusFx(atk,def,move,applied);
    return applied;
}

#include "realtime_special.inc.c"

// Damage for one hit; *outMult = type multiplier x100, *outCrit = crit flag.
static s32 RtComputeDamage(struct RtFighter *atk, struct RtFighter *def, u16 move, u16 *outMult, bool8 *outCrit, bool8 roll)
{
    const struct BattleMove *m = &gBattleMoves[move];
    u32 power = m->power;
    u8 type = RtMoveType(atk,move);
    s32 attack, defense, damage;
    u16 mult;
    u8 critDenom = RtCriticalDenominator(atk, def, move);
    u8 heldAtk = ItemId_GetHoldEffect(atk->item), heldDef = ItemId_GetHoldEffect(def->item), i;
    static const u8 typeItems[][2] = {
        {HOLD_EFFECT_BUG_POWER,TYPE_BUG}, {HOLD_EFFECT_STEEL_POWER,TYPE_STEEL},
        {HOLD_EFFECT_GROUND_POWER,TYPE_GROUND}, {HOLD_EFFECT_ROCK_POWER,TYPE_ROCK},
        {HOLD_EFFECT_GRASS_POWER,TYPE_GRASS}, {HOLD_EFFECT_DARK_POWER,TYPE_DARK},
        {HOLD_EFFECT_FIGHTING_POWER,TYPE_FIGHTING}, {HOLD_EFFECT_ELECTRIC_POWER,TYPE_ELECTRIC},
        {HOLD_EFFECT_WATER_POWER,TYPE_WATER}, {HOLD_EFFECT_FLYING_POWER,TYPE_FLYING},
        {HOLD_EFFECT_POISON_POWER,TYPE_POISON}, {HOLD_EFFECT_ICE_POWER,TYPE_ICE},
        {HOLD_EFFECT_GHOST_POWER,TYPE_GHOST}, {HOLD_EFFECT_PSYCHIC_POWER,TYPE_PSYCHIC},
        {HOLD_EFFECT_FIRE_POWER,TYPE_FIRE}, {HOLD_EFFECT_DRAGON_POWER,TYPE_DRAGON},
        {HOLD_EFFECT_NORMAL_POWER,TYPE_NORMAL}};

    *outCrit = FALSE;
    mult = RtMultiplier(type, def);
    *outMult = mult;

    if ((type==TYPE_GROUND && def->ability==ABILITY_LEVITATE)
     || (m->effect==EFFECT_OHKO && def->ability==ABILITY_STURDY)
     || (move!=MOVE_STRUGGLE && def->ability==ABILITY_WONDER_GUARD && mult<=100))
    { *outMult=0; return 0; }
    if (type==TYPE_FIRE && def->ability==ABILITY_FLASH_FIRE)
    { if(roll) def->flashFire=TRUE; *outMult=0; return 0; }
    if ((type==TYPE_WATER && def->ability==ABILITY_WATER_ABSORB) || (type==TYPE_ELECTRIC && def->ability==ABILITY_VOLT_ABSORB))
    { if(roll) RtSetHp(def,def->hp+def->maxHp/4); *outMult=0; return 0; }
    if (move==MOVE_BEAT_UP)
    {
        struct Pokemon *party=atk->isEnemy?gEnemyParty:gPlayerParty;
        u8 index=atk->cast.hitCount?atk->cast.hitCount-1:atk->partyIndex;
        u16 species=GetMonData(&party[index],MON_DATA_SPECIES);
        if(!RtIsUsableMon(&party[index]) || GetMonData(&party[index],MON_DATA_STATUS)) return 0;
        damage=gSpeciesInfo[species].baseAttack * 10 * (GetMonData(&party[index],MON_DATA_LEVEL)*2/5+2);
        damage=damage/gSpeciesInfo[def->species].baseDefense/50+2;
        if(roll && critDenom && !(Random()%critDenom)) { *outCrit=TRUE; damage*=2; }
        *outMult=100;
        return damage;
    }

    switch (m->effect)
    {
    case EFFECT_DRAGON_RAGE:
        return mult ? 40 : 0;
    case EFFECT_SONICBOOM:
        return mult ? 20 : 0;
    case EFFECT_LEVEL_DAMAGE:
        return mult ? atk->level : 0;
    case EFFECT_PSYWAVE:
        return mult ? (atk->level * (roll ? 50 + Random() % 101 : 100)) / 100 + 1 : 0;
    case EFFECT_SUPER_FANG:
        return mult ? (def->hp / 2 > 0 ? def->hp / 2 : 1) : 0;
    case EFFECT_ENDEAVOR: return mult && def->hp>atk->hp?def->hp-atk->hp:0;
    case EFFECT_OHKO:
        return (mult && atk->level >= def->level) ? def->hp : 0;
    }
    if (move == MOVE_STRUGGLE)
    {
        type = TYPE_MYSTERY;
        mult = 100;
        *outMult = mult;
    }

    power=RtBasePower(atk,def,move);
    if(!power) return 0;
    if(move==MOVE_PRESENT)
    {
        if(roll) { u8 present=Random()&255; if(present>=204) { RtSetHp(def,def->hp+(def->maxHp/4?def->maxHp/4:1)); return 0; } power=present<102?40:present<178?80:120; }
        else power=52;
    }
    if(def->hidden && move!=MOVE_THUNDER && move!=MOVE_SKY_UPPERCUT) power*=2;
    if (RtWeatherWorks())
    {
        if(move==MOVE_SOLAR_BEAM && sRt->weather && sRt->weather!=RTW_SUN) power/=2;
        if ((sRt->weather==RTW_RAIN && type==TYPE_WATER) || (sRt->weather==RTW_SUN && type==TYPE_FIRE)) power=power*3/2;
        if ((sRt->weather==RTW_RAIN && type==TYPE_FIRE) || (sRt->weather==RTW_SUN && type==TYPE_WATER)) power/=2;
    }
    if(type==TYPE_ELECTRIC && (atk->clocks[RTC_CHARGE] || (atk->cast.active && atk->cast.phase==1))) power*=2;
    if((type==TYPE_ELECTRIC && def->clocks[RTC_MUDSPORT]) || (type==TYPE_FIRE && def->clocks[RTC_WATERSPORT])) power/=2;
    if(type==TYPE_FIRE && atk->flashFire && atk->ability==ABILITY_FLASH_FIRE) power=power*3/2;
    if(def->ability==ABILITY_THICK_FAT && (type==TYPE_FIRE || type==TYPE_ICE)) power/=2;
    if(atk->hp*3<=atk->maxHp && ((type==TYPE_FIRE && atk->ability==ABILITY_BLAZE) || (type==TYPE_WATER && atk->ability==ABILITY_TORRENT) || (type==TYPE_GRASS && atk->ability==ABILITY_OVERGROW) || (type==TYPE_BUG && atk->ability==ABILITY_SWARM))) power=power*3/2;

    for(i=0;i<ARRAY_COUNT(typeItems);i++) if(heldAtk==typeItems[i][0] && type==typeItems[i][1]) power=power*(100+ItemId_GetHoldEffectParam(atk->item))/100;
    if(roll && critDenom && !(Random()%critDenom)) *outCrit=TRUE;
    if (!RtSpecial(move,type))
    {
        attack = RtDamageStat(atk, RT_STAT_ATK, *outCrit, FALSE);
        if ((atk->status & STATUS1_BURN) && atk->ability!=ABILITY_GUTS) { attack /= 2; if (attack < 1) attack = 1; }
        defense = RtDamageStat(def, RT_STAT_DEF, *outCrit, TRUE);
        if(atk->ability==ABILITY_HUGE_POWER || atk->ability==ABILITY_PURE_POWER) attack*=2;
        if(atk->ability==ABILITY_HUSTLE || (atk->ability==ABILITY_GUTS && atk->status)) attack=attack*3/2;
        if(def->ability==ABILITY_MARVEL_SCALE && def->status) defense=defense*3/2;
        if(heldAtk==HOLD_EFFECT_CHOICE_BAND) attack=attack*3/2;
        if(heldAtk==HOLD_EFFECT_THICK_CLUB && (atk->species==SPECIES_CUBONE || atk->species==SPECIES_MAROWAK)) attack*=2;
        if(heldDef==HOLD_EFFECT_METAL_POWDER && def->species==SPECIES_DITTO) defense*=2;
        if(def->clocks[RTC_REFLECT] && !*outCrit) defense*=2;
    }
    else
    {
        attack = RtDamageStat(atk, RT_STAT_SPATK, *outCrit, FALSE);
        defense = RtDamageStat(def, RT_STAT_SPDEF, *outCrit, TRUE);
        if(heldAtk==HOLD_EFFECT_LIGHT_BALL && atk->species==SPECIES_PIKACHU) attack*=2;
        if(heldAtk==HOLD_EFFECT_DEEP_SEA_TOOTH && atk->species==SPECIES_CLAMPERL) attack*=2;
        if(heldDef==HOLD_EFFECT_DEEP_SEA_SCALE && def->species==SPECIES_CLAMPERL) defense*=2;
        if(heldAtk==HOLD_EFFECT_SOUL_DEW && (atk->species==SPECIES_LATIOS || atk->species==SPECIES_LATIAS)) attack=attack*3/2;
        if(heldDef==HOLD_EFFECT_SOUL_DEW && (def->species==SPECIES_LATIOS || def->species==SPECIES_LATIAS)) defense=defense*3/2;
        if(def->clocks[RTC_LIGHTSCREEN] && !*outCrit) defense*=2;
    }
    if (m->effect == EFFECT_EXPLOSION)
        defense = (defense / 2) > 0 ? defense / 2 : 1;

    damage = ((2 * atk->level / 5 + 2) * (s32)power * attack / defense) / 50 + 2;

    if (*outCrit) damage *= 2;
    if (RtTypeIs(atk,type))
        damage = damage * 15 / 10;
    damage = damage * mult / 100;
    damage = damage * (roll ? 85 + Random() % 16 : 92) / 100;
    if (mult != 0 && damage == 0)
        damage = 1;
    if (m->effect == EFFECT_FALSE_SWIPE && damage >= def->hp)
        damage = def->hp - 1;
    return damage;
}

static s32 RtCalcDamage(struct RtFighter *atk, struct RtFighter *def, u16 move, u16 *outMult, bool8 *outCrit)
{
    return RtComputeDamage(atk,def,move,outMult,outCrit,TRUE);
}

#include "realtime_damage.inc.c"

// One move hits its target.
static void RtResolveHit(struct RtFighter *atk, struct RtFighter *def, u16 move, s16 fxX, s16 fxY)
{ RtApplyImpact(atk,def,move,0); }

// Frames at 60 Hz. Individual move timers are separate from action recovery.
static u16 RtGetMoveCooldown(u16 move)
{
    u8 power = gBattleMoves[move].power;
    if(move==MOVE_QUICK_ATTACK) return 180;
    switch (gBattleMoves[move].effect)
    {
    case EFFECT_SLEEP: return 360;
    case EFFECT_YAWN: return 420;
    case EFFECT_PROTECT: return 600;
    case EFFECT_ENDURE: return 480;
    case EFFECT_RESTORE_HP:
    case EFFECT_SOFTBOILED:
    case EFFECT_REST:
    case EFFECT_MORNING_SUN:
    case EFFECT_SYNTHESIS:
    case EFFECT_MOONLIGHT: return 360;
    case EFFECT_RECHARGE: return 360;
    case EFFECT_OVERHEAT: return 300;
    case EFFECT_OHKO: return 600;
    }
    if (power == 0)
        return 180;
    return 30 + power * 3 / 2;
}

// Shared recovery prevents cycling four powerful moves to bypass their timers.
// This deliberately leaves movement-speed scaling unchanged.
static u16 RtGetMoveRecovery(u16 move)
{
    u8 power = gBattleMoves[move].power;
    if(move==MOVE_QUICK_ATTACK) return 75;
    switch (gBattleMoves[move].effect)
    {
    case EFFECT_RECHARGE: return 360;
    case EFFECT_OHKO: return 240;
    case EFFECT_RESTORE_HP:
    case EFFECT_SOFTBOILED:
    case EFFECT_REST:
    case EFFECT_MORNING_SUN:
    case EFFECT_SYNTHESIS:
    case EFFECT_MOONLIGHT: return 150;
    }
    if (!power) return 75;
    if (power >= 120) return 180;
    if (power >= 90) return 120;
    if (power >= 60) return 75;
    return 45;
}

static bool8 RtIsContactMove(u16 move)
{ return RtProfile(move)->kind == RT_CLOSE || RtProfile(move)->kind == RT_DASH; }

static u8 RtFireProjectile(struct RtFighter *atk, struct RtFighter *def, u16 move)
{
    u8 i; struct RtProjectile *p; s32 dx,dy;
    for(i=0;i<RT_MAX_PROJ;i++) if(!sRt->proj[i].active) break;
    if(i==RT_MAX_PROJ) return 0xFF;
    p=&sRt->proj[i]; memset(p,0,sizeof(*p));
    p->spriteId=MAX_SPRITES; p->fxId=0xFF;
    p->active=TRUE; p->fromEnemy=atk->isEnemy; p->sourceParty=atk->partyIndex; p->move=move;
    p->x=atk->x; p->y=atk->y; p->life=RT_PROJ_LIFE;
    p->maxDistance=RtProfile(move)->reach;
    RtAim(atk,def,&dx,&dy); p->vx=dx*RT_PROJ_SPEED/256; p->vy=dy*RT_PROJ_SPEED/256;
#ifdef PMD_TEST
    if(!atk->isEnemy) gRtCnt[8]++;
#endif
    if(RtProfile(move)->kind==RT_FIELD)
    {
        struct SpriteTemplate t;
        p->fieldKind=(move==MOVE_SPIKES?RTF_SPIKES:RTF_WEB)+1; p->maxDistance=40;
        if(sRt->fieldTiles==0xFFFF || sRt->statusPalSlot==0xFF) return i;
        t.tileTag=0x77B4; t.paletteTag=TAG_NONE; t.oam=&gOamData_AffineOff_ObjNormal_32x32;
        t.anims=gDummySpriteAnimTable; t.images=NULL; t.affineAnims=gDummySpriteAffineAnimTable; t.callback=SpriteCallbackDummy;
        p->spriteId=CreateSprite(&t,FROM_Q8(p->x),FROM_Q8(p->y),0);
        if(p->spriteId!=MAX_SPRITES) { gSprites[p->spriteId].oam.tileNum=sRt->fieldTiles+(p->fieldKind-1)*16; gSprites[p->spriteId].oam.paletteNum=sRt->statusPalSlot; gSprites[p->spriteId].oam.priority=1; }
        return i;
    }
    if(sRtPtdMoveArt[move].shot)
    {
        s16 x,y,tx,ty; s32 visualDx,visualDy;
        atk->dir=RtDirFromVector(dx,dy);
        p->artFront=RtPtdEmitter(atk,dx,dy,&x,&y);
        p->x+=dx*p->artFront;p->y+=dy*p->artFront;p->distance=Q8(p->artFront);
        p->artOffsetX=x-FROM_Q8(p->x);p->artOffsetY=y-FROM_Q8(p->y);
        RtPtdBody(def,&tx,&ty);p->artEndY=ty-FROM_Q8(def->y);
        p->artRange=RtLength(FROM_Q8(def->x-atk->x),FROM_Q8(def->y-atk->y));
        if(p->artRange<=p->artFront)p->artRange=p->artFront+1;
        p->fxId=RtPtdSpawn(sRtPtdMoveArt[move].shot,x,y,0xFF,TRUE);
        RtNormalize(tx-x,ty-y,256,&visualDx,&visualDy);RtPtdOrient(p->fxId,visualDx,visualDy,256,256);
        if(!RtPtdRayClear(FROM_Q8(atk->x),FROM_Q8(atk->y),FROM_Q8(p->x),FROM_Q8(p->y)))p->life=0;
        if(p->fxId!=0xFF)return i;
    }
    { u8 fx=RtProfile(move)->fx;
      if(move==MOVE_SECRET_POWER) { static const u8 effects[]={FX_LEAF,FX_LEAF,FX_ROCK,FX_WATER,FX_WATER,FX_WATER,FX_ROCK,FX_SHADOW,FX_SPARK,FX_IMPACT}; u8 terrain=BattleSetup_GetTerrainId(); fx=effects[terrain<10?terrain:9]; }
      p->fxId=RtSpawnFx(fx,FROM_Q8(p->x),FROM_Q8(p->y),0xFF,TRUE); }
    return i;
}
static void RtFixedShot(struct RtFighter *f, struct RtFighter *target, u16 move, u16 damage)
{
    u8 i=RtFireProjectile(f,target,move);
    if(i<RT_MAX_PROJ)
    { struct RtProjectile *p=&sRt->proj[i];p->fixedDamage=damage;p->maxDistance=144;RtNormalize(target->x-f->x,target->y-f->y,RT_PROJ_SPEED,&p->vx,&p->vy); }
}
static void RtUpdateProjectiles(void)
{
    u8 i; struct RtProjectile *p; struct RtFighter *target,*owner; s16 tx,ty; s32 dist; bool8 touched;
    for(i=0;i<RT_MAX_PROJ;i++)
    {
        p=&sRt->proj[i]; if(!p->active) continue; touched=FALSE;
        target=p->fromEnemy?&sRt->player:&sRt->enemy; owner=p->fromEnemy?&sRt->enemy:&sRt->player;
        if(owner->partyIndex!=p->sourceParty || !owner->hp) p->life=0;
        if((RtProfile(p->move)->flags&RTMF_ALWAYS) && !target->hidden)
            RtNormalize(target->x-p->x,target->y-p->y,RT_PROJ_SPEED,&p->vx,&p->vy);
        p->x+=p->vx; p->y+=p->vy; p->distance+=RtLength(p->vx,p->vy); if(p->age<255) p->age++;
        if(p->fxId!=0xFF && sRt->fx[p->fxId].active)
        {
            struct RtFx *fx=&sRt->fx[p->fxId];struct Sprite *sprite=&gSprites[fx->spriteId];
            sprite->x=FROM_Q8(p->x);sprite->y=FROM_Q8(p->y);
            if(fx->ptdAnim)
            {
                s32 fraction=(FROM_Q8(p->distance)-p->artFront)*256/(p->artRange-p->artFront);
                if(fraction<0)fraction=0;if(fraction>256)fraction=256;
                sprite->x+=p->artOffsetX*(256-fraction)/256;
                sprite->y+=(p->artOffsetY*(256-fraction)+p->artEndY*fraction)/256;
            }
        }
        if(p->spriteId!=MAX_SPRITES) { gSprites[p->spriteId].x=FROM_Q8(p->x); gSprites[p->spriteId].y=FROM_Q8(p->y); }
        if(RtProfile(p->move)->flags&RTMF_ALWAYS)
        { if(RtLength(FROM_Q8(target->x-owner->x),FROM_Q8(target->y-owner->y))>p->maxDistance) p->life=0; }
        else if(p->distance>Q8(p->maxDistance)) p->life=0;
        if(p->life && sRt->arena && RtArenaHitCover(FROM_Q8(p->x-p->vx),FROM_Q8(p->y-p->vy),FROM_Q8(p->x),FROM_Q8(p->y),p->move)) p->life=0;
        if(p->life && !p->fieldKind)
        {
            struct RtField *sub=RtSubstitute(target);
            if(sub && RtAbs(FROM_Q8(p->x)-sub->x)<17 && RtAbs(FROM_Q8(p->y)-sub->y)<18)
            { u16 mult; bool8 crit; u16 damage=p->fixedDamage?p->fixedDamage:RtDamaging(p->move)?RtCalcDamage(owner,target,p->move,&mult,&crit):0; RtHitSubstitute(owner,sub->x,sub->y,damage); p->life=0; }
        }
        if(p->life && target->active && target->hp && RtCanHitHidden(target,p->move) && RtAbs(FROM_Q8(p->x-target->x))<RT_HIT_RADIUS && RtAbs(FROM_Q8(p->y-target->y))<RT_HIT_RADIUS)
        { if(!p->fieldKind) RtApplyImpact(owner,target,p->move,p->fixedDamage); p->life=0;touched=TRUE; }
        RtScreenToTile(FROM_Q8(p->x),FROM_Q8(p->y)+4,&tx,&ty);
        if(!sRt->arena && p->distance>Q8(16) && MapGridGetCollisionAt(tx,ty) && !(TestPlayerAvatarFlags(PLAYER_AVATAR_FLAG_SURFING) && MetatileBehavior_IsSurfable(MapGridGetMetatileBehaviorAt(tx,ty)))) p->life=0;
        if(FROM_Q8(p->x)<RT_FIELD_LEFT-32 || FROM_Q8(p->x)>RT_FIELD_RIGHT+32 || FROM_Q8(p->y)<RT_FIELD_TOP-32 || FROM_Q8(p->y)>RT_FIELD_BOTTOM+32) p->life=0;
        if(!p->life || !--p->life)
        {
            if(!touched && RtPtdHasArt(p->move) && p->fxId<RT_MAX_FX && sRt->fx[p->fxId].active)
            {struct Sprite *sprite=&gSprites[sRt->fx[p->fxId].spriteId];RtPtdMiss(p->move,sprite->x,sprite->y);}
            if(p->fieldKind && owner->hp && owner->partyIndex==p->sourceParty)
            {RtPlaceField(owner,p->fieldKind-1,FROM_Q8(p->x),FROM_Q8(p->y));RtPtdStatusAt(p->move,FROM_Q8(p->x),FROM_Q8(p->y));}
            RtDestroyFx(p->fxId);
            if(p->spriteId!=MAX_SPRITES) DestroySprite(&gSprites[p->spriteId]);
            p->active=FALSE;
        }
    }
}

static void RtClearProjectiles(void)
{
    u8 i;

    for (i = 0; i < RT_MAX_PROJ; i++)
    {
        if (sRt->proj[i].active)
        {
            RtDestroyFx(sRt->proj[i].fxId);
            if(sRt->proj[i].spriteId!=MAX_SPRITES) DestroySprite(&gSprites[sRt->proj[i].spriteId]);
            sRt->proj[i].active = FALSE;
        }
    }
}

#include "realtime_flame.inc.c"
#include "realtime_psychic.inc.c"
#include "realtime_delivery.inc.c"

// Starts using a move. Contact moves resolve at the peak of the lunge.
static void RtUseMove(struct RtFighter *atk, struct RtFighter *def, u16 move)
{
    u8 slot; u16 duration=RtGetMoveCooldown(move);
    if (atk->isEnemy) gBattleResults.lastUsedMoveOpponent=move;
    else gBattleResults.lastUsedMovePlayer=move;
    atk->cooldown=RtGetMoveRecovery(move);
    if(move==MOVE_SOLAR_BEAM && sRt->weather==RTW_SUN && RtWeatherWorks()) { duration/=2; atk->cooldown/=2; }
    atk->recoveryMax=atk->cooldown;
    if(move==MOVE_STRUGGLE) atk->struggleCooldown=duration;
    for(slot=0;slot<4;slot++) if(RtMove(atk,slot)==move)
    { atk->lastSlot=slot; atk->moveCooldown[slot]=atk->moveCooldownMax[slot]=duration; }
    RtLaunchCast(atk,def,move);
    atk->lastMove=move;
    if(atk->actionsUsed<0xFFFF) atk->actionsUsed++;
    if(ItemId_GetHoldEffect(atk->item)==HOLD_EFFECT_CHOICE_BAND && !atk->choiceMove) atk->choiceMove=move;
}

static void RtUpdateLunge(struct RtFighter *atk, struct RtFighter *def)
{ RtUpdateCast(atk,def); }

static void RtUpdateCommon(struct RtFighter *f)
{
    u8 slot;
    u16 elapsed = sRt->frameStep;
    RtUpdateStatus(f);
    RtUpdateClocks(f);
    for (slot = 0; slot < MAX_MON_MOVES; slot++)
        f->moveCooldown[slot] = f->moveCooldown[slot] > elapsed ? f->moveCooldown[slot] - elapsed : 0;
    f->struggleCooldown = f->struggleCooldown > elapsed ? f->struggleCooldown - elapsed : 0;
    f->cooldown = f->cooldown > elapsed ? f->cooldown - elapsed : 0;
    if (f->hurtTimer != 0)
        f->hurtTimer--;
    if (f->kbX != 0 || f->kbY != 0)
    {
        RtMoveFighter(f, f->kbX, f->kbY);
        f->kbX = f->kbX * 3 / 4;
        f->kbY = f->kbY * 3 / 4;
        if (RtAbs(f->kbX) < 32 && RtAbs(f->kbY) < 32)
        {
            f->kbX = 0;
            f->kbY = 0;
        }
    }
}

static u16 RtGetPlayerMove(u8 slot, bool8 *usesPp)
{
    *usesPp=FALSE;
    if (!RtHasPp(&sRt->player)) return MOVE_STRUGGLE;
    if (!RtMove(&sRt->player,slot) || !RtPp(&sRt->player,slot)) return MOVE_NONE;
    *usesPp=TRUE; return RtMove(&sRt->player,slot);
}

static void RtUpdatePlayer(void)
{
    struct RtFighter *f = &sRt->player;
    s32 dx = 0, dy = 0, speed;
    u16 held = gMain.heldKeysRaw;
    u16 pressed = gMain.newKeysRaw;
    u8 i;
    u16 move;
    bool8 usesPp;

    RtUpdateCommon(f);
    if (!f->hp) return;
    RtUpdateLunge(f, &sRt->enemy);

    if (RtCanMove(f) && !f->clocks[RTC_RAMPAGE] && (!f->cast.active || f->cast.kind == RT_BEAM))
    {
        if (held & DPAD_LEFT)
            dx = -1;
        else if (held & DPAD_RIGHT)
            dx = 1;
        if (held & DPAD_UP)
            dy = -1;
        else if (held & DPAD_DOWN)
            dy = 1;

        if (dx != 0 || dy != 0)
        {
            speed = RtGetMoveSpeed(f);
            if (dx != 0 && dy != 0)
                speed = speed * 181 / 256; // diagonal
            RtMoveFighter(f, dx * speed, dy * speed);
            if (pressed & (DPAD_LEFT | DPAD_RIGHT | DPAD_UP | DPAD_DOWN) || (dx != 0 && dy == 0) || (dy != 0 && dx == 0))
                if (!f->cast.active) f->dir = RtDirFromVector(dx, dy);
            if (!f->cast.active) { f->faceX = dx; f->faceY = dy; }
        }
    }

    if (f->cooldown == 0 && !RtBusy(f) && !f->clocks[RTC_RAMPAGE])
    {
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            if (!(pressed & sMoveButtons[i]))
                continue;
            move = RtGetPlayerMove(i, &usesPp);
            if (move == MOVE_NONE || (move == MOVE_STRUGGLE ? f->struggleCooldown != 0 : f->moveCooldown[i] != 0))
            {
                PlaySE(SE_FAILURE);
                break;
            }
            if (!RtCanAct(f, move)) break;
            if (usesPp)
            {
                RtSpendPp(f,i,move);
            }
            RtUseMove(f, &sRt->enemy, move);
            break;
        }
    }
}

static u16 RtChooseEnemyMove(void)
{
    struct RtFighter *f=&sRt->enemy;
    u16 moves[MAX_MON_MOVES];
    u8 count = 0, damaging = 0, i;
    u16 move;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        move = RtMove(f,i);
        if (move == MOVE_NONE || !RtPp(f,i) || !RtMoveAllowed(f,move) || sRt->enemy.moveCooldown[i] != 0)
            continue;
        moves[count++] = move;
        if (gBattleMoves[move].power != 0)
            damaging++;
    }
    if (count == 0)
        return !RtHasPp(f) && !f->struggleCooldown ? MOVE_STRUGGLE : MOVE_NONE;
    // Prefer attacks; status moves about a quarter of the time.
    for (i = 0; i < 4; i++)
    {
        move = moves[Random() % count];
        if (gBattleMoves[move].power != 0 || damaging == 0 || (Random() % 4) == 0)
            return move;
    }
    return moves[0];
}

static void RtUpdateEasyEnemy(void)
{
    struct RtFighter *f = &sRt->enemy;
    struct RtFighter *target = &sRt->player;
    s32 dx, dy, dist, vx, vy, speed;
    bool8 contact, isStatus;
    u16 move;

    RtUpdateCommon(f);
    if (!f->hp || (f->status & (STATUS1_SLEEP | STATUS1_FREEZE))) return;
    RtUpdateLunge(f, target);
    if (RtBusy(f) || !RtCanMove(f) || f->clocks[RTC_RAMPAGE])
        return;

    dx = FROM_Q8(target->x - f->x);
    dy = FROM_Q8(target->y - f->y);
    dist = RtLength(dx, dy);
    speed = RtGetMoveSpeed(f);

    if (f->windup != 0)
    {
        RtFace(f, target);
        if (--f->windup == 0 && RtCanAct(f, f->pendingMove))
        { u8 slot; for(slot=0;slot<4;slot++) if(RtMove(f,slot)==f->pendingMove && RtPp(f,slot)) { RtSpendPp(f,slot,f->pendingMove); break; } RtUseMove(f, target, f->pendingMove); }
        return;
    }

    if (!target->active || target->hp == 0)
        return;

    if (f->cooldown != 0 || f->hurtTimer > 6)
    {
        // Wander a little between attacks.
        if (f->aiTimer == 0)
        {
            f->aiTimer = 20 + Random() % 40;
            f->aiDx = (Random() % 3) - 1;
            f->aiDy = (Random() % 3) - 1;
            // Keep some distance when close.
            if (dist < 24)
            {
                f->aiDx = dx > 0 ? -1 : 1;
                f->aiDy = dy > 0 ? -1 : 1;
            }
        }
        f->aiTimer--;
        if ((f->aiDx != 0 || f->aiDy != 0) && f->hurtTimer == 0)
        {
            if (!RtMoveFighter(f, f->aiDx * speed / 2, f->aiDy * speed / 2))
                f->aiTimer = 0;
            if (f->aiDx != 0)
                f->lastHorizDir = f->aiDx < 0 ? RT_DIR_LEFT : RT_DIR_RIGHT;
        }
        return;
    }

    if (f->aiMove == MOVE_NONE)
        f->aiMove = RtChooseEnemyMove();
    move = f->aiMove;
    if (move == MOVE_NONE)
    {
        f->cooldown = 6;
        return;
    }
    isStatus = gBattleMoves[move].power == 0;
    contact = RtIsContactMove(move) || move == MOVE_STRUGGLE;

    if (RtProfile(move)->kind==RT_SELF || dist<RtProfile(move)->reach)
    {
        // In range: telegraph, then attack.
        f->pendingMove = move;
        f->aiMove = MOVE_NONE;
        f->windup = contact ? 16 : 22;
        RtFace(f, target);
        return;
    }

    // Chase.
    RtNormalize(dx, dy, speed, &vx, &vy);
    if (!RtMoveFighter(f, vx, vy))
    {
        // Blocked: try a random sidestep for a moment.
        f->aiDx = (Random() % 3) - 1;
        f->aiDy = (Random() % 3) - 1;
        RtMoveFighter(f, f->aiDx * speed, f->aiDy * speed);
    }
    RtFace(f, target);
}

#include "realtime_ai.inc.c"
#include "realtime_roster.inc.c"

// ---------------------------------------------------------------------------
// Rewards
// ---------------------------------------------------------------------------

static void RtGiveExperience(u8 partyIndex,u16 enemySpecies,u32 gained)
{
    struct RtFighter *f = &sRt->player;
    struct Pokemon *mon = &gPlayerParty[partyIndex];
    u32 exp, maximum;
    bool8 boosted = FALSE;
    u16 learned;
    bool8 first;
    u8 level;

    if (GetMonData(mon, MON_DATA_HP) == 0)
        return;

    if (GetMonData(mon, MON_DATA_HELD_ITEM) == ITEM_LUCKY_EGG)
    {
        gained = gained * 150 / 100;
        boosted = TRUE;
    }
    if (GetMonData(mon, MON_DATA_OT_ID) != (u32)(gSaveBlock2Ptr->playerTrainerId[0] | (gSaveBlock2Ptr->playerTrainerId[1] << 8) | (gSaveBlock2Ptr->playerTrainerId[2] << 16) | (gSaveBlock2Ptr->playerTrainerId[3] << 24)))
    {
        gained = gained * 150 / 100;
        boosted = TRUE;
    }

    MonGainEVs(mon, enemySpecies);

    if (GetMonData(mon, MON_DATA_LEVEL) >= MAX_LEVEL)
        return;

    exp = GetMonData(mon, MON_DATA_EXP);
    maximum = gExperienceTables[gSpeciesInfo[GetMonData(mon,MON_DATA_SPECIES)].growthRate][MAX_LEVEL];
    exp = gained > maximum - exp ? maximum : exp + gained;
    SetMonData(mon, MON_DATA_EXP, &exp);

    RtGetNickname(mon, gStringVar1);
    ConvertIntToDecimalStringN(gStringVar2, gained, STR_CONV_MODE_LEFT_ALIGN, 6);
    RtQueueMessage(boosted ? sText_GainedBoosted : sText_GainedExp);

    while (TryIncrementMonLevel(mon))
    {
        CalculateMonStats(mon);
        AdjustFriendship(mon, FRIENDSHIP_EVENT_GROW_LEVEL);
        sRt->leveledUpMask |= 1 << partyIndex;
        level = GetMonData(mon, MON_DATA_LEVEL);
        RtGetNickname(mon, gStringVar1);
        ConvertIntToDecimalStringN(gStringVar2, level, STR_CONV_MODE_LEFT_ALIGN, 3);
        RtQueueMessage(sText_GrewTo);

        first = TRUE;
        while ((learned = MonTryLearningNewMove(mon, first)) != MOVE_NONE)
        {
            first = FALSE;
            if (learned == MON_ALREADY_KNOWS_MOVE)
                continue;
            RtGetNickname(mon, gStringVar1);
            if (learned == MON_HAS_MAX_MOVES)
            {
                u8 j;
                for(j=0;j<sRt->learnCount && (sRt->learnMoves[j]!=gMoveToLearn || sRt->learnParty[j]!=partyIndex);j++);
                if(j==sRt->learnCount && sRt->learnCount<ARRAY_COUNT(sRt->learnMoves)) {sRt->learnParty[sRt->learnCount]=partyIndex;sRt->learnMoves[sRt->learnCount++]=gMoveToLearn;}
            }
            else
            {
                StringCopy(gStringVar2, gMoveNames[learned]);
                RtQueueMessage(sText_Learned);
            }
        }
    }

    if(partyIndex!=f->partyIndex)return;
    f->level = GetMonData(mon, MON_DATA_LEVEL);
    f->hp = GetMonData(mon, MON_DATA_HP);
    f->maxHp = GetMonData(mon, MON_DATA_MAX_HP);
    f->shownHp = f->hp;
    sRt->dirtyPlayer = TRUE;
    sRt->dirtyMoves = TRUE;
}

static void RtAwardExperience(void)
{
    u16 species=GetMonData(sRt->enemy.mon,MON_DATA_SPECIES);
    u32 base=(gSpeciesInfo[species].expYield*sRt->enemy.level)/7,amount;
    u8 i;bool8 sharing=IsPartyExpShareEnabled();
    if(sRt->isTrainer)base=base*3/2;
    if(!base)base=1;
    for(i=0;i<gPlayerPartyCount;i++) {
        struct Pokemon *mon=&gPlayerParty[i];
        if(!GetMonData(mon,MON_DATA_SPECIES) || GetMonData(mon,MON_DATA_IS_EGG) || !GetMonData(mon,MON_DATA_HP))continue;
        if(!sharing && i!=sRt->player.partyIndex)continue;
        amount=sharing && !(sRt->expParticipantsMask&(1<<i))?base/2:base;
        RtGiveExperience(i,species,amount?amount:1);
    }
}

// ---------------------------------------------------------------------------
// Poke Balls
// ---------------------------------------------------------------------------

static const u16 sRtBallItems[] = {
    ITEM_POKE_BALL, ITEM_GREAT_BALL, ITEM_ULTRA_BALL, ITEM_NET_BALL,
    ITEM_DIVE_BALL, ITEM_NEST_BALL, ITEM_REPEAT_BALL, ITEM_TIMER_BALL,
    ITEM_LUXURY_BALL, ITEM_PREMIER_BALL, ITEM_SAFARI_BALL, ITEM_MASTER_BALL
};
static const u8 sText_PausedTeam[] = _("PAUSED - YOUR TEAM");
static const u8 sText_TeamControls[] = _("UP/DOWN: choose  A: switch\nL/R: move details  B/SELECT: resume");
static const u8 sText_CantSwitch[] = _("Choose another healthy POKéMON.");
static const u8 sText_SwitchBlocked[] = _("This POKéMON is trapped!");
static const u8 sText_PausedBalls[] = _("PAUSED - POKé BALLS");
static const u8 sText_PausedMoves[] = _("PAUSED - MOVE DETAILS");
static const u8 sText_PickerControls[] = _("UP/DOWN: choose   A: throw\nL/R: move details   B/SELECT: resume");
static const u8 sText_MoveDetailsControls[] = _("L/R: POKé BALLS\nB/SELECT: resume battle");
static const u8 sText_MasterConfirm[] = _("Use a MASTER BALL?\nA: throw   B: keep choosing");
static const u8 sText_StorageFull[] = _("Party and PC are full.\nB/SELECT: resume battle");
static const u8 sText_TrainerNoCatch[] = _("You can't catch a trainer's POKéMON.\nB/SELECT: resume battle");
static const u8 sText_PickerCursor[] = _(">");

static void RtMenuPrint(u16 x, u16 y, const u8 *text, const u8 *color)
{
    RtPrint(RT_WIN_MENU, FONT_SMALL, x, y, color, text);
}

#include "realtime_learning.inc.c"

static void RtDrawPicker(void)
{
    u8 i, start;
    u16 item, move, pp;
    u8 text[32], *ptr;
    struct RtFighter *f = &sRt->player;
    RtDrawPanel(RT_WIN_MENU, C_PANEL_DARK);
    RtMenuPrint(4, 2, sRt->pickerTab ? sText_PausedMoves : sRt->partyPicker ? sText_PausedTeam : sText_PausedBalls, sTextAccent);
    if (!sRt->pickerTab && sRt->partyPicker)
    {
        for (i = 0; i < gPlayerPartyCount; i++)
        {
            u16 hp = GetMonData(&gPlayerParty[i], MON_DATA_HP);
            RtGetNickname(&gPlayerParty[i], text);
            if (i == sRt->pickerCursor) RtMenuPrint(4, 23 + i * 14, sText_PickerCursor, sTextAccent);
            RtMenuPrint(16, 23 + i * 14, text, hp ? sTextWhite : sTextGray);
            ptr = ConvertIntToDecimalStringN(text, hp, STR_CONV_MODE_LEFT_ALIGN, 3);
            ptr = StringCopy(ptr, sText_Slash);
            ConvertIntToDecimalStringN(ptr, GetMonData(&gPlayerParty[i], MON_DATA_MAX_HP), STR_CONV_MODE_LEFT_ALIGN, 3);
            RtPrintRight(RT_WIN_MENU, FONT_SMALL, 214, 23 + i * 14, hp ? sTextWhite : sTextGray, text);
        }
        RtMenuPrint(4, 108, sText_TeamControls, sTextWhite);
    }
    else if (!sRt->pickerTab)
    {
        if (!sRt->pickerCount) RtMenuPrint(4, 25, sText_NoBalls, sTextWhite);
        start = (sRt->pickerCursor / 7) * 7;
        for (i = start; i < sRt->pickerCount && i < start + 7; i++)
        {
            item = sRt->pickerBalls[i];
            if (i == sRt->pickerCursor) RtMenuPrint(4, 21 + (i - start) * 12, sText_PickerCursor, sTextAccent);
            RtMenuPrint(16, 21 + (i - start) * 12, ItemId_GetName(item), i == sRt->pickerCursor ? sTextAccent : sTextWhite);
            ConvertIntToDecimalStringN(text, BagGetQuantityByItemId(item), STR_CONV_MODE_LEFT_ALIGN, 4);
            RtPrintRight(RT_WIN_MENU, FONT_SMALL, 214, 21 + (i - start) * 12, sTextWhite, text);
        }
        RtMenuPrint(4, 108, sRt->masterConfirm ? sText_MasterConfirm : sText_PickerControls, sTextWhite);
    }
    else
    {
        RtGetNickname(f->mon, text);
        RtMenuPrint(4, 19, text, sTextWhite);
        ptr = ConvertIntToDecimalStringN(text, f->hp, STR_CONV_MODE_LEFT_ALIGN, 3);
        ptr = StringCopy(ptr, sText_Slash);
        ConvertIntToDecimalStringN(ptr, f->maxHp, STR_CONV_MODE_LEFT_ALIGN, 3);
        RtPrintRight(RT_WIN_MENU, FONT_SMALL, 214, 19, sTextWhite, text);
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            move = RtMove(f, i);
            RtMenuPrint(4, 38 + i * 18, sButtonLabels[i], sTextAccent);
            RtMenuPrint(16, 38 + i * 18, move == MOVE_NONE ? sText_Dash : gMoveNames[move], sTextWhite);
            pp = RtPp(f, i);
            ptr = ConvertIntToDecimalStringN(text, pp, STR_CONV_MODE_LEFT_ALIGN, 2);
            ptr = StringCopy(ptr, sText_Slash);
            ConvertIntToDecimalStringN(ptr, move == MOVE_NONE ? 0 : RtMaxPp(f, i), STR_CONV_MODE_LEFT_ALIGN, 2);
            RtPrintRight(RT_WIN_MENU, FONT_SMALL, 214, 38 + i * 18, sTextWhite, text);
        }
        RtMenuPrint(4, 108, sText_MoveDetailsControls, sTextWhite);
    }
    PutWindowTilemap(sRt->winIds[RT_WIN_MENU]);
    CopyWindowToVram(sRt->winIds[RT_WIN_MENU], COPYWIN_FULL);
}

static void RtOpenPicker(void)
{
    u8 i;
    sRt->winIds[RT_WIN_MENU] = AddWindow(&sRtWindowTemplates[RT_WIN_MENU]);
    if (sRt->winIds[RT_WIN_MENU] == WINDOW_NONE) return;
    FillBgTilemapBufferRect(0,0,0,0,30,20,0);
    RtLoadNativeFrame(TRUE);
    for (i = 0; i < RT_WIN_MENU; i++)
    {
        ClearWindowTilemap(sRt->winIds[i]);
        CopyWindowToVram(sRt->winIds[i], COPYWIN_MAP);
    }
    sRt->partyPicker=sRt->isTrainer || sRt->batonPass;
    sRt->pickerCount = 0;
    for (i = 0; i < ARRAY_COUNT(sRtBallItems); i++)
        if (CheckBagHasItem(sRtBallItems[i], 1))
            sRt->pickerBalls[sRt->pickerCount++] = sRtBallItems[i];
    if (sRt->partyPicker) sRt->pickerCount = gPlayerPartyCount;
    sRt->pickerCursor = sRt->partyPicker ? sRt->player.partyIndex : 0;
    sRt->pickerTab = 0;
    sRt->masterConfirm = FALSE;
    for (i = 0; i < MAX_SPRITES; i++)
    {
        sRt->spriteAnimPaused[i] = gSprites[i].animPaused;
        sRt->spriteAffinePaused[i] = gSprites[i].affineAnimPaused;
        if (gSprites[i].inUse) { gSprites[i].animPaused = TRUE; gSprites[i].affineAnimPaused = TRUE; }
    }
    sRt->pickerReturnState = sRt->state;
    sRt->pickerReturnTimer = sRt->timer;
    sRt->state = RT_STATE_PICKER;
    RtDrawPicker();
}

static void RtClosePicker(void)
{
    u8 i;
    ClearStdWindowAndFrameToTransparent(sRt->winIds[RT_WIN_MENU],TRUE);
    RemoveWindow(sRt->winIds[RT_WIN_MENU]);
    sRt->winIds[RT_WIN_MENU] = WINDOW_NONE;
    RtLoadNativeFrame(FALSE);
    for (i = 0; i < MAX_SPRITES; i++)
    {
        gSprites[i].animPaused = sRt->spriteAnimPaused[i];
        gSprites[i].affineAnimPaused = sRt->spriteAffinePaused[i];
    }
    RtDrawBallHint();
    RtDrawEnemyHud();
    PutWindowTilemap(sRt->winIds[RT_WIN_ENEMY]);
    CopyWindowToVram(sRt->winIds[RT_WIN_ENEMY], COPYWIN_FULL);
    if (sRt->player.active) RtShowPlayerHud();
    if (sRt->msgVisible)
    {
        if (!sRt->msgCompact) DrawTextBorderOuter(sRt->winIds[RT_WIN_MSG],RT_FRAME_TILES,RT_FRAME_PAL);
        PutWindowTilemap(sRt->winIds[RT_WIN_MSG]);
        CopyWindowToVram(sRt->winIds[RT_WIN_MSG], COPYWIN_FULL);
    }
    sRt->state = sRt->pickerReturnState;
    sRt->timer = sRt->pickerReturnTimer;
    sRt->lastVblank = gMain.vblankCounter2;
}

static bool8 RtHasCatchStorage(void)
{
    u16 i;
    if (gPlayerPartyCount < PARTY_SIZE) return TRUE;
    for (i = 0; i < TOTAL_BOXES_COUNT * IN_BOX_COUNT; i++)
        if (!GetBoxMonDataAt(i / IN_BOX_COUNT, i % IN_BOX_COUNT, MON_DATA_SPECIES)) return TRUE;
    return FALSE;
}

static void RtPickerError(const u8 *text)
{
    PlaySE(SE_FAILURE);
    FillWindowPixelRect(sRt->winIds[RT_WIN_MENU], PIXEL_FILL(C_PANEL_LIGHT), 3, 108, 218, 28);
    RtMenuPrint(4, 108, text, sTextRed);
    CopyWindowToVram(sRt->winIds[RT_WIN_MENU], COPYWIN_GFX);
}

static void RtUpdatePicker(void)
{
    u8 i;
    u16 keys = gMain.newKeysRaw;
    if (keys & (B_BUTTON | SELECT_BUTTON))
    {
        if (sRt->masterConfirm) { sRt->masterConfirm = FALSE; RtDrawPicker(); }
        else { sRt->batonPass=FALSE; RtClosePicker(); }
        return;
    }
    if (keys & (L_BUTTON | R_BUTTON))
    {
        sRt->pickerTab ^= 1;
        sRt->masterConfirm = FALSE;
        RtDrawPicker();
    }
    if (sRt->pickerTab || !sRt->pickerCount) return;
    if (keys & (DPAD_UP | DPAD_DOWN))
    {
        sRt->pickerCursor = (sRt->pickerCursor + sRt->pickerCount + ((keys & DPAD_UP) ? -1 : 1)) % sRt->pickerCount;
        sRt->masterConfirm = FALSE;
        PlaySE(SE_SELECT);
        RtDrawPicker();
    }
    if (keys & A_BUTTON)
    {
        if (sRt->partyPicker)
        {
            if (sRt->pickerCursor == sRt->player.partyIndex || !RtIsUsableMon(&gPlayerParty[sRt->pickerCursor])) { RtPickerError(sText_CantSwitch); return; }
            if (sRt->player.noEscapeTimer || sRt->player.clocks[RTC_TRAP] || sRt->player.clocks[RTC_INGRAIN]) { RtPickerError(sText_SwitchBlocked); return; }
            i = sRt->pickerCursor;
            RtClosePicker();
            RtSwitchPlayer(i);
            return;
        }
        if (!RtHasCatchStorage()) { RtPickerError(sText_StorageFull); return; }
        if (sRt->pickerBalls[sRt->pickerCursor] == ITEM_MASTER_BALL && !sRt->masterConfirm)
        {
            sRt->masterConfirm = TRUE;
            RtDrawPicker();
            return;
        }
        RtStartBallThrow(sRt->pickerBalls[sRt->pickerCursor]);
    }
}

// Gen 3 catch formula. Returns the number of shakes (4 = caught).
static u8 RtRollCatch(u16 ball)
{
    u32 odds, shakeOdds;
    u8 mult = 10, shakes;
    u16 maxHp = sRt->enemy.maxHp;
    u16 hp = sRt->enemy.hp;
    u8 catchRate = gSpeciesInfo[sRt->enemy.species].catchRate;

    if (ball == ITEM_MASTER_BALL) return 4;
    switch (ball)
    {
    case ITEM_SAFARI_BALL: mult = 15; break;
    case ITEM_NET_BALL:
        if (gSpeciesInfo[sRt->enemy.species].types[0] == TYPE_BUG || gSpeciesInfo[sRt->enemy.species].types[1] == TYPE_BUG
         || gSpeciesInfo[sRt->enemy.species].types[0] == TYPE_WATER || gSpeciesInfo[sRt->enemy.species].types[1] == TYPE_WATER) mult = 30;
        break;
    case ITEM_TIMER_BALL:
        mult = 10 + sRt->fightFrames / 180;
        if (sRt->fightFrames >= 5400) mult = 40;
        break;
    case ITEM_GREAT_BALL: mult = 15; break;
    case ITEM_ULTRA_BALL: mult = 20; break;
    case ITEM_NEST_BALL:
        if (sRt->enemy.level < 30)
            mult = 40 - sRt->enemy.level;
        break;
    case ITEM_REPEAT_BALL:
        if (GetSetPokedexFlag(SpeciesToNationalPokedexNum(sRt->enemy.species), FLAG_GET_CAUGHT))
            mult = 30;
        break;
    }

    odds = (catchRate * mult / 10) * (maxHp * 3 - hp * 2) / (3 * maxHp);
    {
        u32 status = GetMonData(sRt->enemy.mon, MON_DATA_STATUS);
        if (status & (STATUS1_SLEEP | STATUS1_FREEZE)) odds *= 2;
        else if (status & (STATUS1_POISON | STATUS1_TOXIC_POISON | STATUS1_BURN | STATUS1_PARALYSIS)) odds = odds * 15 / 10;
    }
    if (odds > 254)
        return 4;
    if (odds == 0)
        odds = 1;
    shakeOdds = Sqrt(Sqrt(16711680 / odds));
    shakeOdds = 1048560 / shakeOdds;
    for (shakes = 0; shakes < 4 && Random() < shakeOdds; shakes++)
        ;
    return shakes;
}

static void RtStartBallThrow(u16 ball)
{
    struct SpriteTemplate template;
    u8 i;

    if (ball == ITEM_NONE)
    {
        PlaySE(SE_FAILURE);
        RtShowTransient(sText_NoBalls, 50);
        return;
    }
    // A confirmed throw releases transient attack effects to guarantee room
    // for its ball graphics. Cancelling the picker never touches them.
    RtStopPresentation(TRUE);
    RtCancelCast(&sRt->player); RtCancelCast(&sRt->enemy);
    RtClearProjectiles();
    for (i = 0; i < RT_MAX_FX; i++) RtDestroyFx(i);
    sRt->ballItem = ball;
    sRt->ballId = ItemIdToBallId(ball);
    if (LoadCompressedSpriteSheetUsingHeap(&gBallSpriteSheets[sRt->ballId])
     || GetSpriteTileStartByTag(gBallSpriteSheets[sRt->ballId].tag) == 0xFFFF)
    {
        PlaySE(SE_FAILURE);
        return;
    }
    sRt->ballPalSlot = RtAcquireCompressedPal(RT_PAL_KEY_BALL(sRt->ballId), gBallSpritePalettes[sRt->ballId].data);

    template.tileTag = gBallSpriteSheets[sRt->ballId].tag;
    template.paletteTag = TAG_NONE;
    template.oam = &gOamData_AffineOff_ObjNormal_16x16;
    template.anims = gDummySpriteAnimTable;
    template.images = NULL;
    template.affineAnims = gDummySpriteAffineAnimTable;
    template.callback = SpriteCallbackDummy;

    sRt->ballStartX = Q8(sRt->originX);
    sRt->ballStartY = Q8(sRt->originY - 8);
    sRt->ballEndX = sRt->enemy.x;
    sRt->ballEndY = sRt->enemy.y;
    sRt->ballSpriteId = CreateSprite(&template, FROM_Q8(sRt->ballStartX), FROM_Q8(sRt->ballStartY), 0);
    if (sRt->ballSpriteId != MAX_SPRITES)
    {
        gSprites[sRt->ballSpriteId].oam.priority = 1;
        if (sRt->ballPalSlot != 0xFF)
            gSprites[sRt->ballSpriteId].oam.paletteNum = sRt->ballPalSlot;
    }
    if (sRt->ballSpriteId == MAX_SPRITES || sRt->ballPalSlot == 0xFF)
    {
        RtDestroyBall();
        PlaySE(SE_FAILURE);
        return;
    }
    RtClosePicker();
    RemoveBagItem(ball, 1);
    if (ball == ITEM_MASTER_BALL) gBattleResults.usedMasterBall = TRUE;
    sRt->ballShakesTotal = RtRollCatch(ball);
    sRt->ballSuccess = (sRt->ballShakesTotal == 4);
    if (sRt->ballShakesTotal > 3)
        sRt->ballShakesTotal = 3;
    sRt->ballShakesDone = 0;
    if (sRt->enemy.statusSpriteId != MAX_SPRITES) gSprites[sRt->enemy.statusSpriteId].invisible = TRUE;
    if (sRt->enemy.confusionSpriteId != MAX_SPRITES) gSprites[sRt->enemy.confusionSpriteId].invisible = TRUE;
    sRt->ballState = 0;
    sRt->timer = 0;
    RtClearProjectiles();
    PlaySE(SE_BALL_THROW);
    sRt->state = RT_STATE_BALL;
}

static void RtDestroyBall(void)
{
    if (sRt->ballSpriteId != MAX_SPRITES)
    {
        DestroySprite(&gSprites[sRt->ballSpriteId]);
        sRt->ballSpriteId = MAX_SPRITES;
    }
    FreeSpriteTilesByTag(gBallSpriteSheets[sRt->ballId].tag);
    RtReleasePal(sRt->ballPalSlot);
    sRt->ballPalSlot = 0xFF;
}

static void RtCatchSuccess(void)
{
    struct Pokemon *mon = &gEnemyParty[0];
    u16 nationalNum = SpeciesToNationalPokedexNum(sRt->enemy.species);
    u8 ballId = sRt->ballItem;
    u8 result;

    SetMonData(mon, MON_DATA_POKEBALL, &ballId);
    GetSetPokedexFlag(nationalNum, FLAG_SET_SEEN);
    GetSetPokedexFlag(nationalNum, FLAG_SET_CAUGHT);
    IncrementGameStat(GAME_STAT_POKEMON_CAPTURES);

    RtGetNickname(mon, gStringVar1);
    RtQueueMessage(sText_Gotcha);
    result = GiveMonToPlayer(mon);
    if (result == MON_GIVEN_TO_PC)
        RtQueueMessage(sText_SentToPc);
    else if (result == MON_CANT_GIVE)
        RtQueueMessage(sText_NoRoom);
    sRt->outcome = RT_OUTCOME_CAUGHT;
    gBattleOutcome = B_OUTCOME_CAUGHT;
    gBattleResults.caughtMonSpecies = sRt->enemy.species;
    gBattleResults.caughtMonBall = ballId;
    { u8 name[POKEMON_NAME_LENGTH + 1]; GetMonData(mon, MON_DATA_NICKNAME, name); CpuCopy16(name, gBattleResults.caughtMonNick, POKEMON_NAME_LENGTH); }
}

static void RtUpdateBall(void)
{
    struct Sprite *ball = &gSprites[sRt->ballSpriteId];
    s32 t;

    switch (sRt->ballState)
    {
    case 0: // flying in an arc
        t = ++sRt->timer;
        sRt->ballX = sRt->ballStartX + (sRt->ballEndX - sRt->ballStartX) * t / 24;
        sRt->ballY = sRt->ballStartY + (sRt->ballEndY - sRt->ballStartY) * t / 24;
        ball->x = FROM_Q8(sRt->ballX);
        ball->y = FROM_Q8(sRt->ballY) - (t * (24 - t)) / 6;
        if (t >= 24)
        {
            PlaySE(SE_BALL_OPEN);
            RtSpawnFx(FX_SPARKLE, FROM_Q8(sRt->enemy.x), FROM_Q8(sRt->enemy.y), 0, FALSE);
            gSprites[sRt->enemy.spriteId].invisible = TRUE;
            sRt->ballState++;
            sRt->timer = 0;
        }
        break;
    case 1: // drop to the ground
        ball->y += 1;
        if (++sRt->timer >= 8)
        {
            PlaySE(SE_BALL_BOUNCE_1);
            sRt->ballState++;
            sRt->timer = 0;
        }
        break;
    case 2: // shakes
        sRt->timer++;
        if (sRt->timer < 20)
            break;
        if (sRt->ballShakesDone < sRt->ballShakesTotal)
        {
            t = sRt->timer - 20;
            ball->x2 = (t < 4) ? -2 : (t < 8) ? 2 : (t < 12) ? -1 : 0;
            if (t == 0)
                PlaySE(SE_BALL);
            if (t >= 30)
            {
                sRt->ballShakesDone++;
                sRt->timer = 0;
            }
        }
        else
        {
            sRt->ballState = sRt->ballSuccess ? 3 : 4;
            sRt->timer = 0;
        }
        break;
    case 3: // caught
        if (sRt->timer++ == 0)
        {
            PlaySE(SE_BALL_CLICK);
            ball->x2 = 0;
        }
        if (sRt->timer == 30)
        {
            PlayFanfare(MUS_CAUGHT);
            RtCatchSuccess();
            RtStartMessages(RT_STATE_END);
        }
        break;
    case 4: // broke free
        PlaySE(SE_BALL_OPEN);
        RtDestroyBall();
        gSprites[sRt->enemy.spriteId].invisible = FALSE;
        RtSpawnFx(FX_SPARKLE, FROM_Q8(sRt->enemy.x), FROM_Q8(sRt->enemy.y), 0, FALSE);
        RtShowTransient(sText_BrokeFree, 60);
        if (sRt->enemy.cooldown < 30) sRt->enemy.cooldown = 30;
        if (sRt->player.cooldown < 10) sRt->player.cooldown = 10;
        sRt->state = sRt->pickerReturnState;
        sRt->timer = sRt->pickerReturnTimer;
        break;
    }
}

// ---------------------------------------------------------------------------
// Flow
// ---------------------------------------------------------------------------

static s8 RtFindNextUsableMon(void)
{
    u8 i;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (RtIsUsableMon(&gPlayerParty[i]))
            return i;
    }
    return -1;
}

static void RtSummonPlayerMon(u8 partyIndex, s32 x, s32 y)
{
    struct RtFighter *f = &sRt->player;

    RtInitFighter(f, &gPlayerParty[partyIndex], FALSE, x, y, partyIndex);
    f->dir = RtDirFromVector(sRt->enemy.x - f->x, sRt->enemy.y - f->y);
    f->lastHorizDir = (sRt->enemy.x < f->x) ? RT_DIR_LEFT : RT_DIR_RIGHT;
    sRt->participantsMask |= 1 << partyIndex;
    sRt->expParticipantsMask |= 1 << partyIndex;
    RtBeginPresentation(f,RTP_SEND);
    PlaySE(SE_BALL_OPEN);
    PlayCry_Normal(f->species, 0);
    RtGetNickname(f->mon, gStringVar1);
    RtShowTransientExpanded(sText_Go, 60);
    sRt->dirtyPlayer = TRUE;
    sRt->dirtyMoves = TRUE;
}

static void RtUpdateFighters(void)
{
    RtApplyEntryAbilities(&sRt->player);
    RtApplyEntryAbilities(&sRt->enemy);
    if (sRt->player.active)
        RtUpdatePlayer();
    if (sRt->enemy.active)
    {
        sRt->trainerItemTimer=RtDec(sRt->trainerItemTimer,sRt->frameStep);
        if(!RtUseTrainerItem()) RtUpdateEnemy();
    }
    RtUpdateProjectiles();
    RtUpdateField();
    RtUpdateWeather();
    RtUpdateArena();
    RtSeparateFighters();
    RtSyncSprite(&sRt->player);
    RtSyncSprite(&sRt->enemy);

}

// Dashes, knockback and Substitute can still leave the two bodies overlapping;
// ease them apart by one pixel a frame wherever there is room.
static void RtSeparateFighters(void)
{
    struct RtFighter *a = &sRt->player, *b = &sRt->enemy;
    s32 ax, ay, bx, by, sx, sy;
    if (!a->active || !b->active || !a->hp || !b->hp) return;
    ax = FROM_Q8(a->x); ay = FROM_Q8(a->y); bx = FROM_Q8(b->x); by = FROM_Q8(b->y);
    if (!RtBodiesOverlap(ax, ay, bx, by)) return;
    sx = bx > ax ? 1 : bx < ax ? -1 : (gMain.vblankCounter2 & 1) ? 1 : -1;
    sy = by > ay ? 1 : by < ay ? -1 : 0;
    // Push along whichever axis has less overlap left to clear.
    if (RT_BODY_SEP_X - RtAbs(bx - ax) <= RT_BODY_SEP_Y - RtAbs(by - ay) || !sy) sy = 0; else sx = 0;
    if (RtIsSpotWalkable(bx + sx, by + sy) && (sRt->arena || RtWildStepClear(bx, by, bx + sx, by + sy))) { b->x += Q8(sx); b->y += Q8(sy); }
    else if (RtIsSpotWalkable(ax - sx, ay - sy) && (sRt->arena || RtWildStepClear(ax, ay, ax - sx, ay - sy))) { a->x -= Q8(sx); a->y -= Q8(sy); }
}

// The spawn planner can fail in cramped spots; never put both Pokemon (or the
// player's Pokemon and the player) on the same tile.
static void RtFixSpawns(s32 *px, s32 *py, s32 *ex, s32 *ey)
{
    s16 r, i, j;
    s32 x, y;
    if (*px == sRt->originX && *py == sRt->originY)
    {
        for (r = 1; r <= 4; r++)
            for (i = -r; i <= r; i++)
                for (j = -r; j <= r; j++)
                {
                    if (RtAbs(i) != r && RtAbs(j) != r) continue;
                    x = sRt->originX + i * 16; y = sRt->originY + j * 16;
                    if (RtIsSpotWalkable(x, y)) { *px = x; *py = y; goto playerPlaced; }
                }
    }
playerPlaced:
    if (RtLength(*ex - *px, *ey - *py) >= 32 && !(*ex == sRt->originX && *ey == sRt->originY)) return;
    for (r = 2; r <= 5; r++)
        for (i = -r; i <= r; i++)
            for (j = -r; j <= r; j++)
            {
                if (RtAbs(i) != r && RtAbs(j) != r) continue;
                x = *px + i * 16; y = *py + j * 16;
                if (x == sRt->originX && y == sRt->originY) continue;
                if (RtIsSpotWalkable(x, y)) { *ex = x; *ey = y; return; }
            }
}

static void RtEndBattleMusic(void)
{
    Overworld_PlaySpecialMapMusic();
}

#include "realtime_trainers.inc.c"
#include "realtime_arena.inc.c"
#include "realtime_presentation.inc.c"

#include "realtime_spacing.inc.c"

static void RtSetupBattle(void)
{
    s32 ex = 0, ey = 0, px = 0, py = 0;
    s8 lead;

    RtReadMapOrigin();

    RtCacheFieldCollision();
    if(RtPlanWildSpawns())
    {
        px=sRt->originX+sRt->wildPlayerDx*16;py=sRt->originY+sRt->wildPlayerDy*16;
        ex=sRt->originX+sRt->wildEnemyDx*16;ey=sRt->originY+sRt->wildEnemyDy*16;
    }
    else
    {
        // No artificial arena or wall bypass: retain the native encounter origin.
        px=ex=sRt->originX;py=ey=sRt->originY;
    }
    RtFixSpawns(&px,&py,&ex,&ey);
    // The player's own tile was open only so spawn planning could start from it;
    // from here on the trainer standing there blocks Pokemon like any NPC.
    if(sRt->collisionReady && !(px==sRt->originX && py==sRt->originY))
    {
        s16 hx=sRt->tileX-sRt->collisionLeft,hy=sRt->tileY-sRt->collisionTop;
        if(hx>=0 && hx<32 && hy>=0 && hy<20) sRt->collision[hy*32+hx]=TRUE;
    }
    if(!sRt->arena){BeginBattleFieldCamera();sRt->cameraActive=TRUE;}
    if (sRt->arena)
    {
        RtSetupArena(); ex=352; ey=160; px=128; py=160;
        sRt->originX=32; sRt->originY=176;
    }
    if (!sRt->arena) RtScanPalSlots(sRt->palKey);
    RtInitStatusGraphics();
    { struct SpriteSheet sheet={sRtFieldTiles,sizeof(sRtFieldTiles),RT_FIELD_TAG}; LoadSpriteSheet(&sheet); sRt->fieldTiles=GetSpriteTileStartByTag(RT_FIELD_TAG); }
    { struct SpriteSheet sheet={sRtWeatherTiles,sizeof(sRtWeatherTiles),0x77B6}; LoadSpriteSheet(&sheet); sRt->weatherTiles=GetSpriteTileStartByTag(0x77B6); }
    sRt->enemySpawnX=ex; sRt->enemySpawnY=ey;
    RtInitFighter(&sRt->enemy,&gEnemyParty[0],TRUE,ex,ey,0);
    sRt->enemy.lastHorizDir=(ex<sRt->originX)?RT_DIR_RIGHT:RT_DIR_LEFT;
    sRt->enemy.dir=sRt->enemy.lastHorizDir; sRt->enemy.cooldown=30;
    sRt->player.x=Q8(px); sRt->player.y=Q8(py);

    lead = RtFindNextUsableMon();
    sRt->player.partyIndex = (lead < 0) ? 0 : lead;
    // The trainer may throw before the summon/countdown has finished.
    // Cache details for the paused panel without creating a second icon.
    sRt->player.mon = &gPlayerParty[sRt->player.partyIndex];
    sRt->player.species = GetMonData(sRt->player.mon, MON_DATA_SPECIES);
    sRt->player.level = GetMonData(sRt->player.mon, MON_DATA_LEVEL);
    sRt->player.hp = sRt->player.shownHp = GetMonData(sRt->player.mon, MON_DATA_HP);
    sRt->player.maxHp = GetMonData(sRt->player.mon, MON_DATA_MAX_HP);

    GetSetPokedexFlag(SpeciesToNationalPokedexNum(sRt->enemy.species), FLAG_SET_SEEN);
    if (!sRt->isTrainer)
    {
        IncrementGameStat(GAME_STAT_TOTAL_BATTLES);
        IncrementGameStat(GAME_STAT_WILD_BATTLES);
    }

    RtCreateHud();
    PlayNewMapMusic(sRt->legendarySpecies ? (sRt->legendarySpecies==SPECIES_MEWTWO?MUS_VS_MEWTWO:sRt->legendarySpecies==SPECIES_DEOXYS?MUS_VS_DEOXYS:MUS_VS_LEGEND) : sRt->isTrainer ? (RtTrainerNeedsArena(gTrainerBattleOpponent_A) ? MUS_VS_GYM_LEADER : MUS_VS_TRAINER) : MUS_VS_WILD);
    PlayCry_Normal(sRt->enemy.species, 0);
    RtBeginPresentation(&sRt->enemy,sRt->isTrainer?RTP_SEND:RTP_WILD);
    if (sRt->isTrainer) RtGetTrainerName(gStringVar1);
    else RtGetNickname(sRt->enemy.mon, gStringVar1);
    RtShowTransientExpanded(sRt->isTrainer ? sText_TrainerIntro : sText_WildAppeared, 0xFFFF);
}

static void RtShowCountdownNumber(u8 number)
{
    ConvertIntToDecimalStringN(gStringVar1, number, STR_CONV_MODE_LEFT_ALIGN, 1);
    StringExpandPlaceholders(gStringVar4, sText_Ready);
    RtShowMessageNow(gStringVar4);
    sRt->transientTimer = 0;
    PlaySE(SE_SELECT);
}

static void RtResumeFight(void)
{
    RtHideMessage();
    RtStopPresentation(TRUE);
    RtSyncSprite(&sRt->player);
    RtSyncSprite(&sRt->enemy);
    sRt->state = RT_STATE_FIGHT;
}

static void RtStartFight(void)
{
    RtResumeFight();
    PlaySE(SE_BALL_TRAY_EXIT);
}

static bool8 RtUpdateMessages(void)
{
    if (sRt->msgCount == 0)
        return TRUE;
    if (sRt->msgTimer == 0)
    {
        RtShowMessageNow(sRt->msgQueue[sRt->msgHead]);
        sRt->transientTimer = 0;
    }
    sRt->msgTimer += sRt->frameStep;
    if ((sRt->msgTimer > 4 && (gMain.newKeysRaw & (A_BUTTON | B_BUTTON))) || sRt->msgTimer > max(50,StringLength(sRt->msgQueue[sRt->msgHead])*2))
    {
        sRt->msgHead = (sRt->msgHead + 1) % RT_MSG_MAX;
        sRt->msgCount--;
        sRt->msgTimer = 0;
        if (sRt->msgCount == 0)
        {
            RtHideMessage();
            return TRUE;
        }
    }
    return FALSE;
}

static u16 RtGetEvolutionTarget(u8 *partyIndex)
{
    u8 i;
    u16 target;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (!(sRt->leveledUpMask & (1 << i)))
            continue;
        if (GetMonData(&gPlayerParty[i], MON_DATA_HP) == 0)
            continue;
        target = GetEvolutionTargetSpecies(&gPlayerParty[i], EVO_MODE_NORMAL, ITEM_NONE);
        if (target != SPECIES_NONE)
        {
            *partyIndex = i;
            return target;
        }
    }
    return SPECIES_NONE;
}

static bool8 RtTryFlee(void)
{
    if (!(gMain.newKeysRaw & START_BUTTON)) return FALSE;
    if (sRt->isTrainer) { RtShowTransient(sText_CantRunTrainer, 45); return TRUE; }
    if (sRt->player.noEscapeTimer) { RtShowTransient(sText_SwitchBlocked,45); return TRUE; }
    if (sRt->state == RT_STATE_PICKER) RtClosePicker();
    RtStopPresentation(TRUE);
    PlaySE(SE_FLEE);
    RtClearProjectiles();
    RtCancelCast(&sRt->player); RtCancelCast(&sRt->enemy);
    RtSyncSprite(&sRt->player); RtSyncSprite(&sRt->enemy);
    RtQueueMessage(sText_GotAway);
    sRt->outcome = RT_OUTCOME_RAN;
    gBattleOutcome = B_OUTCOME_RAN;
    RtStartMessages(RT_STATE_END);
    return TRUE;
}

static void Task_RealtimeBattle(u8 taskId)
{
    u16 target;
    u8 partyIndex;
    s8 next;
    s32 x, y;
    u32 elapsed = gMain.vblankCounter2 - sRt->lastVblank;
    u16 prevTimer;

    // Timers use the hardware frame clock, so displayed seconds remain
    // accurate even if an overworld update misses a video frame.
    sRt->lastVblank = gMain.vblankCounter2;
    sRt->frameStep = elapsed == 0 ? 1 : elapsed > 60 ? 60 : elapsed;
#ifdef PMD_TEST
    gRtPerf[elapsed > 7 ? 7 : elapsed]++;
#endif

    switch (sRt->state)
    {
    case RT_STATE_WAIT_POPUP:
        // The map name popup scrolls BG0; wait until it is gone.
        if (IsMapNamePopupTaskActive())
        {
            DismissMapNamePopup();
            break;
        }
        RtBeginSpacing();
        break;
    case RT_STATE_CLEAR_SPACE:
        RtUpdateSpacing();
        break;
    case RT_STATE_SETUP:
        RtSetupBattle();
        sRt->timer = 0;
        sRt->state = RT_STATE_INTRO;
        break;
    case RT_STATE_INTRO:
        if (RtTryFlee()) break;
        RtUpdatePresentation();
        if (gMain.newKeysRaw & SELECT_BUTTON) { RtOpenPicker(); break; }
        RtUpdateFx();
        RtSyncSprite(&sRt->enemy);
        if (++sRt->timer == 50)
        {
            RtSummonPlayerMon(sRt->player.partyIndex, FROM_Q8(sRt->player.x), FROM_Q8(sRt->player.y));
            RtShowPlayerHud();
        }
        if (sRt->timer >= 50)
            RtSyncSprite(&sRt->player);
        if (sRt->timer >= 100 && !sRt->presentation.active)
        {
            sRt->delaySeconds = GetRealtimeBattleStartDelaySeconds();
            sRt->countdown = sRt->delaySeconds * 60;
            sRt->shownNumber = 0;
            if (sRt->countdown == 0)
                RtStartFight();
            else
                sRt->state = RT_STATE_COUNTDOWN;
        }
        RtUpdateHud();
        break;
    case RT_STATE_COUNTDOWN:
        if (RtTryFlee()) break;
        RtUpdatePresentation();
        if (gMain.newKeysRaw & SELECT_BUTTON) { RtOpenPicker(); break; }
        RtUpdateFx();
        RtSyncSprite(&sRt->player);
        RtSyncSprite(&sRt->enemy);
        if (sRt->countdown == 0)
        {
            RtStartFight();
        }
        else
        {
            u8 number = (sRt->countdown + 59) / 60;
            if (number != sRt->shownNumber)
            {
                sRt->shownNumber = number;
                RtShowCountdownNumber(number);
            }
            sRt->countdown = sRt->countdown > sRt->frameStep ? sRt->countdown - sRt->frameStep : 0;
        }
        RtUpdateHud();
        break;
    case RT_STATE_FIGHT:
        if (RtTryFlee()) break;
        if (gMain.newKeysRaw & SELECT_BUTTON)
        {
            RtOpenPicker();
            break;
        }
        sRt->fightFrames += sRt->frameStep;
        RtUpdateFighters();
        RtUpdateFx();
        RtUpdateHud();
        if (sRt->player.hp == 0)
        {
            RtClearProjectiles();
            sRt->timer = 0;
            sRt->state = RT_STATE_PLAYER_FAINT;
        }
        else if (sRt->enemy.hp == 0)
        {
            RtClearProjectiles();
            sRt->timer = 0;
            sRt->state = RT_STATE_ENEMY_FAINT;
        }
        break;
    case RT_STATE_PICKER:
        if (RtTryFlee()) break;
        RtUpdatePicker();
        break;
    case RT_STATE_LEARN_MOVE:
        RtUpdateLearning();
        break;
    case RT_STATE_BALL:
        RtUpdateBall();
        RtUpdateFx();
        RtSyncSprite(&sRt->player);
        RtUpdateHud();
        break;
    case RT_STATE_MANUAL_SWITCH:
        RtUpdateSwitch();
        break;
    case RT_STATE_PRESENTATION:
        RtUpdatePresentation(); RtSyncSprite(&sRt->player); RtSyncSprite(&sRt->enemy); RtUpdateFx();
        if(!sRt->presentation.active)
        {
            if(sRt->afterPresentation==RT_STATE_FIGHT) RtResumeFight();
            else sRt->state=sRt->afterPresentation;
        }
        break;
    case RT_STATE_ENEMY_FAINT:
        RtUpdatePresentation();
        RtUpdateFx();
        RtUpdateHud();
        prevTimer = sRt->timer;
        sRt->timer += sRt->frameStep;
        // Real time, not update count: a slow frame must not stretch the faint.
        if (prevTimer == 0)
        {
            sRt->enemy.hurtTimer = 0;
            sRt->enemy.windup = 0;
            sRt->enemy.lungeTimer = 0;
            sRt->player.lungeTimer = 0;
            RtSyncSprite(&sRt->enemy);
            RtSyncSprite(&sRt->player);
            RtCancelCast(&sRt->enemy); RtCancelCast(&sRt->player);
            if(sRt->isTrainer) RtBeginPresentation(&sRt->enemy,RTP_RECALL);
            PlayCry_Normal(sRt->enemy.species, -25);
        }
        if (prevTimer < 22 && sRt->timer >= 22)
        {
            RtStopPresentation(FALSE);
            RtSpawnFx(FX_SMOKE, FROM_Q8(sRt->enemy.x), FROM_Q8(sRt->enemy.y), 0, FALSE);
            PlaySE(SE_FAINT);
            RtRemoveFighterSprite(&sRt->enemy);
        }
        if (prevTimer < 34 && sRt->timer >= 34)
        {
            if (!sRt->isTrainer || RtNextEnemy() < 0)
                PlayNewMapMusic(sRt->isTrainer ? MUS_VICTORY_TRAINER : MUS_VICTORY_WILD);
            RtGetNickname(sRt->enemy.mon, gStringVar1);
            RtQueueMessage(sRt->isTrainer ? sText_Fainted : sText_WildFainted);
            gBattleResults.opponentFaintCounter++;
            gBattleResults.lastOpponentSpecies = sRt->enemy.species;
            RtAwardExperience();
            if (sRt->isTrainer && RtNextEnemy() >= 0) RtStartMessages(RT_STATE_ENEMY_SWITCH);
            else
            {
                if (sRt->isTrainer) RtTrainerReward();
                sRt->outcome = RT_OUTCOME_WON;
                gBattleOutcome = B_OUTCOME_WON;
                RtStartMessages(RT_STATE_END);
            }
        }
        break;
    case RT_STATE_ENEMY_SWITCH:
        RtSwitchEnemy();
        break;
    case RT_STATE_PLAYER_FAINT:
        RtUpdatePresentation();
        RtUpdateFx();
        RtUpdateHud();
        prevTimer = sRt->timer;
        sRt->timer += sRt->frameStep;
        // Real time, not update count: a slow frame must not stretch the faint.
        if (prevTimer == 0)
        {
            sRt->player.hurtTimer = 0;
            sRt->player.lungeTimer = 0;
            sRt->enemy.windup = 0;
            sRt->enemy.lungeTimer = 0;
            RtSyncSprite(&sRt->player);
            RtSyncSprite(&sRt->enemy);
            RtCancelCast(&sRt->enemy); RtCancelCast(&sRt->player);
            RtBeginPresentation(&sRt->player,RTP_RECALL);
            PlayCry_Normal(sRt->player.species, -25);
        }
        if (prevTimer < 22 && sRt->timer >= 22)
        {
            RtStopPresentation(FALSE);
            RtSpawnFx(FX_SMOKE, FROM_Q8(sRt->player.x), FROM_Q8(sRt->player.y), 0, FALSE);
            PlaySE(SE_FAINT);
            RtRemoveFighterSprite(&sRt->player);
        }
        if (prevTimer < 34 && sRt->timer >= 34)
        {
            gBattleResults.playerFaintCounter++;
            RtGetNickname(&gPlayerParty[sRt->player.partyIndex], gStringVar1);
            RtQueueMessage(sText_Fainted);
            if (RtFindNextUsableMon() >= 0)
            {
                RtStartMessages(RT_STATE_SWITCH);
            }
            else
            {
                StringCopy(gStringVar1, gSaveBlock2Ptr->playerName);
                RtQueueMessage(sText_OutOfMons);
                RtQueueMessage(sText_WhitedOut);
                sRt->outcome = RT_OUTCOME_LOST;
                gBattleOutcome = B_OUTCOME_LOST;
                RtStartMessages(RT_STATE_WHITEOUT);
            }
        }
        break;
    case RT_STATE_SWITCH:
        x = FROM_Q8(sRt->player.x);
        y = FROM_Q8(sRt->player.y);
        next = RtFindNextUsableMon();
        RtSummonPlayerMon(next, x, y);
        sRt->player.cooldown = 30;
        if (sRt->enemy.cooldown < 45) sRt->enemy.cooldown = 45;
        sRt->afterPresentation=RT_STATE_FIGHT;
        sRt->state = RT_STATE_PRESENTATION;
        break;
    case RT_STATE_MESSAGES:
        RtUpdateFx();
        RtUpdateHud();
        if (RtUpdateMessages())
        {
            if(RtStartLearning()) break;
            sRt->state = sRt->afterMsgState;
            sRt->timer = 0;
        }
        break;
    case RT_STATE_TRAINER_SPEECH:
        if (IsFieldMessageBoxHidden() && (gMain.newKeysRaw & (A_BUTTON | B_BUTTON)))
        {
            HideFieldMessageBox();
            sRt->state = sRt->afterSpeechState;
            sRt->timer = 0;
        }
        break;
    case RT_STATE_END:
        if(!sRt->playerReturned && sRt->player.active && sRt->player.hp) { sRt->playerReturned=TRUE; RtCancelCast(&sRt->player); RtBeginPresentation(&sRt->player,RTP_RECALL); sRt->afterPresentation=RT_STATE_END; sRt->state=RT_STATE_PRESENTATION; break; }
        if (sRt->isTrainer && RtBeginFinalTrainerSpeech(RT_STATE_END)) break;
        // Wait for fanfares to finish before going back to the map music.
        if (!IsFanfareTaskInactive())
            break;
        target = RtGetEvolutionTarget(&partyIndex);
        if (target != SPECIES_NONE)
        {
            sRt->state = RT_STATE_EVOLVE;
            sRt->timer = 0;
            BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
            break;
        }
        if (sRt->isTrainer)
        {
            if (!sRt->timer) { BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK); sRt->timer = 1; }
            if (!gPaletteFade.active) RtFinishTrainer(taskId, B_OUTCOME_WON);
            break;
        }
        if (sRtLegendEnd)
        {
            if (!sRt->timer) { BeginNormalPaletteFade(PALETTES_ALL,0,0,16,RGB_BLACK); sRt->timer=1; }
            if (!gPaletteFade.active) RtFinishLegend(taskId);
            break;
        }
        RtCleanup();
        RtEndBattleMusic();
        UnfreezeObjectEvents();
        UnlockPlayerFieldControls();
        DestroyTask(taskId);
        break;
    case RT_STATE_WHITEOUT:
        if (sRt->isTrainer && RtBeginFinalTrainerSpeech(RT_STATE_WHITEOUT)) break;
        if (sRt->timer++ == 0)
            BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        if (gPaletteFade.active)
            break;
        if (sRt->isTrainer)
        {
            if (!(GetTrainerBattleMode() == TRAINER_BATTLE_EARLY_RIVAL && (GetRivalBattleFlags() & RIVAL_BATTLE_HEAL_AFTER)))
                RemoveMoney(&gSaveBlock1Ptr->money, ComputeWhiteOutMoneyLoss());
            RtFinishTrainer(taskId, B_OUTCOME_LOST);
            break;
        }
        if (sRtLegendEnd) { RtFinishLegend(taskId); break; }
        RtCleanup();
        DestroyTask(taskId);
        gMain.state = 0;
        SetMainCallback2(CB2_WhiteOut);
        break;
    case RT_STATE_EVOLVE:
        if (gPaletteFade.active)
            break;
        target = RtGetEvolutionTarget(&partyIndex);
        RtCleanup();
        DestroyTask(taskId);
        CleanupOverworldWindowsAndTilemaps();
        if (sRtTrainerEnd) { gBattleOutcome = B_OUTCOME_WON; gCB2_AfterEvolution = RtTrainerAfterEvolution; }
        else if (sRtLegendEnd) { gCB2_AfterEvolution = RtLegendAfterEvolution; }
        else { gCB2_AfterEvolution = CB2_ReturnToField; gFieldCallback = FieldCB_SafariZoneRanOutOfBalls; }
        BeginEvolutionScene(&gPlayerParty[partyIndex], target, TRUE, partyIndex);
        break;
    }
}

static void RtCleanup(void)
{
    u8 i;

    if(sRt->cameraActive){EndBattleFieldCamera();sRt->cameraActive=FALSE;}
    RtRestoreSpacing();
    RtFreeArena();
    RtCleanupEngine();
    RtClearProjectiles();
    for (i = 0; i < RT_MAX_FX; i++)
        RtDestroyFx(i);
    if (sRt->ballSpriteId != MAX_SPRITES)
        RtDestroyBall();
    RtRemoveFighterSprite(&sRt->player);
    RtRemoveFighterSprite(&sRt->enemy);
    RtReleasePal(sRt->statusPalSlot);
    RtRestoreBorrowedPals();
    RtDestroyHud();
    if (sRt->statusTileStart != 0xFFFF) FreeSpriteTilesByTag(RT_STATUS_TAG);
    FREE_AND_SET_NULL(sRt);
    gRealtimeBattleActive = FALSE;
}

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

u8 GetRealtimeBattleStartDelaySeconds(void)
{
    return RT_DELAY_FROM_SAVE(gSaveBlock2Ptr->optionsBattleStartDelay) + 1;
}

u8 GetRealtimeBattleAI(void)
{
    return RT_AI_FROM_SAVE(gSaveBlock2Ptr->optionsBattleAI);
}

bool8 TryStartRealtimeWildBattle(void)
{
    if (gQuestLogState == QL_STATE_PLAYBACK) return FALSE;
    if (TestPlayerAvatarFlags(PLAYER_AVATAR_FLAG_SURFING | PLAYER_AVATAR_FLAG_UNDERWATER)) return FALSE;
    if (!IsStandardWildBattle()) return FALSE;
    if (!RtStartBattleCommon(FALSE)) return FALSE;
    gBattleTypeFlags = 0;
    memset(&gBattleResults, 0, sizeof(gBattleResults));
    return TRUE;
}

static bool8 RtStartBattleCommon(bool8 trainer)
{
    u8 i;
    if (gRealtimeBattleActive || RtFindNextUsableMon() < 0 || RtScanPalSlots(NULL) < RT_MIN_PAL_SLOTS) return FALSE;
    sRtTrainerEnd = NULL;
    sRtLegendEnd = NULL;
    sRt = AllocZeroed(sizeof(*sRt));
    if (sRt == NULL)
        return FALSE;
    sRt->isTrainer = trainer;
    for (i=0;i<2;i++) { sRt->arenaPortrait[i]=MAX_SPRITES; sRt->arenaPortraitPal[i]=0xFF; }
    if (!trainer)
    {
        RtReadMapOrigin();
        if (!RtIsSpotWalkable(sRt->originX,sRt->originY))
        {
            // The original caller starts a normal battle when RT cannot fit.
            // No field controls, tasks or graphics have been acquired yet.
            FREE_AND_SET_NULL(sRt);
            return FALSE;
        }
    }
    HideMonFollower();
    RtScanPalSlots(sRt->palKey);
    sRt->ballPalSlot = 0xFF;
    sRt->statusPalSlot = 0xFF;
    sRt->statusTileStart = 0xFFFF;
    sRt->fieldTiles=0xFFFF;
    sRt->forcedEnemy=0xFF;
    sRt->weatherTiles=0xFFFF;
    for(i=0;i<12;i++) sRt->weatherSprites[i]=MAX_SPRITES;
    sRt->player.glowSprite=sRt->enemy.glowSprite=MAX_SPRITES;
    for(i=0;i<8;i++) sRt->player.cast.fx[i]=sRt->enemy.cast.fx[i]=0xFF;
    sRt->player.statusSpriteId = sRt->player.confusionSpriteId = MAX_SPRITES;
    sRt->enemy.statusSpriteId = sRt->enemy.confusionSpriteId = MAX_SPRITES;
    for (i = 0; i < RT_WIN_COUNT; i++)
        sRt->winIds[i] = WINDOW_NONE;
    sRt->player.spriteId = MAX_SPRITES;
    sRt->enemy.spriteId = MAX_SPRITES;
    sRt->ballSpriteId = MAX_SPRITES;
    sRt->state = RT_STATE_WAIT_POPUP;
    sRt->lastVblank = gMain.vblankCounter2;
    sRt->frameStep = 1;
    sRt->aiDifficulty = GetRealtimeBattleAI();
    gRealtimeBattleActive = TRUE;

    LockPlayerFieldControls();
    FreezeObjectEvents();
    StopPlayerAvatar();
    CreateTask(Task_RealtimeBattle, 80);
    return TRUE;
}

bool8 IsRealtimeBattlePaused(void)
{
    return sRt != NULL && sRt->state == RT_STATE_PICKER;
}

// Trainer setup reuses the original party builder and original completion
// callback, so trainer flags, post-battle scripts, badges and rematches keep
// their normal event flow. AI difficulty is shared with wild battles.
bool8 TryStartRealtimeTrainerBattle(void)
{
    if (gRealtimeBattleActive || gTrainerBattleOpponent_A == TRAINER_SECRET_BASE) return FALSE;
    if (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_TRAINER_TOWER | BATTLE_TYPE_BATTLE_TOWER | BATTLE_TYPE_EREADER_TRAINER)) return FALSE;
    HideMonFollower();
    if (RtScanPalSlots(NULL) < RT_MIN_PAL_SLOTS || RtFindNextUsableMon() < 0) return FALSE;
    CreateRealtimeTrainerParty();
    if (!gEnemyPartyCount) return FALSE;
    // Wild start performs the shared resource/field-control setup. Trainer
    // eligibility is checked here, so ghosts and surf do not bypass trainer RT.
    if (!RtStartBattleCommon(TRUE)) return FALSE;
    sRt->isTrainer = TRUE;
    sRt->arena = RT_ENABLE_ARENAS && RtTrainerNeedsArena(gTrainerBattleOpponent_A);
    sRtTrainerEnd = gMain.savedCallback;
    memset(&gBattleResults, 0, sizeof(gBattleResults));
    gBattleOutcome = 0;
    HideFieldMessageBox();
    return TRUE;
}

bool8 TryStartRealtimeLegendaryBattle(void)
{
    u16 species=GetMonData(&gEnemyParty[0],MON_DATA_SPECIES);
    if (!RtLegendTheme(species) || gQuestLogState==QL_STATE_PLAYBACK) return FALSE;
    // Use the same bounded native field camera for scripted encounters.
    if (!RtStartBattleCommon(TRUE)) return FALSE;
    sRt->isTrainer=FALSE; sRt->arena=RT_ENABLE_ARENAS; sRt->legendarySpecies=species;
    sRtLegendEnd=gMain.savedCallback;
    memset(&gBattleResults,0,sizeof(gBattleResults)); gBattleOutcome=0;
    HideFieldMessageBox(); return TRUE;
}

bool8 IsRealtimeArenaBattle(void)
{
    return sRt != NULL && sRt->arenaActive;
}

#ifdef RT_REGRESSION
#include "../tests/realtime_regression.inc.c"
#endif


#ifdef RT_OVERWORLD_TEST
#include "../tests/realtime_overworld_test.inc.c"
#endif
