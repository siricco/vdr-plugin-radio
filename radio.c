/*
 * radio.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vdr/plugin.h>
#include <vdr/status.h>
#include <vdr/interface.h>
#include <vdr/transfer.h>
#include <string>
#include "getopt.h"
#include "radioaudio.h"
#include "radioimage.h"
#include "radiotextosd.h"
#include "rtplusosd.h"
#include "menusetupradio.h"
#include "radiocheck.h"
#include "radiotools.h"
#include "radioepg.h"
#include "inforx.h"
#include "service.h"

#if VDRVERSNUM < 10737
    #error This version of radio-plugin requires vdr >= 1.7.37
#endif

#ifndef GIT_REV
static const char *VERSION = "1.1.0";
#else
static const char *VERSION = GIT_REV;
#endif

static const char *DESCRIPTION    = trNOOP("Radio Background-Image/RDS-Text");
static const char *MAINMENUENTRY  = trNOOP("Show RDS-Radiotext");
char *ConfigDir;
char *DataDir;
char *LiveFile;
char *ReplayFile;

// Setup-Params
int S_Activate = false;
int S_StillPic = 1;
int S_HMEntry = false;
int S_RtFunc = 1;
int S_RtOsdTitle = 1;
int S_RtOsdTags = 2;
int S_RtOsdPos = 1;
int S_RtOsdRows = 2;
int S_RtOsdLoop = 0;
int S_RtOsdTO = 60;
int S_RtSkinColor = 1;
int S_RtBgCol = 0;
int S_RtBgTra = 0xA0;
int S_RtFgCol = 1;
int S_RtDispl = 1;
int S_RtMsgItems = 0;
//int S_RtpMemNo = 25;
int S_RassText = 1;
int S_ExtInfo = 0;
uint32_t rt_color[9];
int S_Verbose = 0;
int S_Encrypted = 0;
// Radiotext
char RT_Text[5][RT_MEL];
char RTP_Artist[RT_MEL], RTP_Title[RT_MEL], RTP_Composer[RT_MEL];
char RTP_Album[RT_MEL], RTP_Conductor[RT_MEL], RTP_Band[RT_MEL];
int RT_Info, RT_Index, RT_PTY;
time_t RTP_Starttime;
bool RT_OsdTO = false, RTplus_Osd = false, RT_ReOpen = false;
int RT_OsdTOTemp = 0, Radio_CA = 0;
int RT_Charset = 0;    // 0= ISO-8859-1, 1= UTF8, 2= ..
// RadioCheck
const cChannel *chan;
int IsRadioOrReplay;
// Info
bool DoInfoReq = false, InfoRequest = false;
int InfoTimeout = 3;

// --- cPluginRadio -------------------------------------------------------

class cRadioImage;
class cRadioAudio;

class cPluginRadio : public cPlugin, cStatus {
private:
    // Add any member variables or functions you may need here.
    bool ConfigDirParam;
    bool DataDirParam;
    bool LiveFileParam;
    bool ReplayFileParam;
    cRadioImage *radioImage;
    cRadioAudio *radioAudio;
public:
    cPluginRadio(void);
    virtual ~cPluginRadio();
    virtual const char *Version(void) { return VERSION; }
    virtual const char *Description(void) { return tr(DESCRIPTION); }
    virtual const char *CommandLineHelp(void);
    virtual bool ProcessArgs(int argc, char *argv[]);
    virtual bool Start(void);
    virtual void Stop(void);
    virtual void Housekeeping(void);
    virtual void MainThreadHook(void) { }
    virtual cString Active(void) { return NULL; }
    virtual const char *MainMenuEntry(void) { return (S_Activate==0 || S_RtFunc==0 || S_RtDispl==0 || S_HMEntry ? NULL : tr(MAINMENUENTRY)); }
    virtual cOsdObject *MainMenuAction(void);
    virtual cMenuSetupPage *SetupMenu(void);
    virtual bool SetupParse(const char *Name, const char *Value);
    virtual bool Service(const char *Id, void *Data);
    virtual const char **SVDRPHelpPages(void);
    virtual cString SVDRPCommand(const char *Command, const char *Option, int &ReplyCode);
protected:
    virtual void ChannelSwitch(const cDevice *Device, int ChannelNumber, bool LiveView);
    virtual void Replaying(const cControl *Control, const char *Name, const char *FileName, bool On);
};

cPluginRadio::cPluginRadio(void)
{
    // Initialize any member variables here.
    // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
    // VDR OBJECTS TO EXIST OR PROADUCE ANY OUTPUT!

    radioImage = 0;
    radioAudio = 0;

    ConfigDirParam = false;
    DataDirParam = false;
    LiveFileParam = false;
    ReplayFileParam = false;

    rt_color[0] = 0xFF000000;   //Black
    rt_color[1] = 0xFFFCFCFC;   //White
    rt_color[2] = 0xFFFC1414;   //Red
    rt_color[3] = 0xFF24FC24;   //Green
    rt_color[4] = 0xFFFCC024;   //Yellow
    rt_color[5] = 0xFFB000FC;   //Magenta
    rt_color[6] = 0xFF0000FC;   //Blue
    rt_color[7] = 0xFF00FCFC;   //Cyan
    rt_color[8] = 0x00000000;   //Transparent
}

cPluginRadio::~cPluginRadio()
{
    // Clean up after yourself!
    if (ConfigDir)  free(ConfigDir);
    if (DataDir)    free(DataDir);
    if (LiveFile)   free(LiveFile);
    if (ReplayFile) free(ReplayFile);

    cRadioCheck::Exit();
    radioImage->Exit();
}


const char *cPluginRadio::CommandLineHelp(void)
{
    // Return a string that describes all known command line options.
    return "  -f dir,   --files=dir      use dir as image directory (default: <vdrconfig>/plugins/radio)\n"
           "  -d dir,   --data=dir       use dir as temp. data directory (default: <vdrconfig>/plugins/radio)\n"
           "  -l file,  --live=file      use file as default mpegfile in livemode (default: <dir>/radio.mpg)\n"
           "  -r file,  --replay=file    use file as default mpegfile in replaymode (default: <dir>/replay.mpg)\n"
           "  -e 1,     --encrypted=1    use transfermode/backgroundimage @ encrypted radiochannels     \n"
           "  -v level, --verbose=level  set verbose level (default = 0, 0 = Off, 1 = RDS-Text+Tags,    \n"
           "                                                2 = +RDS-Telegram/Debug, 3 = +RawData 0xfd, \n"
           "                                                += 16 = Rass-Info, +=32 = TMC-Info)         \n";
}

bool cPluginRadio::ProcessArgs(int argc, char *argv[])
{
    // Implement command line argument processing here if applicable.

    static struct option long_options[] = {
            { "files",      required_argument, NULL, 'f' },
            { "data",       required_argument, NULL, 'd' },
            { "live",       required_argument, NULL, 'l' },
            { "replay",     required_argument, NULL, 'r' },
            { "encrypted",  required_argument, NULL, 'e' },
            { "verbose",    required_argument, NULL, 'v' },
            { NULL }
    };

    int c;
    while ((c = getopt_long(argc, argv, "f:d:l:r:e:v:", long_options, NULL)) != -1) {
    switch (c) {
        case 'f':
                printf("vdr-radio: arg files-dir = %s\n", optarg);
                ConfigDir = strdup(optarg);
                ConfigDirParam = true;
                break;
        case 'd':
                printf("vdr-radio: arg data-dir = %s\n", optarg);
                DataDir = strdup(optarg);
                DataDirParam = true;
                break;
        case 'l':
                printf("vdr-radio: arg live-mpeg = %s\n", optarg);
                LiveFile = strdup(optarg);
                LiveFileParam = true;
                break;
        case 'r':
                printf("vdr-radio: arg replay-mpeg = %s\n", optarg);
                ReplayFile = strdup(optarg);
                ReplayFileParam = true;
                break;
        case 'v':
                printf("vdr-radio: arg verbose = %s\n", optarg);
                if (isnumber(optarg))
                    S_Verbose = atoi(optarg);
                break;
        case 'e':
                printf("vdr-radio: arg encrypted = %s\n", optarg);
                if (isnumber(optarg))
                    S_Encrypted = atoi(optarg);
                break;
        default:
                printf("vdr-radio: arg char = %c\n", c);
                return false;
        }
    }

    return true;
}

bool cPluginRadio::Start(void)
{
    // Start any background activities the plugin shall perform.
    printf("vdr-radio: Radio-Plugin Backgr.Image/RDS-Text starts...\n");

    radioImage = new cRadioImage;
    if (!radioImage)
        return false;
    radioImage->Init();

    radioAudio = new cRadioAudio;
    if (!radioAudio)
        return false;

    if (!ConfigDirParam)
        ConfigDir = strdup(ConfigDirectory(Name()));
    if (!DataDirParam) {
        DataDir = strdup("/tmp/vdr-radio.XXXXXX");
        mkdtemp(DataDir);
        }
    if (!LiveFileParam) {
        asprintf(&LiveFile, "%s/radio.mpg", ConfigDir);
    }
    if (!ReplayFileParam) {
        asprintf(&ReplayFile, "%s/replay.mpg", ConfigDir);
    }

    cRadioCheck::Init();

    return true;
}

void cPluginRadio::Stop(void)
{
    cRadioCheck::Exit();

    if (IsRadioOrReplay > 0) {
        radioAudio->DisableRadioTextProcessing();
    }
    radioImage->Exit();
}

void cPluginRadio::Housekeeping(void)
{
    // Perform any cleanup or other regular tasks.
}

cOsdObject *cPluginRadio::MainMenuAction(void)
{
    // Perform the action when selected from the main VDR menu.
/*  if (!cDevice::PrimaryDevice()->Transferring() && !cDevice::PrimaryDevice()->Replaying()) {
        //cRemote::CallPlugin("radio"); // try again later <-- disabled, looping if activate over menu @ tv in dvb-livemode
        }   */
    if (S_Activate > 0 && S_RtFunc > 0 && S_RtDispl > 0 && IsRadioOrReplay > 0) {
        if (!RTplus_Osd) {
            cRadioTextOsd *rtosd = new cRadioTextOsd();
            return rtosd;
        } else {
            cRTplusOsd *rtposd = new cRTplusOsd();
            return rtposd;
        }
    }

    return NULL;
}

