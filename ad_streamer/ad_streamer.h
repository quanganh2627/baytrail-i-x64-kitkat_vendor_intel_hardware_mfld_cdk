/*
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
#pragma once

/**
 * The name of the Audience driver
 */
#define ES3X5_DEVICE_PATH    "/dev/audience_es305"

/* Audience command size in bytes */
#define AD_CMD_SIZE              4
/* Delay to wait after a command before to read the chip response (us),
 * as per Audience recommendations. */
#define AD_CMD_DELAY_US          20000

/**
 * Start the streaming on Audience chip sending the start command.
 * The chip must be configured for the desired stream setup before to call start_streaming().
 * The bytes streamed from the chip are written to the file descriptor provided as argument.
 * The stream read operations will be done in a dedicated thread created by the function
 * start_streaming().
 * The write operations to the provided file descriptor will be done in a dedicated thread created
 * by the function start_streaming().
 * The streaming will last until the function stop_streaming() is called.
 *
 * @param[in] audience_fd File descriptor of the Audience driver
 * @param[in] outfile_fd File descriptor on which the bytes from streaming have to be written.
 * The file must be opened with write access.
 *
 * @return -1 in case of error, 0 otherwise
 */
int start_streaming(int audience_fd, int outfile_fd);

/**
 * Stop the streaming operation previously started with start_streaming().
 * In case no streaming is on going, the function does nothing and returns 0.
 * The stop command is sent to the Audience chip and then the streaming threads created by
 * the function start_streaming() are stopped.
 *
 * @return -1 in case of error, 0 otherwise
 */
int stop_streaming(void);
