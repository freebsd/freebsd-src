/*
 * Copyright (c) 1999 Seigo Tanimura
 * All rights reserved.
 *
 * Portions of this source are based on cwcealdr.cpp and dhwiface.cpp in
 * cwcealdr1.zip, the sample sources by Crystal Semiconductor.
 * Copyright (c) 1996-1998 Crystal Semiconductor Corp.
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
 * $FreeBSD$
 */

#include <sys/soundcard.h>
#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/ac97.h>
#include <dev/sound/chip.h>
#include <dev/sound/pci/csareg.h>
#include <dev/sound/pci/csavar.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

/* device private data */
struct csa_info;

struct csa_chinfo {
	struct csa_info *parent;
	pcm_channel *channel;
	snd_dbuf *buffer;
	int dir;
	u_int32_t fmt;
	int dma;
};

struct csa_info {
	csa_res		res; /* resource */
	void		*ih; /* Interrupt cookie */
	bus_dma_tag_t	parent_dmat; /* DMA tag */
	struct csa_bridgeinfo *binfo; /* The state of the parent. */

	/* Contents of board's registers */
	u_long		pfie;
	u_long		pctl;
	u_long		cctl;
	struct csa_chinfo pch, rch;
};

/* -------------------------------------------------------------------- */

/* prototypes */
static int      csa_init(struct csa_info *);
static void     csa_intr(void *);
static void	csa_setplaysamplerate(csa_res *resp, u_long ulInRate);
static void	csa_setcapturesamplerate(csa_res *resp, u_long ulOutRate);
static void	csa_startplaydma(struct csa_info *csa);
static void	csa_startcapturedma(struct csa_info *csa);
static void	csa_stopplaydma(struct csa_info *csa);
static void	csa_stopcapturedma(struct csa_info *csa);
static void	csa_powerupadc(csa_res *resp);
static void	csa_powerupdac(csa_res *resp);
static int	csa_startdsp(csa_res *resp);
static int	csa_allocres(struct csa_info *scp, device_t dev);
static void	csa_releaseres(struct csa_info *scp, device_t dev);

/* talk to the codec - called from ac97.c */
static u_int32_t csa_rdcd(void *, int);
static void  	 csa_wrcd(void *, int, u_int32_t);

/* channel interface */
static void *csachan_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir);
static int csachan_setdir(void *data, int dir);
static int csachan_setformat(void *data, u_int32_t format);
static int csachan_setspeed(void *data, u_int32_t speed);
static int csachan_setblocksize(void *data, u_int32_t blocksize);
static int csachan_trigger(void *data, int go);
static int csachan_getptr(void *data);
static pcmchan_caps *csachan_getcaps(void *data);

static u_int32_t csa_playfmt[] = {
	AFMT_U8,
	AFMT_STEREO | AFMT_U8,
	AFMT_S8,
	AFMT_STEREO | AFMT_S8,
	AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE,
	AFMT_S16_BE,
	AFMT_STEREO | AFMT_S16_BE,
	0
};
static pcmchan_caps csa_playcaps = {8000, 48000, csa_playfmt, 0};

static u_int32_t csa_recfmt[] = {
	AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE,
	0
};
static pcmchan_caps csa_reccaps = {11025, 48000, csa_recfmt, 0};

static pcm_channel csa_chantemplate = {
	csachan_init,
	csachan_setdir,
	csachan_setformat,
	csachan_setspeed,
	csachan_setblocksize,
	csachan_trigger,
	csachan_getptr,
	csachan_getcaps,
};

/* -------------------------------------------------------------------- */

/* channel interface */
static void *
csachan_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir)
{
	struct csa_info *csa = devinfo;
	struct csa_chinfo *ch = (dir == PCMDIR_PLAY)? &csa->pch : &csa->rch;

	ch->parent = csa;
	ch->channel = c;
	ch->buffer = b;
	ch->buffer->bufsize = CS461x_BUFFSIZE;
	if (chn_allocbuf(ch->buffer, csa->parent_dmat) == -1) return NULL;
	return ch;
}

