/*
 * Copyright (c) 1999 Cameron Grant <gandalf@vilnya.demon.co.uk>
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
#include <dev/sound/pci/t4dwave.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

/* -------------------------------------------------------------------- */

#define TDX_PCI_ID 	0x20001023
#define TNX_PCI_ID 	0x20011023

#define TR_BUFFSIZE 	0xf000
#define TR_TIMEOUT_CDC	0xffff
#define TR_INTSAMPLES	0x2000
#define TR_MAXPLAYCH	4

struct tr_info;

/* channel registers */
struct tr_chinfo {
	u_int32_t cso, alpha, fms, fmc, ec;
	u_int32_t lba;
	u_int32_t eso, delta;
	u_int32_t rvol, cvol;
	u_int32_t gvsel, pan, vol, ctrl;
	int index, ss;
	snd_dbuf *buffer;
	pcm_channel *channel;
	struct tr_info *parent;
};

/* device private data */
struct tr_info {
	u_int32_t type;

	bus_space_tag_t st;
	bus_space_handle_t sh;
	bus_dma_tag_t parent_dmat;

	struct resource *reg, *irq;
	int		regtype, regid, irqid;
	void		*ih;

	u_int32_t playchns;
	struct tr_chinfo chinfo[TR_MAXPLAYCH];
	struct tr_chinfo recchinfo;
};

/* -------------------------------------------------------------------- */

/*
 * prototypes
 */

/* channel interface */
static void *trchan_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir);
static int trchan_setdir(void *data, int dir);
static int trchan_setformat(void *data, u_int32_t format);
static int trchan_setspeed(void *data, u_int32_t speed);
static int trchan_setblocksize(void *data, u_int32_t blocksize);
static int trchan_trigger(void *data, int go);
static int trchan_getptr(void *data);
static pcmchan_caps *trchan_getcaps(void *data);

/* talk to the codec - called from ac97.c */
static u_int32_t tr_rdcd(void *, int);
static void  	 tr_wrcd(void *, int, u_int32_t);

/* stuff */
static int       tr_init(struct tr_info *);
static void      tr_intr(void *);

/* talk to the card */
static u_int32_t tr_rd(struct tr_info *, int, int);
static void 	 tr_wr(struct tr_info *, int, u_int32_t, int);

/* manipulate playback channels */
static void 	 tr_clrint(struct tr_info *, char);
static void 	 tr_enaint(struct tr_info *, char, int);
static u_int32_t tr_testint(struct tr_info *, char);
static void	 tr_rdch(struct tr_info *, char, struct tr_chinfo *);
static void	 tr_wrch(struct tr_info *, char, struct tr_chinfo *);
static void 	 tr_selch(struct tr_info *, char);
static void 	 tr_startch(struct tr_info *, char);
static void 	 tr_stopch(struct tr_info *, char);

/* -------------------------------------------------------------------- */

static u_int32_t tr_recfmt[] = {
	AFMT_U8,
	AFMT_STEREO | AFMT_U8,
	AFMT_S8,
	AFMT_STEREO | AFMT_S8,
	AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE,
	AFMT_U16_LE,
	AFMT_STEREO | AFMT_U16_LE,
	0
};
static pcmchan_caps tr_reccaps = {4000, 48000, tr_recfmt, 0};

static u_int32_t tr_playfmt[] = {
	AFMT_U8,
	AFMT_STEREO | AFMT_U8,
	AFMT_S8,
	AFMT_STEREO | AFMT_S8,
	AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE,
	AFMT_U16_LE,
	AFMT_STEREO | AFMT_U16_LE,
	0
};
static pcmchan_caps tr_playcaps = {4000, 48000, tr_playfmt, 0};

static pcm_channel tr_chantemplate = {
	trchan_init,
	trchan_setdir,
	trchan_setformat,
	trchan_setspeed,
	trchan_setblocksize,
	trchan_trigger,
	trchan_getptr,
	trchan_getcaps,
	NULL, 			/* free */
	NULL, 			/* nop1 */
	NULL, 			/* nop2 */
	NULL, 			/* nop3 */
	NULL, 			/* nop4 */
	NULL, 			/* nop5 */
	NULL, 			/* nop6 */
	NULL, 			/* nop7 */
};

/* -------------------------------------------------------------------- */

