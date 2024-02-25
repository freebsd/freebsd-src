/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012-2016 Ruslan Bukin <br@bsdpad.com>
 * Copyright (c) 2023-2024 Florian Walpen <dev@submerge.ch>
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

/*
 * RME HDSPe driver for FreeBSD.
 * Supported cards: AIO, RayDAT.
 */

#include <sys/types.h>
#include <sys/sysctl.h>

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pci/hdspe.h>
#include <dev/sound/chip.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <mixer_if.h>

static bool hdspe_unified_pcm = false;

static SYSCTL_NODE(_hw, OID_AUTO, hdspe, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "PCI HDSPe");

SYSCTL_BOOL(_hw_hdspe, OID_AUTO, unified_pcm, CTLFLAG_RWTUN,
    &hdspe_unified_pcm, 0, "Combine physical ports in one unified pcm device");

static struct hdspe_clock_source hdspe_clock_source_table_rd[] = {
	{ "internal", 0 << 1 | 1, HDSPE_STATUS1_CLOCK(15),       0,       0 },
	{ "word",     0 << 1 | 0, HDSPE_STATUS1_CLOCK( 0), 1 << 24, 1 << 25 },
	{ "aes",      1 << 1 | 0, HDSPE_STATUS1_CLOCK( 1),  1 << 0,  1 << 8 },
	{ "spdif",    2 << 1 | 0, HDSPE_STATUS1_CLOCK( 2),  1 << 1,  1 << 9 },
	{ "adat1",    3 << 1 | 0, HDSPE_STATUS1_CLOCK( 3),  1 << 2, 1 << 10 },
	{ "adat2",    4 << 1 | 0, HDSPE_STATUS1_CLOCK( 4),  1 << 3, 1 << 11 },
	{ "adat3",    5 << 1 | 0, HDSPE_STATUS1_CLOCK( 5),  1 << 4, 1 << 12 },
	{ "adat4",    6 << 1 | 0, HDSPE_STATUS1_CLOCK( 6),  1 << 5, 1 << 13 },
	{ "tco",      9 << 1 | 0, HDSPE_STATUS1_CLOCK( 9), 1 << 26, 1 << 27 },
	{ "sync_in", 10 << 1 | 0, HDSPE_STATUS1_CLOCK(10),       0,       0 },
	{ NULL,       0 << 1 | 0, HDSPE_STATUS1_CLOCK( 0),       0,       0 },
};

static struct hdspe_clock_source hdspe_clock_source_table_aio[] = {
	{ "internal", 0 << 1 | 1, HDSPE_STATUS1_CLOCK(15),       0,       0 },
	{ "word",     0 << 1 | 0, HDSPE_STATUS1_CLOCK( 0), 1 << 24, 1 << 25 },
	{ "aes",      1 << 1 | 0, HDSPE_STATUS1_CLOCK( 1),  1 << 0,  1 << 8 },
	{ "spdif",    2 << 1 | 0, HDSPE_STATUS1_CLOCK( 2),  1 << 1,  1 << 9 },
	{ "adat",     3 << 1 | 0, HDSPE_STATUS1_CLOCK( 3),  1 << 2, 1 << 10 },
	{ "tco",      9 << 1 | 0, HDSPE_STATUS1_CLOCK( 9), 1 << 26, 1 << 27 },
	{ "sync_in", 10 << 1 | 0, HDSPE_STATUS1_CLOCK(10),       0,       0 },
	{ NULL,       0 << 1 | 0, HDSPE_STATUS1_CLOCK( 0),       0,       0 },
};

static struct hdspe_channel chan_map_aio[] = {
	{ HDSPE_CHAN_AIO_LINE,    "line" },
	{ HDSPE_CHAN_AIO_PHONE,  "phone" },
	{ HDSPE_CHAN_AIO_AES,      "aes" },
	{ HDSPE_CHAN_AIO_SPDIF, "s/pdif" },
	{ HDSPE_CHAN_AIO_ADAT,    "adat" },
	{ 0,                        NULL },
};

static struct hdspe_channel chan_map_aio_uni[] = {
	{ HDSPE_CHAN_AIO_ALL, "all" },
	{ 0,                   NULL },
};

static struct hdspe_channel chan_map_rd[] = {
	{ HDSPE_CHAN_RAY_AES,      "aes" },
	{ HDSPE_CHAN_RAY_SPDIF, "s/pdif" },
	{ HDSPE_CHAN_RAY_ADAT1,  "adat1" },
	{ HDSPE_CHAN_RAY_ADAT2,  "adat2" },
	{ HDSPE_CHAN_RAY_ADAT3,  "adat3" },
	{ HDSPE_CHAN_RAY_ADAT4,  "adat4" },
	{ 0,                        NULL },
};

