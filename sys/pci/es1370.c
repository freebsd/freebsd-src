/* 
 * Support the ENSONIQ AudioPCI board based on the ES1370 and Codec
 * AK4531.
 *
 * Copyright (c) 1998 by Joachim Kuebart. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgement:
 *	This product includes software developed by Joachim Kuebart.
 * 
 * 4. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.	IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: es1370.c,v 1.3 1999/05/09 10:43:54 peter Exp $
 */

#include "pci.h"
#include "pcm.h"

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/kernel.h>
#include <machine/bus_pio.h>
#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <pci/es1370_reg.h>
#include <i386/isa/snd/sound.h>
#define DSP_ULAW_NOT_WANTED
#include <i386/isa/snd/ulaw.h>

#if NPCI != 0


/* -------------------------------------------------------------------- */

/*
 * #defines
 */

#ifdef __alpha__
#define IO_SPACE_MAPPING	ALPHA_BUS_SPACE_IO
#define MEM_SPACE_MAPPING	ALPHA_BUS_SPACE_MEM
#else /* not __alpha__ */
#define IO_SPACE_MAPPING	I386_BUS_SPACE_IO
#define MEM_SPACE_MAPPING	I386_BUS_SPACE_MEM
#endif /* not __alpha__ */

#define DMA_ALIGN_THRESHOLD 4
#define DMA_ALIGN_MASK (~(DMA_ALIGN_THRESHOLD - 1))
#define DMA_READ_THRESHOLD 0x200

#define MEM_MAP_REG 0x14

#define UNIT(minor) ((minor) >> 4)
#define DEV(minor) ((minor) & 0xf)


/* -------------------------------------------------------------------- */

/*
 * PCI IDs of supported chips
 */

#define ES1370_PCI_ID 0x50001274


/* -------------------------------------------------------------------- */

/*
 * device private data
 */

struct es_info {
	bus_space_tag_t st;
	bus_space_handle_t sh;

	bus_dma_tag_t	parent_dmat;
	bus_dmamap_t	dmam_in, dmam_out;

	/* Contents of board's registers */
	u_long		ctrl;
	u_long		sctrl;
};


/* -------------------------------------------------------------------- */

/*
 * prototypes
 */

static void	dma_wrintr(snddev_info *);
static void	dma_rdintr(snddev_info *);
static int      es_init(snddev_info *);
static snd_callback_t es_callback;
static d_open_t es_dsp_open;
static d_close_t es_dsp_close;
static d_ioctl_t es_dsp_ioctl;
static d_read_t es_dsp_read;
static d_write_t es_dsp_write;
static void     es_intr(void *);
static int      es_rdabort(snddev_info *);
static void     es_rd_map(void *, bus_dma_segment_t *, int, int);
static int      es_wrabort(snddev_info *);
static void     es_wr_map(void *, bus_dma_segment_t *, int, int);
static const char *es_pci_probe __P((pcici_t, pcidi_t));
static void	es_pci_attach __P((pcici_t, int));
static int	es_rd_dmaupdate(snddev_info *);
static d_select_t es_select;
static int	es_wr_dmaupdate(snddev_info *);
static int	alloc_dmabuf(snddev_info *, int);
static int      write_codec(snddev_info *, u_char, u_char);


/* -------------------------------------------------------------------- */

/*
 * PCI driver and PCM driver method tables
 */

static struct pci_device es_pci_driver = {
	"es",
	es_pci_probe,
	es_pci_attach,
	&nsnd,
	NULL
};

COMPAT_PCI_DRIVER(es_pci, es_pci_driver);

static snddev_info es_op_desc = {
	"ENSONIQ AudioPCI",

	0,			/* type, apparently unused */
	NULL,			/* ISA probe */
	NULL,			/* ISA attach */

	es_dsp_open,
	es_dsp_close,
	es_dsp_read,
	es_dsp_write,
	es_dsp_ioctl,
	es_select,

	NULL,			/* Interrupt Service Routine */
	es_callback,

	ES_BUFFSIZE,

	AFMT_FULLDUPLEX | AFMT_STEREO | AFMT_U8 | AFMT_S16_LE, /* brag :-) */
};


/* -------------------------------------------------------------------- */

/*
 * The mixer interface
 */