static u_int32_t
tr_fmttobits(u_int32_t fmt)
{
	u_int32_t bits = 0;
	bits |= (fmt & AFMT_STEREO)? 0x4 : 0;
	bits |= (fmt & (AFMT_S8 | AFMT_S16_LE))? 0x2 : 0;
	bits |= (fmt & (AFMT_S16_LE | AFMT_U16_LE))? 0x8 : 0;
	return bits;
}

/* Hardware */

static u_int32_t
tr_rd(struct tr_info *tr, int regno, int size)
{
	switch(size) {
	case 1:
		return bus_space_read_1(tr->st, tr->sh, regno);
	case 2:
		return bus_space_read_2(tr->st, tr->sh, regno);
	case 4:
		return bus_space_read_4(tr->st, tr->sh, regno);
	default:
		return 0xffffffff;
	}
}

static void
tr_wr(struct tr_info *tr, int regno, u_int32_t data, int size)
{
	switch(size) {
	case 1:
		bus_space_write_1(tr->st, tr->sh, regno, data);
		break;
	case 2:
		bus_space_write_2(tr->st, tr->sh, regno, data);
		break;
	case 4:
		bus_space_write_4(tr->st, tr->sh, regno, data);
		break;
	}
}

/* ac97 codec */

static u_int32_t
tr_rdcd(void *devinfo, int regno)
{
	struct tr_info *tr = (struct tr_info *)devinfo;
	int i, j, treg, trw;

	switch (tr->type) {
	case TDX_PCI_ID:
		treg=TDX_REG_CODECRD;
		trw=TDX_CDC_RWSTAT;
		break;
	case TNX_PCI_ID:
		treg=(regno & 0x100)? TNX_REG_CODEC2RD : TNX_REG_CODEC1RD;
		trw=TNX_CDC_RWSTAT;
		break;
	default:
		printf("!!! tr_rdcd defaulted !!!\n");
		return 0xffffffff;
	}

	regno &= 0x7f;
	tr_wr(tr, treg, regno | trw, 4);
	j=trw;
	for (i=TR_TIMEOUT_CDC; (i > 0) && (j & trw); i--) j=tr_rd(tr, treg, 4);
	if (i == 0) printf("codec timeout during read of register %x\n", regno);
	return (j >> TR_CDC_DATA) & 0xffff;
}

static void
tr_wrcd(void *devinfo, int regno, u_int32_t data)
{
	struct tr_info *tr = (struct tr_info *)devinfo;
	int i, j, treg, trw;

	switch (tr->type) {
	case TDX_PCI_ID:
		treg=TDX_REG_CODECWR;
		trw=TDX_CDC_RWSTAT;
		break;
	case TNX_PCI_ID:
		treg=TNX_REG_CODECWR;
		trw=TNX_CDC_RWSTAT | ((regno & 0x100)? TNX_CDC_SEC : 0);
		break;
	default:
		printf("!!! tr_wrcd defaulted !!!");
		return;
	}

	regno &= 0x7f;
#if 0
	printf("tr_wrcd: reg %x was %x", regno, tr_rdcd(devinfo, regno));
#endif
	j=trw;
	for (i=TR_TIMEOUT_CDC; (i>0) && (j & trw); i--) j=tr_rd(tr, treg, 4);
	tr_wr(tr, treg, (data << TR_CDC_DATA) | regno | trw, 4);
#if 0
	printf(" - wrote %x, now %x\n", data, tr_rdcd(devinfo, regno));
#endif
	if (i==0) printf("codec timeout writing %x, data %x\n", regno, data);
}

/* playback channel interrupts */

static u_int32_t
tr_testint(struct tr_info *tr, char channel)
{
	return tr_rd(tr, (channel & 0x20)? TR_REG_ADDRINTB : TR_REG_ADDRINTA,
	             4) & (1<<(channel & 0x1f));
}

static void
tr_clrint(struct tr_info *tr, char channel)
{
	tr_wr(tr, (channel & 0x20)? TR_REG_ADDRINTB : TR_REG_ADDRINTA,
	      1<<(channel & 0x1f), 4);
}

