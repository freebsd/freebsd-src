/*
 * Copyright (c) 2000 Katsurajima Naoto <raven@katsurajima.seya.yokohama.jp>
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
 *
 * $FreeBSD$
 */

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/ac97.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>


/* -------------------------------------------------------------------- */

#define ICH_RECPRIMARY 0

#define ICH_TIMEOUT 1000 /* semaphore timeout polling count */

#define PCIR_NAMBAR 0x10
#define PCIR_NABMBAR 0x14

/* Native Audio Bus Master Control Registers */
#define ICH_REG_PI_BDBAR 0x00
#define ICH_REG_PI_CIV   0x04
#define ICH_REG_PI_LVI   0x05
#define ICH_REG_PI_SR    0x06
#define ICH_REG_PI_PICB  0x08
#define ICH_REG_PI_PIV   0x0a
#define ICH_REG_PI_CR    0x0b
#define ICH_REG_PO_BDBAR 0x10
#define ICH_REG_PO_CIV   0x14
#define ICH_REG_PO_LVI   0x15
#define ICH_REG_PO_SR    0x16
#define ICH_REG_PO_PICB  0x18
#define ICH_REG_PO_PIV   0x1a
#define ICH_REG_PO_CR    0x1b
#define ICH_REG_MC_BDBAR 0x20
#define ICH_REG_MC_CIV   0x24
#define ICH_REG_MC_LVI   0x25
#define ICH_REG_MC_SR    0x26
#define ICH_REG_MC_PICB  0x28
#define ICH_REG_MC_PIV   0x2a
#define ICH_REG_MC_CR    0x2b
#define ICH_REG_GLOB_CNT 0x2c
#define ICH_REG_GLOB_STA 0x30
#define ICH_REG_ACC_SEMA 0x34
/* Status Register Values */
#define ICH_X_SR_DCH   0x0001
#define ICH_X_SR_CELV  0x0002
#define ICH_X_SR_LVBCI 0x0004
#define ICH_X_SR_BCIS  0x0008
#define ICH_X_SR_FIFOE 0x0010
/* Control Register Values */
#define ICH_X_CR_RPBM  0x01
#define ICH_X_CR_RR    0x02
#define ICH_X_CR_LVBIE 0x04
#define ICH_X_CR_FEIE  0x08
#define ICH_X_CR_IOCE  0x10
/* Global Control Register Values */
#define ICH_GLOB_CTL_GIE  0x00000001
#define ICH_GLOB_CTL_COLD 0x00000002 /* negate */
#define ICH_GLOB_CTL_WARM 0x00000004
#define ICH_GLOB_CTL_SHUT 0x00000008
#define ICH_GLOB_CTL_PRES 0x00000010
#define ICH_GLOB_CTL_SRES 0x00000020
/* Global Status Register Values */
#define ICH_GLOB_STA_GSCI   0x00000001
#define ICH_GLOB_STA_MIINT  0x00000002
#define ICH_GLOB_STA_MOINT  0x00000004
#define ICH_GLOB_STA_PIINT  0x00000020
#define ICH_GLOB_STA_POINT  0x00000040
#define ICH_GLOB_STA_MINT   0x00000080
#define ICH_GLOB_STA_PCR    0x00000100
#define ICH_GLOB_STA_SCR    0x00000200
#define ICH_GLOB_STA_PRES   0x00000400
#define ICH_GLOB_STA_SRES   0x00000800
#define ICH_GLOB_STA_SLOT12 0x00007000
#define ICH_GLOB_STA_RCODEC 0x00008000
#define ICH_GLOB_STA_AD3    0x00010000
#define ICH_GLOB_STA_MD3    0x00020000
#define ICH_GLOB_STA_IMASK  (ICH_GLOB_STA_MIINT | ICH_GLOB_STA_MOINT | ICH_GLOB_STA_PIINT | ICH_GLOB_STA_POINT | ICH_GLOB_STA_MINT | ICH_GLOB_STA_PRES | ICH_GLOB_STA_SRES)

/* AC'97 power/ready functions */
#define AC97_POWER_PINPOWER  0x0100
#define AC97_POWER_PINREADY  0x0001
#define AC97_POWER_POUTPOWER 0x0200
#define AC97_POWER_POUTREADY 0x0002

/* play/record buffer */
#define ICH_FIFOINDEX 32
#define ICH_BDC_IOC 0x80000000
#define ICH_BDC_BUP 0x40000000
#define ICH_DEFAULT_BLOCKSZ 2048
/* buffer descriptor */
struct ich_desc {
	volatile u_int32_t buffer;
	volatile u_int32_t length;
};

struct sc_info;

/* channel registers */
struct sc_chinfo {
	int run, spd, dir, fmt;
	struct snd_dbuf *buffer;
	struct pcm_channel *channel;
	struct sc_info *parent;
	struct ich_desc *index;
	bus_dmamap_t imap;
	u_int32_t lvi;
};

/* device private data */
struct sc_info {
	device_t	dev;
	u_int32_t 	type, rev;
	u_int32_t	cd2id, ctrlbase;

	struct resource *nambar, *nabmbar;
	int		nambarid, nabmbarid;
	bus_space_tag_t nambart, nabmbart;
	bus_space_handle_t nambarh, nabmbarh;
	bus_dma_tag_t dmat;
	struct resource *irq;
	int		irqid;
	void		*ih;

	struct ac97_info *codec;
	struct sc_chinfo *pi, *po;
};

struct {
	u_int32_t dev, subdev;
	char *name;
} ich_devs[] = {
	{0x71958086, 0, "Intel 443MX"},
	{0x24138086, 0, "Intel 82801AA (ICH)"},
	{0x24158086, 0, "Intel 82801AA (ICH)"},
	{0x24258086, 0, "Intel 82901AB (ICH)"},
	{0x24458086, 0, "Intel 82801BA (ICH2)"},
	{0, 0, NULL}
};

