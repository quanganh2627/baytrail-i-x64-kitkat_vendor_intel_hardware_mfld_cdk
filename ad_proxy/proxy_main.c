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
 ** Author:
 ** Zhang, Dongsheng <dongsheng.zhang@intel.com>
 **
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <termios.h>
#include <pthread.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/ioctl.h>

#define LOG_TAG "ad_proxy"
#include "cutils/log.h"

#include "ad_i2c.h"
#include "ad_usb_tty.h"

#define AD_VERSION "V1.3"


#define TTY_RDY_WAIT	       100   /* in ms */
#define AUDIENCE_ACK_US_DELAY  20000 /* in us */

#define AD_WRITE_DATA_BLOCK_OPCODE 0x802F

enum {
    ID_STD = 0,
    ID_TTY,
    ID_NB
};

typedef enum {
    STATE_WAIT_COMMAND,
    STATE_READ_ACK,
    STATE_WAIT_DATA_BLOCK
} protocol_state_t;

/**
 * Type to contain a 4-bytes Audience command
 */
typedef struct {
    unsigned char bytes[4];
} audience_command_t;

#define ACM_TTY "/dev/ttyGS0"

#define CMD_SIZE 64
char cmd[CMD_SIZE];
volatile int quit = 0;

int fds[ID_NB] = {-1};
struct pollfd polls[ID_NB];

pthread_t proxy_thread;

static void ad_signal_handler(int signal)
{
    ALOGD("Signal [%d]", signal);
    /* Signal exit to the proxy thread */
    quit = 1;
    /* Wait for the proxy thread exit */
    pthread_join(proxy_thread, NULL);

    if (polls[ID_TTY].fd >= 0) {
        close(polls[ID_TTY].fd);
    }

    ad_i2c_exit();

    exit(0);
}

/**
 * Returns the Audience command opcode. The opcode is in the upper 16bits of the command.
 *
 * @param[in] cmd audience_command_t
 *
 * @return integer value of the command 16bits opcode
 */
static unsigned int get_command_opcode(audience_command_t* cmd)
{
    // Opcode is 16bits, HI in byte #0, LO in byte #1
    return cmd->bytes[0] << 8 | cmd->bytes[1];
}
/**
 * Returns the Audience command argument. The argument is in the lower 16bits of the command.
 *
 * @param[in] cmd audience_command_t
 *
 * @return integer value of the command 16bits argument
 */
static unsigned int get_command_arg(audience_command_t* cmd)
{
    // Arg is 16bits, HI in byte #2, LO in byte #3
    return cmd->bytes[2] << 8 | cmd->bytes[3];
}

static void* ad_proxy_worker(void* data)

