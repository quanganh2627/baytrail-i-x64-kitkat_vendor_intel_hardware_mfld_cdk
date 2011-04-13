#include <stdio.h>
#define LOG_TAG "ALSAModule"
#include <utils/Log.h>
#include <signal.h>
#include "AudioModemControl.h"
#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#define AUDIO_AT_CHANNEL_NAME "/dev/gsmtty13"

int amc_voice();
int amc_bt();
int amc_adjust_volume(int volume);
int amc_mixing();
