/*
 * radiotextosd.h
 *
 *  Created on: 27.05.2018
 *      Author: uli
 */

#ifndef __RADIOTEXTOSD_H_
#define __RADIOTEXTOSD_H_

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
    eKeys LastKey;
    cTimeMs osdtimer;
    void rtp_print(void);
    bool rtclosed;
    bool rassclosed;
    static cBitmap rds, arec, rass, radio;
    static cBitmap index, marker, page1, pages2, pages3, pages4, pageE;
    static cBitmap no0, no1, no2, no3, no4, no5, no6, no7, no8, no9, bok;
public:
    cRadioTextOsd();
    ~cRadioTextOsd();
    virtual void Hide(void);
    virtual void Show(void);
    void ShowText(void);
    void RTOsdClose(void);
    int RassImage(int QArchiv, int QKey, bool DirUp);
    void RassOsd(void);
    void RassOsdTip(void);
    void RassOsdClose(void);
    void RassImgSave(const char *size, int pos);
    virtual eOSState ProcessKey(eKeys Key);
    virtual bool IsMenu(void) const { return false; }
};



#endif /* RADIOTEXTOSD_H_ */
