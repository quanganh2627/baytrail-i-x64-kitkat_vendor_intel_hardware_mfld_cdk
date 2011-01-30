#include <sys/types.h>
#include <sys/stat.h>
//#include <sys/io.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include "src24bit_cmtspeech.h"
#include <semaphore.h>
#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include "cs-protocol.h"
//#include <cs-protocol.h>
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <utils/Log.h>
#define LOG_TAG "BridgeAppTEST"
#include "bridgeapp.h"

#undef LOGV
#define LOGV LOGD
// This is supposed to work for displaying verbose logs, but doesn't for some reason
// so we use the undef/define combination above for LOGV.
#define LOG_NDEBUG 1 



#include <sched.h>
#define FATAL do { LOGV(stderr, "Error at line %d, file %s (%d) [%s]\n", \
  __LINE__, __FILE__, errno, strerror(errno)); exit(1); } while(0)

#define BURSTEMODE_ENVVAR "BURST_MODE"
#define BURSTEMODE_96KHZ "96"
#define BURSTEMODE_48KHZ "48"
#define NB_FRAMES_PER_PLAY 2
#define NB_FRAMES_PER_CAPTURE 2
#define SLOT_SIZE 320

/* SRAM area limited to 4KB */

#define MAP_SIZE 4096UL
#define SPEECH_BUFFER_SIZE 4096UL

#define NB_RX_BUFS 4
#define NB_TX_BUFS 1

#define PCM_DUPLEX_BUF_SIZE 320

#define PCM_BM_BUF_SIZE 656
#define PCM_BM_CTRL_SIZE 8 /* 8 u16 for in band PCM CTRL channel */
#define PCM_BM_SIG_SIZE  5 /* Within theses 8 u16, the first 5 u16 are dedicated to SIGNATURE */

#define DL_CTRL_CODEC(p) ((p[2] & (1 << 6)) >> 6)

#define true 1 
#define false 0 
typedef int bool;
bool read_modem_start = false; 
static char *device = "hw";

struct sigaction act;
enum codec_type {
	CODEC_NB,
	CODEC_WB
};


struct cs_ssp_iface {
	void *mmap_base;
	unsigned long mmap_size;

	/* mmap states/info exposed by ssp speech driver to application */
	struct cs_mmap_config_block *mmap_cfg;

	unsigned int rx_slot; /* index of current rx_slot */ 
	unsigned int tx_slot; /* index of current tx_slot */ 

	int pcm_data_size; /* 320 for Narrow Band, 640 for Wide Band */
	char *env_burst_mode;
	int burst_mode;
	enum codec_type codec;
	int fd;
};
unsigned int alsa_errors;
static struct cs_ssp_iface ssp_iface;
int state_play;
int signal_end;
int signal_gotdata;
unsigned short buf[PCM_BM_BUF_SIZE];
/* Allow CtrlC to stop the apps */

void sighandler(int signo)
{	
	switch (signo) {
	case SIGIO:
		LOGV("gotdat %d\n", signal_gotdata);
		signal_gotdata++;
		break;
	case SIGINT:
		LOGV("SIGINT received\n");
		signal_end = 1;
		break;
	default:
		break;
	}
}

struct slots_buf_48 {
	unsigned int num_write;
	unsigned int num_read;
	TSRCHandle16in_24out* SRCHandle16in_24out;
	TSRCHandle24in_16out* SRCHandle24in_16out;
	sem_t sem_buf;
};


struct slots_buf_48 rx_voice;
struct slots_buf_48 tx_voice;

/*------------------*/
/* common functions */
/*------------------*/


void start_thread_with_priority_max (pthread_t *thread, void *start_routine,
				void *arg)
{
	pthread_attr_t attr;
	struct sched_param param;
	int status;

	status = pthread_attr_init (&attr);
	if (status != 0)
		perror ("pthread_attr_init failed ");

	status = pthread_attr_setschedpolicy (&attr, SCHED_RR);
	if (status != 0)
		perror ("pthread_set_schedpolicy failed ");

	param.sched_priority = sched_get_priority_max (SCHED_RR);
	status = pthread_attr_setschedparam (&attr, &param);
	if (status != 0)
		perror ("pthread_setschedparam failed ");

	status = pthread_create (thread, &attr, start_routine, arg);
	if (status != 0)
		perror ("pthread_create failed (YOU ARE PROBABLY NOT ROOT");

}

