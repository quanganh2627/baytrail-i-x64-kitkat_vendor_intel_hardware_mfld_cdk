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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Start the Audience Streamer with arguments from command line. The streamer will run the streaming
 * operation from the Audience chip as per commend line arguments.
 *
 * @param[in] argc Number of arguments
 * @param[in] argv Array of string arguments
 *
 * @return -1 in case the streamer stops on error, 0 otherwise.
 */
int ad_streamer_cmd(int argc, char **argv);

#ifdef __cplusplus
}
#endif
