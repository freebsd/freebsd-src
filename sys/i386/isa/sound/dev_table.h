/*
 * dev_table.h
 * 
 * Global definitions for device call tables
 * 
 * Copyright by Hannu Savolainen 1993
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. 2.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 */

#ifndef _DEV_TABLE_H_
#define _DEV_TABLE_H_

/*
 * NOTE! 	NOTE!	NOTE!	NOTE!
 * 
 * If you modify this file, please check the dev_table.c also.
 * 
 * NOTE! 	NOTE!	NOTE!	NOTE!
 */

extern int      sound_started;

struct driver_info {
	char	*driver_id;
	int      card_subtype;	/* Driver specific. Usually 0 */
	int      card_type;	/* From soundcard.h	 */
	char    *name;
	void    (*attach) (struct address_info * hw_config);
	int     (*probe) (struct address_info * hw_config);
};

struct card_info {
	int     card_type;	/* Link (search key) to the driver list */
	struct address_info config;
	int     enabled;
};

typedef struct pnp_sounddev {
	int     id;
	void    (*setup) (void *dev);
	char    *driver_name;
}               pnp_sounddev;

/*
 * Device specific parameters (used only by dmabuf.c)
 */
#define MAX_SUB_BUFFERS		(32*MAX_REALTIME_FACTOR)

#define DMODE_NONE		0
#define DMODE_OUTPUT		1
#define DMODE_INPUT		2

struct dma_buffparms {
	int	dma_mode;	/* DMODE_INPUT, DMODE_OUTPUT or DMODE_NONE */

	char	*raw_buf;	/* Pointers to raw buffers */
	u_long	raw_buf_phys;

	/*
	 * Device state tables
	 */

	u_long	flags;
#define DMA_BUSY	0x00000001
#define DMA_RESTART	0x00000002
#define DMA_ACTIVE	0x00000004
#define DMA_STARTED	0x00000008
#define DMA_ALLOC_DONE	0x00000020

	int	open_mode;

	/*
	 * Queue parameters.
	 */
	int             qlen;
	int             qhead;
	int             qtail;

	int             nbufs;
	int             counts[MAX_SUB_BUFFERS];
	int             subdivision;

	int             fragment_size;
	int             max_fragments;

	int             bytes_in_use;

	int             underrun_count;
	int             byte_counter;

	int             mapping_flags;
#define			DMA_MAP_MAPPED		0x00000001
	char            neutral_byte;
        int             dma_chan;
};

/*
 * Structure for use with various microcontrollers and DSP processors in the
 * recent soundcards.
 */
typedef struct coproc_operations {
	char	name[32];
	int	(*open) (void *devc, int sub_device);
	void	(*close) (void *devc, int sub_device);
	int	(*ioctl) (void *devc, u_int cmd, ioctl_arg arg, int local);
	void	(*reset) (void *devc);

	void	*devc;	/* Driver specific info */
}       coproc_operations;

struct audio_operations {
	char            name[32];
	int             flags;
#define NOTHING_SPECIAL 	0
#define NEEDS_RESTART		1
#define DMA_AUTOMODE		2
#define DMA_DUPLEX		4
	int	format_mask;	/* Bitmask for supported audio formats */
	void    *devc;	/* Driver specific info */
	int     (*open) (int dev, int mode);
	void    (*close) (int dev);
	void    (*output_block) (int dev, unsigned long buf,
		int count, int intrflag, int dma_restart);
	void    (*start_input) (int dev, unsigned long buf,
		int count, int intrflag, int dma_restart);
	int     (*ioctl) (int dev, u_int cmd, ioctl_arg arg, int local);
	int     (*prepare_for_input) (int dev, int bufsize, int nbufs);
	int     (*prepare_for_output) (int dev, int bufsize, int nbufs);
	void    (*reset) (int dev);
	void    (*halt_xfer) (int dev);
	int     (*local_qlen) (int dev);
	void    (*copy_from_user) (int dev, char *localbuf, int localoffs,
		snd_rw_buf * userbuf, int useroffs, int len);
	void    (*halt_input) (int dev);
	void    (*halt_output) (int dev);
	void    (*trigger) (int dev, int bits);
	long    buffsize;
	int     dmachan1, dmachan2;
	struct dma_buffparms *dmap_in, *dmap_out;
	struct coproc_operations *coproc;
	int     mixer_dev;
	int     enable_bits;
	int     open_mode;
	int     go;
	int     otherside;
	int     busy;
};

