/*
 * radiotools.h - part of radio.c, a plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __RADIO_TOOLS_H
#define __RADIO_TOOLS_H

bool file_exists(const char *filename);
bool enforce_directory(const char *path);
unsigned short crc16_ccitt(unsigned char *daten, int len, bool skipfirst);
void hexdump(const void *Data, int Len);
char *rds_entitychar(char *text);
char *xhtml2text(char *text);
char *rtrim(char *text);
bool ParseMpaFrameHeader(const uchar *data, uint32_t *mpaFrameInfo, int *frameSize, char *bitRate);
char *audiobitrate(const unsigned char *data);
void tmc_parser(unsigned char *data, int len);		// Alert-c
const char* ptynr2string(int nr);

#endif //__RADIO_TOOLS_H
