/*
 * Copyright (c) 2002 Orion Hodson <orion@freebsd.org>
 * Portions of this code derived from via82c686.c:
 * 	Copyright (c) 2000 David Jones <dej@ox.org>
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
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Some Credits:
 *
 * Grzybowski Rafal, Russell Davies, Mark Handley, Daniel O'Connor for
 * comments, machine time, testing patches, and patience.  VIA for
 * providing specs.  ALSA for helpful comments and some register poke
 * ordering.  
 */

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/ac97.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>
#include <sys/sysctl.h>

#include <dev/sound/pci/via8233.h>

SND_DECLARE_FILE("$FreeBSD$");

#define VIA8233_PCI_ID 0x30591106

#define SEGS_PER_CHAN	2			/* Segments per channel */
#define NCHANS		2			/* Lines-in,out (mic later) */
#define	NSEGS		NCHANS * SEGS_PER_CHAN	/* Segments in SGD table */

#define	VIA_DEFAULT_BUFSZ	0x1000

#undef DEB
#define DEB(x) x

/* we rely on this struct being packed to 64 bits */
struct via_dma_op {
        u_int32_t ptr;
        u_int32_t flags;
#define VIA_DMAOP_EOL         0x80000000
#define VIA_DMAOP_FLAG        0x40000000
#define VIA_DMAOP_STOP        0x20000000
#define VIA_DMAOP_COUNT(x)    ((x)&0x00FFFFFF)
};

struct via_info;

struct via_chinfo {
	struct via_info *parent;
	struct pcm_channel *channel;
	struct snd_dbuf *buffer;
	struct via_dma_op *sgd_table;
	int dir, blksz;
	int rbase; 			/* base register for channel */
};

struct via_info {
	bus_space_tag_t st;
	bus_space_handle_t sh;
	bus_dma_tag_t parent_dmat;
	bus_dma_tag_t sgd_dmat;
	bus_dmamap_t sgd_dmamap;

	struct resource *reg, *irq;
	int regid, irqid;
	void *ih;
	struct ac97_info *codec;

	unsigned int bufsz;

	struct via_chinfo pch, rch;
	struct via_dma_op *sgd_table;
	u_int16_t codec_caps;
};

static u_int32_t via_fmt[] = {
	AFMT_U8,
	AFMT_STEREO | AFMT_U8,
	AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE,
	0
};

static struct pcmchan_caps via_vracaps = { 4000, 48000, via_fmt, 0 };
static struct pcmchan_caps via_caps = { 48000, 48000, via_fmt, 0 };

static u_int32_t
via_rd(struct via_info *via, int regno, int size)
{
	switch (size) {
	case 1:
		return bus_space_read_1(via->st, via->sh, regno);
	case 2:
		return bus_space_read_2(via->st, via->sh, regno);
	case 4:
		return bus_space_read_4(via->st, via->sh, regno);
	default:
		return 0xFFFFFFFF;
	}
}

static void
via_wr(struct via_info *via, int regno, u_int32_t data, int size)
{

	switch (size) {
	case 1:
		bus_space_write_1(via->st, via->sh, regno, data);
		break;
	case 2:
		bus_space_write_2(via->st, via->sh, regno, data);
		break;
	case 4:
		bus_space_write_4(via->st, via->sh, regno, data);
		break;
	}
}

/* -------------------------------------------------------------------- */
/* Codec interface */

static int
via_waitready_codec(struct via_info *via)
{
	int i;

	/* poll until codec not busy */
	for (i = 0; i < 1000; i++) {
		if ((via_rd(via, VIA_AC97_CONTROL, 4) & VIA_AC97_BUSY) == 0)
			return 0;
		DELAY(1);
	}
	printf("via: codec busy\n");
	return 1;
}

static int
via_waitvalid_codec(struct via_info *via)
{
	int i;

	/* poll until codec valid */
	for (i = 0; i < 1000; i++) {
		if (via_rd(via, VIA_AC97_CONTROL, 4) & VIA_AC97_CODEC00_VALID)
			return 0;
		DELAY(1);
	}
	printf("via: codec invalid\n");
	return 1;
}

static int
via_write_codec(kobj_t obj, void *addr, int reg, u_int32_t val)
{
	struct via_info *via = addr;

	if (via_waitready_codec(via)) return -1;

	via_wr(via, VIA_AC97_CONTROL, 
	       VIA_AC97_CODEC00_VALID | VIA_AC97_INDEX(reg) |
	       VIA_AC97_DATA(val), 4);

	return 0;
}