{
    static protocol_state_t state = STATE_WAIT_COMMAND;

    int data_block_session = 0;
    size_t data_block_size = 0;

    int status;

    ALOGD("Start proxy worker thread");
    while (!quit) {

        ALOGD("State: %d", state);
        switch (state) {
            default:
            case STATE_WAIT_COMMAND: {
                audience_command_t audience_cmb_buf;
                // Read a command from AuViD
                status = read(polls[ID_TTY].fd, audience_cmb_buf.bytes,
                              sizeof(audience_cmb_buf.bytes));
                if (status != sizeof(audience_cmb_buf.bytes)) {

                    ALOGE("%s Read CMD from AuViD failed\n", __func__);
                    quit = 1;
                    break;
                }
                // Forward the command to the chip
                status = ad_i2c_write(audience_cmb_buf.bytes, sizeof(audience_cmb_buf.bytes));
                if (status != sizeof(audience_cmb_buf.bytes)) {

                    ALOGE("%s Write CMD to chip failed\n", __func__);
                    quit = 1;
                    break;
                }
                // Is it a Data Block command ?
                if (get_command_opcode(&audience_cmb_buf) == AD_WRITE_DATA_BLOCK_OPCODE) {

                    data_block_session = 1;
                    // Block Size is in 16bits command's arg
                    data_block_size = get_command_arg(&audience_cmb_buf);

                    ALOGD("Starting Data Block session for %d bytes", data_block_size);
                }
                // Need to handle the chip ACK response
                state = STATE_READ_ACK;
                break;
            }
            case STATE_READ_ACK: {
                audience_command_t audience_cmb_ack_buf;
                // Wait before to read ACK
                usleep(AUDIENCE_ACK_US_DELAY);
                status = ad_i2c_read(audience_cmb_ack_buf.bytes,
                                     sizeof(audience_cmb_ack_buf.bytes));
                if (status != sizeof(audience_cmb_ack_buf.bytes)) {

                    ALOGE("%s Read ACK from chip failed\n", __func__);
                    quit = 1;
                    break;
                }
                // Send back the ACK to AuViD
                status = write(polls[ID_TTY].fd, audience_cmb_ack_buf.bytes,
                               sizeof(audience_cmb_ack_buf.bytes));
                if (status != sizeof(audience_cmb_ack_buf.bytes)) {

                    ALOGE("%s Write ACK to AuVid failed\n", __func__);
                    quit = 1;
                    break;
                }
                if (data_block_session == 1) {

                    // Next step is Data Block to handle
                    state = STATE_WAIT_DATA_BLOCK;
                } else {

                    // Next step is to handle a next command
                    state = STATE_WAIT_COMMAND;
                }
                break;
            }
            case STATE_WAIT_DATA_BLOCK: {
                unsigned char* data_block_payload = (unsigned char *) malloc(data_block_size);
                if (data_block_payload == NULL) {

                    ALOGE("%s Read Data Block memory allocation failed.\n", __func__);
                    quit = 1;
                    break;
                }
                // Read data block from AuViD
                status = read(polls[ID_TTY].fd, data_block_payload, data_block_size);
                if (status != (int)data_block_size) {

                    ALOGE("%s Read Data Block failed\n", __func__);
                    free(data_block_payload);
                    quit = 1;
                    break;
                }

                // Forward these bytes to the chip
                status = ad_i2c_write(data_block_payload, data_block_size);
                free(data_block_payload);

                if (status != (int)data_block_size) {

                    ALOGE("%s Write Data Block failed\n", __func__);
                    quit = 1;
                    break;
                }
                // No more in data block session
                data_block_session = 0;
                // Need to handle ACK of datablock session
                state = STATE_READ_ACK;
                break;
            }
        }
    }
    ALOGD("Exit Working Thread");
    pthread_exit(NULL);
    /* To avoid warning, must return a void* */
    return NULL;
}

static void dump_command_help(void)
{
    fprintf(stderr, "Commands:\n" \
                    "Q\t\tQuit\n" \
                    "Dn\t\tSet dump level to n (n in [0..2] 0:No dump; 1:dump packet; 2:dump payload)\n" \
                    "V\t\tPrint ad_proxy version\n" \
                    "W0xDEADBEEF\tWrite 0xDEADBEEF to es305b\n" \
                    "R\t\tRead a 32bits word from es305b\n");
}

