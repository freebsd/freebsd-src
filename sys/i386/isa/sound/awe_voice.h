/*
 * sound/awe_voice.h
 *
 * Voice information definitions for the low level driver for the 
 * AWE32/Sound Blaster 32 wave table synth.
 *   version 0.2.0a; Oct. 30, 1996
 *
 * (C) 1996 Takashi Iwai
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

#ifndef AWE_VOICE_H
#define AWE_VOICE_H

#ifndef SAMPLE_TYPE_AWE32
#define SAMPLE_TYPE_AWE32	0x20
#endif

#ifndef _PATCHKEY
#define _PATCHKEY(id) ((id<<8)|0xfd)
#endif

/*----------------------------------------------------------------
 * patch information record
 *----------------------------------------------------------------*/

/* patch interface header: 16 bytes */
typedef struct awe_patch_info {
	short key;			/* use AWE_PATCH here */
#define AWE_PATCH	_PATCHKEY(0x07)

	short device_no;		/* synthesizer number */
	unsigned short sf_id;		/* file id (should be zero) */
	short sf_version;		/* patch version (not referred) */
	long len;			/* data length (without this header) */

	short type;			/* following data type */
#define AWE_LOAD_INFO		0
#define AWE_LOAD_DATA		1

	short reserved;			/* word alignment data */
	char data[0];			/* patch data follows here */
} awe_patch_info;


/*----------------------------------------------------------------
 * raw voice information record
 *----------------------------------------------------------------*/

/* wave table envelope & effect parameters to control EMU8000 */
typedef struct _awe_voice_parm {
	unsigned short moddelay;	/* modulation delay (0x8000) */
	unsigned short modatkhld;	/* modulation attack & hold time (0x7f7f) */
	unsigned short moddcysus;	/* modulation decay & sustain (0x7f7f) */
	unsigned short modrelease;	/* modulation release time (0x807f) */
	short modkeyhold, modkeydecay;	/* envelope change per key (not used) */
	unsigned short voldelay;	/* volume delay (0x8000) */
	unsigned short volatkhld;	/* volume attack & hold time (0x7f7f) */
	unsigned short voldcysus;	/* volume decay & sustain (0x7f7f) */
	unsigned short volrelease;	/* volume release time (0x807f) */
	short volkeyhold, volkeydecay;	/* envelope change per key (not used) */
	unsigned short lfo1delay;	/* LFO1 delay (0x8000) */
	unsigned short lfo2delay;	/* LFO2 delay (0x8000) */
	unsigned short pefe;		/* modulation pitch & cutoff (0x0000) */
	unsigned short fmmod;		/* LFO1 pitch & cutoff (0x0000) */
	unsigned short tremfrq;		/* LFO1 volume & freq (0x0000) */
	unsigned short fm2frq2;		/* LFO2 pitch & freq (0x0000) */
	unsigned char cutoff;		/* initial cutoff (0xff) */
	unsigned char filterQ;		/* initial filter Q [0-15] (0x0) */
	unsigned char chorus;		/* chorus send (0x00) */
	unsigned char reverb;		/* reverb send (0x00) */
	unsigned short reserved[4];	/* not used */
} awe_voice_parm;

/* wave table parameters: 92 bytes */
typedef struct _awe_voice_info {
	unsigned short sf_id;		/* file id (should be zero) */
	unsigned short sample;		/* sample id */
	long start, end;		/* sample offset correction */
	long loopstart, loopend;	/* loop offset correction */
	short rate_offset;		/* sample rate pitch offset */
	unsigned short mode;		/* sample mode */
#define AWE_MODE_ROMSOUND		0x8000
#define AWE_MODE_STEREO			1
#define AWE_MODE_LOOPING		2
#define AWE_MODE_NORELEASE		4	/* obsolete */
#define AWE_MODE_INIT_PARM		8

	short root;			/* midi root key */
	short tune;			/* pitch tuning (in cents) */
	char low, high;			/* key note range */
	char vellow, velhigh;		/* velocity range */
	char fixkey, fixvel;		/* fixed key, velocity */
	char pan, fixpan;		/* panning, fixed panning */
	short exclusiveClass;		/* exclusive class (0 = none) */
	unsigned char amplitude;	/* sample volume (127 max) */
	unsigned char attenuation;	/* attenuation (0.375dB) */
	short scaleTuning;		/* pitch scale tuning(%), normally 100 */
	awe_voice_parm parm;		/* voice envelope parameters */
	short index;			/* internal index (set by driver) */
} awe_voice_info;

