/*
 * radiotextosd.c - part of radio.c, a plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vdr/remote.h>
#include "radiotextosd.h"
#include "radioaudio.h"
#include "radioimage.h"
#include "radioskin.h"
#include "radiotools.h"

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

extern bool RT_MsgShow, RT_PlusShow;//, RTP_TToggle;
extern bool RT_Replay;
extern char RT_Titel[80], RTp_Titel[80];
extern rtp_classes rtp_content;

extern bool RDS_PSShow;
extern int RDS_PSIndex;
extern char RDS_PSText[12][9];
extern char RDS_PTYN[9];
extern bool RdsLogo;

extern bool ARec_Record;

extern int Rass_Archiv;
extern bool Rass_Flags[11][4];
extern bool Rass_Gallery[RASS_GALMAX + 1];
extern int Rass_GalStart, Rass_GalEnd, Rass_GalCount, Rass_SlideFoto;

extern cRadioImage *RadioImage;
extern cRadioAudio *RadioAudio;
extern cRadioTextOsd *RadioTextOsd;

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
    rtpRows = 0;
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
    rtpRows = -1;

    osdtimer.Set();
    ftext = cFont::GetFont(fontSml);
    fheight = ftext->Height() + 4;

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

bool cRadioTextOsd::OsdResize(void) {
    int r = 0;
    if (S_RtOsdTags >= 1) {
        r = 2;
        if (RTP_Composer[0])  r++;
        if (RTP_Conductor[0]) r++;
        if (RTP_Band[0])      r++;
        if (RTP_Album[0])     r++;
        if (r < OSD_TAGROWS)  r++; // add spacer
    }
    if (r != rtpRows) {
        //dsyslog("%s: rtpRows %d -> %d", __func__, rtpRows, r);
        rtpRows = r;
        bheight = fheight * (S_RtOsdRows + 1 + rtpRows);
        bheight += 20;
        return true;
    }
    return false;
}

void cRadioTextOsd::ShowText(void) {
    char stext[OSD_TAGROWS + 1][100];
    char *ptext[OSD_TAGROWS + 1];
    int yoff = 17, ii = 1;

    if (OsdResize())
        RTOsdClose();

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
                for (int i = ind + 1; i < S_RtOsdRows; i++, ii++) {
                    osd->DrawText(5, yoff + fheight * ii, Convert(RT_Text[i]), fcolor, clrTransparent, ftext, Setup.OSDWidth - 4, ftext->Height());
                }
                for (int i = 0; i <= ind; i++, ii++) {
                    osd->DrawText(5, yoff + fheight * ii, Convert(RT_Text[i]), fcolor, clrTransparent, ftext, Setup.OSDWidth - 4, ftext->Height());
                }
            }
            else {          // latest top
                for (int i = ind; i >= 0; i--, ii++) {
                    osd->DrawText(5, yoff + fheight * ii, Convert(RT_Text[i]), fcolor, clrTransparent, ftext, Setup.OSDWidth - 4, ftext->Height());
                }
                for (int i = S_RtOsdRows - 1; i > ind; i--, ii++) {
                    osd->DrawText(5, yoff + fheight * ii, Convert(RT_Text[i]), fcolor, clrTransparent, ftext, Setup.OSDWidth - 4, ftext->Height());
                }
            }
            // + RT-Plus or PS-Text = 3 rows
            if ((S_RtOsdTags == 1 && RT_PlusShow) || S_RtOsdTags >= 2) {
                if (!RDS_PSShow || !strstr(RTP_Title, "---") || !strstr(RTP_Artist, "---")
                    || RTP_Composer[0] || RTP_Album[0] || RTP_Conductor[0] || RTP_Band[0]) {

                    int fwidth = 0;
                    int n = rtpRows;

                    if (RTP_Composer[0])  { sprintf(stext[n], "> %s :", class2string(item_Composer));  fwidth = max(fwidth, ftext->Width(stext[n])); ptext[n--] = RTP_Composer; }
                    if (RTP_Conductor[0]) { sprintf(stext[n], "> %s :", class2string(item_Conductor)); fwidth = max(fwidth, ftext->Width(stext[n])); ptext[n--] = RTP_Conductor; }
                    if (RTP_Band[0])      { sprintf(stext[n], "> %s :", class2string(item_Band));      fwidth = max(fwidth, ftext->Width(stext[n])); ptext[n--] = RTP_Band; }
                    if (RTP_Album[0])     { sprintf(stext[n], "> %s :", class2string(item_Album));     fwidth = max(fwidth, ftext->Width(stext[n])); ptext[n--] = RTP_Album; }
                    sprintf(stext[n], "> %s :", class2string(item_Artist)); fwidth = max(fwidth, ftext->Width(stext[n])); ptext[n--] = RTP_Artist;
                    sprintf(stext[n], "> %s :", class2string(item_Title));  fwidth = max(fwidth, ftext->Width(stext[n])); ptext[n--] = RTP_Title;
                    for (; n > 0; n--)    { sprintf(stext[n], ""); ptext[n] = stext[n]; }
                    fwidth += 15;

                    const char *tilde = "~";
                    int owidth = 0;

                    if (!rtp_content.items.running)
                        owidth = ftext->Width(tilde) + 5;

                    for (int j = 6, n = 1; n <= rtpRows; j = 3, n++, ii++) {
                        osd->DrawText(4, j + yoff + fheight * ii, stext[n], fcolor, clrTransparent, ftext, fwidth - 5, ftext->Height());
                        if (owidth && stext[n][0])
                            osd->DrawText(fwidth, j + yoff + fheight * ii, tilde, fcolor, clrTransparent, ftext, owidth, ftext->Height());
                        osd->DrawText(fwidth + owidth, j + yoff + fheight * ii, Convert(ptext[n]), fcolor, clrTransparent, ftext, Setup.OSDWidth - 4, ftext->Height());
                    }
                }
                else {
                    char *temp;
                    asprintf(&temp, "%s", "");
                    int ind = (RDS_PSIndex == 0) ? 11 : RDS_PSIndex - 1;
                    for (int i = ind + 1; i < 12; i++) {
                        asprintf(&temp, "%s%s ", temp, Convert(RDS_PSText[i]));
                    }
                    for (int i = 0; i <= ind; i++) {
                        asprintf(&temp, "%s%s ", temp, Convert(RDS_PSText[i]));
                    }
                    snprintf(stext[1], 6 * 9, "%s", temp);
                    snprintf(stext[2], 6 * 9, "%s", temp + (6 * 9));
                    free(temp);

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
            dsyslog("20%s : %s", class2string(i), rtp_content.rtp_class[i]);
    }
    dsyslog("--- Programme ---");
    for (int i = RTP_CLASS_PROG_MIN; i <= RTP_CLASS_PROG_MAX; i++) {
        if (*rtp_content.rtp_class[i])
            dsyslog("20%s : %s", class2string(i), rtp_content.rtp_class[i]);
    }
    dsyslog("--- Interactivity ---");
    for (int i = RTP_CLASS_IACT_MIN; i <= RTP_CLASS_IACT_MAX; i++) {
        if (*rtp_content.rtp_class[i])
            dsyslog("20%s : %s", class2string(i), rtp_content.rtp_class[i]);
    }
    dsyslog("--- Info ---");
    for (int i = RTP_CLASS_INFO_MIN; i <= RTP_CLASS_INFO_MAX; i++) {
        if (*rtp_content.rtp_class[i])
            dsyslog("20%s : %s", class2string(i), rtp_content.rtp_class[i]);
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
