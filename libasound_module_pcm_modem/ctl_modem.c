#define LOG_TAG "ALSAModule"
#include <utils/Log.h>
#include <sys/poll.h>
#define _POSIX_C_SOURCE

#include <alsa/asoundlib.h>
#include <alsa/control_external.h>

#include <amc.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

typedef struct snd_ctl_modem {
    snd_ctl_ext_t ext;

    char *source;
    char *sink;

    int sink_muted;
    int source_muted;

    int subscribed;
    int updated;
} snd_ctl_modem_t;


#define VOICE_EARPIECE "Voice Earpiece"
#define VOICE_SPEAKER "Voice Speaker"
#define AMC_VOICE "AMC Voice"
#define AMC_VOICE_BT "AMC voice BT"
#define VOICE_HEADSET "Voice Headset"
#define VOICE_BT "Voice BT"
#define AMC_ADJUST_VOLUME "AMC Adjust Volume"

#define UPDATE_AMC_VOICE     0x01
#define UPDATE_AMC_VOICE_BT    0x02
#define UPDATE_EARPIECE   0x04
#define UPDATE_SPEAKER  0x08
#define UPDATE_HEADSET  0x10
#define UPDATE_BT  0x20
#define UPDATE_AMC_ADJUST_VOLUME 0x40

#define MEDFIELDAUDIO "medfieldaudio"

static int modem_update_volume(snd_ctl_modem_t * ctl)
{
    int err;
    assert(ctl);

    return 0;
}

static int modem_elem_count(snd_ctl_ext_t * ext)
{
    snd_ctl_modem_t *ctl = ext->private_data;
    int count = 0, err;

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
    if (numid > 0 && numid <= 7)
        return numid - 1;

    name = snd_ctl_elem_id_get_name(id);
    LOGD("%s : name = %s\n", __func__, name);

    if (strcmp(name, VOICE_EARPIECE) == 0)
        return 0;
    if (strcmp(name, VOICE_SPEAKER) == 0)
        return 1;
    if (strcmp(name, AMC_VOICE) == 0)
        return 2;
    if (strcmp(name, AMC_VOICE_BT) == 0)
        return 3;
    if (strcmp(name, VOICE_HEADSET) == 0)
        return 4;
    if (strcmp(name, VOICE_BT) == 0)
        return 5;
    if (strcmp(name, AMC_ADJUST_VOLUME) == 0)
        return 6;

    return SND_CTL_EXT_KEY_NOT_FOUND;
}

static int modem_get_attribute(snd_ctl_ext_t * ext, snd_ctl_ext_key_t key,
                               int *type, unsigned int *acc,
                               unsigned int *count)
{
    snd_ctl_modem_t *ctl = ext->private_data;
    int err = 0;

    if (key > 8)
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
    case 0:
        break;
    case 1:
        *value = !ctl->source_muted;
        break;
    case 2:
        break;
    case 3:
        *value = !ctl->sink_muted;
        break;
    case 4:
        break;
    case 5:
        break;
    case 6:
        break;
    default:
        err = -EINVAL;
        goto finish;
    }

finish:

    return err;
}


static bool modem_set_param(snd_ctl_t *handle, const char *name,
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
        SNDERR("Control '%s' cannot get element info: %d", name, err);
        goto err;
    }

    int count = snd_ctl_elem_info_get_count(info);
    if (index >= count) {
        SNDERR("Control '%s' index is out of range (%d >= %d)", name, index, count);
        goto err;
    }

    if (index == -1)
        index = 0;
    else
        count = index + 1;

    snd_ctl_elem_type_t type = snd_ctl_elem_info_get_type(info);

    snd_ctl_elem_value_t *control;
    snd_ctl_elem_value_alloca(&control);

    snd_ctl_elem_info_get_id(info, id);
    snd_ctl_elem_value_set_id(control, id);

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
        SNDERR("Control '%s' write error", name);
        goto err;
    }
    return true;

err:
    return false;
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
        SNDERR("Open error:%s\n", snd_strerror(err));
        goto finish;
    }
    int index = 0;
    int volume = 200;

    switch (key) {
    case 0://earpiece
        SNDERR("voice route to earpiece \n");
        modem_set_param(handle, "Playback Switch", 0, index);
        modem_set_param(handle, "Headset Playback Route", 0, index);
        modem_set_param(handle, "Mode Playback Route", 1, index);
        modem_set_param(handle, "Speaker Mux Playback Route", 0, -1);
        modem_set_param(handle, "Mic1Mode Capture Route", 0, index);
        modem_set_param(handle, "Txpath1 Capture Route", 0, index);
        modem_set_param(handle, "Mic1 Capture Volume", 3, index);
        break;
    case 1: //speaker
        SNDERR("voice route to Speaker \n");
        modem_set_param(handle, "Speaker Mux Playback Route", 1, index);
        modem_set_param(handle, "Mode Playback Route", 1, index);
        modem_set_param(handle, "Headset Playback Route", 1, index);
        if (!!ctl->source_muted == !*value)
            goto finish;
        ctl->source_muted = !*value;
        break;
    case 2: //modem setup
        SNDERR("modem voice route to MSIC \n");
        amc_voice();
        break;
    case 3://modem setup for bt call
        SNDERR("modem voice route to BT \n");
        amc_bt();
        break;
    case 4: // headset
        SNDERR("voice route to headset \n");
        modem_set_param(handle, "Playback Switch", 1, index);
        modem_set_param(handle, "Headset Playback Route", 0, index);
        modem_set_param(handle, "Mode Playback Route", 1, index);
        modem_set_param(handle, "Speaker Mux Playback Route", 0, -1);
        modem_set_param(handle, "Mic1Mode Capture Route", 0, index);
        modem_set_param(handle, "Mic_InputL Capture Route", 0, index);
        modem_set_param(handle, "Mic_InputR Capture Route", 0, index);
        modem_set_param(handle, "Txpath1 Capture Route", 6, index);
        modem_set_param(handle, "Mic1 Capture Volume", 3, index);
        break;
    case 5: // bt
        SNDERR("voice route to BT \n");
        break;
    case 6: //volume control
        volume = * value;
        amc_adjust_volume(volume);
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
    } else if (ctl->updated & UPDATE_AMC_VOICE) {
        ctl->updated &= ~UPDATE_AMC_VOICE;
    } else if (ctl->updated & UPDATE_AMC_VOICE_BT) {
        ctl->updated &= ~UPDATE_AMC_VOICE_BT;
    } else if (ctl->updated & UPDATE_HEADSET) {
        ctl->updated &= ~UPDATE_HEADSET;
    } else if (ctl->updated & UPDATE_BT) {
        ctl->updated &= ~UPDATE_BT;
    } else if (ctl->updated & UPDATE_AMC_ADJUST_VOLUME) {
        ctl->updated &= ~UPDATE_AMC_ADJUST_VOLUME;
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
                SNDERR("Invalid type for %s", id);
                return -EINVAL;
            }
            continue;
        }
        if (strcmp(id, "device") == 0) {
            if (snd_config_get_string(n, &device) < 0) {
                SNDERR("Invalid type for %s", id);
                return -EINVAL;
            }
            continue;
        }
        if (strcmp(id, "source") == 0) {
            if (snd_config_get_string(n, &source) < 0) {
                SNDERR("Invalid type for %s", id);
                return -EINVAL;
            }
            continue;
        }
        if (strcmp(id, "sink") == 0) {
            if (snd_config_get_string(n, &sink) < 0) {
                SNDERR("Invalid type for %s", id);
                return -EINVAL;
            }
            continue;
        }
        SNDERR("Unknown field %s", id);
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
