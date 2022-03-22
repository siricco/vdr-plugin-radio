/*
 * rdspatpmt.h: PAT section filter
 */

#ifndef __RDSPATPMT_H
#define __RDSPATPMT_H

#include <vdr/filter.h>
#include <vdr/thread.h>

#define MAXRDSPIDS 16

// --- mutex with trylock ---

class cMutexT {
private:
  pthread_mutex_t mutex;
public:
  cMutexT(void) { pthread_mutexattr_t attr;
                  pthread_mutexattr_init(&attr);
                  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
                  pthread_mutex_init(&mutex, &attr);
                }
  ~cMutexT()         { pthread_mutex_destroy(&mutex); }
  void Lock(void)    { pthread_mutex_lock(&mutex); }
  bool TryLock(void) { return !pthread_mutex_trylock(&mutex); }
  void Unlock(void)  { pthread_mutex_unlock(&mutex); }
  };

// ---

class cRdsPidFilter : public cFilter {
private:
  cMutexT mutex;
  cTimeMs timer;
  int patVersion;
  int sid;
  int pid;
  int numRdsPids;
  int rdsPids[MAXRDSPIDS];
  cSectionSyncer sectionSyncer;
  void ResetPat(void);
  void ResetPmt(void);
protected:
  virtual void Process(u_short Pid, u_char Tid, const u_char *Data, int Length);

public:
  cRdsPidFilter(void);
  virtual void SetStatus(bool On);
  void Trigger(int Sid = -1, int Pid = 0); // triggers reading the PMT PID
  };

#endif //__RDSPATPMT_H
