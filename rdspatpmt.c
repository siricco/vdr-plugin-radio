/*
 * rdspatpmt.c: PAT section filter
 */

#include <libsi/descriptor.h>
#include "rdspatpmt.h"
#include "radioaudio.h"

#define PMT_SCAN_TIMEOUT  2000 // ms

extern cRadioAudio *RadioAudio;

// --- cRdsPidFilter ------------------------------------------------------------

//#define DEBUG_RDS_PATPMT
#ifdef DEBUG_RDS_PATPMT
#define DBGLOG(a...) { cString s = cString::sprintf(a); fprintf(stderr, "%s\n", *s); dsyslog("%s", *s); }
#else
#define DBGLOG(a...) void()
#endif

cRdsPidFilter::cRdsPidFilter(void)
{
  ResetPat();
  ResetPmt();
  Set(0x00, 0x00);  // PAT
}

void cRdsPidFilter::ResetPat(void)
{
  patVersion = -1;
  sectionSyncer.Reset();
}

void cRdsPidFilter::ResetPmt(void)
{
  sid = -1;
  pid = 0;
  numRdsPids = 0;
}

void cRdsPidFilter::SetStatus(bool On)
{
  mutex.Lock();
  if (On) { // restart PAT or PMT Pid
     if (pid)
        timer.Set(PMT_SCAN_TIMEOUT);
     }
  DBGLOG("RdsPAT filter set status %d, patVersion %d, sid %d, pmtpid %d", On, patVersion, sid, pid);
  cFilter::SetStatus(On);
  mutex.Unlock();
}

void cRdsPidFilter::Trigger(int Sid, int Pid)
{
  mutex.Lock();
  DBGLOG("RdsPAT filter trigger SID %d (%d) PID %d (%d)", Sid, sid, Pid, pid);

  SetStatus(false);
  if (Sid < 0) {
     ResetPat();
     ResetPmt();
     }
  else {
     ResetPmt();
     if (Pid > 0)
        Add(Pid, SI::TableIdPMT);
     sid = Sid;
     pid = Pid;
     SetStatus(true);
     }
  mutex.Unlock();
}

void cRdsPidFilter::Process(u_short Pid, u_char Tid, const u_char *Data, int Length)
{
  if (!mutex.TryLock()) {
     DBGLOG("RdsPAT: Process() *** RdsPidFilter busy ***");
     return;
     }
  DBGLOG("RdsPAT: Process(Sid %d) Pid %d Tid %d Len %d", sid, Pid, Tid, Length);

  if (Pid == 0x00 && Tid == SI::TableIdPAT) {
     SI::PAT pat(Data, false);
     if (!pat.CheckCRCAndParse())
        goto done;
#if VDRVERSNUM >= 20502
     if (sectionSyncer.Check(pat.getVersionNumber(), pat.getSectionNumber()))
#else
     if (sectionSyncer.Sync(pat.getVersionNumber(), pat.getSectionNumber(), pat.getLastSectionNumber()))
#endif
        {
        DBGLOG("RdsPAT: %d %d -> %d %d/%d", Transponder(), patVersion, pat.getVersionNumber(), pat.getSectionNumber(), pat.getLastSectionNumber());
        if (pat.getVersionNumber() != patVersion) {
           if (patVersion >= 0 && pid > 0)
              Trigger(sid); // restart PMT filter
           patVersion = pat.getVersionNumber();
           }

        SI::PAT::Association assoc;
        for (SI::Loop::Iterator it; pat.associationLoop.getNext(assoc, it); ) {
            if (!assoc.isNITPid()) {
               if (assoc.getServiceId() == sid) {
                  int Pmt = assoc.getPid();
                  DBGLOG("RdsPAT: PmtPid %d sid %d", Pmt, sid);
                  Trigger(sid, Pmt); // start PMT Filter
                  break;
                  }
               }
            }
        }
     }
  else if (pid == Pid && Tid == SI::TableIdPMT) {
     SI::PMT pmt(Data, false);
     if (!pmt.CheckCRCAndParse())
        goto done;

     if (pmt.getServiceId() == sid) {
        // Scan the stream-specific loop:
        SI::PMT::Stream stream;
        for (SI::Loop::Iterator it; pmt.streamLoop.getNext(stream, it); ) {
            int esPid = stream.getPid();
            ///// RDS /////
            SI::AncillaryDataDescriptor *ad;
            for (SI::Loop::Iterator it; (ad = (SI::AncillaryDataDescriptor *)stream.streamDescriptors.getNext(it, SI::AncillaryDataDescriptorTag)); ) {
                if (ad->getAncillaryDataIdentifier() & 0x40) { // RDS via UECP
                   DBGLOG(">>>> %s : RDS via UECP (type: 0x%X pid: %d/0x%X)", Channel()->Name(), stream.getStreamType(), esPid, esPid);
                   if (numRdsPids < MAXRDSPIDS)
                      rdsPids[numRdsPids++] = esPid;
                   }
                delete ad;
                }
            }
        RadioAudio->HandleRdsPids(rdsPids, numRdsPids);
        Trigger(); // stop PAt+PMT Filter
        }
     }
  if (timer.TimedOut()) {
     DBGLOG("RdsPMT timeout Pid %d", pid);
     Trigger(); // stop PAt+PMT Filter
     }
done:
  mutex.Unlock();
}