static const struct {
	unsigned        volidx:4;
	unsigned        left:4;
	unsigned        right:4;
	unsigned        stereo:1;
	unsigned        recmask:13;
	unsigned        avail:1;
}               mixtable[SOUND_MIXER_NRDEVICES] = {
	[SOUND_MIXER_VOLUME]	= { 0, 0x0, 0x1, 1, 0x0000, 1 },
	[SOUND_MIXER_PCM] 	= { 1, 0x2, 0x3, 1, 0x0400, 1 },
	[SOUND_MIXER_SYNTH]	= { 2, 0x4, 0x5, 1, 0x0060, 1 },
	[SOUND_MIXER_CD]	= { 3, 0x6, 0x7, 1, 0x0006, 1 },
	[SOUND_MIXER_LINE]	= { 4, 0x8, 0x9, 1, 0x0018, 1 },
	[SOUND_MIXER_LINE1]	= { 5, 0xa, 0xb, 1, 0x1800, 1 },
	[SOUND_MIXER_LINE2]	= { 6, 0xc, 0x0, 0, 0x0100, 1 },
	[SOUND_MIXER_LINE3]	= { 7, 0xd, 0x0, 0, 0x0200, 1 },
	[SOUND_MIXER_MIC]	= { 8, 0xe, 0x0, 0, 0x0001, 1 },
	[SOUND_MIXER_OGAIN]	= { 9, 0xf, 0x0, 0, 0x0000, 1 } };

static int
mixer_ioctl(snddev_info *d, u_long cmd, caddr_t data, int fflag, struct proc *p)
{
	int             i, j, *val, ret = 0;

	val = (int *)data;
	i = cmd & 0xff;

	switch (cmd & IOC_DIRMASK) {
	case IOC_IN | IOC_OUT:		/* _IOWR */
		switch (i) {
		case SOUND_MIXER_RECSRC:
			for (i = j = 0; i != SOUND_MIXER_NRDEVICES; i++)
				if ((*val & (1 << i)) != 0) {
					if (!mixtable[i].recmask)
						*val &= ~(1 << i);
					else
						j |= mixtable[i].recmask;
				}
			d->mix_recsrc = *val;
			write_codec(d, CODEC_LIMIX1, j & 0x55);
			write_codec(d, CODEC_RIMIX1, j & 0xaa);
			write_codec(d, CODEC_LIMIX2, (j >> 8) & 0x17);
			write_codec(d, CODEC_RIMIX2, (j >> 8) & 0x0f);
			write_codec(d, CODEC_OMIX1, 0x7f);
			write_codec(d, CODEC_OMIX2, 0x3f);
			break;
		
		default:
			if (i >= SOUND_MIXER_NRDEVICES || !mixtable[i].avail)
				ret = EINVAL;
			else {
				int l, r, rl, rr;

				l = *val & 0xff;
				if (l > 100)
					l = 100;
				if (mixtable[i].left == 0xf) {
					if (l < 2)
						rl = 0x80;
					else
						rl = 7 - (l - 2) / 14;
				} else {
					if (l < 10)
						rl = 0x80;
					else
						rl = 15 - (l - 10) / 6;
				}
				if (mixtable[i].stereo) {
					r = (*val >> 8) & 0xff;
					if (r > 100)
						r = 100;
					if (r < 10)
						rr = 0x80;
					else
						rr = 15 - (r - 10) / 6;
					write_codec(d, mixtable[i].right, rr);
				} else
					r = l;
				write_codec(d, mixtable[i].left, rl);
				*val = d->mix_levels[i] = ((u_int) r << 8) | l;
			}
			break;
		}
		break;

	default:
		ret = ENOSYS;
		break;
	}

	return (ret);
}


/* -------------------------------------------------------------------- */

/*
 * File operations
 */

static int
es_dsp_open(dev_t dev, int oflags, int devtype, struct proc *p)
{
	int		unit = UNIT(minor(dev));
	snddev_info    *d = &pcm_info[unit];

	if (d->flags & SND_F_BUSY)
		return (EBUSY);
	d->flags = 0;

	d->dbuf_out.total = d->dbuf_out.prev_total =
	    d->dbuf_in.total = d->dbuf_in.prev_total = 0;

	switch (DEV(minor(dev))) {
	case SND_DEV_DSP16:
		d->play_fmt = d->rec_fmt = AFMT_S16_LE;
		break;

	case SND_DEV_DSP:
		d->play_fmt = d->rec_fmt = AFMT_U8;
		break;

	case SND_DEV_AUDIO:
		d->play_fmt = d->rec_fmt = AFMT_MU_LAW;
		break;

	default:
		return (ENXIO);
	}

	if ((oflags & FREAD) == 0)
		d->rec_fmt = 0;
	else if ((oflags & FWRITE) == 0)
		d->play_fmt = 0;

	d->play_speed = d->rec_speed = DSP_DEFAULT_SPEED;

	d->flags |= SND_F_BUSY;
	if (oflags & O_NONBLOCK)
		d->flags |= SND_F_NBIO;

	ask_init(d);

	return (0);
}