/* instrument info header: 4 bytes */
typedef struct _awe_voice_rec {
	unsigned char bank;		/* midi bank number */
	unsigned char instr;		/* midi preset number */
	short nvoices;			/* number of voices */
	awe_voice_info info[0];		/* voice information follows here */
} awe_voice_rec;


/*----------------------------------------------------------------
 * sample wave information
 *----------------------------------------------------------------*/

/* wave table sample header: 32 bytes */
typedef struct awe_sample_info {
	unsigned short sf_id;		/* file id (should be zero) */
	unsigned short sample;		/* sample id */
	long start, end;		/* start & end offset */
	long loopstart, loopend;	/* loop start & end offset */
	long size;			/* size (0 = ROM) */
	short checksum_flag;		/* use check sum = 1 */
	unsigned short mode_flags;	/* mode flags */
#define AWE_SAMPLE_8BITS	1	/* wave data is 8bits */
#define AWE_SAMPLE_UNSIGNED	2	/* wave data is unsigned */
#define AWE_SAMPLE_NO_BLANK	4	/* no blank loop is attached */
#define AWE_SAMPLE_SINGLESHOT	8	/* single-shot w/o loop */
#define AWE_SAMPLE_BIDIR_LOOP	16	/* bidirectional looping */
#define AWE_SAMPLE_STEREO_LEFT	32	/* stereo left sound */
#define AWE_SAMPLE_STEREO_RIGHT	64	/* stereo right sound */
	unsigned long checksum;		/* check sum */
	unsigned short data[0];		/* sample data follows here */
} awe_sample_info;


/*----------------------------------------------------------------
 * awe hardware controls
 *----------------------------------------------------------------*/

#define _AWE_DEBUG_MODE			0x00
#define _AWE_REVERB_MODE		0x01
#define _AWE_CHORUS_MODE		0x02
#define _AWE_REMOVE_LAST_SAMPLES	0x03
#define _AWE_INITIALIZE_CHIP		0x04
#define _AWE_SEND_EFFECT		0x05
#define _AWE_TERMINATE_CHANNEL		0x06
#define _AWE_TERMINATE_ALL		0x07
#define _AWE_INITIAL_VOLUME		0x08
#define _AWE_SET_GUS_BANK		0x09

#define _AWE_MODE_FLAG			0x80
#define _AWE_COOKED_FLAG		0x40	/* not supported */
#define _AWE_MODE_VALUE_MASK		0x3F

#define _AWE_CMD(chn, voice, cmd, p1, p2) \
{_SEQ_NEEDBUF(8); _seqbuf[_seqbufptr] = SEQ_PRIVATE;\
 _seqbuf[_seqbufptr+1] = chn;\
 _seqbuf[_seqbufptr+2] = _AWE_MODE_FLAG|(cmd);\
 _seqbuf[_seqbufptr+3] = voice;\
 *(unsigned short*)&_seqbuf[_seqbufptr+4] = p1;\
 *(unsigned short*)&_seqbuf[_seqbufptr+6] = p2;\
 _SEQ_ADVBUF(8);}

#define AWE_DEBUG_MODE(dev,p1)	_AWE_CMD(dev, 0, _AWE_DEBUG_MODE, p1, 0)
#define AWE_REVERB_MODE(dev,p1)	_AWE_CMD(dev, 0, _AWE_REVERB_MODE, p1, 0)
#define AWE_CHORUS_MODE(dev,p1)	_AWE_CMD(dev, 0, _AWE_CHORUS_MODE, p1, 0)
#define AWE_REMOVE_LAST_SAMPLES(dev) _AWE_CMD(dev, 0, _AWE_REMOVE_LAST_SAMPLES, 0, 0)
#define AWE_INITIALIZE_CHIP(dev) _AWE_CMD(dev, 0, _AWE_INITIALIZE_CHIP, 0, 0)
#define AWE_SEND_EFFECT(dev,voice,type,value) _AWE_CMD(dev,voice,_AWE_SEND_EFFECT,type,value)
#define AWE_TERMINATE_CHANNEL(dev,voice) _AWE_CMD(dev,voice,_AWE_TERMINATE_CHANNEL,0,0)
#define AWE_TERMINATE_ALL(dev) _AWE_CMD(dev, 0, _AWE_TERMINATE_ALL, 0, 0)
#define AWE_INITIAL_VOLUME(dev,atten) _AWE_CMD(dev, 0, _AWE_INITIAL_VOLUME, atten, 0)
#define AWE_SET_GUS_BANK(dev,bank) _AWE_CMD(dev, 0, _AWE_SET_GUS_BANK, bank, 0)

