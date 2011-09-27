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
 */

#define LOG_TAG "ALSAPlugInModemCTL"
#include <utils/Log.h>
#include <sys/poll.h>
#define _POSIX_C_SOURCE

#include <alsa/asoundlib.h>
#include <alsa/control_external.h>

#include "vpc_hardware.h"

typedef struct snd_ctl_modem {
    snd_ctl_ext_t ext;

    vpc_device_t *vpc;

    char *source;
    char *sink;

    int sink_muted;
    int source_muted;

    int subscribed;
    int updated;
} snd_ctl_modem_t;

static snd_pcm_t *phandle;
static snd_pcm_t *chandle;

#define VOICE_EARPIECE "Voice Earpiece"
#define VOICE_SPEAKER "Voice Speaker"
#define VOICE_HEADSET "Voice Headset"
#define VOICE_BT "Voice BT"
#define VOICE_HEADPHONE "Voice Headphone"

#define UPDATE_EARPIECE   0x01
#define UPDATE_SPEAKER  0x02
#define UPDATE_HEADSET  0x04
#define UPDATE_BT  0x08
#define UPDATE_HEADPHONE  0x10

typedef enum snd_ctl_ext_incall_key {
    VOICE_EARPIECE_INCALL = 0x0,
    VOICE_SPEAKER_INCALL,
    VOICE_HEADSET_INCALL,
    VOICE_BT_INCALL,
    VOICE_HEADPHONE_INCALL,
    EXT_INCALL
} snd_ctl_ext_incall_key_t;


#define MEDFIELDAUDIO "medfieldaudio"


static int modem_elem_count(snd_ctl_ext_t * ext)
{
    snd_ctl_modem_t *ctl = ext->private_data;
    int count = 0;

    assert(ctl);

    if (ctl->source)
        count += 2;
    if (ctl->sink)
        count += 2;

finish:
    return count;
}

static int modem_elem_list(snd_ctl_ext_t * ext, unsigned int offset,
                           snd_ctl_elem_id_t * id)
{
    snd_ctl_modem_t *ctl = ext->private_data;
    int err;

    assert(ctl);

    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);

    err = 0;

finish:
    return err;
}

static snd_ctl_ext_key_t modem_find_elem(snd_ctl_ext_t * ext,
        const snd_ctl_elem_id_t * id)
{
    const char *name;
    unsigned int numid;

    numid = snd_ctl_elem_id_get_numid(id);
    if (numid > 0 && numid <= EXT_INCALL)
        return numid - 1;

    name = snd_ctl_elem_id_get_name(id);
    LOGD("%s : name = %s\n", __func__, name);

    if (strcmp(name, VOICE_EARPIECE) == 0)
        return VOICE_EARPIECE_INCALL;
    if (strcmp(name, VOICE_SPEAKER) == 0)
        return VOICE_SPEAKER_INCALL;
    if (strcmp(name, VOICE_HEADSET) == 0)
        return VOICE_HEADSET_INCALL;
    if (strcmp(name, VOICE_BT) == 0)
        return VOICE_BT_INCALL;
    if (strcmp(name, VOICE_HEADPHONE) == 0)
        return VOICE_HEADPHONE_INCALL;

    return SND_CTL_EXT_KEY_NOT_FOUND;
}

static int modem_get_attribute(snd_ctl_ext_t * ext, snd_ctl_ext_key_t key,
                               int *type, unsigned int *acc,
                               unsigned int *count)
{
    snd_ctl_modem_t *ctl = ext->private_data;
    int err = 0;

    if (key > EXT_INCALL + 1)
        return -EINVAL;

    assert(ctl);

    if (key & 1)
        *type = SND_CTL_ELEM_TYPE_BOOLEAN;
    else
        *type = SND_CTL_ELEM_TYPE_INTEGER;

    *acc = SND_CTL_EXT_ACCESS_READWRITE;

    *count = 1;
finish:

    return err;
}

static int modem_get_integer_info(snd_ctl_ext_t * ext,
                                  snd_ctl_ext_key_t key, long *imin,
                                  long *imax, long *istep)
{
    *istep = 1;
    *imin = 0;
    *imax = 100;

    return 0;
}

