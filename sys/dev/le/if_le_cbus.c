/*-
 * Copyright (c) 1994-2000
 *	Paul Richards. All rights reserved.
 *
 * PC-98 port by Chiharu Shibata & FreeBSD(98) porting team.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name Paul Richards may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL RICHARDS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PAUL RICHARDS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: FreeBSD: src/sys/dev/lnc/if_lnc_cbus.c,v 1.12 2005/11/12 19:14:21
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/socket.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_media.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <isa/isavar.h>

#include <dev/le/lancereg.h>
#include <dev/le/lancevar.h>
#include <dev/le/am7990var.h>

#define	LE_CBUS_MEMSIZE	(16*1024)
#define	CNET98S_IOSIZE	32
#define	CNET98S_RDP	0x10
#define	CNET98S_RAP	0x12
#define	CNET98S_RESET	0x14
#define	CNET98S_BDP	0x16

struct le_cbus_softc {
	struct am7990_softc	sc_am7990;	/* glue to MI code */

	struct resource		*sc_rres;

	struct resource		*sc_ires;
	void			*sc_ih;

	bus_dma_tag_t		sc_pdmat;
	bus_dma_tag_t		sc_dmat;
	bus_dmamap_t		sc_dmam;
};

static device_probe_t le_cbus_probe;
static device_attach_t le_cbus_attach;
static device_detach_t le_cbus_detach;
static device_resume_t le_cbus_resume;
static device_suspend_t le_cbus_suspend;

static device_method_t le_cbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		le_cbus_probe),
	DEVMETHOD(device_attach,	le_cbus_attach),
	DEVMETHOD(device_detach,	le_cbus_detach),
	/* We can just use the suspend method here. */
	DEVMETHOD(device_shutdown,	le_cbus_suspend),
	DEVMETHOD(device_suspend,	le_cbus_suspend),
	DEVMETHOD(device_resume,	le_cbus_resume),

	{ 0, 0 }
};

DEFINE_CLASS_0(le, le_cbus_driver, le_cbus_methods, sizeof(struct le_cbus_softc));
DRIVER_MODULE(le, isa, le_cbus_driver, le_devclass, 0, 0);
MODULE_DEPEND(le, ether, 1, 1, 1);

static bus_addr_t le_ioaddr_cnet98s[CNET98S_IOSIZE] = {
	0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007,
	0x008, 0x009, 0x00a, 0x00b, 0x00c, 0x00d, 0x00e, 0x00f,
	0x400, 0x401, 0x402, 0x403, 0x404, 0x405, 0x406, 0x407,
	0x408, 0x409, 0x40a, 0x40b, 0x40c, 0x40d, 0x40e, 0x40f,
};

static void le_cbus_wrbcr(struct lance_softc *, uint16_t, uint16_t);
#ifdef LEDEBUG
static uint16_t le_cbus_rdbcr(struct lance_softc *, uint16_t);
#endif
static void le_cbus_wrcsr(struct lance_softc *, uint16_t, uint16_t);
static uint16_t le_cbus_rdcsr(struct lance_softc *, uint16_t);
static void le_cbus_hwreset(struct lance_softc *);
static bus_dmamap_callback_t le_cbus_dma_callback;

static void
le_cbus_wrbcr(struct lance_softc *sc, uint16_t port, uint16_t val)
{
	struct le_cbus_softc *lesc = (struct le_cbus_softc *)sc;

	bus_write_2(lesc->sc_rres, CNET98S_RAP, port);
	bus_barrier(lesc->sc_rres, CNET98S_RAP, 2, BUS_SPACE_BARRIER_WRITE);
	bus_write_2(lesc->sc_rres, CNET98S_BDP, val);
}

