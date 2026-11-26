#ifndef GUARD_GOD_MODE_H
#define GUARD_GOD_MODE_H
#ifndef PLAYTEST_TOOLS
#define PLAYTEST_TOOLS 0
#endif
#if PLAYTEST_TOOLS
void CB2_GodModeMenu(void);
bool8 IsGodModeMenuActive(void);
bool8 GodModeHmUnlocked(void);
bool8 GodModeIsHm(u16 move);
void GodModeCheckHmAccess(void);
#else
#define IsGodModeMenuActive() FALSE
#define GodModeHmUnlocked() FALSE
#define GodModeIsHm(move) FALSE
#endif
#endif
