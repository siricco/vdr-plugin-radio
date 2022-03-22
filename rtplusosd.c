/*
 * rtplusosd.c - part of radio.c, a plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "rtplusosd.h"
#include "rtpluslist.h"
#include "radioaudio.h"
#include "radiotools.h"

extern char RT_Titel[80], RTp_Titel[80];
extern rtp_classes rtp_content;

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

    SetCols(3, 17);

    struct tm *ts = localtime_r(&rtp_content.start, &tm_store);
    snprintf(text, sizeof(text), "%s  %02d:%02d", InfoRequest ? tr("extra Info  since") : tr("RTplus Memory  since"), ts->tm_hour, ts->tm_min);
    Add(new cOsdItem(hk(text)));
    snprintf(text, sizeof(text), "%s", " ");
    Add(new cOsdItem(hk(text)));

    snprintf(text, sizeof(text), "-- %s --", tr("Item"));
    Add(new cOsdItem(hk(text)));
    for (int i = RTP_CLASS_ITEM_MIN; i <= RTP_CLASS_ITEM_MAX; i++) {
        if (*rtp_content.rtp_class[i]) {
            snprintf(text, sizeof(text), "\t%s:\t%s", class2string(i), Convert(rtp_content.rtp_class[i]));
            Add(new cOsdItem(hk(text)));
        }
    }
    snprintf(text, sizeof(text), "%s", " ");
    Add(new cOsdItem(hk(text)));

    snprintf(text, sizeof(text), "-- %s --", tr("Programme"));
    Add(new cOsdItem(hk(text)));
    for (int i = RTP_CLASS_PROG_MIN; i <= RTP_CLASS_PROG_MAX; i++) {
        if (*rtp_content.rtp_class[i]) {
            snprintf(text, sizeof(text), "\t%s:\t%s", class2string(i), Convert(rtp_content.rtp_class[i]));
            Add(new cOsdItem(hk(text)));
        }
    }
    snprintf(text, sizeof(text), "%s", " ");
    Add(new cOsdItem(hk(text)));

    snprintf(text, sizeof(text), "-- %s --", tr("Interactivity"));
    Add(new cOsdItem(hk(text)));
    for (int i = RTP_CLASS_IACT_MIN; i <= RTP_CLASS_IACT_MAX; i++) {
        if (*rtp_content.rtp_class[i]) {
            snprintf(text, sizeof(text), "\t%s:\t%s", class2string(i), Convert(rtp_content.rtp_class[i]));
            Add(new cOsdItem(hk(text)));
        }
    }
    snprintf(text, sizeof(text), "%s", " ");
    Add(new cOsdItem(hk(text)));

    snprintf(text, sizeof(text), "-- %s --", tr("Info"));
    Add(new cOsdItem(hk(text)));
    for (int i = RTP_CLASS_INFO_MIN; i <= RTP_CLASS_INFO_MAX; i++) {
        if (*rtp_content.rtp_class[i]) {
            snprintf(text, sizeof(text), "\t%s:\t%s", class2string(i), Convert(rtp_content.rtp_class[i]));
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
                fprintf(fd, "%18s: %s\n", class2string(i), rtp_content.rtp_class[i]);
        }
        fprintf(fd, "--- Interactivity ---\n");
        for (int i = RTP_CLASS_IACT_MIN; i <= RTP_CLASS_IACT_MAX; i++) {
            if (*rtp_content.rtp_class[i])
                fprintf(fd, "%18s: %s\n", class2string(i), rtp_content.rtp_class[i]);
        }
        fprintf(fd, "--- Info ---\n");
        for (int i = RTP_CLASS_INFO_MIN; i <= RTP_CLASS_INFO_MAX; i++) {
            if (*rtp_content.rtp_class[i])
                fprintf(fd, "%18s: %s\n", class2string(i), rtp_content.rtp_class[i]);
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
