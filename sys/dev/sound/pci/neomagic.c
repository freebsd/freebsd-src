/*
 * Copyright (c) 1999 Cameron Grant <gandalf@vilnya.demon.co.uk>
 * All rights reserved.
 *
 * Derived from the public domain Linux driver
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
#include <dev/sound/pci/neomagic.h>
#include <dev/sound/pci/neomagic-coeff.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

/* -------------------------------------------------------------------- */

#define	NM_BUFFSIZE	16384

#define NM256AV_PCI_ID 	0x800510c8
#define NM256ZX_PCI_ID 	0x800610c8

struct sc_info;

/* channel registers */
struct sc_chinfo {
	int spd, dir, fmt;
	struct snd_dbuf *buffer;
	struct pcm_channel *channel;
	struct sc_info *parent;
};

/* device private data */
struct sc_info {
	device_t	dev;
	u_int32_t 	type;

	struct resource *reg, *irq, *buf;
	int		regid, irqid, bufid;
	void		*ih;

	u_int32_t 	ac97_base, ac97_status, ac97_busy;
	u_int32_t	buftop, pbuf, rbuf, cbuf, acbuf;
	u_int32_t	playint, recint, misc1int, misc2int;
	u_int32_t	irsz, badintr;

	struct sc_chinfo pch, rch;
};

/* -------------------------------------------------------------------- */

/*
 * prototypes
 */

/* stuff */
static int 	 nm_loadcoeff(struct sc_info *sc, int dir, int num);
static int	 nm_setch(struct sc_chinfo *ch);
static int       nm_init(struct sc_info *);
static void      nm_intr(void *);

/* talk to the card */
static u_int32_t nm_rd(struct sc_info *, int, int);
static void 	 nm_wr(struct sc_info *, int, u_int32_t, int);
static u_int32_t nm_rdbuf(struct sc_info *, int, int);
static void 	 nm_wrbuf(struct sc_info *, int, u_int32_t, int);

static u_int32_t badcards[] = {
	0x0007103c,
	0x008f1028,
	0x00dd1014,
	0x8005110a,
};
#define NUM_BADCARDS (sizeof(badcards) / sizeof(u_int32_t))

/* The actual rates supported by the card. */
static int samplerates[9] = {
	8000,
	11025,
	16000,
	22050,
	24000,
	32000,
	44100,
	48000,
	99999999
};

/* -------------------------------------------------------------------- */

static u_int32_t nm_fmt[] = {
	AFMT_U8,
	AFMT_STEREO | AFMT_U8,
	AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE,
	0
};
static struct pcmchan_caps nm_caps = {4000, 48000, nm_fmt, 0};

/* -------------------------------------------------------------------- */

/* Hardware */
static u_int32_t
nm_rd(struct sc_info *sc, int regno, int size)
{
	bus_space_tag_t st = rman_get_bustag(sc->reg);
	bus_space_handle_t sh = rman_get_bushandle(sc->reg);

	switch (size) {
	case 1:
		return bus_space_read_1(st, sh, regno);
	case 2:
		return bus_space_read_2(st, sh, regno);
	case 4:
		return bus_space_read_4(st, sh, regno);
	default:
		return 0xffffffff;
	}
}

static void
nm_wr(struct sc_info *sc, int regno, u_int32_t data, int size)
{
	bus_space_tag_t st = rman_get_bustag(sc->reg);
	bus_space_handle_t sh = rman_get_bushandle(sc->reg);

	switch (size) {
	case 1:
		bus_space_write_1(st, sh, regno, data);
		break;
	case 2:
		bus_space_write_2(st, sh, regno, data);
		break;
	case 4:
		bus_space_write_4(st, sh, regno, data);
		break;
	}
}

static u_int32_t
nm_rdbuf(struct sc_info *sc, int regno, int size)
{
	bus_space_tag_t st = rman_get_bustag(sc->buf);
	bus_space_handle_t sh = rman_get_bushandle(sc->buf);

	switch (size) {
	case 1:
		return bus_space_read_1(st, sh, regno);
	case 2:
		return bus_space_read_2(st, sh, regno);
	case 4:
		return bus_space_read_4(st, sh, regno);
	default:
		return 0xffffffff;
	}
}

