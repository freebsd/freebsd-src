/*
 * file: sbcard.h
 */

typedef struct _sbdev_info {

} sbdev_info ;

extern int sbc_major, sbc_minor ;
/*
 * sound blaster registers
 */

#define SBDSP_RST	0x6
#define DSP_READ	(io_base + 0xA)
#define DSP_WRITE	(io_base + 0xC)
#define SBDSP_CMD	0xC
#define SBDSP_STATUS	0xC
#define DSP_DATA_AVAIL	(io_base + 0xE)
#define DSP_DATA_AVL16	(io_base + 0xF)

#define SB_MIX_ADDR	0x4
#define SB_MIX_DATA	0x5
#if 0
#define OPL3_LEFT	(io_base + 0x0)
#define OPL3_RIGHT	(io_base + 0x2)
#define OPL3_BOTH	(io_base + 0x8)
#endif

/*
 * DSP Commands. There are many, and in many cases they are used explicitly
 */

/* these are not used except for programmed I/O (not in this driver) */
#define	DSP_DAC8		0x10	/* direct DAC output */
#define	DSP_ADC8		0x20	/* direct ADC input */

/* these should be used in the SB 1.0 */
#define	DSP_CMD_DAC8		0x14	/* single cycle 8-bit dma out */
#define	DSP_CMD_ADC8		0x24	/* single cycle 8-bit dma in */

/* these should be used in the SB 2.0 and 2.01 */
#define	DSP_CMD_DAC8_AUTO	0x1c	/* auto 8-bit dma out */
#define	DSP_CMD_ADC8_AUTO	0x2c	/* auto 8-bit dma out */

#define	DSP_CMD_HSSIZE		0x48	/* high speed dma count */
#define	DSP_CMD_HSDAC_AUTO	0x90	/* high speed dac, auto */
#define DSP_CMD_HSADC_AUTO      0x98    /* high speed adc, auto */

/* SBPro commands. Some cards (JAZZ, SMW) also support 16 bits */

	/* prepare for dma input */
#define	DSP_CMD_DMAMODE(stereo, bit16) (0xA0 | (stereo ? 8:0) | (bit16 ? 4:0))

#define	DSP_CMD_DAC2		0x16	/* 2-bit adpcm dma out (cont) */
#define	DSP_CMD_DAC2S		0x17	/* 2-bit adpcm dma out (start) */

#define	DSP_CMD_DAC2S_AUTO	0x1f	/* auto 2-bit adpcm dma out (start) */


/* SB16 commands */
#define	DSP_CMD_O16		0xb0
#define	DSP_CMD_I16		0xb8
#define	DSP_CMD_O8		0xc0
#define	DSP_CMD_I8		0xc8

#define	DSP_MODE_U8MONO		0x00
#define	DSP_MODE_U8STEREO	0x20
#define	DSP_MODE_S16MONO	0x10
#define	DSP_MODE_S16STEREO	0x30

#define DSP_CMD_SPKON		0xD1
#define DSP_CMD_SPKOFF		0xD3
#define DSP_CMD_SPKR(on)	(0xD1 | (on ? 0:2))

#define	DSP_CMD_DMAPAUSE_8	0xD0
#define	DSP_CMD_DMAPAUSE_16	0xD5
#define	DSP_CMD_DMAEXIT_8	0xDA
#define	DSP_CMD_DMAEXIT_16	0xD9
#define	DSP_CMD_TCONST		0x40	/* set time constant */
#define	DSP_CMD_HSDAC		0x91	/* high speed dac */
#define DSP_CMD_HSADC           0x99    /* high speed adc */

#define	DSP_CMD_GETVER		0xE1
#define	DSP_CMD_GETID		0xE7	/* return id bytes */


#define	DSP_CMD_OUT16		0x41	/* send parms for dma out on sb16 */
#define	DSP_CMD_IN16		0x42	/* send parms for dma in on sb16 */
#if 0 /*** unknown ***/
#define	DSP_CMD_FA		0xFA	/* get version from prosonic*/
#define	DSP_CMD_FB		0xFB	/* set irq/dma for prosonic*/
#endif

/*
 * in fact, for the SB16, dma commands are as follows:
 *
 *  cmd, mode, len_low, len_high.
 * 
 * cmd is a combination of DSP_DMA16 or DSP_DMA8 and
 */

