#ifndef SOUNDCARD_H
#define SOUNDCARD_H
/*
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * 
 */

 /* 
  * If you make modifications to this file, please contact me before
  * distributing the modified version. There is already enough 
  * divercity in the world.
  *
  * Regards,
  * Hannu Savolainen
  * hsavolai@cs.helsinki.fi
  */

#define SOUND_VERSION	200

#include <sys/ioctl.h>

/*
 *	Supported card ID numbers (Should be somewhere else?)
 */

#define SNDCARD_ADLIB	1
#define SNDCARD_SB	2
#define SNDCARD_PAS	3
#define SNDCARD_GUS	4
#define SNDCARD_MPU401	5

/***********************************
 * IOCTL Commands for /dev/sequencer
 */

#ifndef _IOWR
/*	@(#)ioctlp.h */

/* Ioctl's have the command encoded in the lower word,
 * and the size of any in or out parameters in the upper
 * word.  The high 2 bits of the upper word are used
 * to encode the in/out status of the parameter; for now
 * we restrict parameters to at most 128 bytes.
 */
/* #define	IOCTYPE		(0xff<<8) */
#define	IOCPARM_MASK	0x7f		/* parameters must be < 128 bytes */
#define	IOC_VOID	0x20000000	/* no parameters */
#define	IOC_OUT		0x40000000	/* copy out parameters */
#define	IOC_IN		0x80000000	/* copy in parameters */
#define	IOC_INOUT	(IOC_IN|IOC_OUT)
/* the 0x20000000 is so we can distinguish new ioctl's from old */
#define	_IO(x,y)	((int)(IOC_VOID|(x<<8)|y))
#define	_IOR(x,y,t)	((int)(IOC_OUT|((sizeof(t)&IOCPARM_MASK)<<16)|(x<<8)|y))
#define	_IOW(x,y,t)	((int)(IOC_IN|((sizeof(t)&IOCPARM_MASK)<<16)|(x<<8)|y))
/* this should be _IORW, but stdio got there first */
#define	_IOWR(x,y,t)	((int)(IOC_INOUT|((sizeof(t)&IOCPARM_MASK)<<16)|(x<<8)|y))
#endif  /* !_IOWR */

#define SNDCTL_SEQ_RESET		_IO  ('Q', 0)
#define SNDCTL_SEQ_SYNC			_IO  ('Q', 1)
#define SNDCTL_SYNTH_INFO		_IOWR('Q', 2, struct synth_info)
#define SNDCTL_SEQ_CTRLRATE		_IOWR('Q', 3, int)	/* Set/get timer resolution (HZ) */
#define SNDCTL_SEQ_GETOUTCOUNT		_IOR ('Q', 4, int)
#define SNDCTL_SEQ_GETINCOUNT		_IOR ('Q', 5, int)
#define SNDCTL_SEQ_PERCMODE		_IOW ('Q', 6, int)
#define SNDCTL_FM_LOAD_INSTR		_IOW ('Q', 7, struct sbi_instrument)	/* Valid for FM only */
#define SNDCTL_SEQ_TESTMIDI		_IOW ('Q', 8, int)
#define SNDCTL_SEQ_RESETSAMPLES		_IOW ('Q', 9, int)
#define SNDCTL_SEQ_NRSYNTHS		_IOR ('Q',10, int)
#define SNDCTL_SEQ_NRMIDIS		_IOR ('Q',11, int)
#define SNDCTL_MIDI_INFO		_IOWR('Q',12, struct midi_info)
#define SNDCTL_SEQ_TRESHOLD		_IOW ('Q',13, int)
#define SNDCTL_SYNTH_MEMAVL		_IOWR('Q',14, int)	/* in=dev#, out=memsize */
#define SNDCTL_FM_4OP_ENABLE		_IOW ('Q',15, int)	/* in=dev# */
#define SNDCTL_PMGR_ACCESS		_IOWR('Q',16, struct patmgr_info)

/*
 *	Sample loading mechanism for internal synthesizers (/dev/sequencer)
 *	The following patch_info structure has been designed to support
 *	Gravis UltraSound. It tries to be universal format for uploading
 *	sample based patches but is propably too limited.
 */

struct patch_info {
		short key;		/* Use GUS_PATCH here */
#define GUS_PATCH	0x04fd
#define OBSOLETE_GUS_PATCH	0x02fd
		short device_no;	/* Synthesizer number */
		short instr_no;		/* Midi pgm# */

