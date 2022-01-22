/*
 * rdspatpmt.h: PAT section filter
 */

#ifndef __RDSPATPMT_H
#define __RDSPATPMT_H

#include <vdr/filter.h>
#include <vdr/thread.h>

#define MAXRDSPIDS 16

class cRdsPidFilter : public cFilter {
private:
  cMutex mutex;
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