static int modem_read_integer(snd_ctl_ext_t * ext, snd_ctl_ext_key_t key,
                              long *value)
{
    snd_ctl_modem_t *ctl = ext->private_data;
    int err = 0, i;
    assert(ctl);

    switch (key) {
    case VOICE_EARPIECE_INCALL:
        break;
    case VOICE_SPEAKER_INCALL:
        *value = !ctl->source_muted;
        break;
    case VOICE_HEADSET_INCALL:
        break;
    case VOICE_BT_INCALL:
        *value = !ctl->sink_muted;
        break;
    case VOICE_HEADPHONE_INCALL:
        break;
    default:
        err = -EINVAL;
        goto finish;
    }

finish:

    return err;
}


static int modem_set_param(snd_ctl_t *handle, const char *name,
                           unsigned int value, int index)
{
    int err, i;
    snd_ctl_elem_id_t *id;
    snd_ctl_elem_info_t *info;

    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_info_alloca(&info);

    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
    snd_ctl_elem_id_set_name(id, name);
    snd_ctl_elem_info_set_id(info, id);

    err = snd_ctl_elem_info(handle, info);
    if (err < 0) {
        LOGE("Control '%s' cannot get element info: %d", name, err);
        goto err;
    }

    int count = snd_ctl_elem_info_get_count(info);
    if (index >= count) {
        LOGE("Control '%s' index is out of range (%d >= %d)", name, index, count);
        goto err;
    }

    snd_ctl_elem_type_t type = snd_ctl_elem_info_get_type(info);

    snd_ctl_elem_value_t *control;
    snd_ctl_elem_value_alloca(&control);

    snd_ctl_elem_info_get_id(info, id);
    snd_ctl_elem_value_set_id(control, id);

    if (count > 1)
        snd_ctl_elem_read(handle, control);

    if (index == -1)
        index = 0;
    else
        count = index + 1;

    for (i = index; i < count; i++)
        switch (type) {
        case SND_CTL_ELEM_TYPE_BOOLEAN:
            snd_ctl_elem_value_set_boolean(control, i, value);
            break;
        case SND_CTL_ELEM_TYPE_INTEGER:
            snd_ctl_elem_value_set_integer(control, i, value);
            break;
        case SND_CTL_ELEM_TYPE_INTEGER64:
            snd_ctl_elem_value_set_integer64(control, i, value);
            break;
        case SND_CTL_ELEM_TYPE_ENUMERATED:
            snd_ctl_elem_value_set_enumerated(control, i, value);
            break;
        case SND_CTL_ELEM_TYPE_BYTES:
            snd_ctl_elem_value_set_byte(control, i, value);
            break;
        default:
            break;
        }

    err = snd_ctl_elem_write(handle, control);
    if (err < 0) {
        LOGE("Control '%s' write error", name);
        goto err;
    }
    return 1;

err:
    return 0;
}

