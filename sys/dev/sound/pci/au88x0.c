/*-
 * Copyright (c) 2003 Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/ac97.h>
#include <dev/sound/pci/au88x0.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>


/***************************************************************************\
 *                                                                         *
 *                          SUPPORTED CHIPSETS                             *
 *                                                                         *
\***************************************************************************/

static struct au88x0_chipset au88x0_chipsets[] = {
	{
		.auc_name		= "Aureal Vortex (8820)",
		.auc_pci_id		= 0x000112eb,

		.auc_control		= 0x1280c,

		.auc_irq_source		= 0x12800,
		.auc_irq_mask		= 0x12804,
		.auc_irq_control	= 0x12808,
		.auc_irq_status		= 0x1199c,

		.auc_dma_control	= 0x1060c,

		.auc_fifo_size		= 0x20,
		.auc_wt_fifos		= 32,
		.auc_wt_fifo_base	= 0x0e800,
		.auc_wt_fifo_ctl	= 0x0f800,
		.auc_wt_dma_ctl		= 0x10500,
		.auc_adb_fifos		= 16,
		.auc_adb_fifo_base	= 0x0e000,
		.auc_adb_fifo_ctl	= 0x0f840,
		.auc_adb_dma_ctl	= 0x10580,

		.auc_adb_route_base	= 0x10800,
		.auc_adb_route_bits	= 7,
		.auc_adb_codec_in	= 0x48,
		.auc_adb_codec_out	= 0x58,
	},
	{
		.auc_name		= "Aureal Vortex 2 (8830)",
		.auc_pci_id		= 0x000212eb,

		.auc_control		= 0x2a00c,

		.auc_irq_source		= 0x2a000,
		.auc_irq_mask		= 0x2a004,
		.auc_irq_control	= 0x2a008,
		.auc_irq_status		= 0x2919c,

		.auc_dma_control	= 0x27ae8,

		.auc_fifo_size		= 0x40,
		.auc_wt_fifos		= 64,
		.auc_wt_fifo_base	= 0x10000,
		.auc_wt_fifo_ctl	= 0x16000,
		.auc_wt_dma_ctl		= 0x27900,
		.auc_adb_fifos		= 32,
		.auc_adb_fifo_base	= 0x14000,
		.auc_adb_fifo_ctl	= 0x16100,
		.auc_adb_dma_ctl	= 0x27a00,

		.auc_adb_route_base	= 0x28000,
		.auc_adb_route_bits	= 8,
		.auc_adb_codec_in	= 0x70,
		.auc_adb_codec_out	= 0x88,
	},
	{
		.auc_name		= "Aureal Vortex Advantage (8810)",
		.auc_pci_id		= 0x000312eb,

		.auc_control		= 0x2a00c,

		.auc_irq_source		= 0x2a000,
		.auc_irq_mask		= 0x2a004,
		.auc_irq_control	= 0x2a008,
		.auc_irq_status		= 0x2919c,

		.auc_dma_control	= 0x27ae8,

		.auc_fifo_size		= 0x20,
		.auc_wt_fifos		= 32,
		.auc_wt_fifo_base	= 0x10000,
		.auc_wt_fifo_ctl	= 0x16000,
		.auc_wt_dma_ctl		= 0x27fd8,
		.auc_adb_fifos		= 16,
		.auc_adb_fifo_base	= 0x14000,
		.auc_adb_fifo_ctl	= 0x16100,
		.auc_adb_dma_ctl	= 0x27180,

		.auc_adb_route_base	= 0x28000,
		.auc_adb_route_bits	= 8,
		.auc_adb_codec_in	= 0x70,
		.auc_adb_codec_out	= 0x88,
	},
	{
		.auc_pci_id		= 0,
	}
};


/***************************************************************************\
 *                                                                         *
 *                       FORMATS AND CAPABILITIES                          *
 *                                                                         *
\***************************************************************************/

static u_int32_t au88x0_formats[] = {
	AFMT_U8,
	AFMT_STEREO | AFMT_U8,
	AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE,
	0
};

static struct pcmchan_caps au88x0_capabilities = {
	4000,			/* minimum sample rate */
	48000,			/* maximum sample rate */
	au88x0_formats,		/* supported formats */
	0			/* no particular capabilities */
};


/***************************************************************************\
 *                                                                         *
 *                           CODEC INTERFACE                               *
 *                                                                         *
\***************************************************************************/

/*
 * Read from the au88x0 register space
 */
#if 1
/* all our writes are 32-bit */
#define au88x0_read(aui, reg, n) \
	bus_space_read_4((aui)->aui_spct, (aui)->aui_spch, (reg))
