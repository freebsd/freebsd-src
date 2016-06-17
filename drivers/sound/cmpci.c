/*****************************************************************************/
/*
 *      cmpci.c  --  C-Media PCI audio driver.
 *
 *      Copyright (C) 1999  ChenLi Tien (cltien@cmedia.com.tw)
 *      		    C-media support (support@cmedia.com.tw)
 *
 *      Based on the PCI drivers by Thomas Sailer (sailer@ife.ee.ethz.ch)
 *
 * 	For update, visit:
 * 		http://members.home.net/puresoft/cmedia.html
 * 		http://www.cmedia.com.tw
 * 	
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Special thanks to David C. Niemi, Jan Pfeifer
 *
 *
 * Module command line parameters:
 *   none so far
 *
 *
 *  Supported devices:
 *  /dev/dsp    standard /dev/dsp device, (mostly) OSS compatible
 *  /dev/mixer  standard /dev/mixer device, (mostly) OSS compatible
 *  /dev/midi   simple MIDI UART interface, no ioctl
 *
 *  The card has both an FM and a Wavetable synth, but I have to figure
 *  out first how to drive them...
 *
 *  Revision history
 *    06.05.98   0.1   Initial release
 *    10.05.98   0.2   Fixed many bugs, esp. ADC rate calculation
 *                     First stab at a simple midi interface (no bells&whistles)
 *    13.05.98   0.3   Fix stupid cut&paste error: set_adc_rate was called instead of
 *                     set_dac_rate in the FMODE_WRITE case in cm_open
 *                     Fix hwptr out of bounds (now mpg123 works)
 *    14.05.98   0.4   Don't allow excessive interrupt rates
 *    08.06.98   0.5   First release using Alan Cox' soundcore instead of miscdevice
 *    03.08.98   0.6   Do not include modversions.h
 *                     Now mixer behaviour can basically be selected between
 *                     "OSS documented" and "OSS actual" behaviour
 *    31.08.98   0.7   Fix realplayer problems - dac.count issues
 *    10.12.98   0.8   Fix drain_dac trying to wait on not yet initialized DMA
 *    16.12.98   0.9   Fix a few f_file & FMODE_ bugs
 *    06.01.99   0.10  remove the silly SA_INTERRUPT flag.
 *                     hopefully killed the egcs section type conflict
 *    12.03.99   0.11  cinfo.blocks should be reset after GETxPTR ioctl.
 *                     reported by Johan Maes <joma@telindus.be>
 *    22.03.99   0.12  return EAGAIN instead of EBUSY when O_NONBLOCK
 *                     read/write cannot be executed
 *    18.08.99   1.5   Only deallocate DMA buffer when unloading.
 *    02.09.99   1.6   Enable SPDIF LOOP
 *                     Change the mixer read back
 *    21.09.99   2.33  Use RCS version as driver version.
 *                     Add support for modem, S/PDIF loop and 4 channels.
 *                     (8738 only)
 *                     Fix bug cause x11amp cannot play.
 *
 *    Fixes:
 *    Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *    18/05/2001 - .bss nitpicks, fix a bug in set_dac_channels where it
 *    		   was calling prog_dmabuf with s->lock held, call missing
 *    		   unlock_kernel in cm_midi_release
 *    08/10/2001 - use set_current_state in some more places
 *
 *	Carlos Eduardo Gorges <carlos@techlinux.com.br>
 *	Fri May 25 2001 
 *	- SMP support ( spin[un]lock* revision )
 *	- speaker mixer support 
 *	Mon Aug 13 2001
 *	- optimizations and cleanups
 *    03/01/2003 - open_mode fixes from Georg Acher <acher@in.tum.de>
 *
 */
/*****************************************************************************/
      
#include <linux/version.h>
#include <linux/config.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/sound.h>
#include <linux/slab.h>
#include <linux/soundcard.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/spinlock.h>
#include <linux/smp_lock.h>
#include <linux/wrapper.h>
#include <asm/uaccess.h>
#include <asm/hardirq.h>
#include <linux/gameport.h>
#include <linux/bitops.h>

#include "dm.h"

/* --------------------------------------------------------------------- */
#undef OSS_DOCUMENTED_MIXER_SEMANTICS
#undef DMABYTEIO
/* --------------------------------------------------------------------- */

#define CM_MAGIC  ((PCI_VENDOR_ID_CMEDIA<<16)|PCI_DEVICE_ID_CMEDIA_CM8338A)

/* CM8338 registers definition ****************/

#define CODEC_CMI_FUNCTRL0		(0x00)
#define CODEC_CMI_FUNCTRL1		(0x04)
#define CODEC_CMI_CHFORMAT		(0x08)
#define CODEC_CMI_INT_HLDCLR		(0x0C)
#define CODEC_CMI_INT_STATUS		(0x10)
#define CODEC_CMI_LEGACY_CTRL		(0x14)
#define CODEC_CMI_MISC_CTRL		(0x18)
#define CODEC_CMI_TDMA_POS		(0x1C)
#define CODEC_CMI_MIXER			(0x20)
#define CODEC_SB16_DATA			(0x22)
#define CODEC_SB16_ADDR			(0x23)
#define CODEC_CMI_MIXER1		(0x24)
#define CODEC_CMI_MIXER2		(0x25)
#define CODEC_CMI_AUX_VOL		(0x26)
#define CODEC_CMI_MISC			(0x27)
#define CODEC_CMI_AC97			(0x28)

#define CODEC_CMI_CH0_FRAME1		(0x80)
#define CODEC_CMI_CH0_FRAME2		(0x84)
#define CODEC_CMI_CH1_FRAME1		(0x88)
#define CODEC_CMI_CH1_FRAME2		(0x8C)

#define CODEC_CMI_EXT_REG		(0xF0)

/*  Mixer registers for SB16 ******************/

#define DSP_MIX_DATARESETIDX		((unsigned char)(0x00))

#define DSP_MIX_MASTERVOLIDX_L		((unsigned char)(0x30))
#define DSP_MIX_MASTERVOLIDX_R		((unsigned char)(0x31))
#define DSP_MIX_VOICEVOLIDX_L		((unsigned char)(0x32))
#define DSP_MIX_VOICEVOLIDX_R		((unsigned char)(0x33))
#define DSP_MIX_FMVOLIDX_L		((unsigned char)(0x34))
#define DSP_MIX_FMVOLIDX_R		((unsigned char)(0x35))
#define DSP_MIX_CDVOLIDX_L		((unsigned char)(0x36))
#define DSP_MIX_CDVOLIDX_R		((unsigned char)(0x37))
#define DSP_MIX_LINEVOLIDX_L		((unsigned char)(0x38))
#define DSP_MIX_LINEVOLIDX_R		((unsigned char)(0x39))

#define DSP_MIX_MICVOLIDX		((unsigned char)(0x3A))
#define DSP_MIX_SPKRVOLIDX		((unsigned char)(0x3B))

#define DSP_MIX_OUTMIXIDX		((unsigned char)(0x3C))

#define DSP_MIX_ADCMIXIDX_L		((unsigned char)(0x3D))
#define DSP_MIX_ADCMIXIDX_R		((unsigned char)(0x3E))

#define DSP_MIX_INGAINIDX_L		((unsigned char)(0x3F))
#define DSP_MIX_INGAINIDX_R		((unsigned char)(0x40))
#define DSP_MIX_OUTGAINIDX_L		((unsigned char)(0x41))
#define DSP_MIX_OUTGAINIDX_R		((unsigned char)(0x42))

#define DSP_MIX_AGCIDX			((unsigned char)(0x43))

#define DSP_MIX_TREBLEIDX_L		((unsigned char)(0x44))
#define DSP_MIX_TREBLEIDX_R		((unsigned char)(0x45))
#define DSP_MIX_BASSIDX_L		((unsigned char)(0x46))
#define DSP_MIX_BASSIDX_R		((unsigned char)(0x47))
#define DSP_MIX_EXTENSION		((unsigned char)(0xf0))
// pseudo register for AUX
#define	DSP_MIX_AUXVOL_L		((unsigned char)(0x50))
#define	DSP_MIX_AUXVOL_R		((unsigned char)(0x51))

// I/O length
#define CM_EXTENT_CODEC	  0x100
#define CM_EXTENT_MIDI	  0x2
#define CM_EXTENT_SYNTH	  0x4
#define CM_EXTENT_GAME	  0x8

// Function Control Register 0 (00h)
#define CHADC0    	0x01
#define CHADC1    	0x02
#define PAUSE0	  	0x04
#define PAUSE1	  	0x08
 
// Function Control Register 0+2 (02h)
#define CHEN0     	0x01
#define CHEN1     	0x02
#define RST_CH0	  	0x04
#define RST_CH1	  	0x08
 
// Function Control Register 1 (04h)
#define JYSTK_EN	0x02
#define UART_EN		0x04
#define	SPDO2DAC	0x40
#define	SPDFLOOP	0x80
 
// Function Control Register 1+1 (05h)
#define	SPDF_0		0x01
#define	SPDF_1		0x02
#define	ASFC		0xe0
#define	DSFC		0x1c
#define	SPDIF2DAC	(SPDF_0 << 8 | SPDO2DAC)

// Channel Format Register (08h)
#define CM_CFMT_STEREO	0x01
#define CM_CFMT_16BIT	0x02
#define CM_CFMT_MASK	0x03
#define	POLVALID	0x20
#define	INVSPDIFI	0x80

// Channel Format Register+2 (0ah)
#define SPD24SEL	0x20

// Channel Format Register+3 (0bh)
#define CHB3D		0x20
#define CHB3D5C		0x80

// Interrupt Hold/Clear Register+2 (0eh)
#define	CH0_INT_EN	0x01
#define	CH1_INT_EN	0x02

// Interrupt Register (10h)
#define CHINT0		0x01
#define CHINT1		0x02
#define	CH0BUSY		0x04
#define	CH1BUSY		0x08

// Legacy Control/Status Register+1 (15h)
#define	EXBASEN		0x10
#define	BASE2LIN	0x20
#define	CENTR2LIN	0x40
#define	CB2LIN		(BASE2LIN|CENTR2LIN)
#define	CHB3D6C		0x80

// Legacy Control/Status Register+2 (16h)
#define	DAC2SPDO	0x20
#define	SPDCOPYRHT	0x40
#define	ENSPDOUT	0x80

// Legacy Control/Status Register+3 (17h)
#define	FMSEL		0x03
#define	VSBSEL		0x0c
#define	VMPU		0x60
#define	NXCHG		0x80
 
// Miscellaneous Control Register (18h)
#define	REAR2LIN	0x20
#define	MUTECH1		0x40
#define	ENCENTER	0x80
 
// Miscellaneous Control Register+1 (19h)
#define	SELSPDIFI2	0x01
#define	SPDF_AC97	0x80
 
// Miscellaneous Control Register+2 (1ah)
#define	AC3_EN		0x04
#define	FM_EN		0x08
#define	SPD32SEL	0x20
#define	XCHGDAC		0x40
#define	ENDBDAC		0x80
 
// Miscellaneous Control Register+3 (1bh)
#define	SPDIFI48K	0x01
#define	SPDO5V		0x02
#define	N4SPK3D		0x04
#define	RESET		0x40
#define	PWD		0x80
#define	SPDIF48K	(SPDIFI48K << 24 | SPDF_AC97 << 8)
 
// Mixer1 (24h)
#define	CDPLAY		0x01
#define	X3DEN		0x02
#define	REAR2FRONT	0x10
#define	SPK4		0x20
#define	WSMUTE		0x40
 
// Miscellaneous Register (27h)
#define	SPDVALID	0x02
#define	CENTR2MIC	0x04

#define CM_CFMT_DACSHIFT   0
#define CM_CFMT_ADCSHIFT   2
#define CM_FREQ_DACSHIFT   2
#define CM_FREQ_ADCSHIFT   5
#define	RSTDAC		RST_CH0
#define	RSTADC		RST_CH1
#define	ENDAC		CHEN0
#define	ENADC		CHEN1
#define	PAUSEDAC	PAUSE0
#define	PAUSEADC	PAUSE1
#define CODEC_CMI_DAC_FRAME1	CODEC_CMI_CH0_FRAME1
#define CODEC_CMI_DAC_FRAME2	CODEC_CMI_CH0_FRAME2
#define CODEC_CMI_ADC_FRAME1	CODEC_CMI_CH1_FRAME1
#define CODEC_CMI_ADC_FRAME2	CODEC_CMI_CH1_FRAME2
#define	DACINT		CHINT0
#define	ADCINT		CHINT1
#define	DACBUSY		CH0BUSY
#define	ADCBUSY		CH1BUSY
#define	ENDACINT	CH0_INT_EN
#define	ENADCINT	CH1_INT_EN

static const unsigned sample_size[] = { 1, 2, 2, 4 };
static const unsigned sample_shift[]	= { 0, 1, 1, 2 };

/* MIDI buffer sizes **************************/

#define MIDIINBUF  256
#define MIDIOUTBUF 256

#define FMODE_MIDI_SHIFT 2
#define FMODE_MIDI_READ  (FMODE_READ << FMODE_MIDI_SHIFT)
#define FMODE_MIDI_WRITE (FMODE_WRITE << FMODE_MIDI_SHIFT)

#define FMODE_DMFM 0x10

#define SND_DEV_DSP16   5 

#define NR_DEVICE 3		/* maximum number of devices */

static unsigned int devindex = 0;

//*********************************************/

struct cm_state {
	/* magic */
	unsigned int magic;

	/* list of cmedia devices */
	struct list_head devs;

	/* the corresponding pci_dev structure */
	struct pci_dev *dev;

	int dev_audio;			/* soundcore stuff */
	int dev_mixer;
	int dev_midi;
	int dev_dmfm;

	unsigned int iosb, iobase, iosynth,
			 iomidi, iogame, irq;	/* hardware resources */
	unsigned short deviceid;		/* pci_id */

        struct {				/* mixer stuff */
                unsigned int modcnt;
		unsigned short vol[13];
        } mix;

	unsigned int rateadc, ratedac;		/* wave stuff */
	unsigned char fmt, enable;

	spinlock_t lock;
	struct semaphore open_sem;
	mode_t open_mode;
	wait_queue_head_t open_wait;

	struct dmabuf {
		void *rawbuf;
		unsigned rawphys;
		unsigned buforder;
		unsigned numfrag;
		unsigned fragshift;
		unsigned hwptr, swptr;
		unsigned total_bytes;
		int count;
		unsigned error;		/* over/underrun */
		wait_queue_head_t wait;
		
		unsigned fragsize;	/* redundant, but makes calculations easier */
		unsigned dmasize;
		unsigned fragsamples;
		unsigned dmasamples;
		
		unsigned mapped:1;	/* OSS stuff */
		unsigned ready:1;
		unsigned endcleared:1;
		unsigned ossfragshift;
		int ossmaxfrags;
		unsigned subdivision;
	} dma_dac, dma_adc;

	struct {			/* midi stuff */
		unsigned ird, iwr, icnt;
		unsigned ord, owr, ocnt;
		wait_queue_head_t iwait;
		wait_queue_head_t owait;
		struct timer_list timer;
		unsigned char ibuf[MIDIINBUF];
		unsigned char obuf[MIDIOUTBUF];
	} midi;

	struct gameport gameport;

	int	chip_version;		
	int	max_channels;
	int	curr_channels;		
	int	speakers;		/* number of speakers */
	int	capability;		/* HW capability, various for chip versions */

	int	status;			/* HW or SW state */
	
	int	spdif_counter;		/* spdif frame counter */
};

/* flags used for capability */
#define	CAN_AC3_HW		0x00000001		/* 037 or later */
#define	CAN_AC3_SW		0x00000002		/* 033 or later */
#define	CAN_AC3			(CAN_AC3_HW | CAN_AC3_SW)
#define CAN_DUAL_DAC		0x00000004		/* 033 or later */
#define	CAN_MULTI_CH_HW		0x00000008		/* 039 or later */
#define	CAN_MULTI_CH		(CAN_MULTI_CH_HW | CAN_DUAL_DAC)
#define	CAN_LINE_AS_REAR	0x00000010		/* 033 or later */
#define	CAN_LINE_AS_BASS	0x00000020		/* 039 or later */
#define	CAN_MIC_AS_BASS		0x00000040		/* 039 or later */

/* flags used for status */
#define	DO_AC3_HW		0x00000001
#define	DO_AC3_SW		0x00000002
#define	DO_AC3			(DO_AC3_HW | DO_AC3_SW)
#define	DO_DUAL_DAC		0x00000004
#define	DO_MULTI_CH_HW		0x00000008
#define	DO_MULTI_CH		(DO_MULTI_CH_HW | DO_DUAL_DAC)
#define	DO_LINE_AS_REAR		0x00000010		/* 033 or later */
#define	DO_LINE_AS_BASS		0x00000020		/* 039 or later */
#define	DO_MIC_AS_BASS		0x00000040		/* 039 or later */
#define	DO_SPDIF_OUT		0x00000100
#define	DO_SPDIF_IN		0x00000200
#define	DO_SPDIF_LOOP		0x00000400

static LIST_HEAD(devs);

/* --------------------------------------------------------------------- */

static inline unsigned ld2(unsigned int x)
{
	unsigned exp=16,l=5,r=0;
	static const unsigned num[]={0x2,0x4,0x10,0x100,0x10000};

	/* num: 2, 4, 16, 256, 65536 */
	/* exp: 1, 2,  4,   8,    16 */
	
	while(l--) {
		if( x >= num[l] ) {
			if(num[l]>2) x >>= exp;
			r+=exp;
		}
		exp>>=1;
	}

	return r;
}

/* --------------------------------------------------------------------- */

static void maskb(unsigned int addr, unsigned int mask, unsigned int value)
{
	outb((inb(addr) & mask) | value, addr);
}

static void maskw(unsigned int addr, unsigned int mask, unsigned int value)
{
	outw((inw(addr) & mask) | value, addr);
}

static void maskl(unsigned int addr, unsigned int mask, unsigned int value)
{
	outl((inl(addr) & mask) | value, addr);
}

