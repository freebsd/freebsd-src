/*-
 * Copyright (c) 2005 Ariff Abdullah <ariff@FreeBSD.org>
 * All rights reserved.
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
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHERIN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THEPOSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * FreeBSD pcm driver for ATI IXP 150/200/250/300 AC97 controllers
 *
 * Features
 *	* 16bit playback / recording
 *	* 32bit native playback - yay!
 *
 * Issues / TODO:
 *	* SPDIF
 *	* Support for more than 2 channels.
 *	* VRA ? VRM ? DRA ?
 *	* 32bit native recording (seems broken, disabled)
 *
 *
 * Thanks goes to:
 *
 *   Shaharil @ SCAN Associates whom relentlessly providing me the
 *   mind blowing Acer Ferrari 4002 WLMi with this ATI IXP hardware.
 *
 *   Reinoud Zandijk <reinoud@NetBSD.org> (auixp), which this driver is
 *   largely based upon although large part of it has been reworked. His
 *   driver is the primary reference and pretty much well documented.
 *
 *   Takashi Iwai (ALSA snd-atiixp), for register definitions and some
 *   random ninja hackery.
 */

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/ac97.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <sys/sysctl.h>
#include <sys/endian.h>

#include <dev/sound/pci/atiixp.h>

SND_DECLARE_FILE("$FreeBSD$");


struct atiixp_dma_op {
	uint32_t addr;
	uint16_t status;
	uint16_t size;
	uint32_t next;
};

struct atiixp_info;

struct atiixp_chinfo {
	struct snd_dbuf *buffer;
	struct pcm_channel *channel;
	struct atiixp_info *parent;
	struct atiixp_dma_op *sgd_table;
	bus_addr_t sgd_addr;
	uint32_t enable_bit, flush_bit, linkptr_bit, dma_dt_cur_bit;
	uint32_t dma_segs;
	uint32_t fmt;
	int caps_32bit, dir, active;
};

struct atiixp_info {
	device_t dev;

	bus_space_tag_t st;
	bus_space_handle_t sh;
	bus_dma_tag_t parent_dmat;
	bus_dma_tag_t sgd_dmat;
	bus_dmamap_t sgd_dmamap;
	bus_addr_t sgd_addr;

	struct resource *reg, *irq;
	int regtype, regid, irqid;
	void *ih;
	struct ac97_info *codec;

	struct atiixp_chinfo pch;
	struct atiixp_chinfo rch;
	struct atiixp_dma_op *sgd_table;
	struct intr_config_hook delayed_attach;

	uint32_t bufsz;
	uint32_t codec_not_ready_bits, codec_idx, codec_found;
	uint32_t dma_segs;
	int registered_channels;

	struct mtx *lock;
};

#define atiixp_rd(_sc, _reg)	\
		bus_space_read_4((_sc)->st, (_sc)->sh, _reg)
#define atiixp_wr(_sc, _reg, _val)	\
		bus_space_write_4((_sc)->st, (_sc)->sh, _reg, _val)

#define atiixp_lock(_sc)	snd_mtxlock((_sc)->lock)
#define atiixp_unlock(_sc)	snd_mtxunlock((_sc)->lock)
#define atiixp_assert(_sc)	snd_mtxassert((_sc)->lock)

static uint32_t atiixp_fmt_32bit[] = {
	AFMT_STEREO | AFMT_S16_LE,
#ifdef AFMT_S32_LE
	AFMT_STEREO | AFMT_S32_LE,
#endif
	0
};

static uint32_t atiixp_fmt[] = {
	AFMT_STEREO | AFMT_S16_LE,
	0
};

static struct pcmchan_caps atiixp_caps_32bit = {
	ATI_IXP_BASE_RATE,
	ATI_IXP_BASE_RATE,
	atiixp_fmt_32bit, 0
};

static struct pcmchan_caps atiixp_caps = {
	ATI_IXP_BASE_RATE, 
	ATI_IXP_BASE_RATE,
	atiixp_fmt, 0
};

static const struct {
	uint16_t vendor;
	uint16_t devid;
	char	 *desc;
} atiixp_hw[] = {
	{ ATI_VENDOR_ID, ATI_IXP_200_ID, "ATI IXP 200" },
	{ ATI_VENDOR_ID, ATI_IXP_300_ID, "ATI IXP 300" },
	{ ATI_VENDOR_ID, ATI_IXP_400_ID, "ATI IXP 400" },
};

static void atiixp_enable_interrupts(struct atiixp_info *);
static void atiixp_disable_interrupts(struct atiixp_info *);
static void atiixp_reset_aclink(struct atiixp_info *);
static void atiixp_flush_dma(struct atiixp_info *, struct atiixp_chinfo *);
static void atiixp_enable_dma(struct atiixp_info *, struct atiixp_chinfo *);
static void atiixp_disable_dma(struct atiixp_info *, struct atiixp_chinfo *);