static int
es_dsp_close(dev_t dev, int cflags, int devtype, struct proc *p)
{
	int		unit = UNIT(minor(dev));
	snddev_info    *d = &pcm_info[unit];

	d->flags &= ~SND_F_BUSY;

	es_rdabort(d);

	return (0);
}

static int
es_dsp_read(dev_t dev, struct uio *buf, int flag)
{
	int		l, l1, limit, ret = 0, unit = UNIT(minor(dev));
	long		s;
	snddev_info    *d = &pcm_info[unit];
	snd_dbuf       *b = &d->dbuf_in;

	if (d->flags & SND_F_READING) {
		/* This shouldn't happen and is actually silly */
		tsleep(&s, PZERO, "sndar", hz);
		return (EBUSY);
	}
	d->flags |= SND_F_READING;

	/*
	 * XXX Check for SND_F_INIT. If set, wait for DMA to run empty and
	 * re-initialize the board
	 */

	if (buf->uio_resid - d->rec_blocksize > 0)
		limit = buf->uio_resid - d->rec_blocksize;
	else
		limit = 0;

	while ((l = buf->uio_resid) > limit) {
		s = spltty();
		es_rd_dmaupdate(d);
		if ((l = min(l, b->rl)) == 0) {
			int timeout;
			if (b->dl == 0)
				dma_rdintr(d);
			if (d->flags & SND_F_NBIO) {
				splx(s);
				break;
			}
			if (buf->uio_resid - limit > b->dl)
				timeout = hz;
			else
				timeout = 1;
			splx(s);
			switch (ret = tsleep((caddr_t)b, PRIBIO | PCATCH,
					     "dsprd", timeout)) {
			case EINTR:
				es_rdabort(d);
				/* FALLTHROUGH */

			case ERESTART:
				break;

			default:
				continue;
			}
			break;
		}
		splx(s);

		if ((l1 = b->bufsize - b->rp) < l) {
			if (d->flags & SND_F_XLAT8) {
				translate_bytes(ulaw_dsp, b->buf + b->rp, l1);
				translate_bytes(ulaw_dsp, b->buf, l - l1);
			}
			uiomove(b->buf + b->rp, l1, buf);
			uiomove(b->buf, l - l1, buf);
		} else {
			if (d->flags & SND_F_XLAT8)
				translate_bytes(ulaw_dsp, b->buf + b->rp, l);
			uiomove(b->buf + b->rp, l, buf);
		}

		s = spltty();
		b->fl += l;
		b->rl -= l;
		b->rp = (b->rp + l) % b->bufsize;
		splx(s);
	}

	d->flags &= ~SND_F_READING;

	return (ret);
}

static int
es_dsp_write(dev_t dev, struct uio *buf, int flag)
{
	int		l, l1, ret = 0, unit = UNIT(minor(dev));
	long		s;
	snddev_info    *d = &pcm_info[unit];
	snd_dbuf       *b = &d->dbuf_out;

	if (d->flags & SND_F_WRITING) {
		/* This shouldn't happen and is actually silly */
		tsleep(&s, PZERO, "sndaw", hz);
		return (EBUSY);
	}
	d->flags |= SND_F_WRITING;

	/*
	 * XXX Check for SND_F_INIT. If set, wait for DMA to run empty and
	 * re-initialize the board
	 */

	while ((l = buf->uio_resid) != 0) {
		s = spltty();
		es_wr_dmaupdate(d);
		if ((l = min(l, b->fl)) == 0) {
			int timeout;
			if (d->flags & SND_F_NBIO) {
				splx(s);
				break;
			}
			if (buf->uio_resid >= b->dl)
				timeout = hz;
			else
				timeout = 1;
			splx(s);
			switch (ret = tsleep((caddr_t)b, PRIBIO | PCATCH,
					     "dspwr", timeout)) {
			case EINTR:
				es_wrabort(d);
				/* FALLTHROUGH */

			case ERESTART:
				break;

			default:
				continue;
			}
			break;
		}
		splx(s);

		if ((l1 = b->bufsize - b->fp) < l) {
			uiomove(b->buf + b->fp, l1, buf);
			uiomove(b->buf, l - l1, buf);
			if (d->flags & SND_F_XLAT8) {
				translate_bytes(ulaw_dsp, b->buf + b->fp, l1);
				translate_bytes(ulaw_dsp, b->buf, l - l1);
			}
		} else {
			uiomove(b->buf + b->fp, l, buf);
			if (d->flags & SND_F_XLAT8)
				translate_bytes(ulaw_dsp, b->buf + b->fp, l);
		}

		s = spltty();
		b->rl += l;
		b->fl -= l;
		b->fp = (b->fp + l) % b->bufsize;
		if (b->dl == 0)
			dma_wrintr(d);
		splx(s);
	}

	d->flags &= ~SND_F_WRITING;

	return (ret);
}

