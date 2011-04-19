/*
 * Copyright 2008, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <hardware/wifi.h>
#include "libwpa_client/wpa_ctrl.h"

#define LOG_TAG "WifiHW"
#include "cutils/log.h"
#include "cutils/memory.h"
#include "cutils/misc.h"
#include "cutils/properties.h"
#include "private/android_filesystem_config.h"
#ifdef HAVE_LIBC_SYSTEM_PROPERTIES
#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>
#endif

static struct wpa_ctrl *ctrl_conn;
static struct wpa_ctrl *monitor_conn;

extern int do_dhcp();
extern int ifc_init();
extern void ifc_close();
extern char *dhcp_lasterror();
extern void get_dhcp_info();
extern int init_module(void *, unsigned long, const char *);
extern int delete_module(const char *, unsigned int);

static char iface[PROPERTY_VALUE_MAX];
// TODO: use new ANDROID_SOCKET mechanism, once support for multiple
// sockets is in

#ifndef WIFI_COMPAT_DRIVER_MODULE_PATH
#define WIFI_COMPAT_DRIVER_MODULE_PATH		"/lib/modules/compat.ko"
#endif
#ifndef WIFI_CFG80211_DRIVER_MODULE_PATH
#define WIFI_CFG80211_DRIVER_MODULE_PATH	"/lib/modules/cfg80211.ko"
#endif
#ifndef WIFI_MAC80211_DRIVER_MODULE_PATH
#define WIFI_MAC80211_DRIVER_MODULE_PATH	"/lib/modules/mac80211.ko"
#endif
#ifndef WIFI_WL12XX_DRIVER_MODULE_PATH
#define WIFI_WL12XX_DRIVER_MODULE_PATH		"/lib/modules/wl12xx.ko"
#endif
#ifndef WIFI_WL12XX_SDIO_DRIVER_MODULE_PATH
#define WIFI_WL12XX_SDIO_DRIVER_MODULE_PATH	"/lib/modules/wl12xx_sdio.ko"
#endif
#ifndef WIFI_COMPAT_DRIVER_MODULE_NAME
#define WIFI_COMPAT_DRIVER_MODULE_NAME		"compat"
#endif
#ifndef WIFI_CFG80211_DRIVER_MODULE_NAME
#define WIFI_CFG80211_DRIVER_MODULE_NAME	"cfg80211"
#endif
#ifndef WIFI_MAC80211_DRIVER_MODULE_NAME
#define WIFI_MAC80211_DRIVER_MODULE_NAME	"mac80211"
#endif
#ifndef WIFI_WL12XX_DRIVER_MODULE_NAME
#define WIFI_WL12XX_DRIVER_MODULE_NAME		"wl12xx"
#endif
#ifndef WIFI_WL12XX_SDIO_DRIVER_MODULE_NAME
#define WIFI_WL12XX_SDIO_DRIVER_MODULE_NAME	"wl12xx_sdio"
#endif
#ifndef WIFI_COMPAT_DRIVER_MODULE_ARG
#define WIFI_COMPAT_DRIVER_MODULE_ARG			""
#endif
#ifndef WIFI_CFG80211_DRIVER_MODULE_ARG
#define WIFI_CFG80211_DRIVER_MODULE_ARG			""
#endif
#ifndef WIFI_MAC80211_DRIVER_MODULE_ARG
#define WIFI_MAC80211_DRIVER_MODULE_ARG			""
#endif
#ifndef WIFI_WL12XX_DRIVER_MODULE_ARG
#define WIFI_WL12XX_DRIVER_MODULE_ARG			""
#endif
#ifndef WIFI_WL12XX_SDIO_DRIVER_MODULE_ARG
#define WIFI_WL12XX_SDIO_DRIVER_MODULE_ARG		""
#endif
#define WIFI_TEST_INTERFACE			"sta"
#ifndef WIFI_WLAN0_UP
#define WIFI_WLAN0_UP				"ifcfg_mac80211"
#endif

static const char IFACE_DIR[] = "/data/system/wpa_supplicant";
static const char COMPAT_DRIVER_MODULE_NAME[] = WIFI_COMPAT_DRIVER_MODULE_NAME;
static const char CFG80211_DRIVER_MODULE_NAME[] = WIFI_CFG80211_DRIVER_MODULE_NAME;
static const char MAC80211_DRIVER_MODULE_NAME[] = WIFI_MAC80211_DRIVER_MODULE_NAME;
static const char WL12XX_DRIVER_MODULE_NAME[] = WIFI_WL12XX_DRIVER_MODULE_NAME;
static const char WL12XX_SDIO_DRIVER_MODULE_NAME[] = WIFI_WL12XX_SDIO_DRIVER_MODULE_NAME;
static const char COMPAT_DRIVER_MODULE_PATH[] = WIFI_COMPAT_DRIVER_MODULE_PATH;
static const char CFG80211_DRIVER_MODULE_PATH[] = WIFI_CFG80211_DRIVER_MODULE_PATH;
static const char MAC80211_DRIVER_MODULE_PATH[] = WIFI_MAC80211_DRIVER_MODULE_PATH;
static const char WL12XX_DRIVER_MODULE_PATH[] = WIFI_WL12XX_DRIVER_MODULE_PATH;
static const char WL12XX_SDIO_DRIVER_MODULE_PATH[] = WIFI_WL12XX_SDIO_DRIVER_MODULE_PATH;
static const char COMPAT_DRIVER_MODULE_ARG[] = WIFI_COMPAT_DRIVER_MODULE_ARG;
static const char CFG80211_DRIVER_MODULE_ARG[] = WIFI_CFG80211_DRIVER_MODULE_ARG;
static const char MAC80211_DRIVER_MODULE_ARG[] = WIFI_MAC80211_DRIVER_MODULE_ARG;
static const char WL12XX_DRIVER_MODULE_ARG[] = WIFI_WL12XX_DRIVER_MODULE_ARG;
static const char WL12XX_SDIO_DRIVER_MODULE_ARG[] = WIFI_WL12XX_SDIO_DRIVER_MODULE_ARG;
static const char SUPPLICANT_NAME[]     = "wpa_supplicant";
static const char SUPP_PROP_NAME[]      = "init.svc.wpa_supplicant";
static const char SUPP_CONFIG_TEMPLATE[]= "/system/etc/wifi/wpa_supplicant.conf";
static const char SUPP_CONFIG_FILE[]    = "/data/misc/wifi/wpa_supplicant.conf";
static const char WLAN0_UP []		= WIFI_WLAN0_UP;

static int insmod(const char *filename, const char *args)
{
    void *module;
    unsigned int size;
    int ret;

    module = load_file(filename, &size);
    if (!module)
        return -1;
    ret = init_module(module, size, args);

    free(module);

    return ret;
}

static int rmmod(const char *modname)
{
    int ret = -1;
    int maxtry = 10;

    while (maxtry-- > 0) {
        ret = delete_module(modname, O_NONBLOCK | O_EXCL);
        if (ret < 0 && errno == EAGAIN)
            usleep(500000);
        else
            break;
    }

    if (ret != 0)
        LOGD("Unable to unload driver module \"%s\": %s\n",
             modname, strerror(errno));
    return ret;
}

int do_dhcp_request(int *ipaddr, int *gateway, int *mask,
                    int *dns1, int *dns2, int *server, int *lease) {
    /* For test driver, always report success */
    if (strcmp(iface, WIFI_TEST_INTERFACE) == 0)
        return 0;

    if (ifc_init() < 0)
        return -1;

    if (do_dhcp(iface) < 0) {
        ifc_close();
        return -1;
    }
    ifc_close();
    get_dhcp_info(ipaddr, gateway, mask, dns1, dns2, server, lease);
    return 0;
}