static void
nm_wrbuf(struct sc_info *sc, int regno, u_int32_t data, int size)
{
	bus_space_tag_t st = rman_get_bustag(sc->buf);
	bus_space_handle_t sh = rman_get_bushandle(sc->buf);

	switch (size) {
	case 1:
		bus_space_write_1(st, sh, regno, data);
		break;
	case 2:
		bus_space_write_2(st, sh, regno, data);
		break;
	case 4:
		bus_space_write_4(st, sh, regno, data);
		break;
	}
}

/* -------------------------------------------------------------------- */
/* ac97 codec */
static int
nm_waitcd(struct sc_info *sc)
{
	int cnt = 10;

	while (cnt-- > 0) {
		if (nm_rd(sc, sc->ac97_status, 2) & sc->ac97_busy)
			DELAY(100);
		else
			break;
	}
	return (nm_rd(sc, sc->ac97_status, 2) & sc->ac97_busy);
}

static u_int32_t
nm_initcd(kobj_t obj, void *devinfo)
{
	struct sc_info *sc = (struct sc_info *)devinfo;

	nm_wr(sc, 0x6c0, 0x01, 1);
	nm_wr(sc, 0x6cc, 0x87, 1);
	nm_wr(sc, 0x6cc, 0x80, 1);
	nm_wr(sc, 0x6cc, 0x00, 1);
	return 1;
}

static int
nm_rdcd(kobj_t obj, void *devinfo, int regno)
{
	struct sc_info *sc = (struct sc_info *)devinfo;
	u_int32_t x;

	if (!nm_waitcd(sc)) {
		x = nm_rd(sc, sc->ac97_base + regno, 2);
		DELAY(1000);
		return x;
	} else {
		device_printf(sc->dev, "ac97 codec not ready\n");
		return -1;
	}
}

static int
nm_wrcd(kobj_t obj, void *devinfo, int regno, u_int32_t data)
{
	struct sc_info *sc = (struct sc_info *)devinfo;
	int cnt = 3;

	if (!nm_waitcd(sc)) {
		while (cnt-- > 0) {
			nm_wr(sc, sc->ac97_base + regno, data, 2);
			if (!nm_waitcd(sc)) {
				DELAY(1000);
				return 0;
			}
		}
	}
	device_printf(sc->dev, "ac97 codec not ready\n");
	return -1;
}

static kobj_method_t nm_ac97_methods[] = {
    	KOBJMETHOD(ac97_init,		nm_initcd),
    	KOBJMETHOD(ac97_read,		nm_rdcd),
    	KOBJMETHOD(ac97_write,		nm_wrcd),
	{ 0, 0 }
};
AC97_DECLARE(nm_ac97);

/* -------------------------------------------------------------------- */

static void
nm_ackint(struct sc_info *sc, u_int32_t num)
{
	if (sc->type == NM256AV_PCI_ID) {
		nm_wr(sc, NM_INT_REG, num << 1, 2);
	} else if (sc->type == NM256ZX_PCI_ID) {
		nm_wr(sc, NM_INT_REG, num, 4);
	}
}

static int
nm_loadcoeff(struct sc_info *sc, int dir, int num)
{
	int ofs, sz, i;
	u_int32_t addr;

	addr = (dir == PCMDIR_PLAY)? 0x01c : 0x21c;
	if (dir == PCMDIR_REC)
		num += 8;
	sz = coefficientSizes[num];
	ofs = 0;
	while (num-- > 0)
		ofs+= coefficientSizes[num];
	for (i = 0; i < sz; i++)
		nm_wrbuf(sc, sc->cbuf + i, coefficients[ofs + i], 1);
	nm_wr(sc, addr, sc->cbuf, 4);
	if (dir == PCMDIR_PLAY)
		sz--;
	nm_wr(sc, addr + 4, sc->cbuf + sz, 4);
	return 0;
}