static int modem_write_integer(snd_ctl_ext_t * ext, snd_ctl_ext_key_t key,
                               long *value)
{
    snd_ctl_modem_t *ctl = ext->private_data;
    int err = 0, i;

    assert(ctl);

    int card = snd_card_get_index(MEDFIELDAUDIO);
    snd_ctl_t *handle;
    char str[64];
    sprintf(str, "hw:CARD=%d", card);

    if ((err = snd_ctl_open(&handle, str, 0)) < 0) {
        LOGE("Open error:%s\n", snd_strerror(err));
        goto finish;
    }
    int index = 0;

    switch (key) {
    case VOICE_EARPIECE_INCALL://earpiece
        LOGD("voice route to earpiece \n");
        modem_set_param(handle, "Playback Switch", 0, index);
        modem_set_param(handle, "Headset Playback Route", 0, index);
        modem_set_param(handle, "Mode Playback Route", 1, index);
        modem_set_param(handle, "Speaker Mux Playback Route", 0, -1);
        modem_set_param(handle, "DMIC12 Capture Route", 1, index);
        modem_set_param(handle, "DMIC56 Capture Route", 1, index);
        modem_set_param(handle, "Txpath1 Capture Route", 0, index);
        modem_set_param(handle, "Txpath2 Capture Route", 4, index);
        ctl->vpc->route();
        break;
    case VOICE_SPEAKER_INCALL: //speaker
        LOGD("voice route to Speaker \n");
        modem_set_param(handle, "Playback Switch", 1, index);
        modem_set_param(handle, "Headset Playback Route", 1, index);
        modem_set_param(handle, "Mode Playback Route", 1, index);
        modem_set_param(handle, "Speaker Mux Playback Route", 1, -1);
        modem_set_param(handle, "DMIC12 Capture Route", 0, index);
        modem_set_param(handle, "DMIC56 Capture Route", 1, index);
        modem_set_param(handle, "Txpath1 Capture Route", 4, index);
        modem_set_param(handle, "Txpath2 Capture Route", 0, index);
        ctl->vpc->route();

        if (!!ctl->source_muted == !*value)
            goto finish;
        ctl->source_muted = !*value;
        break;
    case VOICE_HEADSET_INCALL: // headset
        LOGD("voice route to headset \n");
        modem_set_param(handle, "Playback Switch", 1, index);
        modem_set_param(handle, "Headset Playback Route", 0, index);
        modem_set_param(handle, "Mode Playback Route", 1, index);
        modem_set_param(handle, "Speaker Mux Playback Route", 0, -1);
        modem_set_param(handle, "Mic1Mode Capture Route", 0, index);
        modem_set_param(handle, "Mic_InputL Capture Route", 0, index);
        modem_set_param(handle, "DMIC56 Capture Route", 1, index);
        modem_set_param(handle, "Txpath1 Capture Route", 6, index);
        modem_set_param(handle, "Txpath2 Capture Route", 4, index);
        modem_set_param(handle, "Mic1 Capture Volume", 1, index);
        ctl->vpc->route();
        break;
    case VOICE_BT_INCALL: // bt
        LOGD("voice route to BT \n");
        ctl->vpc->route();
        break;
    case VOICE_HEADPHONE_INCALL:// headphone
        LOGD("voice route to headphone \n");
        modem_set_param(handle, "Playback Switch", 1, index);
        modem_set_param(handle, "Headset Playback Route", 0, index);
        modem_set_param(handle, "Mode Playback Route", 1, index);
        modem_set_param(handle, "Speaker Mux Playback Route", 0, -1);
        modem_set_param(handle, "DMIC12 Capture Route", 1, index);
        modem_set_param(handle, "DMIC56 Capture Route", 1, index);
        modem_set_param(handle, "Txpath1 Capture Route", 0, index);
        modem_set_param(handle, "Txpath2 Capture Route", 4, index);
        ctl->vpc->route();
        break;

    default:
        err = -EINVAL;
        goto finish;
    }

    snd_ctl_close(handle);

    if (err < 0)
        goto finish;

    err = 1;

finish:
    return err;
}

static void modem_subscribe_events(snd_ctl_ext_t * ext, int subscribe)
{
    snd_ctl_modem_t *ctl = ext->private_data;

    assert(ctl);

    ctl->subscribed = !!(subscribe & SND_CTL_EVENT_MASK_VALUE);
}

static int modem_read_event(snd_ctl_ext_t * ext, snd_ctl_elem_id_t * id,
                            unsigned int *event_mask)
{
    snd_ctl_modem_t *ctl = ext->private_data;
    int offset;
    int err;

    assert(ctl);

    if (ctl->source)
        offset = 2;
    else
        offset = 0;

    if (ctl->updated & UPDATE_EARPIECE) {
        ctl->updated &= ~UPDATE_EARPIECE;
    } else if (ctl->updated & UPDATE_SPEAKER) {
        ctl->updated &= ~UPDATE_SPEAKER;
    } else if (ctl->updated & UPDATE_HEADSET) {
        ctl->updated &= ~UPDATE_HEADSET;
    } else if (ctl->updated & UPDATE_BT) {
        ctl->updated &= ~UPDATE_BT;
    } else if (ctl->updated & UPDATE_HEADPHONE) {
        ctl->updated &= ~UPDATE_HEADPHONE;
    }

    *event_mask = SND_CTL_EVENT_MASK_VALUE;

    err = 1;

finish:

    return err;
}

static int modem_ctl_poll_revents(snd_ctl_ext_t * ext, struct pollfd *pfd,
                                  unsigned int nfds,
                                  unsigned short *revents)
{
    snd_ctl_modem_t *ctl = ext->private_data;
    int err;

    assert(ctl);

    if (ctl->updated)
        *revents = POLLIN;
    else
        *revents = 0;

    err = 0;

finish:

    return err;
}

