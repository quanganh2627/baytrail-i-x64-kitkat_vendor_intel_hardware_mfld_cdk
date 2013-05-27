/*
 **
 ** Copyright 2013 Intel Corporation
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
 */
#include "ad_streamer.h"

#include <full_rw.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ctype.h>


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

/* Default values for optional command arguments */
#define DEFAULT_OUTPUT_FILE    "/data/ad_streamer_capture.bin"
#define DEFAULT_CAPTURE_DURATION_IN_SEC 5

/* Chip ID code */
#define AD_CHIP_ID_ES305     0x1008
#define AD_CHIP_ID_ES325     0x1101

/* Maximum number of channel to capture */
#define MAX_CAPTURE_CHANNEL 2
/* Maximum file path/name length in char, including terminal '0' */
#define MAX_FILE_PATH_SIZE 80

typedef enum {
    AUDIENCE_ES305,
    AUDIENCE_ES325,
    AUDIENCE_UNKNOWN
} chip_version_t;

static const char* chipVersion2String[] = {
    "es305",
    "es325",
    "Unknown"
};

static unsigned char setChannelCmd[AD_CMD_SIZE] = { 0x80, 0x28, 0x00, 0x03 };
static unsigned char chipIdCmd[AD_CMD_SIZE] = { 0x80, 0x0E, 0x00, 0x00 };
static chip_version_t chip_version = AUDIENCE_UNKNOWN;
static int audience_fd = -1;
static int outfile_fd = -1;
static int duration_in_sec = DEFAULT_CAPTURE_DURATION_IN_SEC;
static char fname[MAX_FILE_PATH_SIZE] = DEFAULT_OUTPUT_FILE;
/* Number of channel to be captured */
static unsigned int captureChannelsNumber = 0;
/* Channel IDs of the channels to be captured */
static unsigned int channels[MAX_CAPTURE_CHANNEL] = {0, 0};
/* Streaming handler */
static ad_streamer_handler *handler = NULL;



void cleanup(void) {

    if (audience_fd >= 0) {

        close(audience_fd);
        audience_fd = -1;
    }

    if (outfile_fd >= 0) {

        close(outfile_fd);
        outfile_fd = -1;
    }

    free_streaming_handler(handler);
}

int check_numeric(const char *number) {

    int index = 0;

    while (number[index] != '\0') {

        if (!isdigit(number[index++])) {

            return -1;
        }
    }
    return 0;
}


int select_audience_chip_id(void) {

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
            return -1;
        }
        /* Wait for command execution */
        usleep(AD_CMD_DELAY_US);
        /* Read back the response */
        rc = full_read(audience_fd, chipIdCmdResponse, sizeof(chipIdCmdResponse));
        if (rc < 0) {

            printf("Audience command response read failed (Chip ID): %s\n",
                   strerror(errno));
            return -1;
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

    printf("Audience chip selected: '%s'.\n", chipVersion2String[chip_version]);

    /* es325 specific setup */
    if (chip_version == AUDIENCE_ES325) {

        // According eS325 specifications, bit #15 of setChannel command must be set to 1 */
        setChannelCmd[2] = 0x80;
    }

    return status;
}

void display_cmd_help(void) {

    printf("Format: [-t seconds] [-f /path/filename] [-c chip] channelId [channelId]\n");
    printf("\nArguments Details\n");
    printf("\tchannelId:\tAudience channel to be captured (see channel IDs)\n");
    printf("\t[channelId]:\tOptionnal second Audience channel to be captured (see channel IDs)\n");
    printf("\tf (optional):\tOutput file path (default: %s)\n", DEFAULT_OUTPUT_FILE);
    printf("\tt (optional):\tCapture duration in seconds (default: %d)\n",
           DEFAULT_CAPTURE_DURATION_IN_SEC);
    printf("\tc (optional) (deprecated):\tforce chip selection in <%s|%s> (default: chip"
           "autodetection)\n\n",
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
           "Secondary Mic for 10 seconds to /sdcard/aud_stream.bin\n");
    printf("\nNotes\n");
    printf("\t* Dual channels capture requires the I2C bus to be overclocked to have enough"
           "bandwidth (refer to ad_streamer user guide).\n");
    printf("\t* This tool shall be used only while in call (CSV or VOIP) (refer to ad_streamer"
           "user guide).\n");
}