static int
nm_setch(struct sc_chinfo *ch)
{
	struct sc_info *sc = ch->parent;
	u_int32_t base;
	u_int8_t x;

	for (x = 0; x < 8; x++)
		if (ch->spd < (samplerates[x] + samplerates[x + 1]) / 2)
			break;

	if (x == 8) return 1;

	ch->spd = samplerates[x];
	nm_loadcoeff(sc, ch->dir, x);

	x <<= 4;
	x &= NM_RATE_MASK;
	if (ch->fmt & AFMT_16BIT) x |= NM_RATE_BITS_16;
	if (ch->fmt & AFMT_STEREO) x |= NM_RATE_STEREO;

	base = (ch->dir == PCMDIR_PLAY)? NM_PLAYBACK_REG_OFFSET : NM_RECORD_REG_OFFSET;
	nm_wr(sc, base + NM_RATE_REG_OFFSET, x, 1);
	return 0;
}

/* channel interface */
static void *
nmchan_init(kobj_t obj, void *devinfo, struct snd_dbuf *b, struct pcm_channel *c, int dir)
{
	struct sc_info *sc = devinfo;
	struct sc_chinfo *ch;
	u_int32_t chnbuf;

	chnbuf = (dir == PCMDIR_PLAY)? sc->pbuf : sc->rbuf;
	ch = (dir == PCMDIR_PLAY)? &sc->pch : &sc->rch;
	ch->buffer = b;
	sndbuf_setup(ch->buffer, (u_int8_t *)rman_get_virtual(sc->buf) + chnbuf, NM_BUFFSIZE);
	if (bootverbose)
		device_printf(sc->dev, "%s buf %p\n", (dir == PCMDIR_PLAY)?
			      "play" : "rec", sndbuf_getbuf(ch->buffer));
	ch->parent = sc;
	ch->channel = c;
	ch->dir = dir;
	return ch;
}

static int
nmchan_free(kobj_t obj, void *data)
{
	return 0;
}

static int
nmchan_setformat(kobj_t obj, void *data, u_int32_t format)
{
	struct sc_chinfo *ch = data;

	ch->fmt = format;
	return nm_setch(ch);
}

static int
nmchan_setspeed(kobj_t obj, void *data, u_int32_t speed)
{
	struct sc_chinfo *ch = data;

	ch->spd = speed;
	return nm_setch(ch)? 0 : ch->spd;
}

static int
nmchan_setblocksize(kobj_t obj, void *data, u_int32_t blocksize)
{
	return blocksize;
}

static int
nmchan_trigger(kobj_t obj, void *data, int go)
{
	struct sc_chinfo *ch = data;
	struct sc_info *sc = ch->parent;
	int ssz;

	if (go == PCMTRIG_EMLDMAWR || go == PCMTRIG_EMLDMARD)
		return 0;

	ssz = (ch->fmt & AFMT_16BIT)? 2 : 1;
	if (ch->fmt & AFMT_STEREO)
		ssz <<= 1;

	if (ch->dir == PCMDIR_PLAY) {
		if (go == PCMTRIG_START) {
			nm_wr(sc, NM_PBUFFER_START, sc->pbuf, 4);
			nm_wr(sc, NM_PBUFFER_END, sc->pbuf + NM_BUFFSIZE - ssz, 4);
			nm_wr(sc, NM_PBUFFER_CURRP, sc->pbuf, 4);
			nm_wr(sc, NM_PBUFFER_WMARK, sc->pbuf + NM_BUFFSIZE / 2, 4);
			nm_wr(sc, NM_PLAYBACK_ENABLE_REG, NM_PLAYBACK_FREERUN |
				NM_PLAYBACK_ENABLE_FLAG, 1);
			nm_wr(sc, NM_AUDIO_MUTE_REG, 0, 2);
		} else {
			nm_wr(sc, NM_PLAYBACK_ENABLE_REG, 0, 1);
			nm_wr(sc, NM_AUDIO_MUTE_REG, NM_AUDIO_MUTE_BOTH, 2);
		}
	} else {
		if (go == PCMTRIG_START) {
			nm_wr(sc, NM_RECORD_ENABLE_REG, NM_RECORD_FREERUN |
				NM_RECORD_ENABLE_FLAG, 1);
			nm_wr(sc, NM_RBUFFER_START, sc->rbuf, 4);
			nm_wr(sc, NM_RBUFFER_END, sc->rbuf + NM_BUFFSIZE, 4);
			nm_wr(sc, NM_RBUFFER_CURRP, sc->rbuf, 4);
			nm_wr(sc, NM_RBUFFER_WMARK, sc->rbuf + NM_BUFFSIZE / 2, 4);
		} else {
			nm_wr(sc, NM_RECORD_ENABLE_REG, 0, 1);
		}
	}
	return 0;
}

