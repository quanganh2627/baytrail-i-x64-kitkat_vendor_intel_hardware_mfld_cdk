#include <stdio.h>
#include <sys/poll.h>
#define _POSIX_C_SOURCE

#include <alsa/asoundlib.h>
#include <alsa/pcm_external.h>
#include <amc.h>
#include <alsa/asoundlib.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define MEDFIELDAUDIO "medfieldaudio"

typedef struct snd_pcm_modem {
    snd_pcm_ioplug_t io;
    char *device;

    /* Since ALSA expects a ring buffer we must do some voodoo. */
    size_t last_size;
    size_t ptr;
    int underrun;

    size_t offset;

    size_t frame_size;
    snd_pcm_t *phandle;
    snd_pcm_t *chandle;
} snd_pcm_modem_t;

static int check_stream(snd_pcm_modem_t *pcm)
{
    int err;
    assert(pcm);

    err = 0;

    return err;
}

static int update_ptr(snd_pcm_modem_t *pcm)
{
    size_t size;

    if (pcm->io.stream == SND_PCM_STREAM_PLAYBACK)
//		size = pa_stream_writable_size(pcm->stream);
        ;
    else
//		size = pa_stream_readable_size(pcm->stream);
        ;

    if (size == (size_t) -1)
        return -EIO;

    if (pcm->io.stream == SND_PCM_STREAM_CAPTURE)
        size -= pcm->offset;

    if (size > pcm->last_size) {
        pcm->ptr += size - pcm->last_size;
    }

    pcm->last_size = size;
    return 0;
}

static int check_active(snd_pcm_modem_t *pcm) {
    assert(pcm);

    /*
     * ALSA thinks in periods, not bytes, samples or frames.
     */

    if (pcm->io.stream == SND_PCM_STREAM_PLAYBACK) {
        size_t wsize;

        return wsize;
    } else {
        size_t rsize;

        return rsize;
    }
}

static int update_active(snd_pcm_modem_t *pcm) {
    int ret;

    assert(pcm);

    ret = check_stream(pcm);

    ret = check_active(pcm);

    return ret;
}

static int wait_stream_state(snd_pcm_modem_t *pcm, int target)
{
    assert(pcm);

    return 0;
}

static void stream_success_cb(void * p, int success, void *userdata)
{
    snd_pcm_modem_t *pcm = userdata;

    assert(pcm);
}

static int modem_start(snd_pcm_ioplug_t * io)
{
    snd_pcm_modem_t *pcm = io->private_data;
    int err = 0, err_o = 0, err_u = 0;

    assert(pcm);

    pcm->ptr = 0;
    return err;
}

static int modem_stop(snd_pcm_ioplug_t * io)
{
    snd_pcm_modem_t *pcm = io->private_data;
    int err = 0, err_o = 0, err_u = 0;

    return err;
}

static int modem_drain(snd_pcm_ioplug_t * io)
{
    snd_pcm_modem_t *pcm = io->private_data;
    int err = 0;
    return err;
}

static snd_pcm_sframes_t modem_pointer(snd_pcm_ioplug_t * io)
{
    snd_pcm_modem_t *pcm = io->private_data;
    snd_pcm_sframes_t ret = 0;

    assert(pcm);
    ret = pcm->ptr;

    return ret;
}

static int modem_delay(snd_pcm_ioplug_t * io, snd_pcm_sframes_t * delayp)
{
    snd_pcm_modem_t *pcm = io->private_data;
    int err = 0;

    return err;
}

static snd_pcm_sframes_t modem_write(snd_pcm_ioplug_t * io,
                                     const snd_pcm_channel_area_t * areas,
                                     snd_pcm_uframes_t offset,
                                     snd_pcm_uframes_t size)
{
    snd_pcm_modem_t *pcm = io->private_data;
    const char *buf;
    snd_pcm_sframes_t ret = size;
    int bytes = snd_pcm_format_physical_width(io->format);
    int consume = (size * 1000 * 1000 * 1000) / (bytes * io->rate * io->channels);
    usleep(consume);
    pcm->ptr = (pcm->ptr + size) % io->buffer_size;
    return ret;
}

static snd_pcm_sframes_t modem_read(snd_pcm_ioplug_t * io,
                                    const snd_pcm_channel_area_t * areas,
                                    snd_pcm_uframes_t offset,
                                    snd_pcm_uframes_t size)
{
    snd_pcm_modem_t *pcm = io->private_data;
    void *dst_buf;
    size_t remain_size, frag_length;
    snd_pcm_sframes_t ret = 0;
    usleep(20000);
    assert(pcm);
    return size;
}