static int atiixp_waitready_codec(struct atiixp_info *);
static int atiixp_rdcd(kobj_t, void *, int);
static int atiixp_wrcd(kobj_t, void *, int, uint32_t);

static void  *atiixp_chan_init(kobj_t, void *, struct snd_dbuf *,
						struct pcm_channel *, int);
static int    atiixp_chan_setformat(kobj_t, void *, uint32_t);
static int    atiixp_chan_setspeed(kobj_t, void *, uint32_t);
static int    atiixp_chan_setblocksize(kobj_t, void *, uint32_t);
static void   atiixp_buildsgdt(struct atiixp_chinfo *);
static int    atiixp_chan_trigger(kobj_t, void *, int);
static int    atiixp_chan_getptr(kobj_t, void *);
static struct pcmchan_caps *atiixp_chan_getcaps(kobj_t, void *);

static void atiixp_intr(void *);
static void atiixp_dma_cb(void *, bus_dma_segment_t *, int, int);
static void atiixp_chip_pre_init(struct atiixp_info *);
static void atiixp_chip_post_init(void *);
static int  atiixp_pci_probe(device_t);
static int  atiixp_pci_attach(device_t);
static int  atiixp_pci_detach(device_t);
static int  atiixp_pci_suspend(device_t);
static int  atiixp_pci_resume(device_t);

/*
 * ATI IXP helper functions
 */
static void
atiixp_enable_interrupts(struct atiixp_info *sc)
{
	uint32_t value;

	/* clear all pending */
	atiixp_wr(sc, ATI_REG_ISR, 0xffffffff);

	/* enable all relevant interrupt sources we can handle */
	value = atiixp_rd(sc, ATI_REG_IER);

	value |= ATI_REG_IER_IO_STATUS_EN;

	/*
	 * Disable / ignore internal xrun/spdf interrupt flags
	 * since it doesn't interest us (for now).
	 */
#if 0
	value |= ATI_REG_IER_IN_XRUN_EN;
	value |= ATI_REG_IER_OUT_XRUN_EN;

	value |= ATI_REG_IER_SPDF_XRUN_EN;
	value |= ATI_REG_IER_SPDF_STATUS_EN;
#endif

	atiixp_wr(sc, ATI_REG_IER, value);
}

static void
atiixp_disable_interrupts(struct atiixp_info *sc)
{
	/* disable all interrupt sources */
	atiixp_wr(sc, ATI_REG_IER, 0);

	/* clear all pending */
	atiixp_wr(sc, ATI_REG_ISR, 0xffffffff);
}

static void
atiixp_reset_aclink(struct atiixp_info *sc)
{
	uint32_t value, timeout;

	/* if power is down, power it up */
	value = atiixp_rd(sc, ATI_REG_CMD);
	if (value & ATI_REG_CMD_POWERDOWN) {
		/* explicitly enable power */
		value &= ~ATI_REG_CMD_POWERDOWN;
		atiixp_wr(sc, ATI_REG_CMD, value);

		/* have to wait at least 10 usec for it to initialise */
		DELAY(20);
	};

	/* perform a soft reset */
	value  = atiixp_rd(sc, ATI_REG_CMD);
	value |= ATI_REG_CMD_AC_SOFT_RESET;
	atiixp_wr(sc, ATI_REG_CMD, value);

	/* need to read the CMD reg and wait aprox. 10 usec to init */
	value  = atiixp_rd(sc, ATI_REG_CMD);
	DELAY(20);

	/* clear soft reset flag again */
	value  = atiixp_rd(sc, ATI_REG_CMD);
	value &= ~ATI_REG_CMD_AC_SOFT_RESET;
	atiixp_wr(sc, ATI_REG_CMD, value);

	/* check if the ac-link is working; reset device otherwise */
	timeout = 10;
	value = atiixp_rd(sc, ATI_REG_CMD);
	while (!(value & ATI_REG_CMD_ACLINK_ACTIVE)
						&& --timeout) {
#if 0
		device_printf(sc->dev, "not up; resetting aclink hardware\n");
#endif

		/* dip aclink reset but keep the acsync */
		value &= ~ATI_REG_CMD_AC_RESET;
		value |=  ATI_REG_CMD_AC_SYNC;
		atiixp_wr(sc, ATI_REG_CMD, value);

		/* need to read CMD again and wait again (clocking in issue?) */
		value = atiixp_rd(sc, ATI_REG_CMD);
		DELAY(20);

		/* assert aclink reset again */
		value = atiixp_rd(sc, ATI_REG_CMD);
		value |=  ATI_REG_CMD_AC_RESET;
		atiixp_wr(sc, ATI_REG_CMD, value);

		/* check if its active now */
		value = atiixp_rd(sc, ATI_REG_CMD);
	};

	if (timeout == 0)
		device_printf(sc->dev, "giving up aclink reset\n");
#if 0
	if (timeout != 10)
		device_printf(sc->dev, "aclink hardware reset successful\n");
#endif

	/* assert reset and sync for safety */
	value  = atiixp_rd(sc, ATI_REG_CMD);
	value |= ATI_REG_CMD_AC_SYNC | ATI_REG_CMD_AC_RESET;
	atiixp_wr(sc, ATI_REG_CMD, value);
}

