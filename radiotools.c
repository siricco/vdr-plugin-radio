/*
 * radiotools.c - part of radio.c, a plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */
 
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <sys/stat.h>
#include <vdr/tools.h>
#include "radiotools.h"
#include "radioaudio.h"


/* ----------------------------------------------------------------------------------------------------------- */

bool file_exists(const char *filename)
{
    struct stat file_stat;

    return ((stat(filename, &file_stat) == 0) ? true : false);
}

bool enforce_directory(const char *path)
{
    struct stat sbuf;

    if (stat(path, &sbuf) != 0) {
        dsyslog("radio: creating directory %s", path);
        if (mkdir(path, ACCESSPERMS)) {
            esyslog("radio: ERROR failed to create directory %s", path);
            return false;
        }
    }
    else {
        if (!S_ISDIR(sbuf.st_mode)) {
            esyslog(
                    "radio: ERROR failed to create directory %s: file exists but is not a directory",
                    path);
            return false;
        }
    }

    return true;
}

/* ----------------------------------------------------------------------------------------------------------- */

#define crc_timetest 0
#if crc_timetest
    #include <time.h>
    #include <sys/time.h>
#endif

unsigned short crc16_ccitt(unsigned char *daten, int len, bool skipfirst)
{
#if crc_timetest
    struct timeval t;
    unsigned long long tstart = 0;
    if (gettimeofday(&t, NULL) == 0)
        tstart = t.tv_sec*1000000 + t.tv_usec;
#endif

    // CRC16-CCITT: x^16 + x^12 + x^5 + 1
    // with start 0xffff and result invers
    register unsigned short crc = 0xffff;

    if (skipfirst) {
        daten++;
    }
    while (len--) {
        crc = (crc >> 8) | (crc << 8);
        crc ^= *daten++;
        crc ^= (crc & 0xff) >> 4;
        crc ^= (crc << 8) << 4;
        crc ^= ((crc & 0xff) << 4) << 1;
    }

#if crc_timetest
    if (tstart > 0 && gettimeofday(&t, NULL) == 0)
        printf("vdr-radio: crc-calctime = %d usec\n", (int)((t.tv_sec*1000000 + t.tv_usec) - tstart));
#endif    

    return ~(crc);
}

/* ----------------------------------------------------------------------------------------------------------- */

#define HEXDUMP_COLS 16
void hexdump(const void *Data, int Len)
{
  if (!Data || Len <= 0)
     return;

  char hex[(3 * HEXDUMP_COLS) + 1];
  char ascii[HEXDUMP_COLS + 1];

  uint8_t *p = (uint8_t *)Data;
  int ofs = 0;
  while (Len) {
        char *h = hex;
        int i = Len > HEXDUMP_COLS ? HEXDUMP_COLS : Len;

        *h = 0;
        int j = 0;

        while(j < i) {
            uint8_t c = *p++;
            h += sprintf(h, "%02x ", c);
            ascii[j++] = isprint(c) ? c : '.';
            }
        ascii[j] = '\0';

        for (; j < HEXDUMP_COLS; j++)
            h += sprintf(h, "   ");

        dsyslog("0x%06x: %s  %s", ofs, hex, ascii);

        Len -= i;
        ofs += i;
        }
}

/* ----------------------------------------------------------------------------------------------------------- */

#define EntityChars 56
const char *entitystr[EntityChars]  = { "&apos;",   "&amp;",    "&quot;",  "&gt",      "&lt",      "&copy;",   "&times;", "&nbsp;",
                                        "&Auml;",   "&auml;",   "&Ouml;",  "&ouml;",   "&Uuml;",   "&uuml;",   "&szlig;", "&deg;",
                                        "&Agrave;", "&Aacute;", "&Acirc;", "&Atilde;", "&agrave;", "&aacute;", "&acirc;", "&atilde;",
                                        "&Egrave;", "&Eacute;", "&Ecirc;", "&Euml;",   "&egrave;", "&eacute;", "&ecirc;", "&euml;",
                                        "&Igrave;", "&Iacute;", "&Icirc;", "&Iuml;",   "&igrave;", "&iacute;", "&icirc;", "&iuml;",
                                        "&Ograve;", "&Oacute;", "&Ocirc;", "&Otilde;", "&ograve;", "&oacute;", "&ocirc;", "&otilde;",
                                        "&Ugrave;", "&Uacute;", "&Ucirc;", "&Ntilde;", "&ugrave;", "&uacute;", "&ucirc;", "&ntilde;" };
