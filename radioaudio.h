/*
 * radioaudio.h - part of radio.c, a plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __RADIO_AUDIO_H
#define __RADIO_AUDIO_H

#include <linux/types.h>
#include <linux/dvb/video.h>
#include <vdr/player.h>
#include <vdr/device.h>
#include <vdr/audio.h>
#include <vdr/osd.h>
#include <vdr/menu.h>
#include <vdr/receiver.h>
#include "rdspatpmt.h"

extern char *ConfigDir;
extern char *DataDir;
extern char *ReplayFile;

#define RASS_GALMAX 999

//Setup-Params
extern int S_RtFunc;
extern int S_StillPic;
extern int S_RtOsdTitle;
extern int S_RtOsdTags;
extern int S_RtOsdPos;
extern int S_RtOsdRows;
extern int S_RtOsdLoop;
extern int S_RtOsdTO;
extern int S_RtSkinColor;
extern int S_RtBgCol;
extern int S_RtBgTra;
extern int S_RtFgCol;
extern int S_RtDispl;
extern int S_RtMsgItems;
extern int S_RassText;
extern int S_RockAnt;
extern uint32_t rt_color[9];
extern int S_Verbose;
//Radiotext
#define RT_MEL 65
#define RT_ROWS 5
extern char RT_Text[RT_ROWS][RT_MEL];
extern char RTP_Artist[RT_MEL], RTP_Title[RT_MEL], RTP_Composer[RT_MEL];
extern char RTP_Album[RT_MEL], RTP_Conductor[RT_MEL], RTP_Band[RT_MEL];
extern int RT_Info, RT_Index, RT_PTY;
extern time_t RTP_Starttime;
extern bool RT_OsdTO, RTplus_Osd, RT_ReOpen;
extern int Radio_CA;
extern int RT_OsdTOTemp;
extern int IsRadioOrReplay;
extern int RT_Charset;
// Info
extern bool InfoRequest;

void radioStatusMsg(void);

class cRadioAudio : public cAudio {
private:
    bool enabled;
    int first_packets;
    int audiopid;
    #define RDS_CHUNKSIZE 64 // assumed max. size of splitted RDS chunks in MPEG and AAC-LATM frames
    uchar rdsChunk[RDS_CHUNKSIZE];
    int maxRdsChunkIndex;
    //Radiotext
    cDevice *rdsdevice;
    cRdsPidFilter rdsPidFilter;
    void RadiotextCheckPES(const uchar *Data, int Length);
    bool ParseMpaFrameInfo(const uchar *Data, uint32_t *mpaFrameInfo, int *frameSize);
    int GetLatmRdsDSE(const uchar *plBuffer, int plBufferCnt, bool rt_start);
    void RadiotextCheckTS(const uchar *Data, int Length);
    bool RadiotextParseTS(const uchar *RdsData, int RdsLen);
    void AudioRecorderService(void);
    void RassDecode(uchar *Data, int Length);
    bool CrcOk(uchar *data);
protected:
    virtual void Play(const uchar *Data, int Length, uchar Id);
    virtual void PlayTs(const uchar *Data, int Length);
    virtual void Mute(bool On) {};
    virtual void Clear(void) {};
public:
    cRadioAudio(void);
    virtual ~cRadioAudio(void);
    void EnableRdsPidFilter(int Sid);
    void DisableRdsPidFilter(void);
    void EnableRdsReceiver(int Pid);
    void DisableRdsReceiver(void);
    void HandleRdsPids(const int *RdsPids, int NumRdsPids);
    char *bitrate;
    bool rdsSeen;
    void EnableRadioTextProcessing(const char *Titel, int apid, bool replay = false);
    void DisableRadioTextProcessing();
    void RadiotextDecode(uchar *Data);
    void RDS_PsPtynDecode(bool PTYN, uchar *Data, int Length);
};

// Radiotext-Memory RT+Classes 2.1
enum rtp_class {
    dummy_Class,        // 0
    // Item
    item_Title,         // 1
    item_Album,         // 2
    item_Track,         // 3
    item_Artist,        // 4
    item_Composition,   // 5
    item_Movement,      // 6
    item_Conductor,     // 7
    item_Composer,      // 8
    item_Band,          // 9
    item_Comment,       // 10
    item_Genre,         // 11
    // Info
    info_News,          // 12
    info_NewsLocal,     // 13
    info_Stock,         // 14
    info_Sport,         // 15
    info_Lottery,       // 16
    info_Horoskope,     // 17
    info_DailyDiversion,// 18
    info_Health,        // 19
    info_Event,         // 20
    info_Scene,         // 21
    info_Cinema,        // 22
    info_Tv,            // 23
    info_DateTime,      // 24
    info_Weather,       // 25
    info_Traffic,       // 26
    info_Alarm,         // 27
    info_Advert,        // 28
    info_Url,           // 29
    info_Other,         // 30
    // Programme
    prog_StatShort,     // 31
    prog_Station,       // 32
    prog_Now,           // 33
    prog_Next,          // 34
    prog_Part,          // 35
    prog_Host,          // 36
    prog_EditStaff,     // 37
    prog_Frequency,     // 38
    prog_Homepage,      // 39
    prog_Subchnnel,     // 40
    // Interactivity
    phone_Hotline,      // 41
    phone_Studio,       // 42
    phone_Other,        // 43
    sms_Studio,         // 44
    sms_Other,          // 45
    email_Hotline,      // 46
    email_Studio,       // 47
    email_Other,        // 48
    mms_Other,          // 49
    iact_Chat,          // 50
    iact_ChatCentre,    // 51
    iact_VoteQuestion,  // 52
    iact_VoteCentre     // 53
    // rfu        54-55
    // Private    56-58
    // Descriptor 59-63
};

extern const char *rtp_class_name[54];

#define RTP_CLASS_ITEM_MIN item_Title
#define RTP_CLASS_ITEM_MAX item_Genre
#define IS_ITEM_CLASS(c) ((c) >= RTP_CLASS_ITEM_MIN && (c) <= RTP_CLASS_ITEM_MAX)

#define RTP_CLASS_INFO_MIN info_News
#define RTP_CLASS_INFO_MAX info_Other

#define RTP_CLASS_PROG_MIN prog_StatShort
#define RTP_CLASS_PROG_MAX prog_Subchnnel

#define RTP_CLASS_IACT_MIN phone_Hotline
#define RTP_CLASS_IACT_MAX iact_VoteCentre

#define RTP_CLASS_MAX      iact_VoteCentre

#define MAX_RTPC 50
struct rt_msg_cache {
    int index;
    char Msg[2*MAX_RTPC][RT_MEL];
};

#define I_MAP(c) (1 << (c))
#define I_MASK (I_MAP(item_Title) | I_MAP(item_Album) | I_MAP(item_Artist) | I_MAP(item_Conductor) | I_MAP(item_Composer) | I_MAP(item_Band))

struct rtp_item {
    time_t start;
    uint32_t itemMap;
    char Title[RT_MEL];     // 1
    char Album[RT_MEL];     // 2
    char Artist[RT_MEL];    // 4
    char Conductor[RT_MEL]; // 7
    char Composer[RT_MEL];  // 8
    char Band[RT_MEL];      // 9
};

struct rtp_item_cache {
    int index;
    uint32_t itemMask;
    bool running;
    struct rtp_item Item[MAX_RTPC];
};

struct rtp_info_cache {
    int index;
    time_t start[MAX_RTPC];
    char Content[MAX_RTPC][RT_MEL];
};

struct rtp_classes {
    cMutex rtpMutex;
    time_t start;
    struct rt_msg_cache radiotext;
    //
    char rtp_class[RTP_CLASS_MAX+1][RT_MEL];
    //
    struct rtp_item_cache items;
    //
    struct rtp_info_cache info_News;       // 12
    struct rtp_info_cache info_Stock;      // 14
    struct rtp_info_cache info_Sport;      // 15
    struct rtp_info_cache info_Lottery;    // 16
    struct rtp_info_cache info_Weather;    // 25
    struct rtp_info_cache info_Other;      // 29
};

// plugin audiorecorder service
struct Audiorecorder_StatusRtpChannel_v1_0 {
    const cChannel *channel;
    int status;
    /*
    * 0 = channel is unknown ...
    * 1 = no receiver is attached
    * 2 = receiver is attached
    * 3 = actual recording
    */
};
extern const cChannel *chan;

#endif //__RADIO_AUDIO_H