#define au88x0_write(aui, reg, data, n) \
	bus_space_write_4((aui)->aui_spct, (aui)->aui_spch, (reg), (data))
#else
static uint32_t
au88x0_read(struct au88x0_info *aui, int reg, int size)
{
	uint32_t data;

	switch (size) {
	case 1:
		data = bus_space_read_1(aui->aui_spct, aui->aui_spch, reg);
		break;
	case 2:
		data = bus_space_read_2(aui->aui_spct, aui->aui_spch, reg);
		break;
	case 4:
		data = bus_space_read_4(aui->aui_spct, aui->aui_spch, reg);
		break;
	default:
		panic("unsupported read size %d", size);
	}
	return (data);
}

/*
 * Write to the au88x0 register space
 */
static void
au88x0_write(struct au88x0_info *aui, int reg, uint32_t data, int size)
{

	switch (size) {
	case 1:
		bus_space_write_1(aui->aui_spct, aui->aui_spch, reg, data);
		break;
	case 2:
		bus_space_write_2(aui->aui_spct, aui->aui_spch, reg, data);
		break;
	case 4:
		bus_space_write_4(aui->aui_spct, aui->aui_spch, reg, data);
		break;
	default:
		panic("unsupported write size %d", size);
	}
}
#endif

/*
 * Reset and initialize the codec
 */
static void
au88x0_codec_init(struct au88x0_info *aui)
{
	uint32_t data;
	int i;

	/* wave that chicken */
	au88x0_write(aui, AU88X0_CODEC_CONTROL, 0x8068, 4);
	DELAY(AU88X0_SETTLE_DELAY);
	au88x0_write(aui, AU88X0_CODEC_CONTROL, 0x00e8, 4);
	DELAY(1000);
	for (i = 0; i < 32; ++i) {
		au88x0_write(aui, AU88X0_CODEC_CHANNEL + i * 4, 0, 4);
		DELAY(AU88X0_SETTLE_DELAY);
	}
	au88x0_write(aui, AU88X0_CODEC_CONTROL, 0x00e8, 4);
	DELAY(AU88X0_SETTLE_DELAY);

	/* enable both codec channels */
	data = au88x0_read(aui, AU88X0_CODEC_ENABLE, 4);
	data |= (1 << (8 + 0)) | (1 << (8 + 1));
	au88x0_write(aui, AU88X0_CODEC_ENABLE, data, 4);
	DELAY(AU88X0_SETTLE_DELAY);
}

/*
 * Wait for the codec to get ready to accept a register write
 * Should be called at spltty
 */
static int
au88x0_codec_wait(struct au88x0_info *aui)
{
	uint32_t data;
	int i;

	for (i = 0; i < AU88X0_RETRY_COUNT; ++i) {
		data = au88x0_read(aui, AU88X0_CODEC_CONTROL, 4);
		if (data & AU88X0_CDCTL_WROK)
			return (0);
		DELAY(AU88X0_SETTLE_DELAY);
	}
	device_printf(aui->aui_dev, "timeout while waiting for codec\n");
	return (-1);
}

/*
 * Read from the ac97 codec
 */
static int
au88x0_codec_read(kobj_t obj, void *arg, int reg)
{
	struct au88x0_info *aui = arg;
	uint32_t data;
	int sl;

	sl = spltty();
	au88x0_codec_wait(aui);
	au88x0_write(aui, AU88X0_CODEC_IO, AU88X0_CDIO_READ(reg), 4);
	DELAY(1000);
	data = au88x0_read(aui, AU88X0_CODEC_IO, 4);
	splx(sl);
	data &= AU88X0_CDIO_DATA_MASK;
	data >>= AU88X0_CDIO_DATA_SHIFT;
	return (data);
}

/*
 * Write to the ac97 codec
 */
static int
au88x0_codec_write(kobj_t obj, void *arg, int reg, uint32_t data)
{
	struct au88x0_info *aui = arg;
	int sl;

	sl = spltty();
	au88x0_codec_wait(aui);
	au88x0_write(aui, AU88X0_CODEC_IO, AU88X0_CDIO_WRITE(reg, data), 4);
	splx(sl);
	return 0;
}

/*
 * Codec interface glue
 */
static kobj_method_t au88x0_ac97_methods[] = {
	KOBJMETHOD(ac97_read, au88x0_codec_read),
	KOBJMETHOD(ac97_write, au88x0_codec_write),
	{ 0, 0 }
};
AC97_DECLARE(au88x0_ac97);

#define au88x0_channel(aui, dir) \
	&(aui)->aui_chan[((dir) == PCMDIR_PLAY) ? 0 : 1]