const char *entitychar[EntityChars] = { "'",        "&",        "\"",      ">",        "<",         "c",        "*",      " ",
                                        "�",        "�",        "�",       "�",        "�",         "�",        "�",      "�",
                                        "�",        "�",        "�",       "�",        "�",         "�",        "�",      "�",
                                        "�",        "�",        "�",       "�",        "�",         "�",        "�",      "�",
                                        "�",        "�",        "�",       "�",        "�",         "�",        "�",      "�",
                                        "�",        "�",        "�",       "�",        "�",         "�",        "�",      "�", 
                                        "�",        "�",        "�",       "�",        "�",         "�",        "�",      "�" };

char *rds_entitychar(char *text)
{
    int i = 0, l, lof, lre, space;
    char *temp;
    
    while (i < EntityChars) {
        if ((temp = strstr(text, entitystr[i])) != NULL) {
            if ((S_Verbose & 0x0f) >= 2)
                printf("RText-Entity: %s\n", text);
            l = strlen(entitystr[i]);
            lof = (temp-text);
            if (strlen(text) < RT_MEL) {
                lre = strlen(text) - lof - l;
                space = 1;
                }
            else {
                lre =  RT_MEL - 1 - lof - l;
                space = 0;
                }
            memmove(text+lof, entitychar[i], 1);
            memmove(text+lof+1, temp+l, lre);
            if (space != 0)
                memmove(text+lof+1+lre, "       ", l-1);
            }
        else i++;
        }

    return text;
}

/* ----------------------------------------------------------------------------------------------------------- */

#define XhtmlChars 56
const char *xhtmlstr[XhtmlChars]  = {   "&#039;", "&#038;", "&#034;", "&#062;", "&#060;", "&#169;", "&#042;", "&#160;",
                                        "&#196;", "&#228;", "&#214;", "&#246;", "&#220;", "&#252;", "&#223;", "&#176;",
                                        "&#192;", "&#193;", "&#194;", "&#195;", "&#224;", "&#225;", "&#226;", "&#227;",
                                        "&#200;", "&#201;", "&#202;", "&#203;", "&#232;", "&#233;", "&#234;", "&#235;",
                                        "&#204;", "&#205;", "&#206;", "&#207;", "&#236;", "&#237;", "&#238;", "&#239;",
                                        "&#210;", "&#211;", "&#212;", "&#213;", "&#242;", "&#243;", "&#244;", "&#245;",
                                        "&#217;", "&#218;", "&#219;", "&#209;", "&#249;", "&#250;", "&#251;", "&#241;" };
/*  hex todo:                   "&#x27;", "&#x26;", */
/*  see *entitychar[] 
const char *xhtmlychar[EntityChars] = { "'",    "&",      "\"",     ">",      "<",      "c",      "*",      " ",
                                        "�",    "�",      "�",      "�",      "�",      "�",      "�",      "�",
                                        "�",    "�",      "�",      "�",      "�",      "�",      "�",      "�",
                                        "�",    "�",      "�",      "�",      "�",      "�",      "�",      "�",
                                        "�",    "�",      "�",      "�",      "�",      "�",      "�",      "�",
                                        "�",    "�",      "�",      "�",      "�",      "�",      "�",      "�", 
                                        "�",    "�",      "�",      "�",      "�",      "�",      "�",      "�" };
*/

char *xhtml2text(char *text)
{
    int i = 0, l, lof, lre, space;
    char *temp;

    while (i < XhtmlChars) {
        if ((temp = strstr(text, xhtmlstr[i])) != NULL) {
            if ((S_Verbose & 0x0f) >= 2) {
                printf("XHTML-Char: %s\n", text);
            }
            l = strlen(xhtmlstr[i]);
            lof = (temp - text);
            if (strlen(text) < RT_MEL) {
                lre = strlen(text) - lof - l;
                space = 1;
            }
            else {
                lre = RT_MEL - 1 - lof - l;
                space = 0;
            }
            memmove(text + lof, entitychar[i], 1);
            memmove(text + lof + 1, temp + l, lre);
            if (space != 0) {
                memmove(text + lof + 1 + lre, "     ", l - 1);
            }
        }
        else {
            i++;
        }
    }

    rds_entitychar(text);
    return text;
}

/* ----------------------------------------------------------------------------------------------------------- */

char *rtrim(char *text)
{
    char *s = text + strlen(text) - 1;
    while (s >= text && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')) {
        *s-- = 0;
    }
    return text;
}

/* ----------------------------------------------------------------------------------------------------------- */

