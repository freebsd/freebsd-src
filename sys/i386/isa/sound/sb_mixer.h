/*
 * sound/sb_mixer.h
 * 
 * Definitions for the SB Pro and SB16 mixers
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
 * Modified:
 *	Hunyue Yau	Jan 6 1994
 *	Added defines for the Sound Galaxy NX Pro mixer.
 * 
 */

#define SBPRO_RECORDING_DEVICES	(SOUND_MASK_LINE | SOUND_MASK_MIC | SOUND_MASK_CD)

/* Same as SB Pro, unless I find otherwise */
#define SGNXPRO_RECORDING_DEVICES SBPRO_RECORDING_DEVICES

#define SBPRO_MIXER_DEVICES		(SOUND_MASK_SYNTH | SOUND_MASK_PCM | SOUND_MASK_LINE | SOUND_MASK_MIC | \
					 SOUND_MASK_CD | SOUND_MASK_VOLUME)

/* SG NX Pro has treble and bass settings on the mixer. The 'speaker'
 * channel is the COVOX/DisneySoundSource emulation volume control
 * on the mixer. It does NOT control speaker volume. Should have own
 * mask eventually?
 */
#define SGNXPRO_MIXER_DEVICES	(SBPRO_MIXER_DEVICES|SOUND_MASK_BASS| \
				 SOUND_MASK_TREBLE|SOUND_MASK_SPEAKER )

#define SB16_RECORDING_DEVICES		(SOUND_MASK_SYNTH | SOUND_MASK_LINE | SOUND_MASK_MIC | \
					 SOUND_MASK_CD)

#define SB16_MIXER_DEVICES		(SOUND_MASK_SYNTH | SOUND_MASK_PCM | SOUND_MASK_SPEAKER | SOUND_MASK_LINE | SOUND_MASK_MIC | \
					 SOUND_MASK_CD | SOUND_MASK_RECLEV | \
					 SOUND_MASK_VOLUME | SOUND_MASK_BASS | SOUND_MASK_TREBLE)

/*
 * Mixer registers
 * 
 * NOTE!	RECORD_SRC == IN_FILTER
 */

/* 
 * Mixer registers of SB Pro
 */
#define VOC_VOL		0x04
#define MIC_VOL		0x0A
#define MIC_MIX		0x0A
#define RECORD_SRC	0x0C
#define IN_FILTER	0x0C
#define OUT_FILTER	0x0E
#define MASTER_VOL	0x22
#define FM_VOL		0x26
#define CD_VOL		0x28
#define LINE_VOL	0x2E
#define IRQ_NR		0x80
#define DMA_NR		0x81
#define IRQ_STAT	0x82
#define OPSW		0x3c

/*
 * Additional registers on the SG NX Pro 
 */
#define COVOX_VOL	0x42
#define TREBLE_LVL	0x44
#define BASS_LVL	0x46

#define FREQ_HI         (1 << 3)/* Use High-frequency ANFI filters */
#define FREQ_LOW        0	/* Use Low-frequency ANFI filters */
#define FILT_ON         0	/* Yes, 0 to turn it on, 1 for off */
#define FILT_OFF        (1 << 5)

#define MONO_DAC	0x00
#define STEREO_DAC	0x02

/*
 * Mixer registers of SB16
 */
#define SB16_IMASK_L	0x3d
#define SB16_IMASK_R	0x3e

#define LEFT_CHN	0
#define RIGHT_CHN	1

struct mixer_def {
	unsigned int regno: 8;
	unsigned int bitoffs:4;
	unsigned int nbits:4;
};


typedef struct mixer_def mixer_tab[32][2];
typedef struct mixer_def mixer_ent;

#define MIX_ENT(name, reg_l, bit_l, len_l, reg_r, bit_r, len_r)	\
	{{reg_l, bit_l, len_l}, {reg_r, bit_r, len_r}}

#ifdef __SB_MIXER_C__
mixer_tab sbpro_mix = {
MIX_ENT(SOUND_MIXER_VOLUME,	0x22, 7, 4, 0x22, 3, 4),
MIX_ENT(SOUND_MIXER_BASS,	0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_TREBLE,	0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_SYNTH,	0x26, 7, 4, 0x26, 3, 4),
MIX_ENT(SOUND_MIXER_PCM,	0x04, 7, 4, 0x04, 3, 4),
MIX_ENT(SOUND_MIXER_SPEAKER,	0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_LINE,	0x2e, 7, 4, 0x2e, 3, 4),
MIX_ENT(SOUND_MIXER_MIC,	0x0a, 2, 3, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_CD,		0x28, 7, 4, 0x28, 3, 4),
MIX_ENT(SOUND_MIXER_IMIX,	0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_ALTPCM,	0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_RECLEV,	0x00, 0, 0, 0x00, 0, 0)
};

