/*
 * rtpluslist.h
 *
 *  Created on: 27.05.2018
 *      Author: uli
 */

#ifndef __RTPLUSLIST_H_
#define __RTPLUSLIST_H_

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

#endif /* RTPLUSLIST_H_ */