static void
tr_enaint(struct tr_info *tr, char channel, int enable)
{
	u_int32_t reg = (channel & 0x20)? TR_REG_INTENB : TR_REG_INTENA;
	u_int32_t i = tr_rd(tr, reg, 4);
	channel &= 0x1f;
	i &= ~(1 << channel);
	i |= (enable? 1 : 0) << channel;
	tr_clrint(tr, channel);
	tr_wr(tr, reg, i, 4);
}

/* playback channels */

static void
tr_selch(struct tr_info *tr, char channel)
{
	int i=tr_rd(tr, TR_REG_CIR, 4);
	i &= ~TR_CIR_MASK;
	i |= channel & 0x3f;
	tr_wr(tr, TR_REG_CIR, i, 4);
}

static void
tr_startch(struct tr_info *tr, char channel)
{
	tr_wr(tr, (channel & 0x20)? TR_REG_STARTB : TR_REG_STARTA,
	      1<<(channel & 0x1f), 4);
}

static void
tr_stopch(struct tr_info *tr, char channel)
{
	tr_wr(tr, (channel & 0x20)? TR_REG_STOPB : TR_REG_STOPA,
	      1<<(channel & 0x1f), 4);
}

static void
tr_wrch(struct tr_info *tr, char channel, struct tr_chinfo *ch)
{
	u_int32_t cr[TR_CHN_REGS], i;

	ch->gvsel 	&= 0x00000001;
	ch->fmc		&= 0x00000003;
	ch->fms		&= 0x0000000f;
	ch->ctrl	&= 0x0000000f;
	ch->pan 	&= 0x0000007f;
	ch->rvol	&= 0x0000007f;
	ch->cvol 	&= 0x0000007f;
	ch->vol		&= 0x000000ff;
	ch->ec		&= 0x00000fff;
	ch->alpha	&= 0x00000fff;
	ch->delta	&= 0x0000ffff;
	ch->lba		&= 0x3fffffff;

	cr[1]=ch->lba;
	cr[3]=(ch->rvol<<7) | (ch->cvol);
	cr[4]=(ch->gvsel<<31)|(ch->pan<<24)|(ch->vol<<16)|(ch->ctrl<<12)|(ch->ec);

	switch (tr->type) {
	case TDX_PCI_ID:
		ch->cso &= 0x0000ffff;
		ch->eso &= 0x0000ffff;
		cr[0]=(ch->cso<<16) | (ch->alpha<<4) | (ch->fms);
		cr[2]=(ch->eso<<16) | (ch->delta);
		cr[3]|=0x0000c000;
		break;
	case TNX_PCI_ID:
		ch->cso &= 0x00ffffff;
		ch->eso &= 0x00ffffff;
		cr[0]=((ch->delta & 0xff)<<24) | (ch->cso);
		cr[2]=((ch->delta>>16)<<24) | (ch->eso);
		cr[3]|=(ch->alpha<<20) | (ch->fms<<16) | (ch->fmc<<14);
		break;
	}
	tr_selch(tr, channel);
	for (i=0; i<TR_CHN_REGS; i++)
		tr_wr(tr, TR_REG_CHNBASE+(i<<2), cr[i], 4);
}

static void
tr_rdch(struct tr_info *tr, char channel, struct tr_chinfo *ch)
{
	u_int32_t cr[5], i;
	tr_selch(tr, channel);
	for (i=0; i<5; i++) cr[i]=tr_rd(tr, TR_REG_CHNBASE+(i<<2), 4);
	ch->lba=	(cr[1] & 0x3fffffff);
	ch->fmc=	(cr[3] & 0x0000c000) >> 14;
	ch->rvol=	(cr[3] & 0x00003f80) >> 7;
	ch->cvol=	(cr[3] & 0x0000007f);
	ch->gvsel=	(cr[4] & 0x80000000) >> 31;
	ch->pan=	(cr[4] & 0x7f000000) >> 24;
	ch->vol=	(cr[4] & 0x00ff0000) >> 16;
	ch->ctrl=	(cr[4] & 0x0000f000) >> 12;
	ch->ec=		(cr[4] & 0x00000fff);
	switch(tr->type) {
	case TDX_PCI_ID:
		ch->cso=	(cr[0] & 0xffff0000) >> 16;
		ch->alpha=	(cr[0] & 0x0000fff0) >> 4;
		ch->fms=	(cr[0] & 0x0000000f);
		ch->eso=	(cr[2] & 0xffff0000) >> 16;
		ch->delta=	(cr[2] & 0x0000ffff);
		break;
	case TNX_PCI_ID:
		ch->cso=	(cr[0] & 0x00ffffff);
		ch->eso=	(cr[2] & 0x00ffffff);
		ch->delta=	((cr[2] & 0xff000000) >> 16) |
				((cr[0] & 0xff000000) >> 24);
		ch->alpha=	(cr[3] & 0xfff00000) >> 20;
		ch->fms=	(cr[3] & 0x000f0000) >> 16;
		break;
	}
}

