/*
 * service.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __RADIO_SERVICE_H
#define __RADIO_SERVICE_H

#include <string>

// --- Service Interface -------------------------------------------------------

struct RadioTextService_v1_0 {
   int rds_info;           // 0= no / 1= Text / 2= Text + RTplus-Tags (Item,Artist)
   int rds_pty;            // 0-31
   char *rds_text;
   char *rds_title;
   char *rds_artist;
   struct tm *title_start;
};

struct RadioTextService_v1_1 {
   int rds_info;           // 0= no / 1= Text / 2= Text + RTplus-Tags (Item,Artist)
   int rds_pty;            // 0-31
   std::string rds_pty_info;
   std::string rds_text;
   std::string rds_title;
   std::string rds_artist;
   time_t title_start;
   std::string bitrate;
};

#define RADIO_TEXT_SERVICE0   "RadioTextService-v1.0"
#define RADIO_TEXT_SERVICE1   "RadioTextService-v1.1"
#define RADIO_TEXT_UPDATE     "RadioTextUpdate-v1.0"

#endif // __RADIO_SERVICE_H
