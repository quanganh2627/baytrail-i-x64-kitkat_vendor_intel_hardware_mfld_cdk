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
    mMultiDisplayManagerService(0),
    mActiveDisplayMode(INVALID_MDS_MODE),
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
    LOGV("onModeChange: External display monitor onModeChange");
    if (mode == MDS_HWC_HDMI_PLUGOUT ||
        mode == MDS_HWC_OVERLAY_EXTEND ||
        mode == MDS_HWC_OVERLAY_CLONE_MIPI0) {

        // update active display mode
        mActiveDisplayMode = mode;
        mComposer->onUEvent(mUeventMessage, UEVENT_MSG_LEN - 2, MSG_TYPE_MDS);
    }
}

int IntelExternalDisplayMonitor::getDisplayMode()
{
    LOGV("Get display mode %d", mActiveDisplayMode);

    return mActiveDisplayMode;
}

void IntelExternalDisplayMonitor::binderDied(const wp<IBinder>& who)
{
    LOGV("External display monitor binderDied");
}

bool IntelExternalDisplayMonitor::threadLoop()
{
    //LOGV("External display monitor thread loop");

    if (mMultiDisplayManagerService !=  0) {
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
            mMultiDisplayManagerService =
                interface_cast<IMultiDisplayComposer>(service);
            service->linkToDeath(this);
            break;
        }
        LOGW("Failed to get MDS service.try again...\n");
    } while(--retry);

    if (!retry && mMultiDisplayManagerService == 0) {
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
        mMultiDisplayManagerService->registerModeChangeListener(this);
    }

    return NO_ERROR;
}

void IntelExternalDisplayMonitor::onFirstRef()
{
    LOGV("External display monitor onFirstRef");
    run("HWC external display monitor", PRIORITY_URGENT_DISPLAY);
}