int main(int argc, char *argv[])
{
#define ARG_SIZE	16
    int i = 0;
    int tmp;
    char p[ARG_SIZE];
    int ret = 0;
    int readSize;
    int pollResult;
    int i2c_delay = -1;
    struct sigaction sigact;
    pthread_attr_t attr;
    int thread_started = 0;

    ALOGD("-->ad_proxy startup (Version %s)", AD_VERSION);

    // Set signal handler
    if (sigemptyset(&sigact.sa_mask) == -1) {
        exit(-1);
    }
    sigact.sa_flags = 0;
    sigact.sa_handler = ad_signal_handler;

    if (sigaction(SIGHUP, &sigact, NULL) == -1) {
        exit(-1);
    }
    if (sigaction(SIGTERM, &sigact, NULL) == -1) {
        exit(-1);
    }

    argc--;
    i++;
    do {
        // Parse the i2c operation wait parameter.
        if (argc > 0) {
            strncpy(p, argv[i], ARG_SIZE - 1);
            p[ARG_SIZE - 1] = '\0';
            if (p[0] ==  '-' && p[1] == 'i' && p[2] == 'w') {
                tmp = atoi(p+3);
                if (tmp >= 0 && tmp <= AD_I2C_OP_MAX_DELAY)
                    i2c_delay = tmp;
                ALOGD("i2c operation wait request = %s]; i2c operation wait set = %d", p+3, i2c_delay);
            // Parse the usage request.
            } else {
                fprintf(stdout, "Audience proxy %s\nUsage:\n", AD_VERSION);
                fprintf(stdout, "-iw[i2c operation wait (0 to %d us)]\n", AD_I2C_OP_MAX_DELAY);
                goto EXIT;
            }
            argc--;
            i++;
        }
    } while (argc > 0);

    // set the standard input poll id.
    polls[ID_STD].fd = 0;
    polls[ID_STD].events = POLLIN;
    polls[ID_STD].revents = 0;

    // open the acm tty.
    polls[ID_TTY].fd = ad_open_tty(ACM_TTY,  B115200);
    if (polls[ID_TTY].fd < 0) {
        ALOGE("%s open %s failed (%s)\n",  __func__, ACM_TTY, strerror(errno));
        ret = -1;
        goto EXIT;
    }

    // open the audience device node.
    if (ad_i2c_init(i2c_delay) < 0) {
        ALOGE("%s open %s failed (%s)\n", __func__, AD_DEV_NODE, strerror(errno));
        goto EXIT;
    }

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    if (pthread_create(&proxy_thread, &attr, ad_proxy_worker, NULL) != 0) {
        ALOGE("%s Thread creation has failed (%s)\n", __func__, strerror(errno));
        goto EXIT;
    }
    pthread_attr_destroy(&attr);
    thread_started = 1;

    // main thread to wait for user's command.
    cmd[0] = 0;
    while(!quit) {

        fprintf(stderr, ">:");
        pollResult = poll(&polls[ID_STD], 1, -1);
        if (pollResult <= 0) {
            if (errno != EINTR) {
                ALOGE("%s poll failed (%s)\n", __func__, strerror(errno));
            }
        }

        if (polls[ID_STD].revents & POLLIN) {
            readSize = read(polls[ID_STD].fd, cmd, CMD_SIZE);
            if (readSize > 0) {
                cmd[readSize-1] = 0;
            } else {
                continue;
            }
        }

        switch (cmd[0]) {
        case 'q':
        case 'Q':
            fprintf(stderr, "Quit\n");
            quit = 1;
            break;
         case 'v':
         case 'V':
            fprintf(stderr, "%s\n", AD_VERSION);
            break;
         case 'w':
         case 'W':
            {
                unsigned char status = 0;
                long long int w_d = 0;
                unsigned char w_buf[4];

                cmd[11] = 0;
                if (strlen(cmd + 1) == 10) {
                    w_d = strtoll(cmd+1, NULL, 16);
                    if (w_d != LLONG_MIN && w_d != LLONG_MAX && w_d >= 0) {
                        w_buf[3] = (w_d & 0xff);
                        w_buf[2] = (w_d >> 8) & 0xff;
                        w_buf[1] = (w_d >> 16) & 0xff;
                        w_buf[0] = (w_d >> 24) & 0xff;

                        fprintf(stderr, "W: 0x%02x%02x%02x%02x\n", w_buf[0], w_buf[1], w_buf[2], w_buf[3]);
                        status = ad_i2c_write(w_buf, 4);
                        fprintf(stderr, "W: status %s (%d)\n", status == 4 ? "ok":"error", status);
                        break;
                    }
                }
                fprintf(stderr, "Invalid write command.\n\n");
                dump_command_help();
            }
            break;
         case 'r':
         case 'R':
            {
                unsigned char r_buf[4];
                unsigned char status;
                status = ad_i2c_read(r_buf, 4);
                if (status == 4)
                    fprintf(stderr, "R: 0x%02x%02x%02x%02x\n", r_buf[0], r_buf[1], r_buf[2], r_buf[3]);
                else
                    fprintf(stderr, "Read error (%d)\n", status);
            }
            break;
         default:
            dump_command_help();
            break;
        }
    }

EXIT:
    /* Wait for the proxy thread exit */
    if (thread_started) {
        ALOGD("Waiting working thread");
        pthread_join(proxy_thread, NULL);
    }

    if (polls[ID_TTY].fd >= 0) {
        close(polls[ID_TTY].fd);
    }
    ad_i2c_exit();

    ALOGD("-->ad_proxy exit(%d)", ret);

    pthread_exit(NULL);
    return ret;
}
