/*
 * Copyright (c) 2000 Katsurajima Naoto <raven@katsurajima.seya.yokohama.jp>
 * Copyright (c) 2001 Cameron Grant <cg@freebsd.org>
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

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/ac97.h>
#include <dev/sound/pci/ich.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

SND_DECLARE_FILE("$FreeBSD$");

/* -------------------------------------------------------------------- */

#define ICH_TIMEOUT 1000 /* semaphore timeout polling count */
#define ICH_DTBL_LENGTH 32
#define ICH_DEFAULT_BUFSZ 16384
#define ICH_MAX_BUFSZ 65536

#define SIS7012ID       0x70121039      /* SiS 7012 needs special handling */
#define ICH4ID		0x24c58086	/* ICH4 needs special handling too */

/* buffer descriptor */
struct ich_desc {
	volatile u_int32_t buffer;
	volatile u_int32_t length;
};

struct sc_info;

/* channel registers */
struct sc_chinfo {
	u_int32_t num:8, run:1, run_save:1;
	u_int32_t blksz, blkcnt, spd;
	u_int32_t regbase, spdreg;
	u_int32_t imask;
	u_int32_t civ;

	struct snd_dbuf *buffer;
	struct pcm_channel *channel;
	struct sc_info *parent;

	struct ich_desc *dtbl;
};

/* device private data */
struct sc_info {
	device_t dev;
	int hasvra, hasvrm, hasmic;
	unsigned int chnum, bufsz;
	int sample_size, swap_reg;

	struct resource *nambar, *nabmbar, *irq;
	int nambarid, nabmbarid, irqid;
	bus_space_tag_t nambart, nabmbart;
	bus_space_handle_t nambarh, nabmbarh;
	bus_dma_tag_t dmat;
	bus_dmamap_t dtmap;
	void *ih;

	struct ac97_info *codec;
	struct sc_chinfo ch[3];
	int ac97rate;
	struct ich_desc *dtbl;
	struct intr_config_hook	intrhook;
	int use_intrhook;
};

/* -------------------------------------------------------------------- */

static u_int32_t ich_fmt[] = {
	AFMT_STEREO | AFMT_S16_LE,
	0
};
static struct pcmchan_caps ich_vrcaps = {8000, 48000, ich_fmt, 0};
static struct pcmchan_caps ich_caps = {48000, 48000, ich_fmt, 0};

/* -------------------------------------------------------------------- */
/* Hardware */
static u_int32_t
ich_rd(struct sc_info *sc, int regno, int size)
{
	switch (size) {
	case 1:
		return bus_space_read_1(sc->nabmbart, sc->nabmbarh, regno);
	case 2:
		return bus_space_read_2(sc->nabmbart, sc->nabmbarh, regno);
	case 4:
		return bus_space_read_4(sc->nabmbart, sc->nabmbarh, regno);
	default:
		return 0xffffffff;
	}
}

