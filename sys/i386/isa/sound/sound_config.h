/* sound_config.h
 *
 * A driver for Soundcards, misc configuration parameters.
 *
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

#include <i386/isa/sound/local.h>
#include <i386/isa/sound/os.h>
#include <i386/isa/sound/soundvers.h>

#if !defined(PSS_MPU_BASE) && defined(EXCLUDE_SSCAPE) && defined(EXCLUDE_TRIX)
#define EXCLUDE_MPU_EMU
#endif

#if defined(ISC) || defined(SCO) || defined(SVR42)
#define GENERIC_SYSV
#endif

/*
 * Disable the AD1848 driver if there are no other drivers requiring it.
 */

#if defined(EXCLUDE_GUS16) && defined(EXCLUDE_MSS) && defined(EXCLUDE_PSS) && defined(EXCLUDE_GUSMAX) && defined(EXCLUDE_SSCAPE) && defined(EXCLUDE_TRIX)
#define EXCLUDE_AD1848
#endif

#ifdef PSS_MSS_BASE
#undef EXCLUDE_AD1848
#endif

#undef CONFIGURE_SOUNDCARD
#undef DYNAMIC_BUFFER

#ifdef KERNEL_SOUNDCARD
#define CONFIGURE_SOUNDCARD
#define DYNAMIC_BUFFER
#undef LOADABLE_SOUNDCARD
#endif

#ifdef EXCLUDE_SEQUENCER
#define EXCLUDE_MIDI
#define EXCLUDE_YM3812
#define EXCLUDE_OPL3
#endif

#ifndef SND_DEFAULT_ENABLE
#define SND_DEFAULT_ENABLE	1
#endif

#ifdef CONFIGURE_SOUNDCARD

/* ****** IO-address, DMA and IRQ settings ****

If your card has nonstandard I/O address or IRQ number, change defines
   for the following settings in your kernel Makefile */

#ifndef SBC_BASE
#ifdef PC98
#define SBC_BASE	0x20d2  /* 0x20d2 is the factory default. */
#else
#define SBC_BASE	0x220	/* 0x220 is the factory default. */
#endif
#endif

#ifndef SBC_IRQ
#ifdef PC98
#define SBC_IRQ		10	/* IQR10 is not the factory default on PC9821.	 */
#else
#define SBC_IRQ		7	/* IQR7 is the factory default.	 */
#endif
#endif

#ifndef SBC_DMA
#ifdef PC98
#define SBC_DMA		3
#else
#define SBC_DMA		1
#endif
#endif

#ifndef SB16_DMA
#ifdef PC98
#define SB16_DMA	3
#else
#define SB16_DMA	6
#endif
#endif

#ifndef SB16MIDI_BASE
#ifdef PC98
#define SB16MIDI_BASE	0x80d2
#else
#define SB16MIDI_BASE	0x300
#endif
#endif

#ifndef PAS_BASE
#define PAS_BASE	0x388
#endif

#ifndef PAS_IRQ
#define PAS_IRQ		5
#endif

#ifndef PAS_DMA
#define PAS_DMA		3
#endif

#ifndef GUS_BASE
#define GUS_BASE	0x220
#endif

#ifndef GUS_IRQ
#define GUS_IRQ		15
#endif

#ifndef GUS_MIDI_IRQ
#define GUS_MIDI_IRQ	GUS_IRQ
#endif

#ifndef GUS_DMA
#define GUS_DMA		6
#endif

#ifndef GUS_DMA_READ
#define GUS_DMA_READ	3
#endif

#ifndef MPU_BASE
#define MPU_BASE	0x330
#endif

#ifndef MPU_IRQ
#define MPU_IRQ		6
#endif

/* Echo Personal Sound System */
#ifndef PSS_BASE
#define PSS_BASE        0x220   /* 0x240 or */
#endif

#ifndef PSS_IRQ
#define PSS_IRQ         7
#endif

#ifndef PSS_DMA
#define PSS_DMA         1
#endif

#ifndef MSS_BASE
#define MSS_BASE 0
#endif

#ifndef MSS_DMA
#define MSS_DMA 0
#endif

#ifndef MSS_IRQ
#define MSS_IRQ 0
#endif

#ifndef GUS16_BASE
#define GUS16_BASE 0
#endif

#ifndef GUS16_DMA
#define GUS16_DMA 0
#endif

#ifndef GUS16_IRQ
#define GUS16_IRQ 0
#endif

#ifndef SSCAPE_BASE
#define SSCAPE_BASE 0
#endif

#ifndef SSCAPE_DMA
#define SSCAPE_DMA 0
#endif

#ifndef SSCAPE_IRQ
#define SSCAPE_IRQ 0
#endif

#ifndef SSCAPE_MSS_BASE
#define SSCAPE_MSS_BASE 0
#endif

#ifndef SSCAPE_MSS_DMA
#define SSCAPE_MSS_DMA 0
#endif

#ifndef SSCAPE_MSS_IRQ
#define SSCAPE_MSS_IRQ 0
#endif

