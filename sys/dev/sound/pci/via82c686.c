/*
 * Copyright (c) 2000 David Jones <dej@ox.org>
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
 *
 * $FreeBSD$
 */

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/ac97.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>
#include <sys/sysctl.h>

#include <dev/sound/pci/via82c686.h>

#define VIA_PCI_ID 0x30581106
#define	NSEGS		16	/* Number of segments in SGD table */

#define SEGS_PER_CHAN	(NSEGS/2)

#undef DEB
#define DEB(x)

struct via_info;

struct via_chinfo {
	struct via_info *parent;
	pcm_channel *channel;
	snd_dbuf *buffer;
	int dir;
};

struct via_info {
	bus_space_tag_t st;
	bus_space_handle_t sh;
	bus_dma_tag_t	parent_dmat;
	bus_dma_tag_t	sgd_dmat;

	struct via_chinfo pch, rch;
	struct via_dma_op *sgd_table;
	u_int16_t	codec_caps;
};

static u_int32_t via_rd(struct via_info *via, int regno, int size);
static void via_wr(struct via_info *, int regno, u_int32_t data, int size);

int via_waitready_codec(struct via_info *via);
int via_waitvalid_codec(struct via_info *via);
u_int32_t via_read_codec(void *addr, int reg);
void via_write_codec(void *addr, int reg, u_int32_t val);

static void via_intr(void *);
bus_dmamap_callback_t dma_cb;


/* channel interface */
static void *viachan_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir);
static int viachan_setdir(void *data, int dir);
static int viachan_setformat(void *data, u_int32_t format);
static int viachan_setspeed(void *data, u_int32_t speed);
static int viachan_setblocksize(void *data, u_int32_t blocksize);
static int viachan_trigger(void *data, int go);
static int viachan_getptr(void *data);
static pcmchan_caps *viachan_getcaps(void *data);

static u_int32_t via_playfmt[] = {
	AFMT_U8,
	AFMT_STEREO | AFMT_U8,
	AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE,
	0
};
static pcmchan_caps via_playcaps = {4000, 48000, via_playfmt, 0};

static u_int32_t via_recfmt[] = {
	AFMT_U8,
	AFMT_STEREO | AFMT_U8,
	AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE,
	0
};
static pcmchan_caps via_reccaps = {4000, 48000, via_recfmt, 0};

static pcm_channel via_chantemplate = {
	viachan_init,
	viachan_setdir,
	viachan_setformat,
	viachan_setspeed,
	viachan_setblocksize,
	viachan_trigger,
	viachan_getptr,
	viachan_getcaps,
};


/*
 *  Probe and attach the card
 */
static int
via_probe(device_t dev)
{
	if (pci_get_devid(dev) == VIA_PCI_ID) {
	    device_set_desc(dev, "VIA VT82C686A AC'97 Audio");
	    return 0;
	}
	return ENXIO;
}


void dma_cb(void *p, bus_dma_segment_t *bds, int a, int b)
{
}