int parse_cmd_line(int argc, char** argv) {

    char* cvalue = NULL;
    int index;
    int c;

    opterr = 0;

    /* Parse optional parameters */
    while ((c = getopt (argc, argv, "t:f:c:")) != -1) {

        switch (c)
        {
            case 'f':
                cvalue = optarg;
                strlcpy(fname, cvalue, MAX_FILE_PATH_SIZE);
                break;
            case 'c':
                cvalue = optarg;
                if (strcmp(cvalue, "es305") == 0) {

                    chip_version = AUDIENCE_ES305;
                } else if (strcmp(cvalue, "es325") == 0) {

                    chip_version = AUDIENCE_ES325;
                } else {
                    display_cmd_help();
                    printf("\n*Please use valid chip value\n");
                    return -1;
                }
                break;
            case 't':
                // Cap the duration_in_sec to capture samples (in seconds)
                if (check_numeric(optarg)) {

                    display_cmd_help();
                    printf("\n*Please use valid numeric value for seconds\n");
                    return -1;
                }
                duration_in_sec = atoi(optarg);
                break;
            default:
                display_cmd_help();
                return -1;
        }
    }

    /* Non-optional arguments: channel ID to be captured */
    captureChannelsNumber = 0;
    for (index = optind; index < argc; index++) {

        if (check_numeric(argv[index])) {

            printf("\nERROR: please use valid numeric value for channel flag\n");
            display_cmd_help();
            return -1;
        }
        // Set streaming channel flag
        if (captureChannelsNumber < MAX_CAPTURE_CHANNEL) {

            channels[captureChannelsNumber] = atoi(argv[index]);
            captureChannelsNumber++;
        } else {

            printf("ERROR: Too many arguments\n");
            display_cmd_help();
            return -1;
        }
    }
    /* Check that at least one channel has to be recorded */
    if (captureChannelsNumber == 0) {

        printf("ERROR: Missing channel ID\n");
        display_cmd_help();
        return -1;
    }
    return 0;
}

int ad_streamer_cmd(int argc, char **argv) {

    int rc;

    /* Parse cmd line parameters */
    if (parse_cmd_line(argc, argv) == -1) {

        return -1;
    }

    /* Initialize */
    audience_fd = open(ES3X5_DEVICE_PATH, O_RDWR);

    if (audience_fd < 0) {

        printf("Cannot open %s\n", ES3X5_DEVICE_PATH);
        return -1;
    }

    /* Get the Audience FW version */
    rc = select_audience_chip_id();
    if (rc < 0) {

        cleanup();
        return -1;
    };

    outfile_fd = open(fname, O_CREAT | O_WRONLY);
    if (outfile_fd < 0) {

        printf("Cannot open output file %s: %s\n", fname, strerror(errno));
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
            return -1;
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
                return -1;
            }
        }
    }

    /* Let start recording */
    handler = alloc_streaming_handler(audience_fd, outfile_fd);
    rc = start_streaming(handler);
    if (rc < 0) {

        printf("Start streaming has failed.\n");
        cleanup();
        return -1;
    }

    printf("\nAudio streaming started, capturing for %d seconds\n", duration_in_sec);
    sleep(duration_in_sec);

    /* Stop Streaming */
    rc = stop_streaming(handler);
    if (rc < 0) {

        printf("audience stop stream command failed: %d\n", rc);
        cleanup();
        return -1;
    }

    cleanup();
    return 0;
}
