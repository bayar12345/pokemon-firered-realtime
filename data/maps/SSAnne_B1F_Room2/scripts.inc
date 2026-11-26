SSAnne_B1F_Room2_MapScripts::
	.byte 0

SSAnne_B1F_Room2_EventScript_Huey::
RtScaledBattle_0220::
	trainerbattle_single TRAINER_SAILOR_HUEY, SSAnne_B1F_Room2_Text_HueyIntro, SSAnne_B1F_Room2_Text_HueyDefeat
	msgbox SSAnne_B1F_Room2_Text_HueyPostBattle, MSGBOX_AUTOCLOSE
	end