static void set_dmadac1(struct cm_state *s, unsigned int addr, unsigned int count)
{
	if (addr)
	    outl(addr, s->iobase + CODEC_CMI_ADC_FRAME1);
	outw(count - 1, s->iobase + CODEC_CMI_ADC_FRAME2);
	maskb(s->iobase + CODEC_CMI_FUNCTRL0, ~CHADC1, 0);
}

static void set_dmaadc(struct cm_state *s, unsigned int addr, unsigned int count)
{
	outl(addr, s->iobase + CODEC_CMI_ADC_FRAME1);
	outw(count - 1, s->iobase + CODEC_CMI_ADC_FRAME2);
	maskb(s->iobase + CODEC_CMI_FUNCTRL0, ~0, CHADC1);
}

static void set_dmadac(struct cm_state *s, unsigned int addr, unsigned int count)
{
	outl(addr, s->iobase + CODEC_CMI_DAC_FRAME1);
	outw(count - 1, s->iobase + CODEC_CMI_DAC_FRAME2);
	maskb(s->iobase + CODEC_CMI_FUNCTRL0, ~CHADC0, 0);
	if (s->status & DO_DUAL_DAC)
		set_dmadac1(s, 0, count);
}

static void set_countadc(struct cm_state *s, unsigned count)
{
	outw(count - 1, s->iobase + CODEC_CMI_ADC_FRAME2 + 2);
}

static void set_countdac(struct cm_state *s, unsigned count)
{
	outw(count - 1, s->iobase + CODEC_CMI_DAC_FRAME2 + 2);
	if (s->status & DO_DUAL_DAC)
	    set_countadc(s, count);
}

static unsigned get_dmadac(struct cm_state *s)
{
	unsigned int curr_addr;

	curr_addr = inw(s->iobase + CODEC_CMI_DAC_FRAME2) + 1;
	curr_addr <<= sample_shift[(s->fmt >> CM_CFMT_DACSHIFT) & CM_CFMT_MASK];
	curr_addr = s->dma_dac.dmasize - curr_addr;

	return curr_addr;
}

static unsigned get_dmaadc(struct cm_state *s)
{
	unsigned int curr_addr;

	curr_addr = inw(s->iobase + CODEC_CMI_ADC_FRAME2) + 1;
	curr_addr <<= sample_shift[(s->fmt >> CM_CFMT_ADCSHIFT) & CM_CFMT_MASK];
	curr_addr = s->dma_adc.dmasize - curr_addr;

	return curr_addr;
}

static void wrmixer(struct cm_state *s, unsigned char idx, unsigned char data)
{
	unsigned char regval, pseudo;

	// pseudo register
	if (idx == DSP_MIX_AUXVOL_L) {
		data >>= 4;
		data &= 0x0f;
		regval = inb(s->iobase + CODEC_CMI_AUX_VOL) & ~0x0f;
		outb(regval | data, s->iobase + CODEC_CMI_AUX_VOL);
		return;
	}
	if (idx == DSP_MIX_AUXVOL_R) {
		data &= 0xf0;
		regval = inb(s->iobase + CODEC_CMI_AUX_VOL) & ~0xf0;
		outb(regval | data, s->iobase + CODEC_CMI_AUX_VOL);
		return;
	}
	outb(idx, s->iobase + CODEC_SB16_ADDR);
	udelay(10);
	// pseudo bits
	if (idx == DSP_MIX_OUTMIXIDX) {
		pseudo = data & ~0x1f;
		pseudo >>= 1;	
		regval = inb(s->iobase + CODEC_CMI_MIXER2) & ~0x30;
		outb(regval | pseudo, s->iobase + CODEC_CMI_MIXER2);
	}
	if (idx == DSP_MIX_ADCMIXIDX_L) {
		pseudo = data & 0x80;
		pseudo >>= 1;
		regval = inb(s->iobase + CODEC_CMI_MIXER2) & ~0x40;
		outb(regval | pseudo, s->iobase + CODEC_CMI_MIXER2);
	}
	if (idx == DSP_MIX_ADCMIXIDX_R) {
		pseudo = data & 0x80;
		regval = inb(s->iobase + CODEC_CMI_MIXER2) & ~0x80;
		outb(regval | pseudo, s->iobase + CODEC_CMI_MIXER2);
	}
	outb(data, s->iobase + CODEC_SB16_DATA);
	udelay(10);
}

static unsigned char rdmixer(struct cm_state *s, unsigned char idx)
{
	unsigned char v, pseudo;
	
	// pseudo register
	if (idx == DSP_MIX_AUXVOL_L) {
		v = inb(s->iobase + CODEC_CMI_AUX_VOL) & 0x0f;
		v <<= 4;
		return v;
	}
	if (idx == DSP_MIX_AUXVOL_L) {
		v = inb(s->iobase + CODEC_CMI_AUX_VOL) & 0xf0;
		return v;
	}
	outb(idx, s->iobase + CODEC_SB16_ADDR);
	udelay(10);
	v = inb(s->iobase + CODEC_SB16_DATA);
	udelay(10);
	// pseudo bits
	if (idx == DSP_MIX_OUTMIXIDX) {
		pseudo = inb(s->iobase + CODEC_CMI_MIXER2) & 0x30;
		pseudo <<= 1;
		v |= pseudo;
	}
	if (idx == DSP_MIX_ADCMIXIDX_L) {
		pseudo = inb(s->iobase + CODEC_CMI_MIXER2) & 0x40;
		pseudo <<= 1;
		v |= pseudo;
	}
	if (idx == DSP_MIX_ADCMIXIDX_R) {
		pseudo = inb(s->iobase + CODEC_CMI_MIXER2) & 0x80;
		v |= pseudo;
	}
	return v;
}

static void set_fmt_unlocked(struct cm_state *s, unsigned char mask, unsigned char data)
{
	if (mask)
	{
		s->fmt = inb(s->iobase + CODEC_CMI_CHFORMAT);
		udelay(10);
	}
	s->fmt = (s->fmt & mask) | data;
	outb(s->fmt, s->iobase + CODEC_CMI_CHFORMAT);
	udelay(10);
}

static void set_fmt(struct cm_state *s, unsigned char mask, unsigned char data)
{
	unsigned long flags;

	spin_lock_irqsave(&s->lock, flags);
	set_fmt_unlocked(s,mask,data);
	spin_unlock_irqrestore(&s->lock, flags);
}

static void frobindir(struct cm_state *s, unsigned char idx, unsigned char mask, unsigned char data)
{
	outb(idx, s->iobase + CODEC_SB16_ADDR);
	udelay(10);
	outb((inb(s->iobase + CODEC_SB16_DATA) & mask) | data, s->iobase + CODEC_SB16_DATA);
	udelay(10);
}

static struct {
	unsigned	rate;
	unsigned	lower;
	unsigned	upper;
	unsigned char	freq;
} rate_lookup[] =
{
	{ 5512,		(0 + 5512) / 2,		(5512 + 8000) / 2,	0 },
	{ 8000,		(5512 + 8000) / 2,	(8000 + 11025) / 2,	4 },
	{ 11025,	(8000 + 11025) / 2,	(11025 + 16000) / 2,	1 },
	{ 16000,	(11025 + 16000) / 2,	(16000 + 22050) / 2,	5 },
	{ 22050,	(16000 + 22050) / 2,	(22050 + 32000) / 2,	2 },
	{ 32000,	(22050 + 32000) / 2,	(32000 + 44100) / 2,	6 },
	{ 44100,	(32000 + 44100) / 2,	(44100 + 48000) / 2,	3 },
	{ 48000,	(44100 + 48000) / 2,	48000,			7 }
};

static void set_spdif_copyright(struct cm_state *s, int spdif_copyright)
{
	/* enable SPDIF-in Copyright */
	maskb(s->iobase + CODEC_CMI_LEGACY_CTRL + 2, ~SPDCOPYRHT, spdif_copyright ? SPDCOPYRHT : 0);
}

static void set_spdif_loop(struct cm_state *s, int spdif_loop)
{
	/* enable SPDIF loop */
	if (spdif_loop) {
		s->status |= DO_SPDIF_LOOP;
		/* turn on spdif-in to spdif-out */
		maskb(s->iobase + CODEC_CMI_FUNCTRL1, ~0, SPDFLOOP);
	} else {
		s->status &= ~DO_SPDIF_LOOP;
		/* turn off spdif-in to spdif-out */
		maskb(s->iobase + CODEC_CMI_FUNCTRL1, ~SPDFLOOP, 0);
	}
}

static void set_spdif_monitor(struct cm_state *s, int channel)
{
	// SPDO2DAC
	maskw(s->iobase + CODEC_CMI_FUNCTRL1, ~SPDO2DAC, channel == 2 ? SPDO2DAC : 0);
	// CDPLAY
	if (s->chip_version >= 39)
		maskb(s->iobase + CODEC_CMI_MIXER1, ~CDPLAY, channel ? CDPLAY : 0);
}

static void set_spdifout_level(struct cm_state *s, int level5v)
{
	/* SPDO5V */
	maskb(s->iobase + CODEC_CMI_MISC_CTRL + 3, ~SPDO5V, level5v ? SPDO5V : 0);
}

static void set_spdifin_inverse(struct cm_state *s, int spdif_inverse)
{
	if (spdif_inverse) {
		/* turn on spdif-in inverse */
		if (s->chip_version >= 39)
			maskb(s->iobase + CODEC_CMI_CHFORMAT, ~0, INVSPDIFI);
		else
			maskb(s->iobase + CODEC_CMI_CHFORMAT + 2, ~0, 1);
	} else {
		/* turn off spdif-ininverse */
		if (s->chip_version >= 39) 
			maskb(s->iobase + CODEC_CMI_CHFORMAT, ~INVSPDIFI, 0);
		else
			maskb(s->iobase + CODEC_CMI_CHFORMAT + 2, ~1, 0);
	}
}

static void set_spdifin_channel2(struct cm_state *s, int channel2)
{
	/* SELSPDIFI2 */
	if (s->chip_version >= 39)
		maskb(s->iobase + CODEC_CMI_MISC_CTRL + 1, ~SELSPDIFI2, channel2 ? SELSPDIFI2 : 0);
}

static void set_spdifin_valid(struct cm_state *s, int valid)
{
	/* SPDVALID */
	maskb(s->iobase + CODEC_CMI_MISC, ~SPDVALID, valid ? SPDVALID : 0);
}

static void set_spdifout_unlocked(struct cm_state *s, unsigned rate)
{
	if (rate == 48000 || rate == 44100) {
		// SPDF_0
		maskw(s->iobase + CODEC_CMI_FUNCTRL1, ~0, SPDF_0);
		// SPDIFI48K SPDF_ACc97
		maskl(s->iobase + CODEC_CMI_MISC_CTRL, ~SPDIF48K, rate == 48000 ? SPDIF48K : 0);
		// ENSPDOUT
		maskb(s->iobase + CODEC_CMI_LEGACY_CTRL + 2, ~0, ENSPDOUT);
		// monitor SPDIF out
		set_spdif_monitor(s, 2);
		s->status |= DO_SPDIF_OUT;
	} else {
		maskw(s->iobase + CODEC_CMI_FUNCTRL1, ~SPDF_0, 0);
		maskb(s->iobase + CODEC_CMI_LEGACY_CTRL + 2, ~ENSPDOUT, 0);
		// monitor none
		set_spdif_monitor(s, 0);
		s->status &= ~DO_SPDIF_OUT;
	}
}

static void set_spdifout(struct cm_state *s, unsigned rate)
{
	unsigned long flags;

	spin_lock_irqsave(&s->lock, flags);
	set_spdifout_unlocked(s,rate);
	spin_unlock_irqrestore(&s->lock, flags);
}

static void set_spdifin_unlocked(struct cm_state *s, unsigned rate)
{
	if (rate == 48000 || rate == 44100) {
		// SPDF_1
		maskw(s->iobase + CODEC_CMI_FUNCTRL1, ~0, SPDF_1);
		// SPDIFI48K SPDF_AC97
		maskl(s->iobase + CODEC_CMI_MISC_CTRL, ~SPDIF48K, rate == 48000 ? SPDIF48K : 0);
		s->status |= DO_SPDIF_IN;
	} else {
		maskw(s->iobase + CODEC_CMI_FUNCTRL1, ~SPDF_1, 0);
		s->status &= ~DO_SPDIF_IN;
	}
}

static void set_spdifin(struct cm_state *s, unsigned rate)
{
	unsigned long flags;

	spin_lock_irqsave(&s->lock, flags);
	set_spdifin_unlocked(s,rate);
	spin_unlock_irqrestore(&s->lock, flags);
}

//* find parity for bit 4~30 */
static unsigned parity(unsigned data)
{
	unsigned parity = 0;
	int counter = 4;

	data >>= 4;	// start from bit 4
	while (counter <= 30) {
		if (data & 1)
			parity++;
		data >>= 1;
		counter++;
	}
	return parity & 1;
}

static void set_ac3_unlocked(struct cm_state *s, unsigned rate)
{
	/* enable AC3 */
	if (rate == 48000 || rate == 44100) {
		// mute DAC
		maskb(s->iobase + CODEC_CMI_MIXER1, ~0, WSMUTE);
		// AC3EN for 039, 0x04
		if (s->chip_version >= 39)
			maskb(s->iobase + CODEC_CMI_MISC_CTRL + 2, ~0, AC3_EN);
		// AC3EN for 037, 0x10
		else if (s->chip_version == 37)
			maskb(s->iobase + CODEC_CMI_CHFORMAT + 2, ~0, 0x10);
		if (s->capability & CAN_AC3_HW) {
			// SPD24SEL for 039, 0x20, but cannot be set
			if (s->chip_version >= 39)
				maskb(s->iobase + CODEC_CMI_CHFORMAT + 2, ~0, SPD24SEL);
			// SPD24SEL for 037, 0x02
			else if (s->chip_version == 37)
				maskb(s->iobase + CODEC_CMI_CHFORMAT + 2, ~0, 0x02);
			s->status |= DO_AC3_HW;
			if (s->chip_version >= 39)
				maskb(s->iobase + CODEC_CMI_MIXER1, ~CDPLAY, 0);
		 } else {
			// SPD32SEL for 037 & 039, 0x20
			maskb(s->iobase + CODEC_CMI_MISC_CTRL + 2, ~0, SPD32SEL);
			// set 176K sample rate to fix 033 HW bug
			if (s->chip_version == 33) {
				if (rate == 48000)
					maskb(s->iobase + CODEC_CMI_CHFORMAT + 1, ~0, 0x08);
				else
					maskb(s->iobase + CODEC_CMI_CHFORMAT + 1, ~0x08, 0);
			}
			s->status |= DO_AC3_SW;
		}
	} else {
		maskb(s->iobase + CODEC_CMI_MIXER1, ~WSMUTE, 0);
		maskb(s->iobase + CODEC_CMI_CHFORMAT + 2, ~0x32, 0);
		maskb(s->iobase + CODEC_CMI_MISC_CTRL + 2, ~(SPD32SEL|AC3_EN), 0);
		if (s->chip_version == 33)
			maskb(s->iobase + CODEC_CMI_CHFORMAT + 1, ~0x08, 0);
		if (s->chip_version >= 39)
			maskb(s->iobase + CODEC_CMI_MIXER1, ~0, CDPLAY);
		s->status &= ~DO_AC3;
	}
	s->spdif_counter = 0;
}

static void set_ac3(struct cm_state *s, unsigned rate)
{
	unsigned long flags;

	spin_lock_irqsave(&s->lock, flags);
	set_spdifout_unlocked(s, rate);
	set_ac3_unlocked(s,rate);
	spin_unlock_irqrestore(&s->lock, flags);
}

static int trans_ac3(struct cm_state *s, void *dest, const char *source, int size)
{
	int   i = size / 2;
	unsigned long data;
	unsigned long *dst = (unsigned long *) dest;
	unsigned short *src = (unsigned short *)source;
	int err;

	do {
		if ((err = __get_user(data, src)))
			return err;
		src++;
		data <<= 12;			// ok for 16-bit data
		if (s->spdif_counter == 2 || s->spdif_counter == 3)
			data |= 0x40000000;	// indicate AC-3 raw data
		if (parity(data))
			data |= 0x80000000;	// parity
		if (s->spdif_counter == 0)
			data |= 3;		// preamble 'M'
		else if (s->spdif_counter & 1)
			data |= 5;		// odd, 'W'
		else
			data |= 9;		// even, 'M'
		*dst++ = data;
		s->spdif_counter++;
		if (s->spdif_counter == 384)
			s->spdif_counter = 0;
	} while (--i);

	return 0;
}

static void set_adc_rate_unlocked(struct cm_state *s, unsigned rate)
{
	unsigned char freq = 4;
	int	i;

	if (rate > 48000)
		rate = 48000;
	if (rate < 8000)
		rate = 8000;
	for (i = 0; i < sizeof(rate_lookup) / sizeof(rate_lookup[0]); i++) {
		if (rate > rate_lookup[i].lower && rate <= rate_lookup[i].upper) {
			rate = rate_lookup[i].rate;
			freq = rate_lookup[i].freq;
			break;
	    	}
	}
	s->rateadc = rate;
	freq <<= CM_FREQ_ADCSHIFT;

	maskb(s->iobase + CODEC_CMI_FUNCTRL1 + 1, ~ASFC, freq);
}

static void set_adc_rate(struct cm_state *s, unsigned rate)
{
	unsigned long flags;
	unsigned char freq = 4;
	int	i;

	if (rate > 48000)
		rate = 48000;
	if (rate < 8000)
		rate = 8000;
	for (i = 0; i < sizeof(rate_lookup) / sizeof(rate_lookup[0]); i++) {
		if (rate > rate_lookup[i].lower && rate <= rate_lookup[i].upper) {
			rate = rate_lookup[i].rate;
			freq = rate_lookup[i].freq;
			break;
	    	}
	}
	s->rateadc = rate;
	freq <<= CM_FREQ_ADCSHIFT;

	spin_lock_irqsave(&s->lock, flags);
	maskb(s->iobase + CODEC_CMI_FUNCTRL1 + 1, ~ASFC, freq);
	spin_unlock_irqrestore(&s->lock, flags);
}

