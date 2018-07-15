/*
 * radiocheck.c
 *
 *  Created on: 27.05.2018
 *      Author: uli
 */

// --- cRadioCheck -------------------------------------------------------

#include <vdr/plugin.h>
#include <vdr/status.h>
#include <vdr/config.h>
#include <vdr/interface.h>
#include <vdr/transfer.h>
#include "radiocheck.h"
#include "radioepg.h"
#include "inforx.h"

extern int IsRadioOrReplay;
extern int S_Verbose;
extern int InfoTimeout;

extern bool DoInfoReq, InfoRequest;
extern const cChannel *chan;

cRadioCheck *cRadioCheck::RadioCheck = NULL;

cRadioCheck::cRadioCheck(void)
: cThread("radiocheck")
{
    IsRadioOrReplay = 0;
}

cRadioCheck::~cRadioCheck() {
    if (Running()) {
        Stop();
    }
}

void cRadioCheck::Init(void) {
    if (RadioCheck == NULL) {
        RadioCheck = new cRadioCheck;
        RadioCheck->Start();
    }
}

void cRadioCheck::Exit(void) {
    if (RadioCheck != NULL) {
        RadioCheck->Stop();
        DELETENULL(RadioCheck);
    }
}

void cRadioCheck::Stop(void) {
    Cancel(10);
}

void cRadioCheck::Action(void)
{
    if ((S_Verbose & 0x0f) >= 2) {
        printf("vdr-radio: background-checking starts\n");
    }

    while (Running()) {
        cCondWait::SleepMs(2000);

        // check Live-Radio
        if ((IsRadioOrReplay == 1) && (chan != NULL)) {
            if (chan->Vpid()) {
                isyslog("radio: channnel '%s' got Vpid= %d", chan->Name(),
                        chan->Vpid());
                IsRadioOrReplay = 0;
#if VDRVERSNUM >= 20300
                LOCK_CHANNELS_READ
                Channels->SwitchTo(cDevice::CurrentChannel());
#else
                Channels.SwitchTo(cDevice::CurrentChannel());
#endif
                //cDevice::PrimaryDevice()->SwitchChannel(chan, true);
            } else {
                if ((InfoTimeout -= 2) <= 0) {
                    InfoTimeout = 20;
                    int chtid = chan->Tid();
                    // Kanal-EPG PresentEvent
                    if (chan->Apid(0) > 0
                            && (chtid == PREMIERERADIO_TID
                                    || chtid == KDRADIO_TID
                                    || chtid == UMRADIO_TID1
                                    || chtid == UMRADIO_TID2
                                    || chtid == UMRADIO_TID3
                                    || chtid == UMRADIO_TID4
                                    || chtid == UMRADIO_TID5)) {
#if VDRVERSNUM >= 20300
                        LOCK_SCHEDULES_READ
                        static cStateKey SchedulesStateKey;
                        const cSchedules *scheds = cSchedules::GetSchedulesRead(
                                SchedulesStateKey);
#else
                        cSchedulesLock schedLock;
                        const cSchedules *scheds = cSchedules::Schedules(schedLock);
#endif
                        if (scheds != NULL) {
                            const cSchedule *sched = scheds->GetSchedule(
                                    chan->GetChannelID());
                            if (sched != NULL) {
                                const cEvent *present =
                                        sched->GetPresentEvent();
                                if (present != NULL) {
                                    if (chtid == PREMIERERADIO_TID) { // Premiere
                                        InfoTimeout = epg_premiere(
                                                present->Title(),
                                                present->Description(),
                                                present->StartTime(),
                                                present->EndTime());
                                    }
                                    else if (chtid == KDRADIO_TID) {// Kabel Deutschland
                                        InfoTimeout = epg_kdg(
                                                present->Description(),
                                                present->StartTime(),
                                                present->EndTime());
                                    }
                                    else {
                                        // Unity Media Kabel
                                        InfoTimeout = epg_unitymedia(
                                                present->Description(),
                                                present->StartTime(),
                                                present->EndTime());
                                    }
                                    InfoRequest = true;
                                }
                                else {
                                    dsyslog("radio: no event.present (Tid= %d, Apid= %d)",
                                            chtid, chan->Apid(0));
                                }
                            }
                            else {
                                dsyslog("radio: no schedule (Tid= %d, Apid= %d)",
                                        chtid, chan->Apid(0));
                            }
                        }
                    }
                    // Artist/Title with external script?
                    else if (chan->Apid(0) > 0 && DoInfoReq) {
                        InfoTimeout = info_request(chtid, chan->Apid(0));
                        InfoRequest = (InfoTimeout > 0);
                    }
                }
            }
        }

        // temp. OSD-CloseTimeout
        (RT_OsdTOTemp > 0) ? RT_OsdTOTemp -= 2 : RT_OsdTOTemp = 0; // in sec like this cycletime
        // Radiotext-Autodisplay
        if ((S_RtDispl == 2) && (RT_Info >= 0) && !RT_OsdTO
                && (RT_OsdTOTemp == 0) && RT_ReOpen && !Skins.IsOpen()
                && !cOsd::IsOpen()) {
            cRemote::CallPlugin("radio");
        }
    }

    if ((S_Verbose & 0x0f) >= 2) {
        printf("vdr-radio: background-checking ends\n");
    }
}