static int
via_read_codec(kobj_t obj, void *addr, int reg)
{
	struct via_info *via = addr;

	if (via_waitready_codec(via))
		return -1;

	via_wr(via, VIA_AC97_CONTROL, VIA_AC97_CODEC00_VALID | 
	       VIA_AC97_READ | VIA_AC97_INDEX(reg), 4);

	if (via_waitready_codec(via))
		return -1;

	if (via_waitvalid_codec(via))
		return -1;

	return via_rd(via, VIA_AC97_CONTROL, 2);
}

static kobj_method_t via_ac97_methods[] = {
    	KOBJMETHOD(ac97_read,		via_read_codec),
    	KOBJMETHOD(ac97_write,		via_write_codec),
	{ 0, 0 }
};
AC97_DECLARE(via_ac97);

/* -------------------------------------------------------------------- */

static int
via_buildsgdt(struct via_chinfo *ch)
{
	u_int32_t phys_addr, flag;
	int i, segs, seg_size;

	/*
	 *  Build the scatter/gather DMA (SGD) table.
	 *  There are four slots in the table: two for play, two for record.
	 *  This creates two half-buffers, one of which is playing; the other
	 *  is feeding.
	 */
	seg_size = ch->blksz;
	segs = sndbuf_getsize(ch->buffer) / seg_size;
	phys_addr = vtophys(sndbuf_getbuf(ch->buffer));

	for (i = 0; i < segs; i++) {
		flag = (i == segs - 1) ? VIA_DMAOP_EOL : VIA_DMAOP_FLAG;
		ch->sgd_table[i].ptr = phys_addr + (i * seg_size);
		ch->sgd_table[i].flags = flag | seg_size;
	}

	return 0;
}

static int
via8233pchan_setformat(kobj_t obj, void *data, u_int32_t format)
{
	struct via_chinfo *ch = data;
	struct via_info *via = ch->parent;

	u_int32_t s = 0xff000000;
	u_int8_t  v = (format & AFMT_S16_LE) ? MC_SGD_16BIT : MC_SGD_8BIT;

	if (format & AFMT_STEREO) {
		v |= MC_SGD_CHANNELS(2);
		s |= SLOT3(1) | SLOT4(2);
	} else {
		v |= MC_SGD_CHANNELS(1);
		s |= SLOT3(1) | SLOT4(1);
	}

	via_wr(via, VIA_MC_SLOT_SELECT, s, 4);
	via_wr(via, VIA_MC_SGD_FORMAT, v, 1);

	return 0;
}

static int
via8233rchan_setformat(kobj_t obj, void *data, u_int32_t format)
{
	struct via_chinfo *ch = data;
	struct via_info *via = ch->parent;
	
	u_int32_t f = WR_FORMAT_STOP_INDEX;
	if (format & AFMT_STEREO)
		f |= WR_FORMAT_STEREO;
	if (format & AFMT_S16_LE)
		f |= WR_FORMAT_16BIT;

	via_wr(via, VIA_WR0_FORMAT, f, 4);
	return 0;
}

static int
via8233pchan_setspeed(kobj_t obj, void *data, u_int32_t speed)
{
	struct via_chinfo *ch = data;
	struct via_info *via = ch->parent;

	if (via->codec_caps & AC97_EXTCAP_VRA)
		return ac97_setrate(via->codec, AC97_REGEXT_FDACRATE, speed); 

	return 48000;
}

static int
via8233rchan_setspeed(kobj_t obj, void *data, u_int32_t speed)
{
	struct via_chinfo *ch = data;
	struct via_info *via = ch->parent;

	if (via->codec_caps & AC97_EXTCAP_VRA)
		return ac97_setrate(via->codec, AC97_REGEXT_LADCRATE, speed);

	return 48000;
}

static int
via8233chan_setblocksize(kobj_t obj, void *data, u_int32_t blocksize)
{
	struct via_chinfo *ch = data;

	ch->blksz = blocksize;
	sndbuf_resize(ch->buffer, SEGS_PER_CHAN, ch->blksz);
	return ch->blksz;
}

static int
via8233chan_getptr(kobj_t obj, void *data)
{
	struct via_chinfo *ch = data;
	struct via_info *via = ch->parent;

	u_int32_t v = via_rd(via, ch->rbase + VIA_RP_CURRENT_COUNT, 4);
	u_int32_t index = v >> 24;		/* Last completed buffer */
	u_int32_t count = v & 0x00ffffff;	/* Bytes remaining */
	int ptr = (index + 1) * ch->blksz - count;
	ptr %= SEGS_PER_CHAN * ch->blksz;	/* Wrap to available space */
	return ptr;
}