static int
csachan_setdir(void *data, int dir)
{
	struct csa_chinfo *ch = data;
	struct csa_info *csa = ch->parent;
	csa_res *resp;

	resp = &csa->res;

	if (dir == PCMDIR_PLAY)
		csa_writemem(resp, BA1_PBA, vtophys(ch->buffer->buf));
	else
		csa_writemem(resp, BA1_CBA, vtophys(ch->buffer->buf));
	ch->dir = dir;
	return 0;
}

static int
csachan_setformat(void *data, u_int32_t format)
{
	struct csa_chinfo *ch = data;
	struct csa_info *csa = ch->parent;
	u_long pdtc;
	csa_res *resp;

	resp = &csa->res;

	if (ch->dir == PCMDIR_REC)
		csa_writemem(resp, BA1_CIE, (csa_readmem(resp, BA1_CIE) & ~0x0000003f) | 0x00000001);
	else {
		csa->pfie = csa_readmem(resp, BA1_PFIE) & ~0x0000f03f;
		if (format & AFMT_U8 || format & AFMT_U16_LE || format & AFMT_U16_BE)
			csa->pfie |= 0x8000;
		if (format & AFMT_S16_BE || format & AFMT_U16_BE)
			csa->pfie |= 0x4000;
		if (!(format & AFMT_STEREO))
			csa->pfie |= 0x2000;
		if (format & AFMT_U8 || format & AFMT_S8)
			csa->pfie |= 0x1000;
		csa_writemem(resp, BA1_PFIE, csa->pfie);
		pdtc = csa_readmem(resp, BA1_PDTC) & ~0x000003ff;
		if ((format & AFMT_S16_BE || format & AFMT_U16_BE || format & AFMT_S16_LE || format & AFMT_U16_LE) && (format & AFMT_STEREO))
			pdtc |= 0x00f;
		else if ((format & AFMT_S16_BE || format & AFMT_U16_BE || format & AFMT_S16_LE || format & AFMT_U16_LE) || (format & AFMT_STEREO))
			pdtc |= 0x007;
		else
			pdtc |= 0x003;
		csa_writemem(resp, BA1_PDTC, pdtc);
	}
	ch->fmt = format;
	return 0;
}

static int
csachan_setspeed(void *data, u_int32_t speed)
{
	struct csa_chinfo *ch = data;
	struct csa_info *csa = ch->parent;
	csa_res *resp;

	resp = &csa->res;

	if (ch->dir == PCMDIR_PLAY)
		csa_setplaysamplerate(resp, speed);
	else if (ch->dir == PCMDIR_REC)
		csa_setcapturesamplerate(resp, speed);

	/* rec/play speeds locked together - should indicate in flags */
#if 0
	if (ch->direction == PCMDIR_PLAY) d->rec[0].speed = speed;
	else d->play[0].speed = speed;
#endif
	return speed; /* XXX calc real speed */
}

static void
csa_setplaysamplerate(csa_res *resp, u_long ulInRate)
{
	u_long ulTemp1, ulTemp2;
	u_long ulPhiIncr;
	u_long ulCorrectionPerGOF, ulCorrectionPerSec;
	u_long ulOutRate;

	ulOutRate = 48000;

	/*
	 * Compute the values used to drive the actual sample rate conversion.
	 * The following formulas are being computed, using inline assembly
	 * since we need to use 64 bit arithmetic to compute the values:
	 *
	 *     ulPhiIncr = floor((Fs,in * 2^26) / Fs,out)
	 *     ulCorrectionPerGOF = floor((Fs,in * 2^26 - Fs,out * ulPhiIncr) /
	 *                                GOF_PER_SEC)
	 *     ulCorrectionPerSec = Fs,in * 2^26 - Fs,out * phiIncr -
	 *                          GOF_PER_SEC * ulCorrectionPerGOF
	 *
	 * i.e.
	 *
	 *     ulPhiIncr:ulOther = dividend:remainder((Fs,in * 2^26) / Fs,out)
	 *     ulCorrectionPerGOF:ulCorrectionPerSec =
	 *         dividend:remainder(ulOther / GOF_PER_SEC)
	 */
	ulTemp1 = ulInRate << 16;
	ulPhiIncr = ulTemp1 / ulOutRate;
	ulTemp1 -= ulPhiIncr * ulOutRate;
	ulTemp1 <<= 10;
	ulPhiIncr <<= 10;
	ulTemp2 = ulTemp1 / ulOutRate;
	ulPhiIncr += ulTemp2;
	ulTemp1 -= ulTemp2 * ulOutRate;
	ulCorrectionPerGOF = ulTemp1 / GOF_PER_SEC;
	ulTemp1 -= ulCorrectionPerGOF * GOF_PER_SEC;
	ulCorrectionPerSec = ulTemp1;

	/*
	 * Fill in the SampleRateConverter control block.
	 */
	csa_writemem(resp, BA1_PSRC, ((ulCorrectionPerSec << 16) & 0xFFFF0000) | (ulCorrectionPerGOF & 0xFFFF));
	csa_writemem(resp, BA1_PPI, ulPhiIncr);
}