static void
atiixp_flush_dma(struct atiixp_info *sc, struct atiixp_chinfo *ch)
{
	atiixp_wr(sc, ATI_REG_FIFO_FLUSH, ch->flush_bit);
}

static void
atiixp_enable_dma(struct atiixp_info *sc, struct atiixp_chinfo *ch)
{
	uint32_t value;

	value = atiixp_rd(sc, ATI_REG_CMD);
	if (!(value & ch->enable_bit)) {
		value |= ch->enable_bit;
		atiixp_wr(sc, ATI_REG_CMD, value);
	}
}

static void 
atiixp_disable_dma(struct atiixp_info *sc, struct atiixp_chinfo *ch)
{
	uint32_t value;

	value = atiixp_rd(sc, ATI_REG_CMD);
	if (value & ch->enable_bit) {
		value &= ~ch->enable_bit;
		atiixp_wr(sc, ATI_REG_CMD, value);
	}
}

/*
 * AC97 interface
 */
static int
atiixp_waitready_codec(struct atiixp_info *sc)
{
	int timeout = 500;

	do {
		if ((atiixp_rd(sc, ATI_REG_PHYS_OUT_ADDR) &
				ATI_REG_PHYS_OUT_ADDR_EN) == 0)
			return 0;
		DELAY(1);
	} while (timeout--);

	return -1;
}

static int
atiixp_rdcd(kobj_t obj, void *devinfo, int reg)
{
	struct atiixp_info *sc = devinfo;
	uint32_t data;
	int timeout;

	if (atiixp_waitready_codec(sc))
		return -1;

	data = (reg << ATI_REG_PHYS_OUT_ADDR_SHIFT) |
			ATI_REG_PHYS_OUT_ADDR_EN |
			ATI_REG_PHYS_OUT_RW | sc->codec_idx;

	atiixp_wr(sc, ATI_REG_PHYS_OUT_ADDR, data);

	if (atiixp_waitready_codec(sc))
		return -1;

	timeout = 500;
	do {
		data = atiixp_rd(sc, ATI_REG_PHYS_IN_ADDR);
		if (data & ATI_REG_PHYS_IN_READ_FLAG)
			return data >> ATI_REG_PHYS_IN_DATA_SHIFT;
		DELAY(1);
	} while (timeout--);

	if (reg < 0x7c)
		device_printf(sc->dev, "codec read timeout! (reg 0x%x)\n", reg);

	return -1;
}

static int
atiixp_wrcd(kobj_t obj, void *devinfo, int reg, uint32_t data)
{
	struct atiixp_info *sc = devinfo;

	if (atiixp_waitready_codec(sc))
		return -1;

	data = (data << ATI_REG_PHYS_OUT_DATA_SHIFT) |
			(((uint32_t)reg) << ATI_REG_PHYS_OUT_ADDR_SHIFT) |
			ATI_REG_PHYS_OUT_ADDR_EN | sc->codec_idx;

	atiixp_wr(sc, ATI_REG_PHYS_OUT_ADDR, data);

	return 0;
}

static kobj_method_t atiixp_ac97_methods[] = {
    	KOBJMETHOD(ac97_read,		atiixp_rdcd),
    	KOBJMETHOD(ac97_write,		atiixp_wrcd),
	{ 0, 0 }
};
AC97_DECLARE(atiixp_ac97);

/*
 * Playback / Record channel interface
 */
static void *
atiixp_chan_init(kobj_t obj, void *devinfo, struct snd_dbuf *b,
					struct pcm_channel *c, int dir)
{
	struct atiixp_info *sc = devinfo;
	struct atiixp_chinfo *ch;
	int num;

	atiixp_lock(sc);

	if (dir == PCMDIR_PLAY) {
		ch = &sc->pch;
		ch->linkptr_bit = ATI_REG_OUT_DMA_LINKPTR;
		ch->enable_bit = ATI_REG_CMD_OUT_DMA_EN | ATI_REG_CMD_SEND_EN;
		ch->flush_bit = ATI_REG_FIFO_OUT_FLUSH;
		ch->dma_dt_cur_bit = ATI_REG_OUT_DMA_DT_CUR;
		/* Native 32bit playback working properly */
		ch->caps_32bit = 1;
	} else {
		ch = &sc->rch;
		ch->linkptr_bit = ATI_REG_IN_DMA_LINKPTR;
		ch->enable_bit = ATI_REG_CMD_IN_DMA_EN  | ATI_REG_CMD_RECEIVE_EN;
		ch->flush_bit = ATI_REG_FIFO_IN_FLUSH;
		ch->dma_dt_cur_bit = ATI_REG_IN_DMA_DT_CUR;
		/* XXX Native 32bit recording appear to be broken */
		ch->caps_32bit = 1;
	}