static struct pcmchan_caps *
via8233chan_getcaps(kobj_t obj, void *data)
{
	struct via_chinfo *ch = data;
	struct via_info *via = ch->parent;
	if (via->codec_caps & AC97_EXTCAP_VRA) 
		return &via_vracaps;
	return &via_caps;
}

static void
via8233chan_reset(struct via_info *via, struct via_chinfo *ch)
{
	via_wr(via, ch->rbase + VIA_RP_CONTROL, SGD_CONTROL_STOP, 1);
	via_wr(via, ch->rbase + VIA_RP_CONTROL, 0x00, 1);
	via_wr(via, ch->rbase + VIA_RP_STATUS, 
	       SGD_STATUS_EOL | SGD_STATUS_FLAG, 1);
}

static void*
via8233chan_init(kobj_t obj, void *devinfo, struct snd_dbuf *b,
		 struct pcm_channel *c, int dir)
{
	struct via_info *via = devinfo;
	struct via_chinfo *ch = (dir == PCMDIR_PLAY)? &via->pch : &via->rch;

	ch->parent = via;
	ch->channel = c;
	ch->buffer = b;
	ch->dir = dir;
	ch->sgd_table = &via->sgd_table[(dir == PCMDIR_PLAY)? 0 : SEGS_PER_CHAN];

	if (ch->dir == PCMDIR_PLAY) {
		ch->rbase = VIA_MC_SGD_STATUS;
	} else {
		ch->rbase = VIA_WR0_SGD_STATUS;
		via_wr(via, VIA_WR0_SGD_FORMAT, WR_FIFO_ENABLE, 1);
	}

	if (sndbuf_alloc(ch->buffer, via->parent_dmat, via->bufsz) == -1)
		return NULL;

	via8233chan_reset(via, ch);

	return ch;
}

static int
via8233chan_trigger(kobj_t obj, void* data, int go)
{
	struct via_chinfo *ch = data;
	struct via_info *via = ch->parent;
	struct via_dma_op *ado = ch->sgd_table;

	switch(go) {
	case PCMTRIG_START:
		via_buildsgdt(ch);
		via_wr(via, ch->rbase + VIA_RP_TABLE_PTR, vtophys(ado), 4);
		via_wr(via, ch->rbase + VIA_RP_CONTROL,
		       SGD_CONTROL_START | SGD_CONTROL_AUTOSTART |
		       SGD_CONTROL_I_EOL | SGD_CONTROL_I_FLAG, 1);
		break;
	case PCMTRIG_STOP:
	case PCMTRIG_ABORT:
		via_wr(via, ch->rbase + VIA_RP_CONTROL, SGD_CONTROL_STOP, 1);
		via8233chan_reset(via, ch);
		break;
	}
	return 0;
}

static kobj_method_t via8233pchan_methods[] = {
    	KOBJMETHOD(channel_init,		via8233chan_init),
    	KOBJMETHOD(channel_setformat,		via8233pchan_setformat),
    	KOBJMETHOD(channel_setspeed,		via8233pchan_setspeed),
    	KOBJMETHOD(channel_setblocksize,	via8233chan_setblocksize),
    	KOBJMETHOD(channel_trigger,		via8233chan_trigger),
    	KOBJMETHOD(channel_getptr,		via8233chan_getptr),
    	KOBJMETHOD(channel_getcaps,		via8233chan_getcaps),
	{ 0, 0 }
};
CHANNEL_DECLARE(via8233pchan);

static kobj_method_t via8233rchan_methods[] = {
    	KOBJMETHOD(channel_init,		via8233chan_init),
    	KOBJMETHOD(channel_setformat,		via8233rchan_setformat),
    	KOBJMETHOD(channel_setspeed,		via8233rchan_setspeed),
    	KOBJMETHOD(channel_setblocksize,	via8233chan_setblocksize),
    	KOBJMETHOD(channel_trigger,		via8233chan_trigger),
    	KOBJMETHOD(channel_getptr,		via8233chan_getptr),
    	KOBJMETHOD(channel_getcaps,		via8233chan_getcaps),
	{ 0, 0 }
};
CHANNEL_DECLARE(via8233rchan);

/* -------------------------------------------------------------------- */