static void modem_close(snd_ctl_ext_t * ext)
{
    snd_ctl_modem_t *ctl = ext->private_data;

    assert(ctl);

    ctl->vpc->route();

    free(ctl->source);
    free(ctl->sink);
    free(ctl);
}

static const snd_ctl_ext_callback_t modem_ext_callback = {
    .elem_count = modem_elem_count,
    .elem_list = modem_elem_list,
    .find_elem = modem_find_elem,
    .get_attribute = modem_get_attribute,
    .get_integer_info = modem_get_integer_info,
    .read_integer = modem_read_integer,
    .write_integer = modem_write_integer,
    .subscribe_events = modem_subscribe_events,
    .read_event = modem_read_event,
    .poll_revents = modem_ctl_poll_revents,
    .close = modem_close,
};

SND_CTL_PLUGIN_DEFINE_FUNC(modem)
{
    snd_config_iterator_t i, next;
    const char *server = NULL;
    const char *device = NULL;
    const char *source = NULL;
    const char *sink = NULL;
    int err;
    hw_device_t* hw_device;
    snd_ctl_modem_t *ctl;

    snd_config_for_each(i, next, conf) {
        snd_config_t *n = snd_config_iterator_entry(i);
        const char *id;
        if (snd_config_get_id(n, &id) < 0)
            continue;
        if (strcmp(id, "comment") == 0 || strcmp(id, "type") == 0
                || strcmp(id, "hint") == 0)
            continue;
        if (strcmp(id, "server") == 0) {
            if (snd_config_get_string(n, &server) < 0) {
                LOGE("Invalid type for %s", id);
                return -EINVAL;
            }
            continue;
        }
        if (strcmp(id, "device") == 0) {
            if (snd_config_get_string(n, &device) < 0) {
                LOGE("Invalid type for %s", id);
                return -EINVAL;
            }
            continue;
        }
        if (strcmp(id, "source") == 0) {
            if (snd_config_get_string(n, &source) < 0) {
                LOGE("Invalid type for %s", id);
                return -EINVAL;
            }
            continue;
        }
        if (strcmp(id, "sink") == 0) {
            if (snd_config_get_string(n, &sink) < 0) {
                LOGE("Invalid type for %s", id);
                return -EINVAL;
            }
            continue;
        }
        LOGE("Unknown field %s", id);
        return -EINVAL;
    }

    ctl = calloc(1, sizeof(*ctl));
    if (!ctl)
        return -ENOMEM;

    if (source)
        ctl->source = strdup(source);
    else if (device)
        ctl->source = strdup(device);

    if ((source || device) && !ctl->source) {
        err = -ENOMEM;
        goto error;
    }

    if (sink)
        ctl->sink = strdup(sink);
    else if (device)
        ctl->sink = strdup(device);

    if ((sink || device) && !ctl->sink) {
        err = -ENOMEM;
        goto error;
    }

    hw_module_t *module;
    err = hw_get_module(VPC_HARDWARE_MODULE_ID, (hw_module_t const**)&module);

    if (err == 0) {
        err = module->methods->open(module, VPC_HARDWARE_NAME, &hw_device);
        if (err == 0) {
            LOGD("VPC MODULE OK.");
            ctl->vpc = (vpc_device_t *) hw_device;
        }
        else {
            LOGE("VPC Module not found");
            goto error;
        }
    }

    ctl->ext.version = SND_CTL_EXT_VERSION;
    ctl->ext.card_idx = 0;
    strncpy(ctl->ext.id, "modem", sizeof(ctl->ext.id) - 1);
    strncpy(ctl->ext.driver, "ModemAudio plugin",
            sizeof(ctl->ext.driver) - 1);
    strncpy(ctl->ext.name, "ModemAudio", sizeof(ctl->ext.name) - 1);
    strncpy(ctl->ext.longname, "ModemAudio",
            sizeof(ctl->ext.longname) - 1);
    strncpy(ctl->ext.mixername, "ModemAudio",
            sizeof(ctl->ext.mixername) - 1);

    ctl->ext.callback = &modem_ext_callback;
    ctl->ext.private_data = ctl;

    err = snd_ctl_ext_create(&ctl->ext, name, mode);
    if (err < 0)
        goto error;

    *handlep = ctl->ext.handle;
    return 0;

error:
    free(ctl->source);
    free(ctl->sink);
    free(ctl);

    return err;
}

SND_CTL_PLUGIN_SYMBOL(modem);