static int
es_dsp_ioctl(dev_t dev, u_long cmd, caddr_t data, int fflag, struct proc *p)
{
	int		ret = 0, unit = UNIT(minor(dev));
	snddev_info    *d = &pcm_info[unit];
	long		s;

	if ((cmd & MIXER_WRITE(0)) == MIXER_WRITE(0))
		return mixer_ioctl(d, cmd, data, fflag, p);

	switch(cmd) {
	case AIONWRITE:
		if (d->dbuf_out.dl != 0) {
			s = spltty();
			es_wr_dmaupdate(d);
			splx(s);
		}
		*(int *)data = d->dbuf_out.fl;
		break;

	case FIONREAD:
		if (d->dbuf_in.dl != 0) {
			s = spltty();
			es_rd_dmaupdate(d);
			splx(s);
		}
		*(int *)data = d->dbuf_in.rl;
		break;

	case SNDCTL_DSP_GETISPACE:
		{
			audio_buf_info *a = (audio_buf_info *)data;
			snd_dbuf       *b = &d->dbuf_in;
			if (b->dl != 0) {
				s = spltty();
				es_rd_dmaupdate(d);
				splx(s);
			}
			a->bytes = b->fl;
			a->fragments = b->fl / d->rec_blocksize;
			a->fragstotal = b->bufsize / d->rec_blocksize;
			a->fragsize = d->rec_blocksize;
		}
		break;

	case SNDCTL_DSP_GETOSPACE:
		{
			audio_buf_info *a = (audio_buf_info *)data;
			snd_dbuf       *b = &d->dbuf_out;
			if (b->dl != 0) {
				s = spltty();
				es_wr_dmaupdate(d);
				splx(s);
			}
			a->bytes = b->fl;
			a->fragments = b->fl / d->rec_blocksize;
			a->fragstotal = b->bufsize / d->play_blocksize;
			a->fragsize = d->play_blocksize;
		}
		break;

	case SNDCTL_DSP_GETIPTR:
		{
			count_info     *c = (count_info *)data;
			snd_dbuf       *b = &d->dbuf_in;
			if (b->dl != 0) {
				s = spltty();
				es_rd_dmaupdate(d);
				splx(s);
			}
			c->bytes = b->total;
			c->blocks = (b->total - b->prev_total +
				d->rec_blocksize - 1) / d->rec_blocksize;
			c->ptr = b->fp;
			b->prev_total = b->total;
		}
		break;

	case SNDCTL_DSP_GETOPTR:
		{
			count_info     *c = (count_info *)data;
			snd_dbuf       *b = &d->dbuf_out;
			if (b->dl != 0) {
				s = spltty();
				es_wr_dmaupdate(d);
				splx(s);
			}
			c->bytes = b->total;
			c->blocks = (b->total - b->prev_total +
				d->play_blocksize - 1) / d->play_blocksize;
			c->ptr = b->rp;
			b->prev_total = b->total;
		}
		break;

	case AIOSTOP:
	case SNDCTL_DSP_RESET:
	case SNDCTL_DSP_SYNC:
		ret = EINVAL;
		break;

	default:
		ret = ENOSYS;
		break;
	}
	return (ret);
}

static int
es_select(dev_t i_dev, int rw, struct proc * p)
{
	return (ENOSYS);
}


/* -------------------------------------------------------------------- */

/*
 * The interrupt handler
 */