static int
via_attach(device_t dev)
{
	snddev_info	*d;
	struct via_info *via = 0;
	struct ac97_info *codec;
	char		status[SND_STATUSLEN];

	u_int32_t	data;
	struct resource *reg = 0;
	int		regid;
	struct resource *irq = 0;
	void		*ih = 0;
	int		irqid;

	u_int16_t	v;
	bus_dmamap_t	sgd_dma_map;

	d = device_get_softc(dev);
	if ((via = malloc(sizeof *via, M_DEVBUF, M_NOWAIT)) == NULL) {
		device_printf(dev, "cannot allocate softc\n");
		return ENXIO;
	}
	bzero(via, sizeof *via);

	/* Get resources */
	data = pci_read_config(dev, PCIR_COMMAND, 2);
	data |= (PCIM_CMD_PORTEN | PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCIR_COMMAND, data, 2);
	data = pci_read_config(dev, PCIR_COMMAND, 2);

	pci_write_config(dev, VIA_PCICONF_MISC,
		VIA_PCICONF_ACLINKENAB | VIA_PCICONF_ACSGD |
		VIA_PCICONF_ACNOTRST | VIA_PCICONF_ACVSR, 1);

	regid = PCIR_MAPS;
	reg = bus_alloc_resource(dev, SYS_RES_IOPORT, &regid,
		0, ~0, 1, RF_ACTIVE);
	if (!reg) {
		device_printf(dev, "via: Cannot allocate bus resource.");
		goto bad;
	}
	via->st = rman_get_bustag(reg);
	via->sh = rman_get_bushandle(reg);

	irqid = 0;
	irq = bus_alloc_resource(dev, SYS_RES_IRQ, &irqid,
		0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
	if (!irq
	    || bus_setup_intr(dev, irq, INTR_TYPE_TTY, via_intr, via, &ih)){
		device_printf(dev, "unable to map interrupt\n");
		goto bad;
	}

	via_wr(via, VIA_PLAY_MODE,
		VIA_RPMODE_AUTOSTART |
		VIA_RPMODE_INTR_FLAG | VIA_RPMODE_INTR_EOL, 1);
	via_wr(via, VIA_RECORD_MODE,
		VIA_RPMODE_AUTOSTART |
		VIA_RPMODE_INTR_FLAG | VIA_RPMODE_INTR_EOL, 1);

	codec = ac97_create(dev, via, NULL,
		via_read_codec, via_write_codec);
	if (!codec) goto bad;

	mixer_init(d, &ac97_mixer, codec);

	/*
	 *  The mixer init resets the codec.  So enabling VRA must be done
	 *  afterwards.
	 */
	v = via_read_codec(via, AC97_REG_EXT_AUDIO_ID);
	v &= (AC97_ENAB_VRA | AC97_ENAB_MICVRA);
	via_write_codec(via, AC97_REG_EXT_AUDIO_STAT, v);
	via->codec_caps = v;
	{
	v = via_read_codec(via, AC97_REG_EXT_AUDIO_STAT);
	DEB(printf("init: codec stat: %d\n", v));
	}

	if (!(v & AC97_CODEC_DOES_VRA)) {
		/* no VRA => can do only 48 kbps */
		via_playcaps.minspeed = 48000;
		via_reccaps.minspeed = 48000;
	}

	/* DMA tag for buffers */
	if (bus_dma_tag_create(/*parent*/NULL, /*alignment*/2, /*boundary*/0,
		/*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
		/*highaddr*/BUS_SPACE_MAXADDR,
		/*filter*/NULL, /*filterarg*/NULL,
		/*maxsize*/VIA_BUFFSIZE, /*nsegments*/1, /*maxsegz*/0x3ffff,
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
		BUS_DMA_NOWAIT, &sgd_dma_map) == -1) goto bad;
	if (bus_dmamap_load(via->sgd_dmat, sgd_dma_map, via->sgd_table,
		NSEGS * sizeof(struct via_dma_op), dma_cb, 0, 0)) goto bad;

	snprintf(status, SND_STATUSLEN, "at io 0x%lx irq %ld",
		rman_get_start(reg), rman_get_start(irq));

	/* Register */
	if (pcm_register(dev, via, 1, 1)) goto bad;
	pcm_addchan(dev, PCMDIR_PLAY, &via_chantemplate, via);
	pcm_addchan(dev, PCMDIR_REC, &via_chantemplate, via);
	pcm_setstatus(dev, status);
	return 0;
bad:
	if (via) free(via, M_DEVBUF);
	bus_release_resource(dev, SYS_RES_IOPORT, regid, reg);
	if (ih) bus_teardown_intr(dev, irq, ih);
	if (irq) bus_release_resource(dev, SYS_RES_IRQ, irqid, irq);
	return ENXIO;
}


static device_method_t via_methods[] = {
	DEVMETHOD(device_probe,		via_probe),
	DEVMETHOD(device_attach,	via_attach),
	{ 0, 0}
};

static driver_t via_driver = {
	"pcm",
	via_methods,
	sizeof(snddev_info),
};

static devclass_t pcm_devclass;

DRIVER_MODULE(via, pci, via_driver, pcm_devclass, 0, 0);
MODULE_DEPEND(via, snd_pcm, PCM_MINVER, PCM_PREFVER, PCM_MAXVER);
MODULE_VERSION(via, 1);


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


/* Codec interface */
int
via_waitready_codec(struct via_info *via)
{
	int i;

	/* poll until codec not busy */
	for (i = 0; (i < TIMEOUT) &&
	    (via_rd(via, VIA_CODEC_CTL, 4) & VIA_CODEC_BUSY); i++)
		DELAY(1);
	if (i >= TIMEOUT) {
		printf("via: codec busy\n");
		return 1;
	}

	return 0;
}


int
via_waitvalid_codec(struct via_info *via)
{
	int i;

	/* poll until codec valid */
	for (i = 0; (i < TIMEOUT) &&
	    !(via_rd(via, VIA_CODEC_CTL, 4) & VIA_CODEC_PRIVALID); i++)
		    DELAY(1);
	if (i >= TIMEOUT) {
		printf("via: codec invalid\n");
		return 1;
	}

	return 0;
}


void
via_write_codec(void *addr, int reg, u_int32_t val)
{
	struct via_info *via = addr;

	if (via_waitready_codec(via)) return;

	via_wr(via, VIA_CODEC_CTL,
		VIA_CODEC_PRIVALID | VIA_CODEC_INDEX(reg) | val, 4);
}


u_int32_t
via_read_codec(void *addr, int reg)
{
	struct via_info *via = addr;

	if (via_waitready_codec(via))
		return 1;

	via_wr(via, VIA_CODEC_CTL,
	    VIA_CODEC_PRIVALID | VIA_CODEC_READ | VIA_CODEC_INDEX(reg),4);

	if (via_waitready_codec(via))
		return 1;

	if (via_waitvalid_codec(via))
		return 1;

	return via_rd(via, VIA_CODEC_CTL, 2);
}


/* channel interface */
static void *
viachan_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir)
{
	struct via_info *via = devinfo;
	struct via_chinfo *ch = (dir == PCMDIR_PLAY) ? &via->pch : &via->rch;

	ch->parent = via;
	ch->channel = c;
	ch->buffer = b;
	b->bufsize = VIA_BUFFSIZE;

	if (chn_allocbuf(ch->buffer, via->parent_dmat) == -1) return NULL;
	return ch;
}

