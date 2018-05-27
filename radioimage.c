#include <vdr/remote.h>
#include <vdr/status.h>
#include <vdr/plugin.h>
#include "radioaudio.h"
#include "radioskin.h"
#include "radiotools.h"
#include "service.h"
#include <math.h>

extern cRadioImage *RadioImage;

// --- cRadioImage -------------------------------------------------------

cRadioImage::cRadioImage(void) :
        cThread("radioimage") {
    imagepath = 0;
    imageShown = false;
    RadioImage = this;
}

cRadioImage::~cRadioImage() {
    if (Running())
        Stop();
    free(imagepath);
}

void cRadioImage::Init(void) {
    RadioImage->Start();
}

void cRadioImage::Exit(void) {
    if (RadioImage != NULL) {
        RadioImage->Stop();
        DELETENULL(RadioImage);
    }
}

void cRadioImage::Stop(void) {
    Cancel(2);
}

void cRadioImage::Action(void) {
    if ((S_Verbose & 0x0f) >= 2)
        printf("vdr-radio: image-showing starts\n");

    while (Running()) {
        cCondWait::SleepMs(333);
        if ((IsRadioOrReplay == 1) && imagepath && !imageShown) { // only live $20090905
            imageShown = true;
            Show(imagepath);
        }
    }

    if ((S_Verbose & 0x0f) >= 2) {
        printf("vdr-radio: image-showing ends\n");
    }
}

void cRadioImage::Show(const char *file) {
    uchar *buffer;
    int fd;
    struct stat st;
    struct video_still_picture sp;
    if ((fd = open(file, O_RDONLY)) >= 0) {
        fstat(fd, &st);
        sp.iFrame = (char *) malloc(st.st_size);
        if (sp.iFrame) {
            sp.size = st.st_size;
            if (read(fd, sp.iFrame, sp.size) > 0) {
                buffer = (uchar *) sp.iFrame;
                if (S_StillPic > 0)
                    cDevice::PrimaryDevice()->StillPicture(buffer, sp.size);
                else {
                    for (int i = 1; i <= 25; i++)
                        send_pes_packet(buffer, sp.size, i);
                }
            }
            free(sp.iFrame);
        }
        close(fd);
    }
}

void cRadioImage::send_pes_packet(unsigned char *data, int len, int timestamp) {
#define PES_MAX_SIZE 2048
    int ptslen = timestamp ? 5 : 1;
    static unsigned char pes_header[PES_MAX_SIZE];
    pes_header[0] = pes_header[1] = 0;
    pes_header[2] = 1;
    pes_header[3] = 0xe0;

    while (len > 0) {
        int payload_size = len;
        if (6 + ptslen + payload_size > PES_MAX_SIZE) {
            payload_size = PES_MAX_SIZE - (6 + ptslen);
        }
        pes_header[4] = (ptslen + payload_size) >> 8;
        pes_header[5] = (ptslen + payload_size) & 255;
        if (ptslen == 5) {
            int x;
            x = (0x02 << 4) | (((timestamp >> 30) & 0x07) << 1) | 1;
            pes_header[8] = x;
            x = ((((timestamp >> 15) & 0x7fff) << 1) | 1);
            pes_header[7] = x >> 8;
            pes_header[8] = x & 255;
            x = ((((timestamp) & 0x7fff) < 1) | 1);
            pes_header[9] = x >> 8;
            pes_header[10] = x & 255;
        } else {
            pes_header[6] = 0x0f;
        }

        memcpy(&pes_header[6 + ptslen], data, payload_size);
        cDevice::PrimaryDevice()->PlayPes(pes_header,
                6 + ptslen + payload_size);
        len -= payload_size;
        data += payload_size;
        ptslen = 1;
    }
}

void cRadioImage::SetBackgroundImage(const char *Image) {
    free(imagepath);
    imagepath = 0;

    if (Image) {
        imageShown = false;
        asprintf(&imagepath, "%s", Image);
    }
}

