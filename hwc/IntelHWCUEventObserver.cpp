/*
 * Copyright (C) 2010 The Android Open Source Project
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

#include <IntelHWCUEventObserver.h>
#include <cutils/log.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/queue.h>
#include <linux/netlink.h>

IntelHWCUEventObserver::IntelHWCUEventObserver()
    : mReadyToRun(false)
{
}

IntelHWCUEventObserver::~IntelHWCUEventObserver()
{

}

bool IntelHWCUEventObserver::startObserver()
{
    // create new pthread
    int err = pthread_create(&mThread, NULL,
                             IntelHWCUEventObserver::threadLoop,
                             (void *)this);
    if (err) {
        LOGE("%s: failed to start observer, err %d\n", __func__, err);
        return false;
    }

    mReadyToRun = true;

    LOGD("%s: observer started\n", __func__);
    return true;
}

bool IntelHWCUEventObserver::stopObserver()
{
    mReadyToRun = false;

    int err = pthread_join(mThread, NULL);
    if (err) {
        LOGE("%s: failed to stop observer, err %d\n", __func__, err);
        return false;
    }
    LOGD("%s: observer stopped\n", __func__);
    return true;
}

void *IntelHWCUEventObserver::threadLoop(void *data)
{
    char ueventMessage[UEVENT_MSG_LEN];
    int fd = -1;
    struct sockaddr_nl addr;
    int sz = 64*1024;

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid =  pthread_self() | getpid();
    addr.nl_groups = 0xffffffff;

    fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if(fd < 0) {
        LOGD("%s: failed to open uevent socket\n", __func__);
        return 0;
    }

    setsockopt(fd, SOL_SOCKET, SO_RCVBUFFORCE, &sz, sizeof(sz));

    if(bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        close(fd);
        return 0;
    }

    memset(ueventMessage, 0, UEVENT_MSG_LEN);
    IntelHWCUEventObserver *observer =
        static_cast<IntelHWCUEventObserver*>(data);

    do {
        struct pollfd fds;
        int nr;

        fds.fd = fd;
        fds.events = POLLIN;
        fds.revents = 0;
        nr = poll(&fds, 1, -1);

        if(nr > 0 && fds.revents == POLLIN) {
            int count = recv(fd, ueventMessage, UEVENT_MSG_LEN - 2, 0);
            if (count > 0)
                observer->onUEvent(ueventMessage, UEVENT_MSG_LEN - 2);
        }
    } while (observer->isReadyToRun());

thread_exit:
    LOGD("%s: observer exited\n", __func__);
    return NULL;
}

void IntelHWCUEventObserver::ueventHandler(void *data, const char *msg, int msgLen)
{
    IntelHWCUEventObserver *observer = static_cast<IntelHWCUEventObserver*>(data);
    if (observer)
        observer->onUEvent(msg, msgLen);
}

void IntelHWCUEventObserver::onUEvent(const char *msg, int msgLen)
{

}