		unsigned long mode;
/*
 * The least significant byte has the same format than the GUS .PAT
 * files
 */
#define WAVE_16_BITS	0x01	/* bit 0 = 8 or 16 bit wave data. */
#define WAVE_UNSIGNED	0x02	/* bit 1 = Signed - Unsigned data. */
#define WAVE_LOOPING	0x04	/* bit 2 = looping enabled-1. */
#define WAVE_BIDIR_LOOP	0x08	/* bit 3 = Set is bidirectional looping. */
#define WAVE_LOOP_BACK	0x10	/* bit 4 = Set is looping backward. */
#define WAVE_SUSTAIN_ON	0x20	/* bit 5 = Turn sustaining on. (Env. pts. 3)*/
#define WAVE_ENVELOPES	0x40	/* bit 6 = Enable envelopes - 1 */
				/* 	(use the env_rate/env_offs fields). */
/* Linux specific bits */
#define WAVE_VIBRATO	0x00010000	/* The vibrato info is valid */
#define WAVE_TREMOLO	0x00020000	/* The tremolo info is valid */
#define WAVE_SCALE	0x00040000	/* The scaling info is valid */
/* Other bits must be zeroed */

		long len;	/* Size of the wave data in bytes */
		long loop_start, loop_end; /* Byte offsets from the beginning */

/* 
 * The base_freq and base_note fields are used when computing the
 * playback speed for a note. The base_note defines the tone frequency
 * which is heard if the sample is played using the base_freq as the
 * playback speed.
 *
 * The low_note and high_note fields define the minimum and maximum note
 * frequencies for which this sample is valid. It is possible to define
 * more than one samples for a instrument number at the same time. The
 * low_note and high_note fields are used to select the most suitable one.
 *
 * The fields base_note, high_note and low_note should contain
 * the note frequency multiplied by 1000. For example value for the
 * middle A is 440*1000.
 */

		unsigned int base_freq;
		unsigned long base_note;
		unsigned long high_note;
		unsigned long low_note;
		int panning;	/* -128=left, 127=right */
		int detuning;

/*	New fields introduced in version 1.99.5	*/

       /* Envelope. Enabled by mode bit WAVE_ENVELOPES	*/
		unsigned char	env_rate[ 6 ];	 /* GUS HW ramping rate */
		unsigned char	env_offset[ 6 ]; /* 255 == 100% */

	/* 
	 * The tremolo, vibrato and scale info are not supported yet.
	 * Enable by setting the mode bits WAVE_TREMOLO, WAVE_VIBRATO or
	 * WAVE_SCALE
	 */

		unsigned char	tremolo_sweep;
		unsigned char	tremolo_rate;
		unsigned char	tremolo_depth;
	
		unsigned char	vibrato_sweep;
		unsigned char	vibrato_rate;
		unsigned char	vibrato_depth;

		int		scale_frequency;
		unsigned int	scale_factor;		/* from 0 to 2048 or 0 to 2 */
	
	        int		volume;
	        int		spare[4];
		char data[0];	/* The waveform data starts here */
	};


/*
 * Patch management interface (/dev/sequencer, /dev/patmgr#)
 * Don't use these calls if you want to maintain compatibility with
 * the future versions of the driver.
 */

#define 	PS_NO_PATCHES		0	/* No patch support on device */
#define		PS_MGR_NOT_OK		1	/* Plain patch support (no mgr) */
#define		PS_MGR_OK		2	/* Patch manager supported */
#define		PS_MANAGED		3	/* Patch manager running */

#define SNDCTL_PMGR_IFACE		_IOWR('P', 1, struct patmgr_info)

/*
 * The patmgr_info is a fixed size structure which is used for two
 * different purposes. The intended use is for communication between
 * the application using /dev/sequencer and the patch manager daemon
 * associated with a synthesizer device (ioctl(SNDCTL_PMGR_ACCESS)).
 *
 * This structure is also used with ioctl(SNDCTL_PGMR_IFACE) which allows
 * a patch manager daemon to read and write device parameters. This
 * ioctl available through /dev/sequencer also. Avoid using it since it's
 * extremely hardware dependent. In addition access trough /dev/sequencer 
 * may confuse the patch manager daemon.
 */