/* Set PCM parameters */

void set_pcm_params (snd_pcm_t *handle, int nb_frames)
{
	static snd_output_t *log;
	snd_pcm_sw_params_t *swparams;
	int err;

	/* latency 80 => ring buffer time 80 ms => period time 20 ms */
	/* well, I had to look at source code of snd_pcm_set_params to catch this */
	/* latency = buffer-time */
	/* period-time = buffer-time / 4 (it is harcoded) */
	if ((err = snd_pcm_set_params (handle,
				       SND_PCM_FORMAT_S24_LE,
				       SND_PCM_ACCESS_RW_INTERLEAVED,
				       2,
				      // 8000,
				       48000,
				       0, 80000)) < 0) {
		LOGV ("open error: %s\n", snd_strerror (err));
		exit (EXIT_FAILURE);
	}
	// enforce start_threshold to 1 ... otherwise capture does not work on CDK
	snd_pcm_sw_params_alloca (&swparams);
	snd_pcm_sw_params_current (handle, swparams);
	err = snd_pcm_sw_params_set_start_threshold (handle, swparams, 1);
	assert (err >= 0);
	if (snd_pcm_sw_params (handle, swparams) < 0) {
		LOGV ("unable to install sw params\n");
	}

	snd_output_stdio_attach (&log, stdout, 0);
	snd_pcm_dump (handle, log);
}

/*---------*/
/* UL path */
/*---------*/

void *pcm_tx (void *arg)
{
	unsigned int msg;
	short *address = NULL;

	
	while (signal_end == 0) {
		//LOGV("pcm_tx before sem_wait\n");
		sem_wait (&tx_voice.sem_buf);
		if (tx_voice.num_read < tx_voice.num_write) {
		address = (unsigned short *) (ssp_iface.mmap_base + ssp_iface.mmap_cfg->tx_offsets[ssp_iface.tx_slot]);
		SRCDownsamp48to8(tx_voice.SRCHandle24in_16out, address, 160);
		msg = CS_TX_DATA_READY;
		msg |= ssp_iface.tx_slot;
		write(ssp_iface.fd, &msg, sizeof(msg));
		//LOGV("WRITE %d\n", ssp_iface.tx_slot);
		ssp_iface.tx_slot++;
		ssp_iface.tx_slot %= NB_TX_BUFS;
		tx_voice.num_read++;
		}
	}
	return NULL;
}


