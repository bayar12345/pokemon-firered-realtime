// Internal real-time move data. Time values are 60-Hz video frames.
enum RtDelivery {
    RT_CLOSE, RT_DASH, RT_SHOT, RT_BEAM, RT_CONE, RT_AREA,
    RT_POINT, RT_AIR, RT_SELF, RT_FIELD, RT_BLINK, RT_CALL, RT_PARTY
};
enum RtMoveFlags {
    RTMF_ALWAYS = 1, RTMF_MULTI = 2, RTMF_ROOT = 4, RTMF_LIFT = 8,
    RTMF_CHARGE = 16, RTMF_SELF = 32, RTMF_DISABLED = 64, RTMF_HOMING = 128
};
struct RtMoveDef { u8 kind, reach, radius, fx, flags; u16 startup, active; };

struct RtPtdAnim {
    const u32 *pixels;
    const u16 *palette;
    u16 frameBytes;
    u8 frames, fps, width, height, columns, holdStart, holdEnd;
    u16 paletteKey;
    u8 extent;
};
struct RtPtdMoveArt { u16 shot, impact, cast, beam, beamArena; };
struct RtPtdStatusArt { u16 anim; u8 recipient, clock; };
enum RtClock {
    RTC_ROOT, RTC_SLOW, RTC_FLINCH, RTC_COUNTER, RTC_COAT, RTC_BLOCK,
    RTC_PROTECT, RTC_ENDURE, RTC_GRUDGE, RTC_DESTINY, RTC_PERISH,
    RTC_CURSE, RTC_SEED, RTC_NIGHTMARE, RTC_WISH, RTC_INGRAIN,
    RTC_MUDSPORT, RTC_WATERSPORT, RTC_REFLECT, RTC_LIGHTSCREEN,
    RTC_SAFEGUARD, RTC_MIST, RTC_TAUNT, RTC_ENCORE, RTC_DISABLE,
    RTC_TORMENT, RTC_ATTRACT, RTC_SNATCH, RTC_MAGICCOAT, RTC_IMPRISON,
    RTC_BIDE, RTC_CHARGE, RTC_FOCUS, RTC_RAMPAGE, RTC_TRAP, RTC_LOCKON, RTC_FORESIGHT, RTC_SLEEP_GUARD, RTC_COUNT
};
struct RtCast {
    u16 move, elapsed, nextHit, end, fixedDamage, power;
    s32 originX, originY, dx, dy, targetX, targetY;
    u8 kind, hits, hitCount, phase;
    bool8 active, hit, released, landed, accuracyChecked, accuracyPassed, coverHit;
    u8 fx[8];
    u8 beamSprites[8],beamPal,beamFront;
    bool8 beamLoaded;
    u8 beamMatrix, beamFrame;
    u16 beamAnim, beamPixels;
    s16 beamMuzzleX, beamMuzzleY;
};
enum { RTF_SUB, RTF_WEB, RTF_SPIKES, RTF_FIRE, RTF_WATER, RTF_SAND };
struct RtField {
    bool8 active, enemy;
    u8 kind, spriteId, fxId;
    s16 x, y;
    u16 hp, life, hitTimer;
};
enum { RTP_NONE, RTP_WILD, RTP_SEND, RTP_RECALL };
struct RtPresentation {
    bool8 active, enemy;
    u8 kind, ballSprite, ballPal, ballId, cloneSprite, matrix, effectSprite;
    u16 time, tiles, effectTiles;
    s16 originX, originY;
    u32 iconTiles[128];
};
enum { RTW_NONE, RTW_RAIN, RTW_SUN, RTW_SAND, RTW_HAIL };

// Arena positions use the same world-pixel units as moves; the view renders at 1/2 scale.
enum { RATHEME_GENERIC, RATHEME_ROCK, RATHEME_WATER, RATHEME_ELECTRIC, RATHEME_GRASS,
       RATHEME_POISON, RATHEME_PSYCHIC, RATHEME_FIRE, RATHEME_GROUND, RATHEME_ICE,
       RATHEME_FIGHTING, RATHEME_GHOST, RATHEME_DRAGON, RATHEME_CHAMPION, RATHEME_COUNT };
enum { RAO_ROCK, RAO_TREE, RAO_COLUMN, RAO_ROD, RAO_WATER, RAO_GRASS, RAO_POISON,
       RAO_PSYCHIC, RAO_FIRE, RAO_SAND, RAO_ICE, RAO_SHADOW, RAO_WIND, RAO_ARC };
struct RtArenaObject { s16 x,y; u8 kind,hp,maxHp; };