/* variable rate audio */
static u_int32_t ich_rate[] = {
    48000, 44100, 22050, 16000, 11025, 8000, 0
};

/* -------------------------------------------------------------------- */

/*
 * prototypes
 */

/* channel interface */
static void *ichpchan_init(kobj_t, void *, struct snd_dbuf *, struct pcm_channel *, int);
static int ichpchan_setformat(kobj_t, void *, u_int32_t);
static int ichpchan_setspeed(kobj_t, void *, u_int32_t);
static int ichpchan_setblocksize(kobj_t, void *, u_int32_t);
static int ichpchan_trigger(kobj_t, void *, int);
static int ichpchan_getptr(kobj_t, void *);
static struct pcmchan_caps *ichpchan_getcaps(kobj_t, void *);

static void *ichrchan_init(kobj_t, void *, struct snd_dbuf *, struct pcm_channel *, int);
static int ichrchan_setformat(kobj_t, void *, u_int32_t);
static int ichrchan_setspeed(kobj_t, void *, u_int32_t);
static int ichrchan_setblocksize(kobj_t, void *, u_int32_t);
static int ichrchan_trigger(kobj_t, void *, int);
static int ichrchan_getptr(kobj_t, void *);
static struct pcmchan_caps *ichrchan_getcaps(kobj_t, void *);

/* stuff */
static int       ich_init(struct sc_info *);
static void      ich_intr(void *);

/* -------------------------------------------------------------------- */

static u_int32_t ich_recfmt[] = {
	AFMT_STEREO | AFMT_S16_LE,
	0
};
static struct pcmchan_caps ich_reccaps = {8000, 48000, ich_recfmt, 0};

static u_int32_t ich_playfmt[] = {
	AFMT_STEREO | AFMT_S16_LE,
	0
};
static struct pcmchan_caps ich_playcaps = {8000, 48000, ich_playfmt, 0};

/* -------------------------------------------------------------------- */
/* Hardware */
static u_int32_t
ich_rd(struct sc_info *sc, int regno, int size)
{
	switch (size) {
	case 1:
		return bus_space_read_1(sc->nambart, sc->nambarh, regno);
	case 2:
		return bus_space_read_2(sc->nambart, sc->nambarh, regno);
	case 4:
		return bus_space_read_4(sc->nambart, sc->nambarh, regno);
	default:
		return 0xffffffff;
	}
}

static void
ich_wr(struct sc_info *sc, int regno, u_int32_t data, int size)
{
	switch (size) {
	case 1:
		bus_space_write_1(sc->nambart, sc->nambarh, regno, data);
		break;
	case 2:
		bus_space_write_2(sc->nambart, sc->nambarh, regno, data);
		break;
	case 4:
		bus_space_write_4(sc->nambart, sc->nambarh, regno, data);
		break;
	}
}

/* ac97 codec */
static int
ich_waitcd(void *devinfo)
{
	int i;
	u_int32_t data;
	struct sc_info *sc = (struct sc_info *)devinfo;
	for (i = 0;i < ICH_TIMEOUT;i++) {
		data = bus_space_read_1(sc->nabmbart, sc->nabmbarh,
		    ICH_REG_ACC_SEMA);
		if ((data & 0x01) == 0)
			return 0;
	}
	device_printf(sc->dev, "CODEC semaphore timeout\n");
	return ETIMEDOUT;
}

static int
ich_rdcd(kobj_t obj, void *devinfo, int regno)
{
	struct sc_info *sc = (struct sc_info *)devinfo;
	regno &= 0xff;
	ich_waitcd(sc);
	return ich_rd(sc, regno, 2);
}

static int
ich_wrcd(kobj_t obj, void *devinfo, int regno, u_int32_t data)
{
	struct sc_info *sc = (struct sc_info *)devinfo;
	regno &= 0xff;
	ich_waitcd(sc);
	ich_wr(sc, regno, data, 2);
	return 0;
}

static kobj_method_t ich_ac97_methods[] = {
	KOBJMETHOD(ac97_read,		ich_rdcd),
	KOBJMETHOD(ac97_write,		ich_wrcd),
	{ 0, 0 }
};
AC97_DECLARE(ich_ac97);

/* -------------------------------------------------------------------- */

/* channel common routines */
static void
ichchan_setmap(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct sc_chinfo *ch = arg;

	if (bootverbose) {
		device_printf(ch->parent->dev, "setmap(0x%lx, 0x%lx)\n", (unsigned long)segs->ds_addr, (unsigned long)segs->ds_len);
	}
}

static int
ichchan_initbuf(struct sc_chinfo *ch)
{
	struct sc_info *sc = ch->parent;
	int i;

	if (sndbuf_alloc(ch->buffer, sc->dmat, ICH_DEFAULT_BLOCKSZ * ICH_FIFOINDEX)) {
		return ENOSPC;
	}
	if (bus_dmamem_alloc(sc->dmat,(void **)&ch->index, BUS_DMA_NOWAIT, &ch->imap)) {
		sndbuf_free(ch->buffer);
		return ENOSPC;
	}
	if (bus_dmamap_load(sc->dmat, ch->imap, ch->index,
	    sizeof(struct ich_desc) * ICH_FIFOINDEX, ichchan_setmap, ch, 0)) {
		bus_dmamem_free(sc->dmat, (void **)&ch->index, ch->imap);
		sndbuf_free(ch->buffer);
		return ENOSPC;
	}
	for (i = 0;i < ICH_FIFOINDEX;i++) {
		ch->index[i].buffer = vtophys(sndbuf_getbuf(ch->buffer)) +
		    ICH_DEFAULT_BLOCKSZ * i;
		if (ch->dir == PCMDIR_PLAY)
			ch->index[i].length = 0;
		else
			ch->index[i].length = ICH_BDC_IOC +
			    ICH_DEFAULT_BLOCKSZ / 2;
	}
	return 0;
}