/***************************************************************************\
 *                                                                         *
 *                          CHANNEL INTERFACE                              *
 *                                                                         *
\***************************************************************************/

/*
 * Initialize a PCM channel
 */
static void *
au88x0_chan_init(kobj_t obj, void *arg,
    struct snd_dbuf *buf, struct pcm_channel *chan, int dir)
{
	struct au88x0_info *aui = arg;
	struct au88x0_chan_info *auci = au88x0_channel(aui, dir);

	if (sndbuf_alloc(buf, aui->aui_dmat, aui->aui_bufsize) == -1)
		return (NULL);
	auci->auci_aui = aui;
	auci->auci_pcmchan = chan;
	auci->auci_buf = buf;
	auci->auci_dir = dir;
	return (auci);
}

/*
 * Set the data format for a PCM channel
 */
static int
au88x0_chan_setformat(kobj_t obj, void *arg, u_int32_t format)
{

	/* XXX */
	return (ENXIO);
}

/*
 * Set the sample rate for a PCM channel
 */
static int
au88x0_chan_setspeed(kobj_t obj, void *arg, u_int32_t speed)
{

	/* XXX */
	return (speed);
}

/*
 * Set the block size for a PCM channel
 */
static int
au88x0_chan_setblocksize(kobj_t obj, void *arg, u_int32_t blocksize)
{

	/* XXX */
	return (blocksize);
}

/*
 * Initiate a data transfer
 */
static int
au88x0_chan_trigger(kobj_t obj, void *arg, int trigger)
{
	struct au88x0_chan_info *auci = arg;

	(void)auci;
	switch (trigger) {
	case PCMTRIG_START:
		break;
	case PCMTRIG_STOP:
	case PCMTRIG_ABORT:
		break;
	}
	return (0);
}

/*
 *
 */
static int
au88x0_chan_getptr(kobj_t obj, void *arg)
{

	/* XXX */
	return (0);
}

/*
 * Return the capabilities of a PCM channel
 */
static struct pcmchan_caps *
au88x0_chan_getcaps(kobj_t obj, void *arg)
{

	return (&au88x0_capabilities);
}

/*
 * Channel interface glue
 */
static kobj_method_t au88x0_chan_methods[] = {
	KOBJMETHOD(channel_init,		au88x0_chan_init),
	KOBJMETHOD(channel_setformat,		au88x0_chan_setformat),
	KOBJMETHOD(channel_setspeed,		au88x0_chan_setspeed),
	KOBJMETHOD(channel_setblocksize,	au88x0_chan_setblocksize),
	KOBJMETHOD(channel_trigger,		au88x0_chan_trigger),
	KOBJMETHOD(channel_getptr,		au88x0_chan_getptr),
	KOBJMETHOD(channel_getcaps,		au88x0_chan_getcaps),
	{ 0, 0 }
};
CHANNEL_DECLARE(au88x0_chan);


/***************************************************************************\
 *                                                                         *
 *                          INTERRUPT HANDLER                              *
 *                                                                         *
\***************************************************************************/

static void
au88x0_intr(void *arg)
{
	struct au88x0_info *aui = arg;
	struct au88x0_chipset *auc = aui->aui_chipset;
	int pending, source;

	pending = au88x0_read(aui, auc->auc_irq_control, 4);
	if ((pending & AU88X0_IRQ_PENDING_BIT) == 0)
		return;
	source = au88x0_read(aui, auc->auc_irq_source, 4);
	if (source & AU88X0_IRQ_FATAL_ERR)
		device_printf(aui->aui_dev,
		    "fatal error interrupt received\n");
	if (source & AU88X0_IRQ_PARITY_ERR)
		device_printf(aui->aui_dev,
		    "parity error interrupt received\n");
	/* XXX handle the others... */

	/* acknowledge the interrupts we just handled */
	au88x0_write(aui, auc->auc_irq_source, source, 4);
	au88x0_read(aui, auc->auc_irq_source, 4);
}


/***************************************************************************\
 *                                                                         *
 *                            INITIALIZATION                               *
 *                                                                         *
\***************************************************************************/

/*
 * Reset and initialize the ADB and WT FIFOs
 *
 *  - need to find out what the magic values 0x42000 and 0x2000 mean.
 */