bool ParseMpaFrameHeader(const uchar *data, uint32_t *mpaFrameInfo, int *frameSize) {
    uint32_t info = data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];
    if (info == *mpaFrameInfo)
        return false; // unchanged

    int A = (info >> 21) & 0x7FF;
    int B = (info >> 19) & 0x3;
    int C = (info >> 17) & 0x3;
    int D = (info >> 16) & 0x1;
    int E = (info >> 12) & 0xF;
    int F = (info >> 10) & 0x3;
    int G = (info >>  9) & 0x1;

    int mpa_sr[4] = { 44100, 48000, 32000, 0 }; // v1, v2/2, v2.5/4

    int mpa_br11[16] = { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0 };
    int mpa_br12[16] = { 0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, 0 };
    int mpa_br13[16] = { 0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 0 };

    int mpa_br21[16] = { 0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256, 0 };
    int mpa_br22[16] = { 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, 0 };

    //dsyslog("%08X : A=%X B=%d C=%d D=%d E=%d F=%d G=%d", info, A, B, C, D, E, F, G);
    int ver = (B == 0) ? 25 : (B == 2) ? 20 : (B == 3) ? 10 : 0;

    int MPver = 4 - B; // 1,2,x,2.5
    int MPlay = 4 - C; // 1,2,3,x
    int BR = (MPver == 1) ?
                (MPlay == 1) ? mpa_br11[E] : (MPlay == 2) ? mpa_br12[E] : (MPlay == 3) ? mpa_br13[E] : 0 :
             (MPver == 2 || MPver == 4) ?
                (MPlay == 1) ? mpa_br21[E] : (MPlay == 2 || MPlay == 3) ? mpa_br22[E] : 0 : 0;

    int SR = mpa_sr[F];
    if      (B == 2) { SR /= 2; }
    else if (B == 0) { SR /= 4; }

    int FrameSize = BR ? (MPlay == 1) ? (12 * BR * 1000 / SR + G) * 4 : (144 * BR * 1000 / SR + G) : 0; // Ver.1,Lay.2 -> 960 bytes
    dsyslog("MPEG V %d L %d BR %d SR %d FSize %d", MPver, MPlay, BR, SR, FrameSize);

    *mpaFrameInfo = info;
    *frameSize = FrameSize;

    return true;
}

const char *bitrates[5][16] = {
    // MPEG 1, Layer 1-3
    { "free", "32k", "64k", "96k", "128k", "160k", "192k", "224k", "256k", "288k", "320k", "352k", "384k", "416k", "448k", "bad" },
    { "free", "32k", "48k", "56k",  "64k",  "80k",  "96k", "112k", "128k", "160k", "192k", "224k", "256k", "320k", "384k", "bad" },
    { "free", "32k", "40k", "48k",  "56k",  "64k",  "80k",  "96k", "112k", "128k", "160k", "192k", "224k", "256k", "320k", "bad" }
/*    MPEG 2/2.5, Layer 1+2/3
    { "free", "32k", "48k", "56k",  "64k",  "80k",  "96k", "112k", "128k", "144k", "160k", "176k", "192k", "224k", "256k", "bad" },
    { "free",  "8k", "16k", "24k",  "32k",  "40k",  "48k",  "56k",  "64k",  "80k",  "96k", "112k", "128k", "144k", "160k", "bad" }
*/
};

char *audiobitrate(const unsigned char *data)
{
    int hl = (data[8] > 0) ? 9 + data[8] : 9;
    //printf("vdr-radio: audioheader = <%02x %02x %02x %02x>\n", data[hl], data[hl+1], data[hl+2], data[hl+3]);

    char *temp;
    if (data[hl] == 0xff && (data[hl + 1] & 0xe0) == 0xe0) {  // syncword o.k.
        int layer = (data[hl + 1] & 0x06) >> 1;       // Layer description
        if (layer > 0) {
            switch ((data[hl + 1] & 0x18) >> 3) {     // Audio Version ID
            case 0x00:
                asprintf(&temp, "V2.5");
                break;
            case 0x01:
                asprintf(&temp, "Vres");
                break;
            case 0x02:
                asprintf(&temp, "V2");
                break;
            case 0x03:
                asprintf(&temp, "%s", bitrates[3 - layer][data[hl + 2] >> 4]);
                break;
            }
        }
        else {
            asprintf(&temp, "Lres");
        }
    }
    else {
        asprintf(&temp, "???");
    }

    return temp;
}

const char* ptynr2string(int nr) {
    switch (nr) {
    // Source: http://www.ebu.ch/trev_255-beale.pdf
    case 0:
        return tr("unknown program type");
    case 1:
        return tr("News");
    case 2:
        return tr("Current affairs");
    case 3:
        return tr("Information");
    case 4:
        return tr("Sport");
    case 5:
        return tr("Education");
    case 6:
        return tr("Drama");
    case 7:
        return tr("Culture");
    case 8:
        return tr("Science");
    case 9:
        return tr("Varied");
    case 10:
        return tr("Pop music");
    case 11:
        return tr("Rock music");
    case 12:
        return tr("M.O.R. music");
    case 13:
        return tr("Light classical");
    case 14:
        return tr("Serious classical");
    case 15:
        return tr("Other music");
        // 16-30 "Spares"
    case 31:
        return tr("Alarm");
    default:
        return "?";
    }
}

//--------------- End -----------------------------------------------------------------