static void set_dac_rate(struct cm_state *s, unsigned rate)
{
	unsigned long flags;
	unsigned char freq = 4;
	int	i;

	if (rate > 48000)
		rate = 48000;
	if (rate < 8000)
		rate = 8000;
	for (i = 0; i < sizeof(rate_lookup) / sizeof(rate_lookup[0]); i++) {
		if (rate > rate_lookup[i].lower && rate <= rate_lookup[i].upper) {
			rate = rate_lookup[i].rate;
			freq = rate_lookup[i].freq;
			break;
	    	}
	}
	s->ratedac = rate;
	freq <<= CM_FREQ_DACSHIFT;

	spin_lock_irqsave(&s->lock, flags);
	maskb(s->iobase + CODEC_CMI_FUNCTRL1 + 1, ~DSFC, freq);
	spin_unlock_irqrestore(&s->lock, flags);

	if (s->curr_channels <=  2)
		set_spdifout(s, rate);
	if (s->status & DO_DUAL_DAC)
		set_adc_rate(s, rate);
}

/* --------------------------------------------------------------------- */
static inline void reset_adc(struct cm_state *s)
{
	/* reset bus master */
	outb(s->enable | RSTADC, s->iobase + CODEC_CMI_FUNCTRL0 + 2);
	udelay(10);
	outb(s->enable & ~RSTADC, s->iobase + CODEC_CMI_FUNCTRL0 + 2);
}

static inline void reset_dac(struct cm_state *s)
{
	/* reset bus master */
	outb(s->enable | RSTDAC, s->iobase + CODEC_CMI_FUNCTRL0 + 2);
	udelay(10);
	outb(s->enable & ~RSTDAC, s->iobase + CODEC_CMI_FUNCTRL0 + 2);
	if (s->status & DO_DUAL_DAC)
		reset_adc(s);
}

static inline void pause_adc(struct cm_state *s)
{
	maskb(s->iobase + CODEC_CMI_FUNCTRL0, ~0, PAUSEADC);
}

static inline void pause_dac(struct cm_state *s)
{
	maskb(s->iobase + CODEC_CMI_FUNCTRL0, ~0, PAUSEDAC);
	if (s->status & DO_DUAL_DAC)
		pause_adc(s);
}

static inline void disable_adc(struct cm_state *s)
{
	/* disable channel */
	s->enable &= ~ENADC;
	outb(s->enable, s->iobase + CODEC_CMI_FUNCTRL0 + 2);
	reset_adc(s);
}

static inline void disable_dac(struct cm_state *s)
{
	/* disable channel */
	s->enable &= ~ENDAC;
	outb(s->enable, s->iobase + CODEC_CMI_FUNCTRL0 + 2);
	reset_dac(s);
	if (s->status & DO_DUAL_DAC)
		disable_adc(s);
}

static inline void enable_adc(struct cm_state *s)
{
	if (!(s->enable & ENADC)) {
		/* enable channel */
		s->enable |= ENADC;
		outb(s->enable, s->iobase + CODEC_CMI_FUNCTRL0 + 2);
	}
	maskb(s->iobase + CODEC_CMI_FUNCTRL0, ~PAUSEADC, 0);
}

static inline void enable_dac_unlocked(struct cm_state *s)
{
	if (!(s->enable & ENDAC)) {
		/* enable channel */
		s->enable |= ENDAC;
		outb(s->enable, s->iobase + CODEC_CMI_FUNCTRL0 + 2);
	}
	maskb(s->iobase + CODEC_CMI_FUNCTRL0, ~PAUSEDAC, 0);

	if (s->status & DO_DUAL_DAC)
		enable_adc(s);
}

static inline void enable_dac(struct cm_state *s)
{
	unsigned long flags;

	spin_lock_irqsave(&s->lock, flags);
	enable_dac_unlocked(s);
	spin_unlock_irqrestore(&s->lock, flags);
}

static inline void stop_adc_unlocked(struct cm_state *s)
{
	if (s->enable & ENADC) {
		/* disable interrupt */
		maskb(s->iobase + CODEC_CMI_INT_HLDCLR + 2, ~ENADCINT, 0);
		disable_adc(s);
	}
}

static inline void stop_adc(struct cm_state *s)
{
	unsigned long flags;

	spin_lock_irqsave(&s->lock, flags);
	stop_adc_unlocked(s);
	spin_unlock_irqrestore(&s->lock, flags);

}

static inline void stop_dac_unlocked(struct cm_state *s)
{
	if (s->enable & ENDAC) {
		/* disable interrupt */
		maskb(s->iobase + CODEC_CMI_INT_HLDCLR + 2, ~ENDACINT, 0);
		disable_dac(s);
	}
	if (s->status & DO_DUAL_DAC)
		stop_adc_unlocked(s);
}

static inline void stop_dac(struct cm_state *s)
{
	unsigned long flags;

	spin_lock_irqsave(&s->lock, flags);
	stop_dac_unlocked(s);
	spin_unlock_irqrestore(&s->lock, flags);
}

static inline void start_adc_unlocked(struct cm_state *s)
{
	if ((s->dma_adc.mapped || s->dma_adc.count < (signed)(s->dma_adc.dmasize - 2*s->dma_adc.fragsize))
	    && s->dma_adc.ready) {
		/* enable interrupt */
		maskb(s->iobase + CODEC_CMI_INT_HLDCLR + 2, ~0, ENADCINT);
		enable_adc(s);
	}
}

static void start_adc(struct cm_state *s)
{
	unsigned long flags;

	spin_lock_irqsave(&s->lock, flags);
	start_adc_unlocked(s);
	spin_unlock_irqrestore(&s->lock, flags);
}	

static void start_dac1_unlocked(struct cm_state *s)
{
	if ((s->dma_adc.mapped || s->dma_adc.count > 0) && s->dma_adc.ready) {
		/* enable interrupt */
		maskb(s->iobase + CODEC_CMI_INT_HLDCLR + 2, ~0, ENADCINT);
 		enable_dac_unlocked(s);
	}
}

static void start_dac_unlocked(struct cm_state *s)
{
	if ((s->dma_dac.mapped || s->dma_dac.count > 0) && s->dma_dac.ready) {
		/* enable interrupt */
		maskb(s->iobase + CODEC_CMI_INT_HLDCLR + 2, ~0, ENDACINT);
		enable_dac_unlocked(s);
	}
	if (s->status & DO_DUAL_DAC)
		start_dac1_unlocked(s);
}

static void start_dac(struct cm_state *s)
{
	unsigned long flags;

	spin_lock_irqsave(&s->lock, flags);
	start_dac_unlocked(s);
	spin_unlock_irqrestore(&s->lock, flags);
}	

static int prog_dmabuf(struct cm_state *s, unsigned rec);

static int set_dac_channels(struct cm_state *s, int channels)
{
	unsigned long flags;
	spin_lock_irqsave(&s->lock, flags);

	if ((channels > 2) && (channels <= s->max_channels)
	 && (((s->fmt >> CM_CFMT_DACSHIFT) & CM_CFMT_MASK) == (CM_CFMT_STEREO | CM_CFMT_16BIT))) {
	    set_spdifout_unlocked(s, 0);
	    if (s->capability & CAN_MULTI_CH_HW) {
		// NXCHG
		maskb(s->iobase + CODEC_CMI_LEGACY_CTRL + 3, ~0, NXCHG);
		// CHB3D or CHB3D5C
	       	maskb(s->iobase + CODEC_CMI_CHFORMAT + 3, ~(CHB3D5C|CHB3D), channels > 4 ? CHB3D5C : CHB3D);
		// CHB3D6C
		maskb(s->iobase + CODEC_CMI_LEGACY_CTRL + 1, ~CHB3D6C, channels == 6 ? CHB3D6C : 0);
		// ENCENTER 
		maskb(s->iobase + CODEC_CMI_MISC_CTRL, ~ENCENTER, channels == 6 ? ENCENTER : 0);
		s->status |= DO_MULTI_CH_HW;
	    } else if (s->capability & CAN_DUAL_DAC) {
		unsigned char fmtm = ~0, fmts = 0;
		ssize_t ret;

		// ENDBDAC, turn on double DAC mode
		// XCHGDAC, CH0 -> back, CH1->front
		maskb(s->iobase + CODEC_CMI_MISC_CTRL + 2, ~0, ENDBDAC|XCHGDAC);
		s->status |= DO_DUAL_DAC;
		// prepare secondary buffer

		spin_unlock_irqrestore(&s->lock, flags);
		ret = prog_dmabuf(s, 1);
		if (ret) return ret;
		spin_lock_irqsave(&s->lock, flags);

		// copy the hw state
		fmtm &= ~((CM_CFMT_STEREO | CM_CFMT_16BIT) << CM_CFMT_DACSHIFT);
		fmtm &= ~((CM_CFMT_STEREO | CM_CFMT_16BIT) << CM_CFMT_ADCSHIFT);
		// the HW only support 16-bit stereo
		fmts |= CM_CFMT_16BIT << CM_CFMT_DACSHIFT;
		fmts |= CM_CFMT_16BIT << CM_CFMT_ADCSHIFT;
		fmts |= CM_CFMT_STEREO << CM_CFMT_DACSHIFT;
		fmts |= CM_CFMT_STEREO << CM_CFMT_ADCSHIFT;
		
		set_fmt_unlocked(s, fmtm, fmts);
		set_adc_rate_unlocked(s, s->ratedac);

	    }

	    // N4SPK3D, disable 4 speaker mode (analog duplicate)
	    if (s->speakers > 2)
		maskb(s->iobase + CODEC_CMI_MISC_CTRL + 3, ~N4SPK3D, 0);
	    s->curr_channels = channels;
	} else {
	    if (s->status & DO_MULTI_CH_HW) {
		maskb(s->iobase + CODEC_CMI_LEGACY_CTRL + 3, ~NXCHG, 0);
		maskb(s->iobase + CODEC_CMI_CHFORMAT + 3, ~(CHB3D5C|CHB3D), 0);
		maskb(s->iobase + CODEC_CMI_LEGACY_CTRL + 1, ~CHB3D6C, 0);
	    } else if (s->status & DO_DUAL_DAC) {
		maskb(s->iobase + CODEC_CMI_MISC_CTRL + 2, ~ENDBDAC, 0);
	    }
	    // N4SPK3D, enable 4 speaker mode (analog duplicate)
	    if (s->speakers > 2)
		maskb(s->iobase + CODEC_CMI_MISC_CTRL + 3, ~0, N4SPK3D);
	    s->status &= ~DO_MULTI_CH;
	    s->curr_channels = s->fmt & (CM_CFMT_STEREO << CM_CFMT_DACSHIFT) ? 2 : 1;
	}

	spin_unlock_irqrestore(&s->lock, flags);
	return s->curr_channels;
}

/* --------------------------------------------------------------------- */

#define DMABUF_DEFAULTORDER (16-PAGE_SHIFT)
#define DMABUF_MINORDER 1

static void dealloc_dmabuf(struct dmabuf *db)
{
	struct page *pstart, *pend;
	
	if (db->rawbuf) {
		/* undo marking the pages as reserved */
		pend = virt_to_page(db->rawbuf + (PAGE_SIZE << db->buforder) - 1);
		for (pstart = virt_to_page(db->rawbuf); pstart <= pend; pstart++)
			mem_map_unreserve(pstart);
		free_pages((unsigned long)db->rawbuf, db->buforder);
	}
	db->rawbuf = NULL;
	db->mapped = db->ready = 0;
}

/* Ch1 is used for playback, Ch0 is used for recording */

static int prog_dmabuf(struct cm_state *s, unsigned rec)
{
	struct dmabuf *db = rec ? &s->dma_adc : &s->dma_dac;
	unsigned rate = rec ? s->rateadc : s->ratedac;
	int order;
	unsigned bytepersec;
	unsigned bufs;
	struct page *pstart, *pend;
	unsigned char fmt;
	unsigned long flags;

	fmt = s->fmt;
	if (rec) {
		stop_adc(s);
		fmt >>= CM_CFMT_ADCSHIFT;
	} else {
		stop_dac(s);
		fmt >>= CM_CFMT_DACSHIFT;
	}

	fmt &= CM_CFMT_MASK;
	db->hwptr = db->swptr = db->total_bytes = db->count = db->error = db->endcleared = 0;
	if (!db->rawbuf) {
		db->ready = db->mapped = 0;
		for (order = DMABUF_DEFAULTORDER; order >= DMABUF_MINORDER; order--)
			if ((db->rawbuf = (void *)__get_free_pages(GFP_KERNEL | GFP_DMA, order)))
				break;
		if (!db->rawbuf)
			return -ENOMEM;
		db->buforder = order;
		db->rawphys = virt_to_bus(db->rawbuf);
		if ((db->rawphys ^ (db->rawphys + (PAGE_SIZE << db->buforder) - 1)) & ~0xffff)
			printk(KERN_DEBUG "cmpci: DMA buffer crosses 64k boundary: busaddr 0x%lx  size %ld\n", 
			       (long) db->rawphys, PAGE_SIZE << db->buforder);
		if ((db->rawphys + (PAGE_SIZE << db->buforder) - 1) & ~0xffffff)
			printk(KERN_DEBUG "cmpci: DMA buffer beyond 16MB: busaddr 0x%lx  size %ld\n", 
			       (long) db->rawphys, PAGE_SIZE << db->buforder);
		/* now mark the pages as reserved; otherwise remap_page_range doesn't do what we want */
		pend = virt_to_page(db->rawbuf + (PAGE_SIZE << db->buforder) - 1);
		for (pstart = virt_to_page(db->rawbuf); pstart <= pend; pstart++)
			mem_map_reserve(pstart);
	}
	bytepersec = rate << sample_shift[fmt];
	bufs = PAGE_SIZE << db->buforder;
	if (db->ossfragshift) {
		if ((1000 << db->ossfragshift) < bytepersec)
			db->fragshift = ld2(bytepersec/1000);
		else
			db->fragshift = db->ossfragshift;
	} else {
		db->fragshift = ld2(bytepersec/100/(db->subdivision ? db->subdivision : 1));
		if (db->fragshift < 3)
			db->fragshift = 3;
	}
	db->numfrag = bufs >> db->fragshift;
	while (db->numfrag < 4 && db->fragshift > 3) {
		db->fragshift--;
		db->numfrag = bufs >> db->fragshift;
	}
	db->fragsize = 1 << db->fragshift;
	if (db->ossmaxfrags >= 4 && db->ossmaxfrags < db->numfrag)
		db->numfrag = db->ossmaxfrags;
 	/* to make fragsize >= 4096 */
	db->fragsamples = db->fragsize >> sample_shift[fmt];
	db->dmasize = db->numfrag << db->fragshift;
	db->dmasamples = db->dmasize >> sample_shift[fmt];
	memset(db->rawbuf, (fmt & CM_CFMT_16BIT) ? 0 : 0x80, db->dmasize);
	spin_lock_irqsave(&s->lock, flags);
	if (rec) {
		if (s->status & DO_DUAL_DAC)
		    set_dmadac1(s, db->rawphys, db->dmasize >> sample_shift[fmt]);
		else
		    set_dmaadc(s, db->rawphys, db->dmasize >> sample_shift[fmt]);
		/* program sample counts */
		set_countdac(s, db->fragsamples);
	} else {
		set_dmadac(s, db->rawphys, db->dmasize >> sample_shift[fmt]);
		/* program sample counts */
		set_countdac(s, db->fragsamples);
	}
	spin_unlock_irqrestore(&s->lock, flags);
	db->ready = 1;
	return 0;
}

static inline void clear_advance(struct cm_state *s)
{
	unsigned char c = (s->fmt & (CM_CFMT_16BIT << CM_CFMT_DACSHIFT)) ? 0 : 0x80;
	unsigned char *buf = s->dma_dac.rawbuf;
	unsigned char *buf1 = s->dma_adc.rawbuf;
	unsigned bsize = s->dma_dac.dmasize;
	unsigned bptr = s->dma_dac.swptr;
	unsigned len = s->dma_dac.fragsize;

	if (bptr + len > bsize) {
		unsigned x = bsize - bptr;
		memset(buf + bptr, c, x);
		if (s->status & DO_DUAL_DAC)
			memset(buf1 + bptr, c, x);
		bptr = 0;
		len -= x;
	}
	memset(buf + bptr, c, len);
	if (s->status & DO_DUAL_DAC)
		memset(buf1 + bptr, c, len);
}

/* call with spinlock held! */
static void cm_update_ptr(struct cm_state *s)
{
	unsigned hwptr;
	int diff;

	/* update ADC pointer */
	if (s->dma_adc.ready) {
	    if (s->status & DO_DUAL_DAC) {
		hwptr = get_dmaadc(s) % s->dma_adc.dmasize;
		diff = (s->dma_adc.dmasize + hwptr - s->dma_adc.hwptr) % s->dma_adc.dmasize;
		s->dma_adc.hwptr = hwptr;
		s->dma_adc.total_bytes += diff;
		if (s->dma_adc.mapped) {
			s->dma_adc.count += diff;
			if (s->dma_adc.count >= (signed)s->dma_adc.fragsize)
				wake_up(&s->dma_adc.wait);
		} else {
			s->dma_adc.count -= diff;
			if (s->dma_adc.count <= 0) {
				pause_adc(s);
				s->dma_adc.error++;
			} else if (s->dma_adc.count <= (signed)s->dma_adc.fragsize && !s->dma_adc.endcleared) {
				clear_advance(s);
				s->dma_adc.endcleared = 1;
			}
			if (s->dma_dac.count + (signed)s->dma_dac.fragsize <= (signed)s->dma_dac.dmasize)
				wake_up(&s->dma_adc.wait);
		}
	    } else {
		hwptr = get_dmaadc(s) % s->dma_adc.dmasize;
		diff = (s->dma_adc.dmasize + hwptr - s->dma_adc.hwptr) % s->dma_adc.dmasize;
		s->dma_adc.hwptr = hwptr;
		s->dma_adc.total_bytes += diff;
		s->dma_adc.count += diff;
		if (s->dma_adc.count >= (signed)s->dma_adc.fragsize) 
			wake_up(&s->dma_adc.wait);
		if (!s->dma_adc.mapped) {
			if (s->dma_adc.count > (signed)(s->dma_adc.dmasize - ((3 * s->dma_adc.fragsize) >> 1))) {
				pause_adc(s);
				s->dma_adc.error++;
			}
		}
	    }
	}
	/* update DAC pointer */
	if (s->dma_dac.ready) {
		hwptr = get_dmadac(s) % s->dma_dac.dmasize;
		diff = (s->dma_dac.dmasize + hwptr - s->dma_dac.hwptr) % s->dma_dac.dmasize;
		s->dma_dac.hwptr = hwptr;
		s->dma_dac.total_bytes += diff;
		if (s->dma_dac.mapped) {
			s->dma_dac.count += diff;
			if (s->dma_dac.count >= (signed)s->dma_dac.fragsize)
				wake_up(&s->dma_dac.wait);
		} else {
			s->dma_dac.count -= diff;
			if (s->dma_dac.count <= 0) {
				pause_dac(s);
				s->dma_dac.error++;
			} else if (s->dma_dac.count <= (signed)s->dma_dac.fragsize && !s->dma_dac.endcleared) {
				clear_advance(s);
				s->dma_dac.endcleared = 1;
			}
			if (s->dma_dac.count + (signed)s->dma_dac.fragsize <= (signed)s->dma_dac.dmasize)
				wake_up(&s->dma_dac.wait);
		}
	}
}