struct mixer_operations {
	char    name[32];
	int     (*ioctl) (int dev, unsigned int cmd, ioctl_arg arg);
};

struct synth_operations {
	struct synth_info *info;
	int     midi_dev;
	int     synth_type;
	int     synth_subtype;

	int     (*open) (int dev, int mode);
	void    (*close) (int dev);
	int     (*ioctl) (int dev, unsigned int cmd, ioctl_arg arg);
	int     (*kill_note) (int dev, int voice, int note, int velocity);
	int     (*start_note) (int dev, int voice, int note, int velocity);
	int     (*set_instr) (int dev, int voice, int instr);
	void    (*reset) (int dev);
	void    (*hw_control) (int dev, unsigned char *event);
	int     (*load_patch) (int dev, int format, snd_rw_buf * addr,
		int offs, int count, int pmgr_flag);
	void    (*aftertouch) (int dev, int voice, int pressure);
	void    (*controller) (int dev, int voice, int ctrl_num, int value);
	void    (*panning) (int dev, int voice, int value);
	void    (*volume_method) (int dev, int mode);
	int     (*pmgr_interface) (int dev, struct patmgr_info * info);
	void    (*bender) (int dev, int chn, int value);
	int     (*alloc_voice) (int dev, int chn, int note, struct voice_alloc_info * alloc);
	void    (*setup_voice) (int dev, int voice, int chn);
	int     (*send_sysex) (int dev, unsigned char *bytes, int len);

	struct voice_alloc_info alloc;
	struct channel_info chn_info[16];
};

struct midi_input_info {	/* MIDI input scanner variables */
#define MI_MAX	10
	int             m_busy;
	unsigned char   m_buf[MI_MAX];
	unsigned char   m_prev_status;	/* For running status */
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
	int     (*open) (int dev, int mode,
	        void (*inputintr) (int dev, unsigned char data),
		void (*outputintr) (int dev) );
	void    (*close) (int dev);
	int     (*ioctl) (int dev, unsigned int cmd, ioctl_arg arg);
	int     (*putc) (int dev, unsigned char data);
	int     (*start_read) (int dev);
	int     (*end_read) (int dev);
	void    (*kick) (int dev);
	int     (*command) (int dev, unsigned char *data);
	int     (*buffer_status) (int dev);
	int     (*prefix_cmd) (int dev, unsigned char status);
	struct coproc_operations *coproc;
};

struct sound_lowlev_timer {
	int     dev;
	u_int   (*tmr_start) (int dev, unsigned int usecs);
	void    (*tmr_disable) (int dev);
	void    (*tmr_restart) (int dev);
};

struct sound_timer_operations {
	struct sound_timer_info info;
	int     priority;
	int     devlink;
	int     (*open) (int dev, int mode);
	void    (*close) (int dev);
	int     (*event) (int dev, unsigned char *ev);
	u_long  (*get_time) (int dev);
	int     (*ioctl) (int dev, unsigned int cmd, ioctl_arg arg);
	void    (*arm_timer) (int dev, long time);
};

#ifdef _DEV_TABLE_C_
struct audio_operations *audio_devs[MAX_AUDIO_DEV] = {NULL};
int	num_audiodevs = 0;
struct mixer_operations *mixer_devs[MAX_MIXER_DEV] = {NULL};
int	num_mixers = 0;
struct synth_operations *synth_devs[MAX_SYNTH_DEV + MAX_MIDI_DEV] = {NULL};
int	num_synths = 0;
struct midi_operations *midi_devs[MAX_MIDI_DEV] = {NULL};
int	num_midis = 0;