static void
ichchan_free(struct sc_chinfo *ch)
{
	struct sc_info *sc = ch->parent;

	bus_dmamap_unload(sc->dmat, ch->imap);
	bus_dmamem_free(sc->dmat, (void **)&ch->index, ch->imap);
	sndbuf_free(ch->buffer);
}

/* play channel interface */
static int
ichpchan_power(kobj_t obj, struct sc_info *sc, int sw)
{
	u_int32_t cr;
	int i;

	cr = ich_rdcd(obj, sc, AC97_REG_POWER);
	if (sw) { /* power on */
		cr &= ~AC97_POWER_POUTPOWER;
		ich_wrcd(obj, sc, AC97_REG_POWER, cr);
		for (i = 0;i < ICH_TIMEOUT;i++) {
			cr = ich_rdcd(obj, sc, AC97_REG_POWER);
			if ((cr & AC97_POWER_POUTREADY) != 0)
				break;
		}
	}
	else { /* power off */
		cr |= AC97_POWER_POUTPOWER;
		ich_wrcd(obj, sc, AC97_REG_POWER, cr);
		for (i = 0;i < ICH_TIMEOUT;i++) {
			cr = ich_rdcd(obj, sc, AC97_REG_POWER);
			if ((cr & AC97_POWER_POUTREADY) == 0)
				break;
		}
	}
	if (i == ICH_TIMEOUT)
		return -1;
	return 0;
}

static u_int32_t
ichpchan_getminspeed(kobj_t obj, void *data)
{
	struct sc_chinfo *ch = data;
	u_int32_t extcap;
	u_int32_t minspeed = 48000; /* before AC'97 R2.0 */
	int i = 0;

	extcap = ac97_getextmode(ch->parent->codec);
	if (extcap & AC97_EXTCAP_VRA) {
		for (i = 0;ich_rate[i] != 0;i++) {
			if (ac97_setrate(ch->parent->codec, AC97_REGEXT_FDACRATE, ich_rate[i]) == ich_rate[i])
				minspeed = ich_rate[i];
		}
	}
	return minspeed;
}

static void *
ichpchan_init(kobj_t obj, void *devinfo, struct snd_dbuf *b, struct pcm_channel *c, int dir)
{
	struct sc_info *sc = devinfo;
	struct sc_chinfo *ch;
	u_int32_t cr;
	int i;

	bus_space_write_1(sc->nabmbart, sc->nabmbarh, ICH_REG_PO_CR, 0);
	bus_space_write_1(sc->nabmbart, sc->nabmbarh, ICH_REG_PO_CR,
	    ICH_X_CR_RR);
	for (i = 0;i < ICH_TIMEOUT;i++) {
		cr = bus_space_read_1(sc->nabmbart, sc->nabmbarh,
		    ICH_REG_PO_CR);
		if (cr == 0)
			break;
	}
	if (i == ICH_TIMEOUT) {
		device_printf(sc->dev, "cannot reset play codec\n");
		return NULL;
	}
	if (ichpchan_power(obj, sc, 1) == -1) {
		device_printf(sc->dev, "play DAC not ready\n");
		return NULL;
	}
	ichpchan_power(obj, sc, 0);
	if ((ch = malloc(sizeof(*ch), M_DEVBUF, M_NOWAIT)) == NULL) {
		device_printf(sc->dev, "cannot allocate channel info area\n");
		return NULL;
	}
	ch->buffer = b;
	ch->channel = c;
	ch->parent = sc;
	ch->dir = PCMDIR_PLAY;
	ch->run = 0;
	ch->lvi = 0;
	if (ichchan_initbuf(ch)) {
		device_printf(sc->dev, "cannot allocate channel buffer\n");
		free(ch, M_DEVBUF);
		return NULL;
	}
	bus_space_write_4(sc->nabmbart, sc->nabmbarh, ICH_REG_PO_BDBAR,
	    (u_int32_t)vtophys(ch->index));
	sc->po = ch;
	if (bootverbose) {
		device_printf(sc->dev,"Play codec support rate(Hz): ");
		for (i = 0;ich_rate[i] != 0;i++) {
			if (ichpchan_setspeed(obj, ch, ich_rate[i]) == ich_rate[i]) {
				printf("%d ", ich_rate[i]);
			}
		}
		printf("\n");
	}
	ich_playcaps.minspeed = ichpchan_getminspeed(obj, ch);
	return ch;
}

static int
ichpchan_free(kobj_t obj, void *data)
{
	struct sc_chinfo *ch = data;
	ichchan_free(ch);
	return ichpchan_power(obj, ch->parent, 0);
}

static int
ichpchan_setformat(kobj_t obj, void *data, u_int32_t format)
{
	struct sc_chinfo *ch = data;

	ch->fmt = format;
	return 0;
}

static int
ichpchan_setspeed(kobj_t obj, void *data, u_int32_t speed)
{
	struct sc_chinfo *ch = data;
	u_int32_t extcap;

	extcap = ac97_getextmode(ch->parent->codec);
	if (extcap & AC97_EXTCAP_VRA) {
		ch->spd = (u_int32_t)ac97_setrate(ch->parent->codec, AC97_REGEXT_FDACRATE, speed);
	}
	else {
		ch->spd = 48000; /* before AC'97 R2.0 */
	}
#if(0)
	device_printf(ch->parent->dev, "ichpchan_setspeed():ch->spd = %d\n", ch->spd);
#endif
	return ch->spd;
}