#ifdef CONFIG_SOUND_CMPCI_MIDI
/* hold spinlock for the following! */
static void cm_handle_midi(struct cm_state *s)
{
	unsigned char ch;
	int wake;

	wake = 0;
	while (!(inb(s->iomidi+1) & 0x80)) {
		ch = inb(s->iomidi);
		if (s->midi.icnt < MIDIINBUF) {
			s->midi.ibuf[s->midi.iwr] = ch;
			s->midi.iwr = (s->midi.iwr + 1) % MIDIINBUF;
			s->midi.icnt++;
		}
		wake = 1;
	}
	if (wake)
		wake_up(&s->midi.iwait);
	wake = 0;
	while (!(inb(s->iomidi+1) & 0x40) && s->midi.ocnt > 0) {
		outb(s->midi.obuf[s->midi.ord], s->iomidi);
		s->midi.ord = (s->midi.ord + 1) % MIDIOUTBUF;
		s->midi.ocnt--;
		if (s->midi.ocnt < MIDIOUTBUF-16)
			wake = 1;
	}
	if (wake)
		wake_up(&s->midi.owait);
}
#endif

static void cm_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
        struct cm_state *s = (struct cm_state *)dev_id;
	unsigned int intsrc, intstat;
	unsigned char mask = 0;
	
	/* fastpath out, to ease interrupt sharing */
	intsrc = inl(s->iobase + CODEC_CMI_INT_STATUS);
	if (!(intsrc & 0x80000000))
		return;
	spin_lock(&s->lock);
	intstat = inb(s->iobase + CODEC_CMI_INT_HLDCLR + 2);
	/* acknowledge interrupt */
	if (intsrc & ADCINT)
		mask |= ENADCINT;
	if (intsrc & DACINT)
		mask |= ENDACINT;
	outb(intstat & ~mask, s->iobase + CODEC_CMI_INT_HLDCLR + 2);
	outb(intstat | mask, s->iobase + CODEC_CMI_INT_HLDCLR + 2);
	cm_update_ptr(s);
#ifdef CONFIG_SOUND_CMPCI_MIDI
	cm_handle_midi(s);
#endif
	spin_unlock(&s->lock);
}

#ifdef CONFIG_SOUND_CMPCI_MIDI
static void cm_midi_timer(unsigned long data)
{
	struct cm_state *s = (struct cm_state *)data;
	unsigned long flags;
	
	spin_lock_irqsave(&s->lock, flags);
	cm_handle_midi(s);
	spin_unlock_irqrestore(&s->lock, flags);
	s->midi.timer.expires = jiffies+1;
	add_timer(&s->midi.timer);
}
#endif

/* --------------------------------------------------------------------- */

static const char invalid_magic[] = KERN_CRIT "cmpci: invalid magic value\n";

#ifdef CONFIG_SOUND_CMPCI	/* support multiple chips */
#define VALIDATE_STATE(s)
#else
#define VALIDATE_STATE(s)                         \
({                                                \
	if (!(s) || (s)->magic != CM_MAGIC) { \
		printk(invalid_magic);            \
		return -ENXIO;                    \
	}                                         \
})
#endif

/* --------------------------------------------------------------------- */

#define MT_4          1
#define MT_5MUTE      2
#define MT_4MUTEMONO  3
#define MT_6MUTE      4
#define MT_5MUTEMONO  5

static const struct {
	unsigned left;
	unsigned right;
	unsigned type;
	unsigned rec;
	unsigned play;
} mixtable[SOUND_MIXER_NRDEVICES] = {
	[SOUND_MIXER_CD]     = { DSP_MIX_CDVOLIDX_L,     DSP_MIX_CDVOLIDX_R,     MT_5MUTE,     0x04, 0x02 },
	[SOUND_MIXER_LINE]   = { DSP_MIX_LINEVOLIDX_L,   DSP_MIX_LINEVOLIDX_R,   MT_5MUTE,     0x10, 0x08 },
	[SOUND_MIXER_MIC]    = { DSP_MIX_MICVOLIDX,      DSP_MIX_MICVOLIDX,      MT_5MUTEMONO, 0x01, 0x01 },
	[SOUND_MIXER_SYNTH]  = { DSP_MIX_FMVOLIDX_L,  	 DSP_MIX_FMVOLIDX_R,     MT_5MUTE,     0x40, 0x00 },
	[SOUND_MIXER_VOLUME] = { DSP_MIX_MASTERVOLIDX_L, DSP_MIX_MASTERVOLIDX_R, MT_5MUTE,     0x00, 0x00 },
	[SOUND_MIXER_PCM]    = { DSP_MIX_VOICEVOLIDX_L,  DSP_MIX_VOICEVOLIDX_R,  MT_5MUTE,     0x00, 0x00 },
	[SOUND_MIXER_LINE1]  = { DSP_MIX_AUXVOL_L,       DSP_MIX_AUXVOL_R,       MT_5MUTE,     0x80, 0x20 },
	[SOUND_MIXER_SPEAKER]= { DSP_MIX_SPKRVOLIDX,	 DSP_MIX_SPKRVOLIDX,	 MT_5MUTEMONO, 0x01, 0x01 }
};

static const unsigned char volidx[SOUND_MIXER_NRDEVICES] = 
{
	[SOUND_MIXER_CD]     = 1,
	[SOUND_MIXER_LINE]   = 2,
	[SOUND_MIXER_MIC]    = 3,
	[SOUND_MIXER_SYNTH]  = 4,
	[SOUND_MIXER_VOLUME] = 5,
	[SOUND_MIXER_PCM]    = 6,
	[SOUND_MIXER_LINE1]  = 7,
	[SOUND_MIXER_SPEAKER]= 8
};

static unsigned mixer_outmask(struct cm_state *s)
{
	unsigned long flags;
	int i, j, k;

	spin_lock_irqsave(&s->lock, flags);
	j = rdmixer(s, DSP_MIX_OUTMIXIDX);
	spin_unlock_irqrestore(&s->lock, flags);
	for (k = i = 0; i < SOUND_MIXER_NRDEVICES; i++)
		if (j & mixtable[i].play)
			k |= 1 << i;
	return k;
}

static unsigned mixer_recmask(struct cm_state *s)
{
	unsigned long flags;
	int i, j, k;

	spin_lock_irqsave(&s->lock, flags);
	j = rdmixer(s, DSP_MIX_ADCMIXIDX_L);
	spin_unlock_irqrestore(&s->lock, flags);
	for (k = i = 0; i < SOUND_MIXER_NRDEVICES; i++)
		if (j & mixtable[i].rec)
			k |= 1 << i;
	return k;
}

static int mixer_ioctl(struct cm_state *s, unsigned int cmd, unsigned long arg)
{
	unsigned long flags;
	int i, val, j;
	unsigned char l, r, rl, rr;

	VALIDATE_STATE(s);
        if (cmd == SOUND_MIXER_INFO) {
		mixer_info info;
		strncpy(info.id, "cmpci", sizeof(info.id));
		strncpy(info.name, "C-Media PCI", sizeof(info.name));
		info.modify_counter = s->mix.modcnt;
		if (copy_to_user((void *)arg, &info, sizeof(info)))
			return -EFAULT;
		return 0;
	}
	if (cmd == SOUND_OLD_MIXER_INFO) {
		_old_mixer_info info;
		strncpy(info.id, "cmpci", sizeof(info.id));
		strncpy(info.name, "C-Media cmpci", sizeof(info.name));
		if (copy_to_user((void *)arg, &info, sizeof(info)))
			return -EFAULT;
		return 0;
	}
	if (cmd == OSS_GETVERSION)
		return put_user(SOUND_VERSION, (int *)arg);
	if (_IOC_TYPE(cmd) != 'M' || _IOC_SIZE(cmd) != sizeof(int))
                return -EINVAL;
        if (_IOC_DIR(cmd) == _IOC_READ) {
                switch (_IOC_NR(cmd)) {
                case SOUND_MIXER_RECSRC: /* Arg contains a bit for each recording source */
			return put_user(mixer_recmask(s), (int *)arg);
			
                case SOUND_MIXER_OUTSRC: /* Arg contains a bit for each recording source */
			return put_user(mixer_outmask(s), (int *)arg);
			
                case SOUND_MIXER_DEVMASK: /* Arg contains a bit for each supported device */
			for (val = i = 0; i < SOUND_MIXER_NRDEVICES; i++)
				if (mixtable[i].type)
					val |= 1 << i;
			return put_user(val, (int *)arg);

                case SOUND_MIXER_RECMASK: /* Arg contains a bit for each supported recording source */
			for (val = i = 0; i < SOUND_MIXER_NRDEVICES; i++)
				if (mixtable[i].rec)
					val |= 1 << i;
			return put_user(val, (int *)arg);
			
                case SOUND_MIXER_OUTMASK: /* Arg contains a bit for each supported recording source */
			for (val = i = 0; i < SOUND_MIXER_NRDEVICES; i++)
				if (mixtable[i].play)
					val |= 1 << i;
			return put_user(val, (int *)arg);
			
                 case SOUND_MIXER_STEREODEVS: /* Mixer channels supporting stereo */
			for (val = i = 0; i < SOUND_MIXER_NRDEVICES; i++)
				if (mixtable[i].type && mixtable[i].type != MT_4MUTEMONO)
					val |= 1 << i;
			return put_user(val, (int *)arg);
			
                case SOUND_MIXER_CAPS:
			return put_user(0, (int *)arg);

		default:
			i = _IOC_NR(cmd);
                        if (i >= SOUND_MIXER_NRDEVICES || !mixtable[i].type)
                                return -EINVAL;
			if (!volidx[i])
				return -EINVAL;
			return put_user(s->mix.vol[volidx[i]-1], (int *)arg);
		}
	}
        if (_IOC_DIR(cmd) != (_IOC_READ|_IOC_WRITE)) 
		return -EINVAL;
	s->mix.modcnt++;
	switch (_IOC_NR(cmd)) {
	case SOUND_MIXER_RECSRC: /* Arg contains a bit for each recording source */
		if (get_user(val, (int *)arg))
			return -EFAULT;
		i = generic_hweight32(val);
		for (j = i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
			if (!(val & (1 << i)))
				continue;
			if (!mixtable[i].rec) {
				val &= ~(1 << i);
				continue;
			}
			j |= mixtable[i].rec;
		}
		spin_lock_irqsave(&s->lock, flags);
		wrmixer(s, DSP_MIX_ADCMIXIDX_L, j);
		wrmixer(s, DSP_MIX_ADCMIXIDX_R, (j & 1) | (j>>1) | (j & 0x80));
		spin_unlock_irqrestore(&s->lock, flags);
		return 0;

	case SOUND_MIXER_OUTSRC: /* Arg contains a bit for each recording source */
		if (get_user(val, (int *)arg))
			return -EFAULT;
		for (j = i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
			if (!(val & (1 << i)))
				continue;
			if (!mixtable[i].play) {
				val &= ~(1 << i);
				continue;
			}
			j |= mixtable[i].play;
		}
		spin_lock_irqsave(&s->lock, flags);
		wrmixer(s, DSP_MIX_OUTMIXIDX, j);
		spin_unlock_irqrestore(&s->lock, flags);
		return 0;

	default:
		i = _IOC_NR(cmd);
		if (i >= SOUND_MIXER_NRDEVICES || !mixtable[i].type)
			return -EINVAL;
		if (get_user(val, (int *)arg))
			return -EFAULT;
		l = val & 0xff;
		r = (val >> 8) & 0xff;
		if (l > 100)
			l = 100;
		if (r > 100)
			r = 100;
		spin_lock_irqsave(&s->lock, flags);
		switch (mixtable[i].type) {
		case MT_4:
			if (l >= 10)
				l -= 10;
			if (r >= 10)
				r -= 10;
			frobindir(s, mixtable[i].left, 0xf0, l / 6);
			frobindir(s, mixtable[i].right, 0xf0, l / 6);
			break;

		case MT_4MUTEMONO:
			rl = (l < 4 ? 0 : (l - 5) / 3) & 31;
			rr = (rl >> 2) & 7;
			wrmixer(s, mixtable[i].left, rl<<3);
			if (i == SOUND_MIXER_MIC)
				maskb(s->iobase + CODEC_CMI_MIXER2, ~0x0e, rr<<1);
			break;
			
		case MT_5MUTEMONO:
			r = l;
			rl = l < 4 ? 0 : (l - 5) / 3;
 			wrmixer(s, mixtable[i].left, rl<<3);
			l = rdmixer(s, DSP_MIX_OUTMIXIDX) & ~mixtable[i].play;
			rr = rl ? mixtable[i].play : 0;
			wrmixer(s, DSP_MIX_OUTMIXIDX, l | rr);
			/* for recording */
			if (i == SOUND_MIXER_MIC) {
				if (s->chip_version >= 39) {
					rr = rl >> 1;
					maskb(s->iobase + CODEC_CMI_MIXER2, ~0x0e, rr&0x07);
					frobindir(s, DSP_MIX_EXTENSION, 0x01, rr>>3);
				} else {
					rr = rl >> 2;
					maskb(s->iobase + CODEC_CMI_MIXER2, ~0x0e, rr<<1);
				}
			}
			break;
				
		case MT_5MUTE:
			rl = l < 4 ? 0 : (l - 5) / 3;
			rr = r < 4 ? 0 : (r - 5) / 3;
 			wrmixer(s, mixtable[i].left, rl<<3);
			wrmixer(s, mixtable[i].right, rr<<3);
			l = rdmixer(s, DSP_MIX_OUTMIXIDX);
			l &= ~mixtable[i].play;
			r = (rl|rr) ? mixtable[i].play : 0;
			wrmixer(s, DSP_MIX_OUTMIXIDX, l | r);
			break;

		case MT_6MUTE:
			if (l < 6)
				rl = 0x00;
			else
				rl = l * 2 / 3;
			if (r < 6)
				rr = 0x00;
			else
				rr = r * 2 / 3;
			wrmixer(s, mixtable[i].left, rl);
			wrmixer(s, mixtable[i].right, rr);
			break;
		}
		spin_unlock_irqrestore(&s->lock, flags);

		if (!volidx[i])
			return -EINVAL;
		s->mix.vol[volidx[i]-1] = val;
		return put_user(s->mix.vol[volidx[i]-1], (int *)arg);
	}
}

/* --------------------------------------------------------------------- */

static int cm_open_mixdev(struct inode *inode, struct file *file)
{
	int minor = MINOR(inode->i_rdev);
	struct list_head *list;
	struct cm_state *s;

	for (list = devs.next; ; list = list->next) {
		if (list == &devs)
			return -ENODEV;
		s = list_entry(list, struct cm_state, devs);
		if (s->dev_mixer == minor)
			break;
	}
       	VALIDATE_STATE(s);
	file->private_data = s;
	return 0;
}

static int cm_release_mixdev(struct inode *inode, struct file *file)
{
	struct cm_state *s = (struct cm_state *)file->private_data;
	
	VALIDATE_STATE(s);
	return 0;
}

static int cm_ioctl_mixdev(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	return mixer_ioctl((struct cm_state *)file->private_data, cmd, arg);
}

static /*const*/ struct file_operations cm_mixer_fops = {
	owner:		THIS_MODULE,
	llseek:		no_llseek,
	ioctl:		cm_ioctl_mixdev,
	open:		cm_open_mixdev,
	release:	cm_release_mixdev,
};


/* --------------------------------------------------------------------- */

static int drain_dac(struct cm_state *s, int nonblock)
{
	DECLARE_WAITQUEUE(wait, current);
	unsigned long flags;
	int count, tmo;

	if (s->dma_dac.mapped || !s->dma_dac.ready)
		return 0;
        add_wait_queue(&s->dma_dac.wait, &wait);
        for (;;) {
        	__set_current_state(TASK_INTERRUPTIBLE);
                spin_lock_irqsave(&s->lock, flags);
		count = s->dma_dac.count;
                spin_unlock_irqrestore(&s->lock, flags);
		if (count <= 0)
			break;
		if (signal_pending(current))
                        break;
                if (nonblock) {
                        remove_wait_queue(&s->dma_dac.wait, &wait);
                        set_current_state(TASK_RUNNING);
                        return -EBUSY;
                }
		tmo = 3 * HZ * (count + s->dma_dac.fragsize) / 2 / s->ratedac;
		tmo >>= sample_shift[(s->fmt >> CM_CFMT_DACSHIFT) & CM_CFMT_MASK];
		if (!schedule_timeout(tmo + 1))
			printk(KERN_DEBUG "cmpci: dma timed out??\n");
        }
        remove_wait_queue(&s->dma_dac.wait, &wait);
        set_current_state(TASK_RUNNING);
        if (signal_pending(current))
                return -ERESTARTSYS;
        return 0;
}

/* --------------------------------------------------------------------- */

