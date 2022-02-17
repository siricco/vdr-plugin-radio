/*
 * radioaudio.c - part of radio.c, a plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vdr/remote.h>
#include <vdr/status.h>
#include <vdr/plugin.h>
#include "radioaudio.h"
#include "radioskin.h"
#include "radiotools.h"
#include "service.h"
#include <math.h>

// OSD-Symbols
#include "symbols/rds.xpm"
#include "symbols/arec.xpm"
#include "symbols/rass.xpm"
#include "symbols/radio.xpm"
#include "symbols/index.xpm"
#include "symbols/marker.xpm"
#include "symbols/page1.xpm"
#include "symbols/pages2.xpm"
#include "symbols/pages3.xpm"
#include "symbols/pages4.xpm"
#include "symbols/no0.xpm"
#include "symbols/no1.xpm"
#include "symbols/no2.xpm"
#include "symbols/no3.xpm"
#include "symbols/no4.xpm"
#include "symbols/no5.xpm"
#include "symbols/no6.xpm"
#include "symbols/no7.xpm"
#include "symbols/no8.xpm"
#include "symbols/no9.xpm"
#include "symbols/bok.xpm"
#include "symbols/pageE.xpm"

// Radiotext
int RTP_ItemToggle = 1;
bool RT_MsgShow = false, RT_PlusShow = false, RTP_TToggle = false;
bool RT_Replay = false;
char RT_Titel[80], RTp_Titel[80];
rtp_classes rtp_content;
// RDS rest
bool RDS_PSShow = false;
int RDS_PSIndex = 0;
char RDS_PSText[12][9];
char RDS_PTYN[9];
bool RdsLogo = false;
// plugin audiorecorder service
bool ARec_Receive = false, ARec_Record = false;
// Rass ...
int Rass_Show = -1;         // -1=No, 0=Yes, 1=display
int Rass_Archiv = -1;       // -1=Off, 0=Index, 1000-9990=Slidenr.
bool Rass_Flags[11][4];     // Slides+Gallery existent
// ... Gallery (1..999)
#define RASS_GALMAX 999
bool Rass_Gallery[RASS_GALMAX + 1];
int Rass_GalStart, Rass_GalEnd, Rass_GalCount, Rass_SlideFoto;

cRadioImage *RadioImage;
cRDSReceiver *RDSReceiver;
cRadioAudio *RadioAudio;
cRadioTextOsd *RadioTextOsd;

// RDS-Chartranslation: 0x80..0xff
unsigned char rds_addchar[128] = {
        0xe1, 0xe0, 0xe9, 0xe8, 0xed, 0xec, 0xf3, 0xf2, 0xfa, 0xf9, 0xd1, 0xc7, 0x8c, 0xdf, 0x8e, 0x8f,
        0xe2, 0xe4, 0xea, 0xeb, 0xee, 0xef, 0xf4, 0xf6, 0xfb, 0xfc, 0xf1, 0xe7, 0x9c, 0x9d, 0x9e, 0x9f,
        0xaa, 0xa1, 0xa9, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xa3, 0xab, 0xac, 0xad, 0xae, 0xaf,
        0xba, 0xb9, 0xb2, 0xb3, 0xb1, 0xa1, 0xb6, 0xb7, 0xb5, 0xbf, 0xf7, 0xb0, 0xbc, 0xbd, 0xbe, 0xa7,
        0xc1, 0xc0, 0xc9, 0xc8, 0xcd, 0xcc, 0xd3, 0xd2, 0xda, 0xd9, 0xca, 0xcb, 0xcc, 0xcd, 0xd0, 0xcf,
        0xc2, 0xc4, 0xca, 0xcb, 0xce, 0xcf, 0xd4, 0xd6, 0xdb, 0xdc, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
        0xc3, 0xc5, 0xc6, 0xe3, 0xe4, 0xdd, 0xd5, 0xd8, 0xde, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xf0,
        0xe3, 0xe5, 0xe6, 0xf3, 0xf4, 0xfd, 0xf5, 0xf8, 0xfe, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff };

// announce text/items for lcdproc & other
void radioStatusMsg(void) {
    cPluginManager::CallAllServices(RADIO_TEXT_UPDATE, 0);
    if (!RT_MsgShow || S_RtMsgItems <= 0)
        return;

    if (S_RtMsgItems >= 2) {
        char temp[100];
        int ind = (RT_Index == 0) ? S_RtOsdRows - 1 : RT_Index - 1;
        strcpy(temp, RT_Text[ind]);
        cStatus::MsgOsdTextItem(rtrim(temp), false);
    }

    if ((S_RtMsgItems == 1 || S_RtMsgItems >= 3) && ((S_RtOsdTags == 1 && RT_PlusShow) || S_RtOsdTags >= 2)) {
        struct tm tm_store;
        struct tm *ts = localtime_r(&RTP_Starttime, &tm_store);
        cStatus::MsgOsdProgramme(mktime(ts), RTP_Title, RTP_Artist, 0, NULL, NULL);
    }
}

// --- cRadioImage -------------------------------------------------------

cRadioImage::cRadioImage(void) : cThread("radioimage") {
    imagepath = 0;
    imageShown = false;
    RadioImage = this;
}

cRadioImage::~cRadioImage() {
    if (Running())
        Stop();
    free(imagepath);
}

void cRadioImage::Init(void) {
    RadioImage->Start();
}

void cRadioImage::Exit(void) {
    if (RadioImage != NULL) {
        RadioImage->Stop();
        DELETENULL(RadioImage);
    }
}

void cRadioImage::Stop(void) {
    Cancel(2);
}

void cRadioImage::Action(void) {
    if ((S_Verbose & 0x0f) >= 2)
        printf("vdr-radio: image-showing starts\n");

    while (Running()) {
        cCondWait::SleepMs(333);
        if ((IsRadioOrReplay == 1) && imagepath && !imageShown) { // only live $20090905
            imageShown = true;
            Show(imagepath);
        }
    }

    if ((S_Verbose & 0x0f) >= 2) {
        printf("vdr-radio: image-showing ends\n");
    }
}

void cRadioImage::Show(const char *file) {
    uchar *buffer;
    int fd;
    struct stat st;
    struct video_still_picture sp;

    if (S_StillPic > 1) // use with xineliboutput -> no stillpicture, but sound
        return;

    if ((fd = open(file, O_RDONLY)) >= 0) {
        fstat(fd, &st);
        sp.iFrame = (char *) malloc(st.st_size);
        if (sp.iFrame) {
            sp.size = st.st_size;
            if (read(fd, sp.iFrame, sp.size) > 0) {
                buffer = (uchar *) sp.iFrame;
                if (S_StillPic > 0)
                    cDevice::PrimaryDevice()->StillPicture(buffer, sp.size);
                else {
                    for (int i = 1; i <= 25; i++)
                        send_pes_packet(buffer, sp.size, i);
                }
            }
            free(sp.iFrame);
        }
        close(fd);
    }
}

void cRadioImage::send_pes_packet(unsigned char *data, int len, int timestamp) {
#define PES_MAX_SIZE 2048
    int ptslen = timestamp ? 5 : 1;
    static unsigned char pes_header[PES_MAX_SIZE];
    pes_header[0] = pes_header[1] = 0;
    pes_header[2] = 1;
    pes_header[3] = 0xe0;

    while (len > 0) {
        int payload_size = len;
        if (6 + ptslen + payload_size > PES_MAX_SIZE) {
            payload_size = PES_MAX_SIZE - (6 + ptslen);
        }
        pes_header[4] = (ptslen + payload_size) >> 8;
        pes_header[5] = (ptslen + payload_size) & 255;
        if (ptslen == 5) {
            int x;
            x = (0x02 << 4) | (((timestamp >> 30) & 0x07) << 1) | 1;
            pes_header[8] = x;
            x = ((((timestamp >> 15) & 0x7fff) << 1) | 1);
            pes_header[7] = x >> 8;
            pes_header[8] = x & 255;
            x = ((((timestamp) & 0x7fff) < 1) | 1);
            pes_header[9] = x >> 8;
            pes_header[10] = x & 255;
        } else {
            pes_header[6] = 0x0f;
        }

        memcpy(&pes_header[6 + ptslen], data, payload_size);
        cDevice::PrimaryDevice()->PlayPes(pes_header, 6 + ptslen + payload_size);
        len -= payload_size;
        data += payload_size;
        ptslen = 1;
    }
}

void cRadioImage::SetBackgroundImage(const char *Image) {
    free(imagepath);
    imagepath = 0;

    if (Image) {
        imageShown = false;
        asprintf(&imagepath, "%s", Image);
    }
}

// --- cRDSReceiver ------------------------------------------------------------

cRDSReceiver::cRDSReceiver(int Pid) {
    dsyslog("radio: additional RDS-Receiver starts on Pid=%d", Pid);

    pid = Pid;
    rt_start = rt_bstuff = false;
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
    const int mframel = 263; // max. 255(MSG)+4(ADD/SQC/MFL)+2(CRC)+2(Start/Stop) of RDS-data
    static unsigned char mtext[mframel + 1];
    static int index;
    static int mec = 0;

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
    // print TS-RawData with RDS
    if ((S_Verbose & 0x0f) >= 3) {
        dsyslog("TS-Data(%d):", Length);
        hexdump(Data, Length);
        dsyslog("(TS-End)");
    }

    for (int i = 0, val = 0; i < (TS_SIZE - offset); i++) {
        val = Data[offset + i];

        if (val == 0xfe) {  // Start
            index = -1;
            rt_start = true;
            rt_bstuff = false;
            mec = 0;
            if ((S_Verbose & 0x0f) >= 2) {
                printf("\nRDS-Start: ");
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
                    printf("(RDS-Error: too long, garbage ?)\n");
                }
            }
        }

        if (rt_start && val == 0xff) {  // End
            rt_start = false;
            if ((S_Verbose & 0x0f) >= 2) {
                printf("(RDS-End)\n");
            }
            if (index < 9) {        // min. rdslength, garbage ?
                if ((S_Verbose & 0x0f) >= 1) {
                    printf("RDS-Error: too short -> garbage ?\n");
                }
            } else {
                if ((S_Verbose & 0x0f) >= 1)
                    { dsyslog("-- RDS [%02X] --", mtext[5]); hexdump(mtext, index+1); }
                // crc16-check
                unsigned short crc16 = crc16_ccitt(mtext, index - 3, true);
                if (crc16 != (mtext[index - 2] << 8) + mtext[index - 1]) {
                    if ((S_Verbose & 0x0f) >= 1) {
                        printf("RDS-Error: wrong CRC # calc = %04x <> transmit = %02x%02x\n", crc16, mtext[index - 2], mtext[index - 1]);
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
                        case 0x0d45:
                        case 0xcd46:
                            if ((S_Verbose & 0x20) > 0) {
                                unsigned char tmc[6];       // TMC Alert-C
                                int i;
                                for (i = 9; i <= (index - 3); i++) {
                                    tmc[i - 9] = mtext[i];
                                }
                                tmc_parser(tmc, i - 8);
                            }
                            break;
                        default:
                            if ((S_Verbose & 0x0f) >= 2) {
                                printf("[RDS-ODA AID '%02x%02x' not used -> End]\n", mtext[7], mtext[8]);
                            }
                        }
                        break;
                    case 0x07:
                        RT_PTY = mtext[8];                                // PTY
                        if ((S_Verbose & 0x0f) >= 1) {
                            printf("RDS-PTY set to '%s'\n", ptynr2string(RT_PTY));
                        }
                        break;
                    case 0x3e:
                        RadioAudio->RDS_PsPtynDecode(true, mtext, index); // PTYN
                        break;
                    case 0x02:
                        RadioAudio->RDS_PsPtynDecode(false, mtext, index); // PS
                        break;
                    case 0x30:
                        if ((S_Verbose & 0x20) > 0) {             // TMC Alert-C
                            unsigned char tmc[6];
                            int i;
                            for (i = 7; i <= (index - 3); i++) {
                                tmc[i - 7] = mtext[i];
                            }
                            tmc_parser(tmc, i - 6);
                        }
                        break;
                    }
                }
            }
        }
    }
}

// --- cRadioAudio -------------------------------------------------------------

cRadioAudio::cRadioAudio() :
        cAudio(), enabled(false), first_packets(0), audiopid(0),
        rdsdevice(NULL), bitrate(NULL), rdsSeen(false), maxRdsChunkIndex(RDS_CHUNKSIZE - 1) {
    RadioAudio = this;
    dsyslog("radio: new cRadioAudio");
}

cRadioAudio::~cRadioAudio() {
    free(bitrate);
    dsyslog("radio: delete cRadioAudio");
}

/* for old pes-recordings */
void cRadioAudio::Play(const uchar *Data, int Length, uchar Id) {
    if (!enabled) {
        return;
    }
    if (RDSReceiver)
        return;
    if (Id < 0xc0 || Id > 0xdf) {
        return;
    }

    // Rass-Images Slideshow
    if (S_RassText > 0 && Rass_Archiv == -1 && Rass_Show == 1) {
        Rass_Show = 0;
        char *image;
        asprintf(&image, "%s/Rass_show.mpg", DataDir);
        RadioImage->SetBackgroundImage(image);
        free(image);
    }

    // check Audo-Bitrate
    if (S_RtFunc < 1) {
        return;
    }
    if (first_packets < 3) {
        first_packets++;
        if (first_packets == 3) {
            free(bitrate);
            bitrate = audiobitrate(Data);
        }
        return;
    }

    // check Radiotext-PES
    if (Radio_CA == 0) {
        RadiotextCheckPES(Data, Length);
    }
}

void cRadioAudio::PlayTs(const uchar *Data, int Length) {
    if (!enabled) {
        return;
    }
    if (RDSReceiver)
        return;

    // Rass-Images Slideshow
    if (S_RassText > 0 && Rass_Archiv == -1 && Rass_Show == 1) {
        Rass_Show = 0;
        char *image;
        asprintf(&image, "%s/Rass_show.mpg", DataDir);
        RadioImage->SetBackgroundImage(image);
        free(image);
    }

    if (S_RtFunc < 1) {
        return;
    }
    if (first_packets < 99) {
        first_packets++;
        return;
    }

    // check Radiotext-TS
    if (Radio_CA == 0) {
        RadiotextCheckTS(Data, Length);
    }
}

