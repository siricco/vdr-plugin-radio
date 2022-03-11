/*
 * radiotextosd.h - part of radio.c, a plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __RADIOTEXTOSD_H
#define __RADIOTEXTOSD_H

#include <vdr/osdbase.h>

class cRadioTextOsd : public cOsdObject, public cCharSetConv {
private:
    cOsd *osd;
    cOsd *qosd;
    cOsd *qiosd;
    const cFont *ftitel;
    const cFont *ftext;
    int fheight;
    int bheight;
    int rtpRows;
    eKeys LastKey;
    cTimeMs osdtimer;
    void rtp_print(void);
    bool rtclosed;
    bool rassclosed;
    static cBitmap rds, arec, rass, radio;
    static cBitmap index, marker, page1, pages2, pages3, pages4, pageE;
    static cBitmap no0, no1, no2, no3, no4, no5, no6, no7, no8, no9, bok;
    bool OsdResize(void);
public:
    cRadioTextOsd();
    ~cRadioTextOsd();
    virtual void Hide(void);
    virtual void Show(void);
    virtual void ShowText(void);
    virtual void RTOsdClose(void);
    int RassImage(int QArchiv, int QKey, bool DirUp);
    virtual void RassOsd(void);
    virtual void RassOsdTip(void);
    virtual void RassOsdClose(void);
    virtual void RassImgSave(const char *size, int pos);
    virtual eOSState ProcessKey(eKeys Key);
    virtual bool IsInteractive(void) { return false; }
};

#endif //__RADIOTEXTOSD_H