static void
csa_setcapturesamplerate(csa_res *resp, u_long ulOutRate)
{
	u_long ulPhiIncr, ulCoeffIncr, ulTemp1, ulTemp2;
	u_long ulCorrectionPerGOF, ulCorrectionPerSec, ulInitialDelay;
	u_long dwFrameGroupLength, dwCnt;
	u_long ulInRate;

	ulInRate = 48000;

	/*
	 * We can only decimate by up to a factor of 1/9th the hardware rate.
	 * Return an error if an attempt is made to stray outside that limit.
	 */
	if((ulOutRate * 9) < ulInRate)
		return;

	/*
	 * We can not capture at at rate greater than the Input Rate (48000).
	 * Return an error if an attempt is made to stray outside that limit.
	 */
	if(ulOutRate > ulInRate)
		return;

	/*
	 * Compute the values used to drive the actual sample rate conversion.
	 * The following formulas are being computed, using inline assembly
	 * since we need to use 64 bit arithmetic to compute the values:
	 *
	 *     ulCoeffIncr = -floor((Fs,out * 2^23) / Fs,in)
	 *     ulPhiIncr = floor((Fs,in * 2^26) / Fs,out)
	 *     ulCorrectionPerGOF = floor((Fs,in * 2^26 - Fs,out * ulPhiIncr) /
	 *                                GOF_PER_SEC)
	 *     ulCorrectionPerSec = Fs,in * 2^26 - Fs,out * phiIncr -
	 *                          GOF_PER_SEC * ulCorrectionPerGOF
	 *     ulInitialDelay = ceil((24 * Fs,in) / Fs,out)
	 *
	 * i.e.
	 *
	 *     ulCoeffIncr = neg(dividend((Fs,out * 2^23) / Fs,in))
	 *     ulPhiIncr:ulOther = dividend:remainder((Fs,in * 2^26) / Fs,out)
	 *     ulCorrectionPerGOF:ulCorrectionPerSec =
	 *         dividend:remainder(ulOther / GOF_PER_SEC)
	 *     ulInitialDelay = dividend(((24 * Fs,in) + Fs,out - 1) / Fs,out)
	 */
	ulTemp1 = ulOutRate << 16;
	ulCoeffIncr = ulTemp1 / ulInRate;
	ulTemp1 -= ulCoeffIncr * ulInRate;
	ulTemp1 <<= 7;
	ulCoeffIncr <<= 7;
	ulCoeffIncr += ulTemp1 / ulInRate;
	ulCoeffIncr ^= 0xFFFFFFFF;
	ulCoeffIncr++;
	ulTemp1 = ulInRate << 16;
	ulPhiIncr = ulTemp1 / ulOutRate;
	ulTemp1 -= ulPhiIncr * ulOutRate;
	ulTemp1 <<= 10;
	ulPhiIncr <<= 10;
	ulTemp2 = ulTemp1 / ulOutRate;
	ulPhiIncr += ulTemp2;
	ulTemp1 -= ulTemp2 * ulOutRate;
	ulCorrectionPerGOF = ulTemp1 / GOF_PER_SEC;
	ulTemp1 -= ulCorrectionPerGOF * GOF_PER_SEC;
	ulCorrectionPerSec = ulTemp1;
	ulInitialDelay = ((ulInRate * 24) + ulOutRate - 1) / ulOutRate;

	/*
	 * Fill in the VariDecimate control block.
	 */
	csa_writemem(resp, BA1_CSRC,
		     ((ulCorrectionPerSec << 16) & 0xFFFF0000) | (ulCorrectionPerGOF & 0xFFFF));
	csa_writemem(resp, BA1_CCI, ulCoeffIncr);
	csa_writemem(resp, BA1_CD,
	     (((BA1_VARIDEC_BUF_1 + (ulInitialDelay << 2)) << 16) & 0xFFFF0000) | 0x80);
	csa_writemem(resp, BA1_CPI, ulPhiIncr);

	/*
	 * Figure out the frame group length for the write back task.  Basically,
	 * this is just the factors of 24000 (2^6*3*5^3) that are not present in
	 * the output sample rate.
	 */
	dwFrameGroupLength = 1;
	for(dwCnt = 2; dwCnt <= 64; dwCnt *= 2)
	{
		if(((ulOutRate / dwCnt) * dwCnt) !=
		   ulOutRate)
		{
			dwFrameGroupLength *= 2;
		}
	}
	if(((ulOutRate / 3) * 3) !=
	   ulOutRate)
	{
		dwFrameGroupLength *= 3;
	}
	for(dwCnt = 5; dwCnt <= 125; dwCnt *= 5)
	{
		if(((ulOutRate / dwCnt) * dwCnt) !=
		   ulOutRate)
		{
			dwFrameGroupLength *= 5;
		}
	}

	/*
	 * Fill in the WriteBack control block.
	 */
	csa_writemem(resp, BA1_CFG1, dwFrameGroupLength);
	csa_writemem(resp, BA1_CFG2, (0x00800000 | dwFrameGroupLength));
	csa_writemem(resp, BA1_CCST, 0x0000FFFF);
	csa_writemem(resp, BA1_CSPB, ((65536 * ulOutRate) / 24000));
	csa_writemem(resp, (BA1_CSPB + 4), 0x0000FFFF);
}