/* channel interface */

void *
trchan_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir)
{
	struct tr_info *tr = devinfo;
	struct tr_chinfo *ch;
	if (dir == PCMDIR_PLAY) {
		ch = &tr->chinfo[tr->playchns];
		ch->index = tr->playchns++;
	} else {
		ch = &tr->recchinfo;
		ch->index = -1;
	}
	ch->buffer = b;
	ch->buffer->bufsize = TR_BUFFSIZE;
	ch->parent = tr;
	ch->channel = c;
	if (chn_allocbuf(ch->buffer, tr->parent_dmat) == -1) return NULL;
	else return ch;
}

static int
trchan_setdir(void *data, int dir)
{
	struct tr_chinfo *ch = data;
	struct tr_info *tr = ch->parent;
	if (dir == PCMDIR_PLAY && ch->index >= 0) {
		ch->fmc = ch->fms = ch->ec = ch->alpha = 0;
		ch->lba = vtophys(ch->buffer->buf);
		ch->cso = 0;
		ch->eso = ch->buffer->bufsize - 1;
		ch->rvol = ch->cvol = 0;
		ch->gvsel = 0;
		ch->pan = 0;
		ch->vol = 0;
		ch->ctrl = 0x01;
		ch->delta = 0;
		tr_wrch(tr, ch->index, ch);
		tr_enaint(tr, ch->index, 1);
	} else if (dir == PCMDIR_REC && ch->index == -1) {
		/* set up dma mode regs */
		u_int32_t i;
		tr_wr(tr, TR_REG_DMAR15, 0, 1);
		i = tr_rd(tr, TR_REG_DMAR11, 1) & 0x03;
		tr_wr(tr, TR_REG_DMAR11, i | 0x54, 1);
		/* set up base address */
	   	tr_wr(tr, TR_REG_DMAR0, vtophys(ch->buffer->buf), 4);
		/* set up buffer size */
		i = tr_rd(tr, TR_REG_DMAR4, 4) & ~0x00ffffff;
		tr_wr(tr, TR_REG_DMAR4, i | (ch->buffer->bufsize - 1), 4);
	} else return -1;
	return 0;
}

static int
trchan_setformat(void *data, u_int32_t format)
{
	struct tr_chinfo *ch = data;
	struct tr_info *tr = ch->parent;
	u_int32_t bits = tr_fmttobits(format);

	ch->ss = 1;
	ch->ss <<= (format & AFMT_STEREO)? 1 : 0;
	ch->ss <<= (format & AFMT_16BIT)? 1 : 0;
	if (ch->index >= 0) {
		tr_rdch(tr, ch->index, ch);
		ch->eso = (ch->buffer->bufsize / ch->ss) - 1;
		ch->ctrl = bits | 0x01;
   		tr_wrch(tr, ch->index, ch);
	} else {
		u_int32_t i;
		/* set # of samples between interrupts */
		i = (TR_INTSAMPLES >> ((bits & 0x08)? 1 : 0)) - 1;
		tr_wr(tr, TR_REG_SBBL, i | (i << 16), 4);
		/* set sample format */
		i = 0x18 | (bits << 4);
		tr_wr(tr, TR_REG_SBCTRL, i, 1);
	}
	return 0;
}

static int
trchan_setspeed(void *data, u_int32_t speed)
{
	struct tr_chinfo *ch = data;
	struct tr_info *tr = ch->parent;

	if (ch->index >= 0) {
		tr_rdch(tr, ch->index, ch);
		ch->delta = (speed << 12) / 48000;
   		tr_wrch(tr, ch->index, ch);
		return (ch->delta * 48000) >> 12;
	} else {
		/* setup speed */
		ch->delta = (48000 << 12) / speed;
		tr_wr(tr, TR_REG_SBDELTA, ch->delta, 2);
		return (48000 << 12) / ch->delta;
	}
	return 0;
}

