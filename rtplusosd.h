/*
 * rtplusosd.h - part of radio.c, a plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __RTPLUSOSD_H
#define __RTPLUSOSD_H

#include <vdr/osdbase.h>

class cRTplusOsd : public cOsdMenu, public cCharSetConv {
private:
    int bcount;
    int helpmode;
    const char *listtyp[8];
    const char *btext[8];
    int rtptyp(const char *btext);
    void rtp_fileprint(void);
public:
    cRTplusOsd(void);
    virtual ~cRTplusOsd();
    virtual void Load(void);
    virtual void Update(void);
    virtual eOSState ProcessKey(eKeys Key);
};

#endif //__RTPLUSOSD_H