static int
ichpchan_setblocksize(kobj_t obj, void *data, u_int32_t blocksize)
{
	return blocksize;
}

/* update index */
static void
ichpchan_update(struct sc_chinfo *ch)
{
	struct sc_info *sc = ch->parent;
	u_int32_t lvi;
	int fp;
	int last;
	int i;

	fp = sndbuf_getfreeptr(ch->buffer);
	last = fp - 1;
	if (last < 0)
		last = ICH_DEFAULT_BLOCKSZ * ICH_FIFOINDEX - 1;
	lvi = last / ICH_DEFAULT_BLOCKSZ;
	if (lvi >= ch->lvi) {
		for (i = ch->lvi;i < lvi;i++)
			ch->index[i].length =
			    ICH_BDC_IOC + ICH_DEFAULT_BLOCKSZ / 2;
		ch->index[i].length = ICH_BDC_IOC + ICH_BDC_BUP
		    + (last % ICH_DEFAULT_BLOCKSZ + 1) / 2;
	}
	else {
		for (i = ch->lvi;i < ICH_FIFOINDEX;i++)
			ch->index[i].length =
			    ICH_BDC_IOC + ICH_DEFAULT_BLOCKSZ / 2;
		for (i = 0;i < lvi;i++)
			ch->index[i].length =
			    ICH_BDC_IOC + ICH_DEFAULT_BLOCKSZ / 2;
		ch->index[i].length = ICH_BDC_IOC + ICH_BDC_BUP
		    + (last % ICH_DEFAULT_BLOCKSZ + 1) / 2;
	}
	bus_space_write_1(sc->nabmbart, sc->nabmbarh, ICH_REG_PO_LVI, lvi);
	ch->lvi = lvi;
#if(0)
	device_printf(ch->parent->dev, "ichpchan_update():fp = %d, lvi = %d\n", fp, lvi);
#endif
	return;
}
static void
ichpchan_fillblank(struct sc_chinfo *ch)
{
	struct sc_info *sc = ch->parent;

	ch->lvi++;
	if (ch->lvi == ICH_FIFOINDEX)
		ch->lvi = 0;
	ch->index[ch->lvi].length = ICH_BDC_BUP + ICH_DEFAULT_BLOCKSZ / 2;
	bus_space_write_1(sc->nabmbart, sc->nabmbarh, ICH_REG_PO_LVI, ch->lvi);
	return;
}
/* semantic note: must start at beginning of buffer */
static int
ichpchan_trigger(kobj_t obj, void *data, int go)
{
	struct sc_chinfo *ch = data;
	struct sc_info *sc = ch->parent;
	u_int32_t cr;
	int i;

#if(0)
	device_printf(ch->parent->dev, "ichpchan_trigger(0x%08x, %d)\n", data, go);
#endif
	switch (go) {
	case PCMTRIG_START:
#if(0)
		device_printf(ch->parent->dev, "ichpchan_trigger():PCMTRIG_START\n");
#endif
		ch->run = 1;
		ichpchan_power(obj, sc, 1);
		bus_space_write_4(sc->nabmbart, sc->nabmbarh, ICH_REG_PO_BDBAR,
		    (u_int32_t)vtophys(ch->index));
		ch->lvi = ICH_FIFOINDEX - 1;
		ichpchan_update(ch);
		bus_space_write_1(sc->nabmbart, sc->nabmbarh, ICH_REG_PO_CR,
		    ICH_X_CR_RPBM | ICH_X_CR_LVBIE | ICH_X_CR_IOCE |
		    ICH_X_CR_FEIE);
		break;
	case PCMTRIG_STOP:
#if(0)
		device_printf(ch->parent->dev, "ichpchan_trigger():PCMTRIG_STOP\n");
#endif
		cr = bus_space_read_1(sc->nabmbart, sc->nabmbarh,
		    ICH_REG_PO_CR);
		bus_space_write_1(sc->nabmbart, sc->nabmbarh, ICH_REG_PO_CR,
		    cr & ~ICH_X_CR_RPBM);
		ichpchan_power(obj, sc, 0);
		ch->run = 0;
		break;
	case PCMTRIG_ABORT:
#if(0)
		device_printf(ch->parent->dev, "ichpchan_trigger():PCMTRIG_ABORT\n");
#endif
		bus_space_write_1(sc->nabmbart, sc->nabmbarh, ICH_REG_PO_CR,
		    0);
		bus_space_write_1(sc->nabmbart, sc->nabmbarh, ICH_REG_PO_CR,
		    ICH_X_CR_RR);
		for (i = 0;i < ICH_TIMEOUT;i++) {
			cr = bus_space_read_1(sc->nabmbart, sc->nabmbarh,
			    ICH_REG_PO_CR);
			if (cr == 0)
				break;
		}
		ichpchan_power(obj, sc, 0);
		ch->run = 0;
		ch->lvi = 0;
		break;
	default:
		break;
	}
	return 0;
}

static int
ichpchan_getptr(kobj_t obj, void *data)
{
	struct sc_chinfo *ch = data;
	struct sc_info *sc = ch->parent;
	u_int32_t ci;

#if(0)
	device_printf(ch->parent->dev, "ichpchan_getptr(0x%08x)\n", data);
#endif
	ci = bus_space_read_1(sc->nabmbart, sc->nabmbarh, ICH_REG_PO_CIV);
#if(0)
	device_printf(ch->parent->dev, "ichpchan_getptr():ICH_REG_PO_CIV = %d\n", ci);
#endif
	return ICH_DEFAULT_BLOCKSZ * ci;
}

static struct pcmchan_caps *
ichpchan_getcaps(kobj_t obj, void *data)
{
	return &ich_playcaps;
}

