/*
 * sound/ad1848_mixer.h
 * 
 * Definitions for the mixer of AD1848 and compatible codecs.
 * 
 * Copyright by Hannu Savolainen 1994
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
 */
/*
 * The AD1848 codec has generic input lines called Line, Aux1 and Aux2.
 * Soundcard manufacturers have connected actual inputs (CD, synth, line,
 * etc) to these inputs in different order. Therefore it's difficult
 * to assign mixer channels to to these inputs correctly. The following
 * contains two alternative mappings. The first one is for GUS MAX and
 * the second is just a generic one (line1, line2 and line3).
 * (Actually this is not a mapping but rather some kind of interleaving
 * solution).
 */
#define GUSMAX_MIXER
#ifdef GUSMAX_MIXER
#define MODE1_REC_DEVICES		(SOUND_MASK_LINE | SOUND_MASK_MIC | \
					 SOUND_MASK_CD|SOUND_MASK_IMIX)

#define MODE1_MIXER_DEVICES		(SOUND_MASK_SYNTH | SOUND_MASK_MIC | \
					 SOUND_MASK_CD | \
					 SOUND_MASK_IGAIN | \
					 SOUND_MASK_PCM|SOUND_MASK_IMIX)

#define MODE2_MIXER_DEVICES		(SOUND_MASK_SYNTH | SOUND_MASK_LINE | SOUND_MASK_MIC | \
					 SOUND_MASK_CD | SOUND_MASK_SPEAKER | \
					 SOUND_MASK_IGAIN | \
					 SOUND_MASK_PCM | SOUND_MASK_IMIX)
#else	/* Generic mapping */
#define MODE1_REC_DEVICES		(SOUND_MASK_LINE3 | SOUND_MASK_MIC | \
					 SOUND_MASK_LINE1|SOUND_MASK_IMIX)

#define MODE1_MIXER_DEVICES		(SOUND_MASK_LINE1 | SOUND_MASK_MIC | \
					 SOUND_MASK_LINE2 | \
					 SOUND_MASK_IGAIN | \
					 SOUND_MASK_PCM | SOUND_MASK_IMIX)

#define MODE2_MIXER_DEVICES		(SOUND_MASK_LINE1 | SOUND_MASK_LINE2 | SOUND_MASK_MIC | \
					 SOUND_MASK_LINE3 | SOUND_MASK_SPEAKER | \
					 SOUND_MASK_IGAIN | \
					 SOUND_MASK_PCM | SOUND_MASK_IMIX)
#endif

struct mixer_def {
	unsigned int regno: 7;
	unsigned int polarity:1;	/* 0=normal, 1=reversed */
	unsigned int bitpos:4;
	unsigned int nbits:4;
};

static char mix_cvt[101] = {
	0, 0,3,7,10,13,16,19,21,23,26,28,30,32,34,35,37,39,40,42,
	43,45,46,47,49,50,51,52,53,55,56,57,58,59,60,61,62,63,64,65,
	65,66,67,68,69,70,70,71,72,73,73,74,75,75,76,77,77,78,79,79,
	80,81,81,82,82,83,84,84,85,85,86,86,87,87,88,88,89,89,90,90,
	91,91,92,92,93,93,94,94,95,95,96,96,96,97,97,98,98,98,99,99,
	100
};

typedef struct mixer_def mixer_ent;

/*
 * Most of the mixer entries work in backwards. Setting the polarity field
 * makes them to work correctly.
 *
 * The channel numbering used by individual soundcards is not fixed. Some
 * cards have assigned different meanings for the AUX1, AUX2 and LINE inputs.
 * The current version doesn't try to compensate this.
 */

#define MIX_ENT(name, reg_l, pola_l, pos_l, len_l, reg_r, pola_r, pos_r, len_r)	\
	{{reg_l, pola_l, pos_l, len_l}, {reg_r, pola_r, pos_r, len_r}}

mixer_ent mix_devices[32][2] = {	/* As used in GUS MAX */
MIX_ENT(SOUND_MIXER_VOLUME,	 0, 0, 0, 0,	 0, 0, 0, 0),
MIX_ENT(SOUND_MIXER_BASS,	 0, 0, 0, 0,	 0, 0, 0, 0),
MIX_ENT(SOUND_MIXER_TREBLE,	 0, 0, 0, 0,	 0, 0, 0, 0),
MIX_ENT(SOUND_MIXER_SYNTH,	 4, 1, 0, 5,	 5, 1, 0, 5),
MIX_ENT(SOUND_MIXER_PCM,	 6, 1, 0, 6,	 7, 1, 0, 6),
MIX_ENT(SOUND_MIXER_SPEAKER,	26, 1, 0, 4,	 0, 0, 0, 0),
MIX_ENT(SOUND_MIXER_LINE,	18, 1, 0, 5,	19, 1, 0, 5),
MIX_ENT(SOUND_MIXER_MIC,	 0, 0, 5, 1,	 1, 0, 5, 1),
MIX_ENT(SOUND_MIXER_CD,	 	 2, 1, 0, 5,	 3, 1, 0, 5),
MIX_ENT(SOUND_MIXER_IMIX,	13, 1, 2, 6,	 0, 0, 0, 0),
MIX_ENT(SOUND_MIXER_ALTPCM,	 0, 0, 0, 0,	 0, 0, 0, 0),
MIX_ENT(SOUND_MIXER_RECLEV,	 0, 0, 0, 0,	 0, 0, 0, 0),
MIX_ENT(SOUND_MIXER_IGAIN,	 0, 0, 0, 4,	 1, 0, 0, 4),
MIX_ENT(SOUND_MIXER_OGAIN,	 0, 0, 0, 0,	 0, 0, 0, 0),
MIX_ENT(SOUND_MIXER_LINE1, 	 2, 1, 0, 5,	 3, 1, 0, 5),
MIX_ENT(SOUND_MIXER_LINE2,	 4, 1, 0, 5,	 5, 1, 0, 5),
MIX_ENT(SOUND_MIXER_LINE3,	18, 1, 0, 5,	19, 1, 0, 5)
};

static unsigned short default_mixer_levels[SOUND_MIXER_NRDEVICES] =
{
  0x5a5a,			/* Master Volume */
  0x3232,			/* Bass */
  0x3232,			/* Treble */
  0x4b4b,			/* FM */
  0x4040,			/* PCM */
  0x4b4b,			/* PC Speaker */
  /*  0x2020,			 Ext Line */
  0x0000,			/* Ext Line */
  0x4040,			/* Mic */
  0x4b4b,			/* CD */
  0x0000,			/* Recording monitor */
  0x4b4b,			/* SB PCM */
  0x4b4b,			/* Recording level */
  0x2525,			/* Input gain */
  0x0000,			/* Output gain */
  /*  0x4040,			Line1 */
  0x0000,			/* Line1 */
  0x0000,			/* Line2 */
  0x1515,			/* Line3 (usually line in)*/
};

#define LEFT_CHN	0
#define RIGHT_CHN	1