const char *get_dhcp_error_string() {
    return dhcp_lasterror();
}

int wifi_unload_driver()
{
	int count = 20; /* wait at most 10 seconds for completion */

	if ((rmmod(WL12XX_SDIO_DRIVER_MODULE_NAME) < 0) ||
	(rmmod(WL12XX_DRIVER_MODULE_NAME) < 0) ||
	(rmmod(MAC80211_DRIVER_MODULE_NAME) < 0) ||
	(rmmod(CFG80211_DRIVER_MODULE_NAME) < 0) ||
	(rmmod(COMPAT_DRIVER_MODULE_NAME) < 0))
		return -1;
	return 0;
}

int wifi_load_driver()
{
	char driver_status[PROPERTY_VALUE_MAX];
	char if_status[PROPERTY_VALUE_MAX];
	int count = 100; /* wait at most 20 seconds for completion */


	if ((insmod(COMPAT_DRIVER_MODULE_PATH, COMPAT_DRIVER_MODULE_ARG) < 0)     ||
	    (insmod(CFG80211_DRIVER_MODULE_PATH, CFG80211_DRIVER_MODULE_ARG) < 0) ||
	    (insmod(MAC80211_DRIVER_MODULE_PATH, MAC80211_DRIVER_MODULE_ARG) < 0) ||
	    (insmod(WL12XX_DRIVER_MODULE_PATH, WL12XX_DRIVER_MODULE_ARG) < 0)     ||
	    (insmod(WL12XX_SDIO_DRIVER_MODULE_PATH, WL12XX_SDIO_DRIVER_MODULE_ARG) < 0))
		return -1;

	LOGD("wifi_load_driver(): wifi modules installed");
	property_set("ctl.start",WLAN0_UP);
	sched_yield();
	return 0;
}