static void
es_intr (void *p)
{
	snddev_info    *d = (snddev_info *)p;
	struct es_info *es = (struct es_info *)d->device_data;
	unsigned	intsrc, sctrl;

	intsrc = bus_space_read_4(es->st, es->sh, ES1370_REG_STATUS);
	if ((intsrc & STAT_INTR) == 0)
		return;

	sctrl = es->sctrl;
	if (intsrc & STAT_ADC)
		sctrl &= ~SCTRL_R1INTEN;
	if (intsrc & STAT_DAC1)
		sctrl &= ~SCTRL_P1INTEN;
	if (intsrc & STAT_DAC2) {
		sctrl &= ~SCTRL_P2INTEN;
	}
	bus_space_write_4(es->st, es->sh, ES1370_REG_SERIAL_CONTROL, sctrl);
	bus_space_write_4(es->st, es->sh, ES1370_REG_SERIAL_CONTROL,
		es->sctrl);
	if (intsrc & STAT_DAC2)
		dma_wrintr(d);
	if (intsrc & STAT_ADC)
		dma_rdintr(d);
}


/* -------------------------------------------------------------------- */

/*
 * DMA hassle
 */

static int
alloc_dmabuf(snddev_info *d, int rd)
{
	struct es_info *es = (struct es_info *)d->device_data;
	snd_dbuf       *b = rd ? &d->dbuf_in : &d->dbuf_out;
	bus_dmamap_t   *dmam = rd ? &es->dmam_in : &es->dmam_out;

	if (bus_dmamem_alloc(es->parent_dmat, (void **)&b->buf, BUS_DMA_NOWAIT,
			     dmam) != 0 ||
	    bus_dmamap_load(es->parent_dmat, *dmam, b->buf, d->bufsize,
			    rd ? es_rd_map : es_wr_map, es, 0) != 0)
		return -1;

	b->rp = b->fp = b->dl = b->rl = 0;
	b->fl = b->bufsize = d->bufsize;
	return (0);
}

static int
es_wr_dmaupdate(snddev_info *d)
{
	struct es_info *es = (struct es_info *)d->device_data;
	unsigned	hwptr, delta;

	bus_space_write_4(es->st, es->sh, ES1370_REG_MEMPAGE,
		ES1370_REG_DAC2_FRAMECNT >> 8);
	hwptr = (bus_space_read_4(es->st, es->sh,
		ES1370_REG_DAC2_FRAMECNT & 0xff) >> 14) & 0x3fffc;
	delta = (d->dbuf_out.bufsize + hwptr - d->dbuf_out.rp) %
		d->dbuf_out.bufsize;
	d->dbuf_out.rp = hwptr;
	d->dbuf_out.rl -= delta;
	d->dbuf_out.fl += delta;
	d->dbuf_out.total += delta;

	return delta;
}

static int
es_rd_dmaupdate(snddev_info *d)
{
	struct es_info *es = (struct es_info *)d->device_data;
	unsigned	hwptr, delta;

	bus_space_write_4(es->st, es->sh, ES1370_REG_MEMPAGE,
		ES1370_REG_ADC_FRAMECNT >> 8);
	hwptr = (bus_space_read_4(es->st, es->sh,
		ES1370_REG_ADC_FRAMECNT & 0xff) >> 14) & 0x3fffc;
	delta = (d->dbuf_in.bufsize + hwptr - d->dbuf_in.fp) %
		d->dbuf_in.bufsize;
	d->dbuf_in.fp = hwptr;
	d->dbuf_in.rl += delta;
	d->dbuf_in.fl -= delta;
	d->dbuf_in.total += delta;
	return delta;
}


/* -------------------------------------------------------------------- */

/*
 * Hardware
 */

