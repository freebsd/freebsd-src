/*
 *	dev_table.h
 *
 *	Global definitions for device call tables
 * 
 * Copyright by Hannu Savolainen 1993
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *

*/

#ifndef _DEV_TABLE_H_
#define _DEV_TABLE_H_

/*
 *	NOTE! 	NOTE!	NOTE!	NOTE!
 *
 *	If you modify this file, please check the dev_table.c also.
 *
 *	NOTE! 	NOTE!	NOTE!	NOTE!
 */

struct driver_info {
	int card_type;	/*	From soundcard.h	*/
	char *name;
	long (*attach) (long mem_start, struct address_info *hw_config);
	int (*probe) (struct address_info *hw_config);
};

struct card_info {
	int card_type;	/* Link (search key) to the driver list */
	struct address_info config;
	int enabled;
};

/*
 * Device specific parameters (used only by dmabuf.c)
 */
#define MAX_SUB_BUFFERS		(32*MAX_REALTIME_FACTOR)

#define DMODE_NONE		0
#define DMODE_OUTPUT		1
#define DMODE_INPUT		2

struct dma_buffparms {
	int      dma_mode;	/* DMODE_INPUT, DMODE_OUTPUT or DMODE_NONE */

	/*
 	 * Pointers to raw buffers
 	 */

  	char     *raw_buf[DSP_BUFFCOUNT];
    	unsigned long   raw_buf_phys[DSP_BUFFCOUNT];
    	int             raw_count;

     	/*
         * Device state tables
         */

	unsigned long flags;
#define DMA_BUSY	0x00000001
#define DMA_RESTART	0x00000002
#define DMA_ACTIVE	0x00000004
#define DMA_STARTED	0x00000008
#define DMA_ALLOC_DONE	0x00000020

	int      open_mode;

	/*
	 * Queue parameters.
	 */
       	int      qlen;
       	int      qhead;
       	int      qtail;

	int      nbufs;
	int      counts[MAX_SUB_BUFFERS];
	int      subdivision;
	char    *buf[MAX_SUB_BUFFERS];
	unsigned long buf_phys[MAX_SUB_BUFFERS];

	int      fragment_size;
	int	 max_fragments;

	int	 bytes_in_use;

	int	 underrun_count;
};

/*
 * Structure for use with various microcontrollers and DSP processors 
 * in the recent soundcards.
 */
typedef struct coproc_operations {
		char name[32];
		int (*open) (void *devc, int sub_device);
		void (*close) (void *devc, int sub_device);
		int (*ioctl) (void *devc, unsigned int cmd, unsigned int arg, int local);
		void (*reset) (void *devc);

		void *devc;		/* Driver specific info */
	} coproc_operations;

struct audio_operations {
        char name[32];
	int flags;
#define NOTHING_SPECIAL 	0
#define NEEDS_RESTART		1
#define DMA_AUTOMODE		2
	int  format_mask;	/* Bitmask for supported audio formats */
	void *devc;		/* Driver specific info */
	int (*open) (int dev, int mode);
	void (*close) (int dev);
	void (*output_block) (int dev, unsigned long buf, 
			      int count, int intrflag, int dma_restart);
	void (*start_input) (int dev, unsigned long buf, 
			     int count, int intrflag, int dma_restart);
	int (*ioctl) (int dev, unsigned int cmd, unsigned int arg, int local);
	int (*prepare_for_input) (int dev, int bufsize, int nbufs);
	int (*prepare_for_output) (int dev, int bufsize, int nbufs);
	void (*reset) (int dev);
	void (*halt_xfer) (int dev);
	int (*local_qlen)(int dev);
        void (*copy_from_user)(int dev, char *localbuf, int localoffs,
                               snd_rw_buf *userbuf, int useroffs, int len);
	int buffcount;
	long buffsize;
	int dmachan;
	struct dma_buffparms *dmap;
	struct coproc_operations *coproc;
	int mixer_dev;
};

struct mixer_operations {
	char name[32];
	int (*ioctl) (int dev, unsigned int cmd, unsigned int arg);
};

struct synth_operations {
	struct synth_info *info;
	int midi_dev;
	int synth_type;
	int synth_subtype;