static int
csachan_setblocksize(void *data, u_int32_t blocksize)
{
#if notdef
	return blocksize;
#else
	struct csa_chinfo *ch = data;
	return ch->buffer->bufsize / 2;
#endif /* notdef */
}

static int
csachan_trigger(void *data, int go)
{
	struct csa_chinfo *ch = data;
	struct csa_info *csa = ch->parent;

	if (go == PCMTRIG_EMLDMAWR || go == PCMTRIG_EMLDMARD)
		return 0;

	if (ch->dir == PCMDIR_PLAY) {
		if (go == PCMTRIG_START)
			csa_startplaydma(csa);
		else
			csa_stopplaydma(csa);
	} else {
		if (go == PCMTRIG_START)
			csa_startcapturedma(csa);
		else
			csa_stopcapturedma(csa);
	}
	return 0;
}

static void
csa_startplaydma(struct csa_info *csa)
{
	csa_res *resp;
	u_long ul;

	if (!csa->pch.dma) {
		resp = &csa->res;
		ul = csa_readmem(resp, BA1_PCTL);
		ul &= 0x0000ffff;
		csa_writemem(resp, BA1_PCTL, ul | csa->pctl);
		csa_writemem(resp, BA1_PVOL, 0x80008000);
		csa->pch.dma = 1;
	}
}

static void
csa_startcapturedma(struct csa_info *csa)
{
	csa_res *resp;
	u_long ul;

	if (!csa->rch.dma) {
		resp = &csa->res;
		ul = csa_readmem(resp, BA1_CCTL);
		ul &= 0xffff0000;
		csa_writemem(resp, BA1_CCTL, ul | csa->cctl);
		csa_writemem(resp, BA1_CVOL, 0x80008000);
		csa->rch.dma = 1;
	}
}

static void
csa_stopplaydma(struct csa_info *csa)
{
	csa_res *resp;
	u_long ul;

	if (csa->pch.dma) {
		resp = &csa->res;
		ul = csa_readmem(resp, BA1_PCTL);
		csa->pctl = ul & 0xffff0000;
		csa_writemem(resp, BA1_PCTL, ul & 0x0000ffff);
		csa_writemem(resp, BA1_PVOL, 0xffffffff);
		csa->pch.dma = 0;

		/*
		 * The bitwise pointer of the serial FIFO in the DSP
		 * seems to make an error upon starting or stopping the
		 * DSP. Clear the FIFO and correct the pointer if we
		 * are not capturing.
		 */
		if (!csa->rch.dma) {
			csa_clearserialfifos(resp);
			csa_writeio(resp, BA0_SERBSP, 0);
		}
	}
}