struct patmgr_info {	/* Note! size must be < 4k since kmalloc() is used */
	  unsigned long key;	/* Don't worry. Reserved for communication
	  			   between the patch manager and the driver. */
#define PM_K_EVENT		1 /* Event from the /dev/sequencer driver */
#define PM_K_COMMAND		2 /* Request from a application */
#define PM_K_RESPONSE		3 /* From patmgr to application */
#define PM_ERROR		4 /* Error returned by the patmgr */
	  int device;
	  int command;

/* 
 * Commands 0x000 to 0xfff reserved for patch manager programs 
 */
#define PM_GET_DEVTYPE	1	/* Returns type of the patch mgr interface of dev */
#define		PMTYPE_FM2	1	/* 2 OP fm */
#define		PMTYPE_FM4	2	/* Mixed 4 or 2 op FM (OPL-3) */
#define		PMTYPE_WAVE	3	/* Wave table synthesizer (GUS) */
#define PM_GET_NRPGM	2	/* Returns max # of midi programs in parm1 */
#define PM_GET_PGMMAP	3	/* Returns map of loaded midi programs in data8 */
#define PM_GET_PGM_PATCHES 4	/* Return list of patches of a program (parm1) */
#define PM_GET_PATCH	5	/* Return patch header of patch parm1 */
#define PM_SET_PATCH	6	/* Set patch header of patch parm1 */
#define PM_READ_PATCH	7	/* Read patch (wave) data */
#define PM_WRITE_PATCH	8	/* Write patch (wave) data */

/*
 * Commands 0x1000 to 0xffff are for communication between the patch manager
 * and the client
 */
#define _PM_LOAD_PATCH	0x100

/* 
 * Commands above 0xffff reserved for device specific use
 */

	  long parm1;
	  long parm2;
	  long parm3;

	  union {
		unsigned char data8[4000];
		unsigned short data16[2000];
		unsigned long data32[1000];
		struct patch_info patch;
	  } data;
	};

/*
 * When a patch manager daemon is present, it will be informed by the
 * driver when something important happens. For example when the
 * /dev/sequencer is opened or closed. A record with key == PM_K_EVENT is
 * returned. The command field contains the event type:
 */
#define PM_E_OPENED		1	/* /dev/sequencer opened */
#define PM_E_CLOSED		2	/* /dev/sequencer closed */
#define PM_E_PATCH_RESET	3	/* SNDCTL_RESETSAMPLES called */
#define PM_E_PATCH_LOADED	4	/* A patch has been loaded by appl */

/*
 * /dev/sequencer input events.
 *
 * The data written to the /dev/sequencer is a stream of events. Events
 * are records of 4 or 8 bytes. The first byte defines the size. 
 * Any number of events can be written with a write call. There
 * is a set of macros for sending these events. Use these macros if you
 * want to maximize portability of your program.
 *
 * Events SEQ_WAIT, SEQ_MIDIPUTC and SEQ_ECHO. Are also input events.
 * (All input events are currently 4 bytes long. Be prepared to support
 * 8 byte events also. If you receive any event having first byte >= 0xf0,
 * it's a 8 byte event.
 *
 * The events are documented at the end of this file.
 *
 * Normal events (4 bytes)
 * There is also a 8 byte version of most of the 4 byte events. The
 * 8 byte one is recommended.
 */
#define SEQ_NOTEOFF		0
#define SEQ_FMNOTEOFF		SEQ_NOTEOFF	/* Just old name */
#define SEQ_NOTEON		1
#define	SEQ_FMNOTEON		SEQ_NOTEON
#define SEQ_WAIT		2
#define SEQ_PGMCHANGE		3
#define SEQ_FMPGMCHANGE		SEQ_PGMCHANGE
#define SEQ_SYNCTIMER		4
#define SEQ_MIDIPUTC		5
#define SEQ_DRUMON		6	/*** OBSOLETE ***/
#define SEQ_DRUMOFF		7	/*** OBSOLETE ***/
#define SEQ_ECHO		8	/* For synching programs with output */
#define SEQ_AFTERTOUCH		9
#define SEQ_CONTROLLER		10
#define    CTRL_PITCH_BENDER		255
#define    CTRL_PITCH_BENDER_RANGE	254
#define    CTRL_EXPRESSION		253
#define    CTRL_MAIN_VOLUME		252
#define SEQ_BALANCE		11

/*
 * Note! SEQ_WAIT, SEQ_MIDIPUTC and SEQ_ECHO are used also as
 *	 input events.
 */

/*
 * Event codes 0xf0 to 0xfc are reserved for future extensions.
 */

#define SEQ_FULLSIZE		0xfd	/* Long events */
/*
 *	SEQ_FULLSIZE events are used for loading patches/samples to the
 *	synthesizer devices. These events are passed directly to the driver
 *	of the associated synthesizer device. There is no limit to the size
 *	of the extended events. These events are not queued but executed
 *	immediately when the write() is called (execution can take several
 *	seconds of time). 
 *
 *	When a SEQ_FULLSIZE message is written to the device, it must
 *	be written using exactly one write() call. Other events cannot
 *	be mixed to the same write.
 *	
 *	For FM synths (YM3812/OPL3) use struct sbi_instrument and write it to the 
 *	/dev/sequencer. Don't write other data together with the instrument structure
 *	Set the key field of the structure to FM_PATCH. The device field is used to
 *	route the patch to the corresponding device.
 *
 *	For Gravis UltraSound use struct patch_info. Initialize the key field
 *      to GUS_PATCH.
 */
