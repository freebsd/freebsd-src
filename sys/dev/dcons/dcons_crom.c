/*
 * Copyright (C) 2003
 * 	Hidetoshi Shimokawa. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *
 *	This product includes software developed by Hidetoshi Shimokawa.
 *
 * 4. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * $Id: dcons_crom.c,v 1.8 2003/10/23 15:47:21 simokawa Exp $
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/malloc.h>

#include <sys/bus.h>
#include <machine/bus.h>

#include <dev/firewire/firewire.h>
#include <dev/firewire/firewirereg.h>
#include <dev/firewire/iec13213.h>
#include <dev/dcons/dcons.h>

static bus_addr_t dcons_paddr;

#ifndef CSRVAL_VENDOR_PRIVATE
#define NEED_NEW_DRIVER
#endif

#define ADDR_HI(x)	(((x) >> 24) & 0xffffff)
#define ADDR_LO(x)	((x) & 0xffffff)

struct dcons_crom_softc {
        struct firewire_dev_comm fd;
	struct crom_chunk unit;
	struct crom_chunk spec;
	struct crom_chunk ver;
	bus_dma_tag_t dma_tag;
	bus_dmamap_t dma_map;
	bus_addr_t bus_addr;
};

static void
dcons_crom_identify(driver_t *driver, device_t parent)
{
	BUS_ADD_CHILD(parent, 0, "dcons_crom", device_get_unit(parent));
}

static int
dcons_crom_probe(device_t dev)
{
	device_t pa;

	pa = device_get_parent(dev);
	if(device_get_unit(dev) != device_get_unit(pa)){
		return(ENXIO);
	}

	device_set_desc(dev, "dcons configuration ROM");
	return (0);
}

#ifndef NEED_NEW_DRIVER
static void
dcons_crom_post_busreset(void *arg)
{
	struct dcons_crom_softc *sc;
	struct crom_src *src;
	struct crom_chunk *root;

	sc = (struct dcons_crom_softc *) arg;
	src = sc->fd.fc->crom_src;
	root = sc->fd.fc->crom_root;

	bzero(&sc->unit, sizeof(struct crom_chunk));

	crom_add_chunk(src, root, &sc->unit, CROM_UDIR);
	crom_add_entry(&sc->unit, CSRKEY_SPEC, CSRVAL_VENDOR_PRIVATE);
	crom_add_simple_text(src, &sc->unit, &sc->spec, "FreeBSD");
	crom_add_entry(&sc->unit, CSRKEY_VER, DCONS_CSR_VAL_VER);
	crom_add_simple_text(src, &sc->unit, &sc->ver, "dcons");
	crom_add_entry(&sc->unit, DCONS_CSR_KEY_HI, ADDR_HI(dcons_paddr));
	crom_add_entry(&sc->unit, DCONS_CSR_KEY_LO, ADDR_LO(dcons_paddr));
}
#endif

static void
dmamap_cb(void *arg, bus_dma_segment_t *segments, int seg, int error)
{
	struct dcons_crom_softc *sc;

	if (error)
		printf("dcons_dmamap_cb: error=%d\n", error);

	sc = (struct dcons_crom_softc *)arg;
	sc->bus_addr = segments[0].ds_addr;

	bus_dmamap_sync(sc->dma_tag, sc->dma_map, BUS_DMASYNC_PREWRITE);
	device_printf(sc->fd.dev,
#if __FreeBSD_version < 500000
	    "bus_addr 0x%x\n", sc->bus_addr);
#else
	    "bus_addr 0x%jx\n", (uintmax_t)sc->bus_addr);
#endif
	if (dcons_paddr != 0) {
		/* XXX */
		device_printf(sc->fd.dev, "dcons_paddr is already set\n");
		return;
	}
	dcons_dma_tag = sc->dma_tag;
	dcons_dma_map = sc->dma_map;
	dcons_paddr = sc->bus_addr;
}

static int
dcons_crom_attach(device_t dev)
{
#ifdef NEED_NEW_DRIVER
	printf("dcons_crom: you need newer firewire driver\n");
	return (-1);
#else
	struct dcons_crom_softc *sc;

        sc = (struct dcons_crom_softc *) device_get_softc(dev);
	sc->fd.fc = device_get_ivars(dev);
	sc->fd.dev = dev;
	sc->fd.post_explore = NULL;
	sc->fd.post_busreset = (void *) dcons_crom_post_busreset;

	/* map dcons buffer */
	bus_dma_tag_create(
		/*parent*/ sc->fd.fc->dmat,
		/*alignment*/ sizeof(u_int32_t),
		/*boundary*/ 0,
		/*lowaddr*/ BUS_SPACE_MAXADDR,
		/*highaddr*/ BUS_SPACE_MAXADDR,
		/*filter*/NULL, /*filterarg*/NULL,
		/*maxsize*/ dcons_bufsize,
		/*nsegments*/ 1,
		/*maxsegsz*/ BUS_SPACE_MAXSIZE_32BIT,
		/*flags*/ BUS_DMA_ALLOCNOW,
#if __FreeBSD_version >= 501102
		/*lockfunc*/busdma_lock_mutex,
		/*lockarg*/&Giant,
#endif
		&sc->dma_tag);
	bus_dmamap_create(sc->dma_tag, 0, &sc->dma_map);
	bus_dmamap_load(sc->dma_tag, sc->dma_map,
	    (void *)dcons_buf, dcons_bufsize,
	    dmamap_cb, sc, 0);
	return (0);
#endif
}

static int
dcons_crom_detach(device_t dev)
{
	struct dcons_crom_softc *sc;

        sc = (struct dcons_crom_softc *) device_get_softc(dev);
	sc->fd.post_busreset = NULL;

	/* XXX */
	if (dcons_dma_tag == sc->dma_tag)
		dcons_dma_tag = NULL;

	bus_dmamap_unload(sc->dma_tag, sc->dma_map);
	bus_dmamap_destroy(sc->dma_tag, sc->dma_map);
	bus_dma_tag_destroy(sc->dma_tag);

	return 0;
}

static devclass_t dcons_crom_devclass;

static device_method_t dcons_crom_methods[] = {
	/* device interface */
	DEVMETHOD(device_identify,	dcons_crom_identify),
	DEVMETHOD(device_probe,		dcons_crom_probe),
	DEVMETHOD(device_attach,	dcons_crom_attach),
	DEVMETHOD(device_detach,	dcons_crom_detach),
	{ 0, 0 }
};

static driver_t dcons_crom_driver = {
	"dcons_crom",
	dcons_crom_methods,
	sizeof(struct dcons_crom_softc),
};

DRIVER_MODULE(dcons_crom, firewire, dcons_crom_driver,
					dcons_crom_devclass, 0, 0);
MODULE_VERSION(dcons_crom, 1);
MODULE_DEPEND(dcons_crom, dcons,
	DCONS_VERSION, DCONS_VERSION, DCONS_VERSION);
MODULE_DEPEND(dcons_crom, firewire, 1, 1, 1);