static void
via_intr(void *p)
{
	struct via_info *via = p;
	int r = via_rd(via, VIA_MC_SGD_STATUS, 1);
	if (r & SGD_STATUS_INTR) {
		via_wr(via, VIA_MC_SGD_STATUS, SGD_STATUS_INTR, 1);
		chn_intr(via->pch.channel);
	}

	r = via_rd(via, VIA_WR0_SGD_STATUS, 1);
	if (r & SGD_STATUS_INTR) {
		via_wr(via, VIA_WR0_SGD_STATUS, SGD_STATUS_INTR, 1);
		chn_intr(via->rch.channel);
	}
}

/*
 *  Probe and attach the card
 */
static int
via_probe(device_t dev)
{
	switch(pci_get_devid(dev)) {
	case VIA8233_PCI_ID:
		switch(pci_get_revid(dev)) {
		case 0x10: 
			device_set_desc(dev, "VIA VT8233 (pre)");
			return 0;
		case 0x20:
			device_set_desc(dev, "VIA VT8233C");
			return 0;
		case 0x30:
			device_set_desc(dev, "VIA VT8233");
			return 0;
		case 0x40:
			device_set_desc(dev, "VIA VT8233A");
			return 0;
		case 0x50:
			device_set_desc(dev, "VIA VT8235");
			return 0;
		default:
			device_set_desc(dev, "VIA VT8233X");	/* Unknown */
			return 0;
		}			
	}
	return ENXIO;
}

static void
dma_cb(void *p, bus_dma_segment_t *bds, int a, int b)
{
}

static int
via_chip_init(device_t dev)
{
	int i, s;

	pci_write_config(dev, VIA_PCI_ACLINK_CTRL, 0, 1);	
	DELAY(100);

	/* assert ACLink reset */
	pci_write_config(dev, VIA_PCI_ACLINK_CTRL, VIA_PCI_ACLINK_NRST, 1);
	DELAY(2);

	/* deassert ACLink reset, force SYNC (warm AC'97 reset) */
	pci_write_config(dev, VIA_PCI_ACLINK_CTRL,
			 VIA_PCI_ACLINK_NRST | VIA_PCI_ACLINK_SYNC, 1);

	/* ACLink on, deassert ACLink reset, VSR, SGD data out */
	pci_write_config(dev, VIA_PCI_ACLINK_CTRL,
			 VIA_PCI_ACLINK_EN | VIA_PCI_ACLINK_NRST
			 | VIA_PCI_ACLINK_VRATE | VIA_PCI_ACLINK_SGD, 1);

	for (i = 0; i < 100; i++) {
		s = pci_read_config(dev, VIA_PCI_ACLINK_STAT, 1);
		if (s & VIA_PCI_ACLINK_C00_READY) {
			s = pci_read_config(dev, VIA_PCI_ACLINK_CTRL, 1);
			return 0;
		}
		DELAY(10);
	}
	device_printf(dev, "primary codec not ready (s = 0x%02x)\n", s);
	return ENXIO;
}