/* for old pes-recordings */
void cRadioAudio::RadiotextCheckPES(const uchar *data, int len) {
    const int mframel = 263; // max. 255(MSG)+4(ADD/SQC/MFL)+2(CRC)+2(Start/Stop) of RDS-data
    static unsigned char mtext[mframel + 1];
    static bool rt_start = false, rt_bstuff = false;
    static int index;
    static int mec = 0;

    int offset = 0;
    while (true) {

        int pesl = (offset + 5 < len) ? (data[offset + 4] << 8) + data[offset + 5] + 6 : -1;
        if ((pesl <= 0) || ((offset + pesl) > len)) {
            return;
        }

        offset += pesl;
        int rdsl = data[offset - 2];  // RDS DataFieldLength
        // RDS DataSync = 0xfd @ end
        if ((data[offset - 1] == 0xfd) && (rdsl > 0)) {
            // print RawData with RDS-Info
            if ((S_Verbose & 0x0f) >= 3) {
                printf("\n\nPES-Data(%d/%d): ", pesl, len);
                for (int a = offset - pesl; a < offset; a++) {
                    printf("%02x ", data[a]);
                }
                printf("(End)\n\n");
            }

            for (int i = offset - 3, val; i > offset - 3 - rdsl; i--) { // <-- data reverse, from end to start
                val = data[i];

                if (val == 0xfe) {  // Start
                    index = -1;
                    rt_start = true;
                    rt_bstuff = false;
                    if ((S_Verbose & 0x0f) >= 2) {
                        printf("\nRDS-Start: ");
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
                        case 0xda:              // Rass
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
                    if (index >= mframel) {     // max. rdslength, garbage ?
                        if ((S_Verbose & 0x0f) >= 1) {
                            printf("RDS-Error(PES): too long, garbage ?\n");
                        }
                        rt_start = false;
                    }
                }

                if (rt_start && val == 0xff) {  // End
                    if ((S_Verbose & 0x0f) >= 2) {
                        printf("(RDS-End)\n");
                    }
                    rt_start = false;
                    if (index < 9) {        //  min. rdslength, garbage ?
                        if ((S_Verbose & 0x0f) >= 1) {
                            printf("RDS-Error(PES): too short -> garbage ?\n");
                        }
                    } else {
                        // crc16-check
                        unsigned short crc16 = crc16_ccitt(mtext, index - 3, true);
                        if (crc16
                                != (mtext[index - 2] << 8) + mtext[index - 1]) {
                            if ((S_Verbose & 0x0f) >= 1) {
                                printf("RDS-Error(PES): wrong CRC # calc = %04x <> transmit = %02x%02x\n", crc16, mtext[index - 2], mtext[index - 1]);
                            }
                        } else {
                            switch (mec) {
                            case 0x0a:
                                RadiotextDecode(mtext);      // Radiotext
                                break;
                            case 0x46:
                                switch ((mtext[7] << 8) + mtext[8]) {  // ODA-ID
                                case 0x4bd7:
                                    RadioAudio->RadiotextDecode(mtext); // RT+
                                    break;
                                case 0x0d45:
                                case 0xcd46:
                                    if ((S_Verbose & 0x20) > 0) {
                                        unsigned char tmc[6];   // TMC Alert-C
                                        int i;
                                        for (i = 9; i <= (index - 3); i++)
                                            tmc[i - 9] = mtext[i];
                                        tmc_parser(tmc, i - 8);
                                    }
                                    break;
                                default:
                                    if ((S_Verbose & 0x0f) >= 2) {
                                        printf("[RDS-ODA AID '%02x%02x' not used -> End]\n", mtext[7], mtext[8]);
                                    }
                                }
                                break;
                            case 0x07:
                                RT_PTY = mtext[8];                        // PTY
                                if ((S_Verbose & 0x0f) >= 1) {
                                    printf("RDS-PTY set to '%s'\n", ptynr2string(RT_PTY));
                                }
                                break;
                            case 0x3e:
                                RDS_PsPtynDecode(true, mtext, index);    // PTYN
                                break;
                            case 0x02:
                                RDS_PsPtynDecode(false, mtext, index);     // PS
                                break;
                            case 0xda:
                                RassDecode(mtext, index);                // Rass
                                break;
                            case 0x30:
                                if ((S_Verbose & 0x20) > 0) {     // TMC Alert-C
                                    unsigned char tmc[6];
                                    int i;
                                    for (i = 7; i <= (index - 3); i++) {
                                        tmc[i - 7] = mtext[i];
                                    }
                                    tmc_parser(tmc, i - 6);
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}

int cRadioAudio::GetLatmRdsDSE(const uchar *plBuffer, int plBufferCnt, bool rt_start) {

    // 'Data Stream Element' <DSE> reverse scanning:
    //
    // AAC-LATM frames contain one or more elements and are always terminated by element-id <TERM>.
    // element/frame format: <other>+<DSE>?<FIL>?<[TERM>
    // element IDs (3 bits): <DSE>=4 (100), <FIL>=6 (110), <TERM>=7 (111)
    // To find a <DSE>
    // - count the required bitwise shift to byte-align <TERM> ('111' -> 1110_0000 / 0xE0).
    // - byte-align all data in front of <TERM> and search for <DSE> ('100' -> 1000_0000 / 0x80) with matching length-field
    // - if no <DSE> but a potential <FIL> was detected (0x6x), adjust the shift, skip over <FIL> and search once again for <DSE>
    //
    // The <DSE> may held RDS-data - complete or splitted over multiple frames, in the order [0xfe,...,0xff]).
    //
    // Assumptions to keep the code simple:
    // only one short <FIL> element, RDS data is originally not(!) byte-aligned after the <DSE> header, <DSE> Len < 255 bytes

    const uchar *p = plBuffer + plBufferCnt - 1; // start at the end of the frame
    while (!*p) { p--; }                // last byte with non-zero bits
    uint16_t tmp = (p[-1] << 8) | p[0]; // read 16 bits

    // find required bit-shifts to byte-align <TERM>
    int rs = 0;
    while   (tmp & 0x1F)  { tmp <<= 1; rs--; }
    while (!(tmp & 0x20)) { tmp >>= 1; rs++; }

    if ((tmp & 0xFF) == 0xE0) { // <TERM> -> search for <DSE>, 8-bit header
        int eCnt = -1;

        while (p > plBuffer) {
            const uchar *eFIL = NULL;
            int eLen = -2;
            eCnt++;
            if (!rt_start)
                maxRdsChunkIndex = RDS_CHUNKSIZE - 1;

            if (rs < 0)
                rs += 8; // data is at current byte-pos
            else
                p--;     // data is at byte-pos -1

            int i;
            for (i = 0; i <= maxRdsChunkIndex && p > plBuffer; i++, p--) { // store reversed, as in MPEG-1 TS
                tmp = (p[-1] << 8) | p[0];
                uchar eCh = (tmp >> rs) & 0xFF;
                rdsChunk[i] = eCh;

                if (eCnt == 0 && i < 0xF && !eFIL && (eCh & 0xF) == i && (eCh & 0x70) == 0x60) { // potential short <FIL>, 7-bit header
                    eFIL = p;
                    //dsyslog("%s: <FIL> (ID-byte %02X)", __func__, eCh & 0x7F); hexdump(rdsChunk, i + 1);
                    }
                if (eCh == 0x80 && eLen == (i - 1)) { // <DSE> with matching len-field
                    if (eLen == 3 && rdsChunk[i-2] == 0xBC && rdsChunk[i-3] == 0xC0 && rdsChunk[i-4] == 0x0) {
                        eFIL = NULL;
                        if ((S_Verbose & 0x0f) >= 1) dsyslog("%s: skip MPEG-4 ancillary data (sync %02X, len %d.)", __func__, rdsChunk[i-2], eLen);
                        break;// skip this known <DSE> and search next
                        }
                    if (rt_start) { // continued
                        if (i == maxRdsChunkIndex || rdsChunk[0] == 0xFF) {
                            //dsyslog("%s: <DSE>[%d] <<<", __func__, eCnt); hexdump(rdsChunk, eLen + 2);
                            return eLen; // <DSE> found
                        }
                    }
                    else if (eLen > 0 && rdsChunk[i-2] == 0xFE && (eLen < 2 || rdsChunk[i-3] == 0x00)) { // check 4 bytes "0x80, eLen, 0xfe, 0x00"
                        //dsyslog("%s: <DSE>[%d]", __func__, eCnt); hexdump(rdsChunk, eLen + 2);
                        maxRdsChunkIndex = i;
                        return eLen; // <DSE> found
                        }
                    }
                // simple plausibilty checks - invalid 'rt_stop','rt_start' or 'stuffing'
                if ((i > 0 && (eCh == 0xFF && rdsChunk[i-1] != 0xFE) || (eCh == 0xFD && rdsChunk[i-1] > 2)) || (i > 1 && rdsChunk[i-2] == 0xFE && rdsChunk[i-1] != 0xFF)) {
                    //dsyslog("%s: <--->[%d/%d] invalid start/stop/stuffing %02X,%02X", __func__, eCnt, i, eCh, rdsChunk[i-1]);
                    i = maxRdsChunkIndex; // retry with <FIL> or return
                    }
                eLen = eCh;
                }

            if (eFIL) { // skip <FIL> and search for <DSE> again
                p = eFIL;
                rs--; // adjust shift
                }
            else if (i > maxRdsChunkIndex)
                break;
            }
        }
    else {
        if ((S_Verbose & 0x0f) >= 1) dsyslog("%s: <TERM> not found (0x%02X)", __func__, tmp & 0xFF);
        }
    return -1;
}

enum eStreamType
{
    st_NONE = 1,
    st_MPEG,
    st_LATM
};

void cRadioAudio::RadiotextCheckTS(const uchar *data, int len) {
    static bool pesfound = false;
    static int streamType = 0;
    static int pPesLen = 0;
    static int pFrameSize = 0;
    static int frameSize = 0; // static for mpaFrameInfo
    static uint32_t mpaFrameInfo = 0;

    #define PAYLOAD_BUFSIZE (((RDS_CHUNKSIZE/TS_SIZE)+2)*TS_SIZE)
    static uchar plBuffer[PAYLOAD_BUFSIZE];
    static int plBufferCnt = 0;
    #define HEADER_SIZE 4
    static uchar buFrameHeader[HEADER_SIZE];
    static int buFrameHeaderCnt = -1;
    static int lastApid = -1;
    static bool rt_start = false;
    static uchar ccn = 0;
    #define RDS_SCAN_TIMEOUT 60 // seconds before considering audiostream has no RDS data
    static time_t rdsScan = 0;

    // verify TS
    if (data[0] != 0x47) {
        pesfound = false;
        return;
        }
    if (!TsHasPayload(data))
        return;

    // audio track
    if (TsPid(data) != lastApid) {
        dsyslog("%s: new audio track - Pid %d", __func__, TsPid(data));
        pesfound = false;
        streamType = 0;
        mpaFrameInfo = 0;
        lastApid = TsPid(data);
        rt_start = RadiotextParseTS(NULL, 0); // reset RDS parser
        ccn = TsContinuityCounter(data);
        rdsScan = time(NULL);
        rdsSeen = false;
        }

    if (!rdsScan)
        return;

    if ((S_Verbose & 0x0f) >= 3) {
        dsyslog("TS-Data(%d):", len);
        hexdump(data, len);
        dsyslog("(TS-End)");
        }

    uchar ccc = TsContinuityCounter(data);
    if (ccc != ccn) {
        dsyslog("CC-ERROR: want %d, got %d", ccn, ccc);
        // reset RDS parser and frame detection
        rt_start = RadiotextParseTS(NULL, 0);
        pesfound = false;
        }
    ccn = (ccc + 1) & 0xF;

    int pOffset = TsPayloadOffset(data); // TS-Header+ADFL
    int payloadLen = TS_SIZE - pOffset;

    if (TsPayloadStart(data)) {
        pesfound = false;
        if (data[pOffset] == 0x00 && data[pOffset + 1] == 0x00 && data[pOffset + 2] == 0x01 && // PES-Startcode
                data[pOffset + 3] >= 0xc0 && data[pOffset + 3] <= 0xdf) {
            // PES-Audiostream
            int pesLen = (data[pOffset + 4] << 8) + data[pOffset + 5];
            pPesLen = pesLen + 6; // from payload start
            pesfound = true;

            int headerLen = 8 + data[pOffset + 8] + 1; // PES headerlen
            pOffset += headerLen; // from frame start
            pPesLen -= headerLen;
            payloadLen -= headerLen;

            pFrameSize = 0;
            buFrameHeaderCnt = 0; // start of frame
            }
        }

    if (!pesfound)
        return; /* no PES-Audio found */

    //dsyslog("+++ offset %2d fs %3d ps %3d pll %3d - PUSI %d", pOffset, pFrameSize, pPesLen, payloadLen, TsPayloadStart(data));

    while (payloadLen > 0 && payloadLen <= pPesLen) {
        if (buFrameHeaderCnt >= 0) { // start of frame
            // read (remaining) frameheader
            for (int p = pOffset; p < TS_SIZE && buFrameHeaderCnt < HEADER_SIZE; p++) {
                buFrameHeader[buFrameHeaderCnt++] = data[p];
                }
            if (buFrameHeaderCnt < HEADER_SIZE) {
                break; // need more data
                }
            buFrameHeaderCnt = -1;
            plBufferCnt = 0; // reset payload buffer

            int hdStreamType = st_NONE;
            if (buFrameHeader[0] == 0x56 && (buFrameHeader[1] & 0xE0) == 0xE0) {
                hdStreamType = st_LATM;
                frameSize = ((buFrameHeader[1] & 0x1F) << 8) + buFrameHeader[2];
                pFrameSize += frameSize + 3; // from frame start
                }
            else if (buFrameHeader[0] == 0xFF && (buFrameHeader[1] & 0xE0) == 0xE0) {
                ParseMpaFrameHeader(buFrameHeader, &mpaFrameInfo, &frameSize, &bitrate);
                hdStreamType = st_MPEG;
                pFrameSize += frameSize; // from frame start
                }

            if (frameSize <= 0 || pFrameSize > pPesLen)
                hdStreamType = st_NONE;

            if (streamType != hdStreamType) {
                streamType = hdStreamType;
                if ((S_Verbose & 0x0f) >= 1) {
                    if (streamType != st_NONE)
                        dsyslog("%s: audioformat: %s", __func__, streamType == st_LATM ? "AAC-LC" : "MPEG-1");
                    else
                        dsyslog("%s: ERROR - unhandled audioformat or invalid header: <%02X %02X %02X>", __func__, buFrameHeader[0], buFrameHeader[1], buFrameHeader[2]);
                    }
                }

            if (streamType == st_NONE) {
                pesfound = false;
                return;
                }
#if 0
            if (pFrameSize <= payloadLen && (S_Verbose & 0x0f) >= 1) // frame start and end within one TS
                { dsyslog("%s: short frame - len %d/0x%02X, payload %d, peslen %d", __func__, pFrameSize, pFrameSize, payloadLen, pPesLen); hexdump(data, len); }
#endif
            }
        if (pFrameSize <= payloadLen) { // end of frame
            // copy into payload buffer
            memcpy(plBuffer + plBufferCnt, data + pOffset, pFrameSize);
            plBufferCnt += pFrameSize;

            //--- RDS
            const uchar *rdsData = NULL;
            int rdsLen = 0;

            if (streamType == st_LATM) {
                rdsLen = GetLatmRdsDSE(plBuffer, plBufferCnt, rt_start);
                rdsData = rdsChunk;
                }
            else if (streamType == st_MPEG) {
                if (plBuffer[plBufferCnt - 1] == 0xFD) {
                    rdsLen = plBuffer[plBufferCnt - 2];
                    rdsData = plBuffer + plBufferCnt - (rdsLen + 2);
                    }
                }
            if (rdsLen > 0)
                rt_start = RadiotextParseTS(rdsData, rdsLen);
            //---

            if (pPesLen > pFrameSize) { // more frames in PES - goto next frame header
                pOffset += pFrameSize;
                buFrameHeaderCnt = 0;
                }
            payloadLen -= pFrameSize;
            pPesLen -= pFrameSize;
            pFrameSize = 0;
            }
        else { // save payload for a minimum data size of RDS_CHUNKSIZE
            if ((pFrameSize - payloadLen) < RDS_CHUNKSIZE) {
                memcpy(plBuffer, data + pOffset, payloadLen); // may include PES and Audio Headers!
                plBufferCnt = payloadLen;
                }
            break;
            }
        }
    pFrameSize -= payloadLen;
    pPesLen -= payloadLen;
    pesfound = (pPesLen > 0);

    if (pPesLen < 0 || (pPesLen == 0 && pFrameSize != 0))
        if ((S_Verbose & 0x0f) >= 1) dsyslog("%s: ERROR - invalid PesLen or FrameSize missmatch: PayloadLen:%d PesLen:%d FrameSize:%d", __func__, payloadLen, pPesLen, pFrameSize);

    if (!rdsSeen) {
        if ((time(NULL) - rdsScan) > RDS_SCAN_TIMEOUT) {
            dsyslog("%s: no RDS data after %d seconds - disable RDS scan", __func__, RDS_SCAN_TIMEOUT);
            rdsScan = 0;
            rdsSeen = true; // no more checks
            }
        }
}

bool cRadioAudio::RadiotextParseTS(const uchar *RdsData, int RdsLen) {
    #define MSG_SIZE 263 // max. 255(MSG)+4(ADD/SQC/MFL)+2(CRC)+2(Start/Stop) of RDS-data
    static unsigned char mtext[MSG_SIZE + 1];
    static int rt_start = 0, rt_bstuff = 0, rt_lastVal = 0xff;
    static int index;
    static int mec = 0;

    if (!RdsData) { // reset
        rt_start = 0;
        return rt_start;
        }

    if (!rt_start)
        rt_lastVal = 0xff;

    // RDS data
    int stop_index = (rt_start && index >= 4) ? mtext[4] + 7 : MSG_SIZE - 1;
    for (const uchar *p = RdsData + RdsLen - 1; p >= RdsData; p--) { // <-- data reverse, from end to start
        int val = *p;
        if (val == 0xfe ) { // Start
            if (rt_lastVal == 0xff) { // valid start
                index = -1;
                rt_start = 1;
                rt_bstuff = 0;
                mec = 0;
                if ((S_Verbose & 0x0f) >= 2) {
                    printf("\nRDS-Start: ");
                }
            }
            else if (rt_start) {
                if ((S_Verbose & 0x0f) >= 1) dsyslog("Invalid RDS-start[%d/%d]: %02x %02x", index, RdsLen, rt_lastVal, val);
                rt_start = 0;
                break;
            }
        }
        rt_lastVal = val;

        if (rt_start == 1) {
            if ((S_Verbose & 0x0f) >= 2) {
                printf("%02x ", val);
            }
            // byte-stuffing reverse: 0xfd00->0xfd, 0xfd01->0xfe, 0xfd02->0xff
            int stv = val;
            if (rt_bstuff == 1) {
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
                    if ((S_Verbose & 0x0f) >= 1)
                        dsyslog("RDS-Error(TS): invalid Bytestuffing at [%d]: 0x%02x, garbage ?", index, val);
                    rt_start = 0;
                    break;
                }
                rt_bstuff = 0;
                if ((S_Verbose & 0x0f) >= 2) {
                    printf("(Bytestuffing -> %02x) ", mtext[index]);
                }
                stv = mtext[index]; // stuffed value
            } else {
                mtext[++index] = val;
                if (val == 0xfd) { // stuffing found
                    rt_bstuff = 1;
                    continue;
                }
            }
            // early check for used mec
            if (index == 5) {
                switch (stv) {
                case 0x0a:                  // RT
                case 0x46:                  // ODA-Data
                case 0xda:                  // Rass
                case 0x07:                  // PTY
                case 0x3e:                  // PTYN
                case 0x30:                  // TMC
                case 0x02:                  // PS
                    mec = stv;
                    RdsLogo = true;
                    break;
                default:
                    mec = 0;
                    if ((S_Verbose & 0x0f) >= 2) {
                        dsyslog("[RDS-MEC '%02x' not used -> End]", stv);
                    }
                }
            }
            else if (index == 4) // MFL
                stop_index = stv + 7;

            if (val == 0xff ? index != stop_index : index == stop_index ) { // wrong rdslength or rt_stop, garbage ?
                if ((S_Verbose & 0x0f) >= 1)
                    dsyslog("RDS-Error(TS): invalid RDS length: index %d, stop %d, val %02x, garbage ?", index, stop_index, val);
                rt_start = 0;
                break;
            }
        }

        if (rt_start == 1 && val == 0xff) {	    // End
            if ((S_Verbose & 0x0f) >= 2) {
                printf("(RDS-End)\n");
            }
            rt_start = 0;
            rdsSeen = true;
            if ((S_Verbose & 0x0f) >= 1)
                { dsyslog("-- RDS [%02X] --", mtext[5]); hexdump(mtext, index+1); }

            if (index < 9) {		//  min. rdslength, garbage ?
                if ((S_Verbose & 0x0f) >= 1) {
                    dsyslog("RDS-Error(TS): too short: %d -> garbage ?", index);
                }
            } else if (mec > 0) {
                // crc16-check
                unsigned short crc16 = crc16_ccitt(mtext, index - 3, 1);
                if (crc16 != (mtext[index - 2] << 8) + mtext[index - 1]) {
                    if ((S_Verbose & 0x0f) >= 1) {
                        dsyslog("RDS-Error(TS): wrong CRC # calc = %04x <> transmit = %02x%02x\n", crc16, mtext[index - 2], mtext[index - 1]);
                    }
                } else {
                    switch (mec) {
                    case 0x0a:
                        RadiotextDecode(mtext); // Radiotext
                        break;
                    case 0x46:
                        switch ((mtext[7] << 8) + mtext[8]) { // ODA-ID
                        case 0x4bd7:
                            RadiotextDecode(mtext); // RT+
                            break;
                        case 0x0d45:
                        case 0xcd46:
                            if ((S_Verbose & 0x20) > 0) {
                                unsigned char tmc[6]; // TMC Alert-C
                                int i;
                                for (i = 9; i <= (index - 3); i++)
                                    tmc[i - 9] = mtext[i];
                                tmc_parser(tmc, i - 8);
                            }
                            break;
                        default:
                            if ((S_Verbose & 0x0f) >= 2) {
                                printf("[RDS-ODA AID '%02x%02x' not used -> End]\n", mtext[7], mtext[8]);
                            }
                        }
                        break;
                    case 0x07:
                        RT_PTY = mtext[8];								// PTY
                        if ((S_Verbose & 0x0f) >= 1) {
                            dsyslog("RDS-PTY set to '%s'\n", ptynr2string(RT_PTY));
                        }
                        break;
                    case 0x3e:
                        RDS_PsPtynDecode(1, mtext, index);				// PTYN
                        break;
                    case 0x02:
                        RDS_PsPtynDecode(0, mtext, index);				// PS
                        break;
                    case 0xda:
                        RassDecode(mtext, index);					    // Rass
                        break;
                    case 0x30:
                        if ((S_Verbose & 0x20) > 0) {			// TMC Alert-C
                            unsigned char tmc[6];
                            int i;
                            for (i = 7; i <= (index - 3); i++) {
                                tmc[i - 7] = mtext[i];
                            }
                            tmc_parser(tmc, i - 6);
                        }
                    }
                }
            }
        }
    }
    return rt_start;
}

void cRadioAudio::RadiotextDecode(unsigned char *mtext) {
    static bool rtp_itoggle = false;
    static int rtp_idiffs = 0;
    static cTimeMs rtp_itime;
    static char plustext[RT_MEL];

    // byte 1+2 = ADD (10bit SiteAdress + 6bit EncoderAdress)
    // byte 3   = SQC (Sequence Counter 0x00 = not used)
    int mfl = mtext[4]; // byte 4 = MFL (Message Field Length)
    // byte 5 = MEC (Message Element Code, 0x0a for RT, 0x46 for RTplus)
    if (mtext[5] == 0x0a) {
        // byte 6+7 = DSN+PSN (DataSetNumber+ProgramServiceNumber,
        //                     ignore here, always 0x00 ?)
        // byte 8   = MEL (MessageElementLength, max. 64+1 byte @ RT)
        int mel = mtext[8];
        if (mel == 0 || mel > mfl - 4) {
            if ((S_Verbose & 0x0f) >= 1) {
                dsyslog("RT-Error: Length not correct (MFL= %d, MEL= %d)", mfl, mel);
            }
            return;
        }
        // byte 9 = RT-Status bitcodet (0=AB-flagcontrol, 1-4=Transmission-Number, 5+6=Buffer-Config,
        //                  ignored, always 0x01 ?)
        char temptext[RT_MEL];
        memset(temptext, 0x20, RT_MEL);
        temptext[RT_MEL - 1] = '\0';

        for (int i = 1, ii = 0; i < mel; i++, ii++) {
            uchar ch = mtext[9 + i];
            // additional rds-character, see RBDS-Standard, Annex E
            temptext[ii] = (ch >= 0x80) ? rds_addchar[ch - 0x80] : ch;
        }
        memcpy(plustext, temptext, RT_MEL);
        rds_entitychar(temptext);
        // check repeats
        bool repeat = false;
        for (int ind = 0; !repeat && ind < S_RtOsdRows; ind++) {
            repeat = (memcmp(RT_Text[ind], temptext, RT_MEL - 1) == 0);
            if (repeat && (S_Verbose & 0x0f) >= 1) {
                dsyslog("RText-Rep[%d]: '%s'", ind, RT_Text[ind]);
            }
        }
        if (!repeat) {
            memcpy(RT_Text[RT_Index], temptext, RT_MEL);
            // +Memory
            rtrim(temptext);
            if (++rtp_content.radiotext.index >= 2 * MAX_RTPC)
                rtp_content.radiotext.index = 0;
            memcpy(rtp_content.radiotext.Msg[rtp_content.radiotext.index], temptext, RT_MEL);
            if ((S_Verbose & 0x0f) >= 1) {
                dsyslog("Radiotext[%d/%d]: '%s'", RT_Index, rtp_content.radiotext.index, RT_Text[RT_Index]);
            }
            if (++RT_Index >= S_RtOsdRows)
                RT_Index = 0;
        }
        RTP_TToggle = true;
        RT_MsgShow = true;
        (RT_Info > 0) ? : RT_Info = 1;
        radioStatusMsg();
    }
    else if (RTP_TToggle && mtext[5] == 0x46 && S_RtFunc >= 2) { // RTplus tags V2.1, only if RT
        int mel = mtext[6];
        if (mfl != 10 || mel != 8) { // byte 6 = MEL, only 8 byte for 2 tags
            if ((S_Verbose & 0x0f) >= 1) {
                dsyslog("RTp-Error: Length not correct (MFL= %d, MEL= %d)", mfl, mel);
            }
            return;
        }
        char rtp_temptext[RT_MEL];
        uint rtp_typ[2], rtp_start[2], rtp_len[2];
        // byte 7+8 = ApplicationID, always 0x4bd7
        // byte 9   = Applicationgroup Typecode / PTY ?
        // bit 10#4 = Item Togglebit
        // bit 10#3 = Item Runningbit

        // Tag1: bit 10#2..11#5 = Contenttype, 11#4..12#7 = Startmarker, 12#6..12#1 = Length(6bit)
        rtp_typ[0]   = (0x38 & mtext[10] << 3) | mtext[11] >> 5;
        rtp_start[0] = (0x3e & mtext[11] << 1) | mtext[12] >> 7;
        rtp_len[0]   =  0x3f & mtext[12] >> 1;
        // Tag2: bit 12#0..13#3 = Contenttype, 13#2..14#5 = Startmarker, 14#4..14#0 = Length(5bit)
        rtp_typ[1]   = (0x20 & mtext[12] << 5) | mtext[13] >> 3;
        rtp_start[1] = (0x38 & mtext[13] << 3) | mtext[14] >> 5;
        rtp_len[1]   =  0x1f & mtext[14];

        bool rt_bit_toggle  = mtext[10] & 0x10;
        bool rt_bit_running = mtext[10] & 0x08;

        if ((S_Verbose & 0x0f) >= 1) {
            dsyslog("RTplus (tag=Typ/Start/Len):  Toggle/Run = %d/%d, tag#1 = %d/%d/%d, tag#2 = %d/%d/%d",
                    rt_bit_toggle, rt_bit_running,
                    rtp_typ[0], rtp_start[0], rtp_len[0], rtp_typ[1], rtp_start[1], rtp_len[1]);
        }

        struct rtp_item *item = (rtp_content.items.index >= 0) ? &rtp_content.items.Item[rtp_content.items.index] : NULL;
        if (!item)
            RTP_ItemToggle = !rt_bit_toggle; // set with first item

        // save item info
        int plustxtLen = strlen(plustext);

        for (int i = 0; i < 2; i++) {
            //if (rtp_start[i] + rtp_len[i] + 1 >= RT_MEL) {  // length-error
            if (rtp_start[i] + rtp_len[i] + 1 > plustxtLen) {  // length-error
                if ((S_Verbose & 0x0f) >= 1)
                    dsyslog("RTp-Error (tag#%d = Typ/Start/Len): %d/%d/%d (Start+Length > 'RT-MEL' %d !)", i + 1, rtp_typ[i], rtp_start[i], rtp_len[i], plustxtLen);
                continue;
            }
            if (rtp_typ[i] > RTP_CLASS_MAX) {
                if ((S_Verbose & 0x0f) >= 1)
                    dsyslog("rt+: unhandled class %d", rtp_typ[i]);
                continue;
            }
            if (rtp_typ[i]) {
                // tag string
                char temptext[RT_MEL];
                memcpy(temptext, plustext + rtp_start[i], rtp_len[i] + 1);
                temptext[rtp_len[i] + 1] = '\0';
                rds_entitychar(temptext);
                rtrim(temptext);

                if (item && rt_bit_toggle == RTP_ItemToggle) { // check for item-change by content
                    char *iText = (rtp_typ[i] == item_Title) ? item->Title : (rtp_typ[i] == item_Artist) ? item->Artist : NULL;
                    RTP_ItemToggle |= iText && *iText && strcmp(iText, temptext);
                }

                if (rt_bit_toggle != RTP_ItemToggle) { // item-change or first item
                    if (IS_ITEM_CLASS(rtp_typ[i])) {
                        for (int i = RTP_CLASS_ITEM_MIN; i <= RTP_CLASS_ITEM_MAX; i++)
                            rtp_content.rtp_class[i][0] = '\0'; // clear all current item infos

                        if (I_MASK & I_MAP(rtp_typ[i])) { // visible RT+ item
                            if (!item || item->itemMap) { // first or used item
                                if (++rtp_content.items.index >= MAX_RTPC)
                                    rtp_content.items.index = 0;
                                item = &rtp_content.items.Item[rtp_content.items.index];
                                // clear old cached item infos
                                memset(item, 0, sizeof(struct rtp_item));
                            }
                            item->start = time(NULL);
                            // RTP_Title = RTP_Artist = RTP_Album = RTP_Composer = RTP_Conductor = RTP_Band = NULL;
                            sprintf(RTP_Title, "---");
                            sprintf(RTP_Artist, "---");
                            sprintf(RTP_Composer, "");
                            sprintf(RTP_Album, "");
                            sprintf(RTP_Conductor, "");
                            sprintf(RTP_Band, "");
                        }
                    }
                    if (RT_PlusShow && rtp_itime.Elapsed() > 1000)
                        rtp_idiffs = (int) rtp_itime.Elapsed() / 1000;
                    rtp_itime.Set(0);

                    dsyslog("RTPlus: Item toggle(%d -> %d) index %d", RTP_ItemToggle, rt_bit_toggle, rtp_content.items.index);
                    RTP_ItemToggle = rt_bit_toggle;
                    RTP_Starttime = time(NULL);
                }

                // store msg in rtp_class
                char *rtp_class_msg = rtp_content.rtp_class[rtp_typ[i]];
                strcpy(rtp_class_msg, temptext); // set or update

                if (item) {
                    char *iMsg;
                    char *rMsg;
                    switch (rtp_typ[i]) {
                    case item_Title:    // 1
                        iMsg = item->Title;
                        rMsg = RTP_Title;
                        break;
                    case item_Album:    // 2
                        iMsg = item->Album;
                        rMsg = RTP_Album;
                        break;
                    case item_Artist:   // 4
                        iMsg = item->Artist;
                        rMsg = RTP_Artist;
                        break;
                    case item_Conductor:// 7
                        iMsg = item->Conductor;
                        rMsg = RTP_Conductor;
                        break;
                    case item_Composer: // 8
                        iMsg = item->Composer;
                        rMsg = RTP_Composer;
                        break;
                    case item_Band:     // 9
                        iMsg = item->Band;
                        rMsg = RTP_Band;
                        break;
                    default: iMsg = rMsg = NULL;
                    }
                    if (iMsg) {
                        strcpy(iMsg, rtp_class_msg);
                        strcpy(rMsg, rtp_class_msg);
                        item->itemMap |= I_MAP(rtp_typ[i]);
                        RT_PlusShow = RT_MsgShow = rtp_itoggle = true;
                    }
                }
                struct rtp_info_cache *cCache;
                switch (rtp_typ[i]) {
                case info_News:     // 12
                    cCache = &rtp_content.info_News;
                    break;
                case info_Stock:    // 14
                    cCache = &rtp_content.info_Stock;
                    break;
                case info_Sport:    // 15
                    cCache = &rtp_content.info_Sport;
                    break;
                case info_Lottery:  // 15
                    cCache = &rtp_content.info_Lottery;
                    break;
                case info_Weather:  // 25
                    cCache = &rtp_content.info_Weather;
                    break;
                case info_Other:    // 30
                    cCache = &rtp_content.info_Other;
                    break;
                default: cCache = NULL;
                }
                if (cCache) {
                    if (cCache->index < 0 || (*cCache->Content[cCache->index] && strcmp(cCache->Content[cCache->index], rtp_class_msg))) {
                        if (++cCache->index >= MAX_RTPC)
                            cCache->index = 0;
                        cCache->start[cCache->index] = time(NULL);
                        strcpy(cCache->Content[cCache->index], rtp_class_msg);
                    }
                }
            }
        }
        RTP_TToggle = false;
        item = NULL; // here item may or may not be NULL - just to clearify
        // running status - not allways reliable !
        rtp_content.items.running = rt_bit_running;
        // Title-end @ no Item-Running
        if (!rt_bit_running) {
            if (RT_PlusShow) {
                rtp_itoggle = true;
                rtp_idiffs = (int) rtp_itime.Elapsed() / 1000;
                RTP_Starttime = time(NULL);
            }
            RT_MsgShow = (RT_Info > 0);
        }

        if (rtp_itoggle) {
            if ((S_Verbose & 0x0f) >= 1) {
                struct tm tm_store;
                struct tm *ts = localtime_r(&RTP_Starttime, &tm_store);
                if (rtp_idiffs > 0) {
                    dsyslog("  StartTime : %02d:%02d:%02d  (last Title elapsed = %d s)", ts->tm_hour, ts->tm_min, ts->tm_sec, rtp_idiffs);
                }
                else {
                    dsyslog("  StartTime : %02d:%02d:%02d", ts->tm_hour, ts->tm_min, ts->tm_sec);
                }
                dsyslog("  RTp-Title : '%s'  RTp-Artist: '%s' RTp-Composer: '%s'", RTP_Title, RTP_Artist, RTP_Composer);
            }
            rtp_itoggle = false;
            rtp_idiffs = 0;
            radioStatusMsg();
            AudioRecorderService();
        }
    }
}

void cRadioAudio::RDS_PsPtynDecode(bool ptyn, unsigned char *mtext, int len) {
    if (len < 16)
        return;

    // decode Text
    for (int i = 8; i <= 15; i++) {
        if (mtext[i] <= 0xfe) {
            // additional rds-character, see RBDS-Standard, Annex E
            if (!ptyn) {
                RDS_PSText[RDS_PSIndex][i - 8] = (mtext[i] >= 0x80) ? rds_addchar[mtext[i] - 0x80] : mtext[i];
            }
            else {
                RDS_PTYN[i - 8] = (mtext[i] >= 0x80) ? rds_addchar[mtext[i] - 0x80] : mtext[i];
            }
        }
    }

    if ((S_Verbose & 0x0f) >= 1) {
        if (!ptyn) {
            printf("RDS-PS  No= %d, Content[%d]= '%s'\n", mtext[7], RDS_PSIndex, RDS_PSText[RDS_PSIndex]);
        }
        else {
            printf("RDS-PTYN  No= %d, Content= '%s'\n", mtext[7], RDS_PTYN);
        }
    }

    if (!ptyn) {
        RDS_PSIndex += 1;
        if (RDS_PSIndex >= 12) {
            RDS_PSIndex = 0;
        }
        RT_MsgShow = RDS_PSShow = true;
    }
}

void cRadioAudio::AudioRecorderService(void) {
    /* check plugin audiorecorder service */
    ARec_Receive = ARec_Record = false;

    if (!RT_PlusShow || RT_Replay) {
        return;
    }

    Audiorecorder_StatusRtpChannel_v1_0 arec_service;
    cPlugin *p;

    arec_service.channel = chan;
    p = cPluginManager::CallFirstService("Audiorecorder-StatusRtpChannel-v1.0", &arec_service);
    if (p) {
        ARec_Receive = (arec_service.status >= 2);
        ARec_Record = (arec_service.status == 3);
    }
}

// add <names> of DVB Radio Slides Specification 1.0, 20061228
void cRadioAudio::RassDecode(unsigned char *mtext, int len) {
    if (RT_Replay)       // no recordings $20090905
        return;

    static uint splfd = 0, spmax = 0, index = 0;
    static uint afiles, slidenumr, slideelem, filemax;
    static int filetype;
    static bool slideshow = false, slidesave = false, slidecan = false, slidedel = false, start = false;
    static uchar daten[65536];  // mpegs-stills defined <= 50kB
    FILE *fd;

    // byte 1+2 = ADD (10bit SiteAdress + 6bit EncoderAdress)
    // byte 3   = SQC (Sequence Counter 0x00 = not used)
    // byte 4   = MFL (Message Field Length),
    if (len >= mtext[4] + 7) {    // check complete length
    // byte 5   = MEC (0xda for Rass)
    // byte 6   = MEL
        if (mtext[6] == 0 || mtext[6] > mtext[4] - 2) {
            if ((S_Verbose & 0x0f) >= 1) {
                printf("Rass-Error: Length=0 or not correct (MFL= %d, MEL= %d)\n", mtext[4], mtext[6]);
            }
            return;
        }
        // byte 7+8   = Service-ID zugehöriger Datenkanal
        // byte 9-11  = Nummer aktuelles Paket, <PNR>
        uint plfd = mtext[11] | mtext[10] << 8 | mtext[9] << 16;
        // byte 12-14 = Anzahl Pakete, <NOP>
        uint pmax = mtext[14] | mtext[13] << 8 | mtext[12] << 16;

        // byte 15+16 = Rass-Kennung = Header, <Rass-STA>
        if (mtext[15] == 0x40 && mtext[16] == 0xda) {       // first
        // byte 17+18 = Anzahl Dateien im Archiv, <NOI>
            afiles = mtext[18] | mtext[17] << 8;
            // byte 19+20 = Slide-Nummer, <Rass-ID>
            slidenumr = mtext[20] | mtext[19] << 8;
            // byte 21+22 = Element-Nummer im Slide, <INR>
            slideelem = mtext[22] | mtext[21] << 8;
            // byte 23    = Slide-Steuerbyte, <Cntrl-Byte>: bit0 = Anzeige, bit1 = Speichern, bit2 = DarfAnzeige bei Senderwechsel, bit3 = Löschen
            slideshow = mtext[23] & 0x01;
            slidesave = mtext[23] & 0x02;
            slidecan = mtext[23] & 0x04;
            slidedel = mtext[23] & 0x08;
            // byte 24    = Dateiart, <Item-Type>: 0=unbekannt/1=MPEG-Still/2=Definition
            filetype = mtext[24];
            if (filetype != 1 && filetype != 2) {
                if ((S_Verbose & 0x0f) >= 1) {
                    printf("Rass-Error: Filetype '%d' unknown !\n", filetype);
                }
                //return;
            }
            // byte 25-28 = Dateilänge, <Item-Length>
            filemax = mtext[28] | mtext[27] << 8 | mtext[26] << 16 | mtext[25] << 24;
            if (filemax >= 65536) {
                if ((S_Verbose & 0x0f) >= 1) {
                    printf("Rass-Error: Filesize '%d' will be too big !\n", filemax);
                }
                return;
            }
            // byte 29-31 = Dateioffset Paketnr old, now <rfu>
            // byte 32 = Dateioffset Bytenr old, now <rfu>
            if ((S_Verbose & 0x10) > 0) {
                printf("Rass-Header: afiles= %d, slidenumr= %d, slideelem= %d\n             slideshow= %d, -save= %d, -canschow= %d, -delete= %d\n             filetype= %d, filemax= %d\n",
                        afiles, slidenumr, slideelem, slideshow, slidesave, slidecan, slidedel, filetype, filemax);
                printf("Rass-Start ...\n");
            }
            start = true;
            index = 0;
            for (int i = 33; i < len - 2; i++) {
                if (index < filemax) {
                    daten[index++] = mtext[i];
                }
                else {
                    start = false;
                }
            }
            splfd = plfd;
        }

        else if (plfd < pmax && plfd == splfd + 1) {      // Between
            splfd = plfd;
            if (start) {
                for (int i = 15; i < len - 2; i++) {
                    if (index < filemax) {
                        daten[index++] = mtext[i];
                    }
                    else {
                        start = false;
                    }
                }
            }
        }

        else if (plfd == pmax && plfd == splfd + 1) {     // Last
            if (start) {
                for (int i = 15; i < len - 4; i++) {
                    if (index <= filemax) {
                        daten[index++] = mtext[i];
                    }
                    else {
                        start = false;
                        return;
                    }
                }
                if ((S_Verbose & 0x10) > 0) {
                    printf("... Rass-End (%d bytes)\n", index);
                }
            }
            if (filemax > 0) {      // nothing todo, if 0 byte file
                // crc-check with bytes 'len-4/3'
                unsigned short crc16 = crc16_ccitt(daten, filemax, false);
                if (crc16 != (mtext[len - 4] << 8) + mtext[len - 3]) {
                    if ((S_Verbose & 0x0f) >= 1) {
                        printf("Rass-Error: wrong CRC # calc = %04x <> transmit = %02x%02x\n", crc16, mtext[len - 4], mtext[len - 3]);
                    }
                    start = false;
                    return;
                }
            }
            // show & save file ?
            if (index == filemax && enforce_directory(DataDir)) {
                if (slideshow || (slidecan && Rass_Show == -1)) {
                    if (filetype == 1) {    // show only mpeg-still
                        char *filepath;
                        asprintf(&filepath, "%s/%s", DataDir, "Rass_show.mpg");
                        if ((fd = fopen(filepath, "wb")) != NULL) {
                            fwrite(daten, 1, filemax, fd);
                            //fflush(fd);       // for test in replaymode
                            fclose(fd);
                            Rass_Show = 1;
                            if ((S_Verbose & 0x10) > 0) {
                                printf("Rass-File: ready for displaying :-)\n");
                            }
                        }
                        else {
                            esyslog("radio: ERROR writing Rass-imagefile failed '%s'", filepath);
                        }
                        free(filepath);
                    }
                }
                if (slidesave || slidedel || slidenumr < RASS_GALMAX) {
                    // lfd. Fotogallery 100.. ???
                    if (slidenumr >= 100 && slidenumr < RASS_GALMAX) {
                        (Rass_SlideFoto < RASS_GALMAX) ? Rass_SlideFoto++ : Rass_SlideFoto = 100;
                        slidenumr = Rass_SlideFoto;
                    }
                    //
                    char *filepath;
                    (filetype == 2) ?
                            asprintf(&filepath, "%s/Rass_%d.def", DataDir, slidenumr) :
                            asprintf(&filepath, "%s/Rass_%d.mpg", DataDir, slidenumr);
                    if ((fd = fopen(filepath, "wb")) != NULL) {
                        fwrite(daten, 1, filemax, fd);
                        fclose(fd);
                        if ((S_Verbose & 0x10) > 0)
                            printf("Rass-File: saving '%s'\n", filepath);
                        // archivemarker mpeg-stills
                        if (filetype == 1) {
                            // 0, 1000/1100/1110/1111..9000/9900/9990/9999
                            if (slidenumr == 0 || slidenumr > RASS_GALMAX) {
                                if (slidenumr == 0) {
                                    Rass_Flags[0][0] = !slidedel;
                                    (RT_Info > 0) ? : RT_Info = 0; // open RadioTextOsd for ArchivTip
                                }
                                else {
                                    int islide = (int) floor(slidenumr / 1000);
                                    for (int i = 3; i >= 0; i--) {
                                        if (fmod(slidenumr, pow(10, i)) == 0) {
                                            Rass_Flags[islide][3 - i] = !slidedel;
                                            break;
                                        }
                                    }
                                }
                            }
                            // gallery
                            else {
                                Rass_Gallery[slidenumr] = !slidedel;
                                if (!slidedel && (int) slidenumr > Rass_GalEnd)
                                    Rass_GalEnd = slidenumr;
                                if (!slidedel && (Rass_GalStart == 0 || (int) slidenumr < Rass_GalStart))
                                    Rass_GalStart = slidenumr;
                                // counter
                                Rass_GalCount = 0;
                                for (int i = Rass_GalStart; i <= Rass_GalEnd; i++) {
                                    if (Rass_Gallery[i]) {
                                        Rass_GalCount++;
                                    }
                                }
                                Rass_Flags[10][0] = (Rass_GalCount > 0);
                            }
                        }
                    }
                    else {
                        esyslog("radio: ERROR writing Rass-image/data-file failed '%s'", filepath);
                    }
                    free(filepath);
                }
            }
            start = false;
            splfd = spmax = 0;
        }

        else {
            start = false;
            splfd = spmax = 0;
        }
    }

    else {
        start = false;
        splfd = spmax = 0;
        if ((S_Verbose & 0x0f) >= 1) {
            printf("RDS-Error: [Rass] Length not correct (MFL= %d, len= %d)\n", mtext[4], len);
        }
    }
}

void cRadioAudio::EnableRadioTextProcessing(const char *Titel, int apid, bool replay) {
    RT_Titel[0] = '\0';
    strncpy(RT_Titel, Titel, sizeof(RT_Titel));
    audiopid = apid;
    RT_Replay = replay;
    ARec_Receive = ARec_Record = false;

    first_packets = 0;
    enabled = true;
    free(bitrate);
    asprintf(&bitrate, "...");

    // Radiotext init
    if (S_RtFunc >= 1) {
        RT_MsgShow = RT_PlusShow = RdsLogo = RTP_TToggle = false;
        RT_ReOpen = true;
        RT_OsdTO = false;
        RT_Index = RT_PTY = 0;
        RTP_ItemToggle = 1;
        for (int i = 0; i < RT_ROWS; i++)
            memset(RT_Text[i], 0x20, RT_MEL - 1);
        sprintf(RTP_Title, "---");
        sprintf(RTP_Artist, "---");
        sprintf(RTP_Composer, "");
        sprintf(RTP_Album, "");
        sprintf(RTP_Conductor, "");
        sprintf(RTP_Band, "");

        RTP_Starttime = time(NULL);
        RT_Charset = 0;
        //
        RDS_PSShow = false;
        RDS_PSIndex = 0;
        for (int i = 0; i < 12; i++)
            memset(RDS_PSText[i], 0x20, 8);
    }
    // ...Memory
    memset(&rtp_content, 0, sizeof(rtp_content));
    rtp_content.start = time(NULL);

    rtp_content.radiotext.index = -1;
    rtp_content.items.index = -1;
    rtp_content.items.itemMask = I_MASK;
    rtp_content.info_News.index = -1;       // 12
    rtp_content.info_Stock.index = -1;      // 14
    rtp_content.info_Sport.index = -1;      // 15
    rtp_content.info_Lottery.index = -1;    // 16
    rtp_content.info_Weather.index = -1;    // 25
    rtp_content.info_Other.index = -1;      // 29

    // Rass init
    Rass_Show = Rass_Archiv = -1;
    for (int i = 0; i <= 10; i++) {
        for (int ii = 0; ii < 4; ii++) {
            Rass_Flags[i][ii] = false;
        }
    }
    Rass_GalStart = Rass_GalEnd = Rass_GalCount = 0;
    for (int i = 0; i < RASS_GALMAX; i++) {
        Rass_Gallery[i] = false;
    }
    Rass_SlideFoto = 99;
    //
    InfoRequest = false;

    if (S_RtFunc < 1) {
        return;
    }

    // RDS-Receiver for seperate Data-PIDs, only Livemode
    if (!replay) {
        rdsdevice = cDevice::ActualDevice();
        EnableRdsPidFilter(chan->Sid());
    }
}

void cRadioAudio::EnableRdsPidFilter(int Sid) {
    if (rdsdevice) {
        rdsdevice->AttachFilter(&rdsPidFilter);
        rdsPidFilter.Trigger(Sid);
    }
}

void cRadioAudio::DisableRdsPidFilter(void) {
    if (rdsdevice) {
        rdsPidFilter.Trigger();
        rdsdevice->Detach(&rdsPidFilter);
    }
}

void cRadioAudio::EnableRdsReceiver(int Pid) {
    if (rdsdevice) {
        if (RDSReceiver)
            DisableRdsReceiver();
        RDSReceiver = new cRDSReceiver(Pid);
        rdsdevice->AttachReceiver(RDSReceiver);
    }
}

void cRadioAudio::DisableRdsReceiver(void) {
    if (rdsdevice) {
        if (RDSReceiver) {
            rdsdevice->Detach(RDSReceiver);
            delete RDSReceiver;
            RDSReceiver = NULL;
        }
    }
}

void cRadioAudio::HandleRdsPids(const int *RdsPids, int NumRdsPids) {
    if (chan) {
        const int *a = chan->Apids();
        const int *d = chan->Dpids();

        for (int i = 0; i < NumRdsPids; i++) {
            int pid = RdsPids[i];
            bool found = false;
            for (int j = 0; !found && a[j]; j++)
                found = (a[j] == pid);
            for (int j = 0; !found && d[j]; j++)
                found = (d[j] == pid);
            if ((S_Verbose & 0x0f) >= 1)
                dsyslog("%s: RDS Pid %d - in %s-stream", __func__, pid, found ? "Audio" : "separate Data");
            if (!found) { // seperate RDS-stream
                EnableRdsReceiver(pid);
                break;
            }
        }
    }
}

void cRadioAudio::DisableRadioTextProcessing() {
    RT_Replay = enabled = false;

    // Radiotext & Rass
    RT_Info = -1;
    RT_ReOpen = false;
    Rass_Show = Rass_Archiv = -1;
    Rass_GalStart = Rass_GalEnd = Rass_GalCount = 0;

    if (RadioTextOsd != NULL) {
        RadioTextOsd->Hide();
    }

    if (rdsdevice) { // live
        DisableRdsReceiver();
        DisableRdsPidFilter();
        rdsdevice = NULL;
    }
}

// --- cRadioTextOsd ------------------------------------------------------

cBitmap cRadioTextOsd::rds(rds_xpm);
cBitmap cRadioTextOsd::arec(arec_xpm);
cBitmap cRadioTextOsd::rass(rass_xpm);
cBitmap cRadioTextOsd::index(index_xpm);
cBitmap cRadioTextOsd::radio(radio_xpm);
cBitmap cRadioTextOsd::marker(marker_xpm);
cBitmap cRadioTextOsd::page1(page1_xpm);
cBitmap cRadioTextOsd::pages2(pages2_xpm);
cBitmap cRadioTextOsd::pages3(pages3_xpm);
cBitmap cRadioTextOsd::pages4(pages4_xpm);
cBitmap cRadioTextOsd::no0(no0_xpm);
cBitmap cRadioTextOsd::no1(no1_xpm);
cBitmap cRadioTextOsd::no2(no2_xpm);
cBitmap cRadioTextOsd::no3(no3_xpm);
cBitmap cRadioTextOsd::no4(no4_xpm);
cBitmap cRadioTextOsd::no5(no5_xpm);
cBitmap cRadioTextOsd::no6(no6_xpm);
cBitmap cRadioTextOsd::no7(no7_xpm);
cBitmap cRadioTextOsd::no8(no8_xpm);
cBitmap cRadioTextOsd::no9(no9_xpm);
cBitmap cRadioTextOsd::bok(bok_xpm);
cBitmap cRadioTextOsd::pageE(pageE_xpm);

cRadioTextOsd::cRadioTextOsd() :
        cCharSetConv((RT_Charset == 0) ? "ISO-8859-1" : NULL, cCharSetConv::SystemCharacterTable()) {
    RadioTextOsd = this;
    osd = NULL;
    qosd = NULL;
    qiosd = NULL;
    rtclosed = rassclosed = false;
    RT_ReOpen = false;
    ftext = NULL;
    ftitel = NULL;
    LastKey = kNone;
    fheight = 0;
    bheight = 0;
}

cRadioTextOsd::~cRadioTextOsd() {
    if (Rass_Archiv >= 0) {
        if (!RT_Replay) {
            Rass_Archiv = RassImage(-1, -1, false);
        }
        else {
            Rass_Archiv = -1;
            RadioImage->SetBackgroundImage(ReplayFile);
        }
    }

    if (osd != NULL) {
        delete osd;
        osd = NULL;
    }
    if (qosd != NULL) {
        delete qosd;
        qosd = NULL;
    }
    if (qiosd != NULL) {
        delete qiosd;
        qiosd = NULL;
    }
    RadioTextOsd = NULL;
    RT_ReOpen = !RT_OsdTO;

    cRemote::Put(LastKey);
}

#define OSD_TAGROWS 6

void cRadioTextOsd::Show(void) {
    LastKey = kNone;
    RT_OsdTO = false;
    osdtimer.Set();
    ftext = cFont::GetFont(fontSml);
    fheight = ftext->Height() + 4;
    bheight = (S_RtOsdTags >= 1) ? fheight * (S_RtOsdRows + 1 + OSD_TAGROWS) : fheight * (S_RtOsdRows + 1);
    bheight += 20;

    RTp_Titel[0] = '\0';
    snprintf(RTp_Titel, sizeof(RTp_Titel), "%s - %s", InfoRequest ? tr("ext. Info") : tr("RTplus"), RT_Titel);

    if (S_RtDispl >= 1 && (!Rass_Flags[0][0] || S_RassText >= 2)) { // Rass_Show == -1
        RT_MsgShow = (RT_Info >= 1);
        ShowText();
    }
}

void cRadioTextOsd::Hide(void) {
    RTOsdClose();
    RassOsdClose();
}

void cRadioTextOsd::RTOsdClose(void) {
    if (osd != NULL) {
        delete osd;
        osd = NULL;
    }
}

void cRadioTextOsd::ShowText(void) {
    char stext[OSD_TAGROWS + 1][100];
    char *ptext[OSD_TAGROWS + 1];
    int yoff = 17, ii = 1;

    if (!osd && !qosd && !Skins.IsOpen() && !cOsd::IsOpen()) {
        if (S_RtOsdPos == 1)
            osd = cOsdProvider::NewOsd(Setup.OSDLeft, Setup.OSDTop + Setup.OSDHeight - bheight);
        else
            osd = cOsdProvider::NewOsd(Setup.OSDLeft, Setup.OSDTop);
        tArea Area = { 0, 0, Setup.OSDWidth - 1, bheight - 1, 4 };
        osd->SetAreas(&Area, 1);
    }

    if (osd) {
        uint32_t bcolor, fcolor;
        int skin = theme_skin();
        ftitel = cFont::GetFont(fontOsd);
        ftext = cFont::GetFont(fontSml);
        if (S_RtOsdTitle == 1) {
            // Title
            bcolor = (S_RtSkinColor > 0) ? radioSkin[skin].clrTitleBack : (0x00FFFFFF | S_RtBgTra << 24) & rt_color[S_RtBgCol];
            fcolor = (S_RtSkinColor > 0) ? radioSkin[skin].clrTitleText : rt_color[S_RtFgCol];
            osd->DrawRectangle(0, 0, Setup.OSDWidth - 1, ftitel->Height() + 9, bcolor);
            osd->DrawEllipse(0, 0, 5, 5, 0x00000000, -2);
            osd->DrawEllipse(Setup.OSDWidth - 6, 0, Setup.OSDWidth - 1, 5, 0x00000000, -1);

            sprintf(stext[0], RT_PTY == 0 ? "%s - %s %s%s" : "%s - %s (%s)%s",
                    RT_Titel, InfoRequest ? tr("ext. Info") : tr("Radiotext"),
                    RT_PTY == 0 ? RDS_PTYN : ptynr2string(RT_PTY),
                    RT_MsgShow ? ":" : tr("  [waiting ...]"));

            osd->DrawText(4, 5, stext[0], fcolor, clrTransparent, ftitel, Setup.OSDWidth - 4, ftitel->Height());
            // Radio, RDS- or Rass-Symbol, ARec-Symbol or Bitrate
            int inloff = (ftitel->Height() + 9 - 20) / 2;
            if (Rass_Flags[0][0]) {
                osd->DrawBitmap(Setup.OSDWidth - 51, inloff, rass, bcolor, fcolor);
                if (ARec_Record)
                    osd->DrawBitmap(Setup.OSDWidth - 107, inloff, arec, bcolor, 0xFFFC1414);  // FG=Red
                else {
                    inloff = (ftitel->Height() + 9 - ftext->Height()) / 2;
                    osd->DrawText(4, inloff, RadioAudio->bitrate, fcolor, clrTransparent, ftext, Setup.OSDWidth - 59, ftext->Height(), taRight);
                }
            } else {
                if (InfoRequest && !RdsLogo) {
                    osd->DrawBitmap(Setup.OSDWidth - 72, inloff + 1, radio, fcolor, bcolor);
                    osd->DrawBitmap(Setup.OSDWidth - 48, inloff - 1, radio, fcolor, bcolor);
                }
                else {
                    osd->DrawBitmap(Setup.OSDWidth - 84, inloff, rds, bcolor, fcolor);
                }
                if (ARec_Record) {
                    osd->DrawBitmap(Setup.OSDWidth - 140, inloff, arec, bcolor, 0xFFFC1414);  // FG=Red
                }
                else {
                    inloff = (ftitel->Height() + 9 - ftext->Height()) / 2;
                    osd->DrawText(4, inloff, RadioAudio->bitrate, fcolor, clrTransparent, ftext, Setup.OSDWidth - 92, ftext->Height(), taRight);
                }
            }
        } else {
            osd->DrawRectangle(0, 0, Setup.OSDWidth - 1, ftitel->Height() + 9, 0x00000000);
        }
        // Body
        bcolor = (S_RtSkinColor > 0) ? radioSkin[skin].clrBack : (0x00FFFFFF | S_RtBgTra << 24) & rt_color[S_RtBgCol];
        fcolor = (S_RtSkinColor > 0) ? radioSkin[skin].clrText : rt_color[S_RtFgCol];

        osd->DrawRectangle(0, ftitel->Height() + 10, Setup.OSDWidth - 1, bheight - 1, bcolor);
        osd->DrawEllipse(0, bheight - 6, 5, bheight - 1, 0x00000000, -3);
        osd->DrawEllipse(Setup.OSDWidth - 6, bheight - 6, Setup.OSDWidth - 1, bheight - 1, 0x00000000, -4);
        if (S_RtOsdTitle == 1) {
            osd->DrawRectangle(5, ftitel->Height() + 9, Setup.OSDWidth - 6, ftitel->Height() + 9, fcolor);
        }
        if (RT_MsgShow) {
            // RT-Text roundloop
            int ind = (RT_Index == 0) ? S_RtOsdRows - 1 : RT_Index - 1;
            if (S_RtOsdLoop == 1) { // latest bottom
                for (int i = ind + 1; i < S_RtOsdRows; i++) {
                    osd->DrawText(5, yoff + fheight * (ii++), RT_Text[i], fcolor, clrTransparent, ftext, Setup.OSDWidth - 4, ftext->Height());
                }
                for (int i = 0; i <= ind; i++) {
                    osd->DrawText(5, yoff + fheight * (ii++), RT_Text[i], fcolor, clrTransparent, ftext, Setup.OSDWidth - 4, ftext->Height());
                }
            }
            else {          // latest top
                for (int i = ind; i >= 0; i--) {
                    osd->DrawText(5, yoff + fheight * (ii++), RT_Text[i], fcolor, clrTransparent, ftext, Setup.OSDWidth - 4, ftext->Height());
                }
                for (int i = S_RtOsdRows - 1; i > ind; i--) {
                    osd->DrawText(5, yoff + fheight * (ii++), RT_Text[i], fcolor, clrTransparent, ftext, Setup.OSDWidth - 4, ftext->Height());
                }
            }
            // + RT-Plus or PS-Text = 3 rows
            if ((S_RtOsdTags == 1 && RT_PlusShow) || S_RtOsdTags >= 2) {
                if (!RDS_PSShow || !strstr(RTP_Title, "---") || !strstr(RTP_Artist, "---")
                    || RTP_Composer[0] || RTP_Album[0] || RTP_Conductor[0] || RTP_Band[0]) {

                    int fwidth = 0;
                    int n = OSD_TAGROWS;

                    if (RTP_Composer[0])  { sprintf(stext[n], "> %s", tr("Composer :"));  fwidth = max(fwidth, ftext->Width(stext[n])); ptext[n--] = RTP_Composer; }
                    if (RTP_Conductor[0]) { sprintf(stext[n], "> %s", tr("Conductor :")); fwidth = max(fwidth, ftext->Width(stext[n])); ptext[n--] = RTP_Conductor; }
                    if (RTP_Band[0])      { sprintf(stext[n], "> %s", tr("Band :"));      fwidth = max(fwidth, ftext->Width(stext[n])); ptext[n--] = RTP_Band; }
                    if (RTP_Album[0])     { sprintf(stext[n], "> %s", tr("Album :"));     fwidth = max(fwidth, ftext->Width(stext[n])); ptext[n--] = RTP_Album; }
                    sprintf(stext[n], "> %s", tr("Artist :")); fwidth = max(fwidth, ftext->Width(stext[n])); ptext[n--] = RTP_Artist;
                    sprintf(stext[n], "> %s", tr("Title :"));  fwidth = max(fwidth, ftext->Width(stext[n])); ptext[n--] = RTP_Title;
                    for (; n > 0; n--)    { sprintf(stext[n], ""); ptext[n] = stext[n]; }
                    fwidth += 15;

                    const char *tilde = "~";
                    int owidth = 0;

                    if (!rtp_content.items.running)
                        owidth = ftext->Width(tilde) + 5;

                    for (int j = 6, n = 1; n <= OSD_TAGROWS; j = 3, n++) {
                        osd->DrawText(4, j + yoff + fheight * (ii), stext[n], fcolor, clrTransparent, ftext, fwidth - 5, ftext->Height());
                        if (owidth && stext[n][0])
                            osd->DrawText(fwidth, j + yoff + fheight * (ii), tilde, fcolor, clrTransparent, ftext, owidth, ftext->Height());
                        osd->DrawText(fwidth + owidth, j + yoff + fheight * (ii++), ptext[n], fcolor, clrTransparent, ftext, Setup.OSDWidth - 4, ftext->Height());
                    }
                }
                else {
                    char temp[100];
                    temp[0] = '\0';
                    int ind = (RDS_PSIndex == 0) ? 11 : RDS_PSIndex - 1;
                    for (int i = ind + 1; i < 12; i++) {
                        snprintf(temp, 100, "%s%s ", temp, RDS_PSText[i]);
                    }
                    for (int i = 0; i <= ind; i++) {
                        snprintf(temp, 100, "%s%s ", temp, RDS_PSText[i]);
                    }
                    snprintf(stext[1], 6 * 9, "%s", temp);
                    snprintf(stext[2], 6 * 9, "%s", temp + (6 * 9));

                    osd->DrawText(6, 6 + yoff + fheight * ii, "[", fcolor, clrTransparent, ftext, 12, ftext->Height());
                    osd->DrawText(Setup.OSDWidth - 12, 6 + yoff + fheight * ii, "]", fcolor, clrTransparent, ftext, Setup.OSDWidth - 6, ftext->Height());
                    osd->DrawText(16, 6 + yoff + fheight * (ii++), stext[1], fcolor, clrTransparent, ftext, Setup.OSDWidth - 16, ftext->Height(), taCenter);
                    osd->DrawText(6, 3 + yoff + fheight * ii, "[", fcolor, clrTransparent, ftext, 12, ftext->Height());
                    osd->DrawText(Setup.OSDWidth - 12, 3 + yoff + fheight * ii, "]", fcolor, clrTransparent, ftext, Setup.OSDWidth - 6, ftext->Height());
                    osd->DrawText(16, 3 + yoff + fheight * (ii++), stext[2], fcolor, clrTransparent, ftext, Setup.OSDWidth - 16, ftext->Height(), taCenter);
                }
            }
        }
        osd->Flush();
    }
    RT_MsgShow = false;
}

int cRadioTextOsd::RassImage(int QArchiv, int QKey, bool DirUp) {
    int i;

    if (QKey >= 0 && QKey <= 9) {
        if (QArchiv == 0) {
            (Rass_Flags[QKey][0]) ? QArchiv = QKey * 1000 : QArchiv = 0;
        }
        else if (QArchiv > 0) {
            if (floor(QArchiv / 1000) == QKey) {
                for (i = 3; i >= 0; i--) {
                    if (fmod(QArchiv, pow(10, i)) == 0)
                        break;
                }
                (i > 0) ? QArchiv += QKey * (int) pow(10, --i) : QArchiv = QKey * 1000;
                (Rass_Flags[QKey][3 - i]) ? : QArchiv = QKey * 1000;
            }
            else {
                (Rass_Flags[QKey][0]) ? QArchiv = QKey * 1000 : QArchiv = 0;
            }
        }
    }
    // Gallery
    else if (QKey > 9 && Rass_GalCount >= 0) {
        if (QArchiv < Rass_GalStart || QArchiv > Rass_GalEnd) {
            QArchiv = Rass_GalStart - 1;
        }
        if (DirUp) {
            for (i = QArchiv + 1; i <= Rass_GalEnd; i++) {
                if (Rass_Gallery[i])
                    break;
            }
            QArchiv = (i <= Rass_GalEnd) ? i : Rass_GalStart;
        }
        else {
            for (i = QArchiv - 1; i >= Rass_GalStart; i--) {
                if (Rass_Gallery[i])
                    break;
            }
            QArchiv = (i >= Rass_GalStart) ? i : Rass_GalEnd;
        }
    }

    // show mpeg-still
    char *image;
    if (QArchiv >= 0) {
        asprintf(&image, "%s/Rass_%d.mpg", DataDir, QArchiv);
    }
    else {
        asprintf(&image, "%s/Rass_show.mpg", DataDir);
    }
    RadioImage->SetBackgroundImage(image);
    free(image);

    return QArchiv;
}

void cRadioTextOsd::RassOsd(void) {
    ftext = cFont::GetFont(fontSml);
    int fh = ftext->Height();

    if (!qosd && !osd && !Skins.IsOpen() && !cOsd::IsOpen()) {
        qosd = cOsdProvider::NewOsd(Setup.OSDLeft, Setup.OSDTop + Setup.OSDHeight - (29 + 264 - 6 + 36));
        tArea Area = { 0, 0, 97, 29 + 264 + 5, 4 };
        qosd->SetAreas(&Area, 1);
    }

    if (qosd) {
        uint32_t bcolor, fcolor;
        int skin = theme_skin();
        // Logo
        bcolor = radioSkin[skin].clrTitleBack;
        fcolor = radioSkin[skin].clrTitleText;
        qosd->DrawRectangle(0, 1, 97, 29, bcolor);
        qosd->DrawEllipse(0, 0, 5, 5, 0x00000000, -2);
        qosd->DrawEllipse(92, 0, 97, 5, 0x00000000, -1);
        qosd->DrawBitmap(25, 5, rass, bcolor, fcolor);
        // Body
        bcolor = radioSkin[skin].clrBack;
        fcolor = radioSkin[skin].clrText;
        int offs = 29 + 2;
        qosd->DrawRectangle(0, offs, 97, 29 + 264 + 5, bcolor);
        qosd->DrawEllipse(0, 29 + 264, 5, 29 + 264 + 5, 0x00000000, -3);
        qosd->DrawEllipse(92, 29 + 264, 97, 29 + 264 + 5, 0x00000000, -4);
        qosd->DrawRectangle(5, 29, 92, 29, fcolor);
        // Keys+Index
        offs += 4;
        qosd->DrawBitmap(4, offs, no0, bcolor, fcolor);
        qosd->DrawBitmap(44, offs, index, bcolor, fcolor);
        qosd->DrawBitmap(4, 24 + offs, no1, bcolor, fcolor);
        qosd->DrawBitmap(4, 48 + offs, no2, bcolor, fcolor);
        qosd->DrawBitmap(4, 72 + offs, no3, bcolor, fcolor);
        qosd->DrawBitmap(4, 96 + offs, no4, bcolor, fcolor);
        qosd->DrawBitmap(4, 120 + offs, no5, bcolor, fcolor);
        qosd->DrawBitmap(4, 144 + offs, no6, bcolor, fcolor);
        qosd->DrawBitmap(4, 168 + offs, no7, bcolor, fcolor);
        qosd->DrawBitmap(4, 192 + offs, no8, bcolor, fcolor);
        qosd->DrawBitmap(4, 216 + offs, no9, bcolor, fcolor);
        qosd->DrawBitmap(4, 240 + offs, bok, bcolor, fcolor);
        // Content
        bool mark = false;
        for (int i = 1; i <= 9; i++) {
            // Pages
            if (Rass_Flags[i][0] && Rass_Flags[i][1] && Rass_Flags[i][2] && Rass_Flags[i][3]) {
                qosd->DrawBitmap(48, (i * 24) + offs, pages4, bcolor, fcolor);
            }
            else if (Rass_Flags[i][0] && Rass_Flags[i][1] && Rass_Flags[i][2]) {
                qosd->DrawBitmap(48, (i * 24) + offs, pages3, bcolor, fcolor);
            }
            else if (Rass_Flags[i][0] && Rass_Flags[i][1]) {
                qosd->DrawBitmap(48, (i * 24) + offs, pages2, bcolor, fcolor);
            }
            else if (Rass_Flags[i][0]) {
                qosd->DrawBitmap(48, (i * 24) + offs, page1, bcolor, fcolor);
            }
            // Marker
            if (floor(Rass_Archiv / 1000) == i) {
                qosd->DrawBitmap(28, (i * 24) + offs, marker, bcolor, fcolor);
                mark = true;
            }
        }
        // Gallery
        if (Rass_GalCount > 0) {
            char *temp;
            qosd->DrawBitmap(48, 240 + offs, pageE, bcolor, fcolor);
            asprintf(&temp, "%d", Rass_GalCount);
            qosd->DrawText(67, 240 + offs + (20 - fh), temp, fcolor, clrTransparent, ftext, 97, fh);
            free(temp);
        }
        // Marker gallery/index
        if (!mark) {
            if (Rass_Archiv > 0 && Rass_Archiv <= RASS_GALMAX)
                qosd->DrawBitmap(30, 240 + offs, marker, bcolor, fcolor);
            else
                qosd->DrawBitmap(28, offs, marker, bcolor, fcolor);
        }
        qosd->Flush();
    }
}

void cRadioTextOsd::RassOsdTip(void) {
    ftext = cFont::GetFont(fontSml);
    int fh = ftext->Height();

    if (!qosd && !osd && !Skins.IsOpen() && !cOsd::IsOpen()) {
        qosd = cOsdProvider::NewOsd(Setup.OSDLeft, Setup.OSDTop + Setup.OSDHeight - (29 + (2 * fh) - 6 + 36));
        tArea Area = { 0, 0, 97, 29 + (2 * fh) + 5, 4 };
        qosd->SetAreas(&Area, 1);
    }

    if (qosd) {
        uint32_t bcolor, fcolor;
        int skin = theme_skin();
        // Title
        bcolor = radioSkin[skin].clrTitleBack;
        fcolor = radioSkin[skin].clrTitleText;
        qosd->DrawRectangle(0, 0, 97, 29, bcolor);
        qosd->DrawEllipse(0, 0, 5, 5, 0x00000000, -2);
        qosd->DrawEllipse(92, 0, 97, 5, 0x00000000, -1);
        qosd->DrawBitmap(25, 5, rass, bcolor, fcolor);
        // Body
        bcolor = radioSkin[skin].clrBack;
        fcolor = radioSkin[skin].clrText;
        qosd->DrawRectangle(0, 29 + 2, 97, 29 + (2 * fh) + 5, bcolor);
        qosd->DrawEllipse(0, 29 + (2 * fh), 5, 29 + (2 * fh) + 5, 0x00000000, -3);
        qosd->DrawEllipse(92, 29 + (2 * fh), 97, 29 + (2 * fh) + 5, 0x00000000, -4);
        qosd->DrawRectangle(5, 29, 92, 29, fcolor);
        qosd->DrawText(5, 29 + 4, tr("Records"), fcolor, clrTransparent, ftext, 97, fh);
        qosd->DrawText(5, 29 + fh + 4, ".. <0>", fcolor, clrTransparent, ftext, 97, fh);
        qosd->Flush();
    }
}

void cRadioTextOsd::RassOsdClose(void) {
    if (qosd != NULL) {
        delete qosd;
        qosd = NULL;
    }
}

void cRadioTextOsd::RassImgSave(const char *size, int pos) {
    char *infile, *outfile, *cmd;
    int filenr = 0, error = 0;
    struct tm *ts, tm_store;

    if (!enforce_directory(DataDir))
        return;

    time_t t = time(NULL);
    ts = localtime_r(&t, &tm_store);
    switch (pos) {
    // all from 1-9
    case 1 ... 9:
        for (int i = 3; i >= 0; i--) {
            filenr += (int) (pos * pow(10, i));
            if (Rass_Flags[pos][3 - i]) {
                asprintf(&infile, "%s/Rass_%d.mpg", DataDir, filenr);
                asprintf(&outfile, "%s/Rass_%s-%04d_%02d%02d%02d%02d.jpg", DataDir, RT_Titel, filenr, ts->tm_mon + 1, ts->tm_mday, ts->tm_hour, ts->tm_min);
                asprintf(&cmd, "ffmpeg -i \"%s\" -s %s -f mjpeg -y \"%s\"", infile, size, outfile);
                if ((error = system(cmd)))
                    i = -1;
            }
        }
        asprintf(&cmd, "%s '%d'", tr("Rass-Image(s) saved from Archiv "), pos);
        break;
        // all from gallery
    case 10:
        for (int i = Rass_GalStart; i <= Rass_GalEnd; i++) {
            if (Rass_Gallery[i]) {
                asprintf(&infile, "%s/Rass_%d.mpg", DataDir, i);
                asprintf(&outfile, "%s/Rass_%s-Gallery%04d_%02d%02d.jpg", DataDir, RT_Titel, i, ts->tm_mon + 1, ts->tm_mday);
                asprintf(&cmd, "ffmpeg -i \"%s\" -s %s -f mjpeg -y \"%s\"", infile, size, outfile);
                if ((error = system(cmd))) {
                    i = Rass_GalEnd + 1;
                }
            }
        }
        asprintf(&cmd, "%s", tr("Rass-Image(s) saved from Gallery"));
        break;
        // single
    default:
        asprintf(&infile, "%s/Rass_%d.mpg", DataDir, Rass_Archiv);
        asprintf(&outfile, "%s/Rass_%s-%04d_%02d%02d%02d%02d.jpg", DataDir, RT_Titel, Rass_Archiv, ts->tm_mon + 1, ts->tm_mday, ts->tm_hour, ts->tm_min);
        asprintf(&cmd, "ffmpeg -i \"%s\" -s %s -f mjpeg -y \"%s\"", infile, size, outfile);
        error = system(cmd);
        asprintf(&cmd, "%s: %s", tr("Rass-Image saved"), outfile);
    }
    free(infile);

    // Info
    RassOsdClose();
    if (error) {
        asprintf(&cmd, "%s: %s", tr("Rass-Image failed"), outfile);
        Skins.Message(mtError, cmd, Setup.OSDMessageTime);
    }
    else {
        Skins.Message(mtInfo, cmd, Setup.OSDMessageTime);
    }

    free(outfile);
    free(cmd);
}

void cRadioTextOsd::rtp_print(void) {
    struct tm tm_store;
    time_t t = time(NULL);

    cMutexLock MutexLock(&rtp_content.rtpMutex);

    dsyslog(">>> %s-Memoryclasses @ %s", InfoRequest ? "Info" : "RTplus", asctime(localtime_r(&t, &tm_store)));
    dsyslog("    on '%s' since %s", RT_Titel, asctime(localtime_r(&rtp_content.start, &tm_store)));

    dsyslog("--- Item ---");
    for (int i = RTP_CLASS_ITEM_MIN; i <= RTP_CLASS_ITEM_MAX; i++) {
        if (*rtp_content.rtp_class[i])
            dsyslog("20%s : %s", rtp_class_name[i], rtp_content.rtp_class[i]);
    }
    dsyslog("--- Programme ---");
    for (int i = RTP_CLASS_PROG_MIN; i <= RTP_CLASS_PROG_MAX; i++) {
        if (*rtp_content.rtp_class[i])
            dsyslog("20%s : %s", rtp_class_name[i], rtp_content.rtp_class[i]);
    }
    dsyslog("--- Interactivity ---");
    for (int i = RTP_CLASS_IACT_MIN; i <= RTP_CLASS_IACT_MAX; i++) {
        if (*rtp_content.rtp_class[i])
            dsyslog("20%s : %s", rtp_class_name[i], rtp_content.rtp_class[i]);
    }
    dsyslog("--- Info ---");
    for (int i = RTP_CLASS_INFO_MIN; i <= RTP_CLASS_INFO_MAX; i++) {
        if (*rtp_content.rtp_class[i])
            dsyslog("20%s : %s", rtp_class_name[i], rtp_content.rtp_class[i]);
    }
    // no sorting
    for (int i = 0; i < MAX_RTPC; i++)
        if (*rtp_content.info_News.Content[i])
            dsyslog("       News[%02d]: %s", i, rtp_content.info_News.Content[i]);
    for (int i = 0; i < MAX_RTPC; i++)
        if (*rtp_content.info_Stock.Content[i])
            dsyslog("      Stock[%02d]: %s", i, rtp_content.info_Stock.Content[i]);
    for (int i = 0; i < MAX_RTPC; i++)
        if (*rtp_content.info_Sport.Content[i])
            dsyslog("      Sport[%02d]: %s", i, rtp_content.info_Sport.Content[i]);
    for (int i = 0; i < MAX_RTPC; i++)
        if (*rtp_content.info_Lottery.Content[i])
            dsyslog("    Lottery[%02d]: %s", i, rtp_content.info_Lottery.Content[i]);
    for (int i = 0; i < MAX_RTPC; i++)
        if (*rtp_content.info_Weather.Content[i])
            dsyslog("    Weather[%02d]: %s", i, rtp_content.info_Weather.Content[i]);
    for (int i = 0; i < MAX_RTPC; i++)
        if (*rtp_content.info_Other.Content[i])
            dsyslog("      Other[%02d]: %s", i, rtp_content.info_Other.Content[i]);
    /*
     dsyslog("--- Item-Playlist ---");
     // no sorting
     if (rtp_content.item_Index >= 0) {
     for (int i = 0; i < MAX_RTPC; i++) {
     if (rtp_content.item_Title[i] != NULL && rtp_content.item_Artist[i] != NULL) {
     struct tm tm_store;
     struct tm *ts = localtime_r(&rtp_content.item_Start[i], &tm_store);
     dsyslog("    [%02d]  %02d:%02d  Title: %s | Artist: %s",
     i, ts->tm_hour, ts->tm_min, rtp_content.item_Title[i], rtp_content.item_Artist[i]);
     }
     }
     }

     dsyslog("--- Last seen Radiotext ---");
     // no sorting
     if (rtp_content.radiotext.index >= 0) {
     for (int i = 0; i < 2*MAX_RTPC; i++)
     if (rtp_content.radiotext.Msg[i] != NULL) dsyslog("    [%03d]  %s", i, rtp_content.radiotext.Msg[i]);
     }
     */
    dsyslog("<<<");
}

#define rtplog 0
eOSState cRadioTextOsd::ProcessKey(eKeys Key) {
    // RTplus Infolog
    if (rtplog == 1 && (S_Verbose & 0x0f) >= 1) {
        static int ct = 0;
        if (++ct >= 60) {
            ct = 0;
            rtp_print();
        }
    }

    // check end @ replay
    if (RT_Replay) {
        int rplayCur, rplayTot;
        cControl::Control()->GetIndex(rplayCur, rplayTot, false);
        if (rplayCur >= rplayTot - 1) {
            Hide();
            return osEnd;
        }
    }

    // Timeout or no Info/Rass
    if (RT_OsdTO || (RT_OsdTOTemp > 0) || (RT_Info < 0)) {
        Hide();
        return osEnd;
    }

    eOSState state = cOsdObject::ProcessKey(Key);
    if (state != osUnknown)
        return state;

    // Key pressed ...
    if ((Key != kNone) && (Key < k_Release)) {
        if (osd) {              // Radiotext, -plus Osd
            switch (Key) {
            case kBack:
                RTOsdClose();
                rtclosed = true;
                //rassclosed = false;
                break;
            case k0:
                RTOsdClose();
                RTplus_Osd = true;
                cRemote::CallPlugin("radio");
                return osEnd;
            default:
                Hide();
                LastKey = (Key == kChanUp || Key == kChanDn) ? kNone : Key;
                return osEnd;
            }
        }
        else if (qosd && Rass_Archiv >= 0) {    // Rass-Archiv Osd
            int i, pos;
            pos = (Rass_Archiv > 0 && Rass_Archiv <= RASS_GALMAX) ? 10 : (int) floor(Rass_Archiv / 1000);
            switch (Key) {
            // back to Slideshow
            case kBlue:
            case kBack:
                if (!RT_Replay) {
                    Rass_Archiv = RassImage(-1, 0, false);
                }
                else {
                    Rass_Archiv = -1;
                    RadioImage->SetBackgroundImage(ReplayFile);
                }
                RassOsdClose();
                rassclosed = rtclosed = false;
                break;
                // Archiv-Sides
            case k0 ... k9:
                Rass_Archiv = RassImage(Rass_Archiv, Key - k0, false);
                RassOsd();
                break;
            case kOk:
                if (Rass_Flags[10][0]) {
                    Rass_Archiv = RassImage(Rass_Archiv, 10, true);
                    RassOsd();
                }
                break;
            case kLeft:
            case kRight:
                Rass_Archiv = RassImage(Rass_Archiv, pos, (Key == kRight) ? true : false);
                RassOsd();
                break;
            case kDown:
                (pos == 10) ? i = 0 : i = pos + 1;
                while (i != pos) {
                    if (Rass_Flags[i][0]) {
                        Rass_Archiv = RassImage(Rass_Archiv, i, true);
                        RassOsd();
                        return osContinue;
                    }
                    if (++i > 10) {
                        i = 0;
                    }
                }
                break;
            case kUp:
                (pos == 0) ? i = 10 : i = pos - 1;
                while (i != pos) {
                    if (Rass_Flags[i][0]) {
                        Rass_Archiv = RassImage(Rass_Archiv, i, true);
                        RassOsd();
                        return osContinue;
                    }
                    if (--i < 0) {
                        i = 10;
                    }
                }
                break;
            case kRed:
                RassImgSave("1024x576", 0);
                break;
            case kGreen:
                RassImgSave("1024x576", pos);
                break;
            case kYellow:
                break;  // todo, what ?
            default:
                Hide();
                LastKey = (Key == kChanUp || Key == kChanDn) ? kNone : Key;
                return osEnd;
            }
        }
        else if (qosd && Rass_Archiv == -1) {   // Rass-Slideshow Osd
            switch (Key) {
            // close
            case kBack:
                RassOsdClose();
                rassclosed = true;
                //rtclosed = false;
                break;
                // Archiv-Index
            case k0:
                if (Rass_Flags[0][0]) {
                    RassOsdClose();
                    Rass_Archiv = RassImage(0, 0, false);
                    RassOsd();
                }
                break;
            default:
                Hide();
                LastKey = (Key == kChanUp || Key == kChanDn) ? kNone : Key;
                return osEnd;
            }
        }
        else {                  // no RT && no Rass
            Hide();
            LastKey = (Key == kChanUp || Key == kChanDn) ? kNone : Key;
            return osEnd;
        }
    }
    // no Key pressed ...
    else if (S_RtOsdTO > 0 && osdtimer.Elapsed() / 1000 / 60 >= (uint) S_RtOsdTO) {
        RT_OsdTO = true;
        Hide();
        return osEnd;
    }
    else if (Rass_Archiv >= 0) {
        RassOsd();
    }
    else if (RT_MsgShow && !rtclosed && (!Rass_Flags[0][0] || S_RassText >= 2 || rassclosed)) { // Rass_Show == -1
        RassOsdClose();
        ShowText();
    }
    else if (Rass_Flags[0][0] && !rassclosed && (S_RassText < 2 || rtclosed)) {
        RTOsdClose();
        RassOsdTip();
    }

    return osContinue;
}

// --- cRTplusOsd ------------------------------------------------------

cRTplusOsd::cRTplusOsd(void) :
        cOsdMenu(RTp_Titel, 3, 12), cCharSetConv((RT_Charset == 0) ? "ISO-8859-1" : NULL, cCharSetConv::SystemCharacterTable()) {
    RTplus_Osd = false;

    bcount = helpmode = 0;
    listtyp[0] = tr("Radiotext");
    listtyp[1] = tr("Playlist");
    listtyp[2] = tr("News");
    listtyp[3] = tr("Sports");
    listtyp[4] = tr("Lottery");
    listtyp[5] = tr("Weather");
    listtyp[6] = tr("Stockmarket");
    listtyp[7] = tr("Other");

#define RTP_8_LISTS 8
    for (int i = 0; i < RTP_8_LISTS; i++)
        btext[i] = NULL;

    Load();
    Display();
}

cRTplusOsd::~cRTplusOsd() {
    ;
}

void cRTplusOsd::Load(void) {
    char text[80];
    struct tm tm_store;

    cMutexLock MutexLock(&rtp_content.rtpMutex);

    struct tm *ts = localtime_r(&rtp_content.start, &tm_store);
    snprintf(text, sizeof(text), "%s  %02d:%02d", InfoRequest ? tr("extra Info  since") : tr("RTplus Memory  since"), ts->tm_hour, ts->tm_min);
    Add(new cOsdItem(hk(text)));
    snprintf(text, sizeof(text), "%s", " ");
    Add(new cOsdItem(hk(text)));

    snprintf(text, sizeof(text), "-- %s --", tr("Item"));
    Add(new cOsdItem(hk(text)));
    for (int i = RTP_CLASS_ITEM_MIN; i <= RTP_CLASS_ITEM_MAX; i++) {
        if (*rtp_content.rtp_class[i]) {
            snprintf(text, sizeof(text), "\t%s:\t%s", tr(rtp_class_name[i]), rtp_content.rtp_class[i]);
            Add(new cOsdItem(hk(text)));
        }
    }
    snprintf(text, sizeof(text), "%s", " ");
    Add(new cOsdItem(hk(text)));

    snprintf(text, sizeof(text), "-- %s --", tr("Programme"));
    Add(new cOsdItem(hk(text)));
    for (int i = RTP_CLASS_PROG_MIN; i <= RTP_CLASS_PROG_MAX; i++) {
        if (*rtp_content.rtp_class[i]) {
            snprintf(text, sizeof(text), "\t%s:\t%s", tr(rtp_class_name[i]), rtp_content.rtp_class[i]);
            Add(new cOsdItem(hk(text)));
        }
    }
    snprintf(text, sizeof(text), "%s", " ");
    Add(new cOsdItem(hk(text)));

    snprintf(text, sizeof(text), "-- %s --", tr("Interactivity"));
    Add(new cOsdItem(hk(text)));
    for (int i = RTP_CLASS_IACT_MIN; i <= RTP_CLASS_IACT_MAX; i++) {
        if (*rtp_content.rtp_class[i]) {
            snprintf(text, sizeof(text), "\t%s:\t%s", tr(rtp_class_name[i]), rtp_content.rtp_class[i]);
            Add(new cOsdItem(hk(text)));
        }
    }
    snprintf(text, sizeof(text), "%s", " ");
    Add(new cOsdItem(hk(text)));

    snprintf(text, sizeof(text), "-- %s --", tr("Info"));
    Add(new cOsdItem(hk(text)));
    for (int i = RTP_CLASS_INFO_MIN; i <= RTP_CLASS_INFO_MAX; i++) {
        if (*rtp_content.rtp_class[i]) {
            snprintf(text, sizeof(text), "\t%s:\t%s", tr(rtp_class_name[i]), rtp_content.rtp_class[i]);
            Add(new cOsdItem(hk(text)));
        }
    }

    for (int i = 0; i < RTP_8_LISTS; i++)    btext[i] = NULL;
    bcount = 0;
    btext[bcount++] = listtyp[0];
    if (rtp_content.items.index >= 0)        btext[bcount++] = listtyp[1];
    if (rtp_content.info_News.index >= 0)    btext[bcount++] = listtyp[2];
    if (rtp_content.info_Sport.index >= 0)   btext[bcount++] = listtyp[3];
    if (rtp_content.info_Lottery.index >= 0) btext[bcount++] = listtyp[4];
    if (rtp_content.info_Weather.index >= 0) btext[bcount++] = listtyp[5];
    if (rtp_content.info_Stock.index >= 0)   btext[bcount++] = listtyp[6];
    if (rtp_content.info_Other.index >= 0)   btext[bcount++] = listtyp[7];

    int more = bcount;
    if (more > 0 && helpmode == 0)
        SetHelp(btext[0], btext[1], btext[2], (more > 3) ? ">>" : tr("Exit"));
    more -= 3;
    if (more > 0 && helpmode == 1)
        SetHelp("<<", btext[3], btext[4] , (more > 2) ? ">>" : tr("Exit"));
    more -= 2;
    if (more > 0 && helpmode == 2)
        SetHelp("<<", btext[5], btext[6] , (more > 2) ? ">>" : tr("Exit"));
    more -= 2;
    if (more > 0 && helpmode == 3)
        SetHelp("<<", btext[7], NULL, tr("Exit"));
}

void cRTplusOsd::Update(void) {
    Clear();
    Load();
    Display();
}

int cRTplusOsd::rtptyp(const char *btext) {
    if (btext) {
        for (int i = 0; i <= RTP_8_LISTS; i++) {
            if (btext == listtyp[i])
                return i;
        }
    }
    return -1;
}

void cRTplusOsd::rtp_fileprint(void) {
    struct tm *ts, tm_store;
    char *fname, *fpath;
    FILE *fd;
    int lfd = 0;

    if (!enforce_directory(DataDir))
        return;

    cMutexLock MutexLock(&rtp_content.rtpMutex);

    struct rtp_cached_info {
	struct rtp_info_cache *info;
	const char *name;
    };
    #define NUM_INFOS 6
    struct rtp_cached_info rtp_info[NUM_INFOS] = {
	{ &rtp_content.info_News,    "News"},
	{ &rtp_content.info_Stock,   "Stockmarket"},
	{ &rtp_content.info_Sport,   "Sports"},
	{ &rtp_content.info_Lottery, "Lottery"},
	{ &rtp_content.info_Weather, "Weather"},
	{ &rtp_content.info_Other,   "Other"},
    };

    time_t t = time(NULL);
    ts = localtime_r(&t, &tm_store);
    asprintf(&fname, "%s_%s_%04d-%02d-%02d.%02d.%02d", InfoRequest ? "Info" : "RTplus", RT_Titel, ts->tm_year + 1900, ts->tm_mon + 1, ts->tm_mday, ts->tm_hour, ts->tm_min);
    asprintf(&fpath, "%s/%s", DataDir, fname);
    if ((fd = fopen(fpath, "w")) != NULL) {

        fprintf(fd, ">>> %s-Memoryclasses @ %s", InfoRequest ? "Info" : "RTplus", asctime(localtime_r(&t, &tm_store)));
        fprintf(fd, "    on '%s' since %s", RT_Titel, asctime(localtime_r(&rtp_content.start, &tm_store)));

        fprintf(fd, "--- Programme ---\n");
        for (int i = RTP_CLASS_PROG_MIN; i <= RTP_CLASS_PROG_MAX; i++) {
            if (*rtp_content.rtp_class[i])
                fprintf(fd, "%18s: %s\n", rtp_class_name[i], rtp_content.rtp_class[i]);
        }
        fprintf(fd, "--- Interactivity ---\n");
        for (int i = RTP_CLASS_IACT_MIN; i <= RTP_CLASS_IACT_MAX; i++) {
            if (*rtp_content.rtp_class[i])
                fprintf(fd, "%18s: %s\n", rtp_class_name[i], rtp_content.rtp_class[i]);
        }
        fprintf(fd, "--- Info ---\n");
        for (int i = RTP_CLASS_INFO_MIN; i <= RTP_CLASS_INFO_MAX; i++) {
            if (*rtp_content.rtp_class[i])
                fprintf(fd, "%18s: %s\n", rtp_class_name[i], rtp_content.rtp_class[i]);
        }

        if (rtp_content.items.index >= 0) {
            fprintf(fd, "--- Item-Playlist ---\n");
            int ind = rtp_content.items.index;
            for (int i = ind + 1; true; i++) {
                if (i >= MAX_RTPC || (!*rtp_content.items.Item[i].Title && i > ind))
                    i = 0;
                if (*rtp_content.items.Item[i].Title && *rtp_content.items.Item[i].Artist) {
                    ts = localtime_r(&rtp_content.items.Item[i].start, &tm_store);
                    fprintf(fd, "    %02d:%02d  Title: '%s' | Artist: '%s'\n", ts->tm_hour, ts->tm_min, rtp_content.items.Item[i].Title, rtp_content.items.Item[i].Artist);
                }
                if (i == ind) break;
            }
        }

        for (int i = 0; i < NUM_INFOS; i++) {
            struct rtp_cached_info info = rtp_info[i];
            struct rtp_info_cache *cCache = info.info;

            if (cCache->index >= 0) {
                fprintf(fd, "--- %s ---\n", info.name);
                int ind = cCache->index;
                for (int i = ind + 1; true; i++) {
                    if (i >= MAX_RTPC || !*cCache->Content[i])
                        i = 0;
                    if (*cCache->Content[i])
                        fprintf(fd, "    %02d. %s\n", ++lfd, cCache->Content[i]);
                    if (i == ind) break;
                }
            }
        }

        if (rtp_content.radiotext.index >= 0) {
            fprintf(fd, "--- Last seen Radiotext ---\n");
            int ind = rtp_content.radiotext.index;
            for (int i = ind + 1; true; i++) {
                if (i >= (2 * MAX_RTPC) || !*rtp_content.radiotext.Msg[i])
                    i = 0;
                if (*rtp_content.radiotext.Msg[i])
                    fprintf(fd, "    %03d. %s\n", ++lfd, rtp_content.radiotext.Msg[i]);
                if (i == ind) break;
            }
        }

        fprintf(fd, "<<<\n");
        fclose(fd);

        char *infotext;
        asprintf(&infotext, "%s: %s", InfoRequest ? tr("Info-File saved") : tr("RTplus-File saved"), fpath);
        Skins.Message(mtInfo, infotext, Setup.OSDMessageTime);
        free(infotext);
    } else
        esyslog("radio: ERROR writing RTplus-File failed '%s'", fpath);

    free(fpath);
    free(fname);
}

eOSState cRTplusOsd::ProcessKey(eKeys Key) {
    int typ, ind;
    eOSState state = cOsdMenu::ProcessKey(Key);

    if (HasSubMenu())
        return osContinue;

    if (state == osUnknown) {
        switch (Key) {
        case kBack:
        case kOk:
            return osEnd;
        case kBlue:
            if (bcount > ((helpmode * 2) + 3)) { // menu0 - 3 items
                helpmode++;
                Update();
            } else
                return osEnd;
            break;
        case k0:
            Update();
            break;
        case k8:
            rtp_fileprint();
            break;
        case kRed:
            if (helpmode > 0) {
                helpmode--;
                Update();
            }
            else if ((typ = rtptyp(btext[0])) >= 0)
                AddSubMenu(new cRTplusList(typ));
            break;
        case kGreen:
            ind = (helpmode * 2) + 1;
            if (ind < RTP_8_LISTS && (typ = rtptyp(btext[ind])) >= 0)
                AddSubMenu(new cRTplusList(typ));
            break;
        case kYellow:
            ind = (helpmode * 2) + 2;
            if (ind < RTP_8_LISTS && (typ = rtptyp(btext[ind])) >= 0)
                AddSubMenu(new cRTplusList(typ));
            break;
        default:
            state = osContinue;
        }
    }

    static int ct;
    if (++ct >= 60) {
        ct = 0;
        Update();
    }

    return state;
}

// --- cRTplusList ------------------------------------------------------

cRTplusList::cRTplusList(int Typ) :
        cOsdMenu(RTp_Titel, 4), cCharSetConv((RT_Charset == 0) ? "ISO-8859-1" : NULL, cCharSetConv::SystemCharacterTable()) {
    typ = Typ;
    refresh = false;

    Load();
    Display();
}

cRTplusList::~cRTplusList() {
    typ = 0;
}

void cRTplusList::Load(void) {
    char text[120+10+1];
    struct tm *ts, tm_store;
    int ind, lfd = 0;
    char ctitle[80];
    struct rtp_info_cache *cCache = NULL;
    const char *cName = "?";

    cMutexLock MutexLock(&rtp_content.rtpMutex);

    ts = localtime_r(&rtp_content.start, &tm_store);
    switch (typ) {
    case 0:
        snprintf(text, sizeof(text), "-- %s (max. %d) --", tr("last seen Radiotext"), 2 * MAX_RTPC);
        Add(new cOsdItem(hk(text)));
        snprintf(text, sizeof(text), "%s", " ");
        Add(new cOsdItem(hk(text)));

        if ((ind = rtp_content.radiotext.index) >= 0) {
            for (int i = ind + 1; true; i++) {
                if (i >= (2 * MAX_RTPC) || !*rtp_content.radiotext.Msg[i])
                    i = 0;
                if (*rtp_content.radiotext.Msg[i]) {
                    snprintf(text, sizeof(text), "%d.\t%s", ++lfd, rtp_content.radiotext.Msg[i]);
                    Add(new cOsdItem(hk(text)));
                }
                if (i == ind) break;
            }
        }
        break;
    case 1:
        SetCols(6, 19, 1);
        snprintf(text, sizeof(text), "-- %s --", tr("Playlist"));
        Add(new cOsdItem(hk(text)));
        snprintf(text, sizeof(text), "%s\t%s\t\t%s", tr("Time"), tr("Title"), tr("Artist"));
        Add(new cOsdItem(hk(text)));
        snprintf(text, sizeof(text), "%s", " ");
        Add(new cOsdItem(hk(text)));

        if ((ind = rtp_content.items.index) >= 0) {
            for (int i = ind + 1; true; i++) {
                if (i >= MAX_RTPC)
                    { i = -1; continue; }
                char *title = rtp_content.items.Item[i].Title;
                if (!*title) title = rtp_content.items.Item[i].Album;

                char *artist = rtp_content.items.Item[i].Artist;
                if (!*artist) artist = rtp_content.items.Item[i].Band;
                if (!*artist) artist = rtp_content.items.Item[i].Composer;
                if (!*artist) artist = rtp_content.items.Item[i].Conductor;

                if (*title || *artist) {
                    ts = localtime_r(&rtp_content.items.Item[i].start, &tm_store);
                    snprintf(ctitle, sizeof(ctitle), "%s", *title ? title : "---");
                    snprintf(text, sizeof(text), "%02d:%02d\t%s\t\t%s", ts->tm_hour, ts->tm_min, ctitle, *artist ? artist : "---");
                    Add(new cOsdItem(hk(text)));
                }
                else if (i > ind)
                    i = -1;
                if (i == ind) break;
            }
        }
        break;
    case 2:
        cName = "News";
        cCache = &rtp_content.info_News;
        break;
    case 3:
        cName = "Sports";
        cCache = &rtp_content.info_Sport;
        break;
    case 4:
        cName = "Lottery";
        cCache = &rtp_content.info_Lottery;
        break;
    case 5:
        cName = "Weather";
        cCache = &rtp_content.info_Weather;
        break;
    case 6:
        cName = "Stockmarket";
        cCache = &rtp_content.info_Stock;
        break;
    case 7:
        cName = "Other";
        cCache = &rtp_content.info_Other;
        break;
    default: ;
    }

    if (cCache) {
        SetCols(6, 1);
        snprintf(text, sizeof(text), "%s\t\t%s", tr("Time"), tr(cName));
        Add(new cOsdItem(hk(text)));
        snprintf(text, sizeof(text), "%s", " ");
        Add(new cOsdItem(hk(text)));

        if ((ind = cCache->index) >= 0) {
            for (int i = ind + 1; true; i++) {
                if (i >= MAX_RTPC || !*cCache->Content[i])
                    i = 0;
                if (*cCache->Content[i]) {
                    ts = localtime_r(&cCache->start[i], &tm_store);
                    snprintf(text, sizeof(text), "%02d:%02d\t\t%s", ts->tm_hour, ts->tm_min, cCache->Content[i]);
                    Add(new cOsdItem(hk(text)));
                }
                if (i == ind) break;
            }
        }
    }
    SetHelp(NULL, NULL, refresh ? tr("Refresh Off") : tr("Refresh On"), tr("Back"));
}

void cRTplusList::Update(void) {
    Clear();
    Load();
    Display();
}

eOSState cRTplusList::ProcessKey(eKeys Key) {
    eOSState state = cOsdMenu::ProcessKey(Key);

    if (state == osUnknown) {
        switch (Key) {
        case k0:
            Update();
            break;
        case kYellow:
            refresh = (refresh) ? false : true;
            Update();
            break;
        case kBack:
        case kOk:
        case kBlue:
            return osBack;
        default:
            state = osContinue;
        }
    }

    static int ct;
    if (refresh) {
        if (++ct >= 20) {
            ct = 0;
            Update();
        }
    }

    return state;
}

//--------------- End -----------------------------------------------------------------