static int
es_callback(snddev_info *d, int reason)
{
	struct es_info *es = (struct es_info *)d->device_data;
	int		rd = reason & SND_CB_RD;

	switch(reason & SND_CB_REASON_MASK) {
	case SND_CB_INIT:
		es->ctrl = (es->ctrl & ~CTRL_PCLKDIV) |
			(DAC2_SRTODIV(d->play_speed) << CTRL_SH_PCLKDIV);
		snd_set_blocksize(d);

		es->sctrl &= ~(SCTRL_R1FMT | SCTRL_P2FMT);
		d->flags &= ~SND_F_XLAT8;
		switch(d->play_fmt) {
		case 0:
		case AFMT_U8:
			break;

		case AFMT_S16_LE:
			es->sctrl |= SCTRL_P2SEB;
			break;

		case AFMT_MU_LAW:
			d->flags |= SND_F_XLAT8;
			break;

		default:
			return (-1);
		}

		switch(d->rec_fmt) {
		case 0:
		case AFMT_U8:
			break;

		case AFMT_S16_LE:
			es->sctrl |= SCTRL_R1SEB;
			break;

		case AFMT_MU_LAW:
			d->flags |= SND_F_XLAT8;
			break;

		default:
			return (-1);
		}

		if (d->flags & SND_F_STEREO)
			es->sctrl |= SCTRL_P2SMB | SCTRL_R1SMB;

		bus_space_write_4(es->st, es->sh, ES1370_REG_CONTROL,
			es->ctrl);
		bus_space_write_4(es->st, es->sh, ES1370_REG_SERIAL_CONTROL,
			es->sctrl);
		break;

	case SND_CB_START:
		if (rd) {
			es->ctrl |= CTRL_ADC_EN;
			es->sctrl = (es->sctrl & ~SCTRL_R1LOOPSEL) |
			    SCTRL_R1INTEN;
			bus_space_write_4(es->st, es->sh, ES1370_REG_ADC_SCOUNT,
				d->dbuf_in.dl / d->dbuf_in.sample_size - 1);
		} else {
			es->ctrl |= CTRL_DAC2_EN;
			es->sctrl = (es->sctrl & ~(SCTRL_P2ENDINC |
			    SCTRL_P2STINC | SCTRL_P2LOOPSEL | SCTRL_P2PAUSE |
			    SCTRL_P2DACSEN)) | SCTRL_P2INTEN |
			    (((d->play_fmt == AFMT_S16_LE) ? 2 : 1)
				<< SCTRL_SH_P2ENDINC);
			bus_space_write_4(es->st, es->sh,
			    ES1370_REG_DAC2_SCOUNT,
			    d->dbuf_out.dl / d->dbuf_out.sample_size - 1);
		}
		bus_space_write_4(es->st, es->sh, ES1370_REG_SERIAL_CONTROL,
			es->sctrl);
		bus_space_write_4(es->st, es->sh, ES1370_REG_CONTROL, es->ctrl);
		break;

	case SND_CB_ABORT:
	case SND_CB_STOP:
		if (rd)
			es->ctrl &= ~CTRL_ADC_EN;
		else
			es->ctrl &= ~CTRL_DAC2_EN;
		bus_space_write_4(es->st, es->sh, ES1370_REG_CONTROL, es->ctrl);
		break;

	default:
		return (-1);
	}
	return (0);
}

static int
write_codec(snddev_info *d, u_char i, u_char data)
{
	struct es_info *es = (struct es_info *)d->device_data;
	int		wait = 100;	/* 100 msec timeout */

	do {
		if ((bus_space_read_4(es->st, es->sh, ES1370_REG_STATUS) &
		      STAT_CSTAT) == 0) {
			bus_space_write_2(es->st, es->sh, ES1370_REG_CODEC,
				((u_short)i << CODEC_INDEX_SHIFT) | data);
			return (0);
		}
		DELAY(1000);
		/* tsleep(&wait, PZERO, "sndaw", hz / 1000); */
	} while (--wait);
	printf("pcm: write_codec timed out\n");
	return (-1);
}

static void
es_wr_map(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct es_info *es = (struct es_info *)arg;

	bus_space_write_1(es->st, es->sh, ES1370_REG_MEMPAGE,
		ES1370_REG_DAC2_FRAMEADR >> 8);
	bus_space_write_4(es->st, es->sh, ES1370_REG_DAC2_FRAMEADR & 0xff, 
		segs->ds_addr);
	bus_space_write_4(es->st, es->sh, ES1370_REG_DAC2_FRAMECNT & 0xff, 
		(segs->ds_len >> 2) - 1);
}

static void
es_rd_map(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct es_info *es = (struct es_info *)arg;

	bus_space_write_1(es->st, es->sh, ES1370_REG_MEMPAGE,
		ES1370_REG_ADC_FRAMEADR >> 8);
	bus_space_write_4(es->st, es->sh, ES1370_REG_ADC_FRAMEADR & 0xff, 
		segs->ds_addr);
	bus_space_write_4(es->st, es->sh, ES1370_REG_ADC_FRAMECNT & 0xff, 
		(segs->ds_len >> 2) - 1);
}

