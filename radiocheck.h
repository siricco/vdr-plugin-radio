/*
 * radiocheck.h
 *
 *  Created on: 27.05.2018
 *      Author: uli
 */

#ifndef RADIOCHECK_H_
#define RADIOCHECK_H_

#include <vdr/plugin.h>

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


#endif /* RADIOCHECK_H_ */
