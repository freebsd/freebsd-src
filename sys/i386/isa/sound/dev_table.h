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

struct card_info {
	int card_type;	/*	From soundcard.c	*/
	char *name;
	long (*attach) (long mem_start, struct address_info *hw_config);
	int (*probe) (struct address_info *hw_config);
	struct address_info config;
	int enabled;
};

/** UWM -- new  MIDI structure here.. **/

struct generic_midi_info{
        char *name;	/* Name of the MIDI device.. */
        long (*attach) (long mem_start);
};

struct audio_operations {
        char name[32];
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
	int (*has_output_drained)(int dev);
        void (*copy_from_user)(int dev, char *localbuf, int localoffs,
                               snd_rw_buf *userbuf, int useroffs, int len);
};

struct mixer_operations {
	int (*ioctl) (int dev, unsigned int cmd, unsigned int arg);
};

struct synth_operations {
	struct synth_info *info;
	int synth_type;
	int synth_subtype;

	int (*open) (int dev, int mode);
	void (*close) (int dev);
	int (*ioctl) (int dev, unsigned int cmd, unsigned int arg);
	int (*kill_note) (int dev, int voice, int velocity);
	int (*start_note) (int dev, int voice, int note, int velocity);
	int (*set_instr) (int dev, int voice, int instr);
	void (*reset) (int dev);
	void (*hw_control) (int dev, unsigned char *event);
	int (*load_patch) (int dev, int format, snd_rw_buf *addr,
	     int offs, int count, int pmgr_flag);
	void (*aftertouch) (int dev, int voice, int pressure);
	void (*controller) (int dev, int voice, int ctrl_num, int value);
	void (*panning) (int dev, int voice, int value);
	int (*pmgr_interface) (int dev, struct patmgr_info *info);
};

struct midi_operations {
	struct midi_info info;
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
	int (*command) (int dev, unsigned char data);
	int (*buffer_status) (int dev);
};

/** UWM -- new structure for MIDI  **/

struct generic_midi_operations {
	struct midi_info info;
	int (*open) (int dev, int mode);
	void (*close) (int dev);
	int (*write) (int dev, snd_rw_buf *data);
	int (*read)  (int dev, snd_rw_buf *data);
};	

#ifndef ALL_EXTERNAL_TO_ME

#ifdef _MIDI_TABLE_C_

/** UWM **/
       struct generic_midi_operations * generic_midi_devs[MAX_MIDI_DEV] = {NULL}; 
       int num_generic_midis = 0, pro_midi_dev = 0; 

      struct generic_midi_info midi_supported[] = {

#ifndef EXCLUDE_PRO_MIDI
        {"ProAudioSpectrum MV101",pro_midi_attach}
#endif
        }; 

        int num_midi_drivers = 
            sizeof (midi_supported) / sizeof(struct generic_midi_info);

#endif


#ifdef _DEV_TABLE_C_   
	struct audio_operations * dsp_devs[MAX_DSP_DEV] = {NULL}; int num_dspdevs = 0;
	struct mixer_operations * mixer_devs[MAX_MIXER_DEV] = {NULL}; int num_mixers = 0;
	struct synth_operations * synth_devs[MAX_SYNTH_DEV] = {NULL}; int num_synths = 0;
	struct midi_operations * midi_devs[MAX_MIDI_DEV] = {NULL}; int num_midis = 0;


#   ifndef EXCLUDE_MPU401
        int mpu401_dev = 0;
#   endif