	ch->buffer = b;
	ch->parent = sc;
	ch->channel = c;
	ch->dir = dir;
	ch->dma_segs = sc->dma_segs;

	atiixp_unlock(sc);

	if (sndbuf_alloc(ch->buffer, sc->parent_dmat, sc->bufsz) == -1)
		return NULL;

	atiixp_lock(sc);
	num = sc->registered_channels++;
	ch->sgd_table = &sc->sgd_table[num * ch->dma_segs];
	ch->sgd_addr = sc->sgd_addr +
			(num * ch->dma_segs * sizeof(struct atiixp_dma_op));
	atiixp_disable_dma(sc, ch);
	atiixp_unlock(sc);

	return ch;
}

static int
atiixp_chan_setformat(kobj_t obj, void *data, uint32_t format)
{
	struct atiixp_chinfo *ch = data;
	struct atiixp_info *sc = ch->parent;
	uint32_t value;

	atiixp_lock(sc);
	if (ch->dir == PCMDIR_REC) {
		value = atiixp_rd(sc, ATI_REG_CMD);
		value &= ~ATI_REG_CMD_INTERLEAVE_IN;
		if (ch->caps_32bit == 0 ||
				(format & (AFMT_8BIT|AFMT_16BIT)) != 0)
			value |= ATI_REG_CMD_INTERLEAVE_IN;
		atiixp_wr(sc, ATI_REG_CMD, value);
	} else {
		value = atiixp_rd(sc, ATI_REG_OUT_DMA_SLOT);
		value &= ~ATI_REG_OUT_DMA_SLOT_MASK;
		/* We do not have support for more than 2 channels, _yet_. */
		value |= ATI_REG_OUT_DMA_SLOT_BIT(3) |
				ATI_REG_OUT_DMA_SLOT_BIT(4);
		value |= 0x04 << ATI_REG_OUT_DMA_THRESHOLD_SHIFT;
		atiixp_wr(sc, ATI_REG_OUT_DMA_SLOT, value);
		value = atiixp_rd(sc, ATI_REG_CMD);
		value &= ~ATI_REG_CMD_INTERLEAVE_OUT;
		if (ch->caps_32bit == 0 ||
				(format & (AFMT_8BIT|AFMT_16BIT)) != 0)
			value |= ATI_REG_CMD_INTERLEAVE_OUT;
		atiixp_wr(sc, ATI_REG_CMD, value);
		value = atiixp_rd(sc, ATI_REG_6CH_REORDER);
		value &= ~ATI_REG_6CH_REORDER_EN;
		atiixp_wr(sc, ATI_REG_6CH_REORDER, value);
	}
	ch->fmt = format;
	atiixp_unlock(sc);

	return 0;
}

static int
atiixp_chan_setspeed(kobj_t obj, void *data, uint32_t spd)
{
	/* XXX We're supposed to do VRA/DRA processing right here */
	return ATI_IXP_BASE_RATE;
}

static int
atiixp_chan_setblocksize(kobj_t obj, void *data, uint32_t blksz)
{
	struct atiixp_chinfo *ch = data;
	struct atiixp_info *sc = ch->parent;

	if (blksz > (sc->bufsz / ch->dma_segs))
		blksz = sc->bufsz / ch->dma_segs;

	sndbuf_resize(ch->buffer, ch->dma_segs, blksz);

	return sndbuf_getblksz(ch->buffer);
}

static void
atiixp_buildsgdt(struct atiixp_chinfo *ch)
{
	uint32_t addr, blksz;
	int i;

	addr = sndbuf_getbufaddr(ch->buffer);
	blksz = sndbuf_getblksz(ch->buffer);

	for (i = 0; i < ch->dma_segs; i++) {
		ch->sgd_table[i].addr = htole32(addr + (i * blksz));
		ch->sgd_table[i].status = htole16(0);
		ch->sgd_table[i].size = htole16(blksz >> 2);
		ch->sgd_table[i].next = htole32((uint32_t)ch->sgd_addr + 
						(((i + 1) % ch->dma_segs) *
						sizeof(struct atiixp_dma_op)));
	}
}