#ifdef	__SGNXPRO__
mixer_tab sgnxpro_mix = {
MIX_ENT(SOUND_MIXER_VOLUME,	0x22, 7, 4, 0x22, 3, 4),
MIX_ENT(SOUND_MIXER_BASS,	0x46, 2, 3, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_TREBLE,	0x44, 2, 3, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_SYNTH,	0x26, 7, 4, 0x26, 3, 4),
MIX_ENT(SOUND_MIXER_PCM,	0x04, 7, 4, 0x04, 3, 4),
MIX_ENT(SOUND_MIXER_SPEAKER,	0x42, 2, 3, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_LINE,	0x2e, 7, 4, 0x2e, 3, 4),
MIX_ENT(SOUND_MIXER_MIC,	0x0a, 2, 3, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_CD,		0x28, 7, 4, 0x28, 3, 4),
MIX_ENT(SOUND_MIXER_IMIX,	0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_ALTPCM,	0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_RECLEV,	0x00, 0, 0, 0x00, 0, 0)
};
#endif

mixer_tab sb16_mix = {
MIX_ENT(SOUND_MIXER_VOLUME,	0x30, 7, 5, 0x31, 7, 5),
MIX_ENT(SOUND_MIXER_BASS,	0x46, 7, 4, 0x47, 7, 4),
MIX_ENT(SOUND_MIXER_TREBLE,	0x44, 7, 4, 0x45, 7, 4),
MIX_ENT(SOUND_MIXER_SYNTH,	0x34, 7, 5, 0x35, 7, 5),
MIX_ENT(SOUND_MIXER_PCM,	0x32, 7, 5, 0x33, 7, 5),
MIX_ENT(SOUND_MIXER_SPEAKER,	0x3b, 7, 2, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_LINE,	0x38, 7, 5, 0x39, 7, 5),
MIX_ENT(SOUND_MIXER_MIC,	0x3a, 7, 5, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_CD,		0x36, 7, 5, 0x37, 7, 5),
MIX_ENT(SOUND_MIXER_IMIX,	0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_ALTPCM,	0x00, 0, 0, 0x00, 0, 0),
MIX_ENT(SOUND_MIXER_RECLEV,	0x3f, 7, 2, 0x40, 7, 2)
};

static unsigned short levels[SOUND_MIXER_NRDEVICES] =
{
  0x5a5a,			/* Master Volume */
  0x3232,			/* Bass */
  0x3232,			/* Treble */
  0x4b4b,			/* FM */
  0x4b4b,			/* PCM */
  0x4b4b,			/* PC Speaker */
  0x4b4b,			/* Ext Line */
  0x0000,			/* Mic */
  0x4b4b,			/* CD */
  0x4b4b,			/* Recording monitor */
  0x4b4b,			/* SB PCM */
  0x4b4b};			/* Recording level */

static unsigned char sb16_recmasks_L[SOUND_MIXER_NRDEVICES] =
{
	0x00,	/* SOUND_MIXER_VOLUME	*/
	0x00,	/* SOUND_MIXER_BASS	*/
	0x00,	/* SOUND_MIXER_TREBLE	*/
	0x40,	/* SOUND_MIXER_SYNTH	*/
	0x00,	/* SOUND_MIXER_PCM	*/
	0x00,	/* SOUND_MIXER_SPEAKER	*/
	0x10,	/* SOUND_MIXER_LINE	*/
	0x01,	/* SOUND_MIXER_MIC	*/
	0x04,	/* SOUND_MIXER_CD	*/
	0x00,	/* SOUND_MIXER_IMIX	*/
	0x00,	/* SOUND_MIXER_ALTPCM	*/
	0x00	/* SOUND_MIXER_RECLEV	*/
};

static unsigned char sb16_recmasks_R[SOUND_MIXER_NRDEVICES] =
{
	0x00,	/* SOUND_MIXER_VOLUME	*/
	0x00,	/* SOUND_MIXER_BASS	*/
	0x00,	/* SOUND_MIXER_TREBLE	*/
	0x20,	/* SOUND_MIXER_SYNTH	*/
	0x00,	/* SOUND_MIXER_PCM	*/
	0x00,	/* SOUND_MIXER_SPEAKER	*/
	0x08,	/* SOUND_MIXER_LINE	*/
	0x01,	/* SOUND_MIXER_MIC	*/
	0x02,	/* SOUND_MIXER_CD	*/
	0x00,	/* SOUND_MIXER_IMIX	*/
	0x00,	/* SOUND_MIXER_ALTPCM	*/
	0x00	/* SOUND_MIXER_RECLEV	*/
};
#endif