static int
via_attach(device_t dev)
{
	struct via_info *via = 0;
	char status[SND_STATUSLEN];

	if ((via = malloc(sizeof *via, M_DEVBUF, M_NOWAIT | M_ZERO)) == NULL) {
		device_printf(dev, "cannot allocate softc\n");
		return ENXIO;
	}

	pci_enable_io(dev, SYS_RES_IOPORT);
	pci_set_powerstate(dev, PCI_POWERSTATE_D0);
	pci_enable_busmaster(dev);
	
	via->regid = PCIR_MAPS;
	via->reg = bus_alloc_resource(dev, SYS_RES_IOPORT, &via->regid, 0, ~0,
				      1, RF_ACTIVE);
	if (!via->reg) {
		device_printf(dev, "cannot allocate bus resource.");
		goto bad;
	}
	via->st = rman_get_bustag(via->reg);
	via->sh = rman_get_bushandle(via->reg);

	via->bufsz = pcm_getbuffersize(dev, 4096, VIA_DEFAULT_BUFSZ, 65536);

	via->irqid = 0;
	via->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &via->irqid, 0, ~0, 1,
				      RF_ACTIVE | RF_SHAREABLE);
	if (!via->irq || 
	    snd_setup_intr(dev, via->irq, 0, via_intr, via, &via->ih)) {
		device_printf(dev, "unable to map interrupt\n");
		goto bad;
	}

	/* DMA tag for buffers */
	if (bus_dma_tag_create(/*parent*/NULL, /*alignment*/2, /*boundary*/0,
		/*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
		/*highaddr*/BUS_SPACE_MAXADDR,
		/*filter*/NULL, /*filterarg*/NULL,
		/*maxsize*/via->bufsz, /*nsegments*/1, /*maxsegz*/0x3ffff,
		/*flags*/0, &via->parent_dmat) != 0) {
		device_printf(dev, "unable to create dma tag\n");
		goto bad;
	}

	/*
	 *  DMA tag for SGD table.  The 686 uses scatter/gather DMA and
	 *  requires a list in memory of work to do.  We need only 16 bytes
	 *  for this list, and it is wasteful to allocate 16K.
	 */
	if (bus_dma_tag_create(/*parent*/NULL, /*alignment*/2, /*boundary*/0,
		/*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
		/*highaddr*/BUS_SPACE_MAXADDR,
		/*filter*/NULL, /*filterarg*/NULL,
		/*maxsize*/NSEGS * sizeof(struct via_dma_op),
		/*nsegments*/1, /*maxsegz*/0x3ffff,
		/*flags*/0, &via->sgd_dmat) != 0) {
		device_printf(dev, "unable to create dma tag\n");
		goto bad;
	}

	if (bus_dmamem_alloc(via->sgd_dmat, (void **)&via->sgd_table, 
			     BUS_DMA_NOWAIT, &via->sgd_dmamap) == -1)
		goto bad;
	if (bus_dmamap_load(via->sgd_dmat, via->sgd_dmamap, via->sgd_table, 
			    NSEGS * sizeof(struct via_dma_op), dma_cb, 0, 0))
		goto bad;

	if (via_chip_init(dev))
		goto bad;

	via->codec = AC97_CREATE(dev, via, via_ac97);
	if (!via->codec)
		goto bad;

	mixer_init(dev, ac97_getmixerclass(), via->codec);

	via->codec_caps = ac97_getextcaps(via->codec);

	/* Try to set VRA without generating an error, VRM not reqrd yet */
	if (via->codec_caps & (AC97_EXTCAP_VRA | AC97_EXTCAP_VRM)) {
		u_int16_t ext = ac97_getextmode(via->codec);
		ext |= (via->codec_caps & (AC97_EXTCAP_VRA | AC97_EXTCAP_VRM));
		ac97_setextmode(via->codec, ext);
	}

	snprintf(status, SND_STATUSLEN, "at io 0x%lx irq %ld", 
		 rman_get_start(via->reg), rman_get_start(via->irq));

	/* Register */
	if (pcm_register(dev, via, 1, 1)) goto bad;

	pcm_addchan(dev, PCMDIR_PLAY, &via8233pchan_class, via);
	pcm_addchan(dev, PCMDIR_REC, &via8233rchan_class, via);

	pcm_setstatus(dev, status);

	return 0;
bad:
	if (via->codec) ac97_destroy(via->codec);
	if (via->reg) bus_release_resource(dev, SYS_RES_IOPORT, via->regid, via->reg);
	if (via->ih) bus_teardown_intr(dev, via->irq, via->ih);
	if (via->irq) bus_release_resource(dev, SYS_RES_IRQ, via->irqid, via->irq);
	if (via->parent_dmat) bus_dma_tag_destroy(via->parent_dmat);
	if (via->sgd_dmamap) bus_dmamap_unload(via->sgd_dmat, via->sgd_dmamap);
	if (via->sgd_dmat) bus_dma_tag_destroy(via->sgd_dmat);
	if (via) free(via, M_DEVBUF);
	return ENXIO;
}

static int
via_detach(device_t dev)
{
	int r;
	struct via_info *via = 0;

	r = pcm_unregister(dev);
	if (r) return r;

	via = pcm_getdevinfo(dev);
	bus_release_resource(dev, SYS_RES_IOPORT, via->regid, via->reg);
	bus_teardown_intr(dev, via->irq, via->ih);
	bus_release_resource(dev, SYS_RES_IRQ, via->irqid, via->irq);
	bus_dma_tag_destroy(via->parent_dmat);
	bus_dmamap_unload(via->sgd_dmat, via->sgd_dmamap);
	bus_dma_tag_destroy(via->sgd_dmat);
	free(via, M_DEVBUF);
	return 0;
}


static device_method_t via_methods[] = {
	DEVMETHOD(device_probe,		via_probe),
	DEVMETHOD(device_attach,	via_attach),
	DEVMETHOD(device_detach,	via_detach),
	{ 0, 0}
};

static driver_t via_driver = {
	"pcm",
	via_methods,
	PCM_SOFTC_SIZE,
};

DRIVER_MODULE(snd_via8233, pci, via_driver, pcm_devclass, 0, 0);
MODULE_DEPEND(snd_via8233, snd_pcm, PCM_MINVER, PCM_PREFVER, PCM_MAXVER);
MODULE_VERSION(snd_via8233, 1);