static int
trchan_setblocksize(void *data, u_int32_t blocksize)
{
	struct tr_chinfo *ch = data;
	return ch->buffer->bufsize / 2;
}

static int
trchan_trigger(void *data, int go)
{
	struct tr_chinfo *ch = data;
	struct tr_info *tr = ch->parent;

	if (go == PCMTRIG_EMLDMAWR || go == PCMTRIG_EMLDMARD)
		return 0;

	if (ch->index >= 0) {
		if (go == PCMTRIG_START) {
			tr_rdch(tr, ch->index, ch);
			ch->cso = 0;
   			tr_wrch(tr, ch->index, ch);
			tr_startch(tr, ch->index);
		} else tr_stopch(tr, ch->index);
	} else {
		u_int32_t i = tr_rd(tr, TR_REG_SBCTRL, 1) & ~7;
		tr_wr(tr, TR_REG_SBCTRL, i | (go == PCMTRIG_START)? 1 : 0, 1);
	}
	return 0;
}

static int
trchan_getptr(void *data)
{
	struct tr_chinfo *ch = data;
	struct tr_info *tr = ch->parent;

	if (ch->index >= 0) {
		tr_rdch(tr, ch->index, ch);
		return ch->cso * ch->ss;
	} else return tr_rd(tr, TR_REG_DMAR0, 4) - vtophys(ch->buffer->buf);
}

static pcmchan_caps *
trchan_getcaps(void *data)
{
	struct tr_chinfo *ch = data;
	return (ch->index >= 0)? &tr_playcaps : &tr_reccaps;
}

/* The interrupt handler */

static void
tr_intr(void *p)
{
	struct tr_info *tr = (struct tr_info *)p;
	u_int32_t	intsrc = tr_rd(tr, TR_REG_MISCINT, 4);

	if (intsrc & TR_INT_ADDR) {
		int i;
		for (i = 0; i < tr->playchns; i++) {
			if (tr_testint(tr, i)) {
				chn_intr(tr->chinfo[i].channel);
				tr_clrint(tr, i);
			}
		}
	}
	if (intsrc & TR_INT_SB) {
		chn_intr(tr->recchinfo.channel);
		tr_rd(tr, TR_REG_SBR9, 1);
		tr_rd(tr, TR_REG_SBR10, 1);
	}
}

/* -------------------------------------------------------------------- */

/*
 * Probe and attach the card
 */

static int
tr_init(struct tr_info *tr)
{
	if (tr->type == TDX_PCI_ID) {
		tr_wr(tr, TDX_REG_CODECST, TDX_CDC_ON, 4);
	} else tr_wr(tr, TNX_REG_CODECST, TNX_CDC_ON, 4);

	tr_wr(tr, TR_REG_CIR, TR_CIR_MIDENA | TR_CIR_ADDRENA, 4);
	tr->playchns = 0;
	return 0;
}

static int
tr_pci_probe(device_t dev)
{
	if (pci_get_devid(dev) == TDX_PCI_ID) {
		device_set_desc(dev, "Trident 4DWave DX");
		return 0;
	}
	if (pci_get_devid(dev) == TNX_PCI_ID) {
		device_set_desc(dev, "Trident 4DWave NX");
		return 0;
	}

	return ENXIO;
}

