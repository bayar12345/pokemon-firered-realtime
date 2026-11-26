#include "global.h"
#include "party_exp_share.h"
STATIC_ASSERT(offsetof(struct SaveBlock2, optionsPartyExpShare)==0x17,PartyShareUsesPadding);
STATIC_ASSERT(offsetof(struct SaveBlock2, pokedex)==0x18,PartySharePreservesPokedex);
STATIC_ASSERT(sizeof(struct SaveBlock2)==0xF24,PartySharePreservesSaveSize);
bool8 IsPartyExpShareEnabled(void) {return gSaveBlock2Ptr->optionsPartyExpShare==0xA5;}
void SetPartyExpShareEnabled(bool8 enabled) {gSaveBlock2Ptr->optionsPartyExpShare=enabled?0xA5:0;}
