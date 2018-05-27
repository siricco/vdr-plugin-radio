#include <vdr/remote.h>
#include <vdr/status.h>
#include <vdr/plugin.h>
#include "radioaudio.h"
#include "radioskin.h"
#include "radiotools.h"
#include "rtplusosd.h"
#include "rtpluslist.h"
#include "service.h"
#include <math.h>

extern char *RT_Titel, *RTp_Titel;
extern rtp_classes rtp_content;

// --- cRTplusOsd ------------------------------------------------------

cRTplusOsd::cRTplusOsd(void) :
        cOsdMenu(RTp_Titel, 3, 12), cCharSetConv(
                (RT_Charset == 0) ? "ISO-8859-1" : NULL) {
    RTplus_Osd = false;

    bcount = helpmode = 0;
    listtyp[0] = tr("Radiotext");
    listtyp[1] = tr("Playlist");
    listtyp[2] = tr("Sports");
    listtyp[3] = tr("Lottery");
    listtyp[4] = tr("Weather");
    listtyp[5] = tr("Stockmarket");
    listtyp[6] = tr("Other");

    Load();
    Display();
// TODO
dsyslog("%s %d cRTplusOsd::cRTplusOsd ", __FILE__, __LINE__);
}

cRTplusOsd::~cRTplusOsd() {
}