static ssize_t cm_read(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	struct cm_state *s = (struct cm_state *)file->private_data;
	DECLARE_WAITQUEUE(wait, current);
	ssize_t ret;
	unsigned long flags;
	unsigned swptr;
	int cnt;

	VALIDATE_STATE(s);
	if (ppos != &file->f_pos)
		return -ESPIPE;
	if (s->dma_adc.mapped)
		return -ENXIO;
	if (!s->dma_adc.ready && (ret = prog_dmabuf(s, 1)))
		return ret;
	if (!access_ok(VERIFY_WRITE, buffer, count))
		return -EFAULT;
	ret = 0;

        add_wait_queue(&s->dma_adc.wait, &wait);
	while (count > 0) {
		spin_lock_irqsave(&s->lock, flags);
		swptr = s->dma_adc.swptr;
		cnt = s->dma_adc.dmasize-swptr;
		if (s->dma_adc.count < cnt)
			cnt = s->dma_adc.count;
		if (cnt <= 0)
			__set_current_state(TASK_INTERRUPTIBLE);
		spin_unlock_irqrestore(&s->lock, flags);
		if (cnt > count)
			cnt = count;
		if (cnt <= 0) {
			start_adc(s);
			if (file->f_flags & O_NONBLOCK) {
				if (!ret)
					ret = -EAGAIN;
				goto out;
			}
			if (!schedule_timeout(HZ)) {
				printk(KERN_DEBUG "cmpci: read: chip lockup? dmasz %u fragsz %u count %i hwptr %u swptr %u\n",
				       s->dma_adc.dmasize, s->dma_adc.fragsize, s->dma_adc.count,
				       s->dma_adc.hwptr, s->dma_adc.swptr);
				spin_lock_irqsave(&s->lock, flags);
				stop_adc_unlocked(s);
				set_dmaadc(s, s->dma_adc.rawphys, s->dma_adc.dmasamples);
				/* program sample counts */
				set_countadc(s, s->dma_adc.fragsamples);
				s->dma_adc.count = s->dma_adc.hwptr = s->dma_adc.swptr = 0;
				spin_unlock_irqrestore(&s->lock, flags);
			}
			if (signal_pending(current)) {
				if (!ret)
					ret = -ERESTARTSYS;
				goto out;
			}
			continue;
		}
		if (copy_to_user(buffer, s->dma_adc.rawbuf + swptr, cnt)) {
			if (!ret)
				ret = -EFAULT;
			goto out;
		}
		swptr = (swptr + cnt) % s->dma_adc.dmasize;
		spin_lock_irqsave(&s->lock, flags);
		s->dma_adc.swptr = swptr;
		s->dma_adc.count -= cnt;
		count -= cnt;
		buffer += cnt;
		ret += cnt;
		start_adc_unlocked(s);
		spin_unlock_irqrestore(&s->lock, flags);
	}
out:
	remove_wait_queue(&s->dma_adc.wait, &wait);
	set_current_state(TASK_RUNNING);
	return ret;
}

static ssize_t cm_write(struct file *file, const char *buffer, size_t count, loff_t *ppos)
{
	struct cm_state *s = (struct cm_state *)file->private_data;
	DECLARE_WAITQUEUE(wait, current);
	ssize_t ret;
	unsigned long flags;
	unsigned swptr;
	int cnt;

	VALIDATE_STATE(s);
	if (ppos != &file->f_pos)
		return -ESPIPE;
	if (s->dma_dac.mapped)
		return -ENXIO;
	if (!s->dma_dac.ready && (ret = prog_dmabuf(s, 0)))
		return ret;
	if (!access_ok(VERIFY_READ, buffer, count))
		return -EFAULT;
	if (s->status & DO_DUAL_DAC) {
		if (s->dma_adc.mapped)
			return -ENXIO;
		if (!s->dma_adc.ready && (ret = prog_dmabuf(s, 1)))
			return ret;
	}
	if (!access_ok(VERIFY_READ, buffer, count))
		return -EFAULT;
	ret = 0;

        add_wait_queue(&s->dma_dac.wait, &wait);
	while (count > 0) {
		spin_lock_irqsave(&s->lock, flags);
		if (s->dma_dac.count < 0) {
			s->dma_dac.count = 0;
			s->dma_dac.swptr = s->dma_dac.hwptr;
		}
		if (s->status & DO_DUAL_DAC) {
			s->dma_adc.swptr = s->dma_dac.swptr;
			s->dma_adc.count = s->dma_dac.count;
			s->dma_adc.endcleared = s->dma_dac.endcleared;
		}
		swptr = s->dma_dac.swptr;
		cnt = s->dma_dac.dmasize-swptr;
		if (s->status & DO_AC3_SW) {
			if (s->dma_dac.count + 2 * cnt > s->dma_dac.dmasize)
				cnt = (s->dma_dac.dmasize - s->dma_dac.count) / 2;
		} else {
			if (s->dma_dac.count + cnt > s->dma_dac.dmasize)
				cnt = s->dma_dac.dmasize - s->dma_dac.count;
		}
		if (cnt <= 0)
			__set_current_state(TASK_INTERRUPTIBLE);
		spin_unlock_irqrestore(&s->lock, flags);
		if (cnt > count)
			cnt = count;
		if ((s->status & DO_DUAL_DAC) && (cnt > count / 2))
		    cnt = count / 2;
		if (cnt <= 0) {
			start_dac(s);
			if (file->f_flags & O_NONBLOCK) {
				if (!ret)
					ret = -EAGAIN;
				goto out;
			}
			if (!schedule_timeout(HZ)) {
				printk(KERN_DEBUG "cmpci: write: chip lockup? dmasz %u fragsz %u count %i hwptr %u swptr %u\n",
				       s->dma_dac.dmasize, s->dma_dac.fragsize, s->dma_dac.count,
				       s->dma_dac.hwptr, s->dma_dac.swptr);
				spin_lock_irqsave(&s->lock, flags);
				stop_dac_unlocked(s);
				set_dmadac(s, s->dma_dac.rawphys, s->dma_dac.dmasamples);
				/* program sample counts */
				set_countdac(s, s->dma_dac.fragsamples);
				s->dma_dac.count = s->dma_dac.hwptr = s->dma_dac.swptr = 0;
				if (s->status & DO_DUAL_DAC)  {
					set_dmadac1(s, s->dma_adc.rawphys, s->dma_adc.dmasamples);
					s->dma_adc.count = s->dma_adc.hwptr = s->dma_adc.swptr = 0;
				}
				spin_unlock_irqrestore(&s->lock, flags);
			}
			if (signal_pending(current)) {
				if (!ret)
					ret = -ERESTARTSYS;
				goto out;
			}
			continue;
		}
		if (s->status & DO_AC3_SW) {
			// clip exceeded data, caught by 033 and 037
			if (swptr + 2 * cnt > s->dma_dac.dmasize)
				cnt = (s->dma_dac.dmasize - swptr) / 2;
			if ((ret = trans_ac3(s, s->dma_dac.rawbuf + swptr, buffer, cnt)))
				goto out;
			swptr = (swptr + 2 * cnt) % s->dma_dac.dmasize;
		} else if (s->status & DO_DUAL_DAC) {
			int	i;
			unsigned long *src, *dst0, *dst1;

			src = (unsigned long *) buffer;
			dst0 = (unsigned long *) (s->dma_dac.rawbuf + swptr);
			dst1 = (unsigned long *) (s->dma_adc.rawbuf + swptr);
			// copy left/right sample at one time
			for (i = 0; i <= cnt / 4; i++) {
				if ((ret = __get_user(*dst0++, src++)))
					goto out;
				if ((ret = __get_user(*dst1++, src++)))
					goto out;
			}
			swptr = (swptr + cnt) % s->dma_dac.dmasize;
		} else {
			if (copy_from_user(s->dma_dac.rawbuf + swptr, buffer, cnt)) {
				if (!ret)
					ret = -EFAULT;
				goto out;
			}
			swptr = (swptr + cnt) % s->dma_dac.dmasize;
		}
		spin_lock_irqsave(&s->lock, flags);
		s->dma_dac.swptr = swptr;
		s->dma_dac.count += cnt;
		if (s->status & DO_AC3_SW)
			s->dma_dac.count += cnt;
		s->dma_dac.endcleared = 0;
		spin_unlock_irqrestore(&s->lock, flags);
		count -= cnt;
		buffer += cnt;
		ret += cnt;
		if (s->status & DO_DUAL_DAC) {
			count -= cnt;
			buffer += cnt;
			ret += cnt;
		}
		start_dac(s);
	}
out:
	remove_wait_queue(&s->dma_dac.wait, &wait);
	set_current_state(TASK_RUNNING);
	return ret;
}

static unsigned int cm_poll(struct file *file, struct poll_table_struct *wait)
{
	struct cm_state *s = (struct cm_state *)file->private_data;
	unsigned long flags;
	unsigned int mask = 0;

	VALIDATE_STATE(s);
	if (file->f_mode & FMODE_WRITE)
		poll_wait(file, &s->dma_dac.wait, wait);
	if (file->f_mode & FMODE_READ)
		poll_wait(file, &s->dma_adc.wait, wait);
	spin_lock_irqsave(&s->lock, flags);
	cm_update_ptr(s);
	if (file->f_mode & FMODE_READ) {
		if (s->dma_adc.count >= (signed)s->dma_adc.fragsize)
			mask |= POLLIN | POLLRDNORM;
	}
	if (file->f_mode & FMODE_WRITE) {
		if (s->dma_dac.mapped) {
			if (s->dma_dac.count >= (signed)s->dma_dac.fragsize) 
				mask |= POLLOUT | POLLWRNORM;
		} else {
			if ((signed)s->dma_dac.dmasize >= s->dma_dac.count + (signed)s->dma_dac.fragsize)
				mask |= POLLOUT | POLLWRNORM;
		}
	}
	spin_unlock_irqrestore(&s->lock, flags);
	return mask;
}

static int cm_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct cm_state *s = (struct cm_state *)file->private_data;
	struct dmabuf *db;
	int ret = -EINVAL;
	unsigned long size;

	VALIDATE_STATE(s);
	lock_kernel();
	if (vma->vm_flags & VM_WRITE) {
		if ((ret = prog_dmabuf(s, 0)) != 0)
			goto out;
		db = &s->dma_dac;
	} else if (vma->vm_flags & VM_READ) {
		if ((ret = prog_dmabuf(s, 1)) != 0)
			goto out;
		db = &s->dma_adc;
	} else
		goto out;
	ret = -EINVAL;
	if (vma->vm_pgoff != 0)
		goto out;
	size = vma->vm_end - vma->vm_start;
	if (size > (PAGE_SIZE << db->buforder))
		goto out;
	ret = -EINVAL;
	if (remap_page_range(vma->vm_start, virt_to_phys(db->rawbuf), size, vma->vm_page_prot))
		goto out;
	db->mapped = 1;
	ret = 0;
out:
	unlock_kernel();
	return ret;
}

#define SNDCTL_SPDIF_COPYRIGHT	_SIOW('S',  0, int)       // set/reset S/PDIF copy protection
#define SNDCTL_SPDIF_LOOP	_SIOW('S',  1, int)       // set/reset S/PDIF loop
#define SNDCTL_SPDIF_MONITOR	_SIOW('S',  2, int)       // set S/PDIF monitor
#define SNDCTL_SPDIF_LEVEL	_SIOW('S',  3, int)       // set/reset S/PDIF out level
#define SNDCTL_SPDIF_INV	_SIOW('S',  4, int)       // set/reset S/PDIF in inverse
#define SNDCTL_SPDIF_SEL2	_SIOW('S',  5, int)       // set S/PDIF in #2
#define SNDCTL_SPDIF_VALID	_SIOW('S',  6, int)       // set S/PDIF valid
#define SNDCTL_SPDIFOUT		_SIOW('S',  7, int)       // set S/PDIF out
#define SNDCTL_SPDIFIN		_SIOW('S',  8, int)       // set S/PDIF out

