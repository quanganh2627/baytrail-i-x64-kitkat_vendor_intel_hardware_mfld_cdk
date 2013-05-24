/*
 **
 ** Copyright 2011 Intel Corporation
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **      http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 **
 **
 */

#include "ad_streamer.h"

#include <full_rw.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <semaphore.h>
#include <ctype.h>

#include "hardware_legacy/power.h"


/* Total byte of buffer size would be ES3X5_BUFFER_SIZE * ES3X5_BUFFER_COUNT
 * Audience send 328 bytes for each frame. 164 bytes are half of the frame size.
 * It also need to be less than 256 bytes as the limitation of I2C driver.
 */
#define ES3X5_BUFFER_SIZE        164
/* ES3X5_BUFFER_COUNT must be a power of 2 */
#define ES3X5_BUFFER_COUNT       256

#define false   0
#define true    1

/* Time out used by the run_writefile thread to detect
 * that the capture thread has stopped and does not produce
 * buffer anymore. Assuming a bandwith of about 70KB/s for the
 * catpure thread, and a buffer size of 1024B, the capture thread
 * should produce a buffer about every 14 ms. Since the time out
 * value must be in seconds, the closest value is 1 second.
 */
#define READ_TIMEOUT_IN_SEC 1
/* wait for streaming capture starts */
#define START_TIMEOUT_IN_SEC 1
/* Waiting until all audience stream is flushed in micro seconds */
#define THREAD_EXIT_WAIT_IN_US 100000

/* Private data */
static const unsigned char startStreamCmd[AD_CMD_SIZE] = { 0x80, 0x25, 0x00, 0x01 };
static const unsigned char stopStreamCmd[AD_CMD_SIZE] = { 0x80, 0x25, 0x00, 0x00 };

static int local_audience_fd = -1;
static int local_outfile_fd = -1;
static volatile int stop_requested = false;
static unsigned int written_bytes = 0;
static unsigned int read_bytes = 0;
static unsigned char data[ES3X5_BUFFER_COUNT][ES3X5_BUFFER_SIZE];

static sem_t sem_read;
static sem_t sem_start;

static pthread_t pt_capture;
static pthread_t pt_fwrite;

static char lockid[32];

static volatile long int cap_idx = 0;
static volatile long int wrt_idx = 0;


/* Thread to read stream on Audience chip */
void *run_capture(void *ptr)
{
    int index = 0;
    struct timespec ts;
    int buffer_level;

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += START_TIMEOUT_IN_SEC;

    if (sem_timedwait(&sem_start, &ts)) {

        printf("Error: read start timed out.\n");
        stop_requested = true;
        return NULL;
    }
    while (!stop_requested) {

        full_read(local_audience_fd, data[index++], ES3X5_BUFFER_SIZE);
        cap_idx++;

        if (UINT_MAX - read_bytes >= ES3X5_BUFFER_SIZE) {

            read_bytes += ES3X5_BUFFER_SIZE;
        } else {

            printf("read bytes are overflowed.\n");
        }

        index &= ES3X5_BUFFER_COUNT - 1;
        buffer_level = cap_idx + wrt_idx;

        if (buffer_level > ES3X5_BUFFER_COUNT) {

            printf("Warning: buffer overflow (%d/%d)\n", buffer_level, ES3X5_BUFFER_COUNT);
        }
        sem_post(&sem_read);
    }
    return NULL;
}

/* Thread to write stream to output file */
void *run_writefile(void *ptr)
{
    int index = 0;
    struct timespec ts;

    while (true) {

        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += READ_TIMEOUT_IN_SEC;

        if (sem_timedwait(&sem_read, &ts)) {

            if (read_bytes > written_bytes) {

                printf("Error:timed out.\n");
                stop_requested = true;
            }
            return NULL;
        }

        full_write(local_outfile_fd, data[index++], ES3X5_BUFFER_SIZE);
        wrt_idx--;

        if (UINT_MAX - written_bytes >= ES3X5_BUFFER_SIZE) {

            written_bytes += ES3X5_BUFFER_SIZE;
        } else {

            printf("written bytes are overflowed\n");
        }
        index &= ES3X5_BUFFER_COUNT - 1;
    }
    return NULL;
}

int start_streaming(int audience_fd, int outfile_fd) {

    int rc;
    struct sched_param param;
    pthread_attr_t thread_attr;

    stop_requested = false;
    written_bytes = 0;
    read_bytes = 0;

    // Check arguments
    if (audience_fd < 0 || outfile_fd < 0) {

        printf("%s - Invalid argument(s)", __FUNCTION__);
        return -1;
    }

    // Record file descriptors
    local_audience_fd = audience_fd;
    local_outfile_fd = outfile_fd;

    // Initialze semaphore and threads
    if (sem_init(&sem_read, 0, 0) != 0) {

        printf("%s - Initialzing sem_read semaphore failed\n", __FUNCTION__);
        return -1;
    }
    if (sem_init(&sem_start, 0, 0) != 0) {

        printf("%s - Initialzing sem_start semaphore failed\n", __FUNCTION__);
        return -1;
    }
    if (pthread_attr_init(&thread_attr) != 0) {

        printf("%s - Initialzing thread attr failed\n", __FUNCTION__);
        return -1;
    }
    if (pthread_attr_setschedpolicy(&thread_attr, SCHED_RR) != 0) {

        printf("%s - Initialzing thread policy failed\n", __FUNCTION__);
        return -1;
    }
    param.sched_priority = sched_get_priority_max(SCHED_RR);
    if (pthread_attr_setschedparam (&thread_attr, &param) != 0) {

        printf("%s - Initialzing thread priority failed\n", __FUNCTION__);
        return -1;
    }
    if (pthread_create(&pt_capture, &thread_attr, run_capture, NULL) != 0) {

        printf("%s - Creating read thread failed\n", __FUNCTION__);
        return -1;
    }
    if (pthread_create(&pt_fwrite, NULL, run_writefile, NULL) != 0) {

        printf("%s - Creating write thread failed\n", __FUNCTION__);
        return -1;
    }
    acquire_wake_lock(PARTIAL_WAKE_LOCK, lockid);

    // Start Streaming
    rc = full_write(local_audience_fd, startStreamCmd, sizeof(startStreamCmd));
    if (rc < 0) {

        printf("%s - Audience start stream command failed: %s (%d)\n",
              __FUNCTION__,
              strerror(errno),
              rc);
        release_wake_lock(lockid);
        return rc;
    }

    // Let thread start recording
    sem_post(&sem_start);

    return 0;
}

int stop_streaming(void) {

    int rc;

    release_wake_lock(lockid);

    // Stop Streaming
    rc = full_write(local_audience_fd, stopStreamCmd, sizeof(stopStreamCmd));
    if (rc < 0) {

        printf("audience stop stream command failed: %d\n", rc);
        return rc;
    }

    // Because there is no way to know whether audience I2C buffer is empty, sleep is required.
    usleep(THREAD_EXIT_WAIT_IN_US);
    stop_requested = true;

    printf("Stopping capture.\n");
    pthread_join(pt_capture, NULL);
    printf("Capture stopped.\n");
    pthread_join(pt_fwrite, NULL);

    printf("Total of %d bytes read\n", written_bytes);

    return 0;
}