static int
tr_pci_attach(device_t dev)
{
	u_int32_t	data;
	struct tr_info *tr;
	struct ac97_info *codec = 0;
	int		i;
	int		mapped;
	char 		status[SND_STATUSLEN];

	if ((tr = malloc(sizeof(*tr), M_DEVBUF, M_NOWAIT)) == NULL) {
		device_printf(dev, "cannot allocate softc\n");
		return ENXIO;
	}

	bzero(tr, sizeof(*tr));
	tr->type = pci_get_devid(dev);

	data = pci_read_config(dev, PCIR_COMMAND, 2);
	data |= (PCIM_CMD_PORTEN|PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCIR_COMMAND, data, 2);
	data = pci_read_config(dev, PCIR_COMMAND, 2);

	mapped = 0;
	/* XXX dfr: is this strictly necessary? */
	for (i = 0; (mapped == 0) && (i < PCI_MAXMAPS_0); i++) {
		tr->regid = PCIR_MAPS + i*4;
		tr->regtype = SYS_RES_MEMORY;
		tr->reg = bus_alloc_resource(dev, tr->regtype, &tr->regid,
					     0, ~0, 1, RF_ACTIVE);
		if (!tr->reg) {
			tr->regtype = SYS_RES_IOPORT;
			tr->reg = bus_alloc_resource(dev, tr->regtype,
						     &tr->regid, 0, ~0, 1,
						     RF_ACTIVE);
		}
		if (tr->reg) {
			tr->st = rman_get_bustag(tr->reg);
			tr->sh = rman_get_bushandle(tr->reg);
			mapped++;
		}
	}

	if (mapped == 0) {
		device_printf(dev, "unable to map register space\n");
		goto bad;
	}

	if (tr_init(tr) == -1) {
		device_printf(dev, "unable to initialize the card\n");
		goto bad;
	}

	codec = ac97_create(dev, tr, NULL, tr_rdcd, tr_wrcd);
	if (codec == NULL) goto bad;
	if (mixer_init(dev, &ac97_mixer, codec) == -1) goto bad;

	tr->irqid = 0;
	tr->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &tr->irqid,
				 0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
	if (!tr->irq ||
	    bus_setup_intr(dev, tr->irq, INTR_TYPE_TTY, tr_intr, tr, &tr->ih)) {
		device_printf(dev, "unable to map interrupt\n");
		goto bad;
	}

	if (bus_dma_tag_create(/*parent*/NULL, /*alignment*/2, /*boundary*/0,
		/*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
		/*highaddr*/BUS_SPACE_MAXADDR,
		/*filter*/NULL, /*filterarg*/NULL,
		/*maxsize*/TR_BUFFSIZE, /*nsegments*/1, /*maxsegz*/0x3ffff,
		/*flags*/0, &tr->parent_dmat) != 0) {
		device_printf(dev, "unable to create dma tag\n");
		goto bad;
	}

	snprintf(status, 64, "at %s 0x%lx irq %ld",
		 (tr->regtype == SYS_RES_IOPORT)? "io" : "memory",
		 rman_get_start(tr->reg), rman_get_start(tr->irq));

	if (pcm_register(dev, tr, TR_MAXPLAYCH, 1)) goto bad;
	pcm_addchan(dev, PCMDIR_REC, &tr_chantemplate, tr);
	for (i = 0; i < TR_MAXPLAYCH; i++)
		pcm_addchan(dev, PCMDIR_PLAY, &tr_chantemplate, tr);
	pcm_setstatus(dev, status);

	return 0;

bad:
	if (codec) ac97_destroy(codec);
	if (tr->reg) bus_release_resource(dev, tr->regtype, tr->regid, tr->reg);
	if (tr->ih) bus_teardown_intr(dev, tr->irq, tr->ih);
	if (tr->irq) bus_release_resource(dev, SYS_RES_IRQ, tr->irqid, tr->irq);
	if (tr->parent_dmat) bus_dma_tag_destroy(tr->parent_dmat);
	free(tr, M_DEVBUF);
	return ENXIO;
}

static int
tr_pci_detach(device_t dev)
{
	int r;
	struct tr_info *tr;

	r = pcm_unregister(dev);
	if (r)
		return r;

	tr = pcm_getdevinfo(dev);
	bus_release_resource(dev, tr->regtype, tr->regid, tr->reg);
	bus_teardown_intr(dev, tr->irq, tr->ih);
	bus_release_resource(dev, SYS_RES_IRQ, tr->irqid, tr->irq);
	bus_dma_tag_destroy(tr->parent_dmat);
	free(tr, M_DEVBUF);

	return 0;
}

static device_method_t tr_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		tr_pci_probe),
	DEVMETHOD(device_attach,	tr_pci_attach),
	DEVMETHOD(device_detach,	tr_pci_detach),

	{ 0, 0 }
};

static driver_t tr_driver = {
	"pcm",
	tr_methods,
	sizeof(snddev_info),
};

static devclass_t pcm_devclass;

DRIVER_MODULE(snd_t4dwave, pci, tr_driver, pcm_devclass, 0, 0);
MODULE_DEPEND(snd_t4dwave, snd_pcm, PCM_MINVER, PCM_PREFVER, PCM_MAXVER);
MODULE_VERSION(snd_t4dwave, 1);