void cRTplusOsd::Load(void) {
    char text[80];

    struct tm tm_store;
    struct tm *ts = localtime_r(&rtp_content.start, &tm_store);
    snprintf(text, sizeof(text), "%s  %02d:%02d",
            InfoRequest ? tr("extra Info  since") : tr("RTplus Memory  since"),
            ts->tm_hour, ts->tm_min);
    Add(new cOsdItem(hk(text)));
    snprintf(text, sizeof(text), "%s", " ");
    Add(new cOsdItem(hk(text)));

    snprintf(text, sizeof(text), "-- %s --", tr("Programme"));
    Add(new cOsdItem(hk(text)));
    if (rtp_content.prog_StatShort != NULL) {
        snprintf(text, sizeof(text), "\t%s:\t%s", tr("Stat.Short"),
                Convert(rtp_content.prog_StatShort));
        Add(new cOsdItem(hk(text)));
    }
    if (rtp_content.prog_Station != NULL) {
        snprintf(text, sizeof(text), "\t%s:\t%s", tr("Station"),
                Convert(rtp_content.prog_Station));
        Add(new cOsdItem(hk(text)));
    }
    if (rtp_content.prog_Now != NULL) {
        snprintf(text, sizeof(text), "\t%s:\t%s", tr("Now"),
                Convert(rtp_content.prog_Now));
        Add(new cOsdItem(hk(text)));
    }
    if (rtp_content.prog_Part != NULL) {
        snprintf(text, sizeof(text), "\t%s:\t%s", tr("...Part"),
                Convert(rtp_content.prog_Part));
        Add(new cOsdItem(hk(text)));
    }
    if (rtp_content.prog_Next != NULL) {
        snprintf(text, sizeof(text), "\t%s:\t%s", tr("Next"),
                Convert(rtp_content.prog_Next));
        Add(new cOsdItem(hk(text)));
    }
    if (rtp_content.prog_Host != NULL) {
        snprintf(text, sizeof(text), "\t%s:\t%s", tr("Host"),
                Convert(rtp_content.prog_Host));
        Add(new cOsdItem(hk(text)));
    }
    if (rtp_content.prog_EditStaff != NULL) {
        snprintf(text, sizeof(text), "\t%s:\t%s", tr("Edit.Staff"),
                Convert(rtp_content.prog_EditStaff));
        Add(new cOsdItem(hk(text)));
    }
    if (rtp_content.prog_Homepage != NULL) {
        snprintf(text, sizeof(text), "\t%s:\t%s", tr("Homepage"),
                Convert(rtp_content.prog_Homepage));
        Add(new cOsdItem(hk(text)));
    }
    snprintf(text, sizeof(text), "%s", " ");
    Add(new cOsdItem(hk(text)));

    snprintf(text, sizeof(text), "-- %s --", tr("Interactivity"));
    Add(new cOsdItem(hk(text)));
    if (rtp_content.phone_Hotline != NULL) {
        snprintf(text, sizeof(text), "\t%s:\t%s", tr("Phone-Hotline"),
                Convert(rtp_content.phone_Hotline));
        Add(new cOsdItem(hk(text)));
    }
    if (rtp_content.phone_Studio != NULL) {
        snprintf(text, sizeof(text), "\t%s:\t%s", tr("Phone-Studio"),
                Convert(rtp_content.phone_Studio));
        Add(new cOsdItem(hk(text)));
    }
    if (rtp_content.sms_Studio != NULL) {
        snprintf(text, sizeof(text), "\t%s:\t%s", tr("SMS-Studio"),
                Convert(rtp_content.sms_Studio));
        Add(new cOsdItem(hk(text)));
    }
    if (rtp_content.email_Hotline != NULL) {
        snprintf(text, sizeof(text), "\t%s:\t%s", tr("Email-Hotline"),
                Convert(rtp_content.email_Hotline));
        Add(new cOsdItem(hk(text)));
    }
    if (rtp_content.email_Studio != NULL) {
        snprintf(text, sizeof(text), "\t%s:\t%s", tr("Email-Studio"),
                Convert(rtp_content.email_Studio));
        Add(new cOsdItem(hk(text)));
    }
    snprintf(text, sizeof(text), "%s", " ");
    Add(new cOsdItem(hk(text)));

    snprintf(text, sizeof(text), "-- %s --", tr("Info"));
    Add(new cOsdItem(hk(text)));
    if (rtp_content.info_News != NULL) {
        snprintf(text, sizeof(text), "\t%s:\t%s", tr("News"),
                Convert(rtp_content.info_News));
        Add(new cOsdItem(hk(text)));
    }
    if (rtp_content.info_NewsLocal != NULL) {
        snprintf(text, sizeof(text), "\t%s:\t%s", tr("NewsLocal"),
                Convert(rtp_content.info_NewsLocal));
        Add(new cOsdItem(hk(text)));
    }
    if (rtp_content.info_DateTime != NULL) {
        snprintf(text, sizeof(text), "\t%s:\t%s", tr("DateTime"),
                Convert(rtp_content.info_DateTime));
        Add(new cOsdItem(hk(text)));
    }
    if (rtp_content.info_Traffic != NULL) {
        snprintf(text, sizeof(text), "\t%s:\t%s", tr("Traffic"),
                Convert(rtp_content.info_Traffic));
        Add(new cOsdItem(hk(text)));
    }
    if (rtp_content.info_Alarm != NULL) {
        snprintf(text, sizeof(text), "\t%s:\t%s", tr("Alarm"),
                Convert(rtp_content.info_Alarm));
        Add(new cOsdItem(hk(text)));
    }
    if (rtp_content.info_Advert != NULL) {
        snprintf(text, sizeof(text), "\t%s:\t%s", tr("Advertising"),
                Convert(rtp_content.info_Advert));
        Add(new cOsdItem(hk(text)));
    }
    if (rtp_content.info_Url != NULL) {
        snprintf(text, sizeof(text), "\t%s:\t%s", tr("Url"),
                Convert(rtp_content.info_Url));
        Add(new cOsdItem(hk(text)));
    }

    for (int i = 0; i <= 6; i++)
        btext[i] = NULL;
    bcount = 0;
    asprintf(&btext[bcount++], "%s", listtyp[0]);
    if (rtp_content.item_Index >= 0)
        asprintf(&btext[bcount++], "%s", listtyp[1]);
    if (rtp_content.info_SportIndex >= 0)
        asprintf(&btext[bcount++], "%s", listtyp[2]);
    if (rtp_content.info_LotteryIndex >= 0)
        asprintf(&btext[bcount++], "%s", listtyp[3]);
    if (rtp_content.info_WeatherIndex >= 0)
        asprintf(&btext[bcount++], "%s", listtyp[4]);
    if (rtp_content.info_StockIndex >= 0)
        asprintf(&btext[bcount++], "%s", listtyp[5]);
    if (rtp_content.info_OtherIndex >= 0)
        asprintf(&btext[bcount++], "%s", listtyp[6]);

    switch (bcount) {
    case 4:
        if (helpmode == 0)
            SetHelp(btext[0], btext[1], btext[2], ">>");
        else if (helpmode == 1)
            SetHelp("<<", btext[3], NULL, tr("Exit"));
        break;
    case 5:
        if (helpmode == 0)
            SetHelp(btext[0], btext[1], btext[2], ">>");
        else if (helpmode == 1)
            SetHelp("<<", btext[3], btext[4], tr("Exit"));
        break;
    case 6:
        if (helpmode == 0)
            SetHelp(btext[0], btext[1], btext[2], ">>");
        else if (helpmode == 1)
            SetHelp("<<", btext[3], btext[4], ">>");
        else if (helpmode == 2)
            SetHelp("<<", btext[5], NULL, tr("Exit"));
        break;
    case 7:
        if (helpmode == 0)
            SetHelp(btext[0], btext[1], btext[2], ">>");
        else if (helpmode == 1)
            SetHelp("<<", btext[3], btext[4], ">>");
        else if (helpmode == 2)
            SetHelp("<<", btext[5], btext[6], tr("Exit"));
        break;
    default:
        helpmode = 0;
        SetHelp(btext[0], btext[1], btext[2], tr("Exit"));
    }
}