static void
au88x0_fifo_init(struct au88x0_info *aui)
{
	struct au88x0_chipset *auc = aui->aui_chipset;
	int i;

	/* reset, then clear the ADB FIFOs */
	for (i = 0; i < auc->auc_adb_fifos; ++i)
		au88x0_write(aui, auc->auc_adb_fifo_ctl + i * 4, 0x42000, 4);
	for (i = 0; i < auc->auc_adb_fifos * auc->auc_fifo_size; ++i)
		au88x0_write(aui, auc->auc_adb_fifo_base + i * 4, 0, 4);

	/* reset, then clear the WT FIFOs */
	for (i = 0; i < auc->auc_wt_fifos; ++i)
		au88x0_write(aui, auc->auc_wt_fifo_ctl + i * 4, 0x42000, 4);
	for (i = 0; i < auc->auc_wt_fifos * auc->auc_fifo_size; ++i)
		au88x0_write(aui, auc->auc_wt_fifo_base + i * 4, 0, 4);
}

/*
 * Hardware initialization
 */
static void
au88x0_init(struct au88x0_info *aui)
{
	struct au88x0_chipset *auc = aui->aui_chipset;

	/* reset the chip */
	au88x0_write(aui, auc->auc_control, 0xffffffff, 4);
	DELAY(10000);

	/* clear all interrupts */
	au88x0_write(aui, auc->auc_irq_source, 0xffffffff, 4);
	au88x0_read(aui, auc->auc_irq_source, 4);
	au88x0_read(aui, auc->auc_irq_status, 4);

	/* initialize the codec */
	au88x0_codec_init(aui);

	/* initialize the fifos */
	au88x0_fifo_init(aui);

	/* initialize the DMA engine */
	/* XXX chicken-waving! */
	au88x0_write(aui, auc->auc_dma_control, 0x1380000, 4);
}

/*
 * Construct and set status string
 */
static void
au88x0_set_status(device_t dev)
{
	char status[SND_STATUSLEN];
	struct au88x0_info *aui;

	aui = pcm_getdevinfo(dev);
	snprintf(status, sizeof status, "at %s 0x%lx irq %ld",
	    (aui->aui_regtype == SYS_RES_IOPORT)? "io" : "memory",
	    rman_get_start(aui->aui_reg), rman_get_start(aui->aui_irq));
	pcm_setstatus(dev, status);
}


/***************************************************************************\
 *                                                                         *
 *                            PCI INTERFACE                                *
 *                                                                         *
\***************************************************************************/

/*
 * Probe
 */
static int
au88x0_pci_probe(device_t dev)
{
	struct au88x0_chipset *auc;
	uint32_t pci_id;

	pci_id = pci_get_devid(dev);
	for (auc = au88x0_chipsets; auc->auc_pci_id; ++auc) {
		if (auc->auc_pci_id == pci_id) {
			device_set_desc(dev, auc->auc_name);
			return (0);
		}
	}
	return (ENXIO);
}

/*
 * Attach
 */