#define SEQ_PRIVATE		0xfe	/* Low level HW dependent events (8 bytes) */
#define SEQ_EXTENDED		0xff	/* Extended events (8 bytes) */

/*
 *	Extended events for synthesizers (8 bytes)
 *
 *	Format:
 *
 *		b0	= SEQ_EXTENDED
 *		b1	= command
 *		b2	= device
 *		b3-b7	= parameters
 *
 *	Command				b3	b4	b5	b6	b7
 *	----------------------------------------------------------------------------
 *	SEQ_NOTEON			voice	note	volume	0	0
 *	SEQ_NOTEOFF			voice	note	volume	0	0
 *	SEQ_PGMCHANGE			voice	pgm	0	0	0
 *	SEQ_DRUMON			(voice)	drum#	volume	0	0
 *	SEQ_DRUMOFF			(voice)	drum#	volume	0	0
 */

/*
 * Record for FM patches
 */

typedef unsigned char sbi_instr_data[32];

struct sbi_instrument {
		unsigned short	key;		/* 	Initialize to FM_PATCH or OPL3_PATCH */
#define FM_PATCH	0x01fd
#define OPL3_PATCH	0x03fd
		short		device;		/*	Synth# (0-4)	*/
		int 		channel;	/*	Program# to be initialized 	*/
		sbi_instr_data	operators;	/*	Register settings for operator cells (.SBI format)	*/
	};

struct synth_info {	/* Read only */
		char	name[30];
		int	device;		/* 0-N. INITIALIZE BEFORE CALLING */
		int	synth_type;
#define SYNTH_TYPE_FM			0
#define SYNTH_TYPE_SAMPLE		1

		int	synth_subtype;
#define FM_TYPE_ADLIB			0x00
#define FM_TYPE_OPL3			0x01

#define SAMPLE_TYPE_GUS			0x10

		int	perc_mode;	/* No longer supported */
		int	nr_voices;
		int	nr_drums;	/* Obsolete field */
		int	instr_bank_size;
		unsigned long	capabilities;	
#define SYNTH_CAP_PERCMODE		0x00000001 /* No longer used */
#define SYNTH_CAP_OPL3			0x00000002 /* Set if OPL3 supported */
		int	dummies[19];	/* Reserve space */
	};

struct midi_info {
		char		name[30];
		int		device;		/* 0-N. INITIALIZE BEFORE CALLING */
		unsigned long	capabilities;	/* To be defined later */
		int		dummies[19];	/* Reserve space */
	};

/********************************************
 * IOCTL commands for /dev/dsp and /dev/audio
 */

#define SNDCTL_DSP_RESET		_IO  ('P', 0)
#define SNDCTL_DSP_SYNC			_IO  ('P', 1)
#define SNDCTL_DSP_SPEED		_IOWR('P', 2, int)
#define SNDCTL_DSP_STEREO		_IOWR('P', 3, int)
#define SNDCTL_DSP_GETBLKSIZE		_IOWR('P', 4, int)
#define SNDCTL_DSP_SAMPLESIZE		_IOWR('P', 5, int)	/* 8, 12 or 16 */
#define SOUND_PCM_WRITE_CHANNELS	_IOWR('P', 6, int)
#define SOUND_PCM_WRITE_FILTER		_IOWR('P', 7, int)
#define SNDCTL_DSP_POST			_IO  ('P', 8)

#define SOUND_PCM_READ_RATE		_IOR ('P', 2, int)
#define SOUND_PCM_READ_CHANNELS		_IOR ('P', 6, int)
#define SOUND_PCM_READ_BITS		_IOR ('P', 5, int)
#define SOUND_PCM_READ_FILTER		_IOR ('P', 7, int)

/* Some alias names */
#define SOUND_PCM_WRITE_BITS		SNDCTL_DSP_SAMPLESIZE
#define SOUND_PCM_WRITE_RATE		SNDCTL_DSP_SPEED
#define SOUND_PCM_POST			SNDCTL_DSP_POST
#define SOUND_PCM_RESET			SNDCTL_DSP_RESET
#define SOUND_PCM_SYNC			SNDCTL_DSP_SYNC

/*********************************************
 * IOCTL commands for /dev/mixer
 */
	
/* 
 * Mixer devices
 *
 * There can be up to 20 different analog mixer channels. The
 * SOUND_MIXER_NRDEVICES gives the currently supported maximum. 
 * The SOUND_MIXER_READ_DEVMASK returns a bitmask which tells
 * the devices supported by the particular mixer.
 */