static struct hdspe_channel chan_map_rd_uni[] = {
	{ HDSPE_CHAN_RAY_ALL, "all" },
	{ 0,                   NULL },
};

static void
hdspe_intr(void *p)
{
	struct sc_pcminfo *scp;
	struct sc_info *sc;
	device_t *devlist;
	int devcount;
	int status;
	int err;
	int i;

	sc = (struct sc_info *)p;

	snd_mtxlock(sc->lock);

	status = hdspe_read_1(sc, HDSPE_STATUS_REG);
	if (status & HDSPE_AUDIO_IRQ_PENDING) {
		if ((err = device_get_children(sc->dev, &devlist, &devcount)) != 0)
			return;

		for (i = 0; i < devcount; i++) {
			scp = device_get_ivars(devlist[i]);
			if (scp->ih != NULL)
				scp->ih(scp);
		}

		hdspe_write_1(sc, HDSPE_INTERRUPT_ACK, 0);
		free(devlist, M_TEMP);
	}

	snd_mtxunlock(sc->lock);
}

static void
hdspe_dmapsetmap(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
#if 0
	device_printf(sc->dev, "hdspe_dmapsetmap()\n");
#endif
}

static int
hdspe_alloc_resources(struct sc_info *sc)
{

	/* Allocate resource. */
	sc->csid = PCIR_BAR(0);
	sc->cs = bus_alloc_resource_any(sc->dev, SYS_RES_MEMORY,
	    &sc->csid, RF_ACTIVE);

	if (!sc->cs) {
		device_printf(sc->dev, "Unable to map SYS_RES_MEMORY.\n");
		return (ENXIO);
	}

	sc->cst = rman_get_bustag(sc->cs);
	sc->csh = rman_get_bushandle(sc->cs);

	/* Allocate interrupt resource. */
	sc->irqid = 0;
	sc->irq = bus_alloc_resource_any(sc->dev, SYS_RES_IRQ, &sc->irqid,
	    RF_ACTIVE | RF_SHAREABLE);

	if (!sc->irq ||
	    bus_setup_intr(sc->dev, sc->irq, INTR_MPSAFE | INTR_TYPE_AV,
		NULL, hdspe_intr, sc, &sc->ih)) {
		device_printf(sc->dev, "Unable to alloc interrupt resource.\n");
		return (ENXIO);
	}

	/* Allocate DMA resources. */
	if (bus_dma_tag_create(/*parent*/bus_get_dma_tag(sc->dev),
		/*alignment*/4,
		/*boundary*/0,
		/*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
		/*highaddr*/BUS_SPACE_MAXADDR,
		/*filter*/NULL,
		/*filterarg*/NULL,
		/*maxsize*/2 * HDSPE_DMASEGSIZE,
		/*nsegments*/2,
		/*maxsegsz*/HDSPE_DMASEGSIZE,
		/*flags*/0,
		/*lockfunc*/NULL,
		/*lockarg*/NULL,
		/*dmatag*/&sc->dmat) != 0) {
		device_printf(sc->dev, "Unable to create dma tag.\n");
		return (ENXIO);
	}

	sc->bufsize = HDSPE_DMASEGSIZE;

	/* pbuf (play buffer). */
	if (bus_dmamem_alloc(sc->dmat, (void **)&sc->pbuf, BUS_DMA_WAITOK,
	    &sc->pmap)) {
		device_printf(sc->dev, "Can't alloc pbuf.\n");
		return (ENXIO);
	}

	if (bus_dmamap_load(sc->dmat, sc->pmap, sc->pbuf, sc->bufsize,
	    hdspe_dmapsetmap, sc, BUS_DMA_NOWAIT)) {
		device_printf(sc->dev, "Can't load pbuf.\n");
		return (ENXIO);
	}

	/* rbuf (rec buffer). */
	if (bus_dmamem_alloc(sc->dmat, (void **)&sc->rbuf, BUS_DMA_WAITOK,
	    &sc->rmap)) {
		device_printf(sc->dev, "Can't alloc rbuf.\n");
		return (ENXIO);
	}

	if (bus_dmamap_load(sc->dmat, sc->rmap, sc->rbuf, sc->bufsize,
	    hdspe_dmapsetmap, sc, BUS_DMA_NOWAIT)) {
		device_printf(sc->dev, "Can't load rbuf.\n");
		return (ENXIO);
	}

	bzero(sc->pbuf, sc->bufsize);
	bzero(sc->rbuf, sc->bufsize);

	return (0);
}