static int
au88x0_pci_attach(device_t dev)
{
	struct au88x0_chipset *auc;
	struct au88x0_info *aui = NULL;
	uint32_t config;
	int error;

	if ((aui = malloc(sizeof *aui, M_DEVBUF, M_NOWAIT|M_ZERO)) == NULL) {
		device_printf(dev, "failed to allocate softc\n");
		return (ENXIO);
	}
	aui->aui_dev = dev;

	/* Model-specific parameters */
	aui->aui_model = pci_get_devid(dev);
	for (auc = au88x0_chipsets; auc->auc_pci_id; ++auc)
		if (auc->auc_pci_id == aui->aui_model)
			aui->aui_chipset = auc;
	if (aui->aui_chipset == NULL)
		panic("%s() called for non-au88x0 device", __func__);

	/* enable pio, mmio, bus-mastering dma */
	config = pci_read_config(dev, PCIR_COMMAND, 2);
	config |= (PCIM_CMD_PORTEN|PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCIR_COMMAND, config, 2);

	/* register mapping */
	config = pci_read_config(dev, PCIR_COMMAND, 2);
	if (config & PCIM_CMD_MEMEN) {
		/* try memory-mapped I/O */
		aui->aui_regid = PCIR_BAR(0);
		aui->aui_regtype = SYS_RES_MEMORY;
		aui->aui_reg = bus_alloc_resource(dev, aui->aui_regtype,
		    &aui->aui_regid, 0, ~0, 1, RF_ACTIVE);
	}
	if (aui->aui_reg == NULL && (config & PCIM_CMD_PORTEN)) {
		/* fall back on port I/O */
		aui->aui_regid = PCIR_BAR(0);
		aui->aui_regtype = SYS_RES_IOPORT;
		aui->aui_reg = bus_alloc_resource(dev, aui->aui_regtype,
		    &aui->aui_regid, 0, ~0, 1, RF_ACTIVE);
	}
	if (aui->aui_reg == NULL) {
		/* both mmio and pio failed... */
		device_printf(dev, "failed to map registers\n");
		goto failed;
	}
	aui->aui_spct = rman_get_bustag(aui->aui_reg);
	aui->aui_spch = rman_get_bushandle(aui->aui_reg);

	/* IRQ mapping */
	aui->aui_irqid = 0;
	aui->aui_irqtype = SYS_RES_IRQ;
	aui->aui_irq = bus_alloc_resource(dev, aui->aui_irqtype,
	    &aui->aui_irqid, 0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
	if (aui->aui_irq == 0) {
		device_printf(dev, "failed to map IRQ\n");
		goto failed;
	}

	/* install interrupt handler */
	error = snd_setup_intr(dev, aui->aui_irq, 0, au88x0_intr,
	    aui, &aui->aui_irqh);
	if (error != 0) {
		device_printf(dev, "failed to install interrupt handler\n");
		goto failed;
	}

	/* DMA mapping */
	aui->aui_bufsize = pcm_getbuffersize(dev, AU88X0_BUFSIZE_MIN,
	    AU88X0_BUFSIZE_DFLT, AU88X0_BUFSIZE_MAX);
	error = bus_dma_tag_create(NULL,
	    2, 0, /* 16-bit alignment, no boundary */
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, /* restrict to 4GB */
	    NULL, NULL, /* no filter */
	    aui->aui_bufsize, 1, aui->aui_bufsize,
	    0, busdma_lock_mutex, &Giant, &aui->aui_dmat);
	if (error != 0) {
		device_printf(dev, "failed to create DMA tag\n");
		goto failed;
	}

	/* initialize the hardware */
	au88x0_init(aui);

	/* initialize the ac97 codec and mixer */
	if ((aui->aui_ac97i = AC97_CREATE(dev, aui, au88x0_ac97)) == NULL) {
		device_printf(dev, "failed to initialize ac97 codec\n");
		goto failed;
	}
	if (mixer_init(dev, ac97_getmixerclass(), aui->aui_ac97i) != 0) {
		device_printf(dev, "failed to initialize ac97 mixer\n");
		goto failed;
	}

	/* register with the pcm driver */
	if (pcm_register(dev, aui, 0, 0))
		goto failed;
	pcm_addchan(dev, PCMDIR_PLAY, &au88x0_chan_class, aui);
#if 0
	pcm_addchan(dev, PCMDIR_REC, &au88x0_chan_class, aui);
#endif
	au88x0_set_status(dev);

	return (0);
failed:
	if (aui->aui_ac97i != NULL)
		ac97_destroy(aui->aui_ac97i);
	if (aui->aui_dmat)
		bus_dma_tag_destroy(aui->aui_dmat);
	if (aui->aui_irqh != NULL)
		bus_teardown_intr(dev, aui->aui_irq, aui->aui_irqh);
	if (aui->aui_irq)
		bus_release_resource(dev, aui->aui_irqtype,
		    aui->aui_irqid, aui->aui_irq);
	if (aui->aui_reg)
		bus_release_resource(dev, aui->aui_regtype,
		    aui->aui_regid, aui->aui_reg);
	free(aui, M_DEVBUF);
	return (ENXIO);
}

/*
 * Detach
 */
static int
au88x0_pci_detach(device_t dev)
{
	struct au88x0_info *aui;
	int error;

	aui = pcm_getdevinfo(dev);
	if ((error = pcm_unregister(dev)) != 0)
		return (error);

	/* release resources in reverse order */
	bus_dma_tag_destroy(aui->aui_dmat);
	bus_teardown_intr(dev, aui->aui_irq, aui->aui_irqh);
	bus_release_resource(dev, aui->aui_irqtype,
	    aui->aui_irqid, aui->aui_irq);
	bus_release_resource(dev, aui->aui_regtype,
	    aui->aui_regid, aui->aui_reg);
	free(aui, M_DEVBUF);

	return (0);
}

/*
 * Driver glue
 */
static device_method_t au88x0_methods[] = {
	DEVMETHOD(device_probe,		au88x0_pci_probe),
	DEVMETHOD(device_attach,	au88x0_pci_attach),
	DEVMETHOD(device_detach,	au88x0_pci_detach),
	{ 0, 0 }
};

static driver_t au88x0_driver = {
	"pcm",
	au88x0_methods,
	PCM_SOFTC_SIZE,
};

DRIVER_MODULE(snd_au88x0, pci, au88x0_driver, pcm_devclass, 0, 0);
MODULE_DEPEND(snd_au88x0, snd_pcm, PCM_MINVER, PCM_PREFVER, PCM_MAXVER);
MODULE_VERSION(snd_au88x0, 1);