static int
viachan_setdir(void *data, int dir)
{
	struct via_chinfo *ch = data;
	struct via_info *via = ch->parent;
	struct via_dma_op *ado;
	int i, chunk_size;
	int	phys_addr, flag;

	ch->dir = dir;
	/*
	 *  Build the scatter/gather DMA (SGD) table.
	 *  There are four slots in the table: two for play, two for record.
	 *  This creates two half-buffers, one of which is playing; the other
	 *  is feeding.
	 */
	ado = via->sgd_table;
	chunk_size = ch->buffer->bufsize / SEGS_PER_CHAN;

	if (dir == PCMDIR_REC) {
		ado += SEGS_PER_CHAN;
	}

DEB(printf("SGD table located at va %p\n", ado));
	phys_addr = vtophys(ch->buffer->buf);
	for (i = 0; i < SEGS_PER_CHAN; i++) {
		ado->ptr = phys_addr;
		flag = (i == SEGS_PER_CHAN-1) ?
			VIA_DMAOP_EOL : VIA_DMAOP_FLAG;
		ado->flags = flag | chunk_size;
DEB(printf("ado->ptr/flags = %x/%x\n", phys_addr, flag));
		phys_addr += chunk_size;
		ado++;
	}
	return 0;
}

static int
viachan_setformat(void *data, u_int32_t format)
{
	struct via_chinfo *ch = data;
	struct via_info *via = ch->parent;
	int	mode, mode_set;

	mode_set = 0;
	if (format & AFMT_STEREO)
		mode_set |= VIA_RPMODE_STEREO;
	if (format & AFMT_S16_LE)
		mode_set |= VIA_RPMODE_16BIT;

	/* Set up for output format */
	if (ch->dir == PCMDIR_PLAY) {
DEB(printf("set play format: %x\n", format));
		mode = via_rd(via, VIA_PLAY_MODE, 1);
		mode &= ~(VIA_RPMODE_16BIT | VIA_RPMODE_STEREO);
		mode |= mode_set;
		via_wr(via, VIA_PLAY_MODE, mode, 1);
	}
	else {
DEB(printf("set record format: %x\n", format));
		mode = via_rd(via, VIA_RECORD_MODE, 1);
		mode &= ~(VIA_RPMODE_16BIT | VIA_RPMODE_STEREO);
		mode |= mode_set;
		via_wr(via, VIA_RECORD_MODE, mode, 1);
	}

	return 0;
}