static kobj_method_t ichpchan_methods[] = {
	KOBJMETHOD(channel_init,		ichpchan_init),
	KOBJMETHOD(channel_free,		ichpchan_free),
	KOBJMETHOD(channel_setformat,		ichpchan_setformat),
	KOBJMETHOD(channel_setspeed,		ichpchan_setspeed),
	KOBJMETHOD(channel_setblocksize,	ichpchan_setblocksize),
	KOBJMETHOD(channel_trigger,		ichpchan_trigger),
	KOBJMETHOD(channel_getptr,		ichpchan_getptr),
	KOBJMETHOD(channel_getcaps,		ichpchan_getcaps),
	{ 0, 0 }
};
CHANNEL_DECLARE(ichpchan);

/* -------------------------------------------------------------------- */
/* record channel interface */
static int
ichrchan_power(kobj_t obj, struct sc_info *sc, int sw)
{
	u_int32_t cr;
	int i;

	cr = ich_rdcd(obj, sc, AC97_REG_POWER);
	if (sw) { /* power on */
		cr &= ~AC97_POWER_PINPOWER;
		ich_wrcd(obj, sc, AC97_REG_POWER, cr);
		for (i = 0;i < ICH_TIMEOUT;i++) {
			cr = ich_rdcd(obj, sc, AC97_REG_POWER);
			if ((cr & AC97_POWER_PINREADY) != 0)
				break;
		}
	}
	else { /* power off */
		cr |= AC97_POWER_PINPOWER;
		ich_wrcd(obj, sc, AC97_REG_POWER, cr);
		for (i = 0;i < ICH_TIMEOUT;i++) {
			cr = ich_rdcd(obj, sc, AC97_REG_POWER);
			if ((cr & AC97_POWER_PINREADY) == 0)
				break;
		}
	}
	if (i == ICH_TIMEOUT)
		return -1;
	return 0;
}

static u_int32_t
ichrchan_getminspeed(kobj_t obj, void *data)
{
	struct sc_chinfo *ch = data;
	u_int32_t extcap;
	u_int32_t minspeed = 48000; /* before AC'97 R2.0 */
	int i = 0;

	extcap = ac97_getextmode(ch->parent->codec);
	if (extcap & AC97_EXTCAP_VRM) {
		for (i = 0;ich_rate[i] != 0;i++) {
			if (ac97_setrate(ch->parent->codec, AC97_REGEXT_LADCRATE, ich_rate[i]) == ich_rate[i])
				minspeed = ich_rate[i];
		}
	}
	return minspeed;
}

static void *
ichrchan_init(kobj_t obj, void *devinfo, struct snd_dbuf *b, struct pcm_channel *c, int dir)
{
	struct sc_info *sc = devinfo;
	struct sc_chinfo *ch;
	u_int32_t cr;
	int i;

	/* reset codec */
	bus_space_write_1(sc->nabmbart, sc->nabmbarh, ICH_REG_PI_CR, 0);
	bus_space_write_1(sc->nabmbart, sc->nabmbarh, ICH_REG_PI_CR,
	    ICH_X_CR_RR);
	for (i = 0;i < ICH_TIMEOUT;i++) {
		cr = bus_space_read_1(sc->nabmbart, sc->nabmbarh,
		    ICH_REG_PI_CR);
		if (cr == 0)
			break;
	}
	if (i == ICH_TIMEOUT) {
		device_printf(sc->dev, "cannot reset record codec\n");
		return NULL;
	}
	if (ichrchan_power(obj, sc, 1) == -1) {
		device_printf(sc->dev, "record ADC not ready\n");
		return NULL;
	}
	ichrchan_power(obj, sc, 0);
	if ((ch = malloc(sizeof(*ch), M_DEVBUF, M_NOWAIT)) == NULL) {
		device_printf(sc->dev, "cannot allocate channel info area\n");
		return NULL;
	}
	ch->buffer = b;
	ch->channel = c;
	ch->parent = sc;
	ch->dir = PCMDIR_REC;
	ch->run = 0;
	ch->lvi = 0;
	if (ichchan_initbuf(ch)) {
		device_printf(sc->dev, "cannot allocate channel buffer\n");
		free(ch, M_DEVBUF);
		return NULL;
	}
	bus_space_write_4(sc->nabmbart, sc->nabmbarh, ICH_REG_PI_BDBAR,
	    (u_int32_t)vtophys(ch->index));
	sc->pi = ch;
	if (bootverbose) {
		device_printf(sc->dev,"Record codec support rate(Hz): ");
		for (i = 0;ich_rate[i] != 0;i++) {
			if (ichrchan_setspeed(obj, ch, ich_rate[i]) == ich_rate[i]) {
				printf("%d ", ich_rate[i]);
			}
		}
		printf("\n");
	}
	ich_reccaps.minspeed = ichrchan_getminspeed(obj, ch);
	return ch;
}

static int
ichrchan_free(kobj_t obj, void *data)
{
	struct sc_chinfo *ch = data;
	ichchan_free(ch);
	return ichrchan_power(obj, ch->parent, 0);
}

static int
ichrchan_setformat(kobj_t obj, void *data, u_int32_t format)
{
	struct sc_chinfo *ch = data;

	ch->fmt = format;

	return 0;
}

static int
ichrchan_setspeed(kobj_t obj, void *data, u_int32_t speed)
{
	struct sc_chinfo *ch = data;
	u_int32_t extcap;

	extcap = ac97_getextmode(ch->parent->codec);
	if (extcap & AC97_EXTCAP_VRM) {
		ch->spd = (u_int32_t)ac97_setrate(ch->parent->codec, AC97_REGEXT_LADCRATE, speed);
	}
	else {
		ch->spd = 48000; /* before AC'97 R2.0 */
	}

	return ch->spd;
}