/* reverb mode */
#define	AWE_REVERB_ROOM1	0
#define AWE_REVERB_ROOM2	1
#define	AWE_REVERB_ROOM3	2
#define	AWE_REVERB_HALL1	3
#define	AWE_REVERB_HALL2	4
#define	AWE_REVERB_PLATE	5
#define	AWE_REVERB_DELAY	6
#define	AWE_REVERB_PANNINGDELAY 7

/* chorus mode */
#define AWE_CHORUS_1		0
#define	AWE_CHORUS_2		1
#define	AWE_CHORUS_3		2
#define	AWE_CHORUS_4		3
#define	AWE_CHORUS_FEEDBACK	4
#define	AWE_CHORUS_FLANGER	5
#define	AWE_CHORUS_SHORTDELAY	6
#define	AWE_CHORUS_SHORTDELAY2	7

/* effects */
enum {

/* modulation envelope parameters */
/* 0*/	AWE_FX_ENV1_DELAY,	/* WORD: ENVVAL */
/* 1*/	AWE_FX_ENV1_ATTACK,	/* BYTE: up ATKHLD */
/* 2*/	AWE_FX_ENV1_HOLD,	/* BYTE: lw ATKHLD */
/* 3*/	AWE_FX_ENV1_DECAY,	/* BYTE: lw DCYSUS */
/* 4*/	AWE_FX_ENV1_RELEASE,	/* BYTE: lw DCYSUS */
/* 5*/	AWE_FX_ENV1_SUSTAIN,	/* BYTE: up DCYSUS */
/* 6*/	AWE_FX_ENV1_PITCH,	/* BYTE: up PEFE */
/* 7*/	AWE_FX_ENV1_CUTOFF,	/* BYTE: lw PEFE */

/* volume envelope parameters */
/* 8*/	AWE_FX_ENV2_DELAY,	/* WORD: ENVVOL */
/* 9*/	AWE_FX_ENV2_ATTACK,	/* BYTE: up ATKHLDV */
/*10*/	AWE_FX_ENV2_HOLD,	/* BYTE: lw ATKHLDV */
/*11*/	AWE_FX_ENV2_DECAY,	/* BYTE: lw DCYSUSV */
/*12*/	AWE_FX_ENV2_RELEASE,	/* BYTE: lw DCYSUSV */
/*13*/	AWE_FX_ENV2_SUSTAIN,	/* BYTE: up DCYSUSV */
	
/* LFO1 (tremolo & vibrato) parameters */
/*14*/	AWE_FX_LFO1_DELAY,	/* WORD: LFO1VAL */
/*15*/	AWE_FX_LFO1_FREQ,	/* BYTE: lo TREMFRQ */
/*16*/	AWE_FX_LFO1_VOLUME,	/* BYTE: up TREMFRQ */
/*17*/	AWE_FX_LFO1_PITCH,	/* BYTE: up FMMOD */
/*18*/	AWE_FX_LFO1_CUTOFF,	/* BYTE: lo FMMOD */

/* LFO2 (vibrato) parameters */
/*19*/	AWE_FX_LFO2_DELAY,	/* WORD: LFO2VAL */
/*20*/	AWE_FX_LFO2_FREQ,	/* BYTE: lo FM2FRQ2 */
/*21*/	AWE_FX_LFO2_PITCH,	/* BYTE: up FM2FRQ2 */

/* Other overall effect parameters */
/*22*/	AWE_FX_INIT_PITCH,	/* SHORT: pitch offset */
/*23*/	AWE_FX_CHORUS,		/* BYTE: chorus effects send (0-255) */
/*24*/	AWE_FX_REVERB,		/* BYTE: reverb effects send (0-255) */
/*25*/	AWE_FX_CUTOFF,		/* BYTE: up IFATN */
/*26*/	AWE_FX_FILTERQ,		/* BYTE: up CCCA */

/* Sample / loop offset changes */
/*27*/	AWE_FX_SAMPLE_START,	/* SHORT: offset */
/*28*/	AWE_FX_LOOP_START,	/* SHORT: offset */
/*29*/	AWE_FX_LOOP_END,	/* SHORT: offset */
/*30*/	AWE_FX_COARSE_SAMPLE_START,	/* SHORT: upper word offset */
/*31*/	AWE_FX_COARSE_LOOP_START,	/* SHORT: upper word offset */
/*32*/	AWE_FX_COARSE_LOOP_END,		/* SHORT: upper word offset */

	AWE_FX_END,
};


#endif /* AWE_VOICE_H */
