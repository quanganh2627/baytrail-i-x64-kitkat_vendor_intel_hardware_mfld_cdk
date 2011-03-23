
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

#ifndef LOG_TAG
#define LOG_TAG ""
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef ANDROID /*#ifdef __linux*/

#include <stdio.h>
#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_DEBUG
#endif

#define LOG_LEVEL_MAX	 6
#define LOG_LEVEL_VERBOSE 5
#define LOG_LEVEL_DEBUG   4
#define LOG_LEVEL_INFO	3
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_ERROR   1
#define LOG_LEVEL_MIN	 0

#if ((LOCAL_LOGV == 1) && (LOG_LEVEL_VERBOSE <= LOG_LEVEL))
#define LOGV(format, args...) {\
			fprintf(stdout, "V %s " format "  (%s - %d)\n", \
							LOG_TAG, ##args, __func__, __LINE__); \
			fflush(stdout); }
#else
#define LOGV(...)((void)0)
#endif

#if ((LOCAL_LOGD == 1) && (LOG_LEVEL_DEBUG <= LOG_LEVEL))
#define LOGD(format, args...) {\
			fprintf(stdout, "D %s " format "  (%s - %d)\n", \
							LOG_TAG, ##args, __func__, __LINE__); \
			fflush(stdout); }
#else
#define LOGD(...)((void)0)
#endif

#if LOG_LEVEL_INFO <= LOG_LEVEL
#define LOGI(format, args...) {\
			fprintf(stdout, "I %s " format "  (%s - %d)\n", \
							LOG_TAG, ##args, __func__, __LINE__); \
			fflush(stdout); }
#else
#define LOGI(...)((void)0)
#endif

#if LOG_LEVEL_WARNING <= LOG_LEVEL
#define LOGW(format, args...) {\
			fprintf(stdout, "W %s " format "  (%s - %d)\n", \
							LOG_TAG, ##args, __func__, __LINE__); \
			fflush(stdout); }
#else
#define LOGW(...)((void)0)
#endif

#if LOG_LEVEL_ERROR <= LOG_LEVEL
#define LOGE(format, args...) {\
			fprintf(stdout, "E %s " format "  (%s - %d)\n", \
							LOG_TAG, ##args, __func__, __LINE__); \
			fflush(stdout); }
#else
#define LOGE(...)((void)0)
#endif


#if KEEP_LOCAL_LOGV == 1
#undef LOGV
#define LOGV LOGD
#endif

#endif /*__linux*/


#ifdef ANDROID

#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef HAVE_PTHREADS
#include <pthread.h>
#endif
#include <stdarg.h>

#include <cutils/uio.h>
#include <cutils/logd.h>

#if LOCAL_LOGV == 1
#define LOGV(format, args...)((void)LOG(LOG_VERBOSE, LOG_TAG, format \
							"  (%s - %d)\n", ##args, __func__, __LINE__))
#else
#define LOGV(...)  ((void)0)
#endif

#if LOCAL_LOGD == 1
#define LOGD(format, args...)((void)LOG(LOG_DEBUG, LOG_TAG, format \
							"  (%s - %d)\n", ##args, __func__, __LINE__))
#else
#define LOGD(...)  ((void)0)
#endif

#ifndef LOGI
#define LOGI(format, args...)((void)LOG(LOG_INFO, LOG_TAG, format \
							"  (%s - %d)\n", ##args, __func__, __LINE__))
#endif

#ifndef LOGW
#define LOGW(format, args...)((void)LOG(LOG_WARN, LOG_TAG, format \
							"  (%s - %d)\n", ##args, __func__, __LINE__))
#endif

#ifndef LOGE
#define LOGE(format, args...)((void)LOG(LOG_ERROR, LOG_TAG, format \
							"  (%s - %d)\n", ##args, __func__, __LINE__))
#endif

#ifndef LOG
#define LOG(priority, tag, fmt...) \
			__android_log_print(ANDROID_##priority, tag, fmt)
#endif

#if KEEP_LOCAL_LOGV == 1
#undef LOGV
#define LOGV LOGD
#endif

#endif /* ANDROID*/

#ifdef __cplusplus
}
#endif

#endif /*LOG_H*/