static int
nmchan_getptr(kobj_t obj, void *data)
{
	struct sc_chinfo *ch = data;
	struct sc_info *sc = ch->parent;

	if (ch->dir == PCMDIR_PLAY)
		return nm_rd(sc, NM_PBUFFER_CURRP, 4) - sc->pbuf;
	else
		return nm_rd(sc, NM_RBUFFER_CURRP, 4) - sc->rbuf;
}

static struct pcmchan_caps *
nmchan_getcaps(kobj_t obj, void *data)
{
	return &nm_caps;
}

static kobj_method_t nmchan_methods[] = {
    	KOBJMETHOD(channel_init,		nmchan_init),
    	KOBJMETHOD(channel_free,		nmchan_free),
    	KOBJMETHOD(channel_setformat,		nmchan_setformat),
    	KOBJMETHOD(channel_setspeed,		nmchan_setspeed),
    	KOBJMETHOD(channel_setblocksize,	nmchan_setblocksize),
    	KOBJMETHOD(channel_trigger,		nmchan_trigger),
    	KOBJMETHOD(channel_getptr,		nmchan_getptr),
    	KOBJMETHOD(channel_getcaps,		nmchan_getcaps),
	{ 0, 0 }
};
CHANNEL_DECLARE(nmchan);

/* The interrupt handler */
static void
nm_intr(void *p)
{
	struct sc_info *sc = (struct sc_info *)p;
	int status, x;

	status = nm_rd(sc, NM_INT_REG, sc->irsz);
	if (status == 0)
		return;

	if (status & sc->playint) {
		status &= ~sc->playint;
		nm_ackint(sc, sc->playint);
		chn_intr(sc->pch.channel);
	}
	if (status & sc->recint) {
		status &= ~sc->recint;
		nm_ackint(sc, sc->recint);
		chn_intr(sc->rch.channel);
	}
	if (status & sc->misc1int) {
		status &= ~sc->misc1int;
		nm_ackint(sc, sc->misc1int);
		x = nm_rd(sc, 0x400, 1);
		nm_wr(sc, 0x400, x | 2, 1);
	 	device_printf(sc->dev, "misc int 1\n");
	}
	if (status & sc->misc2int) {
		status &= ~sc->misc2int;
		nm_ackint(sc, sc->misc2int);
		x = nm_rd(sc, 0x400, 1);
		nm_wr(sc, 0x400, x & ~2, 1);
	 	device_printf(sc->dev, "misc int 2\n");
	}
	if (status) {
		nm_ackint(sc, status);
	 	device_printf(sc->dev, "unknown int\n");
	}
}

/* -------------------------------------------------------------------- */

/*
 * Probe and attach the card
 */