static int cm_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct cm_state *s = (struct cm_state *)file->private_data;
	unsigned long flags;
        audio_buf_info abinfo;
        count_info cinfo;
	int val, mapped, ret;
	unsigned char fmtm, fmtd;

	VALIDATE_STATE(s);
        mapped = ((file->f_mode & FMODE_WRITE) && s->dma_dac.mapped) ||
		((file->f_mode & FMODE_READ) && s->dma_adc.mapped);
	switch (cmd) {
	case OSS_GETVERSION:
		return put_user(SOUND_VERSION, (int *)arg);

	case SNDCTL_DSP_SYNC:
		if (file->f_mode & FMODE_WRITE)
			return drain_dac(s, 0/*file->f_flags & O_NONBLOCK*/);
		return 0;
		
	case SNDCTL_DSP_SETDUPLEX:
		return 0;

	case SNDCTL_DSP_GETCAPS:
		return put_user(DSP_CAP_DUPLEX | DSP_CAP_REALTIME | DSP_CAP_TRIGGER | DSP_CAP_MMAP | DSP_CAP_BIND, (int *)arg);
		
        case SNDCTL_DSP_RESET:
		if (file->f_mode & FMODE_WRITE) {
			stop_dac(s);
			synchronize_irq();
			s->dma_dac.swptr = s->dma_dac.hwptr = s->dma_dac.count = s->dma_dac.total_bytes = 0;
			if (s->status & DO_DUAL_DAC)
				s->dma_adc.swptr = s->dma_adc.hwptr = s->dma_adc.count = s->dma_adc.total_bytes = 0;
		}
		if (file->f_mode & FMODE_READ) {
			stop_adc(s);
			synchronize_irq();
			s->dma_adc.swptr = s->dma_adc.hwptr = s->dma_adc.count = s->dma_adc.total_bytes = 0;
		}
		return 0;

        case SNDCTL_DSP_SPEED:
		if (get_user(val, (int *)arg))
			return -EFAULT;
		if (val >= 0) {
			if (file->f_mode & FMODE_READ) {
			 	spin_lock_irqsave(&s->lock, flags);
				stop_adc_unlocked(s);
				s->dma_adc.ready = 0;
				set_adc_rate_unlocked(s, val);
				spin_unlock_irqrestore(&s->lock, flags);
			}
			if (file->f_mode & FMODE_WRITE) {
				stop_dac(s);
				s->dma_dac.ready = 0;
				if (s->status & DO_DUAL_DAC)
					s->dma_adc.ready = 0;
				set_dac_rate(s, val);
			}
		}
		return put_user((file->f_mode & FMODE_READ) ? s->rateadc : s->ratedac, (int *)arg);

        case SNDCTL_DSP_STEREO:
		if (get_user(val, (int *)arg))
			return -EFAULT;
		fmtd = 0;
		fmtm = ~0;
		if (file->f_mode & FMODE_READ) {
			stop_adc(s);
			s->dma_adc.ready = 0;
			if (val)
				fmtd |= CM_CFMT_STEREO << CM_CFMT_ADCSHIFT;
			else
				fmtm &= ~(CM_CFMT_STEREO << CM_CFMT_ADCSHIFT);
		}
		if (file->f_mode & FMODE_WRITE) {
			stop_dac(s);
			s->dma_dac.ready = 0;
			if (val)
				fmtd |= CM_CFMT_STEREO << CM_CFMT_DACSHIFT;
			else
				fmtm &= ~(CM_CFMT_STEREO << CM_CFMT_DACSHIFT);
			if (s->status & DO_DUAL_DAC) {
				s->dma_adc.ready = 0;
				if (val)
					fmtd |= CM_CFMT_STEREO << CM_CFMT_ADCSHIFT;
				else
					fmtm &= ~(CM_CFMT_STEREO << CM_CFMT_ADCSHIFT);
			}
		}
		set_fmt(s, fmtm, fmtd);
		return 0;

        case SNDCTL_DSP_CHANNELS:
		if (get_user(val, (int *)arg))
			return -EFAULT;
		if (val != 0) {
			fmtd = 0;
			fmtm = ~0;
			if (file->f_mode & FMODE_READ) {
				stop_adc(s);
				s->dma_adc.ready = 0;
				if (val >= 2)
					fmtd |= CM_CFMT_STEREO << CM_CFMT_ADCSHIFT;
				else
					fmtm &= ~(CM_CFMT_STEREO << CM_CFMT_ADCSHIFT);
			}
			if (file->f_mode & FMODE_WRITE) {
				stop_dac(s);
				s->dma_dac.ready = 0;
				if (val >= 2)
					fmtd |= CM_CFMT_STEREO << CM_CFMT_DACSHIFT;
				else
					fmtm &= ~(CM_CFMT_STEREO << CM_CFMT_DACSHIFT);
				if (s->status & DO_DUAL_DAC) {
					s->dma_adc.ready = 0;
					if (val >= 2)
						fmtd |= CM_CFMT_STEREO << CM_CFMT_ADCSHIFT;
					else
						fmtm &= ~(CM_CFMT_STEREO << CM_CFMT_ADCSHIFT);
				}
			}
			set_fmt(s, fmtm, fmtd);
			if ((s->capability & CAN_MULTI_CH)
			     && (file->f_mode & FMODE_WRITE)) {
				val = set_dac_channels(s, val);
				return put_user(val, (int *)arg);
			}
		}            
		return put_user((s->fmt & ((file->f_mode & FMODE_READ) ? (CM_CFMT_STEREO << CM_CFMT_ADCSHIFT) 
					   : (CM_CFMT_STEREO << CM_CFMT_DACSHIFT))) ? 2 : 1, (int *)arg);
		
	case SNDCTL_DSP_GETFMTS: /* Returns a mask */
                return put_user(AFMT_S16_LE|AFMT_U8|AFMT_AC3, (int *)arg);
		
	case SNDCTL_DSP_SETFMT: /* Selects ONE fmt*/
		if (get_user(val, (int *)arg))
			return -EFAULT;
		if (val != AFMT_QUERY) {
			fmtd = 0;
			fmtm = ~0;
			if (file->f_mode & FMODE_READ) {
				stop_adc(s);
				s->dma_adc.ready = 0;
				if (val == AFMT_S16_LE)
					fmtd |= CM_CFMT_16BIT << CM_CFMT_ADCSHIFT;
				else
					fmtm &= ~(CM_CFMT_16BIT << CM_CFMT_ADCSHIFT);
			}
			if (file->f_mode & FMODE_WRITE) {
				stop_dac(s);
				s->dma_dac.ready = 0;
				if (val == AFMT_S16_LE || val == AFMT_AC3)
					fmtd |= CM_CFMT_16BIT << CM_CFMT_DACSHIFT;
				else
					fmtm &= ~(CM_CFMT_16BIT << CM_CFMT_DACSHIFT);
				if (val == AFMT_AC3) {
					fmtd |= CM_CFMT_STEREO << CM_CFMT_DACSHIFT;
					set_ac3(s, s->ratedac);
				} else
					set_ac3(s, 0);
				if (s->status & DO_DUAL_DAC) {
					s->dma_adc.ready = 0;
					if (val == AFMT_S16_LE)
						fmtd |= CM_CFMT_STEREO << CM_CFMT_ADCSHIFT;
					else
						fmtm &= ~(CM_CFMT_STEREO << CM_CFMT_ADCSHIFT);
				}
			}
			set_fmt(s, fmtm, fmtd);
		}
		if (s->status & DO_AC3) return put_user(AFMT_AC3, (int *)arg);
		return put_user((s->fmt & ((file->f_mode & FMODE_READ) ? (CM_CFMT_16BIT << CM_CFMT_ADCSHIFT)
					   : (CM_CFMT_16BIT << CM_CFMT_DACSHIFT))) ? AFMT_S16_LE : AFMT_U8, (int *)arg);

	case SNDCTL_DSP_POST:
                return 0;

        case SNDCTL_DSP_GETTRIGGER:
		val = 0;
		if (s->status & DO_DUAL_DAC) {
			if (file->f_mode & FMODE_WRITE &&
			 (s->enable & ENDAC) &&
			 (s->enable & ENADC))
				val |= PCM_ENABLE_OUTPUT;
			return put_user(val, (int *)arg);
		}
		if (file->f_mode & FMODE_READ && s->enable & ENADC) 
			val |= PCM_ENABLE_INPUT;
		if (file->f_mode & FMODE_WRITE && s->enable & ENDAC) 
			val |= PCM_ENABLE_OUTPUT;
		return put_user(val, (int *)arg);

	case SNDCTL_DSP_SETTRIGGER:
		if (get_user(val, (int *)arg))
			return -EFAULT;
		if (file->f_mode & FMODE_READ) {
			if (val & PCM_ENABLE_INPUT) {
				if (!s->dma_adc.ready && (ret =  prog_dmabuf(s, 1)))
					return ret;
				start_adc(s);
			} else
				stop_adc(s);
		}
		if (file->f_mode & FMODE_WRITE) {
			if (val & PCM_ENABLE_OUTPUT) {
				if (!s->dma_dac.ready && (ret = prog_dmabuf(s, 0)))
					return ret;
				if (s->status & DO_DUAL_DAC) {
					if (!s->dma_adc.ready && (ret = prog_dmabuf(s, 1)))
						return ret;
				}
				start_dac(s);
			} else
				stop_dac(s);
		}
		return 0;

	case SNDCTL_DSP_GETOSPACE:
		if (!(file->f_mode & FMODE_WRITE))
			return -EINVAL;
		if (!(s->enable & ENDAC) && (val = prog_dmabuf(s, 0)) != 0)
			return val;
		spin_lock_irqsave(&s->lock, flags);
		cm_update_ptr(s);
		abinfo.fragsize = s->dma_dac.fragsize;
                abinfo.bytes = s->dma_dac.dmasize - s->dma_dac.count;
                abinfo.fragstotal = s->dma_dac.numfrag;
                abinfo.fragments = abinfo.bytes >> s->dma_dac.fragshift;
		spin_unlock_irqrestore(&s->lock, flags);
		return copy_to_user((void *)arg, &abinfo, sizeof(abinfo)) ? -EFAULT : 0;

	case SNDCTL_DSP_GETISPACE:
		if (!(file->f_mode & FMODE_READ))
			return -EINVAL;
		if (!(s->enable & ENADC) && (val = prog_dmabuf(s, 1)) != 0)
			return val;
		spin_lock_irqsave(&s->lock, flags);
		cm_update_ptr(s);
		abinfo.fragsize = s->dma_adc.fragsize;
                abinfo.bytes = s->dma_adc.count;
                abinfo.fragstotal = s->dma_adc.numfrag;
                abinfo.fragments = abinfo.bytes >> s->dma_adc.fragshift;      
		spin_unlock_irqrestore(&s->lock, flags);
		return copy_to_user((void *)arg, &abinfo, sizeof(abinfo)) ? -EFAULT : 0;
		
        case SNDCTL_DSP_NONBLOCK:
                file->f_flags |= O_NONBLOCK;
                return 0;

        case SNDCTL_DSP_GETODELAY:
		if (!(file->f_mode & FMODE_WRITE))
			return -EINVAL;
		spin_lock_irqsave(&s->lock, flags);
		cm_update_ptr(s);
                val = s->dma_dac.count;
		spin_unlock_irqrestore(&s->lock, flags);
		return put_user(val, (int *)arg);

        case SNDCTL_DSP_GETIPTR:
		if (!(file->f_mode & FMODE_READ))
			return -EINVAL;
		spin_lock_irqsave(&s->lock, flags);
		cm_update_ptr(s);
                cinfo.bytes = s->dma_adc.total_bytes;
                cinfo.blocks = s->dma_adc.count >> s->dma_adc.fragshift;
                cinfo.ptr = s->dma_adc.hwptr;
		if (s->dma_adc.mapped)
			s->dma_adc.count &= s->dma_adc.fragsize-1;
		spin_unlock_irqrestore(&s->lock, flags);
                return copy_to_user((void *)arg, &cinfo, sizeof(cinfo))  ? -EFAULT : 0;

        case SNDCTL_DSP_GETOPTR:
		if (!(file->f_mode & FMODE_WRITE))
			return -EINVAL;
		spin_lock_irqsave(&s->lock, flags);
		cm_update_ptr(s);
                cinfo.bytes = s->dma_dac.total_bytes;
                cinfo.blocks = s->dma_dac.count >> s->dma_dac.fragshift;
                cinfo.ptr = s->dma_dac.hwptr;
		if (s->dma_dac.mapped)
			s->dma_dac.count &= s->dma_dac.fragsize-1;
		if (s->status & DO_DUAL_DAC) {
			if (s->dma_adc.mapped)
				s->dma_adc.count &= s->dma_adc.fragsize-1;
		}
		spin_unlock_irqrestore(&s->lock, flags);
                return copy_to_user((void *)arg, &cinfo, sizeof(cinfo)) ? -EFAULT : 0;

        case SNDCTL_DSP_GETBLKSIZE:
		if (file->f_mode & FMODE_WRITE) {
			if ((val = prog_dmabuf(s, 0)))
				return val;
			if (s->status & DO_DUAL_DAC) {
				if ((val = prog_dmabuf(s, 1)))
					return val;
				return put_user(2 * s->dma_dac.fragsize, (int *)arg);
			}
			return put_user(s->dma_dac.fragsize, (int *)arg);
		}
		if ((val = prog_dmabuf(s, 1)))
			return val;
		return put_user(s->dma_adc.fragsize, (int *)arg);

        case SNDCTL_DSP_SETFRAGMENT:
		if (get_user(val, (int *)arg))
			return -EFAULT;
		if (file->f_mode & FMODE_READ) {
			s->dma_adc.ossfragshift = val & 0xffff;
			s->dma_adc.ossmaxfrags = (val >> 16) & 0xffff;
			if (s->dma_adc.ossfragshift < 4)
				s->dma_adc.ossfragshift = 4;
			if (s->dma_adc.ossfragshift > 15)
				s->dma_adc.ossfragshift = 15;
			if (s->dma_adc.ossmaxfrags < 4)
				s->dma_adc.ossmaxfrags = 4;
		}
		if (file->f_mode & FMODE_WRITE) {
			s->dma_dac.ossfragshift = val & 0xffff;
			s->dma_dac.ossmaxfrags = (val >> 16) & 0xffff;
			if (s->dma_dac.ossfragshift < 4)
				s->dma_dac.ossfragshift = 4;
			if (s->dma_dac.ossfragshift > 15)
				s->dma_dac.ossfragshift = 15;
			if (s->dma_dac.ossmaxfrags < 4)
				s->dma_dac.ossmaxfrags = 4;
			if (s->status & DO_DUAL_DAC) {
				s->dma_adc.ossfragshift = s->dma_dac.ossfragshift;
				s->dma_adc.ossmaxfrags = s->dma_dac.ossmaxfrags;
			}
		}
		return 0;

        case SNDCTL_DSP_SUBDIVIDE:
		if ((file->f_mode & FMODE_READ && s->dma_adc.subdivision) ||
		    (file->f_mode & FMODE_WRITE && s->dma_dac.subdivision))
			return -EINVAL;
		if (get_user(val, (int *)arg))
			return -EFAULT;
		if (val != 1 && val != 2 && val != 4)
			return -EINVAL;
		if (file->f_mode & FMODE_READ)
			s->dma_adc.subdivision = val;
		if (file->f_mode & FMODE_WRITE) {
			s->dma_dac.subdivision = val;
			if (s->status & DO_DUAL_DAC)
				s->dma_adc.subdivision = val;
		}
		return 0;

        case SOUND_PCM_READ_RATE:
		return put_user((file->f_mode & FMODE_READ) ? s->rateadc : s->ratedac, (int *)arg);

        case SOUND_PCM_READ_CHANNELS:
		return put_user((s->fmt & ((file->f_mode & FMODE_READ) ? (CM_CFMT_STEREO << CM_CFMT_ADCSHIFT) : (CM_CFMT_STEREO << CM_CFMT_DACSHIFT))) ? 2 : 1, (int *)arg);

        case SOUND_PCM_READ_BITS:
		return put_user((s->fmt & ((file->f_mode & FMODE_READ) ? (CM_CFMT_16BIT << CM_CFMT_ADCSHIFT) : (CM_CFMT_16BIT << CM_CFMT_DACSHIFT))) ? 16 : 8, (int *)arg);

        case SOUND_PCM_READ_FILTER:
		return put_user((file->f_mode & FMODE_READ) ? s->rateadc : s->ratedac, (int *)arg);

	case SNDCTL_DSP_GETCHANNELMASK:
		return put_user(DSP_BIND_FRONT|DSP_BIND_SURR|DSP_BIND_CENTER_LFE|DSP_BIND_SPDIF, (int *)arg);
		
	case SNDCTL_DSP_BIND_CHANNEL:
		if (get_user(val, (int *)arg))
			return -EFAULT;
		if (val == DSP_BIND_QUERY) {
			val = DSP_BIND_FRONT;
			if (s->status & DO_SPDIF_OUT)
				val |= DSP_BIND_SPDIF;
			else {
				if (s->curr_channels == 4)
					val |= DSP_BIND_SURR;
				if (s->curr_channels > 4)
					val |= DSP_BIND_CENTER_LFE;
			}
		} else {
			if (file->f_mode & FMODE_READ) {
				stop_adc(s);
				s->dma_adc.ready = 0;
				if (val & DSP_BIND_SPDIF) {
					set_spdifin(s, s->rateadc);
					if (!(s->status & DO_SPDIF_OUT))
						val &= ~DSP_BIND_SPDIF;
				}
			}
			if (file->f_mode & FMODE_WRITE) {
				stop_dac(s);
				s->dma_dac.ready = 0;
				if (val & DSP_BIND_SPDIF) {
					set_spdifout(s, s->ratedac);
					set_dac_channels(s, s->fmt & (CM_CFMT_STEREO << CM_CFMT_DACSHIFT) ? 2 : 1);
					if (!(s->status & DO_SPDIF_OUT))
						val &= ~DSP_BIND_SPDIF;
				} else {
					int channels;
					int mask;

					mask = val & (DSP_BIND_FRONT|DSP_BIND_SURR|DSP_BIND_CENTER_LFE);
					switch (mask) {
					    case DSP_BIND_FRONT:
						channels = 2;
						break;
					    case DSP_BIND_FRONT|DSP_BIND_SURR:
						channels = 4;
						break;
					    case DSP_BIND_FRONT|DSP_BIND_SURR|DSP_BIND_CENTER_LFE:
						channels = 6;
						break;
					    default:
						channels = s->fmt & (CM_CFMT_STEREO << CM_CFMT_DACSHIFT) ? 2 : 1;
						break;
					}
					set_dac_channels(s, channels);
				}
			}
		}
		return put_user(val, (int *)arg);

	case SOUND_PCM_WRITE_FILTER:
	case SNDCTL_DSP_MAPINBUF:
	case SNDCTL_DSP_MAPOUTBUF:
        case SNDCTL_DSP_SETSYNCRO:
                return -EINVAL;
	case SNDCTL_SPDIF_COPYRIGHT:
		if (get_user(val, (int *)arg))
			return -EFAULT;
		set_spdif_copyright(s, val);
                return 0;
	case SNDCTL_SPDIF_LOOP:
		if (get_user(val, (int *)arg))
			return -EFAULT;
		set_spdif_loop(s, val);
                return 0;
	case SNDCTL_SPDIF_MONITOR:
		if (get_user(val, (int *)arg))
			return -EFAULT;
		set_spdif_monitor(s, val);
                return 0;
	case SNDCTL_SPDIF_LEVEL:
		if (get_user(val, (int *)arg))
			return -EFAULT;
		set_spdifout_level(s, val);
                return 0;
	case SNDCTL_SPDIF_INV:
		if (get_user(val, (int *)arg))
			return -EFAULT;
		set_spdifin_inverse(s, val);
                return 0;
	case SNDCTL_SPDIF_SEL2:
		if (get_user(val, (int *)arg))
			return -EFAULT;
		set_spdifin_channel2(s, val);
                return 0;
	case SNDCTL_SPDIF_VALID:
		if (get_user(val, (int *)arg))
			return -EFAULT;
		set_spdifin_valid(s, val);
                return 0;
	case SNDCTL_SPDIFOUT:
		if (get_user(val, (int *)arg))
			return -EFAULT;
		set_spdifout(s, val ? s->ratedac : 0);
                return 0;
	case SNDCTL_SPDIFIN:
		if (get_user(val, (int *)arg))
			return -EFAULT;
		set_spdifin(s, val ? s->rateadc : 0);
                return 0;
	}
	return mixer_ioctl(s, cmd, arg);
}

static int cm_open(struct inode *inode, struct file *file)
{
	int minor = MINOR(inode->i_rdev);
	DECLARE_WAITQUEUE(wait, current);
	unsigned char fmtm = ~0, fmts = 0;
	struct list_head *list;
	struct cm_state *s;

	for (list = devs.next; ; list = list->next) {
		if (list == &devs)
			return -ENODEV;
		s = list_entry(list, struct cm_state, devs);
		if (!((s->dev_audio ^ minor) & ~0xf))
			break;
	}
       	VALIDATE_STATE(s);
	file->private_data = s;
	/* wait for device to become free */
	down(&s->open_sem);
	while (s->open_mode & file->f_mode) {
		if (file->f_flags & O_NONBLOCK) {
			up(&s->open_sem);
			return -EBUSY;
		}
		add_wait_queue(&s->open_wait, &wait);
		__set_current_state(TASK_INTERRUPTIBLE);
		up(&s->open_sem);
		schedule();
		remove_wait_queue(&s->open_wait, &wait);
		set_current_state(TASK_RUNNING);
		if (signal_pending(current))
			return -ERESTARTSYS;
		down(&s->open_sem);
	}
	if (file->f_mode & FMODE_READ) {
		fmtm &= ~((CM_CFMT_STEREO | CM_CFMT_16BIT) << CM_CFMT_ADCSHIFT);
		if ((minor & 0xf) == SND_DEV_DSP16)
			fmts |= CM_CFMT_16BIT << CM_CFMT_ADCSHIFT;
		s->dma_adc.ossfragshift = s->dma_adc.ossmaxfrags = s->dma_adc.subdivision = 0;
		set_adc_rate(s, 8000);
		// spdif-in is turnned off by default
		set_spdifin(s, 0);
	}
	if (file->f_mode & FMODE_WRITE) {
		fmtm &= ~((CM_CFMT_STEREO | CM_CFMT_16BIT) << CM_CFMT_DACSHIFT);
		if ((minor & 0xf) == SND_DEV_DSP16)
			fmts |= CM_CFMT_16BIT << CM_CFMT_DACSHIFT;
		s->dma_dac.ossfragshift = s->dma_dac.ossmaxfrags = s->dma_dac.subdivision = 0;
		set_dac_rate(s, 8000);
		// clear previous multichannel, spdif, ac3 state
		set_spdifout(s, 0);
		if (s->deviceid == PCI_DEVICE_ID_CMEDIA_CM8738) {
			set_ac3(s, 0);
			set_dac_channels(s, 1);
		}
	}
	set_fmt(s, fmtm, fmts);
	s->open_mode |= file->f_mode & (FMODE_READ | FMODE_WRITE);
	up(&s->open_sem);
	return 0;
}

static int cm_release(struct inode *inode, struct file *file)
{
	struct cm_state *s = (struct cm_state *)file->private_data;

	VALIDATE_STATE(s);
	lock_kernel();
	if (file->f_mode & FMODE_WRITE)
		drain_dac(s, file->f_flags & O_NONBLOCK);
	down(&s->open_sem);
	if (file->f_mode & FMODE_WRITE) {
		stop_dac(s);

		dealloc_dmabuf(&s->dma_dac);
		if (s->status & DO_DUAL_DAC)
			dealloc_dmabuf(&s->dma_adc);

		if (s->status & DO_MULTI_CH)
			set_dac_channels(s, 0);
		if (s->status & DO_AC3)
			set_ac3(s, 0);
		if (s->status & DO_SPDIF_OUT)
			set_spdifout(s, 0);
	}
	if (file->f_mode & FMODE_READ) {
		stop_adc(s);
		dealloc_dmabuf(&s->dma_adc);
	}
	s->open_mode &= ~(file->f_mode & (FMODE_READ|FMODE_WRITE));
	up(&s->open_sem);
	wake_up(&s->open_wait);
	unlock_kernel();
	return 0;
}

static /*const*/ struct file_operations cm_audio_fops = {
	owner:		THIS_MODULE,
	llseek:		no_llseek,
	read:		cm_read,
	write:		cm_write,
	poll:		cm_poll,
	ioctl:		cm_ioctl,
	mmap:		cm_mmap,
	open:		cm_open,
	release:	cm_release,
};

#ifdef CONFIG_SOUND_CMPCI_MIDI
/* --------------------------------------------------------------------- */

