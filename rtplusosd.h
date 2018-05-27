/*
 * rtplusosd.h
 *
 *  Created on: 27.05.2018
 *      Author: uli
 */

#ifndef __RTPLUSOSD_H_
#define __RTPLUSOSD_H_

#include <vdr/osdbase.h>

class cRTplusOsd : public cOsdMenu, public cCharSetConv {
private:
    int bcount;
    int helpmode;
    const char *listtyp[7];
    char *btext[7];
    int rtptyp(char *btext);
    void rtp_fileprint(void);
public:
    cRTplusOsd(void);
    virtual ~cRTplusOsd();
    virtual void Load(void);
    virtual void Update(void);
    virtual eOSState ProcessKey(eKeys Key);
};


#endif /* RTPLUSOSD_H_ */