static void
hdspe_map_dmabuf(struct sc_info *sc)
{
	uint32_t paddr, raddr;
	int i;

	paddr = vtophys(sc->pbuf);
	raddr = vtophys(sc->rbuf);

	for (i = 0; i < HDSPE_MAX_SLOTS * 16; i++) {
		hdspe_write_4(sc, HDSPE_PAGE_ADDR_BUF_OUT + 4 * i,
                    paddr + i * 4096);
		hdspe_write_4(sc, HDSPE_PAGE_ADDR_BUF_IN + 4 * i,
                    raddr + i * 4096);
	}
}

static int
hdspe_sysctl_sample_rate(SYSCTL_HANDLER_ARGS)
{
	struct sc_info *sc = oidp->oid_arg1;
	int error;
	unsigned int speed, multiplier;

	speed = sc->force_speed;

	/* Process sysctl (unsigned) integer request. */
	error = sysctl_handle_int(oidp, &speed, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	/* Speed from 32000 to 192000, 0 falls back to pcm speed setting. */
	sc->force_speed = 0;
	if (speed > 0) {
		multiplier = 1;
		if (speed > (96000 + 128000) / 2)
			multiplier = 4;
		else if (speed > (48000 + 64000) / 2)
			multiplier = 2;

		if (speed < ((32000 + 44100) / 2) * multiplier)
			sc->force_speed = 32000 * multiplier;
		else if (speed < ((44100 + 48000) / 2) * multiplier)
			sc->force_speed = 44100 * multiplier;
		else
			sc->force_speed = 48000 * multiplier;
	}

	return (0);
}


static int
hdspe_sysctl_period(SYSCTL_HANDLER_ARGS)
{
	struct sc_info *sc = oidp->oid_arg1;
	int error;
	unsigned int period;

	period = sc->force_period;

	/* Process sysctl (unsigned) integer request. */
	error = sysctl_handle_int(oidp, &period, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	/* Period is from 2^5 to 2^14, 0 falls back to pcm latency settings. */
	sc->force_period = 0;
	if (period > 0) {
		sc->force_period = 32;
		while (sc->force_period < period && sc->force_period < 4096)
			sc->force_period <<= 1;
	}

	return (0);
}

static int
hdspe_sysctl_clock_preference(SYSCTL_HANDLER_ARGS)
{
	struct sc_info *sc;
	struct hdspe_clock_source *clock_table, *clock;
	char buf[16] = "invalid";
	int error;
	uint32_t setting;

	sc = oidp->oid_arg1;

	/* Select sync ports table for device type. */
	if (sc->type == HDSPE_AIO)
		clock_table = hdspe_clock_source_table_aio;
	else if (sc->type == HDSPE_RAYDAT)
		clock_table = hdspe_clock_source_table_rd;
	else
		return (ENXIO);

	/* Extract preferred clock source from settings register. */
	setting = sc->settings_register & HDSPE_SETTING_CLOCK_MASK;
	for (clock = clock_table; clock->name != NULL; ++clock) {
		if (clock->setting == setting)
			break;
	}
	if (clock->name != NULL)
		strlcpy(buf, clock->name, sizeof(buf));

	/* Process sysctl string request. */
	error = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	/* Find clock source matching the sysctl string. */
	for (clock = clock_table; clock->name != NULL; ++clock) {
		if (strncasecmp(buf, clock->name, sizeof(buf)) == 0)
			break;
	}

	/* Set preferred clock source in settings register. */
	if (clock->name != NULL) {
		setting = clock->setting & HDSPE_SETTING_CLOCK_MASK;
		snd_mtxlock(sc->lock);
		sc->settings_register &= ~HDSPE_SETTING_CLOCK_MASK;
		sc->settings_register |= setting;
		hdspe_write_4(sc, HDSPE_SETTINGS_REG, sc->settings_register);
		snd_mtxunlock(sc->lock);
	}
	return (0);
}

static int
hdspe_sysctl_clock_source(SYSCTL_HANDLER_ARGS)
{
	struct sc_info *sc;
	struct hdspe_clock_source *clock_table, *clock;
	char buf[16] = "invalid";
	uint32_t status;

	sc = oidp->oid_arg1;

	/* Select sync ports table for device type. */
	if (sc->type == HDSPE_AIO)
		clock_table = hdspe_clock_source_table_aio;
	else if (sc->type == HDSPE_RAYDAT)
		clock_table = hdspe_clock_source_table_rd;
	else
		return (ENXIO);

	/* Read current (autosync) clock source from status register. */
	snd_mtxlock(sc->lock);
	status = hdspe_read_4(sc, HDSPE_STATUS1_REG);
	status &= HDSPE_STATUS1_CLOCK_MASK;
	snd_mtxunlock(sc->lock);

	/* Translate status register value to clock source. */
	for (clock = clock_table; clock->name != NULL; ++clock) {
		/* In clock master mode, override with internal clock source. */
		if (sc->settings_register & HDSPE_SETTING_MASTER) {
			if (clock->setting & HDSPE_SETTING_MASTER)
				break;
		} else if (clock->status == status)
			break;
	}

	/* Process sysctl string request. */
	if (clock->name != NULL)
		strlcpy(buf, clock->name, sizeof(buf));
	return (sysctl_handle_string(oidp, buf, sizeof(buf), req));
}

static int
hdspe_sysctl_clock_list(SYSCTL_HANDLER_ARGS)
{
	struct sc_info *sc;
	struct hdspe_clock_source *clock_table, *clock;
	char buf[256];
	int n;

	sc = oidp->oid_arg1;
	n = 0;

	/* Select clock source table for device type. */
	if (sc->type == HDSPE_AIO)
		clock_table = hdspe_clock_source_table_aio;
	else if (sc->type == HDSPE_RAYDAT)
		clock_table = hdspe_clock_source_table_rd;
	else
		return (ENXIO);

	/* List available clock sources. */
	buf[0] = 0;
	for (clock = clock_table; clock->name != NULL; ++clock) {
		if (n > 0)
			n += strlcpy(buf + n, ",", sizeof(buf) - n);
		n += strlcpy(buf + n, clock->name, sizeof(buf) - n);
	}
	return (sysctl_handle_string(oidp, buf, sizeof(buf), req));
}

static int
hdspe_sysctl_sync_status(SYSCTL_HANDLER_ARGS)
{
	struct sc_info *sc;
	struct hdspe_clock_source *clock_table, *clock;
	char buf[256];
	char *state;
	int n;
	uint32_t status;

	sc = oidp->oid_arg1;
	n = 0;

	/* Select sync ports table for device type. */
	if (sc->type == HDSPE_AIO)
		clock_table = hdspe_clock_source_table_aio;
	else if (sc->type == HDSPE_RAYDAT)
		clock_table = hdspe_clock_source_table_rd;
	else
		return (ENXIO);

	/* Read current lock and sync bits from status register. */
	snd_mtxlock(sc->lock);
	status = hdspe_read_4(sc, HDSPE_STATUS1_REG);
	snd_mtxunlock(sc->lock);

	/* List clock sources with lock and sync state. */
	for (clock = clock_table; clock->name != NULL; ++clock) {
		if (clock->sync_bit != 0) {
			if (n > 0)
				n += strlcpy(buf + n, ",", sizeof(buf) - n);
			state = "none";
			if ((clock->sync_bit & status) != 0)
				state = "sync";
			else if ((clock->lock_bit & status) != 0)
				state = "lock";
			n += snprintf(buf + n, sizeof(buf) - n, "%s(%s)",
			    clock->name, state);
		}
	}
	return (sysctl_handle_string(oidp, buf, sizeof(buf), req));
}

static int
hdspe_probe(device_t dev)
{
	uint32_t rev;

	if (pci_get_vendor(dev) == PCI_VENDOR_XILINX &&
	    pci_get_device(dev) == PCI_DEVICE_XILINX_HDSPE) {
		rev = pci_get_revid(dev);
		switch (rev) {
		case PCI_REVISION_AIO:
			device_set_desc(dev, "RME HDSPe AIO");
			return (0);
		case PCI_REVISION_RAYDAT:
			device_set_desc(dev, "RME HDSPe RayDAT");
			return (0);
		}
	}

	return (ENXIO);
}

static int
hdspe_init(struct sc_info *sc)
{
	long long period;

	/* Set latency. */
	sc->period = 32;
	/*
	 * The pcm channel latency settings propagate unreliable blocksizes,
	 * different for recording and playback, and skewed due to rounding
	 * and total buffer size limits.
	 * Force period to a consistent default until these issues are fixed.
	 */
	sc->force_period = 256;
	sc->ctrl_register = hdspe_encode_latency(7);

	/* Set rate. */
	sc->speed = HDSPE_SPEED_DEFAULT;
	sc->force_speed = 0;
	sc->ctrl_register &= ~HDSPE_FREQ_MASK;
	sc->ctrl_register |= HDSPE_FREQ_MASK_DEFAULT;
	hdspe_write_4(sc, HDSPE_CONTROL_REG, sc->ctrl_register);

	switch (sc->type) {
	case HDSPE_RAYDAT:
	case HDSPE_AIO:
		period = HDSPE_FREQ_AIO;
		break;
	default:
		return (ENXIO);
	}

	/* Set DDS value. */
	period /= sc->speed;
	hdspe_write_4(sc, HDSPE_FREQ_REG, period);

	/* Other settings. */
	sc->settings_register = 0;
	hdspe_write_4(sc, HDSPE_SETTINGS_REG, sc->settings_register);

	return (0);
}

static int
hdspe_attach(device_t dev)
{
	struct hdspe_channel *chan_map;
	struct sc_pcminfo *scp;
	struct sc_info *sc;
	uint32_t rev;
	int i, err;

#if 0
	device_printf(dev, "hdspe_attach()\n");
#endif

	sc = device_get_softc(dev);
	sc->lock = snd_mtxcreate(device_get_nameunit(dev),
	    "snd_hdspe softc");
	sc->dev = dev;

	pci_enable_busmaster(dev);
	rev = pci_get_revid(dev);
	switch (rev) {
	case PCI_REVISION_AIO:
		sc->type = HDSPE_AIO;
		chan_map = hdspe_unified_pcm ? chan_map_aio_uni : chan_map_aio;
		break;
	case PCI_REVISION_RAYDAT:
		sc->type = HDSPE_RAYDAT;
		chan_map = hdspe_unified_pcm ? chan_map_rd_uni : chan_map_rd;
		break;
	default:
		return (ENXIO);
	}

	/* Allocate resources. */
	err = hdspe_alloc_resources(sc);
	if (err) {
		device_printf(dev, "Unable to allocate system resources.\n");
		return (ENXIO);
	}

	if (hdspe_init(sc) != 0)
		return (ENXIO);

	for (i = 0; i < HDSPE_MAX_CHANS && chan_map[i].descr != NULL; i++) {
		scp = malloc(sizeof(struct sc_pcminfo), M_DEVBUF, M_NOWAIT | M_ZERO);
		scp->hc = &chan_map[i];
		scp->sc = sc;
		scp->dev = device_add_child(dev, "pcm", -1);
		device_set_ivars(scp->dev, scp);
	}

	hdspe_map_dmabuf(sc);

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "sync_status", CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    sc, 0, hdspe_sysctl_sync_status, "A",
	    "List clock source signal lock and sync status");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "clock_source", CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    sc, 0, hdspe_sysctl_clock_source, "A",
	    "Currently effective clock source");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "clock_preference", CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, hdspe_sysctl_clock_preference, "A",
	    "Set 'internal' (master) or preferred autosync clock source");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "clock_list", CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    sc, 0, hdspe_sysctl_clock_list, "A",
	    "List of supported clock sources");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "period", CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, hdspe_sysctl_period, "A",
	    "Force period of samples per interrupt (32, 64, ... 4096)");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "sample_rate", CTLTYPE_UINT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, hdspe_sysctl_sample_rate, "A",
	    "Force sample rate (32000, 44100, 48000, ... 192000)");

	return (bus_generic_attach(dev));
}