cMenuSetupPage *cPluginRadio::SetupMenu(void)
{
    // Return a setup menu in case the plugin supports one.
    return new cMenuSetupRadio;
}

bool cPluginRadio::SetupParse(const char *Name, const char *Value)
{
    // Parse your own setup parameters and store their values.
    if      (!strcasecmp(Name, "Activate"))               S_Activate = atoi(Value);
    else if (!strcasecmp(Name, "UseStillPic"))            S_StillPic = atoi(Value);
    else if (!strcasecmp(Name, "HideMenuEntry"))          S_HMEntry = atoi(Value);
    else if (!strcasecmp(Name, "RDSText-Function"))       S_RtFunc = atoi(Value);
    else if (!strcasecmp(Name, "RDSText-OsdTitle"))       S_RtOsdTitle = atoi(Value);
    else if (!strcasecmp(Name, "RDSText-OsdTags"))        S_RtOsdTags = atoi(Value);
    else if (!strcasecmp(Name, "RDSText-OsdPosition"))    S_RtOsdPos = atoi(Value);
    else if (!strcasecmp(Name, "RDSText-OsdRows")) {
       S_RtOsdRows = atoi(Value);
       if (S_RtOsdRows > RT_ROWS)  S_RtOsdRows = RT_ROWS;
       }
    else if (!strcasecmp(Name, "RDSText-OsdLooping"))     S_RtOsdLoop = atoi(Value);
    else if (!strcasecmp(Name, "RDSText-OsdSkinColor"))   S_RtSkinColor = atoi(Value);
    else if (!strcasecmp(Name, "RDSText-OsdBackgrColor")) S_RtBgCol = atoi(Value);
    else if (!strcasecmp(Name, "RDSText-OsdBackgrTrans")) S_RtBgTra = atoi(Value);
    else if (!strcasecmp(Name, "RDSText-OsdForegrColor")) S_RtFgCol = atoi(Value);
    else if (!strcasecmp(Name, "RDSText-OsdTimeout")) {
       S_RtOsdTO = atoi(Value);
       if (S_RtOsdTO > 1440)  S_RtOsdTO = 1440;
       }
    else if (!strcasecmp(Name, "RDSText-Display"))        S_RtDispl = atoi(Value);
    else if (!strcasecmp(Name, "RDSText-MsgItems"))       S_RtMsgItems = atoi(Value);
    //else if (!strcasecmp(Name, "RDSplus-MemNumber"))    S_RtpMemNo = atoi(Value);
    else if (!strcasecmp(Name, "RDSText-Rass"))           S_RassText = atoi(Value);
    else if (!strcasecmp(Name, "ExtInfo-Req"))            S_ExtInfo = atoi(Value);
    else
        return false;

    return true;
}