void *pcm_capture (void *arg)
{
	static snd_pcm_t *handle;
	unsigned int *sptr;
	int err,i;
	unsigned int *samples;
  	static unsigned int buffer[1920];
	snd_pcm_sframes_t frames;
	snd_pcm_sframes_t frames_avail;
	snd_pcm_sframes_t frames_avail_update;
	snd_pcm_sframes_t frames_rewindable;

	if ((err = snd_pcm_open (&handle, device, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		LOGV ("Capture open error: %s\n", snd_strerror (err));
		exit (EXIT_FAILURE);
	}
	LOGV ("TX set_pcm_params for capture\n");
	set_pcm_params (handle, NB_FRAMES_PER_CAPTURE);
	
	while (signal_end == 0) {
		frames_avail = snd_pcm_avail(handle);
		snd_pcm_hwsync(handle);
		frames_avail_update = snd_pcm_avail_update(handle);
		frames_rewindable = snd_pcm_rewindable(handle);
		frames = snd_pcm_readi (handle, &buffer, 960);
		samples = (unsigned int*)&buffer;
		sptr = (unsigned int*)SRCGetInputWritePtrDownsamp48to8(tx_voice.SRCHandle24in_16out,960);
		for (i=0; i<960; i++) {
			*sptr++ = ((*samples++)<<8);
			samples++;
		}
		snd_pcm_hwsync(handle);
		frames_avail = snd_pcm_avail_update(handle);
		if (frames < 0) {
			frames = snd_pcm_recover (handle, frames, 0);
			//LOGV ("pcm_capture recover %d\n", tx_voice.num_write);
		} else {
			tx_voice.num_write++;
		}


		sem_post (&tx_voice.sem_buf);

	}
	snd_pcm_close (handle);
	return NULL;
}


/*---------*/
/* DL path */
/*---------*/

void *pcm_rx (void *arg)
{
    	struct pollfd pfd;
    	unsigned int rx_slot;
    	unsigned int msg;
    	int ret;
		int ret2;
		pfd.fd=ssp_iface.fd;
    	pfd.events = POLLIN;
    	pfd.revents = 0;
		short *address = NULL;

		while(signal_end == 0){
		ret = poll(&pfd, 1, -1 /* no timeout */);
		if(ret ){
			if (pfd.revents & POLLIN) {
				if (sizeof(msg) == read(pfd.fd, &msg, sizeof(msg))) {
					if (read_modem_start == false)
					read_modem_start = true;
					if ((msg & CS_CMD_MASK) == CS_RX_DATA_RECEIVED) {
						ssp_iface.rx_slot = (msg & CS_PARAM_MASK);
						//LOGV("READ %d\n", ssp_iface.rx_slot);
						address = (unsigned short *) (ssp_iface.mmap_base + ssp_iface.mmap_cfg->rx_offsets[ssp_iface.rx_slot]);

						if (ssp_iface.burst_mode) {
							/* check for codec type change */
							unsigned short *ptr_dl_ctrl = (address + PCM_BM_SIG_SIZE);
							ssp_iface.codec = DL_CTRL_CODEC(ptr_dl_ctrl);
							ssp_iface.pcm_data_size = ssp_iface.codec == CODEC_NB ? 320 : 640;
						//	LOGV("codec %d\n", ssp_iface.codec);

							/* pcm data address */
							address += PCM_BM_CTRL_SIZE;
						}
						SRCUpsamp8to48(rx_voice.SRCHandle16in_24out, address, 160);
						ssp_iface.rx_slot++;
						ssp_iface.rx_slot %= NB_RX_BUFS;
						sem_post(&rx_voice.sem_buf);
					}
				}
			}
		}
		else {
     //       LOGV("ERROR: poll%d\n",ret);
        	}
	}
	return NULL;
}
/*************/
/* ALSA PLAY */
/*************/

void *pcm_play (void *arg)
{
	snd_pcm_sframes_t frames;
	static snd_pcm_t *handle;
	static unsigned int buffer[1920];
	int err,i,k,l;
	unsigned int *sptr;
 	unsigned int* samples;
 	snd_pcm_sframes_t frames_avail;
 	snd_pcm_sframes_t frames_avail_update;
 	snd_pcm_sframes_t frames_rewindable;

	if ((err = snd_pcm_open (&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		LOGV ("Playback open error: %s\n", snd_strerror (err));
		exit (EXIT_FAILURE);
	}

	set_pcm_params (handle, NB_FRAMES_PER_PLAY);

    while(read_modem_start == false) {
//		LOGV("FIRST WRITE\n");
	   	
 		samples = (unsigned int*)&buffer;
 		sptr = (unsigned int*)SRCGetOutputReadPtrUpsamp8to48(rx_voice.SRCHandle16in_24out,960);
 		for (k=0; k<960; k++) {
 			for (l=0; l<2; l++)
 				*samples++ = *sptr;
 			sptr++;
 		}
		frames = snd_pcm_writei (handle, &buffer, 960);
		frames = snd_pcm_writei (handle, &buffer, 960);
		frames_avail = snd_pcm_avail(handle);
 		snd_pcm_hwsync(handle);
 		frames_avail_update = snd_pcm_avail_update(handle);
	}
	while (0 == state_play ) {
		sem_wait (&rx_voice.sem_buf);
		frames_avail = snd_pcm_avail(handle);
		snd_pcm_hwsync(handle);
		frames_avail_update = snd_pcm_avail_update(handle);
		frames_rewindable = snd_pcm_rewindable(handle);
		LOGV ("frame avail just after sem wait  %d\n",frames_avail);
		if (frames_avail < 0){
			LOGV("frames avail error %s\n", snd_strerror(frames_avail));
			frames = snd_pcm_recover (handle, frames, 0);
			alsa_errors++;
		}

		/* if pcm play thread has les than one frame to play we double write the current one */
		if ( frames_avail >= 3300 ) {
	//	LOGV ("pcm_play above %d\n", frames_avail );
		samples = (unsigned int*)&buffer[0];
 		sptr = (unsigned int*)SRCGetOutputReadPtrUpsamp8to48(rx_voice.SRCHandle16in_24out,960);
 		for (k=0; k<960; k++) {
 			for (l=0; l<2; l++)
 				*samples++ = *sptr;
 			sptr++;
 		}
		frames = snd_pcm_writei (handle, &buffer[0], 960);
	//	LOGV ("RX num_read1=%d->%d (%ld)\n", rx_voice.num_read-NB_FRAMES_PER_PLAY, rx_voice.num_read, frames);
		frames = snd_pcm_writei (handle, &buffer[0], 960);
	//	LOGV ("RX num_read2=%d->%d (%ld)\n", rx_voice.num_read-NB_FRAMES_PER_PLAY, rx_voice.num_read, frames);
		if (frames < 0) {
			frames = snd_pcm_recover (handle, frames, 0);
	//		LOGV ("pcm_play recover %d (%ld)\n", rx_voice.num_read, frames);
			alsa_errors++;
		} 
		else {
	//	    LOGV ("RX num_read=%d->%d (%ld)\n", rx_voice.num_read-NB_FRAMES_PER_PLAY, rx_voice.num_read, frames);
		}
		}
		/* If pcm play thread is late to write sample more than 2 frames we reset the queue */
		
		else if ( frames_avail <= 1000 ) {
        alsa_errors++;
	//	LOGV ("pcm_play below %d\n",frames_avail) ;
		frames = snd_pcm_recover (handle,frames, 0);

		}
		else if ( frames_avail > 1000 && frames_avail < 3300) {

		samples = (unsigned int*)&buffer[0];
 		sptr = (unsigned int*)SRCGetOutputReadPtrUpsamp8to48(rx_voice.SRCHandle16in_24out,960);
 		for (k=0; k<960; k++) {
 			for (l=0; l<2; l++)
 				*samples++ = *sptr;
 			sptr++;
 		}
	//	LOGV("before writei normal case\n");
		frames = snd_pcm_writei (handle, &buffer[0], 960);
	//	LOGV ("frame avail %d\n",frames_avail) ;
		
		if (frames < 0) {
			frames = snd_pcm_recover (handle, frames, 0);
	//		LOGV ("pcm_play recover %d (%ld)\n", rx_voice.num_read, frames);
			alsa_errors++;
		} 
		else {
	//	    LOGV ("RX num_read=%d->%d (%ld)\n", rx_voice.num_read-NB_FRAMES_PER_PLAY, rx_voice.num_read, frames);
		}
		}
		if (alsa_errors > 0){
        	state_play++;
			}
		}

	snd_pcm_close (handle);
	return 0;
}

/**************/
/* MAIN body  */
/**************/

int asf_start (void)
{
	struct cs_buffer_config buf_cfg;
	int ret;
	struct sigaction action;
	action.sa_handler = sighandler;
	sigaction (SIGINT, &action, NULL);
	static unsigned int buffer[1920];
	pthread_t thread_capture;
	pthread_t thread_tx;
	pthread_t thread_play;
	pthread_t thread_rx;

	/* Our process ID and Session ID */
	pid_t pid, sid;
	/* Signal handling structure */
	struct sigaction sigact;
	sigset_t waitset;
	int sig;

	/* Fork off the parent process */
	pid = fork();
	if (pid < 0)
		exit(EXIT_FAILURE);

	if (pid > 0)
		return pid;

	/* Change the file mode mask */
	umask(0);

	/*-------------------------------------*/
	LOGV("BRIDGEAPP\n");

	memset(&ssp_iface, 0, sizeof(ssp_iface));
	memset(&action, 0, sizeof(action));
	sigaction(SIGINT, &action, NULL);

	sem_init (&tx_voice.sem_buf, 0, 0);
	sem_init (&rx_voice.sem_buf, 0, 0);

 	ssp_iface.env_burst_mode = getenv(BURSTEMODE_ENVVAR);

  	if (NULL == ssp_iface.env_burst_mode)
	{
		LOGV("Full duplex\n");
		ssp_iface.burst_mode = 0;
	}
	else if (!strcmp(BURSTEMODE_96KHZ, ssp_iface.env_burst_mode))
	{
		LOGV("Burst Mode 96\n");
		ssp_iface.burst_mode = 1;
	}
	else if (!strcmp(BURSTEMODE_48KHZ, ssp_iface.env_burst_mode))
	{
		LOGV("Burst Mode 48\n");
		ssp_iface.burst_mode = 1;
	}
	else
	{
		LOGV("incorrectly set => Burst Mode 96\n");
		ssp_iface.burst_mode = 1;
	}

	LOGV("before open\n"); 
	ssp_iface.fd = open("/dev/cmt_speech", O_RDWR);
	LOGV("after open\n"); 
	if (ssp_iface.fd < 0) {
		LOGV("error with open /dev/cmt_speech\n");
		return 0;
	}

	/* Narrow Band => 320 */
	/* Wide Band   => 640 */
	/* we start by default in NB. Then driver will automatically detects */
	//ssp_iface.burst_mode = 1;

	ssp_iface.pcm_data_size = 320;

	/* MMAP */
	ssp_iface.mmap_size = MAP_SIZE; /* map one page */
	ssp_iface.mmap_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, ssp_iface.fd, 0);
	if (ssp_iface.mmap_base == (void *) -1) FATAL;
	LOGV("Memory mapped at address %p\n", ssp_iface.mmap_base); 
	ssp_iface.mmap_cfg = (struct cs_mmap_config_block *)ssp_iface.mmap_base;

		/* 4) IOCTL */
	buf_cfg.rx_bufs = NB_RX_BUFS;
	buf_cfg.tx_bufs = NB_TX_BUFS;
	if (ssp_iface.burst_mode) {
		buf_cfg.flags = CS_FEAT_BURST_MODE;
		buf_cfg.buf_size = PCM_BM_BUF_SIZE;
	} else {
		buf_cfg.flags = 0;
		buf_cfg.buf_size = PCM_DUPLEX_BUF_SIZE;
	}
	LOGV("before ioctl\n"); 
	ret = ioctl(ssp_iface.fd, CS_CONFIG_BUFS, &buf_cfg);
	LOGV("after ioctl\n");

	if (ret < 0) {
		LOGV("error %d with ioctl\n", ret);
		return 0;
	}
	if (ssp_iface.burst_mode) 
	rx_voice.SRCHandle16in_24out = SRCInitUpsamp8to48(ssp_iface.mmap_base + ssp_iface.mmap_cfg->rx_offsets[ssp_iface.rx_slot], NB_RX_BUFS*(160+PCM_BM_CTRL_SIZE), 960);
	else 
	rx_voice.SRCHandle16in_24out = SRCInitUpsamp8to48(ssp_iface.mmap_base + ssp_iface.mmap_cfg->rx_offsets[ssp_iface.rx_slot], NB_RX_BUFS*160, 960);

	tx_voice.SRCHandle24in_16out = SRCInitDownsamp48to8(4*960, NB_TX_BUFS*160);

	start_thread_with_priority_max (&thread_tx, &pcm_tx, NULL);
	start_thread_with_priority_max (&thread_rx, &pcm_rx, NULL);

	start_thread_with_priority_max (&thread_capture, &pcm_capture, NULL);

	state_play=0;
	while (state_play < 10 ) {
	state_play = 0;
	alsa_errors = 0;
	LOGV(" START capture and play\n");
	sem_init (&rx_voice.sem_buf, 0, 0);
	pthread_create(&thread_play, NULL, &pcm_play, NULL);
	pthread_join (thread_play, NULL);
	sleep(1);
	}
	pthread_join (thread_capture, NULL);
	pthread_join (thread_tx, NULL);
	pthread_join (thread_rx, NULL);
	LOGV(" STOP\n");
	return 0;

}