#ifdef CONFIG_SEQUENCER
extern struct sound_timer_operations default_sound_timer;
struct sound_timer_operations *sound_timer_devs[MAX_TIMER_DEV] =
	{&default_sound_timer, NULL};
int             num_sound_timers = 1;
#else
struct sound_timer_operations *sound_timer_devs[MAX_TIMER_DEV] = {NULL};
int             num_sound_timers = 0;
#endif

/*
 * List of low level drivers compiled into the kernel.
 *
 * remember, each entry contains:

	char	*driver_id;
	int      card_subtype;	(Driver specific. Usually 0)
	int      card_type;	(From soundcard.h)
	char    *name;
	void    (*attach) (struct address_info * hw_config);
	int     (*probe) (struct address_info * hw_config);
 *
 */

struct driver_info sound_drivers[] = {

#ifdef CONFIG_PSS
    {"PSSECHO", 0, SNDCARD_PSS, "Echo Personal Sound System PSS (ESC614)",
		attach_pss, probe_pss},
    {"PSSMPU", 0, SNDCARD_PSS_MPU, "PSS-MPU",
		attach_pss_mpu, probe_pss_mpu},
    {"PSSMSS", 0, SNDCARD_PSS_MSS, "PSS-MSS",
		attach_pss_mss, probe_pss_mss},
#endif

#ifdef CONFIG_MSS
    /* XXX changed type from 0 to 1 -lr 970705 */
    {"MSS", 1, SNDCARD_MSS, "MS Sound System",
		attach_mss, probe_mss},
    /* MSS without IRQ/DMA config registers (for DEC Alphas) */
    {"PCXBJ", 1, SNDCARD_PSEUDO_MSS, "MS Sound System (AXP)",
		attach_mss, probe_mss},
#endif

#ifdef CONFIG_MAD16
    {"MAD16", 0, SNDCARD_MAD16, "MAD16/Mozart (MSS)",
		attach_mad16, probe_mad16},
    {"MAD16MPU", 0, SNDCARD_MAD16_MPU, "MAD16/Mozart (MPU)",
		attach_mad16_mpu, probe_mad16_mpu},
#endif

#ifdef CONFIG_CS4232
    {"CS4232", 0, SNDCARD_CS4232, "CS4232",
		attach_cs4232, probe_cs4232},
    {"CS4232MPU", 0, SNDCARD_CS4232_MPU, "CS4232 MIDI",
		attach_cs4232_mpu, probe_cs4232_mpu},
#endif

#ifdef CONFIG_YM3812
    {"OPL3", 0, SNDCARD_ADLIB, "OPL-2/OPL-3 FM",
		attach_adlib_card, probe_adlib},
#endif

#ifdef CONFIG_PAS
    {"PAS16", 0, SNDCARD_PAS, "ProAudioSpectrum",
		attach_pas_card, probe_pas},
#endif

#if defined(CONFIG_MPU401) && defined(CONFIG_MIDI)
    {"MPU401", 0, SNDCARD_MPU401, "Roland MPU-401",
		attach_mpu401, probe_mpu401},
#endif

#if defined(CONFIG_MAUI)
    {"MAUI", 0, SNDCARD_MAUI, "TB Maui",
		attach_maui, probe_maui},
#endif

#if defined(CONFIG_UART6850) && defined(CONFIG_MIDI)
    {"MIDI6850", 0, SNDCARD_UART6850, "6860 UART Midi",
		attach_uart6850, probe_uart6850},
#endif

#ifdef CONFIG_SB
    {"SBLAST", 0, SNDCARD_SB, "SoundBlaster",
		attach_sb_card, probe_sb},
#endif

#if defined(CONFIG_SB) && defined(CONFIG_SB16)
#ifdef CONFIG_AUDIO
    {"SB16", 0, SNDCARD_SB16, "SoundBlaster16",
		sb16_dsp_init, sb16_dsp_detect},
#endif
#ifdef CONFIG_AWE32
    {"AWE32", 0, SNDCARD_AWE32,     "AWE32 Synth",
		 attach_awe, probe_awe},
#endif
#ifdef CONFIG_MIDI
    {"SB16MIDI", 0, SNDCARD_SB16MIDI, "SB16 MIDI",
		attach_sb16midi, probe_sb16midi},
#endif
#endif

#ifdef CONFIG_GUS16
    {"GUS16", 0, SNDCARD_GUS16, "Ultrasound 16-bit opt.",
		attach_gus_db16, probe_gus_db16},
#endif

#ifdef CONFIG_GUS
    {"GUS", 0, SNDCARD_GUS, "Gravis Ultrasound",
		attach_gus_card, probe_gus},
#endif

#ifdef CONFIG_SSCAPE
    {"SSCAPE", 0, SNDCARD_SSCAPE, "Ensoniq Soundscape",
		attach_sscape, probe_sscape},
    {"SSCAPEMSS", 0, SNDCARD_SSCAPE_MSS, "MS Sound System (SoundScape)",
		attach_ss_mss, probe_ss_mss},
#endif

#if	NTRIX > 0
    {"TRXPRO", 0, SNDCARD_TRXPRO, "MediaTriX AudioTriX Pro",
		attach_trix_wss, probe_trix_wss},
    {"TRXPROSB", 0, SNDCARD_TRXPRO_SB, "AudioTriX (SB mode)",
		attach_trix_sb, probe_trix_sb},
    {"TRXPROMPU", 0, SNDCARD_TRXPRO_MPU, "AudioTriX MIDI",
		attach_trix_mpu, probe_trix_mpu},
#endif

#ifdef CONFIG_PNP
    {"AD1848", 0, 500, "PnP MSS",
		attach_pnp_ad1848, probe_pnp_ad1848},
#endif