#define SOUND_MIXER_NRDEVICES	12
#define SOUND_MIXER_VOLUME	0
#define SOUND_MIXER_BASS	1
#define SOUND_MIXER_TREBLE	2
#define SOUND_MIXER_SYNTH	3
#define SOUND_MIXER_PCM		4
#define SOUND_MIXER_SPEAKER	5
#define SOUND_MIXER_LINE	6
#define SOUND_MIXER_MIC		7
#define SOUND_MIXER_CD		8
#define SOUND_MIXER_IMIX	9	/*  Recording monitor  */
#define SOUND_MIXER_ALTPCM	10
#define SOUND_MIXER_RECLEV	11	/* Recording level */

/* Some on/off settings (SOUND_SPECIAL_MIN - SOUND_SPECIAL_MAX) */
/* Not counted to SOUND_MIXER_NRDEVICES, but use the same number space */
#define SOUND_ONOFF_MIN		28
#define SOUND_ONOFF_MAX		30
#define SOUND_MIXER_MUTE	28	/* 0 or 1 */
#define SOUND_MIXER_ENHANCE	29	/* Enhanced stereo (0, 40, 60 or 80) */
#define SOUND_MIXER_LOUD	30	/* 0 or 1 */

/* Note!	Number 31 cannot be used since the sign bit is reserved */

#define SOUND_DEVICE_LABELS	{"Vol  ", "Bass ", "Trebl", "Synth", "Pcm  ", "Spkr ", "Line ", \
				 "Mic  ", "CD   ", "Mix  ", "Pcm2 ", "rec"}

#define SOUND_DEVICE_NAMES	{"vol", "bass", "treble", "synth", "pcm", "speaker", "line", \
				 "mic", "cd", "mix", "pcm2", "rec"}

/*	Device bitmask identifiers	*/

#define SOUND_MIXER_RECSRC	0xff	/* Arg contains a bit for each recording source */
#define SOUND_MIXER_DEVMASK	0xfe	/* Arg contains a bit for each supported device */
#define SOUND_MIXER_RECMASK	0xfd	/* Arg contains a bit for each supported recording source */
#define SOUND_MIXER_CAPS	0xfc
	#define SOUND_CAP_EXCL_INPUT	0x00000001	/* Only one recording source at a time */
#define SOUND_MIXER_STEREODEVS	0xfb	/* Mixer channels supporting stereo */

/*	Device mask bits	*/

#define SOUND_MASK_VOLUME	(1 << SOUND_MIXER_VOLUME)
#define SOUND_MASK_BASS		(1 << SOUND_MIXER_BASS)
#define SOUND_MASK_TREBLE	(1 << SOUND_MIXER_TREBLE)
#define SOUND_MASK_SYNTH	(1 << SOUND_MIXER_SYNTH)
#define SOUND_MASK_PCM		(1 << SOUND_MIXER_PCM)
#define SOUND_MASK_SPEAKER	(1 << SOUND_MIXER_SPEAKER)
#define SOUND_MASK_LINE		(1 << SOUND_MIXER_LINE)
#define SOUND_MASK_MIC		(1 << SOUND_MIXER_MIC)
#define SOUND_MASK_CD		(1 << SOUND_MIXER_CD)
#define SOUND_MASK_IMIX		(1 << SOUND_MIXER_IMIX)
#define SOUND_MASK_ALTPCM	(1 << SOUND_MIXER_ALTPCM)
#define SOUND_MASK_RECLEV	(1 << SOUND_MIXER_RECLEV)

#define SOUND_MASK_MUTE		(1 << SOUND_MIXER_MUTE)
#define SOUND_MASK_ENHANCE	(1 << SOUND_MIXER_ENHANCE)
#define SOUND_MASK_LOUD		(1 << SOUND_MIXER_LOUD)

#define MIXER_READ(dev)		_IOR('M', dev, int)
#define SOUND_MIXER_READ_VOLUME		MIXER_READ(SOUND_MIXER_VOLUME)
#define SOUND_MIXER_READ_BASS		MIXER_READ(SOUND_MIXER_BASS)
#define SOUND_MIXER_READ_TREBLE		MIXER_READ(SOUND_MIXER_TREBLE)
#define SOUND_MIXER_READ_SYNTH		MIXER_READ(SOUND_MIXER_SYNTH)
#define SOUND_MIXER_READ_PCM		MIXER_READ(SOUND_MIXER_PCM)
#define SOUND_MIXER_READ_SPEAKER	MIXER_READ(SOUND_MIXER_SPEAKER)
#define SOUND_MIXER_READ_LINE		MIXER_READ(SOUND_MIXER_LINE)
#define SOUND_MIXER_READ_MIC		MIXER_READ(SOUND_MIXER_MIC)
#define SOUND_MIXER_READ_CD		MIXER_READ(SOUND_MIXER_CD)
#define SOUND_MIXER_READ_IMIX		MIXER_READ(SOUND_MIXER_IMIX)
#define SOUND_MIXER_READ_ALTPCM		MIXER_READ(SOUND_MIXER_ALTPCM)
#define SOUND_MIXER_READ_RECLEV		MIXER_READ(SOUND_MIXER_RECLEV)
#define SOUND_MIXER_READ_MUTE		MIXER_READ(SOUND_MIXER_MUTE)
#define SOUND_MIXER_READ_ENHANCE	MIXER_READ(SOUND_MIXER_ENHANCE)
#define SOUND_MIXER_READ_LOUD		MIXER_READ(SOUND_MIXER_LOUD)