#ifdef LEDEBUG
static uint16_t
le_cbus_rdbcr(struct lance_softc *sc, uint16_t port)
{
	struct le_cbus_softc *lesc = (struct le_cbus_softc *)sc;

	bus_write_2(lesc->sc_rres, CNET98S_RAP, port);
	bus_barrier(lesc->sc_rres, CNET98S_RAP, 2, BUS_SPACE_BARRIER_WRITE);
	return (bus_read_2(lesc->sc_rres, CNET98S_BDP));
}
#endif

static void
le_cbus_wrcsr(struct lance_softc *sc, uint16_t port, uint16_t val)
{
	struct le_cbus_softc *lesc = (struct le_cbus_softc *)sc;

	bus_write_2(lesc->sc_rres, CNET98S_RAP, port);
	bus_barrier(lesc->sc_rres, CNET98S_RAP, 2, BUS_SPACE_BARRIER_WRITE);
	bus_write_2(lesc->sc_rres, CNET98S_RDP, val);
}

static uint16_t
le_cbus_rdcsr(struct lance_softc *sc, uint16_t port)
{
	struct le_cbus_softc *lesc = (struct le_cbus_softc *)sc;

	bus_write_2(lesc->sc_rres, CNET98S_RAP, port);
	bus_barrier(lesc->sc_rres, CNET98S_RAP, 2, BUS_SPACE_BARRIER_WRITE);
	return (bus_read_2(lesc->sc_rres, CNET98S_RDP));
}

static void
le_cbus_hwreset(struct lance_softc *sc)
{
	struct le_cbus_softc *lesc = (struct le_cbus_softc *)sc;

	/*
	 * NB: These are Contec C-NET(98)S only.
	 */

	/* Reset the chip. */
	bus_write_2(lesc->sc_rres, CNET98S_RESET,
	    bus_read_2(lesc->sc_rres, CNET98S_RESET));
	DELAY(500);

	/* ISA bus configuration */
	/* ISACSR0 - set Master Mode Read Active time to 300ns. */
	le_cbus_wrbcr(sc, LE_BCR0, 0x0006);
	/* ISACSR1 - set Master Mode Write Active time to 300ns. */
	le_cbus_wrbcr(sc, LE_BCR1, 0x0006);
#ifdef LEDEBUG
	device_printf(dev, "ISACSR2=0x%x\n", le_cbus_rdbcr(sc, LE_BCR2));
#endif
	/* ISACSR5 - LED1 */
	le_cbus_wrbcr(sc, LE_BCR5, LE_B4_PSE | LE_B4_XMTE);
	/* ISACSR6 - LED2 */
	le_cbus_wrbcr(sc, LE_BCR6, LE_B4_PSE | LE_B4_RCVE);
	/* ISACSR7 - LED3 */
	le_cbus_wrbcr(sc, LE_BCR7, LE_B4_PSE | LE_B4_COLE);
}

static void
le_cbus_dma_callback(void *xsc, bus_dma_segment_t *segs, int nsegs, int error)
{
	struct lance_softc *sc = (struct lance_softc *)xsc;

	if (error != 0)
		return;
	KASSERT(nsegs == 1, ("%s: bad DMA segment count", __func__));
	sc->sc_addr = segs[0].ds_addr;
}