static int modem_pcm_poll_revents(snd_pcm_ioplug_t * io,
                                  struct pollfd *pfd, unsigned int nfds,
                                  unsigned short *revents)
{
    int err = 0;
    snd_pcm_modem_t *pcm = io->private_data;

    assert(pcm);

    err = check_stream(pcm);
    if (err < 0)
        goto finish;

    err = check_active(pcm);
    if (err < 0)
        goto finish;

    if (err > 0)
        *revents = io->stream == SND_PCM_STREAM_PLAYBACK ? POLLOUT : POLLIN;
    else
        *revents = 0;

finish:
    return err;
}

static int modem_prepare(snd_pcm_ioplug_t * io)
{
    snd_pcm_modem_t *pcm = io->private_data;
    int err = 0, r;
    unsigned c, d;

    assert(pcm);

    pcm->offset = 0;
    pcm->underrun = 0;

finish:
    return err;
}

static int modem_hw_params(snd_pcm_ioplug_t * io,
                           snd_pcm_hw_params_t * params)
{
    snd_pcm_modem_t *pcm = io->private_data;
    int err = 0;

    assert(pcm);

    pcm->frame_size =
        (snd_pcm_format_physical_width(io->format) * io->channels) / 8;

    switch (io->format) {
    case SND_PCM_FORMAT_U8:
        break;
    case SND_PCM_FORMAT_A_LAW:
        break;
    case SND_PCM_FORMAT_MU_LAW:
        break;
    case SND_PCM_FORMAT_S16_LE:
        break;
    case SND_PCM_FORMAT_S16_BE:
        break;
#ifdef PA_SAMPLE_FLOAT32LE
    case SND_PCM_FORMAT_FLOAT_LE:
        break;
#endif
#ifdef PA_SAMPLE_FLOAT32BE
    case SND_PCM_FORMAT_FLOAT_BE:
        break;
#endif
#ifdef PA_SAMPLE_S32LE
    case SND_PCM_FORMAT_S32_LE:
        break;
#endif
#ifdef PA_SAMPLE_S32BE
    case SND_PCM_FORMAT_S32_BE:
        pcm->ss.format = PA_SAMPLE_S32BE;
        break;
#endif
    default:
        SNDERR("PulseAudio: Unsupported format %s\n",
               snd_pcm_format_name(io->format));
        err = -EINVAL;
        goto finish;
    }

finish:
    return err;
}

static int modem_close(snd_pcm_ioplug_t * io)
{
    snd_pcm_modem_t *pcm = io->private_data;

    assert(pcm);
    if(pcm->phandle)
        snd_pcm_close(pcm->phandle);

    if(pcm->chandle)
        snd_pcm_close(pcm->chandle);

    pcm->phandle= NULL;
    pcm->chandle= NULL;
    free(pcm->device);
    free(pcm);

    return 0;
}

static int modem_pause(snd_pcm_ioplug_t * io, int enable)
{
    snd_pcm_modem_t *pcm = io->private_data;
    int err = 0;

    assert (pcm);

    err = check_stream(pcm);
    if (err < 0)
        goto finish;

finish:
    return err;
}

static const snd_pcm_ioplug_callback_t modem_playback_callback = {
    .start = modem_start,
    .stop = modem_stop,
    .drain = modem_drain,
    .pointer = modem_pointer,
    .transfer = modem_write,
    .delay = modem_delay,
    .poll_revents = modem_pcm_poll_revents,
    .prepare = modem_prepare,
    .hw_params = modem_hw_params,
    .close = modem_close,
    .pause = modem_pause
};


static const snd_pcm_ioplug_callback_t modem_capture_callback = {
    .start = modem_start,
    .stop = modem_stop,
    .pointer = modem_pointer,
    .transfer = modem_read,
    .delay = modem_delay,
    .poll_revents = modem_pcm_poll_revents,
    .prepare = modem_prepare,
    .hw_params = modem_hw_params,
    .close = modem_close,
};