static int
nm_init(struct sc_info *sc)
{
	u_int32_t ofs, i;

	if (sc->type == NM256AV_PCI_ID) {
		sc->ac97_base = NM_MIXER_OFFSET;
		sc->ac97_status = NM_MIXER_STATUS_OFFSET;
		sc->ac97_busy = NM_MIXER_READY_MASK;

		sc->buftop = 2560 * 1024;

		sc->irsz = 2;
		sc->playint = NM_PLAYBACK_INT;
		sc->recint = NM_RECORD_INT;
		sc->misc1int = NM_MISC_INT_1;
		sc->misc2int = NM_MISC_INT_2;
	} else if (sc->type == NM256ZX_PCI_ID) {
		sc->ac97_base = NM_MIXER_OFFSET;
		sc->ac97_status = NM2_MIXER_STATUS_OFFSET;
		sc->ac97_busy = NM2_MIXER_READY_MASK;

		sc->buftop = (nm_rd(sc, 0xa0b, 2)? 6144 : 4096) * 1024;

		sc->irsz = 4;
		sc->playint = NM2_PLAYBACK_INT;
		sc->recint = NM2_RECORD_INT;
		sc->misc1int = NM2_MISC_INT_1;
		sc->misc2int = NM2_MISC_INT_2;
	} else return -1;
	sc->badintr = 0;
	ofs = sc->buftop - 0x0400;
	sc->buftop -= 0x1400;

 	if ((nm_rdbuf(sc, ofs, 4) & NM_SIG_MASK) == NM_SIGNATURE) {
		i = nm_rdbuf(sc, ofs + 4, 4);
		if (i != 0 && i != 0xffffffff)
			sc->buftop = i;
	}

	sc->cbuf = sc->buftop - NM_MAX_COEFFICIENT;
	sc->rbuf = sc->cbuf - NM_BUFFSIZE;
	sc->pbuf = sc->rbuf - NM_BUFFSIZE;
	sc->acbuf = sc->pbuf - (NM_TOTAL_COEFF_COUNT * 4);

	nm_wr(sc, 0, 0x11, 1);
	nm_wr(sc, NM_RECORD_ENABLE_REG, 0, 1);
	nm_wr(sc, 0x214, 0, 2);

	return 0;
}

static int
nm_pci_probe(device_t dev)
{
	struct sc_info *sc = NULL;
	char *s = NULL;
	u_int32_t subdev, i, data;

	subdev = (pci_get_subdevice(dev) << 16) | pci_get_subvendor(dev);
	switch (pci_get_devid(dev)) {
	case NM256AV_PCI_ID:
		i = 0;
		while ((i < NUM_BADCARDS) && (badcards[i] != subdev))
			i++;

		/* Try to catch other non-ac97 cards */

		if (i == NUM_BADCARDS) {
			if (!(sc = malloc(sizeof(*sc), M_DEVBUF,
					  M_NOWAIT | M_ZERO))) {
				device_printf(dev, "cannot allocate softc\n");
				return ENXIO;
			}

			data = pci_read_config(dev, PCIR_COMMAND, 2);
			pci_write_config(dev, PCIR_COMMAND, data |
					 PCIM_CMD_PORTEN | PCIM_CMD_MEMEN |
					 PCIM_CMD_BUSMASTEREN, 2);

			sc->regid = PCIR_MAPS + 4;
			sc->reg = bus_alloc_resource(dev, SYS_RES_MEMORY,
						     &sc->regid, 0, ~0, 1,
						     RF_ACTIVE);

			if (!sc->reg) {
				device_printf(dev, "unable to map register space\n");
				pci_write_config(dev, PCIR_COMMAND, data, 2);
				free(sc, M_DEVBUF);
				return ENXIO;
			}

			if ((nm_rd(sc, NM_MIXER_PRESENCE, 2) & 
				NM_PRESENCE_MASK) != NM_PRESENCE_VALUE) {
				i = 0;	/* non-ac97 card, but not listed */
				DEB(device_printf(dev, "subdev = 0x%x - badcard?\n",
				    subdev));
			}
			pci_write_config(dev, PCIR_COMMAND, data, 2);
			bus_release_resource(dev, SYS_RES_MEMORY, sc->regid,
					     sc->reg);
			free(sc, M_DEVBUF);
		}

		if (i == NUM_BADCARDS)
			s = "NeoMagic 256AV";
		DEB(else)
			DEB(device_printf(dev, "this is a non-ac97 NM256AV, not attaching\n"));

		break;

	case NM256ZX_PCI_ID:
		s = "NeoMagic 256ZX";
		break;
	}

	if (s) device_set_desc(dev, s);
	return s? 0 : ENXIO;
}