static void
dma_wrintr(snddev_info *d)
{
	snd_dbuf       *b = &d->dbuf_out;

	/*
	 * According to Linux driver:
	 * dmaupdate()
	 * Bei underrun error++
	 * wake_up(dac2.wait)
	 */

	if (b->dl != 0) {
		es_wr_dmaupdate(d);
		wakeup(b);
	}

	if (b->rl >= DMA_ALIGN_THRESHOLD &&
	    !(d->flags & SND_F_ABORTING)) {
		int l = min(b->rl, d->play_blocksize);
		l &= DMA_ALIGN_MASK;

		if (l != b->dl) {
			if (b->dl != 0) {
				d->callback(d, SND_CB_WR | SND_CB_STOP);
				es_wr_dmaupdate(d);
				l = min(b->rl, d->play_blocksize);
				l &= DMA_ALIGN_MASK;
			}
			b->dl = l;
			d->callback(d, SND_CB_WR | SND_CB_START);
		}
	} else if (b->dl != 0) {
		b->dl = 0;
		d->callback(d, SND_CB_WR | SND_CB_STOP);
		es_wr_dmaupdate(d);
	}
}

static void
dma_rdintr(snddev_info *d)
{
	snd_dbuf       *b = &d->dbuf_in;

	if (b->dl != 0) {
		es_rd_dmaupdate(d);
		wakeup(b);
	}

	if (b->fl >= DMA_READ_THRESHOLD &&
	    !(d->flags & SND_F_ABORTING)) {
		int l = min(b->fl, d->rec_blocksize);
		l &= DMA_ALIGN_MASK;

		if (l != b->dl) {
			if (b->dl != 0) {
				d->callback(d, SND_CB_RD | SND_CB_STOP);
				es_rd_dmaupdate(d);
				l = min(b->fl, d->rec_blocksize);
				l &= DMA_ALIGN_MASK;
			}
			b->dl = l;
			d->callback(d, SND_CB_RD | SND_CB_START);
		}
	} else {
		if (b->dl != 0) {
			b->dl = 0;
			d->callback(d, SND_CB_RD | SND_CB_STOP);
			es_rd_dmaupdate(d);
		}
	}
}

static int
es_wrabort(snddev_info *d)
{
	snd_dbuf       *b = &d->dbuf_out;
	long		s;
	int		missing;

	s = spltty();
	if (b->dl != 0) {
		wakeup(b);
		b->dl = 0;
		d->callback(d, SND_CB_WR | SND_CB_ABORT);
	}
	es_wr_dmaupdate(d);
	missing = b->rl;
	b->rl = 0;
	b->fp = b->rp;
	b->fl = b->bufsize;
	splx(s);
	return missing;
}

static int
es_rdabort(snddev_info *d)
{
	snd_dbuf       *b = &d->dbuf_in;
	long		s;
	int		missing;

	s = spltty();
	if (b->dl != 0) {
		wakeup(b);
		b->dl = 0;
		d->callback(d, SND_CB_RD | SND_CB_ABORT);
		es_rd_dmaupdate(d);
	}
	missing = b->rl;
	b->rl = 0;
	b->fp = b->rp;
	b->fl = b->bufsize;
	splx(s);
	return missing;
}


/* -------------------------------------------------------------------- */

/*
 * Probe and attach the card
 */

static int
es_init(snddev_info *d)
{
	struct es_info *es = (struct es_info *)d->device_data;
	u_int		i;

	es->ctrl = CTRL_CDC_EN | CTRL_SERR_DIS |
		(DAC2_SRTODIV(DSP_DEFAULT_SPEED) << CTRL_SH_PCLKDIV);
	bus_space_write_4(es->st, es->sh, ES1370_REG_CONTROL, es->ctrl);
	es->sctrl = 0;
	bus_space_write_4(es->st, es->sh, ES1370_REG_SERIAL_CONTROL, es->sctrl);
	write_codec(d, CODEC_RES_PD, 3);/* No RST, PD */
	write_codec(d, CODEC_CSEL, 0);	/* CODEC ADC and CODEC DAC use
					 * {LR,B}CLK2 and run off the LRCLK2
					 * PLL; program DAC_SYNC=0!  */
	write_codec(d, CODEC_ADSEL, 0);	/* Recording source is mixer */
	write_codec(d, CODEC_MGAIN, 0);	/* MIC amp is 0db */

	i = SOUND_MASK_MIC;
	mixer_ioctl(d, SOUND_MIXER_WRITE_RECSRC, (caddr_t) &i, 0, NULL);
	i = 0;
	mixer_ioctl(d, SOUND_MIXER_WRITE_VOLUME, (caddr_t) &i, 0, NULL);
	mixer_ioctl(d, SOUND_MIXER_WRITE_PCM, (caddr_t) &i, 0, NULL);
	mixer_ioctl(d, SOUND_MIXER_WRITE_SYNTH, (caddr_t) &i, 0, NULL);
	mixer_ioctl(d, SOUND_MIXER_WRITE_CD, (caddr_t) &i, 0, NULL);
	mixer_ioctl(d, SOUND_MIXER_WRITE_LINE, (caddr_t) &i, 0, NULL);
	mixer_ioctl(d, SOUND_MIXER_WRITE_LINE1, (caddr_t) &i, 0, NULL);
	mixer_ioctl(d, SOUND_MIXER_WRITE_LINE2, (caddr_t) &i, 0, NULL);
	mixer_ioctl(d, SOUND_MIXER_WRITE_LINE3, (caddr_t) &i, 0, NULL);
	mixer_ioctl(d, SOUND_MIXER_WRITE_MIC, (caddr_t) &i, 0, NULL);

	return (0);
}