static int modem_hw_constraint(snd_pcm_modem_t * pcm)
{
    snd_pcm_ioplug_t *io = &pcm->io;

    static const snd_pcm_access_t access_list[] = {
        SND_PCM_ACCESS_RW_INTERLEAVED
    };
    static const unsigned int formats[] = {
        SND_PCM_FORMAT_U8,
        SND_PCM_FORMAT_A_LAW,
        SND_PCM_FORMAT_MU_LAW,
        SND_PCM_FORMAT_S16_LE,
        SND_PCM_FORMAT_S16_BE,
        SND_PCM_FORMAT_FLOAT_LE,
        SND_PCM_FORMAT_FLOAT_BE,
        SND_PCM_FORMAT_S32_LE,
        SND_PCM_FORMAT_S32_BE
    };

    int err;

    err = snd_pcm_ioplug_set_param_list(io, SND_PCM_IOPLUG_HW_ACCESS,
                                        ARRAY_SIZE(access_list),
                                        access_list);
    if (err < 0)
        return err;

    err = snd_pcm_ioplug_set_param_list(io, SND_PCM_IOPLUG_HW_FORMAT,
                                        ARRAY_SIZE(formats), formats);
    if (err < 0)
        return err;

    err =
        snd_pcm_ioplug_set_param_minmax(io,
                                        SND_PCM_IOPLUG_HW_BUFFER_BYTES,
                                        1, 4 * 1024 * 1024);
    if (err < 0)
        return err;

    err =
        snd_pcm_ioplug_set_param_minmax(io,
                                        SND_PCM_IOPLUG_HW_PERIOD_BYTES,
                                        128, 2 * 1024 * 1024);
    if (err < 0)
        return err;

    err =
        snd_pcm_ioplug_set_param_minmax(io, SND_PCM_IOPLUG_HW_PERIODS,
                                        3, 1024);
    if (err < 0)
        return err;

    return 0;
}

SND_PCM_PLUGIN_DEFINE_FUNC(modem)
{
    snd_config_iterator_t i, next;
    const char *server = NULL;
    const char *device = NULL;
    int err;
    snd_pcm_modem_t *pcm;

    SNDERR("modem alsa plugin open \n");

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
        SNDERR("Unknown field %s", id);
        return -EINVAL;
    }

    pcm = calloc(1, sizeof(*pcm));
    if (!pcm)
        return -ENOMEM;

    if (device) {
        pcm->device = strdup(device);

        if (!pcm->device) {
            err = -ENOMEM;
            goto error;
        }
    }

    pcm->io.version = SND_PCM_IOPLUG_VERSION;
    pcm->io.name = "ALSA <-> PulseAudio PCM I/O Plugin";
    pcm->io.poll_fd = NULL;
    pcm->io.poll_events = POLLIN;
    pcm->io.mmap_rw = 0;
    pcm->io.callback = stream == SND_PCM_STREAM_PLAYBACK ?
                       &modem_playback_callback : &modem_capture_callback;
    pcm->io.private_data = pcm;

    err = snd_pcm_ioplug_create(&pcm->io, name, stream, mode);
    if (err < 0)
        goto error;

    err = modem_hw_constraint(pcm);
    if (err < 0) {
        snd_pcm_ioplug_delete(&pcm->io);
        goto error;
    }

    *pcmp = pcm->io.pcm;

    int card = snd_card_get_index(MEDFIELDAUDIO);
    char device_v[128];
    sprintf(device_v, "hw:%d,2", card);
    SNDERR("%s \n",device_v);

    if ((err = snd_pcm_open(&pcm->phandle, device_v, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        SNDERR("Playback open error: %s\n", snd_strerror(err));
        return 0;
    }

    if ((err = snd_pcm_open(&pcm->chandle, device_v, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        SNDERR("Capture open error: %s\n", snd_strerror(err));
        return 0;
    }

    if ((err = snd_pcm_set_params(pcm->phandle,
                                  SND_PCM_FORMAT_S16_LE,
                                  SND_PCM_ACCESS_RW_INTERLEAVED,
                                  2,
                                  48000,
                                  0,
                                  500000)) < 0) {	/* 0.5sec */
        return 0;
    }

    if ((err = snd_pcm_set_params(pcm->chandle,
                                  SND_PCM_FORMAT_S16_LE,
                                  SND_PCM_ACCESS_RW_INTERLEAVED,
                                  1,
                                  48000,
                                  1,
                                  500000)) < 0) { /* 0.5sec */
        SNDERR("Capture open error: %s\n", snd_strerror(err));
        return 0;
    }


    SNDERR("pcm_modem init ok\n");
    return 0;

error:
    free(pcm->device);
    free(pcm);

    return err;
}

SND_PCM_PLUGIN_SYMBOL(modem);