static void
csa_stopcapturedma(struct csa_info *csa)
{
	csa_res *resp;
	u_long ul;

	if (csa->rch.dma) {
		resp = &csa->res;
		ul = csa_readmem(resp, BA1_CCTL);
		csa->cctl = ul & 0x0000ffff;
		csa_writemem(resp, BA1_CCTL, ul & 0xffff0000);
		csa_writemem(resp, BA1_CVOL, 0xffffffff);
		csa->rch.dma = 0;

		/*
		 * The bitwise pointer of the serial FIFO in the DSP
		 * seems to make an error upon starting or stopping the
		 * DSP. Clear the FIFO and correct the pointer if we
		 * are not playing.
		 */
		if (!csa->pch.dma) {
			csa_clearserialfifos(resp);
			csa_writeio(resp, BA0_SERBSP, 0);
		}
	}
}

static void
csa_powerupdac(csa_res *resp)
{
	int i;
	u_long ul;

	/*
	 * Power on the DACs on the AC97 codec.  We turn off the DAC
	 * powerdown bit and write the new value of the power control
	 * register.
	 */
	ul = csa_readio(resp, BA0_AC97_POWERDOWN);
	ul &= 0xfdff;
	csa_writeio(resp, BA0_AC97_POWERDOWN, ul);

	/*
	 * Now, we wait until we sample a DAC ready state.
	 */
	for (i = 0 ; i < 32 ; i++) {
		/*
		 * First, lets wait a short while to let things settle out a
		 * bit, and to prevent retrying the read too quickly.
		 */
		DELAY(125);

		/*
		 * Read the current state of the power control register.
		 */
		ul = csa_readio(resp, BA0_AC97_POWERDOWN);

		/*
		 * If the DAC ready state bit is set, then stop waiting.
		 */
		if ((ul & 0x2) != 0)
			break;
	}
	/*
	 * The DACs are now calibrated, so we can unmute the DAC output.
	 */
	csa_writeio(resp, BA0_AC97_PCM_OUT_VOLUME, 0x0808);
}

static void
csa_powerupadc(csa_res *resp)
{
	int i;
	u_long ul;

	/*
	 * Power on the ADCs on the AC97 codec.  We turn off the ADC
	 * powerdown bit and write the new value of the power control
	 * register.
	 */
	ul = csa_readio(resp, BA0_AC97_POWERDOWN);
	ul &= 0xfeff;
	csa_writeio(resp, BA0_AC97_POWERDOWN, ul);

	/*
	 * Now, we wait until we sample a ADC ready state.
	 */
	for (i = 0 ; i < 32 ; i++) {
		/*
		 * First, lets wait a short while to let things settle out a
		 * bit, and to prevent retrying the read too quickly.
		 */
		DELAY(125);

		/*
		 * Read the current state of the power control register.
		 */
		ul = csa_readio(resp, BA0_AC97_POWERDOWN);

		/*
		 * If the ADC ready state bit is set, then stop waiting.
		 */
		if ((ul & 0x1) != 0)
			break;
	}
}

static int
csa_startdsp(csa_res *resp)
{
	int i;
	u_long ul;

	/*
	 * Set the frame timer to reflect the number of cycles per frame.
	 */
	csa_writemem(resp, BA1_FRMT, 0xadf);

	/*
	 * Turn on the run, run at frame, and DMA enable bits in the local copy of
	 * the SP control register.
	 */
	csa_writemem(resp, BA1_SPCR, SPCR_RUN | SPCR_RUNFR | SPCR_DRQEN);

	/*
	 * Wait until the run at frame bit resets itself in the SP control
	 * register.
	 */
	ul = 0;
	for (i = 0 ; i < 25 ; i++) {
		/*
		 * Wait a little bit, so we don't issue PCI reads too frequently.
		 */
#if notdef
		DELAY(1000);
#else
		DELAY(125);
#endif /* notdef */
		/*
		 * Fetch the current value of the SP status register.
		 */
		ul = csa_readmem(resp, BA1_SPCR);

		/*
		 * If the run at frame bit has reset, then stop waiting.
		 */
		if((ul & SPCR_RUNFR) == 0)
			break;
	}
	/*
	 * If the run at frame bit never reset, then return an error.
	 */
	if((ul & SPCR_RUNFR) != 0)
		return (EAGAIN);

	return (0);
}

