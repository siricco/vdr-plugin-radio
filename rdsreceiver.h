/*
 * rdsreceiver.h
 *
 *  Created on: 27.05.2018
 *      Author: uli
 */

#ifndef __RDSRECEIVER_H_
#define __RDSRECEIVER_H_

#include <vdr/receiver.h>

// RDS-Receiver for seperate Data-Pids
class cRDSReceiver : public cReceiver {
private:
    int pid;
    bool rt_start;
    bool rt_bstuff;
protected:
#if VDRVERSNUM >= 20300
    virtual void Receive(const uchar *Data, int Length);
#else
    virtual void Receive(uchar *Data, int Length);
#endif
public:
    cRDSReceiver(int Pid);
    virtual ~cRDSReceiver(void);
};


#endif /* __RDSRECEIVER_H_ */
