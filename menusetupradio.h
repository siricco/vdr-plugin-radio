/*
 * menusetupradio.h
 *
 *  Created on: 27.05.2018
 *      Author: uli
 */

#ifndef MENUSETUPRADIO_H_
#define MENUSETUPRADIO_H_

#include <vdr/plugin.h>

class cMenuSetupRadio : public cMenuSetupPage {
private:
    int newS_Activate;
    int newS_StillPic;
    int newS_HMEntry;
    int newS_RtFunc;
    int newS_RtOsdTitle;
    int newS_RtOsdTags;
    int newS_RtOsdPos;
    int newS_RtOsdRows;
    int newS_RtOsdLoop;
    int newS_RtOsdTO;
    int newS_RtSkinColor;
    int newS_RtBgCol;
    int newS_RtBgTra;
    int newS_RtFgCol;
    int newS_RtDispl;
    int newS_RtMsgItems;
    //int newS_RtpMemNo;
    int newS_RassText;
    int newS_ExtInfo;
    const char *T_StillPic[3];
    const char *T_RtFunc[3];
    const char *T_RtOsdTags[3];
    const char *T_RtOsdPos[2];
    const char *T_RtOsdLoop[2];
    const char *T_RtBgColor[9];
    const char *T_RtFgColor[9];
    const char *T_RtDisplay[3];
    const char *T_RtMsgItems[4];
    const char *T_RassText[3];
protected:
    virtual void Store(void);
public:
    cMenuSetupRadio(void);
};

#endif /* MENUSETUPRADIO_H_ */
