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

#include "local.h"


#undef CONFIGURE_SOUNDCARD
#undef DYNAMIC_BUFFER

#ifdef KERNEL_SOUNDCARD
#define CONFIGURE_SOUNDCARD
#define DYNAMIC_BUFFER
#undef LOADABLE_SOUNDCARD
#endif

#ifdef EXCLUDE_SEQUENCER
#ifndef EXCLUDE_MIDI
#define EXCLUDE_MIDI
#endif
#ifndef EXCLUDE_YM3812
#define EXCLUDE_YM3812
#endif
#ifndef EXCLUDE_OPL3
#define EXCLUDE_OPL3
#endif
#endif

#ifndef SND_DEFAULT_ENABLE
#define SND_DEFAULT_ENABLE	1
#endif

/** UWM - new MIDI stuff **/

#ifdef EXCLUDE_CHIP_MIDI
#ifndef EXCLUDE_PRO_MIDI
#define EXCLUDE_PRO_MIDI
#endif
#endif

/** UWM - stuff **/
 
#if defined(EXCLUDE_SEQUENCER) && defined(EXCLUDE_AUDIO)
#undef CONFIGURE_SOUNDCARD
#endif

#ifdef CONFIGURE_SOUNDCARD

/* ****** IO-address, DMA and IRQ settings ****

If your card has nonstandard I/O address or IRQ number, change defines
   for the following settings in your kernel Makefile */

#ifndef SBC_BASE
#define SBC_BASE	0x220	/* 0x220 is the factory default. */
#endif

#ifndef SBC_IRQ
#define SBC_IRQ		7	/* IQR7 is the factory default.	 */
#endif

#ifndef SBC_DMA
#define SBC_DMA		1
#endif

#ifndef SB16_DMA
#define SB16_DMA	6
#endif

#ifndef SB16MIDI_BASE
#define SB16MIDI_BASE	0x300
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

#ifndef MPU_BASE
#define MPU_BASE	0x330
#endif

#ifndef MPU_IRQ
#define MPU_IRQ		6
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

#define FM_MONO		0x388	/* This is the I/O address used by AdLib */

/* SEQ_MAX_QUEUE is the maximum number of sequencer events buffered by the
   driver. (There is no need to alter this) */
#define SEQ_MAX_QUEUE	1024

#define SBFM_MAXINSTR		(256)	/* Size of the FM Instrument
						   bank				 */
/* 128 instruments for general MIDI setup and 16 unassigned	 */

#define SND_NDEVS	50	/* Number of supported devices */
#define SND_DEV_CTL	0	/* Control port /dev/mixer */
#define SND_DEV_SEQ	1	/* Sequencer output /dev/sequencer (FM
				   synthesizer and MIDI output) */
#define SND_DEV_MIDIN	2	/* MIDI input /dev/midin (not implemented
				   yet) */
#define SND_DEV_DSP	3	/* Digitized voice /dev/dsp */
#define SND_DEV_AUDIO	4	/* Sparc compatible /dev/audio */
#define SND_DEV_DSP16	5	/* Like /dev/dsp but 16 bits/sample */
#define SND_DEV_STATUS	6	/* /dev/sndstatus */

/* UWM ... note add new MIDI devices here..  
 *  Also do not forget to add table midi_supported[]
 *  Minor numbers for on-chip midi devices start from 15.. and 
 *  should be contiguous.. viz. 15,16,17....
 * ERROR!!!!!!!!! NO NO. Minor numbers above 15 are reserved!!!!!! Hannu
 *  Also note the max # of midi devices as MAX_MIDI_DEV
 */ 

#define CMIDI_DEV_PRO  15  	/* Chip midi device == /dev/pro_midi */

/*
 *  Add other midis here...
		.
		.
		.
		.
 */

#define DSP_DEFAULT_SPEED	8000

#define ON		1
#define OFF		0

#define MAX_DSP_DEV	4
#define MAX_MIXER_DEV	2
#define MAX_SYNTH_DEV	3
#define MAX_MIDI_DEV	4

struct fileinfo {
       	  int mode;	/* Open mode */
       };

struct address_info {
	int io_base;
	int irq;
	int dma;
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

#include "os.h"
#include "sound_calls.h"
#include "dev_table.h"

#ifndef DEB
#define DEB(x)
#endif

#endif
