#include <vdr/remote.h>
#include <vdr/status.h>
#include <vdr/plugin.h>
#include "radioaudio.h"
#include "radioskin.h"
#include "radiotools.h"
#include "service.h"
#include <math.h>

extern char *RTp_Titel;
extern rtp_classes rtp_content;

// --- cRTplusList ------------------------------------------------------

cRTplusList::cRTplusList(int Typ) :
        cOsdMenu(RTp_Titel, 4), cCharSetConv(
                (RT_Charset == 0) ? "ISO-8859-1" : NULL) {
    typ = Typ;
    refresh = false;

    Load();
    Display();
}

cRTplusList::~cRTplusList() {
    typ = 0;
}

void cRTplusList::Load(void) {
    char text[80];
    struct tm *ts, tm_store;
    int ind, lfd = 0;
    char ctitle[80];
    // TODO
    dsyslog("%s %d cRTplusList::Load", __FILE__, __LINE__);
    ts = localtime_r(&rtp_content.start, &tm_store);
    switch (typ) {
    case 0:
        snprintf(text, sizeof(text), "-- %s (max. %d) --",
                tr("last seen Radiotext"), 2 * MAX_RTPC);
        Add(new cOsdItem(hk(text)));
        snprintf(text, sizeof(text), "%s", " ");
        Add(new cOsdItem(hk(text)));
        ind = rtp_content.rt_Index;
        if (ind < (2 * MAX_RTPC - 1) && rtp_content.radiotext[ind + 1] != NULL) {
            for (int i = ind + 1; i < 2 * MAX_RTPC; i++) {
                if (rtp_content.radiotext[i] != NULL) {
                    snprintf(text, sizeof(text), "%d.\t%s", ++lfd,
                            Convert(rtp_content.radiotext[i]));
                    Add(new cOsdItem(hk(text)));
                }
            }
        }
        for (int i = 0; i <= ind; i++) {
            if (rtp_content.radiotext[i] != NULL) {
                snprintf(text, sizeof(text), "%d.\t%s", ++lfd,
                        Convert(rtp_content.radiotext[i]));
                Add(new cOsdItem(hk(text)), refresh);
            }
        }
        break;
    case 1:
        SetCols(6, 19, 1);
        snprintf(text, sizeof(text), "-- %s --", tr("Playlist"));
        Add(new cOsdItem(hk(text)));
        snprintf(text, sizeof(text), "%s\t%s\t\t%s", tr("Time"), tr("Title"),
                tr("Artist"));
        Add(new cOsdItem(hk(text)));
        snprintf(text, sizeof(text), "%s", " ");
        Add(new cOsdItem(hk(text)));
        ind = rtp_content.item_Index;
        if (ind < (MAX_RTPC - 1) && rtp_content.item_Title[ind + 1] != NULL) {
            for (int i = ind + 1; i < MAX_RTPC; i++) {
                if (rtp_content.item_Title[i] != NULL
                        && rtp_content.item_Artist[i] != NULL) {
                    ts = localtime_r(&rtp_content.item_Start[i], &tm_store);
                    snprintf(ctitle, sizeof(ctitle), "%s",
                            Convert(rtp_content.item_Title[i]));
                    snprintf(text, sizeof(text), "%02d:%02d\t%s\t\t%s",
                            ts->tm_hour, ts->tm_min, ctitle,
                            Convert(rtp_content.item_Artist[i]));
                    Add(new cOsdItem(hk(text)));
                }
            }
        }
        for (int i = 0; i <= ind; i++) {
            if (rtp_content.item_Title[i] != NULL
                    && rtp_content.item_Artist[i] != NULL) {
                ts = localtime_r(&rtp_content.item_Start[i], &tm_store);
                snprintf(ctitle, sizeof(ctitle), "%s",
                        Convert(rtp_content.item_Title[i]));
                snprintf(text, sizeof(text), "%02d:%02d\t%s\t\t%s", ts->tm_hour,
                        ts->tm_min, ctitle,
                        Convert(rtp_content.item_Artist[i]));
                Add(new cOsdItem(hk(text)), refresh);
            }
        }
        break;
    case 2:
        snprintf(text, sizeof(text), "-- %s --", tr("Sports"));
        Add(new cOsdItem(hk(text)));
        snprintf(text, sizeof(text), "%s", " ");
        Add(new cOsdItem(hk(text)));
        ind = rtp_content.info_SportIndex;
        if (ind < (MAX_RTPC - 1) && rtp_content.info_Sport[ind + 1] != NULL) {
            for (int i = ind + 1; i < MAX_RTPC; i++) {
                if (rtp_content.info_Sport[i] != NULL) {
                    snprintf(text, sizeof(text), "%d.\t%s", ++lfd,
                            Convert(rtp_content.info_Sport[i]));
                    Add(new cOsdItem(hk(text)));
                }
            }
        }
        for (int i = 0; i <= ind; i++) {
            if (rtp_content.info_Sport[i] != NULL) {
                snprintf(text, sizeof(text), "%d.\t%s", ++lfd,
                        Convert(rtp_content.info_Sport[i]));
                Add(new cOsdItem(hk(text)), refresh);
            }
        }
        break;
    case 3:
        snprintf(text, sizeof(text), "-- %s --", tr("Lottery"));
        Add(new cOsdItem(hk(text)));
        snprintf(text, sizeof(text), "%s", " ");
        Add(new cOsdItem(hk(text)));
        ind = rtp_content.info_LotteryIndex;
        if (ind < (MAX_RTPC - 1) && rtp_content.info_Lottery[ind + 1] != NULL) {
            for (int i = ind + 1; i < MAX_RTPC; i++) {
                if (rtp_content.info_Lottery[i] != NULL) {
                    snprintf(text, sizeof(text), "%d.\t%s", ++lfd,
                            Convert(rtp_content.info_Lottery[i]));
                    Add(new cOsdItem(hk(text)));
                }
            }
        }
        for (int i = 0; i <= ind; i++) {
            if (rtp_content.info_Lottery[i] != NULL) {
                snprintf(text, sizeof(text), "%d.\t%s", ++lfd,
                        Convert(rtp_content.info_Lottery[i]));
                Add(new cOsdItem(hk(text)), refresh);
            }
        }
        break;
    case 4:
        snprintf(text, sizeof(text), "-- %s --", tr("Weather"));
        Add(new cOsdItem(hk(text)));
        snprintf(text, sizeof(text), "%s", " ");
        Add(new cOsdItem(hk(text)));
        ind = rtp_content.info_WeatherIndex;
        if (ind < (MAX_RTPC - 1) && rtp_content.info_Weather[ind + 1] != NULL) {
            for (int i = ind + 1; i < MAX_RTPC; i++) {
                if (rtp_content.info_Weather[i] != NULL) {
                    snprintf(text, sizeof(text), "%d.\t%s", ++lfd,
                            Convert(rtp_content.info_Weather[i]));
                    Add(new cOsdItem(hk(text)));
                }
            }
        }
        for (int i = 0; i <= ind; i++) {
            if (rtp_content.info_Weather[i] != NULL) {
                snprintf(text, sizeof(text), "%d.\t%s", ++lfd,
                        Convert(rtp_content.info_Weather[i]));
                Add(new cOsdItem(hk(text)), refresh);
            }
        }
        break;
    case 5:
        snprintf(text, sizeof(text), "-- %s --", tr("Stockmarket"));
        Add(new cOsdItem(hk(text)));
        snprintf(text, sizeof(text), "%s", " ");
        Add(new cOsdItem(hk(text)));
        ind = rtp_content.info_StockIndex;
        if (ind < (MAX_RTPC - 1) && rtp_content.info_Stock[ind + 1] != NULL) {
            for (int i = ind + 1; i < MAX_RTPC; i++) {
                if (rtp_content.info_Stock[i] != NULL) {
                    snprintf(text, sizeof(text), "%d.\t%s", ++lfd,
                            Convert(rtp_content.info_Stock[i]));
                    Add(new cOsdItem(hk(text)));
                }
            }
        }
        for (int i = 0; i <= ind; i++) {
            if (rtp_content.info_Stock[i] != NULL) {
                snprintf(text, sizeof(text), "%d.\t%s", ++lfd,
                        Convert(rtp_content.info_Stock[i]));
                Add(new cOsdItem(hk(text)), refresh);
            }
        }
        break;
    case 6:
        snprintf(text, sizeof(text), "-- %s --", tr("Other"));
        Add(new cOsdItem(hk(text)));
        snprintf(text, sizeof(text), "%s", " ");
        Add(new cOsdItem(hk(text)));
        ind = rtp_content.info_OtherIndex;
        if (ind < (MAX_RTPC - 1) && rtp_content.info_Other[ind + 1] != NULL) {
            for (int i = ind + 1; i < MAX_RTPC; i++) {
                if (rtp_content.info_Other[i] != NULL) {
                    snprintf(text, sizeof(text), "%d.\t%s", ++lfd,
                            Convert(rtp_content.info_Other[i]));
                    Add(new cOsdItem(hk(text)));
                }
            }
        }
        for (int i = 0; i <= ind; i++) {
            if (rtp_content.info_Other[i] != NULL) {
                snprintf(text, sizeof(text), "%d.\t%s", ++lfd,
                        Convert(rtp_content.info_Other[i]));
                Add(new cOsdItem(hk(text)), refresh);
            }
        }
        break;
    }

    SetHelp(NULL, NULL, refresh ? tr("Refresh Off") : tr("Refresh On"),
            tr("Back"));
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