#define SOUND_MIXER_READ_RECSRC		MIXER_READ(SOUND_MIXER_RECSRC)
#define SOUND_MIXER_READ_DEVMASK	MIXER_READ(SOUND_MIXER_DEVMASK)
#define SOUND_MIXER_READ_RECMASK	MIXER_READ(SOUND_MIXER_RECMASK)
#define SOUND_MIXER_READ_STEREODEVS	MIXER_READ(SOUND_MIXER_STEREODEVS)
#define SOUND_MIXER_READ_CAPS		MIXER_READ(SOUND_MIXER_CAPS)

#define MIXER_WRITE(dev)		_IOWR('M', dev, int)
#define SOUND_MIXER_WRITE_VOLUME	MIXER_WRITE(SOUND_MIXER_VOLUME)
#define SOUND_MIXER_WRITE_BASS		MIXER_WRITE(SOUND_MIXER_BASS)
#define SOUND_MIXER_WRITE_TREBLE	MIXER_WRITE(SOUND_MIXER_TREBLE)
#define SOUND_MIXER_WRITE_SYNTH		MIXER_WRITE(SOUND_MIXER_SYNTH)
#define SOUND_MIXER_WRITE_PCM		MIXER_WRITE(SOUND_MIXER_PCM)
#define SOUND_MIXER_WRITE_SPEAKER	MIXER_WRITE(SOUND_MIXER_SPEAKER)
#define SOUND_MIXER_WRITE_LINE		MIXER_WRITE(SOUND_MIXER_LINE)
#define SOUND_MIXER_WRITE_MIC		MIXER_WRITE(SOUND_MIXER_MIC)
#define SOUND_MIXER_WRITE_CD		MIXER_WRITE(SOUND_MIXER_CD)
#define SOUND_MIXER_WRITE_IMIX		MIXER_WRITE(SOUND_MIXER_IMIX)
#define SOUND_MIXER_WRITE_ALTPCM	MIXER_WRITE(SOUND_MIXER_ALTPCM)
#define SOUND_MIXER_WRITE_RECLEV	MIXER_WRITE(SOUND_MIXER_RECLEV)
#define SOUND_MIXER_WRITE_MUTE		MIXER_WRITE(SOUND_MIXER_MUTE)
#define SOUND_MIXER_WRITE_ENHANCE	MIXER_WRITE(SOUND_MIXER_ENHANCE)
#define SOUND_MIXER_WRITE_LOUD		MIXER_WRITE(SOUND_MIXER_LOUD)

#define SOUND_MIXER_WRITE_RECSRC	MIXER_WRITE(SOUND_MIXER_RECSRC)

/*
 *	The following mixer ioctl calls are compatible with the BSD driver by
 *	  Steve Haehnichen <shaehnic@ucsd.edu>
 *
 * Since this interface is entirely SB specific, it will be dropped in the
 * near future.
 */

typedef unsigned char S_BYTE;
typedef unsigned char S_FLAG;
struct stereo_vol
{
  S_BYTE l;			/* Left volume */
  S_BYTE r;			/* Right volume */
};

#define MIXER_IOCTL_SET_LEVELS 		_IOW ('s', 20, struct sb_mixer_levels)
#define MIXER_IOCTL_SET_PARAMS 		_IOW ('s', 21, struct sb_mixer_params)
#define MIXER_IOCTL_READ_LEVELS 	_IOR ('s', 22, struct sb_mixer_levels)
#define MIXER_IOCTL_READ_PARAMS 	_IOR ('s', 23, struct sb_mixer_params)
#define MIXER_IOCTL_RESET		_IO  ('s', 24)

/*
 * Mixer volume levels for MIXER_IOCTL_SET_VOL & MIXER_IOCTL_READ_VOL
 */
struct sb_mixer_levels
{
  struct stereo_vol master;	/* Master volume */
  struct stereo_vol voc;	/* DSP Voice volume */
  struct stereo_vol fm;		/* FM volume */
  struct stereo_vol line;	/* Line-in volume */
  struct stereo_vol cd;		/* CD audio */
  S_BYTE mic;			/* Microphone level */
};