static ssize_t cm_midi_read(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	struct cm_state *s = (struct cm_state *)file->private_data;
	DECLARE_WAITQUEUE(wait, current);
	ssize_t ret;
	unsigned long flags;
	unsigned ptr;
	int cnt;

	VALIDATE_STATE(s);
	if (ppos != &file->f_pos)
		return -ESPIPE;
	if (!access_ok(VERIFY_WRITE, buffer, count))
		return -EFAULT;
	ret = 0;
	add_wait_queue(&s->midi.iwait, &wait);
	while (count > 0) {
		spin_lock_irqsave(&s->lock, flags);
		ptr = s->midi.ird;
		cnt = MIDIINBUF - ptr;
		if (s->midi.icnt < cnt)
			cnt = s->midi.icnt;
		if (cnt <= 0)
			__set_current_state(TASK_INTERRUPTIBLE);
		spin_unlock_irqrestore(&s->lock, flags);
		if (cnt > count)
			cnt = count;
		if (cnt <= 0) {
			if (file->f_flags & O_NONBLOCK)
			{
				if (!ret)
					ret = -EAGAIN;
				break;
			}
			schedule();
			if (signal_pending(current))
			{
				if (!ret)
					ret = -ERESTARTSYS;
				break;
			}
			continue;
		}
		if (copy_to_user(buffer, s->midi.ibuf + ptr, cnt))
		{
			if (!ret)
				ret = -EFAULT;
			break;
		}
		ptr = (ptr + cnt) % MIDIINBUF;
		spin_lock_irqsave(&s->lock, flags);
		s->midi.ird = ptr;
		s->midi.icnt -= cnt;
		spin_unlock_irqrestore(&s->lock, flags);
		count -= cnt;
		buffer += cnt;
		ret += cnt;
		break;
	}
	__set_current_state(TASK_RUNNING);
	remove_wait_queue(&s->midi.iwait, &wait);
	return ret;
}

static ssize_t cm_midi_write(struct file *file, const char *buffer, size_t count, loff_t *ppos)
{
	struct cm_state *s = (struct cm_state *)file->private_data;
	DECLARE_WAITQUEUE(wait, current);
	ssize_t ret;
	unsigned long flags;
	unsigned ptr;
	int cnt;

	VALIDATE_STATE(s);
	if (ppos != &file->f_pos)
		return -ESPIPE;
	if (!access_ok(VERIFY_READ, buffer, count))
		return -EFAULT;
	if (count == 0)
		return 0;
	ret = 0;
	add_wait_queue(&s->midi.owait, &wait);
	while (count > 0) {
		spin_lock_irqsave(&s->lock, flags);
		ptr = s->midi.owr;
		cnt = MIDIOUTBUF - ptr;
		if (s->midi.ocnt + cnt > MIDIOUTBUF)
			cnt = MIDIOUTBUF - s->midi.ocnt;
		if (cnt <= 0) {
			__set_current_state(TASK_INTERRUPTIBLE);
			cm_handle_midi(s);
		}
		spin_unlock_irqrestore(&s->lock, flags);
		if (cnt > count)
			cnt = count;
		if (cnt <= 0) {
			if (file->f_flags & O_NONBLOCK)
			{
				if (!ret)
					ret = -EAGAIN;
				break;
			}
			schedule();
			if (signal_pending(current)) {
				if (!ret)
					ret = -ERESTARTSYS;
				break;
			}
			continue;
		}
		if (copy_from_user(s->midi.obuf + ptr, buffer, cnt))
		{
			if (!ret)
				ret = -EFAULT;
			break;
		}
		ptr = (ptr + cnt) % MIDIOUTBUF;
		spin_lock_irqsave(&s->lock, flags);
		s->midi.owr = ptr;
		s->midi.ocnt += cnt;
		spin_unlock_irqrestore(&s->lock, flags);
		count -= cnt;
		buffer += cnt;
		ret += cnt;
		spin_lock_irqsave(&s->lock, flags);
		cm_handle_midi(s);
		spin_unlock_irqrestore(&s->lock, flags);
	}
	__set_current_state(TASK_RUNNING);
	remove_wait_queue(&s->midi.owait, &wait);
	return ret;
}

static unsigned int cm_midi_poll(struct file *file, struct poll_table_struct *wait)
{
	struct cm_state *s = (struct cm_state *)file->private_data;
	unsigned long flags;
	unsigned int mask = 0;

	VALIDATE_STATE(s);
	if (file->f_mode & FMODE_WRITE)
		poll_wait(file, &s->midi.owait, wait);
	if (file->f_mode & FMODE_READ)
		poll_wait(file, &s->midi.iwait, wait);
	spin_lock_irqsave(&s->lock, flags);
	if (file->f_mode & FMODE_READ) {
		if (s->midi.icnt > 0)
			mask |= POLLIN | POLLRDNORM;
	}
	if (file->f_mode & FMODE_WRITE) {
		if (s->midi.ocnt < MIDIOUTBUF)
			mask |= POLLOUT | POLLWRNORM;
	}
	spin_unlock_irqrestore(&s->lock, flags);
	return mask;
}

static int cm_midi_open(struct inode *inode, struct file *file)
{
	int minor = MINOR(inode->i_rdev);
	DECLARE_WAITQUEUE(wait, current);
	unsigned long flags;
	struct list_head *list;
	struct cm_state *s;

	for (list = devs.next; ; list = list->next) {
		if (list == &devs)
			return -ENODEV;
		s = list_entry(list, struct cm_state, devs);
		if (s->dev_midi == minor)
			break;
	}
       	VALIDATE_STATE(s);
	file->private_data = s;
	/* wait for device to become free */
	down(&s->open_sem);
	while (s->open_mode & (file->f_mode << FMODE_MIDI_SHIFT)) {
		if (file->f_flags & O_NONBLOCK) {
			up(&s->open_sem);
			return -EBUSY;
		}
 		add_wait_queue(&s->open_wait, &wait);
 		__set_current_state(TASK_INTERRUPTIBLE);
		up(&s->open_sem);
 		schedule();
 		remove_wait_queue(&s->open_wait, &wait);
 		set_current_state(TASK_RUNNING);
		if (signal_pending(current))
			return -ERESTARTSYS;
		down(&s->open_sem);
	}
	spin_lock_irqsave(&s->lock, flags);
	if (!(s->open_mode & (FMODE_MIDI_READ | FMODE_MIDI_WRITE))) {
		s->midi.ird = s->midi.iwr = s->midi.icnt = 0;
		s->midi.ord = s->midi.owr = s->midi.ocnt = 0;
		/* enable MPU-401 */
		maskb(s->iobase + CODEC_CMI_FUNCTRL1, ~0, UART_EN);
		outb(0xff, s->iomidi+1); /* reset command */
		if (!(inb(s->iomidi+1) & 0x80))
			inb(s->iomidi);
		outb(0x3f, s->iomidi+1); /* uart command */
		if (!(inb(s->iomidi+1) & 0x80))
			inb(s->iomidi);
		s->midi.ird = s->midi.iwr = s->midi.icnt = 0;
		init_timer(&s->midi.timer);
		s->midi.timer.expires = jiffies+1;
		s->midi.timer.data = (unsigned long)s;
		s->midi.timer.function = cm_midi_timer;
		add_timer(&s->midi.timer);
	}
	if (file->f_mode & FMODE_READ) {
		s->midi.ird = s->midi.iwr = s->midi.icnt = 0;
	}
	if (file->f_mode & FMODE_WRITE) {
		s->midi.ord = s->midi.owr = s->midi.ocnt = 0;
	}
	spin_unlock_irqrestore(&s->lock, flags);
	s->open_mode |= (file->f_mode << FMODE_MIDI_SHIFT) & (FMODE_MIDI_READ | FMODE_MIDI_WRITE);
	up(&s->open_sem);
	MOD_INC_USE_COUNT;
	return 0;
}

static int cm_midi_release(struct inode *inode, struct file *file)
{
	struct cm_state *s = (struct cm_state *)file->private_data;
	DECLARE_WAITQUEUE(wait, current);
	unsigned long flags;
	unsigned count, tmo;

	VALIDATE_STATE(s);
	lock_kernel();

	if (file->f_mode & FMODE_WRITE) {
		__set_current_state(TASK_INTERRUPTIBLE);
		add_wait_queue(&s->midi.owait, &wait);
		for (;;) {
			spin_lock_irqsave(&s->lock, flags);
			count = s->midi.ocnt;
			spin_unlock_irqrestore(&s->lock, flags);
			if (count <= 0)
				break;
			if (signal_pending(current))
				break;
			if (file->f_flags & O_NONBLOCK) {
				remove_wait_queue(&s->midi.owait, &wait);
				set_current_state(TASK_RUNNING);
				unlock_kernel();
				return -EBUSY;
			}
			tmo = (count * HZ) / 3100;
			if (!schedule_timeout(tmo ? : 1) && tmo)
				printk(KERN_DEBUG "cmpci: midi timed out??\n");
		}
		remove_wait_queue(&s->midi.owait, &wait);
		set_current_state(TASK_RUNNING);
	}
	down(&s->open_sem);
	s->open_mode &= ~((file->f_mode << FMODE_MIDI_SHIFT) & (FMODE_MIDI_READ|FMODE_MIDI_WRITE));
	spin_lock_irqsave(&s->lock, flags);
	if (!(s->open_mode & (FMODE_MIDI_READ | FMODE_MIDI_WRITE))) {
		del_timer(&s->midi.timer);		
		outb(0xff, s->iomidi+1); /* reset command */
		if (!(inb(s->iomidi+1) & 0x80))
			inb(s->iomidi);
		/* disable MPU-401 */
		maskb(s->iobase + CODEC_CMI_FUNCTRL1, ~UART_EN, 0);
	}
	spin_unlock_irqrestore(&s->lock, flags);
	up(&s->open_sem);
	wake_up(&s->open_wait);
	unlock_kernel();
	return 0;
}

static /*const*/ struct file_operations cm_midi_fops = {
	owner:		THIS_MODULE,
	llseek:		no_llseek,
	read:		cm_midi_read,
	write:		cm_midi_write,
	poll:		cm_midi_poll,
	open:		cm_midi_open,
	release:	cm_midi_release,
};
#endif

/* --------------------------------------------------------------------- */

#ifdef CONFIG_SOUND_CMPCI_FM
static int cm_dmfm_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	static const unsigned char op_offset[18] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
		0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
		0x10, 0x11, 0x12, 0x13, 0x14, 0x15
	};
	struct cm_state *s = (struct cm_state *)file->private_data;
	struct dm_fm_voice v;
	struct dm_fm_note n;
	struct dm_fm_params p;
	unsigned int io;
	unsigned int regb;

	switch (cmd) {		
	case FM_IOCTL_RESET:
		for (regb = 0xb0; regb < 0xb9; regb++) {
			outb(regb, s->iosynth);
			outb(0, s->iosynth+1);
			outb(regb, s->iosynth+2);
			outb(0, s->iosynth+3);
		}
		return 0;

	case FM_IOCTL_PLAY_NOTE:
		if (copy_from_user(&n, (void *)arg, sizeof(n)))
			return -EFAULT;
		if (n.voice >= 18)
			return -EINVAL;
		if (n.voice >= 9) {
			regb = n.voice - 9;
			io = s->iosynth+2;
		} else {
			regb = n.voice;
			io = s->iosynth;
		}
		outb(0xa0 + regb, io);
		outb(n.fnum & 0xff, io+1);
		outb(0xb0 + regb, io);
		outb(((n.fnum >> 8) & 3) | ((n.octave & 7) << 2) | ((n.key_on & 1) << 5), io+1);
		return 0;

	case FM_IOCTL_SET_VOICE:
		if (copy_from_user(&v, (void *)arg, sizeof(v)))
			return -EFAULT;
		if (v.voice >= 18)
			return -EINVAL;
		regb = op_offset[v.voice];
		io = s->iosynth + ((v.op & 1) << 1);
		outb(0x20 + regb, io);
		outb(((v.am & 1) << 7) | ((v.vibrato & 1) << 6) | ((v.do_sustain & 1) << 5) | 
		     ((v.kbd_scale & 1) << 4) | (v.harmonic & 0xf), io+1);
		outb(0x40 + regb, io);
		outb(((v.scale_level & 0x3) << 6) | (v.volume & 0x3f), io+1);
		outb(0x60 + regb, io);
		outb(((v.attack & 0xf) << 4) | (v.decay & 0xf), io+1);
		outb(0x80 + regb, io);
		outb(((v.sustain & 0xf) << 4) | (v.release & 0xf), io+1);
		outb(0xe0 + regb, io);
		outb(v.waveform & 0x7, io+1);
		if (n.voice >= 9) {
			regb = n.voice - 9;
			io = s->iosynth+2;
		} else {
			regb = n.voice;
			io = s->iosynth;
		}
		outb(0xc0 + regb, io);
		outb(((v.right & 1) << 5) | ((v.left & 1) << 4) | ((v.feedback & 7) << 1) |
		     (v.connection & 1), io+1);
		return 0;
		
	case FM_IOCTL_SET_PARAMS:
		if (copy_from_user(&p, (void *)arg, sizeof(p)))
			return -EFAULT;
		outb(0x08, s->iosynth);
		outb((p.kbd_split & 1) << 6, s->iosynth+1);
		outb(0xbd, s->iosynth);
		outb(((p.am_depth & 1) << 7) | ((p.vib_depth & 1) << 6) | ((p.rhythm & 1) << 5) | ((p.bass & 1) << 4) |
		     ((p.snare & 1) << 3) | ((p.tomtom & 1) << 2) | ((p.cymbal & 1) << 1) | (p.hihat & 1), s->iosynth+1);
		return 0;

	case FM_IOCTL_SET_OPL:
		outb(4, s->iosynth+2);
		outb(arg, s->iosynth+3);
		return 0;

	case FM_IOCTL_SET_MODE:
		outb(5, s->iosynth+2);
		outb(arg & 1, s->iosynth+3);
		return 0;
	}
	return -EINVAL;
}

static int cm_dmfm_open(struct inode *inode, struct file *file)
{
	int minor = MINOR(inode->i_rdev);
	DECLARE_WAITQUEUE(wait, current);
	struct list_head *list;
	struct cm_state *s;

	for (list = devs.next; ; list = list->next) {
		if (list == &devs)
			return -ENODEV;
		s = list_entry(list, struct cm_state, devs);
		if (s->dev_dmfm == minor)
			break;
	}
       	VALIDATE_STATE(s);
	file->private_data = s;
	/* wait for device to become free */
	down(&s->open_sem);
	while (s->open_mode & FMODE_DMFM) {
		if (file->f_flags & O_NONBLOCK) {
			up(&s->open_sem);
			return -EBUSY;
		}
		add_wait_queue(&s->open_wait, &wait);
		__set_current_state(TASK_INTERRUPTIBLE);
		up(&s->open_sem);
		schedule();
		remove_wait_queue(&s->open_wait, &wait);
		set_current_state(TASK_RUNNING);
		if (signal_pending(current))
			return -ERESTARTSYS;
		down(&s->open_sem);
	}
	/* init the stuff */
	outb(1, s->iosynth);
	outb(0x20, s->iosynth+1); /* enable waveforms */
	outb(4, s->iosynth+2);
	outb(0, s->iosynth+3);  /* no 4op enabled */
	outb(5, s->iosynth+2);
	outb(1, s->iosynth+3);  /* enable OPL3 */
	s->open_mode |= FMODE_DMFM;
	up(&s->open_sem);
	MOD_INC_USE_COUNT;
	return 0;
}

static int cm_dmfm_release(struct inode *inode, struct file *file)
{
	struct cm_state *s = (struct cm_state *)file->private_data;
	unsigned int regb;

	VALIDATE_STATE(s);
	lock_kernel();
	down(&s->open_sem);
	s->open_mode &= ~FMODE_DMFM;
	for (regb = 0xb0; regb < 0xb9; regb++) {
		outb(regb, s->iosynth);
		outb(0, s->iosynth+1);
		outb(regb, s->iosynth+2);
		outb(0, s->iosynth+3);
	}
	up(&s->open_sem);
	wake_up(&s->open_wait);
	unlock_kernel();
	return 0;
}

static /*const*/ struct file_operations cm_dmfm_fops = {
	owner:		THIS_MODULE,
	llseek:		no_llseek,
	ioctl:		cm_dmfm_ioctl,
	open:		cm_dmfm_open,
	release:	cm_dmfm_release,
};
#endif /* CONFIG_SOUND_CMPCI_FM */



static struct initvol {
	int mixch;
	int vol;
} initvol[] __initdata = {
	{ SOUND_MIXER_WRITE_CD, 0x4f4f },
	{ SOUND_MIXER_WRITE_LINE, 0x4f4f },
	{ SOUND_MIXER_WRITE_MIC, 0x4f4f },
	{ SOUND_MIXER_WRITE_SYNTH, 0x4f4f },
	{ SOUND_MIXER_WRITE_VOLUME, 0x4f4f },
	{ SOUND_MIXER_WRITE_PCM, 0x4f4f }
};

/* check chip version and capability */
static int query_chip(struct cm_state *s)
{
	int ChipVersion = -1;
	unsigned char RegValue;

	// check reg 0Ch, bit 24-31
	RegValue = inb(s->iobase + CODEC_CMI_INT_HLDCLR + 3);
	if (RegValue == 0) {
	    // check reg 08h, bit 24-28
	    RegValue = inb(s->iobase + CODEC_CMI_CHFORMAT + 3);
	    RegValue &= 0x1f;
	    if (RegValue == 0) {
		ChipVersion = 33;
		s->max_channels = 4;
		s->capability |= CAN_AC3_SW;
		s->capability |= CAN_DUAL_DAC;
	    } else {
		ChipVersion = 37;
		s->max_channels = 4;
		s->capability |= CAN_AC3_HW;
		s->capability |= CAN_DUAL_DAC;
	    }
	} else {
	    // check reg 0Ch, bit 26
	    if (RegValue & (1 << (26-24))) {
		ChipVersion = 39;
	    	if (RegValue & (1 << (24-24)))
		    s->max_channels  = 6;
	    	else
		    s->max_channels = 4;
		s->capability |= CAN_AC3_HW;
		s->capability |= CAN_DUAL_DAC;
		s->capability |= CAN_MULTI_CH_HW;
	    } else {
		ChipVersion = 55; // 4 or 6 channels
		s->max_channels  = 6;
		s->capability |= CAN_AC3_HW;
		s->capability |= CAN_DUAL_DAC;
		s->capability |= CAN_MULTI_CH_HW;
	    }
	}
	// still limited to number of speakers
	if (s->max_channels > s->speakers)
		s->max_channels = s->speakers;
	return ChipVersion;
}