int ensure_config_file_exists()
{
    char buf[2048];
    int srcfd, destfd;
    int nread;

    if (access(SUPP_CONFIG_FILE, R_OK|W_OK) == 0) {
        return 0;
    } else if (errno != ENOENT) {
        LOGE("Cannot access \"%s\": %s", SUPP_CONFIG_FILE, strerror(errno));
        return -1;
    }

    srcfd = open(SUPP_CONFIG_TEMPLATE, O_RDONLY);
    if (srcfd < 0) {
        LOGE("Cannot open \"%s\": %s", SUPP_CONFIG_TEMPLATE, strerror(errno));
        return -1;
    }

    destfd = open(SUPP_CONFIG_FILE, O_CREAT|O_WRONLY, 0660);
    if (destfd < 0) {
        close(srcfd);
        LOGE("Cannot create \"%s\": %s", SUPP_CONFIG_FILE, strerror(errno));
        return -1;
    }

    while ((nread = read(srcfd, buf, sizeof(buf))) != 0) {
        if (nread < 0) {
            LOGE("Error reading \"%s\": %s", SUPP_CONFIG_TEMPLATE, strerror(errno));
            close(srcfd);
            close(destfd);
            unlink(SUPP_CONFIG_FILE);
            return -1;
        }
        write(destfd, buf, nread);
    }

    close(destfd);
    close(srcfd);

    if (chown(SUPP_CONFIG_FILE, AID_SYSTEM, AID_WIFI) < 0) {
        LOGE("Error changing group ownership of %s to %d: %s",
             SUPP_CONFIG_FILE, AID_WIFI, strerror(errno));
        unlink(SUPP_CONFIG_FILE);
        return -1;
    }
    return 0;
}