static void
hdspe_dmafree(struct sc_info *sc)
{

	bus_dmamap_unload(sc->dmat, sc->rmap);
	bus_dmamap_unload(sc->dmat, sc->pmap);
	bus_dmamem_free(sc->dmat, sc->rbuf, sc->rmap);
	bus_dmamem_free(sc->dmat, sc->pbuf, sc->pmap);
	sc->rbuf = sc->pbuf = NULL;
}

static int
hdspe_detach(device_t dev)
{
	struct sc_info *sc;
	int err;

	sc = device_get_softc(dev);
	if (sc == NULL) {
		device_printf(dev,"Can't detach: softc is null.\n");
		return (0);
	}

	err = device_delete_children(dev);
	if (err)
		return (err);

	hdspe_dmafree(sc);

	if (sc->ih)
		bus_teardown_intr(dev, sc->irq, sc->ih);
	if (sc->dmat)
		bus_dma_tag_destroy(sc->dmat);
	if (sc->irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq);
	if (sc->cs)
		bus_release_resource(dev, SYS_RES_MEMORY, PCIR_BAR(0), sc->cs);
	if (sc->lock)
		snd_mtxfree(sc->lock);

	return (0);
}

static device_method_t hdspe_methods[] = {
	DEVMETHOD(device_probe,     hdspe_probe),
	DEVMETHOD(device_attach,    hdspe_attach),
	DEVMETHOD(device_detach,    hdspe_detach),
	{ 0, 0 }
};

static driver_t hdspe_driver = {
	"hdspe",
	hdspe_methods,
	PCM_SOFTC_SIZE,
};

DRIVER_MODULE(snd_hdspe, pci, hdspe_driver, 0, 0);