/*
 * Mixer parameters for MIXER_IOCTL_SET_PARAMS & MIXER_IOCTL_READ_PARAMS
 */
struct sb_mixer_params
{
  S_BYTE record_source;		/* Recording source (See SRC_xxx below) */
  S_FLAG hifreq_filter;		/* Filter frequency (hi/low) */
  S_FLAG filter_input;		/* ANFI input filter */
  S_FLAG filter_output;		/* DNFI output filter */
  S_FLAG dsp_stereo;		/* 1 if DSP is in Stereo mode */
};

#define SRC_MIC         1	/* Select Microphone recording source */
#define SRC_CD          3	/* Select CD recording source */
#define SRC_LINE        7	/* Use Line-in for recording source */

#if !defined(KERNEL) && !defined(INKERNEL)
/*
 *	Some convenience macros to simplify programming of the
 *	/dev/sequencer interface
 *
 *	These macros define the API which should be used when possible.
 */

void seqbuf_dump();	/* This function must be provided by programs */

/* Sample seqbuf_dump() implementation:
 *
 *	SEQ_DEFINEBUF (2048);	-- Defines a buffer for 2048 bytes
 *
 *	int seqfd;		-- The file descriptor for /dev/sequencer.
 *
 *	void
 *	seqbuf_dump ()
 *	{
 *	  if (_seqbufptr)
 *	    if (write (seqfd, _seqbuf, _seqbufptr) == -1)
 *	      {
 *		perror ("write /dev/sequencer");
 *		exit (-1);
 *	      }
 *	  _seqbufptr = 0;
 *	}
 */

#define SEQ_DEFINEBUF(len)		unsigned char _seqbuf[len]; int _seqbuflen = len, _seqbufptr = 0
#define SEQ_PM_DEFINES			struct patmgr_info _pm_info
#define _SEQ_NEEDBUF(len)		if ((_seqbufptr+(len)) > _seqbuflen) seqbuf_dump()
#define _SEQ_ADVBUF(len)		_seqbufptr += len
#define SEQ_DUMPBUF			seqbuf_dump
#define PM_LOAD_PATCH(dev, bank, pgm)	(SEQ_DUMPBUF(), _pm_info.command = _PM_LOAD_PATCH, \
					_pm_info.device=dev, _pm_info.data.data8[0]=pgm, \
					_pm_info.parm1 = bank, _pm_info.parm2 = 1, \
					ioctl(seqfd, SNDCTL_PMGR_ACCESS, &_pm_info))
#define PM_LOAD_PATCHES(dev, bank, pgm) (SEQ_DUMPBUF(), _pm_info.command = _PM_LOAD_PATCH, \
					_pm_info.device=dev, memcpy(_pm_info.data.data8, pgm, 128), \
					_pm_info.parm1 = bank, _pm_info.parm2 = 128, \
					ioctl(seqfd, SNDCTL_PMGR_ACCESS, &_pm_info))

#define SEQ_START_NOTE(dev, voice, note, vol)	{_SEQ_NEEDBUF(8);\
					_seqbuf[_seqbufptr] = SEQ_EXTENDED;\
					_seqbuf[_seqbufptr+1] = SEQ_NOTEON;\
					_seqbuf[_seqbufptr+2] = (dev);\
					_seqbuf[_seqbufptr+3] = (voice);\
					_seqbuf[_seqbufptr+4] = (note);\
					_seqbuf[_seqbufptr+5] = (vol);\
					_seqbuf[_seqbufptr+6] = 0;\
					_seqbuf[_seqbufptr+7] = 0;\
					_SEQ_ADVBUF(8);}

#define SEQ_STOP_NOTE(dev, voice, note, vol)	{_SEQ_NEEDBUF(8);\
					_seqbuf[_seqbufptr] = SEQ_EXTENDED;\
					_seqbuf[_seqbufptr+1] = SEQ_NOTEOFF;\
					_seqbuf[_seqbufptr+2] = (dev);\
					_seqbuf[_seqbufptr+3] = (voice);\
					_seqbuf[_seqbufptr+4] = (note);\
					_seqbuf[_seqbufptr+5] = (vol);\
					_seqbuf[_seqbufptr+6] = 0;\
					_seqbuf[_seqbufptr+7] = 0;\
					_SEQ_ADVBUF(8);}