static void
ich_wr(struct sc_info *sc, int regno, u_int32_t data, int size)
{
	switch (size) {
	case 1:
		bus_space_write_1(sc->nabmbart, sc->nabmbarh, regno, data);
		break;
	case 2:
		bus_space_write_2(sc->nabmbart, sc->nabmbarh, regno, data);
		break;
	case 4:
		bus_space_write_4(sc->nabmbart, sc->nabmbarh, regno, data);
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

	for (i = 0; i < ICH_TIMEOUT; i++) {
		data = ich_rd(sc, ICH_REG_ACC_SEMA, 1);
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

	return bus_space_read_2(sc->nambart, sc->nambarh, regno);
}

static int
ich_wrcd(kobj_t obj, void *devinfo, int regno, u_int16_t data)
{
	struct sc_info *sc = (struct sc_info *)devinfo;

	regno &= 0xff;
	ich_waitcd(sc);
	bus_space_write_2(sc->nambart, sc->nambarh, regno, data);

	return 0;
}

static kobj_method_t ich_ac97_methods[] = {
	KOBJMETHOD(ac97_read,		ich_rdcd),
	KOBJMETHOD(ac97_write,		ich_wrcd),
	{ 0, 0 }
};
AC97_DECLARE(ich_ac97);

/* -------------------------------------------------------------------- */
/* common routines */

static void
ich_filldtbl(struct sc_chinfo *ch)
{
	u_int32_t base;
	int i;

	base = vtophys(sndbuf_getbuf(ch->buffer));
	ch->blkcnt = sndbuf_getsize(ch->buffer) / ch->blksz;
	if (ch->blkcnt != 2 && ch->blkcnt != 4 && ch->blkcnt != 8 && ch->blkcnt != 16 && ch->blkcnt != 32) {
		ch->blkcnt = 2;
		ch->blksz = sndbuf_getsize(ch->buffer) / ch->blkcnt;
	}

	for (i = 0; i < ICH_DTBL_LENGTH; i++) {
		ch->dtbl[i].buffer = base + (ch->blksz * (i % ch->blkcnt));
		ch->dtbl[i].length = ICH_BDC_IOC
				   | (ch->blksz / ch->parent->sample_size);
	}
}

static int
ich_resetchan(struct sc_info *sc, int num)
{
	int i, cr, regbase;

	if (num == 0)
		regbase = ICH_REG_PO_BASE;
	else if (num == 1)
		regbase = ICH_REG_PI_BASE;
	else if (num == 2)
		regbase = ICH_REG_MC_BASE;
	else
		return ENXIO;

	ich_wr(sc, regbase + ICH_REG_X_CR, 0, 1);
	DELAY(100);
	ich_wr(sc, regbase + ICH_REG_X_CR, ICH_X_CR_RR, 1);
	for (i = 0; i < ICH_TIMEOUT; i++) {
		cr = ich_rd(sc, regbase + ICH_REG_X_CR, 1);
		if (cr == 0)
			return 0;
	}

	device_printf(sc->dev, "cannot reset channel %d\n", num);
	return ENXIO;
}

/* -------------------------------------------------------------------- */
/* channel interface */

static void *
ichchan_init(kobj_t obj, void *devinfo, struct snd_dbuf *b, struct pcm_channel *c, int dir)
{
	struct sc_info *sc = devinfo;
	struct sc_chinfo *ch;
	unsigned int num;

	num = sc->chnum++;
	ch = &sc->ch[num];
	ch->num = num;
	ch->buffer = b;
	ch->channel = c;
	ch->parent = sc;
	ch->run = 0;
	ch->dtbl = sc->dtbl + (ch->num * ICH_DTBL_LENGTH);
	ch->blkcnt = 2;
	ch->blksz = sc->bufsz / ch->blkcnt;

	switch(ch->num) {
	case 0: /* play */
		KASSERT(dir == PCMDIR_PLAY, ("wrong direction"));
		ch->regbase = ICH_REG_PO_BASE;
		ch->spdreg = sc->hasvra? AC97_REGEXT_FDACRATE : 0;
		ch->imask = ICH_GLOB_STA_POINT;
		break;

	case 1: /* record */
		KASSERT(dir == PCMDIR_REC, ("wrong direction"));
		ch->regbase = ICH_REG_PI_BASE;
		ch->spdreg = sc->hasvra? AC97_REGEXT_LADCRATE : 0;
		ch->imask = ICH_GLOB_STA_PIINT;
		break;

	case 2: /* mic */
		KASSERT(dir == PCMDIR_REC, ("wrong direction"));
		ch->regbase = ICH_REG_MC_BASE;
		ch->spdreg = sc->hasvrm? AC97_REGEXT_MADCRATE : 0;
		ch->imask = ICH_GLOB_STA_MINT;
		break;

	default:
		return NULL;
	}

	if (sndbuf_alloc(ch->buffer, sc->dmat, sc->bufsz))
		return NULL;

	ich_wr(sc, ch->regbase + ICH_REG_X_BDBAR, (u_int32_t)vtophys(ch->dtbl), 4);

	return ch;
}

static int
ichchan_setformat(kobj_t obj, void *data, u_int32_t format)
{
	return 0;
}

static int
ichchan_setspeed(kobj_t obj, void *data, u_int32_t speed)
{
	struct sc_chinfo *ch = data;
	struct sc_info *sc = ch->parent;

	if (ch->spdreg) {
		int r;
		if (sc->ac97rate <= 32000 || sc->ac97rate >= 64000)
			sc->ac97rate = 48000;
		r = (speed * 48000) / sc->ac97rate;
		/*
		 * Cast the return value of ac97_setrate() to u_int so that
		 * the math don't overflow into the negative range.
		 */
		ch->spd = ((u_int)ac97_setrate(sc->codec, ch->spdreg, r) *
		    sc->ac97rate) / 48000;
	} else {
		ch->spd = 48000;
	}
	return ch->spd;
}

static int
ichchan_setblocksize(kobj_t obj, void *data, u_int32_t blocksize)
{
	struct sc_chinfo *ch = data;
	struct sc_info *sc = ch->parent;

	ch->blksz = blocksize;
	ich_filldtbl(ch);
	ich_wr(sc, ch->regbase + ICH_REG_X_LVI, ch->blkcnt - 1, 1);

	return ch->blksz;
}

static int
ichchan_trigger(kobj_t obj, void *data, int go)
{
	struct sc_chinfo *ch = data;
	struct sc_info *sc = ch->parent;

	switch (go) {
	case PCMTRIG_START:
		ch->run = 1;
		ich_wr(sc, ch->regbase + ICH_REG_X_BDBAR, (u_int32_t)vtophys(ch->dtbl), 4);
		ich_wr(sc, ch->regbase + ICH_REG_X_CR, ICH_X_CR_RPBM | ICH_X_CR_LVBIE | ICH_X_CR_IOCE, 1);
		break;

	case PCMTRIG_ABORT:
		ich_resetchan(sc, ch->num);
		ch->run = 0;
		break;
	}
	return 0;
}

static int
ichchan_getptr(kobj_t obj, void *data)
{
	struct sc_chinfo *ch = data;
	struct sc_info *sc = ch->parent;
      	u_int32_t pos;

	ch->civ = ich_rd(sc, ch->regbase + ICH_REG_X_CIV, 1) % ch->blkcnt;

	pos = ch->civ * ch->blksz;

	return pos;
}

static struct pcmchan_caps *
ichchan_getcaps(kobj_t obj, void *data)
{
	struct sc_chinfo *ch = data;

	return ch->spdreg? &ich_vrcaps : &ich_caps;
}

static kobj_method_t ichchan_methods[] = {
	KOBJMETHOD(channel_init,		ichchan_init),
	KOBJMETHOD(channel_setformat,		ichchan_setformat),
	KOBJMETHOD(channel_setspeed,		ichchan_setspeed),
	KOBJMETHOD(channel_setblocksize,	ichchan_setblocksize),
	KOBJMETHOD(channel_trigger,		ichchan_trigger),
	KOBJMETHOD(channel_getptr,		ichchan_getptr),
	KOBJMETHOD(channel_getcaps,		ichchan_getcaps),
	{ 0, 0 }
};
CHANNEL_DECLARE(ichchan);

/* -------------------------------------------------------------------- */
/* The interrupt handler */

static void
ich_intr(void *p)
{
	struct sc_info *sc = (struct sc_info *)p;
	struct sc_chinfo *ch;
	u_int32_t cbi, lbi, lvi, st, gs;
	int i;

	gs = ich_rd(sc, ICH_REG_GLOB_STA, 4) & ICH_GLOB_STA_IMASK;
	if (gs & (ICH_GLOB_STA_PRES | ICH_GLOB_STA_SRES)) {
		/* Clear resume interrupt(s) - nothing doing with them */
		ich_wr(sc, ICH_REG_GLOB_STA, gs, 4);
	}
	gs &= ~(ICH_GLOB_STA_PRES | ICH_GLOB_STA_SRES);

	for (i = 0; i < 3; i++) {
		ch = &sc->ch[i];
		if ((ch->imask & gs) == 0) 
			continue;
		gs &= ~ch->imask;
		st = ich_rd(sc, ch->regbase + 
				(sc->swap_reg ? ICH_REG_X_PICB : ICH_REG_X_SR),
			    2);
		st &= ICH_X_SR_FIFOE | ICH_X_SR_BCIS | ICH_X_SR_LVBCI;
		if (st & (ICH_X_SR_BCIS | ICH_X_SR_LVBCI)) {
				/* block complete - update buffer */
			if (ch->run)
				chn_intr(ch->channel);
			lvi = ich_rd(sc, ch->regbase + ICH_REG_X_LVI, 1);
			cbi = ch->civ % ch->blkcnt;
			if (cbi == 0)
				cbi = ch->blkcnt - 1;
			else
				cbi--;
			lbi = lvi % ch->blkcnt;
			if (cbi >= lbi)
				lvi += cbi - lbi;
			else
				lvi += cbi + ch->blkcnt - lbi;
			lvi %= ICH_DTBL_LENGTH;
			ich_wr(sc, ch->regbase + ICH_REG_X_LVI, lvi, 1);

		}
		/* clear status bit */
		ich_wr(sc, ch->regbase + 
			   (sc->swap_reg ? ICH_REG_X_PICB : ICH_REG_X_SR),
		       st, 2);
	}
	if (gs != 0) {
		device_printf(sc->dev, 
			      "Unhandled interrupt, gs_intr = %x\n", gs);
	}
}

/* ------------------------------------------------------------------------- */
/* Sysctl to control ac97 speed (some boards overclocked ac97). */

static int
ich_initsys(struct sc_info* sc)
{
#ifdef SND_DYNSYSCTL
	SYSCTL_ADD_INT(snd_sysctl_tree(sc->dev), 
		       SYSCTL_CHILDREN(snd_sysctl_tree_top(sc->dev)),
		       OID_AUTO, "ac97rate", CTLFLAG_RW, 
		       &sc->ac97rate, 48000, 
		       "AC97 link rate (default = 48000)");
#endif /* SND_DYNSYSCTL */
	return 0;
}

/* -------------------------------------------------------------------- */
/* Calibrate card (some boards are overclocked and need scaling) */

static
void ich_calibrate(void *arg)
{
	struct sc_info *sc;
	struct sc_chinfo *ch;
	struct timeval t1, t2;
	u_int8_t ociv, nciv;
	u_int32_t wait_us, actual_48k_rate, bytes;

	sc = (struct sc_info *)arg;
	ch = &sc->ch[1];

	if (sc->use_intrhook)
		config_intrhook_disestablish(&sc->intrhook);

	/*
	 * Grab audio from input for fixed interval and compare how
	 * much we actually get with what we expect.  Interval needs
	 * to be sufficiently short that no interrupts are
	 * generated.
	 */

	KASSERT(ch->regbase == ICH_REG_PI_BASE, ("wrong direction"));

	bytes = sndbuf_getsize(ch->buffer) / 2;
	ichchan_setblocksize(0, ch, bytes);

	/*
	 * our data format is stereo, 16 bit so each sample is 4 bytes.
	 * assuming we get 48000 samples per second, we get 192000 bytes/sec.
	 * we're going to start recording with interrupts disabled and measure
	 * the time taken for one block to complete.  we know the block size,
	 * we know the time in microseconds, we calculate the sample rate:
	 *
	 * actual_rate [bps] = bytes / (time [s] * 4)
	 * actual_rate [bps] = (bytes * 1000000) / (time [us] * 4)
	 * actual_rate [Hz] = (bytes * 250000) / time [us]
	 */

	/* prepare */
	ociv = ich_rd(sc, ch->regbase + ICH_REG_X_CIV, 1);
	nciv = ociv;
	ich_wr(sc, ch->regbase + ICH_REG_X_BDBAR, (u_int32_t)vtophys(ch->dtbl), 4);

	/* start */
	microtime(&t1);
	ich_wr(sc, ch->regbase + ICH_REG_X_CR, ICH_X_CR_RPBM, 1);

	/* wait */
	while (nciv == ociv) {
		microtime(&t2);
		if (t2.tv_sec - t1.tv_sec > 1)
			break;
		nciv = ich_rd(sc, ch->regbase + ICH_REG_X_CIV, 1);
	}
	microtime(&t2);

	/* stop */
	ich_wr(sc, ch->regbase + ICH_REG_X_CR, 0, 1);

	/* reset */
	DELAY(100);
	ich_wr(sc, ch->regbase + ICH_REG_X_CR, ICH_X_CR_RR, 1);

	/* turn time delta into us */
	wait_us = ((t2.tv_sec - t1.tv_sec) * 1000000) + t2.tv_usec - t1.tv_usec;

	if (nciv == ociv) {
		device_printf(sc->dev, "ac97 link rate calibration timed out after %d us\n", wait_us);
		return;
	}

	actual_48k_rate = (bytes * 250000) / wait_us;

	if (actual_48k_rate < 47500 || actual_48k_rate > 48500) {
		sc->ac97rate = actual_48k_rate;
	} else {
		sc->ac97rate = 48000;
	}

	if (bootverbose || sc->ac97rate != 48000) {
		device_printf(sc->dev, "measured ac97 link rate at %d Hz", actual_48k_rate);
		if (sc->ac97rate != actual_48k_rate)
			printf(", will use %d Hz", sc->ac97rate);
	 	printf("\n");
	}
	return;
}

/* -------------------------------------------------------------------- */
/* Probe and attach the card */

static void
ich_setmap(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	return;
}

static int
ich_init(struct sc_info *sc)
{
	u_int32_t stat;
	int sz;

	ich_wr(sc, ICH_REG_GLOB_CNT, ICH_GLOB_CTL_COLD, 4);
	DELAY(600000);
	stat = ich_rd(sc, ICH_REG_GLOB_STA, 4);

	if ((stat & ICH_GLOB_STA_PCR) == 0) {
		/* ICH4 may fail when busmastering is enabled. Continue */
		if (pci_get_devid(sc->dev) != ICH4ID) {
			return ENXIO;
		}
	}

	ich_wr(sc, ICH_REG_GLOB_CNT, ICH_GLOB_CTL_COLD | ICH_GLOB_CTL_PRES, 4);

	if (ich_resetchan(sc, 0) || ich_resetchan(sc, 1))
		return ENXIO;
	if (sc->hasmic && ich_resetchan(sc, 2))
		return ENXIO;

	if (bus_dmamem_alloc(sc->dmat, (void **)&sc->dtbl, BUS_DMA_NOWAIT, &sc->dtmap))
		return ENOSPC;

	sz = sizeof(struct ich_desc) * ICH_DTBL_LENGTH * 3;
	if (bus_dmamap_load(sc->dmat, sc->dtmap, sc->dtbl, sz, ich_setmap, NULL, 0)) {
		bus_dmamem_free(sc->dmat, (void **)&sc->dtbl, sc->dtmap);
		return ENOSPC;
	}

	return 0;
}

static int
ich_pci_probe(device_t dev)
{
	switch(pci_get_devid(dev)) {
	case 0x71958086:
		device_set_desc(dev, "Intel 443MX");
		return 0;

	case 0x24158086:
		device_set_desc(dev, "Intel 82801AA (ICH)");
		return 0;

	case 0x24258086:
		device_set_desc(dev, "Intel 82801AB (ICH)");
		return 0;

	case 0x24458086:
		device_set_desc(dev, "Intel 82801BA (ICH2)");
		return 0;

	case 0x24858086:
		device_set_desc(dev, "Intel 82801CA (ICH3)");
		return 0;

	case ICH4ID:
		device_set_desc(dev, "Intel 82801DB (ICH4)");
		return 0;

	case SIS7012ID:
		device_set_desc(dev, "SiS 7012");
		return 0;

	case 0x01b110de:
		device_set_desc(dev, "Nvidia nForce AC97 controller");
		return 0;

	default:
		return ENXIO;
	}
}

static int
ich_pci_attach(device_t dev)
{
	u_int16_t		extcaps;
	struct sc_info 		*sc;
	char 			status[SND_STATUSLEN];

	if ((sc = malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT)) == NULL) {
		device_printf(dev, "cannot allocate softc\n");
		return ENXIO;
	}

	bzero(sc, sizeof(*sc));
	sc->dev = dev;

	/*
	 * The SiS 7012 register set isn't quite like the standard ich.
	 * There really should be a general "quirks" mechanism.
	 */
	if (pci_get_devid(dev) == SIS7012ID) {
		sc->swap_reg = 1;
		sc->sample_size = 1;
	} else {
		sc->swap_reg = 0;
		sc->sample_size = 2;
	}

	/*
	 * By default, ich4 has NAMBAR and NABMBAR i/o spaces as
	 * read-only.  Need to enable "legacy support", by poking into
	 * pci config space.  The driver should use MMBAR and MBBAR,
	 * but doing so will mess things up here.  ich4 has enough new
	 * features it warrants it's own driver. 
	 */
	if (pci_get_devid(dev) == ICH4ID) {
		pci_write_config(dev, PCIR_ICH_LEGACY, ICH_LEGACY_ENABLE, 1);
	}

	pci_enable_io(dev, SYS_RES_IOPORT);
	/*
	 * Enable bus master. On ich4 this may prevent the detection of
	 * the primary codec becoming ready in ich_init().
	 */
	pci_enable_busmaster(dev);

	sc->nambarid = PCIR_NAMBAR;
	sc->nabmbarid = PCIR_NABMBAR;
	sc->nambar = bus_alloc_resource(dev, SYS_RES_IOPORT, &sc->nambarid, 0, ~0, 1, RF_ACTIVE);
	sc->nabmbar = bus_alloc_resource(dev, SYS_RES_IOPORT, &sc->nabmbarid, 0, ~0, 1, RF_ACTIVE);

	if (!sc->nambar || !sc->nabmbar) {
		device_printf(dev, "unable to map IO port space\n");
		goto bad;
	}

	sc->nambart = rman_get_bustag(sc->nambar);
	sc->nambarh = rman_get_bushandle(sc->nambar);
	sc->nabmbart = rman_get_bustag(sc->nabmbar);
	sc->nabmbarh = rman_get_bushandle(sc->nabmbar);

	sc->bufsz = pcm_getbuffersize(dev, 4096, ICH_DEFAULT_BUFSZ, ICH_MAX_BUFSZ);
	if (bus_dma_tag_create(NULL, 8, 0, BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR,
			       NULL, NULL, sc->bufsz, 1, 0x3ffff, 0, &sc->dmat) != 0) {
		device_printf(dev, "unable to create dma tag\n");
		goto bad;
	}

	sc->irqid = 0;
	sc->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->irqid, 0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
	if (!sc->irq || snd_setup_intr(dev, sc->irq, INTR_MPSAFE, ich_intr, sc, &sc->ih)) {
		device_printf(dev, "unable to map interrupt\n");
		goto bad;
	}

	if (ich_init(sc)) {
		device_printf(dev, "unable to initialize the card\n");
		goto bad;
	}

	sc->codec = AC97_CREATE(dev, sc, ich_ac97);
	if (sc->codec == NULL)
		goto bad;
	mixer_init(dev, ac97_getmixerclass(), sc->codec);

	/* check and set VRA function */
	extcaps = ac97_getextcaps(sc->codec);
	sc->hasvra = extcaps & AC97_EXTCAP_VRA;
	sc->hasvrm = extcaps & AC97_EXTCAP_VRM;
	sc->hasmic = ac97_getcaps(sc->codec) & AC97_CAP_MICCHANNEL;
	ac97_setextmode(sc->codec, sc->hasvra | sc->hasvrm);

	if (pcm_register(dev, sc, 1, sc->hasmic? 2 : 1))
		goto bad;

	pcm_addchan(dev, PCMDIR_PLAY, &ichchan_class, sc);		/* play */
	pcm_addchan(dev, PCMDIR_REC, &ichchan_class, sc);		/* record */
	if (sc->hasmic)
		pcm_addchan(dev, PCMDIR_REC, &ichchan_class, sc);	/* record mic */

	snprintf(status, SND_STATUSLEN, "at io 0x%lx, 0x%lx irq %ld bufsz %u",
		 rman_get_start(sc->nambar), rman_get_start(sc->nabmbar), rman_get_start(sc->irq), sc->bufsz);

	pcm_setstatus(dev, status);

	ich_initsys(sc);

	sc->intrhook.ich_func = ich_calibrate;
	sc->intrhook.ich_arg = sc;
	sc->use_intrhook = 1;
	if (config_intrhook_establish(&sc->intrhook) != 0) {
		device_printf(dev, "Cannot establish calibration hook, will calibrate now\n");
		sc->use_intrhook = 0;
		ich_calibrate(sc);
	}

	return 0;

bad:
	if (sc->codec)
		ac97_destroy(sc->codec);
	if (sc->ih)
		bus_teardown_intr(dev, sc->irq, sc->ih);
	if (sc->irq)
		bus_release_resource(dev, SYS_RES_IRQ, sc->irqid, sc->irq);
	if (sc->nambar)
		bus_release_resource(dev, SYS_RES_IOPORT,
		    sc->nambarid, sc->nambar);
	if (sc->nabmbar)
		bus_release_resource(dev, SYS_RES_IOPORT,
		    sc->nabmbarid, sc->nabmbar);
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

	bus_teardown_intr(dev, sc->irq, sc->ih);
	bus_release_resource(dev, SYS_RES_IRQ, sc->irqid, sc->irq);
	bus_release_resource(dev, SYS_RES_IOPORT, sc->nambarid, sc->nambar);
	bus_release_resource(dev, SYS_RES_IOPORT, sc->nabmbarid, sc->nabmbar);
	bus_dma_tag_destroy(sc->dmat);
	free(sc, M_DEVBUF);
	return 0;
}

static int
ich_pci_suspend(device_t dev)
{
	struct sc_info *sc;
	int i;

	sc = pcm_getdevinfo(dev);	
	for (i = 0 ; i < 3; i++) {
		sc->ch[i].run_save = sc->ch[i].run;
		if (sc->ch[i].run) {
			ichchan_trigger(0, &sc->ch[i], PCMTRIG_ABORT);
		}
	}
	return 0;
}

static int
ich_pci_resume(device_t dev)
{
	struct sc_info *sc;
	int i;

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
	/* Re-start DMA engines */
	for (i = 0 ; i < 3; i++) {
		struct sc_chinfo *ch = &sc->ch[i];
		if (sc->ch[i].run_save) {
			ichchan_setblocksize(0, ch, ch->blksz);
			ichchan_setspeed(0, ch, ch->spd);
			ichchan_trigger(0, ch, PCMTRIG_START);
		}
	}
	return 0;
}

static device_method_t ich_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ich_pci_probe),
	DEVMETHOD(device_attach,	ich_pci_attach),
	DEVMETHOD(device_detach,	ich_pci_detach),
	DEVMETHOD(device_suspend, 	ich_pci_suspend),
	DEVMETHOD(device_resume,	ich_pci_resume),
	{ 0, 0 }
};

static driver_t ich_driver = {
	"pcm",
	ich_methods,
	PCM_SOFTC_SIZE,
};

DRIVER_MODULE(snd_ich, pci, ich_driver, pcm_devclass, 0, 0);
MODULE_DEPEND(snd_ich, snd_pcm, PCM_MINVER, PCM_PREFVER, PCM_MAXVER);
MODULE_VERSION(snd_ich, 1);