static int
viachan_setspeed(void *data, u_int32_t speed)
{
	struct via_chinfo *ch = data;
	struct via_info *via = ch->parent;

	/*
	 *  Basic AC'97 defines a 48 kHz sample rate only.  For other rates,
	 *  upsampling is required.
	 *
	 *  The VT82C686A does not perform upsampling, and neither do we.
	 *  If the codec supports variable-rate audio (i.e. does the upsampling
	 *  itself), then negotiate the rate with the codec.  Otherwise,
	 *  return 48 kHz cuz that's all you got.
	 */
	if (ch->dir == PCMDIR_PLAY) {
DEB(printf("requested play speed: %d\n", speed));
		if (via->codec_caps & AC97_CODEC_DOES_VRA) {
			via_write_codec(via, AC97_REG_EXT_DAC_RATE, speed);
			speed = via_read_codec(via, AC97_REG_EXT_DAC_RATE);
		}
		else {
DEB(printf("VRA not supported!\n"));
			speed = 48000;
		}
DEB(printf("obtained play speed: %d\n", speed));
	}
	else {
DEB(printf("requested record speed: %d\n", speed));
		if (via->codec_caps & AC97_CODEC_DOES_VRA) {
			via_write_codec(via, AC97_REG_EXT_ADC_RATE, speed);
			speed = via_read_codec(via, AC97_REG_EXT_ADC_RATE);
		}
		else {
DEB(printf("VRA not supported!\n"));
			speed = 48000;
		}
DEB(printf("obtained record speed: %d\n", speed));
	}
	return speed;
}

static int
viachan_setblocksize(void *data, u_int32_t blocksize)
{
	struct via_chinfo *ch = data;

	return ch->buffer->bufsize / 2;
}

static int
viachan_trigger(void *data, int go)
{
	struct via_chinfo *ch = data;
	struct via_info *via = ch->parent;
	struct via_dma_op *ado;

	if (go == PCMTRIG_EMLDMAWR || go == PCMTRIG_EMLDMARD) return 0;
	if (ch->dir == PCMDIR_PLAY) {
		if (go == PCMTRIG_START) {
			ado = &via->sgd_table[0];
DEB(printf("ado located at va=%p pa=%x\n", ado, vtophys(ado)));
			via_wr(via, VIA_PLAY_DMAOPS_BASE, vtophys(ado),4);
			via_wr(via, VIA_PLAY_CONTROL,
				VIA_RPCTRL_START, 1);
		}
		else {
			/* Stop DMA */
			via_wr(via, VIA_PLAY_CONTROL,
				VIA_RPCTRL_TERMINATE, 1);
		}
	} else {
		if (go == PCMTRIG_START) {
			ado = &via->sgd_table[SEGS_PER_CHAN];
DEB(printf("ado located at va=%p pa=%x\n", ado, vtophys(ado)));
			via_wr(via, VIA_RECORD_DMAOPS_BASE,
				vtophys(ado),4);
			via_wr(via, VIA_RECORD_CONTROL,
				VIA_RPCTRL_START, 1);
		}
		else {
			/* Stop DMA */
			via_wr(via, VIA_RECORD_CONTROL,
				VIA_RPCTRL_TERMINATE, 1);
		}
	}

DEB(printf("viachan_trigger: go=%d\n", go));
	return 0;
}