	int (*open) (int dev, int mode);
	void (*close) (int dev);
	int (*ioctl) (int dev, unsigned int cmd, unsigned int arg);
	int (*kill_note) (int dev, int voice, int note, int velocity);
	int (*start_note) (int dev, int voice, int note, int velocity);
	int (*set_instr) (int dev, int voice, int instr);
	void (*reset) (int dev);
	void (*hw_control) (int dev, unsigned char *event);
	int (*load_patch) (int dev, int format, snd_rw_buf *addr,
	     int offs, int count, int pmgr_flag);
	void (*aftertouch) (int dev, int voice, int pressure);
	void (*controller) (int dev, int voice, int ctrl_num, int value);
	void (*panning) (int dev, int voice, int value);
	void (*volume_method) (int dev, int mode);
	int (*pmgr_interface) (int dev, struct patmgr_info *info);
	void (*bender) (int dev, int chn, int value);
	int (*alloc_voice) (int dev, int chn, int note, struct voice_alloc_info *alloc);
	void (*setup_voice) (int dev, int voice, int chn);

 	struct voice_alloc_info alloc;
 	struct channel_info chn_info[16];
};

struct midi_input_info { /* MIDI input scanner variables */
#define MI_MAX	10
    		int             m_busy;
    		unsigned char   m_buf[MI_MAX];
		unsigned char	m_prev_status;	/* For running status */
    		int             m_ptr;
#define MST_INIT			0
#define MST_DATA			1
#define MST_SYSEX			2
    		int             m_state;
    		int             m_left;
	};

struct midi_operations {
	struct midi_info info;
	struct synth_operations *converter;
	struct midi_input_info in_info;
	int (*open) (int dev, int mode,
		void (*inputintr)(int dev, unsigned char data),
		void (*outputintr)(int dev)
		);
	void (*close) (int dev);
	int (*ioctl) (int dev, unsigned int cmd, unsigned int arg);
	int (*putc) (int dev, unsigned char data);
	int (*start_read) (int dev);
	int (*end_read) (int dev);
	void (*kick)(int dev);
	int (*command) (int dev, unsigned char *data);
	int (*buffer_status) (int dev);
	int (*prefix_cmd) (int dev, unsigned char status);
	struct coproc_operations *coproc;
};

struct sound_timer_operations {
	struct sound_timer_info info;
	int priority;
	int devlink;
	int (*open)(int dev, int mode);
	void (*close)(int dev);
	int (*event)(int dev, unsigned char *ev);
	unsigned long (*get_time)(int dev);
	int (*ioctl) (int dev, unsigned int cmd, unsigned int arg);
	void (*arm_timer)(int dev, long time);
};

#ifdef _DEV_TABLE_C_   
	struct audio_operations *audio_devs[MAX_AUDIO_DEV] = {NULL}; int num_audiodevs = 0;
	struct mixer_operations *mixer_devs[MAX_MIXER_DEV] = {NULL}; int num_mixers = 0;
	struct synth_operations *synth_devs[MAX_SYNTH_DEV+MAX_MIDI_DEV] = {NULL}; int num_synths = 0;
	struct midi_operations *midi_devs[MAX_MIDI_DEV] = {NULL}; int num_midis = 0;

#ifndef EXCLUDE_SEQUENCER
	extern struct sound_timer_operations default_sound_timer;
	struct sound_timer_operations *sound_timer_devs[MAX_TIMER_DEV] = 
		{&default_sound_timer, NULL}; 
	int num_sound_timers = 1;
#else
	struct sound_timer_operations *sound_timer_devs[MAX_TIMER_DEV] = 
		{NULL}; 
	int num_sound_timers = 0;
#endif