static int
ichrchan_setblocksize(kobj_t obj, void *data, u_int32_t blocksize)
{
	return blocksize;
}

/* semantic note: must start at beginning of buffer */
static int
ichrchan_trigger(kobj_t obj, void *data, int go)
{
	struct sc_chinfo *ch = data;
	struct sc_info *sc = ch->parent;
	u_int32_t cr;
	int i;

	switch (go) {
	case PCMTRIG_START:
		ch->run = 1;
		ichrchan_power(obj, sc, 1);
		bus_space_write_4(sc->nabmbart, sc->nabmbarh, ICH_REG_PI_BDBAR,
		    (u_int32_t)vtophys(ch->index));
		ch->lvi = ICH_FIFOINDEX - 1;
		bus_space_write_1(sc->nabmbart, sc->nabmbarh, ICH_REG_PI_LVI,
		    ch->lvi);
		bus_space_write_1(sc->nabmbart, sc->nabmbarh, ICH_REG_PI_CR,
		    ICH_X_CR_RPBM | ICH_X_CR_LVBIE | ICH_X_CR_IOCE |
		    ICH_X_CR_FEIE);
		break;
	case PCMTRIG_STOP:
		cr = bus_space_read_1(sc->nabmbart, sc->nabmbarh,
		    ICH_REG_PI_CR);
		bus_space_write_1(sc->nabmbart, sc->nabmbarh, ICH_REG_PI_CR,
		    cr & ~ICH_X_CR_RPBM);
		ichrchan_power(obj, sc, 0);
		ch->run = 0;
		break;
	case PCMTRIG_ABORT:
		bus_space_write_1(sc->nabmbart, sc->nabmbarh, ICH_REG_PI_CR,
		    0);
		bus_space_write_1(sc->nabmbart, sc->nabmbarh, ICH_REG_PI_CR,
		    ICH_X_CR_RR);
		for (i = 0;i < ICH_TIMEOUT;i++) {
			cr = bus_space_read_1(sc->nabmbart, sc->nabmbarh,
			    ICH_REG_PI_CR);
			if (cr == 0)
				break;
		}
		ichrchan_power(obj, sc, 0);
		ch->run = 0;
		ch->lvi = 0;
		break;
	default:
		break;
	}
	return 0;
}

static int
ichrchan_getptr(kobj_t obj, void *data)
{
	struct sc_chinfo *ch = data;
	struct sc_info *sc = ch->parent;
	u_int32_t ci;

	ci = bus_space_read_1(sc->nabmbart, sc->nabmbarh, ICH_REG_PI_CIV);
	return ci * ICH_DEFAULT_BLOCKSZ;
}

static struct pcmchan_caps *
ichrchan_getcaps(kobj_t obj, void *data)
{
	return &ich_reccaps;
}

static kobj_method_t ichrchan_methods[] = {
	KOBJMETHOD(channel_init,		ichrchan_init),
	KOBJMETHOD(channel_free,		ichrchan_free),
	KOBJMETHOD(channel_setformat,		ichrchan_setformat),
	KOBJMETHOD(channel_setspeed,		ichrchan_setspeed),
	KOBJMETHOD(channel_setblocksize,	ichrchan_setblocksize),
	KOBJMETHOD(channel_trigger,		ichrchan_trigger),
	KOBJMETHOD(channel_getptr,		ichrchan_getptr),
	KOBJMETHOD(channel_getcaps,		ichrchan_getcaps),
	{ 0, 0 }
};
CHANNEL_DECLARE(ichrchan);