static int
csachan_getptr(void *data)
{
	struct csa_chinfo *ch = data;
	struct csa_info *csa = ch->parent;
	csa_res *resp;
	int ptr;

	resp = &csa->res;

	if (ch->dir == PCMDIR_PLAY) {
		ptr = csa_readmem(resp, BA1_PBA) - vtophys(ch->buffer->buf);
		if ((ch->fmt & AFMT_U8) != 0 || (ch->fmt & AFMT_S8) != 0)
			ptr >>= 1;
	} else {
		ptr = csa_readmem(resp, BA1_CBA) - vtophys(ch->buffer->buf);
		if ((ch->fmt & AFMT_U8) != 0 || (ch->fmt & AFMT_S8) != 0)
			ptr >>= 1;
	}

	return (ptr);
}

static pcmchan_caps *
csachan_getcaps(void *data)
{
	struct csa_chinfo *ch = data;
	return (ch->dir == PCMDIR_PLAY)? &csa_playcaps : &csa_reccaps;
}

/* The interrupt handler */
static void
csa_intr (void *p)
{
	struct csa_info *csa = p;

	if ((csa->binfo->hisr & HISR_VC0) != 0)
		chn_intr(csa->pch.channel);
	if ((csa->binfo->hisr & HISR_VC1) != 0)
		chn_intr(csa->rch.channel);
}

/* -------------------------------------------------------------------- */

/*
 * Probe and attach the card
 */

static int
csa_init(struct csa_info *csa)
{
	csa_res *resp;

	resp = &csa->res;

	csa->pfie = 0;
	csa_stopplaydma(csa);
	csa_stopcapturedma(csa);

	/* Crank up the power on the DAC and ADC. */
	csa_powerupadc(resp);
	csa_powerupdac(resp);

	csa_setplaysamplerate(resp, 8000);
	csa_setcapturesamplerate(resp, 8000);

	if (csa_startdsp(resp))
		return (1);

	return 0;
}

/* Allocates resources. */
static int
csa_allocres(struct csa_info *csa, device_t dev)
{
	csa_res *resp;

	resp = &csa->res;
	if (resp->io == NULL) {
		resp->io = bus_alloc_resource(dev, SYS_RES_MEMORY, &resp->io_rid, 0, ~0, CS461x_IO_SIZE, RF_ACTIVE);
		if (resp->io == NULL)
			return (1);
	}
	if (resp->mem == NULL) {
		resp->mem = bus_alloc_resource(dev, SYS_RES_MEMORY, &resp->mem_rid, 0, ~0, CS461x_MEM_SIZE, RF_ACTIVE);
		if (resp->mem == NULL)
			return (1);
	}
	if (resp->irq == NULL) {
		resp->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &resp->irq_rid, 0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
		if (resp->irq == NULL)
			return (1);
	}
	if (bus_dma_tag_create(/*parent*/NULL, /*alignment*/CS461x_BUFFSIZE, /*boundary*/CS461x_BUFFSIZE,
			       /*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
			       /*highaddr*/BUS_SPACE_MAXADDR,
			       /*filter*/NULL, /*filterarg*/NULL,
			       /*maxsize*/CS461x_BUFFSIZE, /*nsegments*/1, /*maxsegz*/0x3ffff,
			       /*flags*/0, &csa->parent_dmat) != 0)
		return (1);

	return (0);
}

/* Releases resources. */
static void
csa_releaseres(struct csa_info *csa, device_t dev)
{
	csa_res *resp;

	resp = &csa->res;
	if (resp->irq != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ, resp->irq_rid, resp->irq);
		resp->irq = NULL;
	}
	if (resp->io != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, resp->io_rid, resp->io);
		resp->io = NULL;
	}
	if (resp->mem != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, resp->mem_rid, resp->mem);
		resp->mem = NULL;
	}
}

static int pcmcsa_probe(device_t dev);
static int pcmcsa_attach(device_t dev);

