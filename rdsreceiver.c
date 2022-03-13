/*
 * rdsreceiver.c - part of radio.c, a plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "rdsreceiver.h"
#include "radioaudio.h"
#include "radiotools.h"
#include "radiotmc.h"

extern bool RdsLogo;
extern cRadioAudio *RadioAudio;

// --- cRDSReceiver ------------------------------------------------------------

cRDSReceiver::cRDSReceiver(int Pid) {
    dsyslog("radio: additional RDS-Receiver starts on Pid=%d", Pid);

    pid = Pid;
    rt_start = false;
    SetPids(NULL);
    AddPid(pid);
}

cRDSReceiver::~cRDSReceiver() {
    dsyslog("radio: additional RDS-Receiver stopped");
}

#if VDRVERSNUM >= 20300
void cRDSReceiver::Receive(const uchar *Data, int Length)
#else
void cRDSReceiver::Receive(uchar *Data, int Length)
#endif
{
    // check TS-Size, -Sync, PID, Payload
    if (Length != TS_SIZE || Data[0] != 0x47 || pid != ((Data[1] & 0x1f) << 8) + Data[2] || !(Data[3] & 0x10))
        return;

    int offset;
    if (Data[1] & 0x40) {                      // 1.TS-Frame, payload-unit-start
        offset = (Data[3] & 0x20) ? Data[4] + 11 : 10; // Header + ADFL + 6 byte: PES-Startcode, -StreamID, -PacketLength
        offset += 3;                     // 3 byte: Extension + Headerlength
        offset += Data[offset - 1];
    } else {
        offset = (Data[3] & 0x20) ? Data[4] + 5 : 4;    // Header + ADFL
    }

    if ((TS_SIZE - offset) <= 0) {
        return;
    }

    uchar rdsData[TS_SIZE];
    int rdsLen = TS_SIZE - offset;
    const uchar *s = Data + TS_SIZE - 1;
    uchar *d = rdsData;

    // reverse RDS-data for historical reasons
    for (int i = rdsLen; i > 0; i--) {
        *d++ = *s--;
    }

    if ((S_Verbose & 0x0f) >= 3)
        hexdump(rdsData, rdsLen);

    RadioAudio->RadiotextParseRDS(rdsData, rdsLen);
}