bool cPluginRadio::Service(const char *Id, void *Data)
{
    static struct tm tm_store;
    if ((strcmp(Id, RADIO_TEXT_SERVICE0) == 0) && (S_Activate > 0) && (S_RtFunc >= 1)) {
        if (Data) {
            RadioTextService_v1_0 *data = (RadioTextService_v1_0*)Data;
            data->rds_pty = RT_PTY;
            data->rds_info = (RT_Info < 0) ? 0 : RT_Info;
            int ind = (RT_Index == 0) ? S_RtOsdRows - 1 : RT_Index - 1;
            data->rds_text = RT_Text[ind];
            data->rds_title = RTP_Title;
            data->rds_artist = RTP_Artist;
            data->title_start = localtime_r(&RTP_Starttime, &tm_store);
        }
        return true;
    }
    else if ((strcmp(Id, RADIO_TEXT_SERVICE1) == 0) && (S_Activate > 0) && (S_RtFunc >= 1)) {
        if (Data) {
            cCharSetConv conf(RT_Charset == 0 ? "ISO-8859-1" : 0, cCharSetConv::SystemCharacterTable());
            RadioTextService_v1_1 *data = (RadioTextService_v1_1*)Data;
            int ind = (RT_Index == 0) ? S_RtOsdRows - 1 : RT_Index - 1;
            data->rds_pty = RT_PTY;
            data->rds_info = RT_Info < 0 ? 0 : RT_Info;
            data->rds_pty_info = RT_PTY > 0 ? ptynr2string(RT_PTY) : "";
            data->rds_text = conf.Convert(RT_Text[ind]);
            data->rds_title = conf.Convert(RTP_Title);
            data->rds_artist = conf.Convert(RTP_Artist);
            data->title_start = RTP_Starttime;
            data->bitrate = radioAudio->bitrate;
        }
        return true;
    }
    return false;
}