int wifi_start_supplicant(void)
{
    char supp_status[PROPERTY_VALUE_MAX] = {'\0'};
    int count = 200; /* wait at most 20 seconds for completion */
#ifdef HAVE_LIBC_SYSTEM_PROPERTIES
    const prop_info *pi;
    unsigned serial = 0;
#endif

    /* Check whether already running */
    if (property_get(SUPP_PROP_NAME, supp_status, NULL)
            && strcmp(supp_status, "running") == 0) {
        LOGE("wifi_start_supplicant(): wifi already running, return 0");
        return 0;
    }

    /* Before starting the daemon, make sure its config file exists */
    if (ensure_config_file_exists() < 0) {
        LOGE("wifi_start_supplicant(): Config file missing, Wi-Fi will not be enabled");
        return -1;
    }

    /* Clear out any stale socket files that might be left over. */
    wpa_ctrl_cleanup();

#ifdef HAVE_LIBC_SYSTEM_PROPERTIES
    /*
     * Get a reference to the status property, so we can distinguish
     * the case where it goes stopped => running => stopped (i.e.,
     * it start up, but fails right away) from the case in which
     * it starts in the stopped state and never manages to start
     * running at all.
     */
    pi = __system_property_find(SUPP_PROP_NAME);
    if (pi != NULL) {
        serial = pi->serial;
    }
#endif
    property_set("ctl.start", SUPPLICANT_NAME);
    sched_yield();

    LOGD("wifi_start_supplicant() : waiting for wlan0 up ,sleep 2 seconds");
    usleep(2000000);

    while (count-- > 0) {
#ifdef HAVE_LIBC_SYSTEM_PROPERTIES
        if (pi == NULL) {
            pi = __system_property_find(SUPP_PROP_NAME);
        }
        if (pi != NULL) {
            __system_property_read(pi, NULL, supp_status);
            if (strcmp(supp_status, "running") == 0) {
                LOGI("wifi_start_supplicant() : supp_status=%s", supp_status);
                return 0;
            } else if (pi->serial != serial &&
                    strcmp(supp_status, "stopped") == 0) {
                    LOGE("wifi_start_supplicant() error: supp_status=%s", supp_status);
                    return -1;
            }
        }
#else
        if (property_get(SUPP_PROP_NAME, supp_status, NULL)) {
            if (strcmp(supp_status, "running") == 0) {
                LOGI("wifi_start_supplicant() : supp_status=%s", supp_status);
                return 0;
            }
        }
#endif
        usleep(100000);
    }
    return -1;
}

int wifi_stop_supplicant(void)
{
    char supp_status[PROPERTY_VALUE_MAX] = {'\0'};
    int count = 50; /* wait at most 5 seconds for completion */

    /* Check whether supplicant already stopped */
    if (property_get(SUPP_PROP_NAME, supp_status, NULL)
        && strcmp(supp_status, "stopped") == 0) {
        return 0;
    }

    property_set("ctl.stop", SUPPLICANT_NAME);
    sched_yield();

    while (count-- > 0) {
        if (property_get(SUPP_PROP_NAME, supp_status, NULL)) {
            if (strcmp(supp_status, "stopped") == 0)
                return 0;
        }
        usleep(100000);
    }
    return -1;
}

int wifi_open_supplicant(void)
{
    char ifname[256];
    char supp_status[PROPERTY_VALUE_MAX] = {'\0'};

    /* Make sure supplicant is running */
    if (!property_get(SUPP_PROP_NAME, supp_status, NULL)
            || strcmp(supp_status, "running") != 0) {
        LOGE("Supplicant not running, cannot connect");
        return -1;
    }

    property_get("wifi.interface", iface, WIFI_TEST_INTERFACE);

    if (access(IFACE_DIR, F_OK) == 0) {
        snprintf(ifname, sizeof(ifname), "%s/%s", IFACE_DIR, iface);
    } else {
        strlcpy(ifname, iface, sizeof(ifname));
    }

    ctrl_conn = wpa_ctrl_open(ifname);
    if (ctrl_conn == NULL) {
        LOGE("Unable to open connection to supplicant on \"%s\": %s",
             ifname, strerror(errno));
        return -1;
    }
    monitor_conn = wpa_ctrl_open(ifname);
    if (monitor_conn == NULL) {
        LOGE("wpa_ctrl_open() failed on monitor_conn: returned \"NULL\"");
        wpa_ctrl_close(ctrl_conn);
        ctrl_conn = NULL;
        return -1;
    }
    if (wpa_ctrl_attach(monitor_conn) != 0) {
        wpa_ctrl_close(monitor_conn);
        wpa_ctrl_close(ctrl_conn);
        ctrl_conn = monitor_conn = NULL;
        return -1;
    }
    return 0;
}

int wifi_send_command(const char *cmd, char *reply, size_t *reply_len)
{
    int ret;

    if (ctrl_conn == NULL) {
        LOGV("Not connected to wpa_supplicant - \"%s\" command dropped.\n", cmd);
        return -1;
    }
    ret = wpa_ctrl_request(ctrl_conn, cmd, strlen(cmd), reply, reply_len, NULL);
    LOGD("wifi_send_command(): %s, return: %s\n", cmd, reply);

    if (ret == -2) {
        LOGD("'%s' command timed out.\n", cmd);
        return -2;
    } else if (ret < 0 || strncmp(reply, "FAIL", 4) == 0) {
        return -1;
    }
    if (strncmp(cmd, "PING", 4) == 0) {
        reply[*reply_len] = '\0';
    }
    return 0;
}