/*
 * List of low level drivers compiled into the kernel.
 */

	struct driver_info sound_drivers[] = {
#ifndef EXCLUDE_PSS
	  {SNDCARD_PSS, "Echo Personal Sound System PSS (ESC614)", attach_pss, probe_pss},
#	ifdef PSS_MPU_BASE
	  {SNDCARD_PSS_MPU, "PSS-MPU", attach_pss_mpu, probe_pss_mpu},
#	endif
#	ifdef PSS_MSS_BASE
	  {SNDCARD_PSS_MSS, "PSS-MSS", attach_pss_mss, probe_pss_mss},
#	endif
#endif
#ifndef EXCLUDE_YM3812
		{SNDCARD_ADLIB,	"OPL-2/OPL-3 FM",		attach_adlib_card, probe_adlib},
#endif
#ifndef EXCLUDE_PAS
		{SNDCARD_PAS,	"ProAudioSpectrum",	attach_pas_card, probe_pas},
#endif
#if !defined(EXCLUDE_MPU401) && !defined(EXCLUDE_MIDI)
		{SNDCARD_MPU401,"Roland MPU-401",	attach_mpu401, probe_mpu401},
#endif
#if !defined(EXCLUDE_UART6850) && !defined(EXCLUDE_MIDI)
		{SNDCARD_UART6850,"6860 UART Midi",	attach_uart6850, probe_uart6850},
#endif
#ifndef EXCLUDE_SB
		{SNDCARD_SB,	"SoundBlaster",		attach_sb_card, probe_sb},
#endif
#if !defined(EXCLUDE_SB) && !defined(EXCLUDE_SB16)
#ifndef EXCLUDE_AUDIO
		{SNDCARD_SB16,	"SoundBlaster16",	sb16_dsp_init, sb16_dsp_detect},
#endif
#ifndef EXCLUDE_MIDI
		{SNDCARD_SB16MIDI,"SB16 MIDI",	attach_sb16midi, probe_sb16midi},
#endif
#endif
#ifndef EXCLUDE_GUS16
		{SNDCARD_GUS16,	"Ultrasound 16-bit opt.",	attach_gus_db16, probe_gus_db16},
#endif
#ifndef EXCLUDE_MSS
		{SNDCARD_MSS,	"MS Sound System",	attach_ms_sound, probe_ms_sound},
#endif
#ifndef EXCLUDE_GUS
		{SNDCARD_GUS,	"Gravis Ultrasound",	attach_gus_card, probe_gus},
#endif
#ifndef EXCLUDE_SSCAPE
		{SNDCARD_SSCAPE, "Ensoniq Soundscape",	attach_sscape, probe_sscape},
		{SNDCARD_SSCAPE_MSS,	"MS Sound System (SoundScape)",	attach_ss_ms_sound, probe_ss_ms_sound},
#endif
#ifndef EXCLUDE_TRIX
		{SNDCARD_TRXPRO, "MediaTriX AudioTriX Pro",	attach_trix_wss, probe_trix_wss},
		{SNDCARD_TRXPRO_SB, "AudioTriX (SB mode)",	attach_trix_sb, probe_trix_sb},
		{SNDCARD_TRXPRO_MPU, "AudioTriX MIDI",	attach_trix_mpu, probe_trix_mpu},
#endif
		{0,			"*?*",			NULL, NULL}
	};