static int
atiixp_chan_trigger(kobj_t obj, void *data, int go)
{
	struct atiixp_chinfo *ch = data;
	struct atiixp_info *sc = ch->parent;
	uint32_t value;

	atiixp_lock(sc);

	switch (go) {
		case PCMTRIG_START:
			atiixp_flush_dma(sc, ch);
			atiixp_buildsgdt(ch);
			atiixp_wr(sc, ch->linkptr_bit, 0);
			atiixp_enable_dma(sc, ch);
			atiixp_wr(sc, ch->linkptr_bit,
				(uint32_t)ch->sgd_addr | ATI_REG_LINKPTR_EN);
			break;
		case PCMTRIG_STOP:
		case PCMTRIG_ABORT:
			atiixp_disable_dma(sc, ch);
			atiixp_flush_dma(sc, ch);
			break;
		default:
			atiixp_unlock(sc);
			return 0;
			break;
	}

	/* Update bus busy status */
	value = atiixp_rd(sc, ATI_REG_IER);
	if (atiixp_rd(sc, ATI_REG_CMD) & (
			ATI_REG_CMD_SEND_EN | ATI_REG_CMD_RECEIVE_EN |
			ATI_REG_CMD_SPDF_OUT_EN))
		value |= ATI_REG_IER_SET_BUS_BUSY;
	else
		value &= ~ATI_REG_IER_SET_BUS_BUSY;
	atiixp_wr(sc, ATI_REG_IER, value);

	atiixp_unlock(sc);

	return 0;
}

static int
atiixp_chan_getptr(kobj_t obj, void *data)
{
	struct atiixp_chinfo *ch = data;
	struct atiixp_info *sc = ch->parent;
	uint32_t ptr;

	atiixp_lock(sc);
	ptr = atiixp_rd(sc, ch->dma_dt_cur_bit);
	atiixp_unlock(sc);

	return ptr;
}

static struct pcmchan_caps *
atiixp_chan_getcaps(kobj_t obj, void *data)
{
	struct atiixp_chinfo *ch = data;

	if (ch->caps_32bit)
		return &atiixp_caps_32bit;
	return &atiixp_caps;
}

static kobj_method_t atiixp_chan_methods[] = {
	KOBJMETHOD(channel_init,		atiixp_chan_init),
	KOBJMETHOD(channel_setformat,		atiixp_chan_setformat),
	KOBJMETHOD(channel_setspeed,		atiixp_chan_setspeed),
	KOBJMETHOD(channel_setblocksize,	atiixp_chan_setblocksize),
	KOBJMETHOD(channel_trigger,		atiixp_chan_trigger),
	KOBJMETHOD(channel_getptr,		atiixp_chan_getptr),
	KOBJMETHOD(channel_getcaps,		atiixp_chan_getcaps),
	{ 0, 0 }
};
CHANNEL_DECLARE(atiixp_chan);

/*
 * PCI driver interface
 */
static void
atiixp_intr(void *p)
{
	struct atiixp_info *sc = p;
	uint32_t status, enable, detected_codecs;

	atiixp_lock(sc);
	status = atiixp_rd(sc, ATI_REG_ISR);

	if (status == 0) {
		atiixp_unlock(sc);
		return;
	}

	if ((status & ATI_REG_ISR_IN_STATUS) && sc->rch.channel) {
		atiixp_unlock(sc);
		chn_intr(sc->rch.channel);
		atiixp_lock(sc);
	}
	if ((status & ATI_REG_ISR_OUT_STATUS) && sc->pch.channel) {
		atiixp_unlock(sc);
		chn_intr(sc->pch.channel);
		atiixp_lock(sc);
	}

#if 0
	if (status & ATI_REG_ISR_IN_XRUN) {
		device_printf(sc->dev,
			"Recieve IN XRUN interrupt\n");
	}
	if (status & ATI_REG_ISR_OUT_XRUN) {
		device_printf(sc->dev,
			"Recieve OUT XRUN interrupt\n");
	}
#endif

	if (status & CODEC_CHECK_BITS) {
		/* mark missing codecs as not ready */
		detected_codecs = status & CODEC_CHECK_BITS;
		sc->codec_not_ready_bits |= detected_codecs;

		/* disable detected interupt sources */
		enable  = atiixp_rd(sc, ATI_REG_IER);
		enable &= ~detected_codecs;
		atiixp_wr(sc, ATI_REG_IER, enable);
	}

	/* acknowledge */
	atiixp_wr(sc, ATI_REG_ISR, status);
	atiixp_unlock(sc);
}

static void
atiixp_dma_cb(void *p, bus_dma_segment_t *bds, int a, int b)
{
	struct atiixp_info *sc = (struct atiixp_info *)p;
	sc->sgd_addr = bds->ds_addr;
}