    {NULL, 0, 0, "*?*", NULL, NULL}
};

int             num_sound_drivers =
sizeof(sound_drivers) / sizeof(struct driver_info);
int             max_sound_drivers =
sizeof(sound_drivers) / sizeof(struct driver_info);

#define FULL_SOUND

#ifndef FULL_SOUND
/*
 * List of devices actually configured in the system.
 * 
 * Note! The detection order is significant. Don't change it.
 *
 * remember, the list contains
 *
 *	int     card_type;	(Link (search key) to the driver list)
 *	struct address_info config;
 *		io_base, irq, dma, dma2,
 *		always_detect, char *name, struct... *osp
 *	int     enabled;
 *	void    *for_driver_use;
 *
 */

struct card_info snd_installed_cards[] = {
#ifdef CONFIG_PSS
    {SNDCARD_PSS, {PSS_BASE, 0, -1, -1}, SND_DEFAULT_ENABLE},
#ifdef PSS_MPU_BASE
    {SNDCARD_PSS_MPU, {PSS_MPU_BASE, PSS_MPU_IRQ, 0, -1}, SND_DEFAULT_ENABLE},
#endif
#ifdef PSS_MSS_BASE
    {SNDCARD_PSS_MSS, {PSS_MSS_BASE, PSS_MSS_IRQ, PSS_MSS_DMA, -1}, SND_DEFAULT_ENABLE},
#endif
#endif	/* config PSS */

#if	NTRIX > 0
    {SNDCARD_TRXPRO, {TRIX_BASE, TRIX_IRQ, TRIX_DMA, TRIX_DMA2}, SND_DEFAULT_ENABLE},
#ifdef TRIX_SB_BASE
    {SNDCARD_TRXPRO_SB, {TRIX_SB_BASE, TRIX_SB_IRQ, TRIX_SB_DMA, -1}, SND_DEFAULT_ENABLE},
#endif
#ifdef TRIX_MPU_BASE
    {SNDCARD_TRXPRO_MPU, {TRIX_MPU_BASE, TRIX_MPU_IRQ, 0, -1}, SND_DEFAULT_ENABLE},
#endif
#endif	/* 	NTRIX > 0	*/

#ifdef CONFIG_SSCAPE
    {SNDCARD_SSCAPE, {SSCAPE_BASE, SSCAPE_IRQ, SSCAPE_DMA, -1}, SND_DEFAULT_ENABLE},
    {SNDCARD_SSCAPE_MSS, {SSCAPE_MSS_BASE, SSCAPE_MSS_IRQ, SSCAPE_MSS_DMA, -1}, SND_DEFAULT_ENABLE},
#endif

#ifdef CONFIG_MAD16
    {SNDCARD_MAD16, {MAD16_BASE, MAD16_IRQ, MAD16_DMA, MAD16_DMA2}, SND_DEFAULT_ENABLE},
#ifdef MAD16_MPU_BASE
    {SNDCARD_MAD16_MPU, {MAD16_MPU_BASE, MAD16_MPU_IRQ, 0, -1}, SND_DEFAULT_ENABLE},
#endif
#endif	/* CONFIG_MAD16 */

#ifdef CONFIG_CS4232
#ifdef CS4232_MPU_BASE
    {SNDCARD_CS4232_MPU, {CS4232_MPU_BASE, CS4232_MPU_IRQ, 0, -1}, SND_DEFAULT_ENABLE},
#endif
    {SNDCARD_CS4232, {CS4232_BASE, CS4232_IRQ, CS4232_DMA, CS4232_DMA2}, SND_DEFAULT_ENABLE},
#endif

#ifdef CONFIG_MSS
#ifdef PSEUDO_MSS
    {SNDCARD_MSS, {MSS_BASE, MSS_IRQ, MSS_DMA, -1}, SND_DEFAULT_ENABLE},
#else
    {SNDCARD_PSEUDO_MSS, {MSS_BASE, MSS_IRQ, MSS_DMA, -1}, SND_DEFAULT_ENABLE},
#endif
#ifdef MSS2_BASE
    {SNDCARD_MSS, {MSS2_BASE, MSS2_IRQ, MSS2_DMA, -1}, SND_DEFAULT_ENABLE},
#endif
#endif	/* CONFIG_MSS */

#ifdef CONFIG_PAS
    {SNDCARD_PAS, {PAS_BASE, PAS_IRQ, PAS_DMA, -1}, SND_DEFAULT_ENABLE},
#endif

#ifdef CONFIG_SB
#ifndef SBC_DMA
#define SBC_DMA		1
#endif
    {SNDCARD_SB, {SBC_BASE, SBC_IRQ, SBC_DMA, -1}, SND_DEFAULT_ENABLE},
#endif

#if defined(CONFIG_MAUI)
    {SNDCARD_MAUI, {MAUI_BASE, MAUI_IRQ, 0, -1}, SND_DEFAULT_ENABLE},
#endif

#if defined(CONFIG_MPU401) && defined(CONFIG_MIDI)
    {SNDCARD_MPU401, {MPU_BASE, MPU_IRQ, 0, -1}, SND_DEFAULT_ENABLE},
#ifdef MPU2_BASE
    {SNDCARD_MPU401, {MPU2_BASE, MPU2_IRQ, 0, -1}, SND_DEFAULT_ENABLE},
#endif
#ifdef MPU3_BASE
    {SNDCARD_MPU401, {MPU3_BASE, MPU2_IRQ, 0, -1}, SND_DEFAULT_ENABLE},
#endif
#endif

#if defined(CONFIG_UART6850) && defined(CONFIG_MIDI)
    {SNDCARD_UART6850, {U6850_BASE, U6850_IRQ, 0, -1}, SND_DEFAULT_ENABLE},
#endif

#if defined(CONFIG_SB) && defined(CONFIG_SB16)
#ifdef CONFIG_AUDIO
    {SNDCARD_SB16, {SBC_BASE, SBC_IRQ, SB16_DMA, -1}, SND_DEFAULT_ENABLE},
#endif
#ifdef CONFIG_MIDI
    {SNDCARD_SB16MIDI, {SB16MIDI_BASE, SBC_IRQ, 0, -1}, SND_DEFAULT_ENABLE},
#endif
#ifdef CONFIG_AWE32
    {SNDCARD_AWE32,{AWE32_BASE, 0, 0, -1}, SND_DEFAULT_ENABLE},
#endif
#endif

#ifdef CONFIG_GUS
#ifdef CONFIG_GUS16
    {SNDCARD_GUS16, {GUS16_BASE, GUS16_IRQ, GUS16_DMA, -1}, SND_DEFAULT_ENABLE},
#endif
    {SNDCARD_GUS, {GUS_BASE, GUS_IRQ, GUS_DMA, GUS_DMA2}, SND_DEFAULT_ENABLE},
#endif

#ifdef CONFIG_YM3812
    {SNDCARD_ADLIB, {FM_MONO, 0, 0, -1}, SND_DEFAULT_ENABLE},
#endif
    /* Define some expansion space */
    {0, {0}, 0},
    {0, {0}, 0},
    {0, {0}, 0},
    {0, {0}, 0},
    {0, {0}, 0}
};

