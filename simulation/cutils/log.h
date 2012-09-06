/*
 **
 ** Copyright 2010 Intel Corporation
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
 */

/* LOG Level policy:
 *
 * - ERROR:
 *		Something fatal has happened. User-visible consequences not recoverable
 *		without explicitly deleting data, uninstalling applications, wiping the
 *		data partitions or reflashing. Always logged.
 *		Mis-use of the library level.
 * - WARNING:
 *		Something serious and unexpected happened. User-visible consequences
 *		recoverable without data loss: waiting, restarting an app, rebooting the
 *		device. Always logged.
 * - INFORMATIVE:
 *		Something interesting to most people happened. Situation detected likely
 *		to have widespread impact,level always logged.
 * - DEBUG:
 *		Note what is happening on the device that could be relevant to
 *		investigate and debug unexpected behaviors. Logged, even on release
 *		builds. a define should be provided to locally compiled out the log.
 * - VERBOSE:
 *		Used for everything else. Stripped out of release builds. A define
 *		should be provided to locally compiled out the log.*/


/* Prior including this file, use the below defines to enable/disable
 * local LOG_DEBUG, LOG_VERBOSE, and/or to treat VERBOSE LOG as DEBUG ones:
 *	#define LOCAL_LOGD 1
 *	#define LOCAL_LOGV 1
 *	#define KEEP_LOCAL_LOGV 1*/


#ifndef LOG_H
#define LOG_H

#include <stdio.h>

#define LOGD(...) ((void)fprintf(stdout, __VA_ARGS__), (void)fprintf(stdout, "\n"))
#define LOGE(...) ((void)fprintf(stderr, __VA_ARGS__), (void)fprintf(stderr, "\n"))
#define LOGW(...) ((void)fprintf(stderr, __VA_ARGS__), (void)fprintf(stderr, "\n"))
#define LOGV(...) ((void)fprintf(stderr, __VA_ARGS__), (void)fprintf(stderr, "\n"))


#endif /*LOG_H*/

