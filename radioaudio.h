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
#include "radioimage.h"

extern char *ConfigDir;
extern char *DataDir;
extern char *ReplayFile;

#define RASS_GALMAX 999
extern bool Rass_Gallery[];
extern int Rass_GalStart, Rass_GalEnd, Rass_GalCount, Rass_SlideFoto;

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
extern char RT_Text[5][RT_MEL];
extern char RTP_Artist[RT_MEL], RTP_Title[RT_MEL];
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
    #define RDS_CHUNKSIZE 128 // assumed max. size of splitted RDS chunks in MPEG and AAC-LATM frames
    uchar rdsChunk[RDS_CHUNKSIZE];
    //Radiotext
    cDevice *rdsdevice;
    void RadiotextCheckPES(const uchar *Data, int Length);
    bool ParseMpaFrameInfo(const uchar *Data, uint32_t *mpaFrameInfo, int *frameSize);
    int GetLatmRdsDSE(const uchar *plBuffer, int plBufferCnt);
    void RadiotextCheckTS(const uchar *Data, int Length);
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
    char *bitrate;
    void EnableRadioTextProcessing(const char *Titel, int apid, bool replay = false);
    void DisableRadioTextProcessing();
    void RadiotextDecode(uchar *Data);
    void RDS_PsPtynDecode(bool PTYN, uchar *Data, int Length);
};

// Radiotext-Memory RT+Classes 2.1
#define MAX_RTPC 50
struct rtp_classes {
    time_t start;
    char temptext[RT_MEL];
    char *radiotext[2*MAX_RTPC];
    int rt_Index;
    // Item
    bool item_New;
    char *item_Title[MAX_RTPC];     // 1
    char *item_Artist[MAX_RTPC];    // 4    
    time_t item_Start[MAX_RTPC];
    int item_Index;
    // Info
    char *info_News;                // 12
    char *info_NewsLocal;           // 13
    char *info_Stock[MAX_RTPC];     // 14
    int info_StockIndex;
    char *info_Sport[MAX_RTPC];     // 15
    int info_SportIndex;
    char *info_Lottery[MAX_RTPC];   // 16
    int info_LotteryIndex;
    char *info_DateTime;            // 24
    char *info_Weather[MAX_RTPC];   // 25
    int info_WeatherIndex;
    char *info_Traffic;             // 26
    char *info_Alarm;               // 27
    char *info_Advert;              // 28
    char *info_Url;                 // 29
    char *info_Other[MAX_RTPC];     // 30
    int info_OtherIndex;
    // Programme
    char *prog_StatShort;           // 31
    char *prog_Station;             // 32
    char *prog_Now;                 // 33
    char *prog_Next;                // 34
    char *prog_Part;                // 35
    char *prog_Host;                // 36
    char *prog_EditStaff;           // 37
    char *prog_Homepage;            // 39
    // Interactivity
    char *phone_Hotline;            // 41
    char *phone_Studio;             // 42
    char *sms_Studio;               // 44
    char *email_Hotline;            // 46
    char *email_Studio;             // 47
// to be continue...
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