/* -------------------------------------------------------------------- */
/* The interrupt handler */
static void
ich_intr(void *p)
{
	struct sc_info *sc = (struct sc_info *)p;
	struct sc_chinfo *ch;
	u_int32_t cp;
	u_int32_t sg;
	u_int32_t st;
	u_int32_t lvi = 0;

#if(0)
	device_printf(sc->dev, "ich_intr(0x%08x)\n", p);
#endif
	/* check interface status */
	sg = bus_space_read_4(sc->nabmbart, sc->nabmbarh, ICH_REG_GLOB_STA);
#if(0)
	device_printf(sc->dev, "ich_intr():REG_GLOB_STA = 0x%08x\n", sg);
#endif
	if (sg & ICH_GLOB_STA_POINT) {
		/* PCM Out INTerrupt */
		/* mask interrupt */
		cp = bus_space_read_1(sc->nabmbart, sc->nabmbarh,
		    ICH_REG_PO_CR);
		cp &= ~(ICH_X_CR_LVBIE | ICH_X_CR_IOCE | ICH_X_CR_FEIE);
		bus_space_write_1(sc->nabmbart, sc->nabmbarh, ICH_REG_PO_CR,
		    cp);
		/* check channel status */
		ch = sc->po;
		st = bus_space_read_2(sc->nabmbart, sc->nabmbarh,
		    ICH_REG_PO_SR);
#if(0)
		device_printf(sc->dev, "ich_intr():REG_PO_SR = 0x%02x\n", st);
#endif
		if (st & (ICH_X_SR_BCIS | ICH_X_SR_LVBCI)) {
			/* play buffer block complete */
			if (st & ICH_X_SR_LVBCI)
				lvi = ch->lvi;
			/* update buffer */
			chn_intr(ch->channel);
			ichpchan_update(ch);
			if (st & ICH_X_SR_LVBCI) {
				/* re-check underflow status */
				if (lvi == ch->lvi) {
					/* ch->buffer->underflow = 1; */
					ichpchan_fillblank(ch);
				}
			}
		}
		/* clear status bit */
		bus_space_write_2(sc->nabmbart, sc->nabmbarh, ICH_REG_PO_SR,
		    st & (ICH_X_SR_FIFOE | ICH_X_SR_BCIS | ICH_X_SR_LVBCI));
		/* set interrupt */
		cp |= (ICH_X_CR_LVBIE | ICH_X_CR_IOCE | ICH_X_CR_FEIE);
		bus_space_write_1(sc->nabmbart, sc->nabmbarh, ICH_REG_PO_CR,
		    cp);
	}
	if (sg & ICH_GLOB_STA_PIINT) {
		/* PCM In INTerrupt */
		/* mask interrupt */
		cp = bus_space_read_1(sc->nabmbart, sc->nabmbarh,
		    ICH_REG_PI_CR);
		cp &= ~(ICH_X_CR_LVBIE | ICH_X_CR_IOCE | ICH_X_CR_FEIE);
		bus_space_write_1(sc->nabmbart, sc->nabmbarh, ICH_REG_PI_CR,
		    cp);
		/* check channel status */
		ch = sc->pi;
		st = bus_space_read_2(sc->nabmbart, sc->nabmbarh,
		    ICH_REG_PI_SR);
#if(0)
		device_printf(sc->dev, "ich_intr():REG_PI_SR = 0x%02x\n", st);
#endif
		if (st & (ICH_X_SR_BCIS | ICH_X_SR_LVBCI)) {
			/* record buffer block filled */
			if (st & ICH_X_SR_LVBCI)
				lvi = ch->lvi;
			/* update space */
			chn_intr(ch->channel);
			ch->lvi = sndbuf_getreadyptr(ch->buffer) / ICH_DEFAULT_BLOCKSZ - 1;
			if (ch->lvi < 0)
				ch->lvi = ICH_FIFOINDEX - 1;
			bus_space_write_1(sc->nabmbart, sc->nabmbarh,
			    ICH_REG_PI_LVI, ch->lvi);
			if (st & ICH_X_SR_LVBCI) {
				/* re-check underflow status */
				if (lvi == ch->lvi) {
					ch->lvi++;
					if (ch->lvi == ICH_FIFOINDEX)
						ch->lvi = 0;
					bus_space_write_1(sc->nabmbart,
					    sc->nabmbarh, ICH_REG_PI_LVI,
					    ch->lvi);
				}
			}
		}
		/* clear status bit */
		bus_space_write_2(sc->nabmbart, sc->nabmbarh, ICH_REG_PI_SR,
		    st & (ICH_X_SR_FIFOE | ICH_X_SR_BCIS | ICH_X_SR_LVBCI));
		/* set interrupt */
		cp |= (ICH_X_CR_LVBIE | ICH_X_CR_IOCE | ICH_X_CR_FEIE);
		bus_space_write_1(sc->nabmbart, sc->nabmbarh, ICH_REG_PI_CR,
		    cp);
	}
}

/* -------------------------------------------------------------------- */

/*
 * Probe and attach the card
 */

static int
ich_init(struct sc_info *sc)
{
	u_int32_t stat;
	u_int32_t save;

	bus_space_write_4(sc->nabmbart, sc->nabmbarh,
	    ICH_REG_GLOB_CNT, ICH_GLOB_CTL_COLD);
	DELAY(600000);
	stat = bus_space_read_4(sc->nabmbart, sc->nabmbarh, ICH_REG_GLOB_STA);
	if ((stat & ICH_GLOB_STA_PCR) == 0)
		return -1;
	bus_space_write_4(sc->nabmbart, sc->nabmbarh,
	    ICH_REG_GLOB_CNT, ICH_GLOB_CTL_COLD | ICH_GLOB_CTL_PRES);
	save = bus_space_read_2(sc->nambart, sc->nambarh, AC97_MIX_MASTER);
	bus_space_write_2(sc->nambart, sc->nambarh, AC97_MIX_MASTER,
			  AC97_MUTE);
	if (ich_waitcd(sc) == ETIMEDOUT)
		return -1;
	DELAY(600); /* it is need for some system */
	stat = bus_space_read_2(sc->nambart, sc->nambarh, AC97_MIX_MASTER);
	if (stat != AC97_MUTE)
		return -1;
	bus_space_write_2(sc->nambart, sc->nambarh, AC97_MIX_MASTER, save);
	return 0;
}

static int
ich_finddev(u_int32_t dev, u_int32_t subdev)
{
	int i;

	for (i = 0; ich_devs[i].dev; i++) {
		if (ich_devs[i].dev == dev &&
		    (ich_devs[i].subdev == subdev || ich_devs[i].subdev == 0))
			return i;
	}
	return -1;
}

static int
ich_pci_probe(device_t dev)
{
	int i;
	u_int32_t subdev;

	subdev = (pci_get_subdevice(dev) << 16) | pci_get_subvendor(dev);
	i = ich_finddev(pci_get_devid(dev), subdev);
	if (i >= 0) {
		device_set_desc(dev, ich_devs[i].name);
		return 0;
	} else
		return ENXIO;
}

