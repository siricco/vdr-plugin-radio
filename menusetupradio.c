/*
 * menusetupradio.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "radioaudio.h"
#include "menusetupradio.h"

extern int S_Activate;
extern int S_ExtInfo;
extern int S_HMEntry;

// --- cMenuSetupRadio -------------------------------------------------------

cMenuSetupRadio::cMenuSetupRadio(void)
{
    T_StillPic[0] = tr("Use PlayPes-Function");
    T_StillPic[1] = tr("Use StillPicture-Function");
    T_StillPic[2] = tr("Off");
    T_RtFunc[0] = tr("Off");
    T_RtFunc[1] = tr("only Text");
    T_RtFunc[2] = tr("Text+TagInfo");
    T_RtOsdTags[0] = tr("Off");
    T_RtOsdTags[1] = tr("only, if some");
    T_RtOsdTags[2] = tr("always");
    T_RtOsdPos[0] = tr("Top");
    T_RtOsdPos[1] = tr("Bottom");
    T_RtOsdLoop[0] = tr("latest at Top");
    T_RtOsdLoop[1] = tr("latest at Bottom");
    T_RtBgColor[0] = T_RtFgColor[0] = tr ("Black");
    T_RtBgColor[1] = T_RtFgColor[1] = tr ("White");
    T_RtBgColor[2] = T_RtFgColor[2] = tr ("Red");
    T_RtBgColor[3] = T_RtFgColor[3] = tr ("Green");
    T_RtBgColor[4] = T_RtFgColor[4] = tr ("Yellow");
    T_RtBgColor[5] = T_RtFgColor[5] = tr ("Magenta");
    T_RtBgColor[6] = T_RtFgColor[6] = tr ("Blue");
    T_RtBgColor[7] = T_RtFgColor[7] = tr ("Cyan");
    T_RtBgColor[8] = T_RtFgColor[8] = tr ("Transparent");
    T_RtDisplay[0] = tr("Off");
    T_RtDisplay[1] = tr("about MainMenu");
    T_RtDisplay[2] = tr("Automatic");
    T_RtMsgItems[0] = tr("Off");
    T_RtMsgItems[1] = tr("only Taginfo");
    T_RtMsgItems[2] = tr("only Text");
    T_RtMsgItems[3] = tr("Text+TagInfo");
    T_RassText[0] = tr("Off");
    T_RassText[1] = tr("Rass only");
    T_RassText[2] = tr("Rass+Text mixed");

    newS_Activate = S_Activate;
    newS_StillPic = S_StillPic;
    newS_Activate = S_Activate;
    newS_RtFunc = S_RtFunc;
    newS_RtOsdTitle = S_RtOsdTitle;
    newS_RtOsdTags = S_RtOsdTags;
    newS_RtOsdPos = S_RtOsdPos;
    newS_RtOsdRows = S_RtOsdRows;
    newS_RtOsdLoop = S_RtOsdLoop;
    newS_RtOsdTO = S_RtOsdTO;
    newS_RtSkinColor = S_RtSkinColor;
    newS_RtBgCol = S_RtBgCol;
    newS_RtBgTra = S_RtBgTra;
    newS_RtFgCol = S_RtFgCol;
    newS_RtDispl = (S_RtDispl > 2 ? 2 : S_RtDispl);
    newS_RtMsgItems = S_RtMsgItems;
    //newS_RtpMemNo = S_RtpMemNo;
    newS_RassText = S_RassText;
    newS_ExtInfo = S_ExtInfo;

    Add(new cMenuEditBoolItem( tr("Activate"),                      &newS_Activate));
    Add(new cMenuEditStraItem( tr("StillPicture"),                  &newS_StillPic, 3, T_StillPic));
    Add(new cMenuEditBoolItem( tr("Hide MainMenuEntry"),            &newS_HMEntry));
    Add(new cMenuEditStraItem( tr("RDSText Function"),              &newS_RtFunc, 3, T_RtFunc));
    Add(new cMenuEditStraItem( tr("RDSText OSD-Position"),          &newS_RtOsdPos, 2, T_RtOsdPos));
    Add(new cMenuEditBoolItem( tr("RDSText OSD-Titlerow"),          &newS_RtOsdTitle));
    Add(new cMenuEditIntItem(  tr("RDSText OSD-Rows (1-5)"),        &newS_RtOsdRows, 1, RT_ROWS));
    Add(new cMenuEditStraItem( tr("RDSText OSD-Scrollmode"),        &newS_RtOsdLoop, 2, T_RtOsdLoop));
    Add(new cMenuEditStraItem( tr("RDSText OSD-Taginfo"),           &newS_RtOsdTags, 3, T_RtOsdTags));
    Add(new cMenuEditBoolItem( tr("RDSText OSD-Skincolors used"),   &newS_RtSkinColor));
    if (newS_RtSkinColor == 0) {
        Add(new cMenuEditStraItem( tr("RDSText OSD-Backgr.Color"),      &newS_RtBgCol, 9, T_RtBgColor));
        Add(new cMenuEditIntItem(  tr("RDSText OSD-Backgr.Transp."),    &newS_RtBgTra, 1, 255));
        Add(new cMenuEditStraItem( tr("RDSText OSD-Foregr.Color"),      &newS_RtFgCol, 8, T_RtFgColor));
        }
    Add(new cMenuEditIntItem(  tr("RDSText OSD-Timeout (0-1440 min)"),  &newS_RtOsdTO, 0, 1440));
    Add(new cMenuEditStraItem( tr("RDSText OSD-Display"),               &newS_RtDispl, 3, T_RtDisplay));
    Add(new cMenuEditStraItem( tr("RDSText StatusMsg (lcdproc & co)"),  &newS_RtMsgItems, 4, T_RtMsgItems));
    //Add(new cMenuEditIntItem(  tr("RDSplus Memorynumber (10-99)"),    &newS_RtpMemNo, 10, 99));
    Add(new cMenuEditStraItem( tr("RDSText Rass-Function"),             &newS_RassText, 3, T_RassText));
    Add(new cMenuEditBoolItem( tr("External Info-Request"),             &newS_ExtInfo));
}

void cMenuSetupRadio::Store(void)
{
    SetupStore("Activate",               S_Activate = newS_Activate);
    SetupStore("UseStillPic",            S_StillPic = newS_StillPic);
    SetupStore("HideMenuEntry",          S_HMEntry = newS_HMEntry);
    SetupStore("RDSText-Function",       S_RtFunc = newS_RtFunc);
    SetupStore("RDSText-OsdTitle",       S_RtOsdTitle = newS_RtOsdTitle);
    SetupStore("RDSText-OsdTags",        S_RtOsdTags = newS_RtOsdTags);
    SetupStore("RDSText-OsdPosition",    S_RtOsdPos = newS_RtOsdPos);
    SetupStore("RDSText-OsdRows",        S_RtOsdRows = newS_RtOsdRows);
    SetupStore("RDSText-OsdLooping",     S_RtOsdLoop = newS_RtOsdLoop);
    SetupStore("RDSText-OsdSkinColor",   S_RtSkinColor = newS_RtSkinColor);
    SetupStore("RDSText-OsdBackgrColor", S_RtBgCol = newS_RtBgCol);
    SetupStore("RDSText-OsdBackgrTrans", S_RtBgTra = newS_RtBgTra);
    SetupStore("RDSText-OsdForegrColor", S_RtFgCol = newS_RtFgCol);
    SetupStore("RDSText-OsdTimeout",     S_RtOsdTO = newS_RtOsdTO);
    SetupStore("RDSText-Display",        S_RtDispl = newS_RtDispl);
    SetupStore("RDSText-MsgItems",       S_RtMsgItems = newS_RtMsgItems);
    //SetupStore("RDSplus-MemNumber",    S_RtpMemNo = newS_RtpMemNo);
    SetupStore("RDSText-Rass",          S_RassText = newS_RassText);
    SetupStore("ExtInfo-Req",           S_ExtInfo = newS_ExtInfo);
}