int wifi_wait_for_event(char *buf, size_t buflen)
{
    size_t nread = buflen - 1;
    int fd;
    fd_set rfds;
    int result;
    struct timeval tval;
    struct timeval *tptr;

    if (monitor_conn == NULL) {
        LOGD("Connection closed\n");
        strncpy(buf, WPA_EVENT_TERMINATING " - connection closed", buflen-1);
        buf[buflen-1] = '\0';
        return strlen(buf);
    }

    result = wpa_ctrl_recv(monitor_conn, buf, &nread);
    if (result < 0) {
        LOGD("wpa_ctrl_recv failed: %s\n", strerror(errno));
        strncpy(buf, WPA_EVENT_TERMINATING " - recv error", buflen-1);
        buf[buflen-1] = '\0';
        return strlen(buf);
    }
    buf[nread] = '\0';
    LOGD("wifi_wait_for_event(): result=%d nread=%d string=\"%s\"\n", result, nread, buf); 
    /* Check for EOF on the socket */
    if (result == 0 && nread == 0) {
        /* Fabricate an event to pass up */
        LOGE("Received EOF on supplicant socket\n");
        strncpy(buf, WPA_EVENT_TERMINATING " - signal 0 received", buflen-1);
        buf[buflen-1] = '\0';
        return strlen(buf);
    }
    /*
     * Events strings are in the format
     *
     *     <N>CTRL-EVENT-XXX
     *
     * where N is the message level in numerical form (0=VERBOSE, 1=DEBUG,
     * etc.) and XXX is the event name. The level information is not useful
     * to us, so strip it off.
     */
    if (buf[0] == '<') {
        char *match = strchr(buf, '>');
        if (match != NULL) {
            nread -= (match+1-buf);
            memmove(buf, match+1, nread+1);
        }
    }
    return nread;
}

void wifi_close_supplicant(void)
{
    if (ctrl_conn != NULL) {
        wpa_ctrl_close(ctrl_conn);
        ctrl_conn = NULL;
    }
    if (monitor_conn != NULL) {
        wpa_ctrl_close(monitor_conn);
        monitor_conn = NULL;
    }
}

static int common_close(struct hw_device_t* device) {
    wifi_stop_supplicant();
    wifi_close_supplicant();
    return 0;
}

static int open_wifi(const struct hw_module_t* module,
             const char*          name,
             struct hw_device_t** device)
{
    int  status = -EINVAL;

    LOGD("%s: name=%s", __FUNCTION__, name);

    struct wifi_device_t *dev = malloc(sizeof(*dev));

    memset(dev, 0, sizeof(*dev));

    dev->common.tag       = HARDWARE_DEVICE_TAG;
    dev->common.version   = 0;
    dev->common.module    = (struct hw_module_t*) module;
    dev->common.close     = common_close;

    dev->do_dhcp_request = do_dhcp_request;
    dev->get_dhcp_error_string = get_dhcp_error_string;
    dev->wifi_enable = wifi_load_driver;
    dev->wifi_disable = wifi_unload_driver;
    dev->wifi_start_supplicant = wifi_start_supplicant;
    dev->wifi_stop_supplicant = wifi_stop_supplicant;
    dev->wifi_open_supplicant = wifi_open_supplicant;
    dev->wifi_close_supplicant = wifi_close_supplicant;
    dev->wifi_command = wifi_send_command;
    dev->wifi_wait_for_event = wifi_wait_for_event;

    *device = &dev->common;
    status  = 0;

    return status;
}

static struct hw_module_methods_t wifi_module_methods = {
    .open = open_wifi
};

const struct wifi_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .version_major = 1,
        .version_minor = 0,
        .id = WIFI_HARDWARE_MODULE_ID, .name = "NLCP Wi-Fi Module",
        .author = "The Android Open Source Project",
        .methods = &wifi_module_methods,
    },
};