static int
ich_pci_attach(device_t dev)
{
	u_int32_t		data;
	u_int32_t		subdev;
	struct sc_info 		*sc;
	char 			status[SND_STATUSLEN];

	if ((sc = malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT)) == NULL) {
		device_printf(dev, "cannot allocate softc\n");
		return ENXIO;
	}

	bzero(sc, sizeof(*sc));
	sc->dev = dev;
	subdev = (pci_get_subdevice(dev) << 16) | pci_get_subvendor(dev);
	sc->type = ich_finddev(pci_get_devid(dev), subdev);
	sc->rev = pci_get_revid(dev);

	data = pci_read_config(dev, PCIR_COMMAND, 2);
	data |= (PCIM_CMD_PORTEN | PCIM_CMD_MEMEN | PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCIR_COMMAND, data, 2);
	data = pci_read_config(dev, PCIR_COMMAND, 2);

	sc->nambarid = PCIR_NAMBAR;
	sc->nabmbarid = PCIR_NABMBAR;
	sc->nambar = bus_alloc_resource(dev, SYS_RES_IOPORT,
	    &sc->nambarid, 0, ~0, 256, RF_ACTIVE);
	sc->nabmbar = bus_alloc_resource(dev, SYS_RES_IOPORT,
	    &sc->nabmbarid, 0, ~0, 64, RF_ACTIVE);
	if (!sc->nambar || !sc->nabmbar) {
		device_printf(dev, "unable to map IO port space\n");
		goto bad;
	}
	sc->nambart = rman_get_bustag(sc->nambar);
	sc->nambarh = rman_get_bushandle(sc->nambar);
	sc->nabmbart = rman_get_bustag(sc->nabmbar);
	sc->nabmbarh = rman_get_bushandle(sc->nabmbar);

	if (bus_dma_tag_create(/*parent*/NULL, /*alignment*/4, /*boundary*/0,
	    /*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
	    /*highaddr*/BUS_SPACE_MAXADDR,
	    /*filter*/NULL, /*filterarg*/NULL,
	    /*maxsize*/65536, /*nsegments*/1, /*maxsegsz*/0x3ffff,
	    /*flags*/0, &sc->dmat) != 0) {
		device_printf(dev, "unable to create dma tag\n");
		goto bad;
	}

	if (ich_init(sc) == -1) {
		device_printf(dev, "unable to initialize the card\n");
		goto bad;
	}

	sc->codec = AC97_CREATE(dev, sc, ich_ac97);
	if (sc->codec == NULL)
		goto bad;
	mixer_init(dev, ac97_getmixerclass(), sc->codec);
	/* check and set VRA function */
	if (ac97_setextmode(sc->codec, AC97_EXTCAP_VRA) == 0) {
		if (bootverbose) {
			device_printf(sc->dev, "set VRA function\n");
		}
	}
	if (ac97_setextmode(sc->codec, AC97_EXTCAP_VRM) == 0) {
		if (bootverbose) {
			device_printf(sc->dev, "set VRM function\n");
		}
	}

	sc->irqid = 0;
	sc->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->irqid,
				 0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
	if (!sc->irq ||
	    bus_setup_intr(dev, sc->irq, INTR_TYPE_TTY, ich_intr, sc, &sc->ih)) {
		device_printf(dev, "unable to map interrupt\n");
		goto bad;
	}

	snprintf(status, SND_STATUSLEN,
	    "at io 0x%lx-0x%lx, 0x%lx-0x%lx irq %ld",
	    rman_get_start(sc->nambar), rman_get_end(sc->nambar),
	    rman_get_start(sc->nabmbar), rman_get_end(sc->nabmbar),
	    rman_get_start(sc->irq));

	if (pcm_register(dev, sc, 1, 1))
		goto bad;
	pcm_addchan(dev, PCMDIR_PLAY, &ichpchan_class, sc);
	pcm_addchan(dev, PCMDIR_REC, &ichrchan_class, sc);
	pcm_setstatus(dev, status);

	return 0;

bad:
	if (sc->codec)
		ac97_destroy(sc->codec);
	if (sc->nambar)
		bus_release_resource(dev, SYS_RES_IOPORT,
		    sc->nambarid, sc->nambar);
	if (sc->nabmbar)
		bus_release_resource(dev, SYS_RES_IOPORT,
		    sc->nabmbarid, sc->nabmbar);
	if (sc->ih)
		bus_teardown_intr(dev, sc->irq, sc->ih);
	if (sc->irq)
		bus_release_resource(dev, SYS_RES_IRQ, sc->irqid, sc->irq);
	free(sc, M_DEVBUF);
	return ENXIO;
}

static int
ich_pci_detach(device_t dev)
{
	struct sc_info *sc;
	int r;

	r = pcm_unregister(dev);
	if (r)
		return r;
	sc = pcm_getdevinfo(dev);

	bus_release_resource(dev, SYS_RES_IOPORT, sc->nambarid, sc->nambar);
	bus_release_resource(dev, SYS_RES_IOPORT, sc->nabmbarid, sc->nabmbar);
	bus_dma_tag_destroy(sc->dmat);
	bus_teardown_intr(dev, sc->irq, sc->ih);
	bus_release_resource(dev, SYS_RES_IRQ, sc->irqid, sc->irq);
	free(sc, M_DEVBUF);
	return 0;
}

static int
ich_pci_resume(device_t dev)
{
	struct sc_info *sc;

	sc = pcm_getdevinfo(dev);

	/* Reinit audio device */
    	if (ich_init(sc) == -1) {
		device_printf(dev, "unable to reinitialize the card\n");
		return ENXIO;
	}
	/* Reinit mixer */
    	if (mixer_reinit(dev) == -1) {
		device_printf(dev, "unable to reinitialize the mixer\n");
		return ENXIO;
	}
	return 0;
}

static device_method_t ich_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ich_pci_probe),
	DEVMETHOD(device_attach,	ich_pci_attach),
	DEVMETHOD(device_detach,	ich_pci_detach),
	DEVMETHOD(device_resume,	ich_pci_resume),
	{ 0, 0 }
};

static driver_t ich_driver = {
	"pcm",
	ich_methods,
	sizeof(struct snddev_info),
};

DRIVER_MODULE(snd_ich, pci, ich_driver, pcm_devclass, 0, 0);
MODULE_DEPEND(snd_ich, snd_pcm, PCM_MINVER, PCM_PREFVER, PCM_MAXVER);
MODULE_VERSION(snd_ich, 1);
