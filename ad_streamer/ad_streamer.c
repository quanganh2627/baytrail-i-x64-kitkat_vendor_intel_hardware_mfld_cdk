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

#define ES3X5_DEVICE_PATH    "/dev/audience_es305"

#define ES305_CH_PRI_MIC         0x1
#define ES305_CH_SEC_MIC         0x2
#define ES305_CH_CLEAN_SPEECH    0x4
#define ES305_CH_FAR_END_IN      0x8
#define ES305_CH_FAR_END_OUT     0x10

#define ES325_CH_PRI_MIC         0x1
#define ES325_CH_SEC_MIC         0x2
#define ES325_CH_THIRD_MIC       0x3
#define ES325_CH_FOURTH_MIC      0x4
#define ES325_CH_FAR_END_INPUT   0x5
#define ES325_CH_AUDIO_INPUT_1   0x6
#define ES325_CH_AUDIO_INPUT_2   0x7
#define ES325_CH_AUDIO_INPUT_3   0x8
#define ES325_CH_AUDIO_INPUT_4   0x9
#define ES325_CH_UI_TONE_1       0xA
#define ES325_CH_UI_TONE_2       0xB
#define ES325_CH_SPK_REF_1       0xC
#define ES325_CH_SPK_REF_2       0xD
#define ES325_CH_CLEAN_SPEECH    0x10
#define ES325_CH_FAR_END_OUT_1   0x11
#define ES325_CH_FAR_END_OUT_2   0x12
#define ES325_CH_AUDIO_OUTPUT_1  0x13
#define ES325_CH_AUDIO_OUTPUT_2  0x14
#define ES325_CH_AUDIO_OUTPUT_3  0x15
#define ES325_CH_AUDIO_OUTPUT_4  0x16

#define ES3X5_CHANNEL_SELECT_FIELD 3

#define DEFAULT_OUTPUT_FILE    "/data/ad_streamer_capture.bin"

#define DEFAULT_CAPTURE_DURATION_IN_SEC 5

/* Total byte of buffer size would be ES3X5_BUFFER_SIZE * ES3X5_BUFFER_COUNT
 * Audience send 328 bytes for each frame. 164 bytes are half of the frame size.
 * It also need to be less than 256 bytes as the limitation of I2C driver.
 */
#define ES3X5_BUFFER_SIZE        164
/* ES3X5_BUFFER_COUNT must be a power of 2 */
#define ES3X5_BUFFER_COUNT       256

/* Audience command size in bytes */
#define AD_CMD_SIZE              4

/* Delay to wait after a command before to read the chip response (us),
 * as per Audience recommendations. */
#define AD_CMD_DELAY_US          20000

#define false   0
#define true    1

#define MAX_FILE_PATH_SIZE 80
#define READ_ACK_BUF_SIZE  8

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
/* Maximum number of channel to capture */
#define MAX_CAPTURE_CHANNEL 2

typedef enum {
    AUDIENCE_ES305,
    AUDIENCE_ES325,
    AUDIENCE_UNKNOWN
} chip_version_t;

/* Private data */
static const unsigned char startStreamCmd[AD_CMD_SIZE] = { 0x80, 0x25, 0x00, 0x01 };
static const unsigned char stopStreamCmd[AD_CMD_SIZE] = { 0x80, 0x25, 0x00, 0x00 };
static unsigned char setChannelCmd[AD_CMD_SIZE] = { 0x80, 0x28, 0x00, 0x03 };
static unsigned char chipIdCmd[AD_CMD_SIZE] = { 0x80, 0x0E, 0x00, 0x00 };

/* Chip ID code */
#define AD_CHIP_ID_ES305     0x1008
#define AD_CHIP_ID_ES325     0x1101

static const char* chipVersion2String[] = {
    "es305",
    "es325",
    "Unknown"
};

static chip_version_t chip_version = AUDIENCE_UNKNOWN;

static int audience_fd = -1;
static int outfile_fd = -1;
static volatile int stop_requested = false;
static unsigned int written_bytes = 0;
static unsigned int read_bytes = 0;
static unsigned char data[ES3X5_BUFFER_COUNT][ES3X5_BUFFER_SIZE];
static int duration_in_sec = DEFAULT_CAPTURE_DURATION_IN_SEC;
static char fname[MAX_FILE_PATH_SIZE] = DEFAULT_OUTPUT_FILE;
/* Number of channel to be captured */
static unsigned int captureChannelsNumber = 0;
/* Channel IDs of the channels to be captured */
static unsigned int channels[MAX_CAPTURE_CHANNEL] = {0, 0};


static sem_t sem_read;
static sem_t sem_start;

static char lockid[32];

