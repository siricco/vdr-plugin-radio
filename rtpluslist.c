/*
 * rtpluslist.c - part of radio.c, a plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "rtpluslist.h"
#include "radioaudio.h"

extern char RTp_Titel[80];
extern rtp_classes rtp_content;

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
    char text[2*RT_MEL+10+1];
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
                    snprintf(text, sizeof(text), "%d.\t%s", ++lfd, Convert(rtp_content.radiotext.Msg[i]));
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
                    snprintf(ctitle, sizeof(ctitle), "%s", *title ? Convert(title) : "---");
                    snprintf(text, sizeof(text), "%02d:%02d\t%s\t\t%s", ts->tm_hour, ts->tm_min, ctitle, *artist ? Convert(artist) : "---");
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
                    snprintf(text, sizeof(text), "%02d:%02d\t\t%s", ts->tm_hour, ts->tm_min, Convert(cCache->Content[i]));
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