#ifdef CONFIG_SOUND_CMPCI_MIDI
static	int	mpuio = CONFIG_SOUND_CMPCI_MPUIO;
#else
static	int	mpuio;
#endif
#ifdef CONFIG_SOUND_CMPCI_FM
static	int	fmio = CONFIG_SOUND_CMPCI_FMIO;
#else
static	int	fmio;
#endif
#ifdef CONFIG_SOUND_CMPCI_SPDIFINVERSE
static	int	spdif_inverse = 1;
#else
static	int	spdif_inverse;
#endif
#ifdef CONFIG_SOUND_CMPCI_SPDIFLOOP
static	int	spdif_loop = 1;
#else
static	int	spdif_loop;
#endif
#ifdef CONFIG_SOUND_CMPCI_SPEAKERS
static	int	speakers = CONFIG_SOUND_CMPCI_SPEAKERS;
#else
static	int	speakers = 2;
#endif
#ifdef CONFIG_SOUND_CMPCI_LINE_REAR
static	int	use_line_as_rear = 1;
#else
static	int	use_line_as_rear;
#endif
#ifdef CONFIG_SOUND_CMPCI_LINE_BASS
static	int	use_line_as_bass = 1;
#else
static	int	use_line_as_bass;
#endif
#ifdef CONFIG_SOUND_CMPCI_MIC_BASS
static	int	use_mic_as_bass = 1;
#else
static	int	use_mic_as_bass = 0;
#endif
#ifdef CONFIG_SOUND_CMPCI_JOYSTICK
static	int	joystick = 1;
#else
static	int	joystick;
#endif
static	int	mic_boost = 0;
MODULE_PARM(mpuio, "i");
MODULE_PARM(fmio, "i");
MODULE_PARM(spdif_inverse, "i");
MODULE_PARM(spdif_loop, "i");
MODULE_PARM(speakers, "i");
MODULE_PARM(use_line_as_rear, "i");
MODULE_PARM(use_line_as_bass, "i");
MODULE_PARM(use_mic_as_bass, "i");
MODULE_PARM(joystick, "i");
MODULE_PARM(mic_boost, "i");
MODULE_PARM_DESC(mpuio, "(0x330, 0x320, 0x310, 0x300) Base of MPU-401, 0 to disable");
MODULE_PARM_DESC(fmio, "(0x388, 0x3C8, 0x3E0) Base of OPL3, 0 to disable");
MODULE_PARM_DESC(spdif_inverse, "(1/0) Invert S/PDIF-in signal");
MODULE_PARM_DESC(spdif_loop, "(1/0) Route S/PDIF-in to S/PDIF-out directly");
MODULE_PARM_DESC(speakers, "(2-6) Number of speakers you connect");
MODULE_PARM_DESC(use_line_as_rear, "(1/0) Use line-in jack as rear-out");
MODULE_PARM_DESC(use_line_as_bass, "(1/0) Use line-in jack as bass/center");
MODULE_PARM_DESC(use_mic_as_bass, "(1/0) Use mic-in jack as bass/center");
MODULE_PARM_DESC(joystick, "(1/0) Enable joystick interface, still need joystick driver");
MODULE_PARM_DESC(mic_boost, "(1/0) Enable microphone boost");

static int __devinit cm_probe(struct pci_dev *pcidev, const struct pci_device_id *pciid)
{
	struct cm_state *s;
	mm_segment_t fs;
	int i, val, ret;
	unsigned char reg_mask = 0;
	struct {
		unsigned short	deviceid;
		char		*devicename;
	} devicetable[] =
	{
		{ PCI_DEVICE_ID_CMEDIA_CM8338A, "CM8338A" },
		{ PCI_DEVICE_ID_CMEDIA_CM8338B, "CM8338B" },
		{ PCI_DEVICE_ID_CMEDIA_CM8738,  "CM8738" },
		{ PCI_DEVICE_ID_CMEDIA_CM8738B, "CM8738B" },
	};
	char	*devicename = "unknown";
	if ((ret = pci_enable_device(pcidev)))
		return ret;
	if (pcidev->irq == 0)
		return -ENODEV;
	s = kmalloc(sizeof(*s), GFP_KERNEL);
	if (!s) {
		printk(KERN_WARNING "cmpci: out of memory\n");
		return -ENOMEM;
	}
	/* search device name */
	for (i = 0; i < sizeof(devicetable) / sizeof(devicetable[0]); i++) {
		if (devicetable[i].deviceid == pcidev->device)
		{
			devicename = devicetable[i].devicename;
			break;
		}
	}
	memset(s, 0, sizeof(struct cm_state));
	init_waitqueue_head(&s->dma_adc.wait);
	init_waitqueue_head(&s->dma_dac.wait);
	init_waitqueue_head(&s->open_wait);
	init_waitqueue_head(&s->midi.iwait);
	init_waitqueue_head(&s->midi.owait);
	init_MUTEX(&s->open_sem);
	spin_lock_init(&s->lock);
	s->magic = CM_MAGIC;
	s->iobase = pci_resource_start(pcidev, 0);
#ifdef CONFIG_IA64
	/* The IA64 quirk handler didn't fix us up */
	if (s->iobase == 0xff00)
	{
		kfree(s);
		return -ENODEV;
	}
#endif
	s->iosynth = fmio;
	s->iomidi = mpuio;
	s->gameport.io = 0x200;
	s->status = 0;
	/* range check */
	if (speakers < 2)
		speakers = 2;
	else if (speakers > 6)
		speakers = 6;
	s->speakers = speakers;
	if (s->iobase == 0)
	{
		kfree(s);
		return -ENODEV;
	}
	s->irq = pcidev->irq;

	if (!request_region(s->iobase, CM_EXTENT_CODEC, "cmpci")) {
		printk(KERN_ERR "cmpci: io ports %#x-%#x in use\n", s->iobase, s->iobase+CM_EXTENT_CODEC-1);
		goto err_region5;
	}
#ifdef CONFIG_SOUND_CMPCI_MIDI
	/* disable MPU-401 */
	maskb(s->iobase + CODEC_CMI_FUNCTRL1, ~0x04, 0);
	if (s->iomidi) {
	    if (!request_region(s->iomidi, CM_EXTENT_MIDI, "cmpci Midi")) {
		printk(KERN_ERR "cmpci: io ports %#x-%#x in use\n", s->iomidi, s->iomidi+CM_EXTENT_MIDI-1);
		s->iomidi = 0;
	    } else {
		/* set IO based at 0x330 */
		switch (s->iomidi) {
		    case 0x330:
			reg_mask = 0;
			break;
		    case 0x320:
			reg_mask = 0x20;
			break;
		    case 0x310:
			reg_mask = 0x40;
			break;
		    case 0x300:
			reg_mask = 0x60;
			break;
		    default:
			s->iomidi = 0;
			break;
		}
		outb((inb(s->iobase + CODEC_CMI_LEGACY_CTRL + 3) & ~0x60) | reg_mask, s->iobase + CODEC_CMI_LEGACY_CTRL + 3);
		/* enable MPU-401 */
		if (s->iomidi) {
		    maskb(s->iobase + CODEC_CMI_FUNCTRL1, ~0, 0x04);
		}
	    }
	}
#endif
#ifdef CONFIG_SOUND_CMPCI_FM
	/* disable FM */
	maskb(s->iobase + CODEC_CMI_MISC_CTRL + 2, ~8, 0);
	if (s->iosynth) {
	    if (!request_region(s->iosynth, CM_EXTENT_SYNTH, "cmpci FM")) {
		printk(KERN_ERR "cmpci: io ports %#x-%#x in use\n", s->iosynth, s->iosynth+CM_EXTENT_SYNTH-1);
		s->iosynth = 0;
	    } else {
		/* set IO based at 0x388 */
		switch (s->iosynth) {
		    case 0x388:
			reg_mask = 0;
			break;
		    case 0x3C8:
			reg_mask = 0x01;
			break;
		    case 0x3E0:
			reg_mask = 0x02;
			break;
		    case 0x3E8:
			reg_mask = 0x03;
			break;
		    default:
			s->iosynth = 0;
			break;
		}
		maskb(s->iobase + CODEC_CMI_LEGACY_CTRL + 3, ~0x03, reg_mask);
		/* enable FM */
		if (s->iosynth) {
		    maskb(s->iobase + CODEC_CMI_MISC_CTRL + 2, ~0, 8);
		}
	    }
	}
#endif
	/* enable joystick */
	if (joystick) {
		if (s->gameport.io && !request_region(s->gameport.io, CM_EXTENT_GAME, "cmpci GAME")) {
			printk(KERN_ERR "cmpci: gameport io ports in use\n");
			s->gameport.io = 0;
	       	} else
			maskb(s->iobase + CODEC_CMI_FUNCTRL1, ~0, 0x02);
	} else {
		maskb(s->iobase + CODEC_CMI_FUNCTRL1, ~0x02, 0);
		s->gameport.io = 0;
	}
	/* initialize codec registers */
	outb(0, s->iobase + CODEC_CMI_INT_HLDCLR + 2);  /* disable ints */
	outb(0, s->iobase + CODEC_CMI_FUNCTRL0 + 2); /* disable channels */
	/* reset mixer */
	wrmixer(s, DSP_MIX_DATARESETIDX, 0);

	/* request irq */
	if (request_irq(s->irq, cm_interrupt, SA_SHIRQ, "cmpci", s)) {
		printk(KERN_ERR "cmpci: irq %u in use\n", s->irq);
		goto err_irq;
	}
	printk(KERN_INFO "cmpci: found %s adapter at io %#x irq %u\n",
	       devicename, s->iobase, s->irq);
	/* register devices */
	if ((s->dev_audio = register_sound_dsp(&cm_audio_fops, -1)) < 0) {
		ret = s->dev_audio;
		goto err_dev1;
	}
	if ((s->dev_mixer = register_sound_mixer(&cm_mixer_fops, -1)) < 0) {
		ret = s->dev_mixer;
		goto err_dev2;
	}
#ifdef CONFIG_SOUND_CMPCI_MIDI
	if ((s->dev_midi = register_sound_midi(&cm_midi_fops, -1)) < 0) {
		ret = s->dev_midi;
		goto err_dev3;
	}
#endif
#ifdef CONFIG_SOUND_CMPCI_FM
	if ((s->dev_dmfm = register_sound_special(&cm_dmfm_fops, 15 /* ?? */)) < 0) {
		ret = s->dev_dmfm;
		goto err_dev4;
	}
#endif
	pci_set_master(pcidev);	/* enable bus mastering */
	/* initialize the chips */
	fs = get_fs();
	set_fs(KERNEL_DS);
	/* set mixer output */
	frobindir(s, DSP_MIX_OUTMIXIDX, 0x1f, 0x1f);
	/* set mixer input */
	val = SOUND_MASK_LINE|SOUND_MASK_SYNTH|SOUND_MASK_CD|SOUND_MASK_MIC;
	mixer_ioctl(s, SOUND_MIXER_WRITE_RECSRC, (unsigned long)&val);
	for (i = 0; i < sizeof(initvol)/sizeof(initvol[0]); i++) {
		val = initvol[i].vol;
		mixer_ioctl(s, initvol[i].mixch, (unsigned long)&val);
	}
	/* use channel 1 for record, channel 0 for play */
	maskb(s->iobase + CODEC_CMI_FUNCTRL0, ~0, CHADC1);
	/* turn off VMIC3 - mic boost */
	if (mic_boost)
		maskb(s->iobase + CODEC_CMI_MIXER2, ~1, 0);
	else
		maskb(s->iobase + CODEC_CMI_MIXER2, ~0, 1);
	s->deviceid = pcidev->device;

	if (pcidev->device == PCI_DEVICE_ID_CMEDIA_CM8738) {

		/* chip version and hw capability check */
		s->chip_version = query_chip(s);
		printk(KERN_INFO "cmpci: chip version = 0%d\n", s->chip_version);

		/* seet SPDIF-in inverse before enable SPDIF loop */
		set_spdifin_inverse(s, spdif_inverse);

		/* enable SPDIF loop */
		set_spdif_loop(s, spdif_loop);

		/* use SPDIF in #1 */
		set_spdifin_channel2(s, 0);

		if (use_line_as_rear) {
			s->capability |= CAN_LINE_AS_REAR;
			s->status |= DO_LINE_AS_REAR;
			maskb(s->iobase + CODEC_CMI_MIXER1, ~0, SPK4);
		} else
			maskb(s->iobase + CODEC_CMI_MIXER1, ~SPK4, 0);
		if (s->chip_version >= 39) {
			if (use_line_as_bass) {
				s->capability |= CAN_LINE_AS_BASS;
				s->status |= DO_LINE_AS_BASS;
				maskb(s->iobase + CODEC_CMI_LEGACY_CTRL + 1, ~0, CB2LIN);
			} else
				maskb(s->iobase + CODEC_CMI_LEGACY_CTRL + 1, ~CB2LIN, 0);
			if (use_mic_as_bass) {
				s->capability |= CAN_MIC_AS_BASS;
				s->status |= DO_MIC_AS_BASS;
				maskb(s->iobase + CODEC_CMI_MISC, ~0, 0x04);
			} else
				maskb(s->iobase + CODEC_CMI_MISC, ~0x04, 0);
		}
	} else {
		s->chip_version = 0;
		/* 8338 will fall here */
		s->max_channels = 2;
	}
	/* register gameport */
	if (joystick)
		gameport_register_port(&s->gameport);
	/* store it in the driver field */
	pci_set_drvdata(pcidev, s);
	/* put it into driver list */
	list_add_tail(&s->devs, &devs);
	/* increment devindex */
	if (devindex < NR_DEVICE-1)
		devindex++;
	return 0;

#ifdef CONFIG_SOUND_CMPCI_FM
	unregister_sound_special(s->dev_dmfm);
err_dev4:
#endif
#ifdef CONFIG_SOUND_CMPCI_MIDI
	unregister_sound_midi(s->dev_midi);
err_dev3:
#endif
	unregister_sound_mixer(s->dev_mixer);
err_dev2:
	unregister_sound_dsp(s->dev_audio);
err_dev1:
	printk(KERN_ERR "cmpci: cannot register misc device\n");
	free_irq(s->irq, s);
err_irq:
	if (s->gameport.io)
		release_region(s->gameport.io, CM_EXTENT_GAME);
#ifdef CONFIG_SOUND_CMPCI_FM
	if (s->iosynth) release_region(s->iosynth, CM_EXTENT_SYNTH);
#endif
#ifdef CONFIG_SOUND_CMPCI_MIDI
	if (s->iomidi) release_region(s->iomidi, CM_EXTENT_MIDI);
#endif
	release_region(s->iobase, CM_EXTENT_CODEC);
err_region5:
	kfree(s);
	return ret;
}

/* --------------------------------------------------------------------- */

MODULE_AUTHOR("ChenLi Tien, cltien@cmedia.com.tw");
MODULE_DESCRIPTION("CM8x38 Audio Driver");
MODULE_LICENSE("GPL");

static void __devinit cm_remove(struct pci_dev *dev)
{
	struct cm_state *s = pci_get_drvdata(dev);

	if (!s)
		return;
	list_del(&s->devs);
	outb(0, s->iobase + CODEC_CMI_INT_HLDCLR + 2);  /* disable ints */
	synchronize_irq();
	outb(0, s->iobase + CODEC_CMI_FUNCTRL0 + 2); /* disable channels */
	free_irq(s->irq, s);

	/* reset mixer */
	wrmixer(s, DSP_MIX_DATARESETIDX, 0);

	if (s->gameport.io) {
		gameport_unregister_port(&s->gameport);
		release_region(s->gameport.io, CM_EXTENT_GAME);
	}
	release_region(s->iobase, CM_EXTENT_CODEC);
#ifdef CONFIG_SOUND_CMPCI_MIDI
	if (s->iomidi) release_region(s->iomidi, CM_EXTENT_MIDI);
#endif
#ifdef CONFIG_SOUND_CMPCI_FM
	if (s->iosynth) release_region(s->iosynth, CM_EXTENT_SYNTH);
#endif
	unregister_sound_dsp(s->dev_audio);
	unregister_sound_mixer(s->dev_mixer);
#ifdef CONFIG_SOUND_CMPCI_MIDI
	unregister_sound_midi(s->dev_midi);
#endif
#ifdef CONFIG_SOUND_CMPCI_FM
	unregister_sound_special(s->dev_dmfm);
#endif
	kfree(s);
	pci_set_drvdata(dev, NULL);
}

static struct pci_device_id id_table[] __devinitdata = {
	{ PCI_VENDOR_ID_CMEDIA, PCI_DEVICE_ID_CMEDIA_CM8738, PCI_ANY_ID, PCI_ANY_ID, 0, 0 },
 	{ PCI_VENDOR_ID_CMEDIA, PCI_DEVICE_ID_CMEDIA_CM8338A, PCI_ANY_ID, PCI_ANY_ID, 0, 0 },
	{ PCI_VENDOR_ID_CMEDIA, PCI_DEVICE_ID_CMEDIA_CM8338B, PCI_ANY_ID, PCI_ANY_ID, 0, 0 },
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, id_table);

static struct pci_driver cm_driver = {
       name: "cmpci",
       id_table: id_table,
       probe: cm_probe,
       remove: cm_remove
};
 
static int __init init_cmpci(void)
{
	if (!pci_present())   /* No PCI bus in this machine! */
		return -ENODEV;
	printk(KERN_INFO "cmpci: version $Revision: 6.16 $ time " __TIME__ " " __DATE__ "\n");
	return pci_module_init(&cm_driver);
}

static void __exit cleanup_cmpci(void)
{
	printk(KERN_INFO "cmpci: unloading\n");
	pci_unregister_driver(&cm_driver);
}

module_init(init_cmpci);
module_exit(cleanup_cmpci);