#ifndef TRIX_BASE
#define	TRIX_BASE	0x530
#endif

#ifndef TRIX_IRQ
#define TRIX_IRQ	10
#endif

#ifndef TRIX_DMA
#define TRIX_DMA	1
#endif

#ifndef U6850_BASE
#define U6850_BASE	0x330
#endif
  
#ifndef U6850_IRQ
#define U6850_IRQ	5
#endif
  
#ifndef U6850_DMA
#define U6850_DMA	1
#endif

#ifndef MAX_REALTIME_FACTOR
#define MAX_REALTIME_FACTOR	4
#endif

/************* PCM DMA buffer sizes *******************/

/* If you are using high playback or recording speeds, the default buffersize
   is too small. DSP_BUFFSIZE must be 64k or less.

   A rule of thumb is 64k for PAS16, 32k for PAS+, 16k for SB Pro and
   4k for SB.

   If you change the DSP_BUFFSIZE, don't modify this file.
   Use the make config command instead. */

#ifndef DSP_BUFFSIZE
#define DSP_BUFFSIZE		(4096)
#endif

#ifndef DSP_BUFFCOUNT
#define DSP_BUFFCOUNT		2	/* 2 is recommended. */
#endif

#define DMA_AUTOINIT		0x10

#ifdef PC98
#define FM_MONO		0x28d2	/* This is the I/O address used by AdLib */
#else
#define FM_MONO		0x388	/* This is the I/O address used by AdLib */
#endif

/* SEQ_MAX_QUEUE is the maximum number of sequencer events buffered by the
   driver. (There is no need to alter this) */
#define SEQ_MAX_QUEUE	1024

#define SBFM_MAXINSTR		(256)	/* Size of the FM Instrument bank */
/* 128 instruments for general MIDI setup and 16 unassigned	 */

/*
 * Minor numbers for the sound driver.
 *
 * Unfortunately Creative called the codec chip of SB as a DSP. For this
 * reason the /dev/dsp is reserved for digitized audio use. There is a
 * device for true DSP processors but it will be called something else.
 * In v3.0 it's /dev/sndproc but this could be a temporary solution.
 */

#define SND_NDEVS	256	/* Number of supported devices */
#define SND_DEV_CTL	0	/* Control port /dev/mixer */
#define SND_DEV_SEQ	1	/* Sequencer output /dev/sequencer (FM
				   synthesizer and MIDI output) */
#define SND_DEV_MIDIN	2	/* Raw midi access */
#define SND_DEV_DSP	3	/* Digitized voice /dev/dsp */
#define SND_DEV_AUDIO	4	/* Sparc compatible /dev/audio */
#define SND_DEV_DSP16	5	/* Like /dev/dsp but 16 bits/sample */
#define SND_DEV_STATUS	6	/* /dev/sndstat */
/* #7 not in use now. Was in 2.4. Free for use after v3.0. */
#define SND_DEV_SEQ2	8	/* /dev/sequecer, level 2 interface */
#define SND_DEV_SNDPROC 9	/* /dev/sndproc for programmable devices */
#define SND_DEV_PSS	SND_DEV_SNDPROC

#define DSP_DEFAULT_SPEED	8000

#define ON		1
#define OFF		0

#define MAX_AUDIO_DEV	5
#define MAX_MIXER_DEV	5
#define MAX_SYNTH_DEV	3
#define MAX_MIDI_DEV	6
#define MAX_TIMER_DEV	3

struct fileinfo {
       	  int mode;	      /* Open mode */
	  DECLARE_FILE();     /* Reference to file-flags. OS-dependent. */
       };

struct address_info {
	int io_base;
	int irq;
	int dma;		/* write dma channel */
	int dma_read;		/* read dma channel */
	int always_detect;	/* 1=Trust me, it's there */
};

#define SYNTH_MAX_VOICES	32

struct voice_alloc_info {
		int max_voice;
		int used_voices;
		int ptr;		/* For device specific use */
		unsigned short map[SYNTH_MAX_VOICES]; /* (ch << 8) | (note+1) */
		int timestamp;
		int alloc_times[SYNTH_MAX_VOICES];
	};

struct channel_info {
		int pgm_num;
		int bender_value;
		unsigned char controllers[128];
	};

/*
 * Process wakeup reasons
 */
#define WK_NONE		0x00
#define WK_WAKEUP	0x01
#define WK_TIMEOUT	0x02
#define WK_SIGNAL	0x04
#define WK_SLEEP	0x08

#define OPEN_READ	1
#define OPEN_WRITE	2
#define OPEN_READWRITE	3

#include <i386/isa/sound/sound_calls.h>
#include <i386/isa/sound/dev_table.h>

#ifndef DEB
#define DEB(x)
#endif

#ifndef AUDIO_DDB
#define AUDIO_DDB(x)
#endif

#define TIMER_ARMED	121234
#define TIMER_NOT_ARMED	1

#endif