const char **cPluginRadio::SVDRPHelpPages(void)
{
    static const char *HelpPages[] = {
        "RTINFO\n"
        "  Print the radiotext information.",
        "RTCLOSE\n"
        "  Close the radiotext-osd,\n"
        "  Reopen can only be done over menu or channelswitch.",
        "RTTCLOSE\n"
        "  Close the radiotext-osd temporarily,\n"
        "  Reopen will be done after osd-messagetimeout.",
        NULL
        };

    return HelpPages;
}

cString cPluginRadio::SVDRPCommand(const char *Command, const char *Option, int &ReplyCode)
{
    if (strcasecmp(Command, "RTINFO") == 0) {
        // we use the default reply code here
        if (RT_Info == 2) {
            int ind = (RT_Index == 0) ? S_RtOsdRows - 1 : RT_Index - 1;
            return cString::sprintf(" Radiotext: %s\n RT-Title : %s\n RT-Artist: %s\n RT-Composer: %s\n", RT_Text[ind], RTP_Title, RTP_Artist, RTP_Composer);
        } else if (RT_Info == 1) {
            int ind = (RT_Index == 0) ? S_RtOsdRows - 1 : RT_Index - 1;
            return cString::sprintf(" Radiotext: %s\n", RT_Text[ind]);
        } else
            return cString::sprintf(" Radiotext not available (yet)\n");
    } else if (strcasecmp(Command, "RTCLOSE") == 0) {
        // we use the default reply code here
        if (RT_OsdTO)
            return cString::sprintf("RT-OSD already closed");
        else {
            RT_OsdTO = true;
            return cString::sprintf("RT-OSD will be closed now");
        }
    } else if (strcasecmp(Command, "RTTCLOSE") == 0) {
        // we use the default reply code here
        RT_OsdTOTemp = 2 * Setup.OSDMessageTime;
        return cString::sprintf("RT-OSD will be temporarily closed");
    }

    return NULL;
}