static void
atiixp_chip_pre_init(struct atiixp_info *sc)
{
	uint32_t value;

	atiixp_lock(sc);

	/* disable interrupts */
	atiixp_disable_interrupts(sc);

	/* clear all DMA enables (preserving rest of settings) */
	value = atiixp_rd(sc, ATI_REG_CMD);
	value &= ~(ATI_REG_CMD_IN_DMA_EN | ATI_REG_CMD_OUT_DMA_EN |
						ATI_REG_CMD_SPDF_OUT_EN );
	atiixp_wr(sc, ATI_REG_CMD, value);

	/* reset aclink */
	atiixp_reset_aclink(sc);

	sc->codec_not_ready_bits = 0;

	/* enable all codecs to interrupt as well as the new frame interrupt */
	atiixp_wr(sc, ATI_REG_IER, CODEC_CHECK_BITS);

	atiixp_unlock(sc);
}

static void
atiixp_chip_post_init(void *arg)
{
	struct atiixp_info *sc = (struct atiixp_info *)arg;
	int i, timeout, found;
	char status[SND_STATUSLEN];

	atiixp_lock(sc);

	if (sc->delayed_attach.ich_func) {
		config_intrhook_disestablish(&sc->delayed_attach);
		sc->delayed_attach.ich_func = NULL;
	}

	/* wait for the interrupts to happen */
	timeout = 100;		/* 100.000 usec -> 0.1 sec */

	while (--timeout) {
		atiixp_unlock(sc);
		DELAY(1000);
		atiixp_lock(sc);
		if (sc->codec_not_ready_bits)
			break;
	}

	atiixp_disable_interrupts(sc);

	if (timeout == 0) {
		device_printf(sc->dev,
			"WARNING: timeout during codec detection; "
			"codecs might be present but haven't interrupted\n");
		atiixp_unlock(sc);
		return;
	}

	found = 0;

	/*
	 * ATI IXP can have upto 3 codecs, but single codec should be
	 * suffice for now.
	 */
	if (!(sc->codec_not_ready_bits &
				ATI_REG_ISR_CODEC0_NOT_READY)) {
		/* codec 0 present */
		sc->codec_found++;
		sc->codec_idx = 0;
		found++;
	}

	if (!(sc->codec_not_ready_bits &
				ATI_REG_ISR_CODEC1_NOT_READY)) {
		/* codec 1 present */
		sc->codec_found++;
	}

	if (!(sc->codec_not_ready_bits &
				ATI_REG_ISR_CODEC2_NOT_READY)) {
		/* codec 2 present */
		sc->codec_found++;
	}

	atiixp_unlock(sc);

	if (found == 0)
		return;

	/* create/init mixer */
	sc->codec = AC97_CREATE(sc->dev, sc, atiixp_ac97);
	if (sc->codec == NULL)
		goto postinitbad;

	mixer_init(sc->dev, ac97_getmixerclass(), sc->codec);

	if (pcm_register(sc->dev, sc, ATI_IXP_NPCHAN, ATI_IXP_NRCHAN))
		goto postinitbad;

	for (i = 0; i < ATI_IXP_NPCHAN; i++)
		pcm_addchan(sc->dev, PCMDIR_PLAY, &atiixp_chan_class, sc);
	for (i = 0; i < ATI_IXP_NRCHAN; i++)
		pcm_addchan(sc->dev, PCMDIR_REC, &atiixp_chan_class, sc);

	snprintf(status, SND_STATUSLEN, "at memory 0x%lx irq %ld %s", 
			rman_get_start(sc->reg), rman_get_start(sc->irq),
			PCM_KLDSTRING(snd_atiixp));

	pcm_setstatus(sc->dev, status);

	atiixp_lock(sc);
	atiixp_enable_interrupts(sc);
	atiixp_unlock(sc);

	return;

postinitbad:
	if (sc->codec)
		ac97_destroy(sc->codec);
	if (sc->ih)
		bus_teardown_intr(sc->dev, sc->irq, sc->ih);
	if (sc->reg)
		bus_release_resource(sc->dev, sc->regtype, sc->regid, sc->reg);
	if (sc->irq)
		bus_release_resource(sc->dev, SYS_RES_IRQ, sc->irqid, sc->irq);
	if (sc->parent_dmat)
		bus_dma_tag_destroy(sc->parent_dmat);
	if (sc->sgd_dmamap)
		bus_dmamap_unload(sc->sgd_dmat, sc->sgd_dmamap);
	if (sc->sgd_dmat)
		bus_dma_tag_destroy(sc->sgd_dmat);
	if (sc->lock)
		snd_mtxfree(sc->lock);
	free(sc, M_DEVBUF);
}

static int
atiixp_pci_probe(device_t dev)
{
	int i;
	uint16_t devid, vendor;

	vendor = pci_get_vendor(dev);
	devid = pci_get_device(dev);
	for (i = 0; i < sizeof(atiixp_hw)/sizeof(atiixp_hw[0]); i++) {
		if (vendor == atiixp_hw[i].vendor &&
					devid == atiixp_hw[i].devid) {
			device_set_desc(dev, atiixp_hw[i].desc);
			return BUS_PROBE_DEFAULT;
		}
	}

	return ENXIO;
}