static const char *
es_pci_probe(pcici_t tag, pcidi_t type)
{
	if (type == ES1370_PCI_ID)
		return ("AudioPCI ES1370");

	return (NULL);
}

static void
es_pci_attach(pcici_t config_id, int unit)
{
	snddev_info    *d;
	u_int32_t	data;
	struct es_info *es;
	pci_port_t	io_port;
	int		i, mapped;
	vm_offset_t	vaddr, paddr;

	if (unit > NPCM_MAX)
		return;

	d = &pcm_info[unit];
	*d = es_op_desc;
	if ((es = malloc(sizeof(*es), M_DEVBUF, M_NOWAIT)) == NULL) {
		printf("pcm%d: cannot allocate softc\n", unit);
		return;
	}
	bzero(es, sizeof(*es));
	d->device_data = es;

	vaddr = paddr = NULL;
	mapped = 0;
	data = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);
	if (mapped == 0 && (data & PCI_COMMAND_MEM_ENABLE)) {
		if (pci_map_mem(config_id, MEM_MAP_REG, &vaddr, &paddr)) {
			es->st = MEM_SPACE_MAPPING;
			es->sh = vaddr;
			mapped++;
		}
	}
	if (mapped == 0 && (data & PCI_COMMAND_IO_ENABLE)) {
		if (pci_map_port(config_id, PCI_MAP_REG_START, &io_port)) {
			es->st = IO_SPACE_MAPPING;
			es->sh = io_port;
			mapped++;
		}
	}
	if (mapped == 0) {
		printf("pcm%d: unable to map any ports\n", unit);
		free(es, M_DEVBUF);
		return;
	}
	printf("pcm%d: using %s space register mapping at %#x\n", unit,
		es->st == IO_SPACE_MAPPING ? "I/O" : "Memory", es->sh);

	d->io_base = es->sh;
	d->mix_devs = 0;
	for (i = 0; i != SOUND_MIXER_NRDEVICES; i++)
		if (mixtable[i].avail)
			d->mix_devs |= (1 << i);
	d->mix_rec_devs = 0;
	for (i = 0; i != SOUND_MIXER_NRDEVICES; i++)
		if (mixtable[i].recmask)
			d->mix_rec_devs |= (1 << i);

	if (es_init(d) == -1) {
		printf("pcm%d: unable to initialize the card\n", unit);
		free(es, M_DEVBUF);
		d->io_base = 0;
		return;
	}
	if (pci_map_int(config_id, es_intr, d, &tty_imask) == 0) {
		printf("pcm%d: unable to map interrupt\n", unit);
		free(es, M_DEVBUF);
		d->io_base = 0;
		return;
	}
	if (bus_dma_tag_create(/*parent*/NULL, /*alignment*/2, /*boundary*/0,
		/*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
		/*highaddr*/BUS_SPACE_MAXADDR,
		/*filter*/NULL, /*filterarg*/NULL,
		/*maxsize*/d->bufsize, /*nsegments*/1, /*maxsegz*/0x3ffff,
		/*flags*/0, &es->parent_dmat) != 0) {
		printf("pcm%d: unable to create dma tag\n", unit);
		free(es, M_DEVBUF);
		d->io_base = 0;
		return;
	}

	if (alloc_dmabuf(d, 0) == -1 ||
	    alloc_dmabuf(d, 1) == -1) {
		printf("pcm%d: unable to allocate dma buffers\n", unit);
		free(es, M_DEVBUF);
		d->io_base = 0;
		return;
	}

	pcminit(d, unit);

	return;
}

#endif /* NPCI != 0 */