int num_sound_cards = sizeof(snd_installed_cards) / sizeof(struct card_info);
int max_sound_cards = sizeof(snd_installed_cards) / sizeof(struct card_info);

#else
int num_sound_cards = 0;
struct card_info snd_installed_cards[20] = {{0}};
int max_sound_cards = 20;
#endif

#ifdef MODULE
int trace_init = 0;
#else
int trace_init = 1;
#endif

#else
extern struct audio_operations *audio_devs[MAX_AUDIO_DEV];
int num_audiodevs;
extern struct mixer_operations *mixer_devs[MAX_MIXER_DEV];
extern int      num_mixers;
extern struct synth_operations *synth_devs[MAX_SYNTH_DEV + MAX_MIDI_DEV];
extern int      num_synths;
extern struct midi_operations *midi_devs[MAX_MIDI_DEV];
extern int      num_midis;
extern struct sound_timer_operations *sound_timer_devs[MAX_TIMER_DEV];
extern int      num_sound_timers;

extern struct driver_info sound_drivers[];
extern int      num_sound_drivers;
extern int      max_sound_drivers;
extern struct card_info snd_installed_cards[];
extern int      num_sound_cards;
extern int      max_sound_cards;

extern int      trace_init;

void            sndtable_init(void);
int             sndtable_get_cardcount(void);
struct address_info *sound_getconf(int card_type);
void            sound_chconf(int card_type, int ioaddr, int irq, int dma);
int             snd_find_driver(int type);
int             sndtable_identify_card(char *name);
void            sound_setup(char *str, int *ints);

int             sound_alloc_dmap(int dev, struct dma_buffparms * dmap, int chan);
void            sound_free_dmap(int dev, struct dma_buffparms * dmap);
extern int      soud_map_buffer(int dev, struct dma_buffparms * dmap, buffmem_desc * info);
void            install_pnp_sounddrv(struct pnp_sounddev * drv);
int             sndtable_probe(int unit, struct address_info * hw_config);
int             sndtable_init_card(int unit, struct address_info * hw_config);
void            sound_timer_init(struct sound_lowlev_timer * t, char *name);
int 
sound_start_dma(int dev, struct dma_buffparms * dmap, int chan,
		unsigned long physaddr,
		int count, int dma_mode, int autoinit);
void            sound_dma_intr(int dev, struct dma_buffparms * dmap, int chan);

#endif				/* _DEV_TABLE_C_ */
#endif				/* _DEV_TABLE_H_ */
