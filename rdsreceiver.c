#include <vdr/remote.h>
#include <vdr/status.h>
#include <vdr/plugin.h>
#include "radioaudio.h"
#include "radioskin.h"
#include "radiotools.h"
#include "service.h"
#include <math.h>
#include "rdsreceiver.h"

extern bool RdsLogo;
extern cRadioAudio *RadioAudio;

// --- cRDSReceiver ------------------------------------------------------------

cRDSReceiver::cRDSReceiver(int Pid) {
    dsyslog("radio: additional RDS-Receiver starts on Pid=%d", Pid);

    pid = Pid;
    rt_start = rt_bstuff = false;
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
    const int mframel = 263; // max. 255(MSG)+4(ADD/SQC/MFL)+2(CRC)+2(Start/Stop) of RDS-data
    static unsigned char mtext[mframel + 1];
    static int index;
    static int mec = 0;

    // check TS-Size, -Sync, PID, Payload
    if (Length != TS_SIZE || Data[0] != 0x47
            || pid != ((Data[1] & 0x1f) << 8) + Data[2] || !(Data[3] & 0x10)) {
        return;
    }

    int offset;
    if (Data[1] & 0x40) {                               // 1.TS-Frame, payload-unit-start
        offset = (Data[3] & 0x20) ? Data[4] + 11 : 10;  // Header + ADFL + 6 byte: PES-Startcode, -StreamID, -PacketLength
        if (Data[offset - 3] == 0xbd) {                 // StreamID = Private stream 1 (for rds)
            offset += 3;                                // 3 byte: Extension + Headerlength
            offset += Data[offset - 1];
        } else {
            return;
        }
    } else {
        offset = (Data[3] & 0x20) ? Data[4] + 5 : 4;    // Header + ADFL
    }

    if ((TS_SIZE - offset) <= 0) {
        return;
    }
    // print TS-RawData with RDS
    if ((S_Verbose & 0x02) == 0x02) {
        printf("\n\nTS-Data(%d):\n", Length);
        int cnt = 0;
        for (int a = 0; a < Length; a++) {
            printf("%02x ", Data[a]);
            cnt++;
            if (cnt > 15) {
                cnt = 0;
                printf("\n");
            }
        }
        printf("(End)\n");
    }

    for (int i = 0, val = 0; i < (TS_SIZE - offset); i++) {
        val = Data[offset + i];

        if (val == 0xfe) {  // Start
            index = -1;
            rt_start = true;
            rt_bstuff = false;
            mec = 0;
            if ((S_Verbose & 0x0f) >= 2) {
                printf("\nrdsreceiver: RDS-Start: ");
            }
        }

        if (rt_start) {
            if ((S_Verbose & 0x0f) >= 2) {
                printf("%02x ", val);
            }
            // byte-stuffing reverse: 0xfd00->0xfd, 0xfd01->0xfe, 0xfd02->0xff
            if (rt_bstuff) {
                switch (val) {
                case 0x00:
                    mtext[index] = 0xfd;
                    break;
                case 0x01:
                    mtext[index] = 0xfe;
                    break;
                case 0x02:
                    mtext[index] = 0xff;
                    break;
                default:
                    mtext[++index] = val;   // should never be
                }
                rt_bstuff = false;
                if ((S_Verbose & 0x0f) >= 2) {
                    printf("(Bytestuffing -> %02x) ", mtext[index]);
                }
            } else {
                mtext[++index] = val;
            }
            if (val == 0xfd && index > 0) {  // stuffing found
                rt_bstuff = true;
            }
            // early check for used MEC
            if (index == 5) {
                //mec = val;
                switch (val) {
                case 0x0a:              // RT
                case 0x46:              // ODA-Data
                case 0x07:              // PTY
                case 0x3e:              // PTYN
                case 0x30:              // TMC
                case 0x02:
                    mec = val;  // PS
                    RdsLogo = true;
                    break;
                default:
                    rt_start = false;
                    if ((S_Verbose & 0x0f) >= 2) {
                        printf("[RDS-MEC '%02x' not used -> End]\n", val);
                    }
                }
            }
            if (index >= mframel) { // max. rdslength, garbage ?
                rt_start = false;
                if ((S_Verbose & 0x0f) >= 2) {
                    printf("rdsreceiver: (RDS-Error: too long %d, garbage ?)\n", index);
                }
            }
        }

        if (rt_start && (val == 0xff)) {  // End
            rt_start = false;
            if ((S_Verbose & 0x0f) >= 2) {
                printf("(RDS-End)\n");
            }
            if (index < 9) {        // min. rdslength, garbage ?
                if ((S_Verbose & 0x0f) >= 1) {
                    printf("rdsreceiver: RDS-Error: too short -> garbage ?\n");
                }
            } else {
                // crc16-check
                unsigned short crc16 = crc16_ccitt(mtext, index - 3, true);
                if (crc16 != (mtext[index - 2] << 8) + mtext[index - 1]) {
                    if ((S_Verbose & 0x0f) >= 1) {
                        printf(
                                "rdsreceiver: RDS-Error: wrong CRC # calc = %04x <> transmit = %02x%02x\n",
                                crc16, mtext[index - 2], mtext[index - 1]);
                    }
                } else {
                    switch (mec) {
                    case 0x0a:
                        RadioAudio->RadiotextDecode(mtext);  // Radiotext
                        break;
                    case 0x46:
                        switch ((mtext[7] << 8) + mtext[8]) {          // ODA-ID
                        case 0x4bd7:
                            RadioAudio->RadiotextDecode(mtext);  // RT+
                            break;
                        default:
                            if ((S_Verbose & 0x0f) >= 2) {
                                printf(
                                        "[RDS-ODA AID '%02x%02x' not used -> End]\n",
                                        mtext[7], mtext[8]);
                            }
                        }
                        break;
                    case 0x07:
                        RT_PTY = mtext[8];                                // PTY
                        if ((S_Verbose & 0x0f) >= 1) {
                            printf("RDS-PTY set to '%s'\n",
                                    ptynr2string(RT_PTY));
                        }
                        break;
                    case 0x3e:
                        RadioAudio->RDS_PsPtynDecode(true, mtext, index); // PTYN
                        break;
                    case 0x02:
                        RadioAudio->RDS_PsPtynDecode(false, mtext, index); // PS
                        break;
                    }
                }
            }
        }
    }
}
