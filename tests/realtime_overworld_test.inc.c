// Isolated native fixture metadata, excluded from release builds.
const u16 gRtWorldTestLayout[]={
 sizeof(struct RtBattle),offsetof(struct RtBattle,enemy),offsetof(struct RtBattle,state),
 offsetof(struct RtBattle,cameraX),offsetof(struct RtBattle,cameraY),offsetof(struct RtBattle,cameraActive),
 offsetof(struct RtBattle,arena),offsetof(struct RtBattle,arenaActive),offsetof(struct RtBattle,winIds),
 offsetof(struct RtBattle,originX),offsetof(struct RtBattle,tileX),offsetof(struct RtBattle,frameStep),
 offsetof(struct RtFighter,x),offsetof(struct RtFighter,y),offsetof(struct RtFighter,hp),offsetof(struct RtFighter,maxHp),
 offsetof(struct RtFighter,spriteId),offsetof(struct RtFighter,active),offsetof(struct RtBattle,collisionReady),
 offsetof(struct RtBattle,enemySpawnX),offsetof(struct RtBattle,worldSprites),offsetof(struct RtBattle,msgCount),
 offsetof(struct RtBattle,msgQueue),offsetof(struct RtBattle,msgVisible),offsetof(struct RtBattle,player)
};
void RealtimeWorldTestTeam(void)
{
 u8 i;
 ZeroPlayerPartyMons();
 CreateMon(&gPlayerParty[0],SPECIES_CHARIZARD,50,31,TRUE,1234,OT_ID_PRESET,0);
 CreateMon(&gPlayerParty[1],SPECIES_BLASTOISE,50,31,TRUE,1235,OT_ID_PRESET,0);
 gPlayerPartyCount=2;
 for(i=0;i<2;i++){SetMonMoveSlot(&gPlayerParty[i],MOVE_THUNDERBOLT,0);SetMonMoveSlot(&gPlayerParty[i],MOVE_FLAMETHROWER,1);SetMonMoveSlot(&gPlayerParty[i],MOVE_SOLAR_BEAM,2);SetMonMoveSlot(&gPlayerParty[i],MOVE_DOUBLE_EDGE,3);}
 CreateMon(&gEnemyParty[0],SPECIES_PIDGEY,5,31,TRUE,2345,OT_ID_PRESET,0);gEnemyPartyCount=1;
}
