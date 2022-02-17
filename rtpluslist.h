/*
 * rtpluslist.h - part of radio.c, a plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __RTPLUSLIST_H
#define __RTPLUSLIST_H

#include <vdr/osdbase.h>

class cRTplusList : public cOsdMenu, public cCharSetConv {
private:
    int typ;
    bool refresh;
public:
    cRTplusList(int Typ = 0);
    ~cRTplusList();
    virtual void Load(void);
    virtual void Update(void);
    virtual eOSState ProcessKey(eKeys Key);
};

#endif //__RTPLUSLIST_H
