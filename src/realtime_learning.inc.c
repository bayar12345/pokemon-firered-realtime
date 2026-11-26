// Level-up choices are resolved while battle clocks are paused, before the next
// opponent or evolution. Preserve HMs and clear PP Ups only on a confirmed swap.
static const u8 sText_LearnTitle[] = _("LEARN A NEW MOVE");
static const u8 sText_LearnWhich[] = _("Choose a move to forget:");
static const u8 sText_LearnSkip[] = _("Do not learn this move");
static const u8 sText_LearnControls[] = _("UP/DOWN: choose  A: select\nB: decline");
static const u8 sText_LearnConfirm[] = _("A: confirm replacement  B: back");
static const u8 sText_LearnDecline[] = _("A: skip this move  B: back");
static const u8 sText_LearnHm[] = _("HM moves cannot be forgotten here.");
static const u8 sText_DidNotLearn[] = _("{STR_VAR_1} did not learn {STR_VAR_2}.");

static void RtDrawLearning(void)
{
    struct Pokemon *mon=&gPlayerParty[sRt->learnParty[sRt->learnIndex]];
    u16 move=sRt->learnMoves[sRt->learnIndex];
    u8 i;
    RtDrawPanel(RT_WIN_MENU,C_PANEL_DARK);
    RtMenuPrint(4,2,sText_LearnTitle,sTextAccent);
    RtGetNickname(mon,gStringVar1);
    RtMenuPrint(4,17,gStringVar1,sTextWhite);
    RtMenuPrint(112,17,gMoveNames[move],sTextWhite);
    RtMenuPrint(4,32,sText_LearnWhich,sTextGray);
    for(i=0;i<5;i++)
    {
        if(i==sRt->learnCursor) RtMenuPrint(4,48+i*11,sText_PickerCursor,sTextAccent);
        RtMenuPrint(16,48+i*11,i==4?sText_LearnSkip:gMoveNames[GetMonData(mon,MON_DATA_MOVE1+i)],sTextWhite);
    }
    RtMenuPrint(4,108,sRt->learnConfirm?(sRt->learnCursor==4?sText_LearnDecline:sText_LearnConfirm):sText_LearnControls,sTextWhite);
    PutWindowTilemap(sRt->winIds[RT_WIN_MENU]);
    CopyWindowToVram(sRt->winIds[RT_WIN_MENU],COPYWIN_FULL);
}

static bool8 RtStartLearning(void)
{
    if(sRt->learnIndex>=sRt->learnCount) { sRt->learnIndex=sRt->learnCount=0; return FALSE; }
    RtOpenPicker();
    if(sRt->winIds[RT_WIN_MENU]==WINDOW_NONE) return FALSE;
    sRt->learnCursor=0; sRt->learnConfirm=FALSE;
    sRt->state=RT_STATE_LEARN_MOVE;
    RtDrawLearning();
    return TRUE;
}

static void RtUpdateLearning(void)
{
    struct Pokemon *mon=&gPlayerParty[sRt->learnParty[sRt->learnIndex]];
    u16 move=sRt->learnMoves[sRt->learnIndex], keys=gMain.newKeysRaw;
    u8 i=sRt->learnCursor;
    if(keys&B_BUTTON)
    {
        if(sRt->learnConfirm) sRt->learnConfirm=FALSE;
        else { sRt->learnCursor=4; sRt->learnConfirm=TRUE; }
        RtDrawLearning(); return;
    }
    if(!sRt->learnConfirm && keys&(DPAD_UP|DPAD_DOWN))
    {
        sRt->learnCursor=(i+(keys&DPAD_UP?4:1))%5;
        RtDrawLearning(); return;
    }
    if(!(keys&A_BUTTON)) return;
    if(i<4 && IsHMMove2(GetMonData(mon,MON_DATA_MOVE1+i)))
    {
        FillWindowPixelRect(sRt->winIds[RT_WIN_MENU],PIXEL_FILL(C_PANEL_LIGHT),4,108,216,28);
        RtMenuPrint(4,108,sText_LearnHm,sTextRed);
        CopyWindowToVram(sRt->winIds[RT_WIN_MENU],COPYWIN_FULL); return;
    }
    if(!sRt->learnConfirm) { sRt->learnConfirm=TRUE; RtDrawLearning(); return; }
    if(i<4)
    {
        RemoveMonPPBonus(mon,i);
        SetMonMoveSlot(mon,move,i);
        sRt->cacheMon[0]=sRt->cacheMon[1]=NULL;
        if(sRt->learnParty[sRt->learnIndex]==sRt->player.partyIndex)sRt->player.moveCooldown[i]=sRt->player.moveCooldownMax[i]=0;
        sRt->switchMoveCd[sRt->learnParty[sRt->learnIndex]][i]=0;
    }
    RtClosePicker();
    RtGetNickname(mon,gStringVar1); StringCopy(gStringVar2,gMoveNames[move]);
    RtQueueMessage(i<4?sText_Learned:sText_DidNotLearn);
    sRt->learnIndex++;
    sRt->dirtyMoves=TRUE;
    RtStartMessages(sRt->afterMsgState);
}