static volatile long int cap_idx = 0;
static volatile long int wrt_idx = 0;

int setup_ad_chip()
{
    int status = 0;
    int rc;
    unsigned int chipId = 0;
    unsigned char chipIdCmdResponse[AD_CMD_SIZE];

    /* Is chip forced at command line ? */
    if (chip_version != AUDIENCE_UNKNOWN) {

        /* chip selection has been forced as cmd line argument: keep it. */
        printf("Deprecated: Audience chip forced to '%s'.\n",
               chipVersion2String[chip_version]);
    } else {

        /* Send the identification command */
        rc = full_write(audience_fd, chipIdCmd, sizeof(chipIdCmd));
        if (rc < 0) {

            printf("Audience command failed (Chip Identification): %s\n",
                   strerror(errno));
            return rc;
        }
        /* Wait for command execution */
        usleep(AD_CMD_DELAY_US);
        /* Read back the response */
        rc = full_read(audience_fd, chipIdCmdResponse, sizeof(chipIdCmdResponse));
        if (rc < 0) {

            printf("Audience command response read failed (Chip ID): %s\n",
                   strerror(errno));
            return rc;
        }
        if (chipIdCmdResponse[0] == chipIdCmd[0] && chipIdCmdResponse[1] == chipIdCmd[1]) {

            /* Retrieve the chip ID from the response: */
            chipId = chipIdCmdResponse[2] << 8 | chipIdCmdResponse[3];

            /* Check the chip ID */
            if (chipId == AD_CHIP_ID_ES325) {

                chip_version = AUDIENCE_ES325;
            } else if (chipId == AD_CHIP_ID_ES305) {

                chip_version = AUDIENCE_ES305;
            } else {

                /* Unknown ID means unsupported chip */
                printf("Unsupported Audience chip ID: 0x%04x\n", chipId);
                chip_version = AUDIENCE_UNKNOWN;
                status = -1;
            }
        } else {

            printf("Audience chip ID command failed. Chip returned 0x%02x%02x%02x%02x\n",
                   chipIdCmdResponse[0],
                   chipIdCmdResponse[1],
                   chipIdCmdResponse[2],
                   chipIdCmdResponse[3]);
            /* Unable to detect the chip */
            chip_version = AUDIENCE_UNKNOWN;
            status = -1;
        }
    }

    printf("Audience chip detected: '%s'.\n", chipVersion2String[chip_version]);

    /* es325 specific setup */
    if (chip_version == AUDIENCE_ES325) {

        // According eS325 specifications, bit #15 of setChannel command must be set to 1 */
        setChannelCmd[2] = 0x80;
    }

    return status;
}

/* Thread to keep track of time */
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

        full_read(audience_fd, data[index++], ES3X5_BUFFER_SIZE);
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

        full_write(outfile_fd, data[index++], ES3X5_BUFFER_SIZE);
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

void cleanup()
{
    release_wake_lock(lockid);
    if (audience_fd >= 0) {

        close(audience_fd);
        audience_fd = -1;
    }
    if (outfile_fd >= 0) {

        close(outfile_fd);
        outfile_fd = -1;
    }
}

int check_numeric(char *number)
{
    int index = 0;

    while (number[index] != '\0') {

        if (!isdigit(number[index++])) {

            return -1;
        }
    }
    return 0;
}