#define	DSP_DMA16		0xb0
#define	DSP_DMA8		0xc0
#   define DSP_F16_DAC		0x00
#   define DSP_F16_ADC		0x08
#   define DSP_F16_AUTO		0x04
#   define DSP_F16_FIFO_ON	0x02

/*
 * mode is a combination of the following:
 */
#define DSP_F16_STEREO	0x20
#define DSP_F16_SIGNED	0x10

#define IMODE_NONE		0
#define IMODE_OUTPUT		PCM_ENABLE_OUTPUT
#define IMODE_INPUT		PCM_ENABLE_INPUT
#define IMODE_INIT		3
#define IMODE_MIDI		4

#define NORMAL_MIDI	0
#define UART_MIDI	1

/*
 * values used for bd_flags in SoundBlaster driver
 */
#define	BD_F_HISPEED	0x0001	/* doing high speed ... */

#define	BD_F_JAZZ16	0x0002	/* jazz16 detected */
#define	BD_F_JAZZ16_2	0x0004	/* jazz16 type 2 */

#define	BD_F_DUP_MIDI	0x0008	/* duplex midi */

#define	BD_F_MIX_MASK	0x0070	/* up to 8 mixers (I know of 3) */
#define	BD_F_MIX_CT1335	0x0010	/* CT1335		*/
#define	BD_F_MIX_CT1345	0x0020	/* CT1345		*/
#define	BD_F_MIX_CT1745	0x0030	/* CT1745		*/

#define	BD_F_SB16	0x0100	/* this is a SB16 */
#define	BD_F_SB16X	0x0200	/* this is a vibra16X or clone */
#define	BD_F_MIDIBUSY	0x0400	/* midi busy */
#define	BD_F_ESS	0x0800	/* this is an ESS chip */


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
 * Modified: Hunyue Yau	Jan 6 1994 Added defines for the Sound Galaxy NX Pro
 * mixer.
 * 
 */

#define SBPRO_RECORDING_DEVICES	\
    (SOUND_MASK_LINE | SOUND_MASK_MIC | SOUND_MASK_CD)

/* Same as SB Pro, unless I find otherwise */
#define SGNXPRO_RECORDING_DEVICES SBPRO_RECORDING_DEVICES

#define SBPRO_MIXER_DEVICES	\
    (SOUND_MASK_SYNTH | SOUND_MASK_PCM | SOUND_MASK_LINE | SOUND_MASK_MIC | \
     SOUND_MASK_CD | SOUND_MASK_VOLUME)

/*
 * SG NX Pro has treble and bass settings on the mixer. The 'speaker' channel
 * is the COVOX/DisneySoundSource emulation volume control on the mixer. It
 * does NOT control speaker volume. Should have own mask eventually?
 */
#define SGNXPRO_MIXER_DEVICES	\
    (SBPRO_MIXER_DEVICES | SOUND_MASK_BASS | \
     SOUND_MASK_TREBLE | SOUND_MASK_SPEAKER )

#define SB16_RECORDING_DEVICES	\
    (SOUND_MASK_SYNTH | SOUND_MASK_LINE | SOUND_MASK_MIC | SOUND_MASK_CD)

