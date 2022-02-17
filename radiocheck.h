/*
 * radiocheck.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __RADIOCHECK_H
#define __RADIOCHECK_H

#include <vdr/tools.h>

class cRadioCheck: public cThread {
private:
    static cRadioCheck *RadioCheck;
protected:
    virtual void Action(void);
    void Stop(void);
public:
    cRadioCheck(void);
    virtual ~cRadioCheck();
    static void Init(void);
    static void Exit(void);
};

#endif // __RADIOCHECK_H