void display_help()
{
    printf("Format: [-t seconds] [-f /path/filename] [-c chip] channelId [channelId]\n");
    printf("\nArguments Details\n");
    printf("\tchannelId:\tAudience channel to be captured (see channel IDs)\n");
    printf("\t[channelId]:\tOptionnal second Audience channel to be captured (see channel IDs)\n");
    printf("\tf (optional):\tOutput file path (default: %s)\n", DEFAULT_OUTPUT_FILE);
    printf("\tt (optional):\tCapture duration in seconds (default: %d)\n",
           DEFAULT_CAPTURE_DURATION_IN_SEC);
    printf("\tc (optional) (deprecated):\tforce chip selection in <%s|%s> (default: chip"
           " autodetection)\n\n",
           chipVersion2String[AUDIENCE_ES305],
           chipVersion2String[AUDIENCE_ES325]);

    printf("\nChannel IDs for %s\n", chipVersion2String[AUDIENCE_ES305]);
    printf("\tPrimary Mic   = %d\n", ES305_CH_PRI_MIC);
    printf("\tSecondary Mic = %d\n", ES305_CH_SEC_MIC);
    printf("\tClean Speech  = %d\n", ES305_CH_CLEAN_SPEECH);
    printf("\tFar End In    = %d\n", ES305_CH_FAR_END_IN);
    printf("\tFar End out   = %d\n", ES305_CH_FAR_END_OUT);
    printf("\nChannel IDs for %s\n", chipVersion2String[AUDIENCE_ES325]);
    printf("\tPrimary Mic   = %d\n", ES325_CH_PRI_MIC);
    printf("\tSecondary Mic = %d\n", ES325_CH_SEC_MIC);
    printf("\tThird Mic     = %d\n", ES325_CH_THIRD_MIC);
    printf("\tFourth Mic    = %d\n", ES325_CH_FOURTH_MIC);
    printf("\tFar End In    = %d\n", ES325_CH_FAR_END_INPUT);
    printf("\tAudio in 1    = %d\n", ES325_CH_AUDIO_INPUT_1);
    printf("\tAudio in 2    = %d\n", ES325_CH_AUDIO_INPUT_2);
    printf("\tAudio in 3    = %d\n", ES325_CH_AUDIO_INPUT_3);
    printf("\tAudio in 4    = %d\n", ES325_CH_AUDIO_INPUT_4);
    printf("\tUI tone 1     = %d\n", ES325_CH_UI_TONE_1);
    printf("\tUI tone 2     = %d\n", ES325_CH_UI_TONE_2);
    printf("\tSpk ref 1     = %d\n", ES325_CH_SPK_REF_1);
    printf("\tSpk ref 2     = %d\n", ES325_CH_SPK_REF_2);
    printf("\tClean Speech  = %d\n", ES325_CH_CLEAN_SPEECH);
    printf("\tFar End out 1 = %d\n", ES325_CH_FAR_END_OUT_1);
    printf("\tFar End out 2 = %d\n", ES325_CH_FAR_END_OUT_2);
    printf("\tAudio out 1   = %d\n", ES325_CH_AUDIO_OUTPUT_1);
    printf("\tAudio out 2   = %d\n", ES325_CH_AUDIO_OUTPUT_2);
    printf("\tAudio out 3   = %d\n", ES325_CH_AUDIO_OUTPUT_3);
    printf("\tAudio out 4   = %d\n", ES325_CH_AUDIO_OUTPUT_4);
    printf("\nExample\n");
    printf("\tad_streamer -t 10 -f /sdcard/aud_stream.bin 1 2\n\n\tCapture Primary Mic and"
           " Secondary Mic for 10 seconds to /sdcard/aud_stream.bin\n");
    printf("\nNotes\n");
    printf("\t* Dual channels capture requires the I2C bus to be overclocked to have enough"
           " bandwidth (refer to ad_streamer user guide).\n");
    printf("\t* This tool shall be used only while in call (CSV or VOIP) (refer to ad_streamer"
           " user guide).\n");
}

