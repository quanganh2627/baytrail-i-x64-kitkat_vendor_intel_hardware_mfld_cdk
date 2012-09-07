/*
 * Copyright © 2012 Intel Corporation
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Jackie Li <yaodong.li@intel.com>
 *
 */
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/queue.h>
#include <linux/netlink.h>
#include "IntelHWComposer.h"
#include "IntelVsyncEventHandler.h"

IntelVsyncEventHandler::IntelVsyncEventHandler(IntelHWComposer *hwc) :
    mComposer(hwc), mNextFakeVSync(0)
{
    LOGV("Vsync Event Handler created");
    mRefreshPeriod = nsecs_t(1e9 / 60);
}

IntelVsyncEventHandler::~IntelVsyncEventHandler()
{

}

void IntelVsyncEventHandler::handleVsyncEvent(const char *msg, int msgLen)
{
    uint64_t ts = 0;
    int pipe = 0;
    const char *vsync_msg, *pipe_msg;

    if (strcmp(msg, "change@/devices/pci0000:00/0000:00:02.0/drm/card0"))
        return;
    msg += strlen(msg) + 1;

    vsync_msg = pipe_msg = 0;

    do {
        if (!strncmp(msg, "VSYNC=", strlen("VSYNC=")))
            vsync_msg = msg;
        else if (!strncmp(msg, "PIPE=", strlen("PIPE=")))
            pipe_msg = msg;
        if (vsync_msg && pipe_msg)
            break;
        msg += strlen(msg) + 1;
    } while (*msg);

    // report vsync event
    if (vsync_msg && pipe_msg) {
        ts = strtoull(vsync_msg + strlen("VSYNC="), NULL, 0);
        pipe = strtol(pipe_msg + strlen("PIPE="), NULL, 0);

        LOGV("%s: vsync timestamp %lld, pipe %d\n", __func__, ts, pipe);
        mComposer->vsync(ts, pipe);
    }
}

bool IntelVsyncEventHandler::threadLoop()
{
    struct pollfd fds;
    int nr;

    fds.fd = mUeventFd;
    fds.events = POLLIN;
    fds.revents = 0;
    nr = poll(&fds, 1, -1);

    if(nr > 0 && fds.revents == POLLIN) {
        int count = recv(mUeventFd, mUeventMessage, UEVENT_MSG_LEN - 2, 0);
        if (count > 0)
            handleVsyncEvent(mUeventMessage, UEVENT_MSG_LEN - 2);
    }

    return true;
}

status_t IntelVsyncEventHandler::readyToRun()
{
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
    return NO_ERROR;
}

void IntelVsyncEventHandler::onFirstRef()
{
    LOGV("Vsync Event Handler onFirstRef");
    run("HWC Vsync Event Handler", PRIORITY_URGENT_DISPLAY);
}
