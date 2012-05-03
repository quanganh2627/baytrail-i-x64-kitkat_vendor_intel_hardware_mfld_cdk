/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#include <binder/IServiceManager.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/queue.h>
#include <linux/netlink.h>
#include <pthread.h>

#include "IntelExternalDisplayMonitor.h"
#include "IntelHWComposer.h"

using namespace android;

IntelExternalDisplayMonitor::IntelExternalDisplayMonitor(IntelHWComposer *hwc) :
    mMDClient(NULL),
    mActiveDisplayMode(INVALID_MDS_MODE),
    mWidiOn(false),
    mMipiOn(true),
    mInitialized(false),
    mComposer(hwc)
{
    LOGV("External display monitor created");
    initialize();
}

IntelExternalDisplayMonitor::~IntelExternalDisplayMonitor()
{
    LOGV("External display monitor Destroyed");
    mInitialized = false;
    mComposer = 0;
}

void IntelExternalDisplayMonitor::initialize()
{
    LOGV("External display monitor initialized");
    mInitialized = true;
}

void IntelExternalDisplayMonitor::onModeChange(int mode)
{
    LOGI("onModeChange: External display monitor onModeChange, 0x%x", mode);
    mActiveDisplayMode = mode;
    mComposer->onUEvent(mUeventMessage, UEVENT_MSG_LEN - 2, MSG_TYPE_MDS);
}

int IntelExternalDisplayMonitor::getDisplayMode()
{
    LOGV("Get display mode %d", mActiveDisplayMode);
    if ((mActiveDisplayMode != INVALID_MDS_MODE) &&
            (mActiveDisplayMode & MDS_HDMI_VIDEO_EXT))
        return OVERLAY_EXTEND;
    else
        return OVERLAY_CLONE_MIPI0;
}

bool IntelExternalDisplayMonitor::isVideoPlaying()
{
    if ((mActiveDisplayMode != INVALID_MDS_MODE) &&
            (mActiveDisplayMode & MDS_VIDEO_PLAYING))
       return true;
    return false;
}

void IntelExternalDisplayMonitor::binderDied(const wp<IBinder>& who)
{
    LOGV("External display monitor binderDied");
}

bool IntelExternalDisplayMonitor::notifyWidi(bool on)
{
    LOGV("Exteranal display notify the MDS widi's state");
    if ((mMDClient != NULL) && (mWidiOn != on)) {
        mWidiOn = on;
        return mMDClient->notifyWidi(on);
    }
    return false;
}

bool IntelExternalDisplayMonitor::notifyMipi(bool on)
{
    LOGV("Exteranal display notify the MDS that Mipi should be turned on/off");
    if ((mMDClient != NULL) && (mMipiOn != on)
                && (mActiveDisplayMode != INVALID_MDS_MODE)
                && (mActiveDisplayMode & MDS_HDMI_VIDEO_EXT)) {
        mMipiOn = on;
        return mMDClient->notifyMipi(on);
    }
    return false;
}

bool IntelExternalDisplayMonitor::threadLoop()
{
    //LOGV("External display monitor thread loop");

    if (mMDClient !=  0) {
        LOGV("threadLoop: found MDS, threadLoop will exit.");
        requestExit();
        return true;
    }

    // if no MDS service available, fall back uevent
    struct pollfd fds;
    int nr;

    fds.fd = mUeventFd;
    fds.events = POLLIN;
    fds.revents = 0;
    nr = poll(&fds, 1, -1);

    if(nr > 0 && fds.revents == POLLIN) {
        int count = recv(mUeventFd, mUeventMessage, UEVENT_MSG_LEN - 2, 0);
        if (count > 0)
            mComposer->onUEvent(mUeventMessage,
                                UEVENT_MSG_LEN - 2,
                                MSG_TYPE_UEVENT);
    }

    return true;
}

status_t IntelExternalDisplayMonitor::readyToRun()
{
    LOGV("External display monitor ready to run");

    // get multi-display manager service, retry 10 seconds
    int retry = 2;
    do {
        const String16 name("MultiDisplay");
        sp<IBinder> service;
        sp<IServiceManager> serviceManager = defaultServiceManager();
        service = serviceManager->getService(name);
        if (service != 0) {
            mMDClient = new MultiDisplayClient();
            if (mMDClient == NULL) {
                LOGE("Fail to create a multidisplay client");
            } else
                LOGI("Create a MultiDisplay client at HWC");
            service->linkToDeath(this);
            break;
        }
        LOGW("Failed to get MDS service.try again...\n");
    } while(--retry);

    if (!retry && mMDClient == NULL) {
        LOGW("Failed to get mds service, fall back uevent\n");
        struct sockaddr_nl addr;
        int sz = 64*1024;

        memset(&addr, 0, sizeof(addr));
        addr.nl_family = AF_NETLINK;
        addr.nl_pid =  pthread_self() | getpid();
        addr.nl_groups = 0xffffffff;

        mUeventFd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
        if(mUeventFd < 0) {
            LOGD("%s: failed to open uevent socket\n", __func__);
            return TIMED_OUT;
        }

        setsockopt(mUeventFd, SOL_SOCKET, SO_RCVBUFFORCE, &sz, sizeof(sz));

        if(bind(mUeventFd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
            close(mUeventFd);
            return TIMED_OUT;
        }

        memset(mUeventMessage, 0, UEVENT_MSG_LEN);
    } else {
        LOGI("Got MultiDisplay Service\n");
        if (mMDClient != NULL)
            mMDClient->registerModeChangeListener(this);
    }

    return NO_ERROR;
}

void IntelExternalDisplayMonitor::onFirstRef()
{
    LOGV("External display monitor onFirstRef");
    run("HWC external display monitor", PRIORITY_URGENT_DISPLAY);
}