static int
le_cbus_probe(device_t dev)
{
	struct le_cbus_softc *lesc;
	struct lance_softc *sc;
	int error, i;

	/*
	 * Skip PnP devices as some wedge when trying to probe them as
	 * C-NET(98)S.
	 */
	if (isa_get_vendorid(dev))
		return (ENXIO);

	lesc = device_get_softc(dev);
	sc = &lesc->sc_am7990.lsc;

	i = 0;
	lesc->sc_rres = isa_alloc_resourcev(dev, SYS_RES_IOPORT, &i,
	    le_ioaddr_cnet98s, CNET98S_IOSIZE, RF_ACTIVE);
	if (lesc->sc_rres == NULL)
		return (ENXIO);
	isa_load_resourcev(lesc->sc_rres, le_ioaddr_cnet98s, CNET98S_IOSIZE);

	/* Reset the chip. */
	bus_write_2(lesc->sc_rres, CNET98S_RESET,
	    bus_read_2(lesc->sc_rres, CNET98S_RESET));
	DELAY(500);

	/* Stop the chip and put it in a known state. */
	le_cbus_wrcsr(sc, LE_CSR0, LE_C0_STOP);
	DELAY(100);
	if (le_cbus_rdcsr(sc, LE_CSR0) != LE_C0_STOP) {
		error = ENXIO;
		goto fail;
	}
	le_cbus_wrcsr(sc, LE_CSR3, 0);
	device_set_desc(dev, "C-NET(98)S");
	error = BUS_PROBE_DEFAULT;

 fail:
	bus_release_resource(dev, SYS_RES_IOPORT,
	    rman_get_rid(lesc->sc_rres), lesc->sc_rres);
	return (error);
}

static int
le_cbus_attach(device_t dev)
{
	struct le_cbus_softc *lesc;
	struct lance_softc *sc;
	int error, i;

	lesc = device_get_softc(dev);
	sc = &lesc->sc_am7990.lsc;

	LE_LOCK_INIT(sc, device_get_nameunit(dev));

	i = 0;
	lesc->sc_rres = isa_alloc_resourcev(dev, SYS_RES_IOPORT, &i,
	    le_ioaddr_cnet98s, CNET98S_IOSIZE, RF_ACTIVE);
	if (lesc->sc_rres == NULL) {
		device_printf(dev, "cannot allocate registers\n");
		error = ENXIO;
		goto fail_mtx;
	}
	isa_load_resourcev(lesc->sc_rres, le_ioaddr_cnet98s, CNET98S_IOSIZE);

	i = 0;
	if ((lesc->sc_ires = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &i, RF_SHAREABLE | RF_ACTIVE)) == NULL) {
		device_printf(dev, "cannot allocate interrupt\n");
		error = ENXIO;
		goto fail_rres;
	}

	error = bus_dma_tag_create(
	    bus_get_dma_tag(dev),	/* parent */
	    1, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR_24BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    BUS_SPACE_MAXSIZE_32BIT,	/* maxsize */
	    0,				/* nsegments */
	    BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &lesc->sc_pdmat);
	if (error != 0) {
		device_printf(dev, "cannot allocate parent DMA tag\n");
		goto fail_ires;
	}

	sc->sc_memsize = LE_CBUS_MEMSIZE;
	/*
	 * For Am79C90, Am79C961 and Am79C961A the init block must be 2-byte
	 * aligned and the ring descriptors must be 8-byte aligned.
	 */
	error = bus_dma_tag_create(
	    lesc->sc_pdmat,		/* parent */
	    8, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR_24BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    sc->sc_memsize,		/* maxsize */
	    1,				/* nsegments */
	    sc->sc_memsize,		/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &lesc->sc_dmat);
	if (error != 0) {
		device_printf(dev, "cannot allocate buffer DMA tag\n");
		goto fail_pdtag;
	}

	error = bus_dmamem_alloc(lesc->sc_dmat, (void **)&sc->sc_mem,
	    BUS_DMA_WAITOK | BUS_DMA_COHERENT, &lesc->sc_dmam);
	if (error != 0) {
		device_printf(dev, "cannot allocate DMA buffer memory\n");
		goto fail_dtag;
	}

	sc->sc_addr = 0;
	error = bus_dmamap_load(lesc->sc_dmat, lesc->sc_dmam, sc->sc_mem,
	    sc->sc_memsize, le_cbus_dma_callback, sc, 0);
	if (error != 0 || sc->sc_addr == 0) {
		device_printf(dev, "cannot load DMA buffer map\n");
		goto fail_dmem;
	}

	sc->sc_flags = 0;
	sc->sc_conf3 = 0;

	/*
	 * Extract the physical MAC address from the ROM.
	 */
	for (i = 0; i < sizeof(sc->sc_enaddr); i++)
		sc->sc_enaddr[i] = bus_read_1(lesc->sc_rres, i * 2);

	sc->sc_copytodesc = lance_copytobuf_contig;
	sc->sc_copyfromdesc = lance_copyfrombuf_contig;
	sc->sc_copytobuf = lance_copytobuf_contig;
	sc->sc_copyfrombuf = lance_copyfrombuf_contig;
	sc->sc_zerobuf = lance_zerobuf_contig;

	sc->sc_rdcsr = le_cbus_rdcsr;
	sc->sc_wrcsr = le_cbus_wrcsr;
	sc->sc_hwreset = le_cbus_hwreset;
	sc->sc_hwinit = NULL;
	sc->sc_hwintr = NULL;
	sc->sc_nocarrier = NULL;
	sc->sc_mediachange = NULL;
	sc->sc_mediastatus = NULL;
	sc->sc_supmedia = NULL;

	error = am7990_config(&lesc->sc_am7990, device_get_name(dev),
	    device_get_unit(dev));
	if (error != 0) {
		device_printf(dev, "cannot attach Am7990\n");
		goto fail_dmap;
	}

	error = bus_setup_intr(dev, lesc->sc_ires, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, am7990_intr, sc, &lesc->sc_ih);
	if (error != 0) {
		device_printf(dev, "cannot set up interrupt\n");
		goto fail_am7990;
	}

	return (0);

 fail_am7990:
	am7990_detach(&lesc->sc_am7990);
 fail_dmap:
	bus_dmamap_unload(lesc->sc_dmat, lesc->sc_dmam);
 fail_dmem:
	bus_dmamem_free(lesc->sc_dmat, sc->sc_mem, lesc->sc_dmam);
 fail_dtag:
	bus_dma_tag_destroy(lesc->sc_dmat);
 fail_pdtag:
	bus_dma_tag_destroy(lesc->sc_pdmat);
 fail_ires:
	bus_release_resource(dev, SYS_RES_IRQ,
	    rman_get_rid(lesc->sc_ires), lesc->sc_ires);
 fail_rres:
	bus_release_resource(dev, SYS_RES_IOPORT,
	    rman_get_rid(lesc->sc_rres), lesc->sc_rres);
 fail_mtx:
	LE_LOCK_DESTROY(sc);
	return (error);
}

