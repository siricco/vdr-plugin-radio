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
#include "radiotmc.h"
#include "radioimage.h"
#include "rdsreceiver.h"
#include "radiotextosd.h"
#include "radioskin.h"
#include "radiotools.h"
#include "service.h"
#include <math.h>

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

// --- cRadioAudio -------------------------------------------------------------

bool cRadioAudio::CrcOk(uchar *data) {
        // crc16-check
        int msglen = data[4] + 4;
        unsigned short crc16 = crc16_ccitt(data, msglen, true);
        unsigned short exp = (data[msglen+1] << 8) + data[msglen + 2];
        if ((crc16 != exp) && ((S_Verbose & 0x0f) >= 1)) {
            printf("Wrong CRC # calc = %04x <> transmit = %04x Len %d\n", crc16, exp, msglen);
        }
        return (crc16 == exp);
}

cRadioAudio::cRadioAudio() :
        cAudio(), enabled(false), first_packets(0), audiopid(0),
        rdsdevice(NULL), bitrate(NULL), rdsSeen(false), maxRdsChunkIndex(RDS_CHUNKSIZE - 1) {
    RadioAudio = this;
    ResetRtpCache();
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
                        if (!CrcOk(mtext)) {
                            if ((S_Verbose & 0x0f) >= 1) {
                                printf("radioaudio: RDS-Error(PES): wrong\n");
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
                if (!CrcOk(mtext)) {
                    if ((S_Verbose & 0x0f) >= 1) {
                        dsyslog("radioaudio: RDS-Error(TS): wrong CRC\n");
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

void cRadioAudio::ResetRtpCache(void) {
    memset(&rtp_content, 0, sizeof(rtp_content));

    rtp_content.radiotext.index = -1;
    rtp_content.items.index = -1;
    rtp_content.items.itemMask = I_MASK;
    rtp_content.info_News.index = -1;       // 12
    rtp_content.info_Stock.index = -1;      // 14
    rtp_content.info_Sport.index = -1;      // 15
    rtp_content.info_Lottery.index = -1;    // 16
    rtp_content.info_Weather.index = -1;    // 25
    rtp_content.info_Other.index = -1;      // 29
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
    if (S_RtClearCache)
       ResetRtpCache();
    else { // clear only RTplus class messages from last audiostream
       for (int i = 0; i <= RTP_CLASS_MAX; i++)
           rtp_content.rtp_class[i][0] = '\0';
       }
    if (rtp_content.radiotext.index < 0) // no radiotext seen before
       rtp_content.start = time(NULL);

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

//--------------- End -----------------------------------------------------------------