#define SEQ_CHN_PRESSURE(dev, voice, pressure)	{_SEQ_NEEDBUF(8);\
					_seqbuf[_seqbufptr] = SEQ_EXTENDED;\
					_seqbuf[_seqbufptr+1] = SEQ_AFTERTOUCH;\
					_seqbuf[_seqbufptr+2] = (dev);\
					_seqbuf[_seqbufptr+3] = (voice);\
					_seqbuf[_seqbufptr+4] = (pressure);\
					_seqbuf[_seqbufptr+5] = 0;\
					_seqbuf[_seqbufptr+6] = 0;\
					_seqbuf[_seqbufptr+7] = 0;\
					_SEQ_ADVBUF(8);}

#define SEQ_PANNING(dev, voice, pos)	{_SEQ_NEEDBUF(8);\
					_seqbuf[_seqbufptr] = SEQ_EXTENDED;\
					_seqbuf[_seqbufptr+1] = SEQ_BALANCE;\
					_seqbuf[_seqbufptr+2] = (dev);\
					_seqbuf[_seqbufptr+3] = (voice);\
					(char)_seqbuf[_seqbufptr+4] = (pos);\
					_seqbuf[_seqbufptr+5] = 0;\
					_seqbuf[_seqbufptr+6] = 0;\
					_seqbuf[_seqbufptr+7] = 0;\
					_SEQ_ADVBUF(8);}

#define SEQ_CONTROL(dev, voice, controller, value)	{_SEQ_NEEDBUF(8);\
					_seqbuf[_seqbufptr] = SEQ_EXTENDED;\
					_seqbuf[_seqbufptr+1] = SEQ_CONTROLLER;\
					_seqbuf[_seqbufptr+2] = (dev);\
					_seqbuf[_seqbufptr+3] = (voice);\
					_seqbuf[_seqbufptr+4] = (controller);\
					*(short *)&_seqbuf[_seqbufptr+5] = (value);\
					_seqbuf[_seqbufptr+7] = 0;\
					_SEQ_ADVBUF(8);}

#define SEQ_PITCHBEND(dev, voice, value) SEQ_CONTROL(dev, voice, CTRL_PITCH_BENDER, value)
#define SEQ_BENDER_RANGE(dev, voice, value) SEQ_CONTROL(dev, voice, CTRL_PITCH_BENDER_RANGE, value)
#define SEQ_EXPRESSION(dev, voice, value) SEQ_CONTROL(dev, voice, CTRL_EXPRESSION, value)
#define SEQ_MAIN_VOLUME(dev, voice, value) SEQ_CONTROL(dev, voice, CTRL_MAIN_VOLUME, value)

#define SEQ_START_TIMER()		{_SEQ_NEEDBUF(4);\
					_seqbuf[_seqbufptr] = SEQ_SYNCTIMER;\
					_seqbuf[_seqbufptr+1] = 0;\
					_seqbuf[_seqbufptr+2] = 0;\
					_seqbuf[_seqbufptr+3] = 0;\
					_SEQ_ADVBUF(4);}
#define SEQ_SET_PATCH(dev, voice, patch)	{_SEQ_NEEDBUF(8);\
					_seqbuf[_seqbufptr] = SEQ_EXTENDED;\
					_seqbuf[_seqbufptr+1] = SEQ_PGMCHANGE;\
					_seqbuf[_seqbufptr+2] = (dev);\
					_seqbuf[_seqbufptr+3] = (voice);\
					_seqbuf[_seqbufptr+4] = (patch);\
					_seqbuf[_seqbufptr+5] = 0;\
					_seqbuf[_seqbufptr+6] = 0;\
					_seqbuf[_seqbufptr+7] = 0;\
					_SEQ_ADVBUF(8);}

#define SEQ_WAIT_TIME(ticks)		{_SEQ_NEEDBUF(4);\
				 	*(unsigned long *)&_seqbuf[_seqbufptr] = SEQ_WAIT | ((ticks) << 8);\
				 	_SEQ_ADVBUF(4);}

#define SEQ_ECHO_BACK(key)		{_SEQ_NEEDBUF(4);\
				 	*(unsigned long *)&_seqbuf[_seqbufptr] = SEQ_ECHO | ((key) << 8);\
				 	_SEQ_ADVBUF(4);}

#define SEQ_MIDIOUT(device, byte)	{_SEQ_NEEDBUF(4);\
					_seqbuf[_seqbufptr] = SEQ_MIDIPUTC;\
					_seqbuf[_seqbufptr+1] = (byte);\
					_seqbuf[_seqbufptr+2] = (device);\
					_seqbuf[_seqbufptr+3] = 0;\
					_SEQ_ADVBUF(4);}
#define SEQ_WRPATCH(patch, len)		{if (_seqbufptr) seqbuf_dump();\
					if (write(seqfd, (char*)(patch), len)==-1) \
					   perror("Write patch: /dev/sequencer");}

#endif
long soundcard_init(long mem_start);
#endif