/*
 *	Note! The detection order is significant. Don't change it.
 */

	struct card_info supported_drivers[] = {
#if !defined(EXCLUDE_MPU401) && !defined(EXCLUDE_MIDI)
		{SNDCARD_MPU401,"Roland MPU-401",	attach_mpu401, probe_mpu401,
			{MPU_BASE, MPU_IRQ, 0}, SND_DEFAULT_ENABLE},
#endif

#ifndef EXCLUDE_PAS
		{SNDCARD_PAS,	"ProAudioSpectrum",	attach_pas_card, probe_pas,
			{PAS_BASE, PAS_IRQ, PAS_DMA}, SND_DEFAULT_ENABLE},
#endif

#ifndef EXCLUDE_SB
		{SNDCARD_SB,	"SoundBlaster",		attach_sb_card, probe_sb,
			{SBC_BASE, SBC_IRQ, SBC_DMA}, SND_DEFAULT_ENABLE},
#endif

#if !defined(EXCLUDE_SB) && !defined(EXCLUDE_SB16) && !defined(EXCLUDE_SBPRO)
#ifndef EXCLUDE_AUDIO
		{SNDCARD_SB16,	"SoundBlaster16",	sb16_dsp_init, sb16_dsp_detect,
			{SBC_BASE, SBC_IRQ, SB16_DMA}, SND_DEFAULT_ENABLE},
#endif
#ifndef EXCLUDE_MIDI
		{SNDCARD_SB16MIDI,"SB16 MPU-401",	attach_sb16midi, probe_sb16midi,
			{SB16MIDI_BASE, SBC_IRQ, 0}, SND_DEFAULT_ENABLE},
#endif
#endif

#ifndef EXCLUDE_GUS
		{SNDCARD_GUS,	"Gravis Ultrasound",	attach_gus_card, probe_gus,
			{GUS_BASE, GUS_IRQ, GUS_DMA}, SND_DEFAULT_ENABLE},
#endif

#ifndef EXCLUDE_YM3812
		{SNDCARD_ADLIB,	"AdLib",		attach_adlib_card, probe_adlib,
			{FM_MONO, 0, 0}, SND_DEFAULT_ENABLE},
#endif
		{0,			"*?*",			NULL, 0}
	};

	int num_sound_drivers =
	    sizeof(supported_drivers) / sizeof (struct card_info);


# ifndef EXCLUDE_AUDIO 
	int sound_buffcounts[MAX_DSP_DEV] = {0};
	long sound_buffsizes[MAX_DSP_DEV] = {0};
	int sound_dsp_dmachan[MAX_DSP_DEV] = {0};
	int sound_dma_automode[MAX_DSP_DEV] = {0};
# endif
#else
	extern struct audio_operations * dsp_devs[MAX_DSP_DEV]; int num_dspdevs;
	extern struct mixer_operations * mixer_devs[MAX_MIXER_DEV]; extern int num_mixers;
	extern struct synth_operations * synth_devs[MAX_SYNTH_DEV]; extern int num_synths;
	extern struct midi_operations * midi_devs[MAX_MIDI_DEV]; extern int num_midis;
#   ifndef EXCLUDE_MPU401
        extern int mpu401_dev;
#   endif

	extern struct card_info supported_drivers[];
	extern int num_sound_drivers;

# ifndef EXCLUDE_AUDIO
	extern int sound_buffcounts[MAX_DSP_DEV];
	extern long sound_buffsizes[MAX_DSP_DEV];
	extern int sound_dsp_dmachan[MAX_DSP_DEV];
	extern int sound_dma_automode[MAX_DSP_DEV];
# endif

#endif

long sndtable_init(long mem_start);
int sndtable_get_cardcount (void);
long CMIDI_init(long mem_start); /* */
struct address_info *sound_getconf(int card_type);
void sound_chconf(int card_type, int ioaddr, int irq, int dma);
#endif

#endif

/* If external to me.... :) */

#ifdef ALL_EXTERNAL_TO_ME

	extern struct audio_operations * dsp_devs[MAX_DSP_DEV]; int num_dspdevs;
        extern struct mixer_operations * mixer_devs[MAX_MIXER_DEV]; extern int num_mixers;
        extern struct synth_operations * synth_devs[MAX_SYNTH_DEV]; extern int num_synths;
        extern struct midi_operations * midi_devs[MAX_MIDI_DEV]; extern int num_midis;
	extern struct generic_midi_operations *generic_midi_devs[]; 
	extern int num_generic_midis, pro_midi_dev;
 
#ifndef EXCLUDE_MPU401
        extern int mpu401_dev;
#endif

	extern struct generic_midi_info midi_supported[];
	extern struct card_info supported_drivers[];
        extern int num_sound_drivers;
	extern int num_midi_drivers;	
#ifndef EXCLUDE_AUDIO
        extern int sound_buffcounts[MAX_DSP_DEV];
        extern long sound_buffsizes[MAX_DSP_DEV];
        extern int sound_dsp_dmachan[MAX_DSP_DEV];
        extern int sound_dma_automode[MAX_DSP_DEV];
#endif

#endif