static int
nm_pci_attach(device_t dev)
{
	u_int32_t	data;
	struct sc_info *sc;
	struct ac97_info *codec = 0;
	char 		status[SND_STATUSLEN];

	if ((sc = malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT)) == NULL) {
		device_printf(dev, "cannot allocate softc\n");
		return ENXIO;
	}

	bzero(sc, sizeof(*sc));
	sc->dev = dev;
	sc->type = pci_get_devid(dev);

	data = pci_read_config(dev, PCIR_COMMAND, 2);
	data |= (PCIM_CMD_PORTEN|PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCIR_COMMAND, data, 2);
	data = pci_read_config(dev, PCIR_COMMAND, 2);

	sc->bufid = PCIR_MAPS;
	sc->buf = bus_alloc_resource(dev, SYS_RES_MEMORY, &sc->bufid,
				     0, ~0, 1, RF_ACTIVE);
	sc->regid = PCIR_MAPS + 4;
	sc->reg = bus_alloc_resource(dev, SYS_RES_MEMORY, &sc->regid,
				     0, ~0, 1, RF_ACTIVE);

	if (!sc->buf || !sc->reg) {
		device_printf(dev, "unable to map register space\n");
		goto bad;
	}

	if (nm_init(sc) == -1) {
		device_printf(dev, "unable to initialize the card\n");
		goto bad;
	}

	codec = AC97_CREATE(dev, sc, nm_ac97);
	if (codec == NULL) goto bad;
	if (mixer_init(dev, ac97_getmixerclass(), codec) == -1) goto bad;

	sc->irqid = 0;
	sc->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->irqid,
				 0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
	if (!sc->irq || snd_setup_intr(dev, sc->irq, 0, nm_intr, sc, &sc->ih)) {
		device_printf(dev, "unable to map interrupt\n");
		goto bad;
	}

	snprintf(status, SND_STATUSLEN, "at memory 0x%lx, 0x%lx irq %ld",
		 rman_get_start(sc->buf), rman_get_start(sc->reg),
		 rman_get_start(sc->irq));

	if (pcm_register(dev, sc, 1, 1)) goto bad;
	pcm_addchan(dev, PCMDIR_REC, &nmchan_class, sc);
	pcm_addchan(dev, PCMDIR_PLAY, &nmchan_class, sc);
	pcm_setstatus(dev, status);

	return 0;

bad:
	if (codec) ac97_destroy(codec);
	if (sc->buf) bus_release_resource(dev, SYS_RES_MEMORY, sc->bufid, sc->buf);
	if (sc->reg) bus_release_resource(dev, SYS_RES_MEMORY, sc->regid, sc->reg);
	if (sc->ih) bus_teardown_intr(dev, sc->irq, sc->ih);
	if (sc->irq) bus_release_resource(dev, SYS_RES_IRQ, sc->irqid, sc->irq);
	free(sc, M_DEVBUF);
	return ENXIO;
}

static int
nm_pci_detach(device_t dev)
{
	int r;
	struct sc_info *sc;

	r = pcm_unregister(dev);
	if (r)
		return r;

	sc = pcm_getdevinfo(dev);
	bus_release_resource(dev, SYS_RES_MEMORY, sc->bufid, sc->buf);
	bus_release_resource(dev, SYS_RES_MEMORY, sc->regid, sc->reg);
	bus_teardown_intr(dev, sc->irq, sc->ih);
	bus_release_resource(dev, SYS_RES_IRQ, sc->irqid, sc->irq);
	free(sc, M_DEVBUF);

	return 0;
}

static int
nm_pci_resume(device_t dev)
{
	struct sc_info *sc;

	sc = pcm_getdevinfo(dev);

	/* Reinit audio device */
    	if (nm_init(sc) == -1) {
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

static device_method_t nm_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		nm_pci_probe),
	DEVMETHOD(device_attach,	nm_pci_attach),
	DEVMETHOD(device_detach,	nm_pci_detach),
	DEVMETHOD(device_resume,	nm_pci_resume),
	{ 0, 0 }
};

static driver_t nm_driver = {
	"pcm",
	nm_methods,
	sizeof(struct snddev_info),
};

static devclass_t pcm_devclass;

DRIVER_MODULE(snd_neomagic, pci, nm_driver, pcm_devclass, 0, 0);
MODULE_DEPEND(snd_neomagic, snd_pcm, PCM_MINVER, PCM_PREFVER, PCM_MAXVER);
MODULE_VERSION(snd_neomagic, 1);