void cPluginRadio::ChannelSwitch(const cDevice *Device, int ChannelNumber, bool LiveView) {
    if (Device != cDevice::PrimaryDevice()) {
        return;
    }

    IsRadioOrReplay = Radio_CA = 0;
    radioAudio->DisableRadioTextProcessing();
    InfoTimeout = 3;

    if (S_Activate == false) {
        return;
    }

    char *image;
    if (cDevice::CurrentChannel() == ChannelNumber) {
#if VDRVERSNUM >= 20300
        LOCK_CHANNELS_READ
        chan = ChannelNumber ? Channels->GetByNumber(ChannelNumber) : NULL;
#else
        chan = ChannelNumber ? Channels.GetByNumber(ChannelNumber) : NULL;
#endif
        if (chan != NULL && chan->Vpid() == 0 && chan->Apid(0) > 0) {
            asprintf(&image, "%s/%s.mpg", ConfigDir, chan->Name());
            if (!file_exists(image)) {
                dsyslog("radio: channel-image not found '%s' (Channelname= %s)", image, chan->Name());
                free(image);
                asprintf(&image, "%s", LiveFile);
                if (!file_exists(image)) {
                    dsyslog("radio: live-image not found '%s' (Channelname= %s)", image, chan->Name());
                }
            }
            dsyslog("radio: [ChannelSwitch # Apid= %d, Ca= %d] channelname '%s', use image '%s'", chan->Apid(0), chan->Ca(0), chan->Name(), image);
            if ((Radio_CA = chan->Ca(0)) == 0 || S_Encrypted == 1) {
                cDevice::PrimaryDevice()->ForceTransferMode();
            }
            radioImage->SetBackgroundImage(image);
            radioAudio->EnableRadioTextProcessing(chan->Name(), chan->Apid(0), false);
            free(image);
            IsRadioOrReplay = 1;
            DoInfoReq = (S_ExtInfo > 0);
        }
    }
}

void cPluginRadio::Replaying(const cControl *Control, const char *Name, const char *FileName, bool On) {
    IsRadioOrReplay = 0;
    radioAudio->DisableRadioTextProcessing();

    if (S_Activate == false) {
        return;
    }

    bool isRadio = false;

    if (On && FileName != NULL) {
        char *vdrfile;
        // check VDR PES-Recordings
        asprintf(&vdrfile, "%s/001.vdr", FileName);
        if (file_exists(vdrfile)) {
            cFileName fn(FileName, false, true, true);
            cUnbufferedFile *f = fn.Open();
            if (f) {
                uchar b[4] = { 0x00, 0x00, 0x00, 0x00 };
                ReadFrame(f, b, sizeof(b), sizeof(b));
                fn.Close();
                isRadio = (b[0] == 0x00) && (b[1] == 0x00) && (b[2] == 0x01)
                        && (0xc0 <= b[3] && b[3] <= 0xdf);
            }
        }
        // check VDR TS-Recordings
        asprintf(&vdrfile, "%s/info", FileName);
        if (file_exists(vdrfile)) {
            cRecordingInfo rfi(FileName);
            if (rfi.Read()) {
                if (rfi.FramesPerSecond() > 0 && rfi.FramesPerSecond() < 18) {// max. seen 13.88 @ ARD-RadioTP 320k
                    isRadio = true;
                }
            }
        }
        free(vdrfile);
    }

    if (isRadio) {
        if (!file_exists(ReplayFile)) {
            dsyslog("radio: replay-image not found '%s'", ReplayFile);
        }
        else {
            radioImage->SetBackgroundImage(ReplayFile);
        }
        radioAudio->EnableRadioTextProcessing(Name, 0, true);
        IsRadioOrReplay = 2;
    }
}


VDRPLUGINCREATOR(cPluginRadio); // Don't touch this!