static int
pcmcsa_probe(device_t dev)
{
	char *s;
	struct sndcard_func *func;

	/* The parent device has already been probed. */

	func = device_get_ivars(dev);
	if (func == NULL || func->func != SCF_PCM)
		return (ENXIO);

	s = "CS461x PCM Audio";

	device_set_desc(dev, s);
	return (0);
}

static int
pcmcsa_attach(device_t dev)
{
	snddev_info *devinfo;
	struct csa_info *csa;
	csa_res *resp;
	int unit;
	char status[SND_STATUSLEN];
	struct ac97_info *codec;
	struct sndcard_func *func;

	devinfo = device_get_softc(dev);
	csa = malloc(sizeof(*csa), M_DEVBUF, M_NOWAIT);
	if (csa == NULL)
		return (ENOMEM);
	bzero(csa, sizeof(*csa));
	unit = device_get_unit(dev);
	func = device_get_ivars(dev);
	csa->binfo = func->varinfo;
	/*
	 * Fake the status of DMA so that the initial value of
	 * PCTL and CCTL can be stored into csa->pctl and csa->cctl,
	 * respectively.
	 */
	csa->pch.dma = csa->rch.dma = 1;

	/* Allocate the resources. */
	resp = &csa->res;
	resp->io_rid = CS461x_IO_OFFSET;
	resp->mem_rid = CS461x_MEM_OFFSET;
	resp->irq_rid = 0;
	if (csa_allocres(csa, dev)) {
		csa_releaseres(csa, dev);
		return (ENXIO);
	}

	if (csa_init(csa)) {
		csa_releaseres(csa, dev);
		return (ENXIO);
	}
	codec = ac97_create(dev, csa, NULL, csa_rdcd, csa_wrcd);
	if (codec == NULL)
		return (ENXIO);
	if (mixer_init(devinfo, &ac97_mixer, codec) == -1)
		return (ENXIO);

	snprintf(status, SND_STATUSLEN, "at irq %ld", rman_get_start(resp->irq));

	/* Enable interrupt. */
	if (bus_setup_intr(dev, resp->irq, INTR_TYPE_TTY, csa_intr, csa, &csa->ih)) {
		csa_releaseres(csa, dev);
		return (ENXIO);
	}
	csa_writemem(resp, BA1_PFIE, csa_readmem(resp, BA1_PFIE) & ~0x0000f03f);
	csa_writemem(resp, BA1_CIE, (csa_readmem(resp, BA1_CIE) & ~0x0000003f) | 0x00000001);

	if (pcm_register(dev, csa, 1, 1)) {
		csa_releaseres(csa, dev);
		return (ENXIO);
	}
	pcm_addchan(dev, PCMDIR_REC, &csa_chantemplate, csa);
	pcm_addchan(dev, PCMDIR_PLAY, &csa_chantemplate, csa);
	pcm_setstatus(dev, status);

	return (0);
}

/* ac97 codec */

static u_int32_t
csa_rdcd(void *devinfo, int regno)
{
	u_int32_t data;
	struct csa_info *csa = (struct csa_info *)devinfo;

	if (csa_readcodec(&csa->res, regno + BA0_AC97_RESET, &data))
		data = 0;

	return data;
}

static void
csa_wrcd(void *devinfo, int regno, u_int32_t data)
{
	struct csa_info *csa = (struct csa_info *)devinfo;

	csa_writecodec(&csa->res, regno + BA0_AC97_RESET, data);
}

static device_method_t pcmcsa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe , pcmcsa_probe ),
	DEVMETHOD(device_attach, pcmcsa_attach),

	{ 0, 0 },
};

static driver_t pcmcsa_driver = {
	"pcm",
	pcmcsa_methods,
	sizeof(snddev_info),
};

static devclass_t pcm_devclass;

DRIVER_MODULE(snd_csapcm, csa, pcmcsa_driver, pcm_devclass, 0, 0);
MODULE_DEPEND(snd_csapcm, snd_pcm, PCM_MINVER, PCM_PREFVER, PCM_MAXVER);
MODULE_DEPEND(snd_csapcm, snd_csa, 1, 1, 1);
MODULE_VERSION(snd_csapcm, 1);