static int
atiixp_pci_attach(device_t dev)
{
	struct atiixp_info *sc;
	int i;

	if ((sc = malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT | M_ZERO)) == NULL) {
		device_printf(dev, "cannot allocate softc\n");
		return ENXIO;
	}

	sc->lock = snd_mtxcreate(device_get_nameunit(dev), "sound softc");
	sc->dev = dev;
	/*
	 * Default DMA segments per playback / recording channel
	 */
	sc->dma_segs = ATI_IXP_DMA_CHSEGS;

	pci_set_powerstate(dev, PCI_POWERSTATE_D0);
	pci_enable_busmaster(dev);

	sc->regid = PCIR_BAR(0);
	sc->regtype = SYS_RES_MEMORY;
	sc->reg = bus_alloc_resource_any(dev, sc->regtype, &sc->regid,
								RF_ACTIVE);

	if (!sc->reg) {
		device_printf(dev, "unable to allocate register space\n");
		goto bad;
	}

	sc->st = rman_get_bustag(sc->reg);
	sc->sh = rman_get_bushandle(sc->reg);

	sc->bufsz = pcm_getbuffersize(dev, 4096, ATI_IXP_DEFAULT_BUFSZ, 65536);

	sc->irqid = 0;
	sc->irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->irqid,
						RF_ACTIVE | RF_SHAREABLE);
	if (!sc->irq || 
			snd_setup_intr(dev, sc->irq, INTR_MPSAFE,
						atiixp_intr, sc, &sc->ih)) {
		device_printf(dev, "unable to map interrupt\n");
		goto bad;
	}

	/*
	 * Let the user choose the best DMA segments.
	 */
	 if (resource_int_value(device_get_name(dev),
			device_get_unit(dev), "dma_segs",
			&i) == 0) {
		if (i < ATI_IXP_DMA_CHSEGS_MIN)
			i = ATI_IXP_DMA_CHSEGS_MIN;
		if (i > ATI_IXP_DMA_CHSEGS_MAX)
			i = ATI_IXP_DMA_CHSEGS_MAX;
		/* round the value */
		sc->dma_segs = i & ~1;
	}

	/*
	 * DMA tag for scatter-gather buffers and link pointers
	 */
	if (bus_dma_tag_create(/*parent*/NULL, /*alignment*/2, /*boundary*/0,
		/*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
		/*highaddr*/BUS_SPACE_MAXADDR,
		/*filter*/NULL, /*filterarg*/NULL,
		/*maxsize*/sc->bufsz, /*nsegments*/1, /*maxsegz*/0x3ffff,
		/*flags*/0, /*lockfunc*/NULL,
		/*lockarg*/NULL, &sc->parent_dmat) != 0) {
		device_printf(dev, "unable to create dma tag\n");
		goto bad;
	}

	if (bus_dma_tag_create(/*parent*/NULL, /*alignment*/2, /*boundary*/0,
		/*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
		/*highaddr*/BUS_SPACE_MAXADDR,
		/*filter*/NULL, /*filterarg*/NULL,
		/*maxsize*/sc->dma_segs * ATI_IXP_NCHANS *
						sizeof(struct atiixp_dma_op),
		/*nsegments*/1, /*maxsegz*/0x3ffff,
		/*flags*/0, /*lockfunc*/NULL,
		/*lockarg*/NULL, &sc->sgd_dmat) != 0) {
		device_printf(dev, "unable to create dma tag\n");
		goto bad;
	}

	if (bus_dmamem_alloc(sc->sgd_dmat, (void **)&sc->sgd_table, 
				BUS_DMA_NOWAIT, &sc->sgd_dmamap) == -1)
		goto bad;

	if (bus_dmamap_load(sc->sgd_dmat, sc->sgd_dmamap, sc->sgd_table, 
				sc->dma_segs * ATI_IXP_NCHANS *
						sizeof(struct atiixp_dma_op),
				atiixp_dma_cb, sc, 0))
		goto bad;


	atiixp_chip_pre_init(sc);

	sc->delayed_attach.ich_func = atiixp_chip_post_init;
	sc->delayed_attach.ich_arg = sc;
	if (cold == 0 ||
			config_intrhook_establish(&sc->delayed_attach) != 0) {
		sc->delayed_attach.ich_func = NULL;
		atiixp_chip_post_init(sc);
	}

	return 0;

bad:
	if (sc->codec)
		ac97_destroy(sc->codec);
	if (sc->ih)
		bus_teardown_intr(dev, sc->irq, sc->ih);
	if (sc->reg)
		bus_release_resource(dev, sc->regtype, sc->regid, sc->reg);
	if (sc->irq)
		bus_release_resource(dev, SYS_RES_IRQ, sc->irqid, sc->irq);
	if (sc->parent_dmat)
		bus_dma_tag_destroy(sc->parent_dmat);
	if (sc->sgd_dmamap)
		bus_dmamap_unload(sc->sgd_dmat, sc->sgd_dmamap);
	if (sc->sgd_dmat)
		bus_dma_tag_destroy(sc->sgd_dmat);
	if (sc->lock)
		snd_mtxfree(sc->lock);
	free(sc, M_DEVBUF);

	return ENXIO;
}

