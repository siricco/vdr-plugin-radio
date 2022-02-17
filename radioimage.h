/*
 * radioimage.h - part of radio.c, a plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __RADIO_IMAGE_H
#define __RADIO_IMAGE_H

#include <vdr/thread.h>

// Separate thread for showing RadioImages
class cRadioImage: public cThread {
private:
    char *imagepath;
    bool imageShown;
    void Show (const char *file);
    void send_pes_packet(unsigned char *data, int len, int timestamp);
protected:
    virtual void Action(void);
    void Stop(void);
public:
    cRadioImage(void);
    virtual ~cRadioImage();
    static void Init(void);
    static void Exit(void);
    void SetBackgroundImage(const char *Image);
};

#endif //__RADIO_IMAGE_H
