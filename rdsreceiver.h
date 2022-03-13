/*
 * rdsreceiver.h - part of radio.c, a plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __RDS_RECEIVER_H
#define __RDS_RECEIVER_H

#include <vdr/receiver.h>

// RDS-Receiver for seperate Data-Pids
class cRDSReceiver : public cReceiver {
private:
    int pid;
    bool rt_start;
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

#endif //__RDS_RECEIVER_H