#define SB16_MIXER_DEVICES	\
    (SOUND_MASK_SYNTH | SOUND_MASK_PCM | SOUND_MASK_SPEAKER | \
     SOUND_MASK_LINE | SOUND_MASK_MIC | SOUND_MASK_CD | \
     SOUND_MASK_IGAIN | SOUND_MASK_OGAIN | \
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
#define SB16_OMASK	0x3c


#ifndef __SB_MIXER_C__
mixer_tab       sbpro_mix;
mixer_tab       sb16_mix;
#ifdef	__SGNXPRO__
mixer_tab       sgnxpro_mix;
#endif
static u_char sb16_recmasks_L[SOUND_MIXER_NRDEVICES];
static u_char sb16_recmasks_R[SOUND_MIXER_NRDEVICES];
#else /* __SB_MIXER_C__ defined */
mixer_tab       sbpro_mix = {
    PMIX_ENT(SOUND_MIXER_VOLUME,  0x22, 7, 4, 0x22, 3, 4),
    PMIX_ENT(SOUND_MIXER_BASS,    0x00, 0, 0, 0x00, 0, 0),
    PMIX_ENT(SOUND_MIXER_TREBLE,  0x00, 0, 0, 0x00, 0, 0),
    PMIX_ENT(SOUND_MIXER_SYNTH,   0x26, 7, 4, 0x26, 3, 4),
    PMIX_ENT(SOUND_MIXER_PCM,     0x04, 7, 4, 0x04, 3, 4),
    PMIX_ENT(SOUND_MIXER_SPEAKER, 0x00, 0, 0, 0x00, 0, 0),
    PMIX_ENT(SOUND_MIXER_LINE,    0x2e, 7, 4, 0x2e, 3, 4),
    PMIX_ENT(SOUND_MIXER_MIC,     0x0a, 2, 3, 0x00, 0, 0),
    PMIX_ENT(SOUND_MIXER_CD,      0x28, 7, 4, 0x28, 3, 4),
    PMIX_ENT(SOUND_MIXER_IMIX,    0x00, 0, 0, 0x00, 0, 0),
    PMIX_ENT(SOUND_MIXER_ALTPCM,  0x00, 0, 0, 0x00, 0, 0),
    PMIX_ENT(SOUND_MIXER_RECLEV,  0x00, 0, 0, 0x00, 0, 0)
};

#ifdef	__SGNXPRO__
mixer_tab       sgnxpro_mix = {
    PMIX_ENT(SOUND_MIXER_VOLUME,  0x22, 7, 4, 0x22, 3, 4),
    PMIX_ENT(SOUND_MIXER_BASS,    0x46, 2, 3, 0x00, 0, 0),
    PMIX_ENT(SOUND_MIXER_TREBLE,  0x44, 2, 3, 0x00, 0, 0),
    PMIX_ENT(SOUND_MIXER_SYNTH,   0x26, 7, 4, 0x26, 3, 4),
    PMIX_ENT(SOUND_MIXER_PCM,     0x04, 7, 4, 0x04, 3, 4),
    PMIX_ENT(SOUND_MIXER_SPEAKER, 0x42, 2, 3, 0x00, 0, 0),
    PMIX_ENT(SOUND_MIXER_LINE,    0x2e, 7, 4, 0x2e, 3, 4),
    PMIX_ENT(SOUND_MIXER_MIC,     0x0a, 2, 3, 0x00, 0, 0),
    PMIX_ENT(SOUND_MIXER_CD,      0x28, 7, 4, 0x28, 3, 4),
    PMIX_ENT(SOUND_MIXER_IMIX,    0x00, 0, 0, 0x00, 0, 0),
    PMIX_ENT(SOUND_MIXER_ALTPCM,  0x00, 0, 0, 0x00, 0, 0),
    PMIX_ENT(SOUND_MIXER_RECLEV,  0x00, 0, 0, 0x00, 0, 0),
    PMIX_ENT(SOUND_MIXER_IGAIN,   0x00, 0, 0, 0x00, 0, 0),
    PMIX_ENT(SOUND_MIXER_OGAIN,   0x00, 0, 0, 0x00, 0, 0)
};
#endif

mixer_tab       sb16_mix = {
    PMIX_ENT(SOUND_MIXER_VOLUME,  0x30, 3, 5, 0x31, 3, 5),
    PMIX_ENT(SOUND_MIXER_BASS,    0x46, 4, 4, 0x47, 4, 4),
    PMIX_ENT(SOUND_MIXER_TREBLE,  0x44, 4, 4, 0x45, 4, 4),
    PMIX_ENT(SOUND_MIXER_SYNTH,   0x34, 3, 5, 0x35, 3, 5),
    PMIX_ENT(SOUND_MIXER_PCM,     0x32, 3, 5, 0x33, 3, 5),
    PMIX_ENT(SOUND_MIXER_SPEAKER, 0x3b, 6, 2, 0x00, 0, 0),
    PMIX_ENT(SOUND_MIXER_LINE,    0x38, 3, 5, 0x39, 3, 5),
    PMIX_ENT(SOUND_MIXER_MIC,     0x3a, 3, 5, 0x00, 0, 0),
    PMIX_ENT(SOUND_MIXER_CD,      0x36, 3, 5, 0x37, 3, 5),
    PMIX_ENT(SOUND_MIXER_IMIX,    0x00, 0, 0, 0x00, 0, 0),
    PMIX_ENT(SOUND_MIXER_ALTPCM,  0x00, 0, 0, 0x00, 0, 0),
    PMIX_ENT(SOUND_MIXER_RECLEV,  0x3f, 6, 2, 0x40, 6, 2), /* Obsol,Use IGAIN*/
    PMIX_ENT(SOUND_MIXER_IGAIN,   0x3f, 6, 2, 0x40, 6, 2),
    PMIX_ENT(SOUND_MIXER_OGAIN,   0x41, 6, 2, 0x42, 6, 2)
};

#ifdef SM_GAMES			/* Master volume is lower and PCM & FM
				 * volumes higher than with SB Pro. This
				 * improves the sound quality */

static u_short levels[SOUND_MIXER_NRDEVICES] =
{
	0x2020,			/* Master Volume */
	0x4b4b,			/* Bass */
	0x4b4b,			/* Treble */
	0x6464,			/* FM */
	0x6464,			/* PCM */
	0x4b4b,			/* PC Speaker */
	0x4b4b,			/* Ext Line */
	0x0000,			/* Mic */
	0x4b4b,			/* CD */
	0x4b4b,			/* Recording monitor */
	0x4b4b,			/* SB PCM */
	0x4b4b,			/* Recording level */
	0x4b4b,			/* Input gain */
0x4b4b};			/* Output gain */

#else				/* If the user selected just plain SB Pro */

static u_short levels[SOUND_MIXER_NRDEVICES] =
{
	0x5a5a,			/* Master Volume */
	0x4b4b,			/* Bass */
	0x4b4b,			/* Treble */
	0x4b4b,			/* FM */
	0x4b4b,			/* PCM */
	0x4b4b,			/* PC Speaker */
	0x4b4b,			/* Ext Line */
	0x1010,			/* Mic */
	0x4b4b,			/* CD */
	0x4b4b,			/* Recording monitor */
	0x4b4b,			/* SB PCM */
	0x4b4b,			/* Recording level */
	0x4b4b,			/* Input gain */
0x4b4b};			/* Output gain */
#endif				/* SM_GAMES */

static u_char sb16_recmasks_L[SOUND_MIXER_NRDEVICES] =
{
	0x00,			/* SOUND_MIXER_VOLUME	 */
	0x00,			/* SOUND_MIXER_BASS	 */
	0x00,			/* SOUND_MIXER_TREBLE	 */
	0x40,			/* SOUND_MIXER_SYNTH	 */
	0x00,			/* SOUND_MIXER_PCM	 */
	0x00,			/* SOUND_MIXER_SPEAKER	 */
	0x10,			/* SOUND_MIXER_LINE	 */
	0x01,			/* SOUND_MIXER_MIC	 */
	0x04,			/* SOUND_MIXER_CD	 */
	0x00,			/* SOUND_MIXER_IMIX	 */
	0x00,			/* SOUND_MIXER_ALTPCM	 */
	0x00,			/* SOUND_MIXER_RECLEV	 */
	0x00,			/* SOUND_MIXER_IGAIN	 */
	0x00			/* SOUND_MIXER_OGAIN	 */
};

static u_char sb16_recmasks_R[SOUND_MIXER_NRDEVICES] =
{
	0x00,			/* SOUND_MIXER_VOLUME	 */
	0x00,			/* SOUND_MIXER_BASS	 */
	0x00,			/* SOUND_MIXER_TREBLE	 */
	0x20,			/* SOUND_MIXER_SYNTH	 */
	0x00,			/* SOUND_MIXER_PCM	 */
	0x00,			/* SOUND_MIXER_SPEAKER	 */
	0x08,			/* SOUND_MIXER_LINE	 */
	0x01,			/* SOUND_MIXER_MIC	 */
	0x02,			/* SOUND_MIXER_CD	 */
	0x00,			/* SOUND_MIXER_IMIX	 */
	0x00,			/* SOUND_MIXER_ALTPCM	 */
	0x00,			/* SOUND_MIXER_RECLEV	 */
	0x00,			/* SOUND_MIXER_IGAIN	 */
	0x00			/* SOUND_MIXER_OGAIN	 */
};

/*
 * Recording sources (SB Pro)
 */
#endif /* __SB_MIXER_C__ */

#define SRC_MIC         1	/* Select Microphone recording source */
#define SRC_CD          3	/* Select CD recording source */
#define SRC_LINE        7	/* Use Line-in for recording source */