int parse_cmd_line(int argc, char** argv)
{
    char *cvalue = NULL;
    int index;
    int c;

    opterr = 0;

    /* Parse optional parameters */
    while ((c = getopt (argc, argv, "t:f:c:")) != -1) {

        switch (c)
        {
            case 'f':
                cvalue = optarg;
                snprintf(fname, MAX_FILE_PATH_SIZE, "%s", cvalue);
                fname[MAX_FILE_PATH_SIZE-1] = '\0';
                break;
            case 'c':
                cvalue = optarg;
                if (strncmp(cvalue, "es305", 5) == 0) {

                    chip_version = AUDIENCE_ES305;
                } else if (strncmp(cvalue, "es325", 5) == 0) {

                    chip_version = AUDIENCE_ES325;
                } else {

                    display_help();
                    printf("\n*Please use valid chip value\n");
                    return -1;
                }
                break;
            case 't':
                // Cap the duration_in_sec to capture samples (in seconds)
                if (check_numeric(optarg)) {

                    display_help();
                    printf("\n*Please use valid numeric value for seconds\n");
                    return -1;
                }
                duration_in_sec = atoi(optarg);
                break;
            default:
                display_help();
                return -1;
        }
    }

    /* Non-optional arguments: channel ID to be captured */
    captureChannelsNumber = 0;
    for (index = optind; index < argc; index++) {

        if (check_numeric(argv[index])) {

            printf("\nERROR: please use valid numeric value for channel flag\n");
            display_help();
            return -1;
        }
        // Set streaming channel flag
        if (captureChannelsNumber < MAX_CAPTURE_CHANNEL) {

            channels[captureChannelsNumber] = atoi(argv[index]);
            captureChannelsNumber++;
        } else {

            printf("ERROR: Too many arguments\n");
            display_help();
            return -1;
        }
    }
    /* Check that at least one channel has to be recorded */
    if (captureChannelsNumber == 0) {

        printf("ERROR: Missing channel ID\n");
        display_help();
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    int rc;

    unsigned char buf[READ_ACK_BUF_SIZE];

    struct sched_param param;
    pthread_attr_t thread_attr;

    pthread_t pt_capture;
    pthread_t pt_fwrite;

    /* Parse cmd line parameters */
    if (parse_cmd_line(argc, argv) == -1) {

        return -1;
    }

    /* Initialize */
    audience_fd = open(ES3X5_DEVICE_PATH, O_RDWR, 0);

    if (audience_fd < 0) {

        printf("Cannot open %s\n", ES3X5_DEVICE_PATH);
        return -1;
    }

    /* Get the Audience FW version */
    rc = setup_ad_chip();
    if (rc < 0) {

        cleanup();
        return rc;
    };

    outfile_fd = open(fname, O_CREAT | O_WRONLY);
    if (outfile_fd < 0) {

        printf("Cannot open output file %s\n", fname);
        cleanup();
        return -1;
    }
    printf("Outputing raw streaming data to %s\n", fname);

    /* Set streaming channels */
    if (chip_version == AUDIENCE_ES305) {

        /* es305: A single command with a bit field of channels to be recorder
         * In case of single channel capture, channels[1] is equal to zero.
         * Maximum number of channels: MAX_CAPTURE_CHANNEL
         */
        setChannelCmd[ES3X5_CHANNEL_SELECT_FIELD] = channels[0] | channels[1];

        rc = full_write(audience_fd, setChannelCmd, sizeof(setChannelCmd));
        if (rc < 0) {

            printf("audience set channel command failed: %d\n", rc);
            cleanup();
            return rc;
        }
    } else if (chip_version == AUDIENCE_ES325) {

        /* es325: As much command(s) as channel(s) to be recorded
         * Maximum number of channels: MAX_CAPTURE_CHANNEL
         */
        unsigned int channel;

        for (channel = 0; channel < captureChannelsNumber; channel++) {

            setChannelCmd[ES3X5_CHANNEL_SELECT_FIELD] = channels[channel];
            rc = full_write(audience_fd, setChannelCmd, sizeof(setChannelCmd));
            if (rc < 0) {

                printf("audience set channel command failed: %d\n", rc);
                cleanup();
                return rc;
            }
        }
    }

    /* Initialze semaphore and threads */
    if (sem_init(&sem_read, 0, 0) != 0) {

        printf("Initialzing semaphore failed\n");
        cleanup();
        return -1;
    }
    if (sem_init(&sem_start, 0, 0) != 0) {

        printf("Initialzing semaphore failed\n");
        cleanup();
        return -1;
    }
    if (pthread_attr_init(&thread_attr) != 0) {

        cleanup();
        return -1;
    }
    if (pthread_attr_setschedpolicy(&thread_attr, SCHED_RR) != 0) {

        cleanup();
        return -1;
    }
    param.sched_priority = sched_get_priority_max(SCHED_RR);
    if (pthread_attr_setschedparam (&thread_attr, &param) != 0) {

        cleanup();
        return -1;
    }
    if (pthread_create(&pt_capture, &thread_attr, run_capture, NULL) != 0) {

        printf("Initialzing read thread failed\n");
        cleanup();
        return -1;
    }
    if (pthread_create(&pt_fwrite, NULL, run_writefile, NULL) != 0) {

        printf("Initialzing write thread failed\n");
        cleanup();
        return -1;
    }
    acquire_wake_lock(PARTIAL_WAKE_LOCK, lockid);

    /* Start Streaming */
    rc = full_write(audience_fd, startStreamCmd, sizeof(startStreamCmd));
    if (rc < 0) {

        printf("audience start stream command failed: %d\n", rc);
        cleanup();
        return rc;
    }

    /* Read back the cmd ack */
    full_read(audience_fd, buf, READ_ACK_BUF_SIZE);

    /* Let thread start recording */
    sem_post(&sem_start);

    printf("\nAudio streaming started, capturing for %d seconds\n", duration_in_sec);
    while (duration_in_sec > 0 && stop_requested != true) {

        sleep(1);
        duration_in_sec -= 1;
    }

    /* Stop Streaming */
    rc = full_write(audience_fd, stopStreamCmd, sizeof(stopStreamCmd));
    if (rc < 0) {

        printf("audience stop stream command failed: %d\n", rc);
        cleanup();
        return rc;
    }

    /* Because there is no way to know whether audience I2C buffer is empty, sleep is required. */
    usleep(THREAD_EXIT_WAIT_IN_US);
    stop_requested = true;

    printf("Stopping capture.\n");
    pthread_join(pt_capture, NULL);
    printf("Capture stopped.\n");
    pthread_join(pt_fwrite, NULL);

    printf("Total of %d bytes read\n", written_bytes);

    cleanup();
    return 0;
}