#if defined(linux) || defined(__FreeBSD__)
/*
 *	List of devices actually configured in the system.
 *
 *	Note! The detection order is significant. Don't change it.
 */

	struct card_info snd_installed_cards[] = {
#ifndef EXCLUDE_PSS
	     {SNDCARD_PSS, {PSS_BASE, PSS_IRQ, PSS_DMA}, SND_DEFAULT_ENABLE},
#	ifdef PSS_MPU_BASE
	     {SNDCARD_PSS_MPU, {PSS_MPU_BASE, PSS_MPU_IRQ, 0}, SND_DEFAULT_ENABLE},
#	endif
#	ifdef PSS_MSS_BASE
	     {SNDCARD_PSS_MSS, {PSS_MSS_BASE, PSS_MSS_IRQ, PSS_MSS_DMA}, SND_DEFAULT_ENABLE},
#	endif
#endif
#ifndef EXCLUDE_TRIX
	     {SNDCARD_TRXPRO, {TRIX_BASE, TRIX_IRQ, TRIX_DMA}, SND_DEFAULT_ENABLE},
#	ifdef TRIX_SB_BASE
	     {SNDCARD_TRXPRO_SB, {TRIX_SB_BASE, TRIX_SB_IRQ, TRIX_SB_DMA}, SND_DEFAULT_ENABLE},
#	endif
#	ifdef TRIX_MPU_BASE
	     {SNDCARD_TRXPRO_MPU, {TRIX_MPU_BASE, TRIX_MPU_IRQ, 0}, SND_DEFAULT_ENABLE},
#	endif
#endif
#ifndef EXCLUDE_SSCAPE
	     {SNDCARD_SSCAPE, {SSCAPE_BASE, SSCAPE_IRQ, SSCAPE_DMA}, SND_DEFAULT_ENABLE},
	     {SNDCARD_SSCAPE_MSS, {SSCAPE_MSS_BASE, SSCAPE_MSS_IRQ, SSCAPE_MSS_DMA}, SND_DEFAULT_ENABLE},
#endif

#ifndef EXCLUDE_MSS
		{SNDCARD_MSS, {MSS_BASE, MSS_IRQ, MSS_DMA}, SND_DEFAULT_ENABLE},
#	ifdef MSS2_BASE
		{SNDCARD_MSS, {MSS2_BASE, MSS2_IRQ, MSS2_DMA}, SND_DEFAULT_ENABLE},
#	endif
#endif

#ifndef EXCLUDE_PAS
		{SNDCARD_PAS, {PAS_BASE, PAS_IRQ, PAS_DMA}, SND_DEFAULT_ENABLE},
#endif

#ifndef EXCLUDE_SB
		{SNDCARD_SB, {SBC_BASE, SBC_IRQ, SBC_DMA}, SND_DEFAULT_ENABLE},
#endif

#if !defined(EXCLUDE_MPU401) && !defined(EXCLUDE_MIDI)
		{SNDCARD_MPU401, {MPU_BASE, MPU_IRQ, 0}, SND_DEFAULT_ENABLE},
#ifdef MPU2_BASE
		{SNDCARD_MPU401, {MPU2_BASE, MPU2_IRQ, 0}, SND_DEFAULT_ENABLE},
#endif
#ifdef MPU3_BASE
		{SNDCARD_MPU401, {MPU3_BASE, MPU2_IRQ, 0}, SND_DEFAULT_ENABLE},
#endif
#endif

#if !defined(EXCLUDE_UART6850) && !defined(EXCLUDE_MIDI)
		{SNDCARD_UART6850, {U6850_BASE, U6850_IRQ, 0}, SND_DEFAULT_ENABLE},
#endif

#if !defined(EXCLUDE_SB) && !defined(EXCLUDE_SB16)
#ifndef EXCLUDE_AUDIO
		{SNDCARD_SB16, {SBC_BASE, SBC_IRQ, SB16_DMA}, SND_DEFAULT_ENABLE},
#endif
#ifndef EXCLUDE_MIDI
		{SNDCARD_SB16MIDI,{SB16MIDI_BASE, SBC_IRQ, 0}, SND_DEFAULT_ENABLE},
#endif
#endif

#ifndef EXCLUDE_GUS
#ifndef EXCLUDE_GUS16
		{SNDCARD_GUS16, {GUS16_BASE, GUS16_IRQ, GUS16_DMA, GUS_DMA_READ}, SND_DEFAULT_ENABLE},
#endif
		{SNDCARD_GUS, {GUS_BASE, GUS_IRQ, GUS_DMA, GUS_DMA_READ}, SND_DEFAULT_ENABLE},
#endif

#ifndef EXCLUDE_YM3812
		{SNDCARD_ADLIB, {FM_MONO, 0, 0}, SND_DEFAULT_ENABLE},
#endif
		{0, {0}, 0}
	};

	int num_sound_cards =
	    sizeof(snd_installed_cards) / sizeof (struct card_info);

#else
	int num_sound_cards = 0;
#endif	/* linux */

	int num_sound_drivers =
	    sizeof(sound_drivers) / sizeof (struct driver_info);

#else
	extern struct audio_operations * audio_devs[MAX_AUDIO_DEV]; extern int num_audiodevs;
	extern struct mixer_operations * mixer_devs[MAX_MIXER_DEV]; extern int num_mixers;
	extern struct synth_operations * synth_devs[MAX_SYNTH_DEV+MAX_MIDI_DEV]; extern int num_synths;
	extern struct midi_operations * midi_devs[MAX_MIDI_DEV]; extern int num_midis;
	extern struct sound_timer_operations * sound_timer_devs[MAX_SYNTH_DEV+MAX_MIDI_DEV]; extern int num_sound_timers;

	extern struct driver_info sound_drivers[];
	extern int num_sound_drivers;
	extern struct card_info snd_installed_cards[];
	extern int num_sound_cards;

int sndtable_probe(int unit, struct address_info *hw_config);
int sndtable_init_card(int unit, struct address_info *hw_config);
long sndtable_init(long mem_start);
int sndtable_get_cardcount (void);
struct address_info *sound_getconf(int card_type);
void sound_chconf(int card_type, int ioaddr, int irq, int dma);
int snd_find_driver(int type);

#endif	/* _DEV_TABLE_C_ */
#endif	/* _DEV_TABLE_H_ */