static int
atiixp_pci_detach(device_t dev)
{
	int r;
	struct atiixp_info *sc;

	r = pcm_unregister(dev);
	if (r)
		return r;

	sc = pcm_getdevinfo(dev);

	atiixp_disable_interrupts(sc);

	bus_teardown_intr(dev, sc->irq, sc->ih);
	bus_release_resource(dev, sc->regtype, sc->regid, sc->reg);
	bus_release_resource(dev, SYS_RES_IRQ, sc->irqid, sc->irq);
	bus_dma_tag_destroy(sc->parent_dmat);
	bus_dmamap_unload(sc->sgd_dmat, sc->sgd_dmamap);
	bus_dma_tag_destroy(sc->sgd_dmat);
	snd_mtxfree(sc->lock);
	free(sc, M_DEVBUF);

	return 0;
}

static int
atiixp_pci_suspend(device_t dev)
{
	struct atiixp_info *sc = pcm_getdevinfo(dev);
	uint32_t value;

	/* quickly disable interrupts and save channels active state */
	atiixp_lock(sc);
	atiixp_disable_interrupts(sc);
	value = atiixp_rd(sc, ATI_REG_CMD);
	sc->pch.active = (value & ATI_REG_CMD_SEND_EN) ? 1 : 0;
	sc->rch.active = (value & ATI_REG_CMD_RECEIVE_EN) ? 1 : 0;
	atiixp_unlock(sc);

	/* stop everything */
	if (sc->pch.channel && sc->pch.active)
		atiixp_chan_trigger(NULL, &sc->pch, PCMTRIG_STOP);
	if (sc->rch.channel && sc->rch.active)
		atiixp_chan_trigger(NULL, &sc->rch, PCMTRIG_STOP);

	/* power down aclink and pci bus */
	atiixp_lock(sc);
	value = atiixp_rd(sc, ATI_REG_CMD);
	value |= ATI_REG_CMD_POWERDOWN | ATI_REG_CMD_AC_RESET;
	atiixp_wr(sc, ATI_REG_CMD, ATI_REG_CMD_POWERDOWN);
	pci_set_powerstate(dev, PCI_POWERSTATE_D3);
	atiixp_unlock(sc);

	return 0;
}

static int
atiixp_pci_resume(device_t dev)
{
	struct atiixp_info *sc = pcm_getdevinfo(dev);

	atiixp_lock(sc);
	/* power up pci bus */
	pci_set_powerstate(dev, PCI_POWERSTATE_D0);
	pci_enable_io(dev, SYS_RES_MEMORY);
	pci_enable_busmaster(dev);
	/* reset / power up aclink */
	atiixp_reset_aclink(sc);
	atiixp_unlock(sc);

	if (mixer_reinit(dev) == -1) {
		device_printf(dev, "unable to reinitialize the mixer\n");
		return ENXIO;
	}

	/*
	 * Resume channel activities. Reset channel format regardless
	 * of its previous state.
	 */
	if (sc->pch.channel) {
		if (sc->pch.fmt)
			atiixp_chan_setformat(NULL, &sc->pch, sc->pch.fmt);
		if (sc->pch.active)
			atiixp_chan_trigger(NULL, &sc->pch, PCMTRIG_START);
	}
	if (sc->rch.channel) {
		if (sc->rch.fmt)
			atiixp_chan_setformat(NULL, &sc->rch, sc->rch.fmt);
		if (sc->rch.active)
			atiixp_chan_trigger(NULL, &sc->rch, PCMTRIG_START);
	}

	/* enable interrupts */
	atiixp_lock(sc);
	atiixp_enable_interrupts(sc);
	atiixp_unlock(sc);

	return 0;
}

static device_method_t atiixp_methods[] = {
	DEVMETHOD(device_probe,		atiixp_pci_probe),
	DEVMETHOD(device_attach,	atiixp_pci_attach),
	DEVMETHOD(device_detach,	atiixp_pci_detach),
	DEVMETHOD(device_suspend,	atiixp_pci_suspend),
	DEVMETHOD(device_resume,	atiixp_pci_resume),
	{ 0, 0 }
};

static driver_t atiixp_driver = {
	"pcm",
	atiixp_methods,
	PCM_SOFTC_SIZE,
};

DRIVER_MODULE(snd_atiixp, pci, atiixp_driver, pcm_devclass, 0, 0);
MODULE_DEPEND(snd_atiixp, sound, SOUND_MINVER, SOUND_PREFVER, SOUND_MAXVER);
MODULE_VERSION(snd_atiixp, 1);