void cRTplusOsd::Update(void) {
    Clear();
    Load();
    Display();
}

int cRTplusOsd::rtptyp(char *btext) {
    for (int i = 0; i <= 6; i++) {
        if (strcmp(btext, listtyp[i]) == 0)
            return i;
    }

    return -1;
}

void cRTplusOsd::rtp_fileprint(void) {
    struct tm *ts, tm_store;
    char *fname, *fpath;
    FILE *fd;
    int ind, lfd = 0;

    if (!enforce_directory(DataDir))
        return;

    time_t t = time(NULL);
    ts = localtime_r(&t, &tm_store);
    asprintf(&fname, "%s_%s_%04d-%02d-%02d.%02d.%02d",
            InfoRequest ? "Info" : "RTplus", RT_Titel, ts->tm_year + 1900,
            ts->tm_mon + 1, ts->tm_mday, ts->tm_hour, ts->tm_min);
    asprintf(&fpath, "%s/%s", DataDir, fname);
    if ((fd = fopen(fpath, "w")) != NULL) {

        fprintf(fd, ">>> %s-Memoryclasses @ %s",
                InfoRequest ? "Info" : "RTplus",
                asctime(localtime_r(&t, &tm_store)));
        fprintf(fd, "    on '%s' since %s", RT_Titel,
                asctime(localtime_r(&rtp_content.start, &tm_store)));

        fprintf(fd, "--- Programme ---\n");
        if (rtp_content.prog_StatShort != NULL)
            fprintf(fd, "StationShort: %s\n", rtp_content.prog_StatShort);
        if (rtp_content.prog_Station != NULL)
            fprintf(fd, "     Station: %s\n", rtp_content.prog_Station);
        if (rtp_content.prog_Now != NULL)
            fprintf(fd, "         Now: %s\n", rtp_content.prog_Now);
        if (rtp_content.prog_Part != NULL)
            fprintf(fd, "        Part: %s\n", rtp_content.prog_Part);
        if (rtp_content.prog_Next != NULL)
            fprintf(fd, "        Next: %s\n", rtp_content.prog_Next);
        if (rtp_content.prog_Host != NULL)
            fprintf(fd, "        Host: %s\n", rtp_content.prog_Host);
        if (rtp_content.prog_EditStaff != NULL)
            fprintf(fd, "    Ed.Staff: %s\n", rtp_content.prog_EditStaff);
        if (rtp_content.prog_Homepage != NULL)
            fprintf(fd, "    Homepage: %s\n", rtp_content.prog_Homepage);

        fprintf(fd, "--- Interactivity ---\n");
        if (rtp_content.phone_Hotline != NULL)
            fprintf(fd, "    Phone-Hotline: %s\n", rtp_content.phone_Hotline);
        if (rtp_content.phone_Studio != NULL)
            fprintf(fd, "     Phone-Studio: %s\n", rtp_content.phone_Studio);
        if (rtp_content.sms_Studio != NULL)
            fprintf(fd, "       SMS-Studio: %s\n", rtp_content.sms_Studio);
        if (rtp_content.email_Hotline != NULL)
            fprintf(fd, "    Email-Hotline: %s\n", rtp_content.email_Hotline);
        if (rtp_content.email_Studio != NULL)
            fprintf(fd, "     Email-Studio: %s\n", rtp_content.email_Studio);

        fprintf(fd, "--- Info ---\n");
        if (rtp_content.info_News != NULL)
            fprintf(fd, "         News: %s\n", rtp_content.info_News);
        if (rtp_content.info_NewsLocal != NULL)
            fprintf(fd, "    NewsLocal: %s\n", rtp_content.info_NewsLocal);
        if (rtp_content.info_DateTime != NULL)
            fprintf(fd, "     DateTime: %s\n", rtp_content.info_DateTime);
        if (rtp_content.info_Traffic != NULL)
            fprintf(fd, "      Traffic: %s\n", rtp_content.info_Traffic);
        if (rtp_content.info_Alarm != NULL)
            fprintf(fd, "        Alarm: %s\n", rtp_content.info_Alarm);
        if (rtp_content.info_Advert != NULL)
            fprintf(fd, "    Advertisg: %s\n", rtp_content.info_Advert);
        if (rtp_content.info_Url != NULL)
            fprintf(fd, "          Url: %s\n", rtp_content.info_Url);

        if (rtp_content.item_Index >= 0) {
            fprintf(fd, "--- Item-Playlist ---\n");
            ind = rtp_content.item_Index;
            if (ind < (MAX_RTPC - 1) && rtp_content.item_Title[ind + 1] != NULL) {
                for (int i = ind + 1; i < MAX_RTPC; i++) {
                    if (rtp_content.item_Title[i] != NULL
                            && rtp_content.item_Artist[i] != NULL) {
                        ts = localtime_r(&rtp_content.item_Start[i], &tm_store);
                        fprintf(fd,
                                "    %02d:%02d  Title: '%s' | Artist: '%s'\n",
                                ts->tm_hour, ts->tm_min,
                                rtp_content.item_Title[i],
                                rtp_content.item_Artist[i]);
                    }
                }
            }
            for (int i = 0; i <= ind; i++) {
                if (rtp_content.item_Title[i] != NULL
                        && rtp_content.item_Artist[i] != NULL) {
                    ts = localtime_r(&rtp_content.item_Start[i], &tm_store);
                    fprintf(fd, "    %02d:%02d  Title: '%s' | Artist: '%s'\n",
                            ts->tm_hour, ts->tm_min, rtp_content.item_Title[i],
                            rtp_content.item_Artist[i]);
                }
            }
        }

        if (rtp_content.info_SportIndex >= 0) {
            fprintf(fd, "--- Sports ---\n");
            ind = rtp_content.info_SportIndex;
            if (ind < (MAX_RTPC - 1) && rtp_content.info_Sport[ind + 1] != NULL) {
                for (int i = ind + 1; i < MAX_RTPC; i++) {
                    if (rtp_content.info_Sport[i] != NULL)
                        fprintf(fd, "    %02d. %s\n", ++lfd,
                                rtp_content.info_Sport[i]);
                }
            }
            for (int i = 0; i <= ind; i++) {
                if (rtp_content.info_Sport[i] != NULL)
                    fprintf(fd, "    %02d. %s\n", ++lfd,
                            rtp_content.info_Sport[i]);
            }
        }

        if (rtp_content.info_LotteryIndex >= 0) {
            fprintf(fd, "--- Lottery ---\n");
            ind = rtp_content.info_LotteryIndex;
            if (ind
                    < (MAX_RTPC - 1)&& rtp_content.info_Lottery[ind+1] != NULL) {
                for (int i = ind + 1; i < MAX_RTPC; i++) {
                    if (rtp_content.info_Lottery[i] != NULL)
                        fprintf(fd, "    %02d. %s\n", ++lfd,
                                rtp_content.info_Lottery[i]);
                }
            }
            for (int i = 0; i <= ind; i++) {
                if (rtp_content.info_Lottery[i] != NULL)
                    fprintf(fd, "    %02d. %s\n", ++lfd,
                            rtp_content.info_Lottery[i]);
            }
        }

        if (rtp_content.info_WeatherIndex >= 0) {
            fprintf(fd, "--- Weather ---\n");
            ind = rtp_content.info_WeatherIndex;
            if (ind
                    < (MAX_RTPC - 1)&& rtp_content.info_Weather[ind+1] != NULL) {
                for (int i = ind + 1; i < MAX_RTPC; i++) {
                    if (rtp_content.info_Weather[i] != NULL)
                        fprintf(fd, "    %02d. %s\n", ++lfd,
                                rtp_content.info_Weather[i]);
                }
            }
            for (int i = 0; i <= ind; i++) {
                if (rtp_content.info_Weather[i] != NULL)
                    fprintf(fd, "    %02d. %s\n", ++lfd,
                            rtp_content.info_Weather[i]);
            }
        }

        if (rtp_content.info_StockIndex >= 0) {
            fprintf(fd, "--- Stockmarket ---\n");
            ind = rtp_content.info_StockIndex;
            if (ind < (MAX_RTPC - 1) && rtp_content.info_Stock[ind + 1] != NULL) {
                for (int i = ind + 1; i < MAX_RTPC; i++) {
                    if (rtp_content.info_Stock[i] != NULL)
                        fprintf(fd, "    %02d. %s\n", ++lfd,
                                rtp_content.info_Stock[i]);
                }
            }
            for (int i = 0; i <= ind; i++) {
                if (rtp_content.info_Stock[i] != NULL)
                    fprintf(fd, "    %02d. %s\n", ++lfd,
                            rtp_content.info_Stock[i]);
            }
        }

        if (rtp_content.info_OtherIndex >= 0) {
            fprintf(fd, "--- Other ---\n");
            ind = rtp_content.info_OtherIndex;
            if (ind < (MAX_RTPC - 1) && rtp_content.info_Other[ind + 1] != NULL) {
                for (int i = ind + 1; i < MAX_RTPC; i++) {
                    if (rtp_content.info_Other[i] != NULL)
                        fprintf(fd, "    %02d. %s\n", ++lfd,
                                rtp_content.info_Other[i]);
                }
            }
            for (int i = 0; i <= ind; i++) {
                if (rtp_content.info_Other[i] != NULL)
                    fprintf(fd, "    %02d. %s\n", ++lfd,
                            rtp_content.info_Other[i]);
            }
        }

        fprintf(fd, "--- Last seen Radiotext ---\n");
        ind = rtp_content.rt_Index;
        if (ind < (2 * MAX_RTPC - 1) && rtp_content.radiotext[ind + 1] != NULL) {
            for (int i = ind + 1; i < 2 * MAX_RTPC; i++) {
                if (rtp_content.radiotext[i] != NULL)
                    fprintf(fd, "    %03d. %s\n", ++lfd,
                            rtp_content.radiotext[i]);
            }
        }
        for (int i = 0; i <= ind; i++) {
            if (rtp_content.radiotext[i] != NULL)
                fprintf(fd, "    %03d. %s\n", ++lfd, rtp_content.radiotext[i]);
        }

        fprintf(fd, "<<<\n");
        fclose(fd);

        char *infotext;
        asprintf(&infotext, "%s: %s",
                InfoRequest ? tr("Info-File saved") : tr("RTplus-File saved"),
                fpath);
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
            if (bcount >= 4 && helpmode == 0) {
                helpmode += 1;
                Update();
            } else if (bcount >= 6 && helpmode == 1) {
                helpmode += 1;
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
            if (helpmode == 0) {
                if (btext[0] != NULL)
                    if ((typ = rtptyp(btext[0])) >= 0)
                        AddSubMenu(new cRTplusList(typ));
            } else {
                helpmode -= 1;
                Update();
            }
            break;
        case kGreen:
            ind = (helpmode * 2) + 1;
            if (btext[ind] != NULL) {
                if ((typ = rtptyp(btext[ind])) >= 0)
                    AddSubMenu(new cRTplusList(typ));
            }
            break;
        case kYellow:
            ind = (helpmode * 2) + 2;
            if (btext[ind] != NULL) {
                if ((typ = rtptyp(btext[ind])) >= 0)
                    AddSubMenu(new cRTplusList(typ));
            }
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