static int
le_cbus_detach(device_t dev)
{
	struct le_cbus_softc *lesc;
	struct lance_softc *sc;

	lesc = device_get_softc(dev);
	sc = &lesc->sc_am7990.lsc;

	bus_teardown_intr(dev, lesc->sc_ires, lesc->sc_ih);
	am7990_detach(&lesc->sc_am7990);
	bus_dmamap_unload(lesc->sc_dmat, lesc->sc_dmam);
	bus_dmamem_free(lesc->sc_dmat, sc->sc_mem, lesc->sc_dmam);
	bus_dma_tag_destroy(lesc->sc_dmat);
	bus_dma_tag_destroy(lesc->sc_pdmat);
	bus_release_resource(dev, SYS_RES_IRQ,
	    rman_get_rid(lesc->sc_ires), lesc->sc_ires);
	bus_release_resource(dev, SYS_RES_IOPORT,
	    rman_get_rid(lesc->sc_rres), lesc->sc_rres);
	LE_LOCK_DESTROY(sc);

	return (0);
}

static int
le_cbus_suspend(device_t dev)
{
	struct le_cbus_softc *lesc;

	lesc = device_get_softc(dev);

	lance_suspend(&lesc->sc_am7990.lsc);

	return (0);
}

static int
le_cbus_resume(device_t dev)
{
	struct le_cbus_softc *lesc;

	lesc = device_get_softc(dev);

	lance_resume(&lesc->sc_am7990.lsc);

	return (0);
}