static int
viachan_getptr(void *data)
{
	struct via_chinfo *ch = data;
	struct via_info *via = ch->parent;
	struct via_dma_op *ado;
	int	ptr, base, len, seg;
	int base1;

	if (ch->dir == PCMDIR_PLAY) {
		ado = &via->sgd_table[0];
		base1 = via_rd(via, VIA_PLAY_DMAOPS_BASE, 4);
		len = via_rd(via, VIA_PLAY_DMAOPS_COUNT, 4);
		base = via_rd(via, VIA_PLAY_DMAOPS_BASE, 4);
		if (base != base1) {	/* Avoid race hazzard	*/
			len = via_rd(via, VIA_PLAY_DMAOPS_COUNT, 4);
		}
DEB(printf("viachan_getptr: len / base = %x / %x\n", len, base));

		/* Base points to SGD segment to do, one past current */

		/* Determine how many segments have been done */
		seg = (base - vtophys(ado)) / sizeof(struct via_dma_op);
		if (seg == 0) seg = SEGS_PER_CHAN;

		/* Now work out offset: seg less count */
		ptr = seg * ch->buffer->bufsize / SEGS_PER_CHAN - len;
DEB(printf("return ptr=%d\n", ptr));
		return ptr;
	}
	else {
		base1 = via_rd(via, VIA_RECORD_DMAOPS_BASE, 4);
		ado = &via->sgd_table[SEGS_PER_CHAN];
		len = via_rd(via, VIA_RECORD_DMAOPS_COUNT, 4);
		base = via_rd(via, VIA_RECORD_DMAOPS_BASE, 4);
		if (base != base1) {	/* Avoid race hazzard	*/
			len = via_rd(via, VIA_RECORD_DMAOPS_COUNT, 4);
		}
DEB(printf("viachan_getptr: len / base = %x / %x\n", len, base));

		/* Base points to next block to do, one past current */

		/* Determine how many segments have been done */
		seg = (base - vtophys(ado)) / sizeof(struct via_dma_op);
		if (seg == 0) seg = SEGS_PER_CHAN;

		/* Now work out offset: seg less count */
		ptr = seg * ch->buffer->bufsize / SEGS_PER_CHAN - len;

		/* DMA appears to operate on memory 'lines' of 32 bytes	*/
		/* so don't return any part line - it isn't in RAM yet	*/
		ptr = ptr & ~0x1f;
DEB(printf("return ptr=%d\n", ptr));
		return ptr;
	}
	return 0;
}

static pcmchan_caps *
viachan_getcaps(void *data)
{
	struct via_chinfo *ch = data;
	return (ch->dir == PCMDIR_PLAY) ? &via_playcaps : &via_reccaps;
}

static void
via_intr(void *p)
{
	struct via_info *via = p;
	int		st;

DEB(printf("viachan_intr\n"));
	/* Read channel */
	st = via_rd(via, VIA_PLAY_STAT, 1);
	if (st & VIA_RPSTAT_INTR) {
		via_wr(via, VIA_PLAY_STAT, VIA_RPSTAT_INTR, 1);
		chn_intr(via->pch.channel);
	}

	/* Write channel */
	st = via_rd(via, VIA_RECORD_STAT, 1);
	if (st & VIA_RPSTAT_INTR) {
		via_wr(via, VIA_RECORD_STAT, VIA_RPSTAT_INTR, 1);
		chn_intr(via->rch.channel);
	}
}


