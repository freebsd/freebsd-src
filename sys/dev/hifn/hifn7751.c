/* $FreeBSD$ */
/*	$OpenBSD: hifn7751.c,v 1.120 2002/05/17 00:33:34 deraadt Exp $	*/

/*
 * Invertex AEON / Hifn 7751 driver
 * Copyright (c) 1999 Invertex Inc. All rights reserved.
 * Copyright (c) 1999 Theo de Raadt
 * Copyright (c) 2000-2001 Network Security Technologies, Inc.
 *			http://www.netsec.net
 *
 * This driver is based on a previous driver by Invertex, for which they
 * requested:  Please send any comments, feedback, bug-fixes, or feature
 * requests to software@invertex.com.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
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
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

#define HIFN_DEBUG

/*
 * Driver for the Hifn 7751 encryption processor.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/clock.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <opencrypto/cryptodev.h>
#include <sys/random.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <dev/hifn/hifn7751reg.h>
#include <dev/hifn/hifn7751var.h>

/*
 * Prototypes and count for the pci_device structure
 */
static	int hifn_probe(device_t);
static	int hifn_attach(device_t);
static	int hifn_detach(device_t);
static	int hifn_suspend(device_t);
static	int hifn_resume(device_t);
static	void hifn_shutdown(device_t);

static device_method_t hifn_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		hifn_probe),
	DEVMETHOD(device_attach,	hifn_attach),
	DEVMETHOD(device_detach,	hifn_detach),
	DEVMETHOD(device_suspend,	hifn_suspend),
	DEVMETHOD(device_resume,	hifn_resume),
	DEVMETHOD(device_shutdown,	hifn_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	{ 0, 0 }
};
static driver_t hifn_driver = {
	"hifn",
	hifn_methods,
	sizeof (struct hifn_softc)
};
static devclass_t hifn_devclass;

DRIVER_MODULE(hifn, pci, hifn_driver, hifn_devclass, 0, 0);

static	void hifn_reset_board(struct hifn_softc *, int);
static	void hifn_reset_puc(struct hifn_softc *);
static	void hifn_puc_wait(struct hifn_softc *);
static	int hifn_enable_crypto(struct hifn_softc *);
static	void hifn_set_retry(struct hifn_softc *sc);
static	void hifn_init_dma(struct hifn_softc *);
static	void hifn_init_pci_registers(struct hifn_softc *);
static	int hifn_sramsize(struct hifn_softc *);
static	int hifn_dramsize(struct hifn_softc *);
static	int hifn_ramtype(struct hifn_softc *);
static	void hifn_sessions(struct hifn_softc *);
static	void hifn_intr(void *);
static	u_int hifn_write_command(struct hifn_command *, u_int8_t *);
static	u_int32_t hifn_next_signature(u_int32_t a, u_int cnt);
static	int hifn_newsession(void *, u_int32_t *, struct cryptoini *);
static	int hifn_freesession(void *, u_int64_t);
static	int hifn_process(void *, struct cryptop *, int);
static	void hifn_callback(struct hifn_softc *, struct hifn_command *, u_int8_t *);
static	int hifn_crypto(struct hifn_softc *, struct hifn_command *, struct cryptop *, int);
static	int hifn_readramaddr(struct hifn_softc *, int, u_int8_t *);
static	int hifn_writeramaddr(struct hifn_softc *, int, u_int8_t *);
static	int hifn_dmamap_load_src(struct hifn_softc *, struct hifn_command *);
static	int hifn_dmamap_load_dst(struct hifn_softc *, struct hifn_command *);
static	int hifn_init_pubrng(struct hifn_softc *);
static	void hifn_rng(void *);
static	void hifn_tick(void *);
static	void hifn_abort(struct hifn_softc *);
static	void hifn_alloc_slot(struct hifn_softc *, int *, int *, int *, int *);

static	void hifn_write_reg_0(struct hifn_softc *, bus_size_t, u_int32_t);
static	void hifn_write_reg_1(struct hifn_softc *, bus_size_t, u_int32_t);

static __inline__ u_int32_t
READ_REG_0(struct hifn_softc *sc, bus_size_t reg)
{
    u_int32_t v = bus_space_read_4(sc->sc_st0, sc->sc_sh0, reg);
    sc->sc_bar0_lastreg = (bus_size_t) -1;
    return (v);
}
#define	WRITE_REG_0(sc, reg, val)	hifn_write_reg_0(sc, reg, val)

static __inline__ u_int32_t
READ_REG_1(struct hifn_softc *sc, bus_size_t reg)
{
    u_int32_t v = bus_space_read_4(sc->sc_st1, sc->sc_sh1, reg);
    sc->sc_bar1_lastreg = (bus_size_t) -1;
    return (v);
}
#define	WRITE_REG_1(sc, reg, val)	hifn_write_reg_1(sc, reg, val)

#ifdef HIFN_DEBUG
static	int hifn_debug = 0;
SYSCTL_INT(_debug, OID_AUTO, hifn, CTLFLAG_RW, &hifn_debug,
	    0, "Hifn driver debugging printfs");
#endif

static	struct hifn_stats hifnstats;
SYSCTL_STRUCT(_kern, OID_AUTO, hifn_stats, CTLFLAG_RD, &hifnstats,
	    hifn_stats, "Hifn driver statistics");
static	int hifn_maxbatch = 2;		/* XXX tune based on part+sys speed */
SYSCTL_INT(_kern, OID_AUTO, hifn_maxbatch, CTLFLAG_RW, &hifn_maxbatch,
	    0, "Hifn driver: max ops to batch w/o interrupt");

/*
 * Probe for a supported device.  The PCI vendor and device
 * IDs are used to detect devices we know how to handle.
 */
static int
hifn_probe(device_t dev)
{
	if (pci_get_vendor(dev) == PCI_VENDOR_INVERTEX &&
	    pci_get_device(dev) == PCI_PRODUCT_INVERTEX_AEON)
		return (0);
	if (pci_get_vendor(dev) == PCI_VENDOR_HIFN &&
	    (pci_get_device(dev) == PCI_PRODUCT_HIFN_7751 ||
	     pci_get_device(dev) == PCI_PRODUCT_HIFN_7951 ||
	     pci_get_device(dev) == PCI_PRODUCT_HIFN_7811))
		return (0);
	if (pci_get_vendor(dev) == PCI_VENDOR_NETSEC &&
	    pci_get_device(dev) == PCI_PRODUCT_NETSEC_7751)
		return (0);
	return (ENXIO);
}

static void
hifn_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	bus_addr_t *paddr = (bus_addr_t*) arg;
	*paddr = segs->ds_addr;
}

static const char*
hifn_partname(struct hifn_softc *sc)
{
	/* XXX sprintf numbers when not decoded */
	switch (pci_get_vendor(sc->sc_dev)) {
	case PCI_VENDOR_HIFN:
		switch (pci_get_device(sc->sc_dev)) {
		case PCI_PRODUCT_HIFN_6500:	return "Hifn 6500";
		case PCI_PRODUCT_HIFN_7751:	return "Hifn 7751";
		case PCI_PRODUCT_HIFN_7811:	return "Hifn 7811";
		case PCI_PRODUCT_HIFN_7951:	return "Hifn 7951";
		}
		return "Hifn unknown-part";
	case PCI_VENDOR_INVERTEX:
		switch (pci_get_device(sc->sc_dev)) {
		case PCI_PRODUCT_INVERTEX_AEON:	return "Invertex AEON";
		}
		return "Invertex unknown-part";
	case PCI_VENDOR_NETSEC:
		switch (pci_get_device(sc->sc_dev)) {
		case PCI_PRODUCT_NETSEC_7751:	return "NetSec 7751";
		}
		return "NetSec unknown-part";
	}
	return "Unknown-vendor unknown-part";
}

/*
 * Attach an interface that successfully probed.
 */
static int 
hifn_attach(device_t dev)
{
	struct hifn_softc *sc = device_get_softc(dev);
	u_int32_t cmd;
	caddr_t kva;
	int rseg, rid;
	char rbase;
	u_int16_t ena, rev;

	KASSERT(sc != NULL, ("hifn_attach: null software carrier!"));
	bzero(sc, sizeof (*sc));
	sc->sc_dev = dev;

	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), "crypto driver", MTX_DEF);

	/* XXX handle power management */

	/*
	 * The 7951 has a random number generator and
	 * public key support; note this.
	 */
	if (pci_get_vendor(dev) == PCI_VENDOR_HIFN &&
	    pci_get_device(dev) == PCI_PRODUCT_HIFN_7951)
		sc->sc_flags = HIFN_HAS_RNG | HIFN_HAS_PUBLIC;
	/*
	 * The 7811 has a random number generator and
	 * we also note it's identity 'cuz of some quirks.
	 */
	if (pci_get_vendor(dev) == PCI_VENDOR_HIFN &&
	    pci_get_device(dev) == PCI_PRODUCT_HIFN_7811)
		sc->sc_flags |= HIFN_IS_7811 | HIFN_HAS_RNG;

	/*
	 * Configure support for memory-mapped access to
	 * registers and for DMA operations.
	 */
#define	PCIM_ENA	(PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN)
	cmd = pci_read_config(dev, PCIR_COMMAND, 4);
	cmd |= PCIM_ENA;
	pci_write_config(dev, PCIR_COMMAND, cmd, 4);
	cmd = pci_read_config(dev, PCIR_COMMAND, 4);
	if ((cmd & PCIM_ENA) != PCIM_ENA) {
		device_printf(dev, "failed to enable %s\n",
			(cmd & PCIM_ENA) == 0 ?
				"memory mapping & bus mastering" :
			(cmd & PCIM_CMD_MEMEN) == 0 ?
				"memory mapping" : "bus mastering");
		goto fail_pci;
	}
#undef PCIM_ENA

	/*
	 * Setup PCI resources. Note that we record the bus
	 * tag and handle for each register mapping, this is
	 * used by the READ_REG_0, WRITE_REG_0, READ_REG_1,
	 * and WRITE_REG_1 macros throughout the driver.
	 */
	rid = HIFN_BAR0;
	sc->sc_bar0res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
			 		    0, ~0, 1, RF_ACTIVE);
	if (sc->sc_bar0res == NULL) {
		device_printf(dev, "cannot map bar%d register space\n", 0);
		goto fail_pci;
	}
	sc->sc_st0 = rman_get_bustag(sc->sc_bar0res);
	sc->sc_sh0 = rman_get_bushandle(sc->sc_bar0res);
	sc->sc_bar0_lastreg = (bus_size_t) -1;

	rid = HIFN_BAR1;
	sc->sc_bar1res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
					    0, ~0, 1, RF_ACTIVE);
	if (sc->sc_bar1res == NULL) {
		device_printf(dev, "cannot map bar%d register space\n", 1);
		goto fail_io0;
	}
	sc->sc_st1 = rman_get_bustag(sc->sc_bar1res);
	sc->sc_sh1 = rman_get_bushandle(sc->sc_bar1res);
	sc->sc_bar1_lastreg = (bus_size_t) -1;

	hifn_set_retry(sc);

	/*
	 * Setup the area where the Hifn DMA's descriptors
	 * and associated data structures.
	 */
	if (bus_dma_tag_create(NULL,			/* parent */
			       1, 0,			/* alignment,boundary */
			       BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
			       BUS_SPACE_MAXADDR,	/* highaddr */
			       NULL, NULL,		/* filter, filterarg */
			       HIFN_MAX_DMALEN,		/* maxsize */
			       MAX_SCATTER,		/* nsegments */
			       HIFN_MAX_SEGLEN,		/* maxsegsize */
			       BUS_DMA_ALLOCNOW,	/* flags */
			       &sc->sc_dmat)) {
		device_printf(dev, "cannot allocate DMA tag\n");
		goto fail_io1;
	}
	if (bus_dmamap_create(sc->sc_dmat, BUS_DMA_NOWAIT, &sc->sc_dmamap)) {
		device_printf(dev, "cannot create dma map\n");
		bus_dma_tag_destroy(sc->sc_dmat);
		goto fail_io1;
	}
	if (bus_dmamem_alloc(sc->sc_dmat, (void**) &kva, BUS_DMA_NOWAIT, &sc->sc_dmamap)) {
		device_printf(dev, "cannot alloc dma buffer\n");
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_dmamap);
		bus_dma_tag_destroy(sc->sc_dmat);
		goto fail_io1;
	}
	if (bus_dmamap_load(sc->sc_dmat, sc->sc_dmamap, kva,
			     sizeof (*sc->sc_dma),
			     hifn_dmamap_cb, &sc->sc_dma_physaddr,
			     BUS_DMA_NOWAIT)) {
		device_printf(dev, "cannot load dma map\n");
		bus_dmamem_free(sc->sc_dmat, kva, sc->sc_dmamap);
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_dmamap);
		bus_dma_tag_destroy(sc->sc_dmat);
		goto fail_io1;
	}
	sc->sc_dma = (struct hifn_dma *)kva;
	bzero(sc->sc_dma, sizeof(*sc->sc_dma));

	KASSERT(sc->sc_st0 != NULL, ("hifn_attach: null bar0 tag!"));
	KASSERT(sc->sc_sh0 != NULL, ("hifn_attach: null bar0 handle!"));
	KASSERT(sc->sc_st1 != NULL, ("hifn_attach: null bar1 tag!"));
	KASSERT(sc->sc_sh1 != NULL, ("hifn_attach: null bar1 handle!"));

	/*
	 * Reset the board and do the ``secret handshake''
	 * to enable the crypto support.  Then complete the
	 * initialization procedure by setting up the interrupt
	 * and hooking in to the system crypto support so we'll
	 * get used for system services like the crypto device,
	 * IPsec, RNG device, etc.
	 */
	hifn_reset_board(sc, 0);

	if (hifn_enable_crypto(sc) != 0) {
		device_printf(dev, "crypto enabling failed\n");
		goto fail_mem;
	}
	hifn_reset_puc(sc);

	hifn_init_dma(sc);
	hifn_init_pci_registers(sc);

	if (hifn_ramtype(sc))
		goto fail_mem;

	if (sc->sc_drammodel == 0)
		hifn_sramsize(sc);
	else
		hifn_dramsize(sc);

	/*
	 * Workaround for NetSec 7751 rev A: half ram size because two
	 * of the address lines were left floating
	 */
	if (pci_get_vendor(dev) == PCI_VENDOR_NETSEC &&
	    pci_get_device(dev) == PCI_PRODUCT_NETSEC_7751 &&
	    pci_get_revid(dev) == 0x61)	/*XXX???*/
		sc->sc_ramsize >>= 1;

	/*
	 * Arrange the interrupt line.
	 */
	rid = 0;
	sc->sc_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid,
					0, ~0, 1, RF_SHAREABLE|RF_ACTIVE);
	if (sc->sc_irq == NULL) {
		device_printf(dev, "could not map interrupt\n");
		goto fail_mem;
	}
	/*
	 * NB: Network code assumes we are blocked with splimp()
	 *     so make sure the IRQ is marked appropriately.
	 */
	if (bus_setup_intr(dev, sc->sc_irq, INTR_TYPE_NET,
			   hifn_intr, sc, &sc->sc_intrhand)) {
		device_printf(dev, "could not setup interrupt\n");
		goto fail_intr2;
	}

	hifn_sessions(sc);

	/*
	 * NB: Keep only the low 16 bits; this masks the chip id
	 *     from the 7951.
	 */
	rev = READ_REG_1(sc, HIFN_1_REVID) & 0xffff;

	rseg = sc->sc_ramsize / 1024;
	rbase = 'K';
	if (sc->sc_ramsize >= (1024 * 1024)) {
		rbase = 'M';
		rseg /= 1024;
	}
	device_printf(sc->sc_dev, "%s, rev %u, %d%cB %cram, %u sessions\n",
		hifn_partname(sc), rev,
		rseg, rbase, sc->sc_drammodel ? 'd' : 's',
		sc->sc_maxses);

	sc->sc_cid = crypto_get_driverid(0);
	if (sc->sc_cid < 0) {
		device_printf(dev, "could not get crypto driver id\n");
		goto fail_intr;
	}

	WRITE_REG_0(sc, HIFN_0_PUCNFG,
	    READ_REG_0(sc, HIFN_0_PUCNFG) | HIFN_PUCNFG_CHIPID);
	ena = READ_REG_0(sc, HIFN_0_PUSTAT) & HIFN_PUSTAT_CHIPENA;

	switch (ena) {
	case HIFN_PUSTAT_ENA_2:
		crypto_register(sc->sc_cid, CRYPTO_3DES_CBC, 0, 0,
		    hifn_newsession, hifn_freesession, hifn_process, sc);
		crypto_register(sc->sc_cid, CRYPTO_ARC4, 0, 0,
		    hifn_newsession, hifn_freesession, hifn_process, sc);
		/*FALLTHROUGH*/
	case HIFN_PUSTAT_ENA_1:
		crypto_register(sc->sc_cid, CRYPTO_MD5, 0, 0,
		    hifn_newsession, hifn_freesession, hifn_process, sc);
		crypto_register(sc->sc_cid, CRYPTO_SHA1, 0, 0,
		    hifn_newsession, hifn_freesession, hifn_process, sc);
		crypto_register(sc->sc_cid, CRYPTO_MD5_HMAC, 0, 0,
		    hifn_newsession, hifn_freesession, hifn_process, sc);
		crypto_register(sc->sc_cid, CRYPTO_SHA1_HMAC, 0, 0,
		    hifn_newsession, hifn_freesession, hifn_process, sc);
		crypto_register(sc->sc_cid, CRYPTO_DES_CBC, 0, 0,
		    hifn_newsession, hifn_freesession, hifn_process, sc);
		break;
	}

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	if (sc->sc_flags & (HIFN_HAS_PUBLIC | HIFN_HAS_RNG))
		hifn_init_pubrng(sc);

	/* NB: 1 means the callout runs w/o Giant locked */
	callout_init(&sc->sc_tickto, 1);
	callout_reset(&sc->sc_tickto, hz, hifn_tick, sc);

	return (0);

fail_intr:
	bus_teardown_intr(dev, sc->sc_irq, sc->sc_intrhand);
fail_intr2:
	/* XXX don't store rid */
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq);
fail_mem:
	bus_dmamap_unload(sc->sc_dmat, sc->sc_dmamap);
	bus_dmamem_free(sc->sc_dmat, sc->sc_dma, sc->sc_dmamap);
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_dmamap);
	bus_dma_tag_destroy(sc->sc_dmat);

	/* Turn off DMA polling */
	WRITE_REG_1(sc, HIFN_1_DMA_CNFG, HIFN_DMACNFG_MSTRESET |
	    HIFN_DMACNFG_DMARESET | HIFN_DMACNFG_MODE);
fail_io1:
	bus_release_resource(dev, SYS_RES_MEMORY, HIFN_BAR1, sc->sc_bar1res);
fail_io0:
	bus_release_resource(dev, SYS_RES_MEMORY, HIFN_BAR0, sc->sc_bar0res);
fail_pci:
	mtx_destroy(&sc->sc_mtx);
	return (ENXIO);
}

/*
 * Detach an interface that successfully probed.
 */
static int 
hifn_detach(device_t dev)
{
	struct hifn_softc *sc = device_get_softc(dev);

	KASSERT(sc != NULL, ("hifn_detach: null software carrier!"));

	HIFN_LOCK(sc);

	/*XXX other resources */
	callout_stop(&sc->sc_tickto);
	callout_stop(&sc->sc_rngto);

	/* Turn off DMA polling */
	WRITE_REG_1(sc, HIFN_1_DMA_CNFG, HIFN_DMACNFG_MSTRESET |
	    HIFN_DMACNFG_DMARESET | HIFN_DMACNFG_MODE);

	crypto_unregister_all(sc->sc_cid);

	bus_generic_detach(dev);	/*XXX should be no children, right? */

	bus_teardown_intr(dev, sc->sc_irq, sc->sc_intrhand);
	/* XXX don't store rid */
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq);

	bus_dmamap_unload(sc->sc_dmat, sc->sc_dmamap);
	bus_dmamem_free(sc->sc_dmat, sc->sc_dma, sc->sc_dmamap);
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_dmamap);
	bus_dma_tag_destroy(sc->sc_dmat);

	bus_release_resource(dev, SYS_RES_MEMORY, HIFN_BAR1, sc->sc_bar1res);
	bus_release_resource(dev, SYS_RES_MEMORY, HIFN_BAR0, sc->sc_bar0res);

	HIFN_UNLOCK(sc);

	mtx_destroy(&sc->sc_mtx);

	return (0);
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void
hifn_shutdown(device_t dev)
{
#ifdef notyet
	hifn_stop(device_get_softc(dev));
#endif
}

/*
 * Device suspend routine.  Stop the interface and save some PCI
 * settings in case the BIOS doesn't restore them properly on
 * resume.
 */
static int
hifn_suspend(device_t dev)
{
	struct hifn_softc *sc = device_get_softc(dev);
#ifdef notyet
	int i;

	hifn_stop(sc);
	for (i = 0; i < 5; i++)
		sc->saved_maps[i] = pci_read_config(dev, PCIR_MAPS + i * 4, 4);
	sc->saved_biosaddr = pci_read_config(dev, PCIR_BIOS, 4);
	sc->saved_intline = pci_read_config(dev, PCIR_INTLINE, 1);
	sc->saved_cachelnsz = pci_read_config(dev, PCIR_CACHELNSZ, 1);
	sc->saved_lattimer = pci_read_config(dev, PCIR_LATTIMER, 1);
#endif
	sc->sc_suspended = 1;

	return (0);
}

/*
 * Device resume routine.  Restore some PCI settings in case the BIOS
 * doesn't, re-enable busmastering, and restart the interface if
 * appropriate.
 */
static int
hifn_resume(device_t dev)
{
	struct hifn_softc *sc = device_get_softc(dev);
#ifdef notyet
	int i;

	/* better way to do this? */
	for (i = 0; i < 5; i++)
		pci_write_config(dev, PCIR_MAPS + i * 4, sc->saved_maps[i], 4);
	pci_write_config(dev, PCIR_BIOS, sc->saved_biosaddr, 4);
	pci_write_config(dev, PCIR_INTLINE, sc->saved_intline, 1);
	pci_write_config(dev, PCIR_CACHELNSZ, sc->saved_cachelnsz, 1);
	pci_write_config(dev, PCIR_LATTIMER, sc->saved_lattimer, 1);

	/* reenable busmastering */
	pci_enable_busmaster(dev);
	pci_enable_io(dev, HIFN_RES);

        /* reinitialize interface if necessary */
        if (ifp->if_flags & IFF_UP)
                rl_init(sc);
#endif
	sc->sc_suspended = 0;

	return (0);
}

static int
hifn_init_pubrng(struct hifn_softc *sc)
{
	u_int32_t r;
	int i;

	if ((sc->sc_flags & HIFN_IS_7811) == 0) {
		/* Reset 7951 public key/rng engine */
		WRITE_REG_1(sc, HIFN_1_PUB_RESET,
		    READ_REG_1(sc, HIFN_1_PUB_RESET) | HIFN_PUBRST_RESET);

		for (i = 0; i < 100; i++) {
			DELAY(1000);
			if ((READ_REG_1(sc, HIFN_1_PUB_RESET) &
			    HIFN_PUBRST_RESET) == 0)
				break;
		}

		if (i == 100) {
			device_printf(sc->sc_dev, "public key init failed\n");
			return (1);
		}
	}

	/* Enable the rng, if available */
	if (sc->sc_flags & HIFN_HAS_RNG) {
		if (sc->sc_flags & HIFN_IS_7811) {
			r = READ_REG_1(sc, HIFN_1_7811_RNGENA);
			if (r & HIFN_7811_RNGENA_ENA) {
				r &= ~HIFN_7811_RNGENA_ENA;
				WRITE_REG_1(sc, HIFN_1_7811_RNGENA, r);
			}
			WRITE_REG_1(sc, HIFN_1_7811_RNGCFG,
			    HIFN_7811_RNGCFG_DEFL);
			r |= HIFN_7811_RNGENA_ENA;
			WRITE_REG_1(sc, HIFN_1_7811_RNGENA, r);
		} else
			WRITE_REG_1(sc, HIFN_1_RNG_CONFIG,
			    READ_REG_1(sc, HIFN_1_RNG_CONFIG) |
			    HIFN_RNGCFG_ENA);

		sc->sc_rngfirst = 1;
		if (hz >= 100)
			sc->sc_rnghz = hz / 100;
		else
			sc->sc_rnghz = 1;
		callout_init(&sc->sc_rngto, 0);
		callout_reset(&sc->sc_rngto, sc->sc_rnghz, hifn_rng, sc);
	}

	/* Enable public key engine, if available */
	if (sc->sc_flags & HIFN_HAS_PUBLIC) {
		WRITE_REG_1(sc, HIFN_1_PUB_IEN, HIFN_PUBIEN_DONE);
		sc->sc_dmaier |= HIFN_DMAIER_PUBDONE;
		WRITE_REG_1(sc, HIFN_1_DMA_IER, sc->sc_dmaier);
	}

	return (0);
}

static void
hifn_rng(void *vsc)
{
#define	RANDOM_BITS(n)	(n)*sizeof (u_int32_t), (n)*sizeof (u_int32_t)*NBBY, 0
	struct hifn_softc *sc = vsc;
	u_int32_t sts, num[2];
	int i;

	if (sc->sc_flags & HIFN_IS_7811) {
		for (i = 0; i < 5; i++) {
			sts = READ_REG_1(sc, HIFN_1_7811_RNGSTS);
			if (sts & HIFN_7811_RNGSTS_UFL) {
				device_printf(sc->sc_dev,
					      "RNG underflow: disabling\n");
				return;
			}
			if ((sts & HIFN_7811_RNGSTS_RDY) == 0)
				break;

			/*
			 * There are at least two words in the RNG FIFO
			 * at this point.
			 */
			num[0] = READ_REG_1(sc, HIFN_1_7811_RNGDAT);
			num[1] = READ_REG_1(sc, HIFN_1_7811_RNGDAT);
			/* NB: discard first data read */
			if (sc->sc_rngfirst)
				sc->sc_rngfirst = 0;
			else
				random_harvest(num, RANDOM_BITS(2), RANDOM_PURE);
		}
	} else {
		num[0] = READ_REG_1(sc, HIFN_1_RNG_DATA);

		/* NB: discard first data read */
		if (sc->sc_rngfirst)
			sc->sc_rngfirst = 0;
		else
			random_harvest(num, RANDOM_BITS(1), RANDOM_PURE);
	}

	callout_reset(&sc->sc_rngto, sc->sc_rnghz, hifn_rng, sc);
#undef RANDOM_BITS
}

static void
hifn_puc_wait(struct hifn_softc *sc)
{
	int i;

	for (i = 5000; i > 0; i--) {
		DELAY(1);
		if (!(READ_REG_0(sc, HIFN_0_PUCTRL) & HIFN_PUCTRL_RESET))
			break;
	}
	if (!i)
		device_printf(sc->sc_dev, "proc unit did not reset\n");
}

/*
 * Reset the processing unit.
 */
static void
hifn_reset_puc(struct hifn_softc *sc)
{
	/* Reset processing unit */
	WRITE_REG_0(sc, HIFN_0_PUCTRL, HIFN_PUCTRL_DMAENA);
	hifn_puc_wait(sc);
}

/*
 * Set the Retry and TRDY registers; note that we set them to
 * zero because the 7811 locks up when forced to retry (section
 * 3.6 of "Specification Update SU-0014-04".  Not clear if we
 * should do this for all Hifn parts, but it doesn't seem to hurt.
 */
static void
hifn_set_retry(struct hifn_softc *sc)
{
	/* NB: RETRY only responds to 8-bit reads/writes */
	pci_write_config(sc->sc_dev, HIFN_RETRY_TIMEOUT, 0, 1);
	pci_write_config(sc->sc_dev, HIFN_TRDY_TIMEOUT, 0, 4);
}

/*
 * Resets the board.  Values in the regesters are left as is
 * from the reset (i.e. initial values are assigned elsewhere).
 */
static void
hifn_reset_board(struct hifn_softc *sc, int full)
{
	u_int32_t reg;

	/*
	 * Set polling in the DMA configuration register to zero.  0x7 avoids
	 * resetting the board and zeros out the other fields.
	 */
	WRITE_REG_1(sc, HIFN_1_DMA_CNFG, HIFN_DMACNFG_MSTRESET |
	    HIFN_DMACNFG_DMARESET | HIFN_DMACNFG_MODE);

	/*
	 * Now that polling has been disabled, we have to wait 1 ms
	 * before resetting the board.
	 */
	DELAY(1000);

	/* Reset the DMA unit */
	if (full) {
		WRITE_REG_1(sc, HIFN_1_DMA_CNFG, HIFN_DMACNFG_MODE);
		DELAY(1000);
	} else {
		WRITE_REG_1(sc, HIFN_1_DMA_CNFG,
		    HIFN_DMACNFG_MODE | HIFN_DMACNFG_MSTRESET);
		hifn_reset_puc(sc);
	}

	KASSERT(sc->sc_dma != NULL, ("hifn_reset_board: null DMA tag!"));
	bzero(sc->sc_dma, sizeof(*sc->sc_dma));

	/* Bring dma unit out of reset */
	WRITE_REG_1(sc, HIFN_1_DMA_CNFG, HIFN_DMACNFG_MSTRESET |
	    HIFN_DMACNFG_DMARESET | HIFN_DMACNFG_MODE);

	hifn_puc_wait(sc);
	hifn_set_retry(sc);

	if (sc->sc_flags & HIFN_IS_7811) {
		for (reg = 0; reg < 1000; reg++) {
			if (READ_REG_1(sc, HIFN_1_7811_MIPSRST) &
			    HIFN_MIPSRST_CRAMINIT)
				break;
			DELAY(1000);
		}
		if (reg == 1000)
			printf(": cram init timeout\n");
	}
}

static u_int32_t
hifn_next_signature(u_int32_t a, u_int cnt)
{
	int i;
	u_int32_t v;

	for (i = 0; i < cnt; i++) {

		/* get the parity */
		v = a & 0x80080125;
		v ^= v >> 16;
		v ^= v >> 8;
		v ^= v >> 4;
		v ^= v >> 2;
		v ^= v >> 1;

		a = (v & 1) ^ (a << 1);
	}

	return a;
}

struct pci2id {
	u_short		pci_vendor;
	u_short		pci_prod;
	char		card_id[13];
};
static struct pci2id pci2id[] = {
	{
		PCI_VENDOR_HIFN,
		PCI_PRODUCT_HIFN_7951,
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00 }
	}, {
		PCI_VENDOR_NETSEC,
		PCI_PRODUCT_NETSEC_7751,
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00 }
	}, {
		PCI_VENDOR_INVERTEX,
		PCI_PRODUCT_INVERTEX_AEON,
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00 }
	}, {
		PCI_VENDOR_HIFN,
		PCI_PRODUCT_HIFN_7811,
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00 }
	}, {
		/*
		 * Other vendors share this PCI ID as well, such as
		 * http://www.powercrypt.com, and obviously they also
		 * use the same key.
		 */
		PCI_VENDOR_HIFN,
		PCI_PRODUCT_HIFN_7751,
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00 }
	},
};

/*
 * Checks to see if crypto is already enabled.  If crypto isn't enable,
 * "hifn_enable_crypto" is called to enable it.  The check is important,
 * as enabling crypto twice will lock the board.
 */
static int 
hifn_enable_crypto(struct hifn_softc *sc)
{
	u_int32_t dmacfg, ramcfg, encl, addr, i;
	char *offtbl = NULL;

	for (i = 0; i < sizeof(pci2id)/sizeof(pci2id[0]); i++) {
		if (pci2id[i].pci_vendor == pci_get_vendor(sc->sc_dev) &&
		    pci2id[i].pci_prod == pci_get_device(sc->sc_dev)) {
			offtbl = pci2id[i].card_id;
			break;
		}
	}
	if (offtbl == NULL) {
		device_printf(sc->sc_dev, "Unknown card!\n");
		return (1);
	}

	ramcfg = READ_REG_0(sc, HIFN_0_PUCNFG);
	dmacfg = READ_REG_1(sc, HIFN_1_DMA_CNFG);

	/*
	 * The RAM config register's encrypt level bit needs to be set before
	 * every read performed on the encryption level register.
	 */
	WRITE_REG_0(sc, HIFN_0_PUCNFG, ramcfg | HIFN_PUCNFG_CHIPID);

	encl = READ_REG_0(sc, HIFN_0_PUSTAT) & HIFN_PUSTAT_CHIPENA;

	/*
	 * Make sure we don't re-unlock.  Two unlocks kills chip until the
	 * next reboot.
	 */
	if (encl == HIFN_PUSTAT_ENA_1 || encl == HIFN_PUSTAT_ENA_2) {
#ifdef HIFN_DEBUG
		if (hifn_debug)
			device_printf(sc->sc_dev,
			    "Strong crypto already enabled!\n");
#endif
		goto report;
	}

	if (encl != 0 && encl != HIFN_PUSTAT_ENA_0) {
#ifdef HIFN_DEBUG
		if (hifn_debug)
			device_printf(sc->sc_dev,
			      "Unknown encryption level 0x%x\n", encl);
#endif
		return 1;
	}

	WRITE_REG_1(sc, HIFN_1_DMA_CNFG, HIFN_DMACNFG_UNLOCK |
	    HIFN_DMACNFG_MSTRESET | HIFN_DMACNFG_DMARESET | HIFN_DMACNFG_MODE);
	DELAY(1000);
	addr = READ_REG_1(sc, HIFN_UNLOCK_SECRET1);
	DELAY(1000);
	WRITE_REG_1(sc, HIFN_UNLOCK_SECRET2, 0);
	DELAY(1000);

	for (i = 0; i <= 12; i++) {
		addr = hifn_next_signature(addr, offtbl[i] + 0x101);
		WRITE_REG_1(sc, HIFN_UNLOCK_SECRET2, addr);

		DELAY(1000);
	}

	WRITE_REG_0(sc, HIFN_0_PUCNFG, ramcfg | HIFN_PUCNFG_CHIPID);
	encl = READ_REG_0(sc, HIFN_0_PUSTAT) & HIFN_PUSTAT_CHIPENA;

#ifdef HIFN_DEBUG
	if (hifn_debug) {
		if (encl != HIFN_PUSTAT_ENA_1 && encl != HIFN_PUSTAT_ENA_2)
			device_printf(sc->sc_dev, "Engine is permanently "
				"locked until next system reset!\n");
		else
			device_printf(sc->sc_dev, "Engine enabled "
				"successfully!\n");
	}
#endif

report:
	WRITE_REG_0(sc, HIFN_0_PUCNFG, ramcfg);
	WRITE_REG_1(sc, HIFN_1_DMA_CNFG, dmacfg);

	switch (encl) {
	case HIFN_PUSTAT_ENA_1:
	case HIFN_PUSTAT_ENA_2:
		break;
	case HIFN_PUSTAT_ENA_0:
	default:
		device_printf(sc->sc_dev, "disabled");
		break;
	}

	return 0;
}

/*
 * Give initial values to the registers listed in the "Register Space"
 * section of the HIFN Software Development reference manual.
 */
static void 
hifn_init_pci_registers(struct hifn_softc *sc)
{
	/* write fixed values needed by the Initialization registers */
	WRITE_REG_0(sc, HIFN_0_PUCTRL, HIFN_PUCTRL_DMAENA);
	WRITE_REG_0(sc, HIFN_0_FIFOCNFG, HIFN_FIFOCNFG_THRESHOLD);
	WRITE_REG_0(sc, HIFN_0_PUIER, HIFN_PUIER_DSTOVER);

	/* write all 4 ring address registers */
	WRITE_REG_1(sc, HIFN_1_DMA_CRAR, sc->sc_dma_physaddr +
	    offsetof(struct hifn_dma, cmdr[0]));
	WRITE_REG_1(sc, HIFN_1_DMA_SRAR, sc->sc_dma_physaddr +
	    offsetof(struct hifn_dma, srcr[0]));
	WRITE_REG_1(sc, HIFN_1_DMA_DRAR, sc->sc_dma_physaddr +
	    offsetof(struct hifn_dma, dstr[0]));
	WRITE_REG_1(sc, HIFN_1_DMA_RRAR, sc->sc_dma_physaddr +
	    offsetof(struct hifn_dma, resr[0]));

	DELAY(2000);

	/* write status register */
	WRITE_REG_1(sc, HIFN_1_DMA_CSR,
	    HIFN_DMACSR_D_CTRL_DIS | HIFN_DMACSR_R_CTRL_DIS |
	    HIFN_DMACSR_S_CTRL_DIS | HIFN_DMACSR_C_CTRL_DIS |
	    HIFN_DMACSR_D_ABORT | HIFN_DMACSR_D_DONE | HIFN_DMACSR_D_LAST |
	    HIFN_DMACSR_D_WAIT | HIFN_DMACSR_D_OVER |
	    HIFN_DMACSR_R_ABORT | HIFN_DMACSR_R_DONE | HIFN_DMACSR_R_LAST |
	    HIFN_DMACSR_R_WAIT | HIFN_DMACSR_R_OVER |
	    HIFN_DMACSR_S_ABORT | HIFN_DMACSR_S_DONE | HIFN_DMACSR_S_LAST |
	    HIFN_DMACSR_S_WAIT |
	    HIFN_DMACSR_C_ABORT | HIFN_DMACSR_C_DONE | HIFN_DMACSR_C_LAST |
	    HIFN_DMACSR_C_WAIT |
	    HIFN_DMACSR_ENGINE |
	    ((sc->sc_flags & HIFN_HAS_PUBLIC) ?
		HIFN_DMACSR_PUBDONE : 0) |
	    ((sc->sc_flags & HIFN_IS_7811) ?
		HIFN_DMACSR_ILLW | HIFN_DMACSR_ILLR : 0));

	sc->sc_d_busy = sc->sc_r_busy = sc->sc_s_busy = sc->sc_c_busy = 0;
	sc->sc_dmaier |= HIFN_DMAIER_R_DONE | HIFN_DMAIER_C_ABORT |
	    HIFN_DMAIER_D_OVER | HIFN_DMAIER_R_OVER |
	    HIFN_DMAIER_S_ABORT | HIFN_DMAIER_D_ABORT | HIFN_DMAIER_R_ABORT |
	    ((sc->sc_flags & HIFN_IS_7811) ?
		HIFN_DMAIER_ILLW | HIFN_DMAIER_ILLR : 0);
	sc->sc_dmaier &= ~HIFN_DMAIER_C_WAIT;
	WRITE_REG_1(sc, HIFN_1_DMA_IER, sc->sc_dmaier);

	WRITE_REG_0(sc, HIFN_0_PUCNFG, HIFN_PUCNFG_COMPSING |
	    HIFN_PUCNFG_DRFR_128 | HIFN_PUCNFG_TCALLPHASES |
	    HIFN_PUCNFG_TCDRVTOTEM | HIFN_PUCNFG_BUS32 |
	    (sc->sc_drammodel ? HIFN_PUCNFG_DRAM : HIFN_PUCNFG_SRAM));

	WRITE_REG_0(sc, HIFN_0_PUISR, HIFN_PUISR_DSTOVER);
	WRITE_REG_1(sc, HIFN_1_DMA_CNFG, HIFN_DMACNFG_MSTRESET |
	    HIFN_DMACNFG_DMARESET | HIFN_DMACNFG_MODE | HIFN_DMACNFG_LAST |
	    ((HIFN_POLL_FREQUENCY << 16 ) & HIFN_DMACNFG_POLLFREQ) |
	    ((HIFN_POLL_SCALAR << 8) & HIFN_DMACNFG_POLLINVAL));
}

/*
 * The maximum number of sessions supported by the card
 * is dependent on the amount of context ram, which
 * encryption algorithms are enabled, and how compression
 * is configured.  This should be configured before this
 * routine is called.
 */
static void
hifn_sessions(struct hifn_softc *sc)
{
	u_int32_t pucnfg;
	int ctxsize;

	pucnfg = READ_REG_0(sc, HIFN_0_PUCNFG);

	if (pucnfg & HIFN_PUCNFG_COMPSING) {
		if (pucnfg & HIFN_PUCNFG_ENCCNFG)
			ctxsize = 128;
		else
			ctxsize = 512;
		sc->sc_maxses = 1 +
		    ((sc->sc_ramsize - 32768) / ctxsize);
	} else
		sc->sc_maxses = sc->sc_ramsize / 16384;

	if (sc->sc_maxses > 2048)
		sc->sc_maxses = 2048;
}

/*
 * Determine ram type (sram or dram).  Board should be just out of a reset
 * state when this is called.
 */
static int
hifn_ramtype(struct hifn_softc *sc)
{
	u_int8_t data[8], dataexpect[8];
	int i;

	for (i = 0; i < sizeof(data); i++)
		data[i] = dataexpect[i] = 0x55;
	if (hifn_writeramaddr(sc, 0, data))
		return (-1);
	if (hifn_readramaddr(sc, 0, data))
		return (-1);
	if (bcmp(data, dataexpect, sizeof(data)) != 0) {
		sc->sc_drammodel = 1;
		return (0);
	}

	for (i = 0; i < sizeof(data); i++)
		data[i] = dataexpect[i] = 0xaa;
	if (hifn_writeramaddr(sc, 0, data))
		return (-1);
	if (hifn_readramaddr(sc, 0, data))
		return (-1);
	if (bcmp(data, dataexpect, sizeof(data)) != 0) {
		sc->sc_drammodel = 1;
		return (0);
	}

	return (0);
}

#define	HIFN_SRAM_MAX		(32 << 20)
#define	HIFN_SRAM_STEP_SIZE	16384
#define	HIFN_SRAM_GRANULARITY	(HIFN_SRAM_MAX / HIFN_SRAM_STEP_SIZE)

static int
hifn_sramsize(struct hifn_softc *sc)
{
	u_int32_t a;
	u_int8_t data[8];
	u_int8_t dataexpect[sizeof(data)];
	int32_t i;

	for (i = 0; i < sizeof(data); i++)
		data[i] = dataexpect[i] = i ^ 0x5a;

	for (i = HIFN_SRAM_GRANULARITY - 1; i >= 0; i--) {
		a = i * HIFN_SRAM_STEP_SIZE;
		bcopy(&i, data, sizeof(i));
		hifn_writeramaddr(sc, a, data);
	}

	for (i = 0; i < HIFN_SRAM_GRANULARITY; i++) {
		a = i * HIFN_SRAM_STEP_SIZE;
		bcopy(&i, dataexpect, sizeof(i));
		if (hifn_readramaddr(sc, a, data) < 0)
			return (0);
		if (bcmp(data, dataexpect, sizeof(data)) != 0)
			return (0);
		sc->sc_ramsize = a + HIFN_SRAM_STEP_SIZE;
	}

	return (0);
}

/*
 * XXX For dram boards, one should really try all of the
 * HIFN_PUCNFG_DSZ_*'s.  This just assumes that PUCNFG
 * is already set up correctly.
 */
static int
hifn_dramsize(struct hifn_softc *sc)
{
	u_int32_t cnfg;

	cnfg = READ_REG_0(sc, HIFN_0_PUCNFG) &
	    HIFN_PUCNFG_DRAMMASK;
	sc->sc_ramsize = 1 << ((cnfg >> 13) + 18);
	return (0);
}

static void
hifn_alloc_slot(struct hifn_softc *sc, int *cmdp, int *srcp, int *dstp, int *resp)
{
	struct hifn_dma *dma = sc->sc_dma;

	if (dma->cmdi == HIFN_D_CMD_RSIZE) {
		dma->cmdi = 0;
		dma->cmdr[HIFN_D_CMD_RSIZE].l = htole32(HIFN_D_VALID |
		    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
		HIFN_CMDR_SYNC(sc, HIFN_D_CMD_RSIZE,
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	}
	*cmdp = dma->cmdi++;
	dma->cmdk = dma->cmdi;

	if (dma->srci == HIFN_D_SRC_RSIZE) {
		dma->srci = 0;
		dma->srcr[HIFN_D_SRC_RSIZE].l = htole32(HIFN_D_VALID |
		    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
		HIFN_SRCR_SYNC(sc, HIFN_D_SRC_RSIZE,
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	}
	*srcp = dma->srci++;
	dma->srck = dma->srci;

	if (dma->dsti == HIFN_D_DST_RSIZE) {
		dma->dsti = 0;
		dma->dstr[HIFN_D_DST_RSIZE].l = htole32(HIFN_D_VALID |
		    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
		HIFN_DSTR_SYNC(sc, HIFN_D_DST_RSIZE,
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	}
	*dstp = dma->dsti++;
	dma->dstk = dma->dsti;

	if (dma->resi == HIFN_D_RES_RSIZE) {
		dma->resi = 0;
		dma->resr[HIFN_D_RES_RSIZE].l = htole32(HIFN_D_VALID |
		    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
		HIFN_RESR_SYNC(sc, HIFN_D_RES_RSIZE,
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	}
	*resp = dma->resi++;
	dma->resk = dma->resi;
}

static int
hifn_writeramaddr(struct hifn_softc *sc, int addr, u_int8_t *data)
{
	struct hifn_dma *dma = sc->sc_dma;
	hifn_base_command_t wc;
	const u_int32_t masks = HIFN_D_VALID | HIFN_D_LAST | HIFN_D_MASKDONEIRQ;
	int r, cmdi, resi, srci, dsti;

	wc.masks = htole16(3 << 13);
	wc.session_num = htole16(addr >> 14);
	wc.total_source_count = htole16(8);
	wc.total_dest_count = htole16(addr & 0x3fff);

	hifn_alloc_slot(sc, &cmdi, &srci, &dsti, &resi);

	WRITE_REG_1(sc, HIFN_1_DMA_CSR,
	    HIFN_DMACSR_C_CTRL_ENA | HIFN_DMACSR_S_CTRL_ENA |
	    HIFN_DMACSR_D_CTRL_ENA | HIFN_DMACSR_R_CTRL_ENA);

	/* build write command */
	bzero(dma->command_bufs[cmdi], HIFN_MAX_COMMAND);
	*(hifn_base_command_t *)dma->command_bufs[cmdi] = wc;
	bcopy(data, &dma->test_src, sizeof(dma->test_src));

	dma->srcr[srci].p = htole32(sc->sc_dma_physaddr
	    + offsetof(struct hifn_dma, test_src));
	dma->dstr[dsti].p = htole32(sc->sc_dma_physaddr
	    + offsetof(struct hifn_dma, test_dst));

	dma->cmdr[cmdi].l = htole32(16 | masks);
	dma->srcr[srci].l = htole32(8 | masks);
	dma->dstr[dsti].l = htole32(4 | masks);
	dma->resr[resi].l = htole32(4 | masks);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	for (r = 10000; r >= 0; r--) {
		DELAY(10);
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		if ((dma->resr[resi].l & htole32(HIFN_D_VALID)) == 0)
			break;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	}
	if (r == 0) {
		device_printf(sc->sc_dev, "writeramaddr -- "
		    "result[%d](addr %d) still valid\n", resi, addr);
		r = -1;
		return (-1);
	} else
		r = 0;

	WRITE_REG_1(sc, HIFN_1_DMA_CSR,
	    HIFN_DMACSR_C_CTRL_DIS | HIFN_DMACSR_S_CTRL_DIS |
	    HIFN_DMACSR_D_CTRL_DIS | HIFN_DMACSR_R_CTRL_DIS);

	return (r);
}

static int
hifn_readramaddr(struct hifn_softc *sc, int addr, u_int8_t *data)
{
	struct hifn_dma *dma = sc->sc_dma;
	hifn_base_command_t rc;
	const u_int32_t masks = HIFN_D_VALID | HIFN_D_LAST | HIFN_D_MASKDONEIRQ;
	int r, cmdi, srci, dsti, resi;

	rc.masks = htole16(2 << 13);
	rc.session_num = htole16(addr >> 14);
	rc.total_source_count = htole16(addr & 0x3fff);
	rc.total_dest_count = htole16(8);

	hifn_alloc_slot(sc, &cmdi, &srci, &dsti, &resi);

	WRITE_REG_1(sc, HIFN_1_DMA_CSR,
	    HIFN_DMACSR_C_CTRL_ENA | HIFN_DMACSR_S_CTRL_ENA |
	    HIFN_DMACSR_D_CTRL_ENA | HIFN_DMACSR_R_CTRL_ENA);

	bzero(dma->command_bufs[cmdi], HIFN_MAX_COMMAND);
	*(hifn_base_command_t *)dma->command_bufs[cmdi] = rc;

	dma->srcr[srci].p = htole32(sc->sc_dma_physaddr +
	    offsetof(struct hifn_dma, test_src));
	dma->test_src = 0;
	dma->dstr[dsti].p =  htole32(sc->sc_dma_physaddr +
	    offsetof(struct hifn_dma, test_dst));
	dma->test_dst = 0;
	dma->cmdr[cmdi].l = htole32(8 | masks);
	dma->srcr[srci].l = htole32(8 | masks);
	dma->dstr[dsti].l = htole32(8 | masks);
	dma->resr[resi].l = htole32(HIFN_MAX_RESULT | masks);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	for (r = 10000; r >= 0; r--) {
		DELAY(10);
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		if ((dma->resr[resi].l & htole32(HIFN_D_VALID)) == 0)
			break;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	}
	if (r == 0) {
		device_printf(sc->sc_dev, "readramaddr -- "
		    "result[%d](addr %d) still valid\n", resi, addr);
		r = -1;
	} else {
		r = 0;
		bcopy(&dma->test_dst, data, sizeof(dma->test_dst));
	}

	WRITE_REG_1(sc, HIFN_1_DMA_CSR,
	    HIFN_DMACSR_C_CTRL_DIS | HIFN_DMACSR_S_CTRL_DIS |
	    HIFN_DMACSR_D_CTRL_DIS | HIFN_DMACSR_R_CTRL_DIS);

	return (r);
}

/*
 * Initialize the descriptor rings.
 */
static void 
hifn_init_dma(struct hifn_softc *sc)
{
	struct hifn_dma *dma = sc->sc_dma;
	int i;

	hifn_set_retry(sc);

	/* initialize static pointer values */
	for (i = 0; i < HIFN_D_CMD_RSIZE; i++)
		dma->cmdr[i].p = htole32(sc->sc_dma_physaddr +
		    offsetof(struct hifn_dma, command_bufs[i][0]));
	for (i = 0; i < HIFN_D_RES_RSIZE; i++)
		dma->resr[i].p = htole32(sc->sc_dma_physaddr +
		    offsetof(struct hifn_dma, result_bufs[i][0]));

	dma->cmdr[HIFN_D_CMD_RSIZE].p =
	    htole32(sc->sc_dma_physaddr + offsetof(struct hifn_dma, cmdr[0]));
	dma->srcr[HIFN_D_SRC_RSIZE].p =
	    htole32(sc->sc_dma_physaddr + offsetof(struct hifn_dma, srcr[0]));
	dma->dstr[HIFN_D_DST_RSIZE].p =
	    htole32(sc->sc_dma_physaddr + offsetof(struct hifn_dma, dstr[0]));
	dma->resr[HIFN_D_RES_RSIZE].p =
	    htole32(sc->sc_dma_physaddr + offsetof(struct hifn_dma, resr[0]));

	dma->cmdu = dma->srcu = dma->dstu = dma->resu = 0;
	dma->cmdi = dma->srci = dma->dsti = dma->resi = 0;
	dma->cmdk = dma->srck = dma->dstk = dma->resk = 0;
}

/*
 * Writes out the raw command buffer space.  Returns the
 * command buffer size.
 */
static u_int
hifn_write_command(struct hifn_command *cmd, u_int8_t *buf)
{
#define	MIN(a,b)	((a)<(b)?(a):(b))
	u_int8_t *buf_pos;
	hifn_base_command_t *base_cmd;
	hifn_mac_command_t *mac_cmd;
	hifn_crypt_command_t *cry_cmd;
	int using_mac, using_crypt, len;
	u_int32_t dlen, slen;

	buf_pos = buf;
	using_mac = cmd->base_masks & HIFN_BASE_CMD_MAC;
	using_crypt = cmd->base_masks & HIFN_BASE_CMD_CRYPT;

	base_cmd = (hifn_base_command_t *)buf_pos;
	base_cmd->masks = htole16(cmd->base_masks);
	slen = cmd->src_mapsize;
	if (cmd->sloplen)
		dlen = cmd->dst_mapsize - cmd->sloplen + sizeof(u_int32_t);
	else
		dlen = cmd->dst_mapsize;
	base_cmd->total_source_count = htole16(slen & HIFN_BASE_CMD_LENMASK_LO);
	base_cmd->total_dest_count = htole16(dlen & HIFN_BASE_CMD_LENMASK_LO);
	dlen >>= 16;
	slen >>= 16;
	base_cmd->session_num = htole16(cmd->session_num |
	    ((slen << HIFN_BASE_CMD_SRCLEN_S) & HIFN_BASE_CMD_SRCLEN_M) |
	    ((dlen << HIFN_BASE_CMD_DSTLEN_S) & HIFN_BASE_CMD_DSTLEN_M));
	buf_pos += sizeof(hifn_base_command_t);

	if (using_mac) {
		mac_cmd = (hifn_mac_command_t *)buf_pos;
		dlen = cmd->maccrd->crd_len;
		mac_cmd->source_count = htole16(dlen & 0xffff);
		dlen >>= 16;
		mac_cmd->masks = htole16(cmd->mac_masks |
		    ((dlen << HIFN_MAC_CMD_SRCLEN_S) & HIFN_MAC_CMD_SRCLEN_M));
		mac_cmd->header_skip = htole16(cmd->maccrd->crd_skip);
		mac_cmd->reserved = 0;
		buf_pos += sizeof(hifn_mac_command_t);
	}

	if (using_crypt) {
		cry_cmd = (hifn_crypt_command_t *)buf_pos;
		dlen = cmd->enccrd->crd_len;
		cry_cmd->source_count = htole16(dlen & 0xffff);
		dlen >>= 16;
		cry_cmd->masks = htole16(cmd->cry_masks |
		    ((dlen << HIFN_CRYPT_CMD_SRCLEN_S) & HIFN_CRYPT_CMD_SRCLEN_M));
		cry_cmd->header_skip = htole16(cmd->enccrd->crd_skip);
		cry_cmd->reserved = 0;
		buf_pos += sizeof(hifn_crypt_command_t);
	}

	if (using_mac && cmd->mac_masks & HIFN_MAC_CMD_NEW_KEY) {
		bcopy(cmd->mac, buf_pos, HIFN_MAC_KEY_LENGTH);
		buf_pos += HIFN_MAC_KEY_LENGTH;
	}

	if (using_crypt && cmd->cry_masks & HIFN_CRYPT_CMD_NEW_KEY) {
		switch (cmd->cry_masks & HIFN_CRYPT_CMD_ALG_MASK) {
		case HIFN_CRYPT_CMD_ALG_3DES:
			bcopy(cmd->ck, buf_pos, HIFN_3DES_KEY_LENGTH);
			buf_pos += HIFN_3DES_KEY_LENGTH;
			break;
		case HIFN_CRYPT_CMD_ALG_DES:
			bcopy(cmd->ck, buf_pos, HIFN_DES_KEY_LENGTH);
			buf_pos += cmd->cklen;
			break;
		case HIFN_CRYPT_CMD_ALG_RC4:
			len = 256;
			do {
				int clen;

				clen = MIN(cmd->cklen, len);
				bcopy(cmd->ck, buf_pos, clen);
				len -= clen;
				buf_pos += clen;
			} while (len > 0);
			bzero(buf_pos, 4);
			buf_pos += 4;
			break;
		}
	}

	if (using_crypt && cmd->cry_masks & HIFN_CRYPT_CMD_NEW_IV) {
		bcopy(cmd->iv, buf_pos, HIFN_IV_LENGTH);
		buf_pos += HIFN_IV_LENGTH;
	}

	if ((cmd->base_masks & (HIFN_BASE_CMD_MAC|HIFN_BASE_CMD_CRYPT)) == 0) {
		bzero(buf_pos, 8);
		buf_pos += 8;
	}

	return (buf_pos - buf);
#undef	MIN
}

static int
hifn_dmamap_aligned(struct hifn_operand *op)
{
	int i;

	for (i = 0; i < op->nsegs; i++) {
		if (op->segs[i].ds_addr & 3)
			return (0);
		if ((i != (op->nsegs - 1)) && (op->segs[i].ds_len & 3))
			return (0);
	}
	return (1);
}

static int
hifn_dmamap_load_dst(struct hifn_softc *sc, struct hifn_command *cmd)
{
	struct hifn_dma *dma = sc->sc_dma;
	struct hifn_operand *dst = &cmd->dst;
	u_int32_t p, l;
	int idx, used = 0, i;

	idx = dma->dsti;
	for (i = 0; i < dst->nsegs - 1; i++) {
		dma->dstr[idx].p = htole32(dst->segs[i].ds_addr);
		dma->dstr[idx].l = htole32(HIFN_D_VALID |
		    HIFN_D_MASKDONEIRQ | dst->segs[i].ds_len);
		HIFN_DSTR_SYNC(sc, idx,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		used++;

		if (++idx == HIFN_D_DST_RSIZE) {
			dma->dstr[idx].l = htole32(HIFN_D_VALID |
			    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
			HIFN_DSTR_SYNC(sc, idx,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			idx = 0;
		}
	}

	if (cmd->sloplen == 0) {
		p = dst->segs[i].ds_addr;
		l = HIFN_D_VALID | HIFN_D_MASKDONEIRQ | HIFN_D_LAST |
		    dst->segs[i].ds_len;
	} else {
		p = sc->sc_dma_physaddr +
		    offsetof(struct hifn_dma, slop[cmd->slopidx]);
		l = HIFN_D_VALID | HIFN_D_MASKDONEIRQ | HIFN_D_LAST |
		    sizeof(u_int32_t);

		if ((dst->segs[i].ds_len - cmd->sloplen) != 0) {
			dma->dstr[idx].p = htole32(dst->segs[i].ds_addr);
			dma->dstr[idx].l = htole32(HIFN_D_VALID |
			    HIFN_D_MASKDONEIRQ |
			    (dst->segs[i].ds_len - cmd->sloplen));
			HIFN_DSTR_SYNC(sc, idx,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			used++;

			if (++idx == HIFN_D_DST_RSIZE) {
				dma->dstr[idx].l = htole32(HIFN_D_VALID |
				    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
				HIFN_DSTR_SYNC(sc, idx,
				    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
				idx = 0;
			}
		}
	}
	dma->dstr[idx].p = htole32(p);
	dma->dstr[idx].l = htole32(l);
	HIFN_DSTR_SYNC(sc, idx, BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	used++;

	if (++idx == HIFN_D_DST_RSIZE) {
		dma->dstr[idx].l = htole32(HIFN_D_VALID | HIFN_D_JUMP |
		    HIFN_D_MASKDONEIRQ);
		HIFN_DSTR_SYNC(sc, idx,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		idx = 0;
	}

	dma->dsti = idx;
	dma->dstu += used;
	return (idx);
}

static int
hifn_dmamap_load_src(struct hifn_softc *sc, struct hifn_command *cmd)
{
	struct hifn_dma *dma = sc->sc_dma;
	struct hifn_operand *src = &cmd->src;
	int idx, i;
	u_int32_t last = 0;

	idx = dma->srci;
	for (i = 0; i < src->nsegs; i++) {
		if (i == src->nsegs - 1)
			last = HIFN_D_LAST;

		dma->srcr[idx].p = htole32(src->segs[i].ds_addr);
		dma->srcr[idx].l = htole32(src->segs[i].ds_len |
		    HIFN_D_VALID | HIFN_D_MASKDONEIRQ | last);
		HIFN_SRCR_SYNC(sc, idx,
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

		if (++idx == HIFN_D_SRC_RSIZE) {
			dma->srcr[idx].l = htole32(HIFN_D_VALID |
			    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
			HIFN_SRCR_SYNC(sc, HIFN_D_SRC_RSIZE,
			    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
			idx = 0;
		}
	}
	dma->srci = idx;
	dma->srcu += src->nsegs;
	return (idx);
} 

static void
hifn_op_cb(void* arg, bus_dma_segment_t *seg, int nsegs, bus_size_t mapsize, int error)
{
	struct hifn_operand *op = arg;

	KASSERT(nsegs <= MAX_SCATTER,
		("hifn_op_cb: too many DMA segments (%u > %u) "
		 "returned when mapping operand", nsegs, MAX_SCATTER));
	op->mapsize = mapsize;
	op->nsegs = nsegs;
	bcopy(seg, op->segs, nsegs * sizeof (seg[0]));
}

static int 
hifn_crypto(
	struct hifn_softc *sc,
	struct hifn_command *cmd,
	struct cryptop *crp,
	int hint)
{
	struct	hifn_dma *dma = sc->sc_dma;
	u_int32_t cmdlen;
	int cmdi, resi, err = 0;

	/*
	 * need 1 cmd, and 1 res
	 *
	 * NB: check this first since it's easy.
	 */
	if ((dma->cmdu + 1) > HIFN_D_CMD_RSIZE ||
	    (dma->resu + 1) > HIFN_D_RES_RSIZE) {
#ifdef HIFN_DEBUG
		if (hifn_debug) {
			device_printf(sc->sc_dev,
				"cmd/result exhaustion, cmdu %u resu %u\n",
				dma->cmdu, dma->resu);
		}
#endif
		hifnstats.hst_nomem_cr++;
		return (ERESTART);
	}

	if (bus_dmamap_create(sc->sc_dmat, BUS_DMA_NOWAIT, &cmd->src_map)) {
		hifnstats.hst_nomem_map++;
		return (ENOMEM);
	}

	if (crp->crp_flags & CRYPTO_F_IMBUF) {
		if (bus_dmamap_load_mbuf(sc->sc_dmat, cmd->src_map,
		    cmd->src_m, hifn_op_cb, &cmd->src, BUS_DMA_NOWAIT)) {
			hifnstats.hst_nomem_load++;
			err = ENOMEM;
			goto err_srcmap1;
		}
	} else if (crp->crp_flags & CRYPTO_F_IOV) {
		if (bus_dmamap_load_uio(sc->sc_dmat, cmd->src_map,
		    cmd->src_io, hifn_op_cb, &cmd->src, BUS_DMA_NOWAIT)) {
			hifnstats.hst_nomem_load++;
			err = ENOMEM;
			goto err_srcmap1;
		}
	} else {
		err = EINVAL;
		goto err_srcmap1;
	}

	if (hifn_dmamap_aligned(&cmd->src)) {
		cmd->sloplen = cmd->src_mapsize & 3;
		cmd->dst = cmd->src;
	} else {
		if (crp->crp_flags & CRYPTO_F_IOV) {
			err = EINVAL;
			goto err_srcmap;
		} else if (crp->crp_flags & CRYPTO_F_IMBUF) {
			int totlen, len;
			struct mbuf *m, *m0, *mlast;

			KASSERT(cmd->dst_m == cmd->src_m,
				("hifn_crypto: dst_m initialized improperly"));
			hifnstats.hst_unaligned++;
			/*
			 * Source is not aligned on a longword boundary.
			 * Copy the data to insure alignment.  If we fail
			 * to allocate mbufs or clusters while doing this
			 * we return ERESTART so the operation is requeued
			 * at the crypto later, but only if there are
			 * ops already posted to the hardware; otherwise we
			 * have no guarantee that we'll be re-entered.
			 */
			totlen = cmd->src_mapsize;
			if (cmd->src_m->m_flags & M_PKTHDR) {
				len = MHLEN;
				MGETHDR(m0, M_DONTWAIT, MT_DATA);
			} else {
				len = MLEN;
				MGET(m0, M_DONTWAIT, MT_DATA);
			}
			if (m0 == NULL) {
				hifnstats.hst_nomem_mbuf++;
				err = dma->cmdu ? ERESTART : ENOMEM;
				goto err_srcmap;
			}
			if (len == MHLEN) {
				M_COPY_PKTHDR(m0, cmd->src_m);
			}
			if (totlen >= MINCLSIZE) {
				MCLGET(m0, M_DONTWAIT);
				if ((m0->m_flags & M_EXT) == 0) {
					hifnstats.hst_nomem_mcl++;
					err = dma->cmdu ? ERESTART : ENOMEM;
					m_freem(m0);
					goto err_srcmap;
				}
				len = MCLBYTES;
			}
			totlen -= len;
			m0->m_pkthdr.len = m0->m_len = len;
			mlast = m0;

			while (totlen > 0) {
				MGET(m, M_DONTWAIT, MT_DATA);
				if (m == NULL) {
					hifnstats.hst_nomem_mbuf++;
					err = dma->cmdu ? ERESTART : ENOMEM;
					m_freem(m0);
					goto err_srcmap;
				}
				len = MLEN;
				if (totlen >= MINCLSIZE) {
					MCLGET(m, M_DONTWAIT);
					if ((m->m_flags & M_EXT) == 0) {
						hifnstats.hst_nomem_mcl++;
						err = dma->cmdu ? ERESTART : ENOMEM;
						mlast->m_next = m;
						m_freem(m0);
						goto err_srcmap;
					}
					len = MCLBYTES;
				}

				m->m_len = len;
				m0->m_pkthdr.len += len;
				totlen -= len;

				mlast->m_next = m;
				mlast = m;
			}
			cmd->dst_m = m0;
		}
	}

	if (cmd->dst_map == NULL) {
		if (bus_dmamap_create(sc->sc_dmat, BUS_DMA_NOWAIT, &cmd->dst_map)) {
			hifnstats.hst_nomem_map++;
			err = ENOMEM;
			goto err_srcmap;
		}
		if (crp->crp_flags & CRYPTO_F_IMBUF) {
			if (bus_dmamap_load_mbuf(sc->sc_dmat, cmd->dst_map,
			    cmd->dst_m, hifn_op_cb, &cmd->dst, BUS_DMA_NOWAIT)) {
				hifnstats.hst_nomem_map++;
				err = ENOMEM;
				goto err_dstmap1;
			}
		} else if (crp->crp_flags & CRYPTO_F_IOV) {
			if (bus_dmamap_load_uio(sc->sc_dmat, cmd->dst_map,
			    cmd->dst_io, hifn_op_cb, &cmd->dst, BUS_DMA_NOWAIT)) {
				hifnstats.hst_nomem_load++;
				err = ENOMEM;
				goto err_dstmap1;
			}
		}
	}

#ifdef HIFN_DEBUG
	if (hifn_debug) {
		device_printf(sc->sc_dev,
		    "Entering cmd: stat %8x ien %8x u %d/%d/%d/%d n %d/%d\n",
		    READ_REG_1(sc, HIFN_1_DMA_CSR),
		    READ_REG_1(sc, HIFN_1_DMA_IER),
		    dma->cmdu, dma->srcu, dma->dstu, dma->resu,
		    cmd->src_nsegs, cmd->dst_nsegs);
	}
#endif

	if (cmd->src_map == cmd->dst_map) {
		bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
		    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);
	} else {
		bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
		    BUS_DMASYNC_PREWRITE);
		bus_dmamap_sync(sc->sc_dmat, cmd->dst_map,
		    BUS_DMASYNC_PREREAD);
	}

	/*
	 * need N src, and N dst
	 */
	if ((dma->srcu + cmd->src_nsegs) > HIFN_D_SRC_RSIZE ||
	    (dma->dstu + cmd->dst_nsegs + 1) > HIFN_D_DST_RSIZE) {
#ifdef HIFN_DEBUG
		if (hifn_debug) {
			device_printf(sc->sc_dev,
				"src/dst exhaustion, srcu %u+%u dstu %u+%u\n",
				dma->srcu, cmd->src_nsegs,
				dma->dstu, cmd->dst_nsegs);
		}
#endif
		hifnstats.hst_nomem_sd++;
		err = ERESTART;
		goto err_dstmap;
	}

	if (dma->cmdi == HIFN_D_CMD_RSIZE) {
		dma->cmdi = 0;
		dma->cmdr[HIFN_D_CMD_RSIZE].l = htole32(HIFN_D_VALID |
		    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
		HIFN_CMDR_SYNC(sc, HIFN_D_CMD_RSIZE,
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	}
	cmdi = dma->cmdi++;
	cmdlen = hifn_write_command(cmd, dma->command_bufs[cmdi]);
	HIFN_CMD_SYNC(sc, cmdi, BUS_DMASYNC_PREWRITE);

	/* .p for command/result already set */
	dma->cmdr[cmdi].l = htole32(cmdlen | HIFN_D_VALID | HIFN_D_LAST |
	    HIFN_D_MASKDONEIRQ);
	HIFN_CMDR_SYNC(sc, cmdi,
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	dma->cmdu++;
	if (sc->sc_c_busy == 0) {
		WRITE_REG_1(sc, HIFN_1_DMA_CSR, HIFN_DMACSR_C_CTRL_ENA);
		sc->sc_c_busy = 1;
	}

	/*
	 * We don't worry about missing an interrupt (which a "command wait"
	 * interrupt salvages us from), unless there is more than one command
	 * in the queue.
	 */
	if (dma->cmdu > 1) {
		sc->sc_dmaier |= HIFN_DMAIER_C_WAIT;
		WRITE_REG_1(sc, HIFN_1_DMA_IER, sc->sc_dmaier);
	}

	hifnstats.hst_ipackets++;
	hifnstats.hst_ibytes += cmd->src_mapsize;

	hifn_dmamap_load_src(sc, cmd);
	if (sc->sc_s_busy == 0) {
		WRITE_REG_1(sc, HIFN_1_DMA_CSR, HIFN_DMACSR_S_CTRL_ENA);
		sc->sc_s_busy = 1;
	}

	/*
	 * Unlike other descriptors, we don't mask done interrupt from
	 * result descriptor.
	 */
#ifdef HIFN_DEBUG
	if (hifn_debug)
		printf("load res\n");
#endif
	if (dma->resi == HIFN_D_RES_RSIZE) {
		dma->resi = 0;
		dma->resr[HIFN_D_RES_RSIZE].l = htole32(HIFN_D_VALID |
		    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
		HIFN_RESR_SYNC(sc, HIFN_D_RES_RSIZE,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	}
	resi = dma->resi++;
	KASSERT(dma->hifn_commands[resi] == NULL,
		("hifn_crypto: command slot %u busy", resi));
	dma->hifn_commands[resi] = cmd;
	HIFN_RES_SYNC(sc, resi, BUS_DMASYNC_PREREAD);
	if ((hint & CRYPTO_HINT_MORE) && sc->sc_curbatch < hifn_maxbatch) {
		dma->resr[resi].l = htole32(HIFN_MAX_RESULT |
		    HIFN_D_VALID | HIFN_D_LAST | HIFN_D_MASKDONEIRQ);
		sc->sc_curbatch++;
		if (sc->sc_curbatch > hifnstats.hst_maxbatch)
			hifnstats.hst_maxbatch = sc->sc_curbatch;
		hifnstats.hst_totbatch++;
	} else {
		dma->resr[resi].l = htole32(HIFN_MAX_RESULT |
		    HIFN_D_VALID | HIFN_D_LAST);
		sc->sc_curbatch = 0;
	}
	HIFN_RESR_SYNC(sc, resi,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	dma->resu++;
	if (sc->sc_r_busy == 0) {
		WRITE_REG_1(sc, HIFN_1_DMA_CSR, HIFN_DMACSR_R_CTRL_ENA);
		sc->sc_r_busy = 1;
	}

	if (cmd->sloplen)
		cmd->slopidx = resi;

	hifn_dmamap_load_dst(sc, cmd);

	if (sc->sc_d_busy == 0) {
		WRITE_REG_1(sc, HIFN_1_DMA_CSR, HIFN_DMACSR_D_CTRL_ENA);
		sc->sc_d_busy = 1;
	}

#ifdef HIFN_DEBUG
	if (hifn_debug) {
		device_printf(sc->sc_dev, "command: stat %8x ier %8x\n",
		    READ_REG_1(sc, HIFN_1_DMA_CSR),
		    READ_REG_1(sc, HIFN_1_DMA_IER));
	}
#endif

	sc->sc_active = 5;
	KASSERT(err == 0, ("hifn_crypto: success with error %u", err));
	return (err);		/* success */

err_dstmap:
	if (cmd->src_map != cmd->dst_map)
		bus_dmamap_unload(sc->sc_dmat, cmd->dst_map);
err_dstmap1:
	if (cmd->src_map != cmd->dst_map)
		bus_dmamap_destroy(sc->sc_dmat, cmd->dst_map);
err_srcmap:
	if (crp->crp_flags & CRYPTO_F_IMBUF) {
		if (cmd->src_m != cmd->dst_m)
			m_freem(cmd->dst_m);
	}
	bus_dmamap_unload(sc->sc_dmat, cmd->src_map);
err_srcmap1:
	bus_dmamap_destroy(sc->sc_dmat, cmd->src_map);
	return (err);
}

static void
hifn_tick(void* vsc)
{
	struct hifn_softc *sc = vsc;

	HIFN_LOCK(sc);
	if (sc->sc_active == 0) {
		struct hifn_dma *dma = sc->sc_dma;
		u_int32_t r = 0;

		if (dma->cmdu == 0 && sc->sc_c_busy) {
			sc->sc_c_busy = 0;
			r |= HIFN_DMACSR_C_CTRL_DIS;
		}
		if (dma->srcu == 0 && sc->sc_s_busy) {
			sc->sc_s_busy = 0;
			r |= HIFN_DMACSR_S_CTRL_DIS;
		}
		if (dma->dstu == 0 && sc->sc_d_busy) {
			sc->sc_d_busy = 0;
			r |= HIFN_DMACSR_D_CTRL_DIS;
		}
		if (dma->resu == 0 && sc->sc_r_busy) {
			sc->sc_r_busy = 0;
			r |= HIFN_DMACSR_R_CTRL_DIS;
		}
		if (r)
			WRITE_REG_1(sc, HIFN_1_DMA_CSR, r);
	} else
		sc->sc_active--;
	HIFN_UNLOCK(sc);
	callout_reset(&sc->sc_tickto, hz, hifn_tick, sc);
}

static void 
hifn_intr(void *arg)
{
	struct hifn_softc *sc = arg;
	struct hifn_dma *dma;
	u_int32_t dmacsr, restart;
	int i, u;

	HIFN_LOCK(sc);
	dma = sc->sc_dma;

	dmacsr = READ_REG_1(sc, HIFN_1_DMA_CSR);

#ifdef HIFN_DEBUG
	if (hifn_debug) {
		device_printf(sc->sc_dev,
		    "irq: stat %08x ien %08x damier %08x i %d/%d/%d/%d k %d/%d/%d/%d u %d/%d/%d/%d\n",
		    dmacsr, READ_REG_1(sc, HIFN_1_DMA_IER), sc->sc_dmaier,
		    dma->cmdi, dma->srci, dma->dsti, dma->resi,
		    dma->cmdk, dma->srck, dma->dstk, dma->resk,
		    dma->cmdu, dma->srcu, dma->dstu, dma->resu);
	}
#endif

	/* Nothing in the DMA unit interrupted */
	if ((dmacsr & sc->sc_dmaier) == 0) {
		hifnstats.hst_noirq++;
		HIFN_UNLOCK(sc);
		return;
	}

	WRITE_REG_1(sc, HIFN_1_DMA_CSR, dmacsr & sc->sc_dmaier);

	if ((sc->sc_flags & HIFN_HAS_PUBLIC) &&
	    (dmacsr & HIFN_DMACSR_PUBDONE))
		WRITE_REG_1(sc, HIFN_1_PUB_STATUS,
		    READ_REG_1(sc, HIFN_1_PUB_STATUS) | HIFN_PUBSTS_DONE);

	restart = dmacsr & (HIFN_DMACSR_D_OVER | HIFN_DMACSR_R_OVER);
	if (restart)
		device_printf(sc->sc_dev, "overrun %x\n", dmacsr);

	if (sc->sc_flags & HIFN_IS_7811) {
		if (dmacsr & HIFN_DMACSR_ILLR)
			device_printf(sc->sc_dev, "illegal read\n");
		if (dmacsr & HIFN_DMACSR_ILLW)
			device_printf(sc->sc_dev, "illegal write\n");
	}

	restart = dmacsr & (HIFN_DMACSR_C_ABORT | HIFN_DMACSR_S_ABORT |
	    HIFN_DMACSR_D_ABORT | HIFN_DMACSR_R_ABORT);
	if (restart) {
		device_printf(sc->sc_dev, "abort, resetting.\n");
		hifnstats.hst_abort++;
		hifn_abort(sc);
		HIFN_UNLOCK(sc);
		return;
	}

	if ((dmacsr & HIFN_DMACSR_C_WAIT) && (dma->cmdu == 0)) {
		/*
		 * If no slots to process and we receive a "waiting on
		 * command" interrupt, we disable the "waiting on command"
		 * (by clearing it).
		 */
		sc->sc_dmaier &= ~HIFN_DMAIER_C_WAIT;
		WRITE_REG_1(sc, HIFN_1_DMA_IER, sc->sc_dmaier);
	}

	/* clear the rings */
	i = dma->resk; u = dma->resu;
	while (u != 0) {
		HIFN_RESR_SYNC(sc, i,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		if (dma->resr[i].l & htole32(HIFN_D_VALID)) {
			HIFN_RESR_SYNC(sc, i,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			break;
		}

		if (i != HIFN_D_RES_RSIZE) {
			struct hifn_command *cmd;
			u_int8_t *macbuf = NULL;

			HIFN_RES_SYNC(sc, i, BUS_DMASYNC_POSTREAD);
			cmd = dma->hifn_commands[i];
			KASSERT(cmd != NULL,
				("hifn_intr: null command slot %u", i));
			dma->hifn_commands[i] = NULL;

			if (cmd->base_masks & HIFN_BASE_CMD_MAC) {
				macbuf = dma->result_bufs[i];
				macbuf += 12;
			}

			hifn_callback(sc, cmd, macbuf);
			hifnstats.hst_opackets++;
			u--;
		}

		if (++i == (HIFN_D_RES_RSIZE + 1))
			i = 0;
	}
	dma->resk = i; dma->resu = u;

	i = dma->srck; u = dma->srcu;
	while (u != 0) {
		if (i == HIFN_D_SRC_RSIZE)
			i = 0;
		HIFN_SRCR_SYNC(sc, i,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		if (dma->srcr[i].l & htole32(HIFN_D_VALID)) {
			HIFN_SRCR_SYNC(sc, i,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			break;
		}
		i++, u--;
	}
	dma->srck = i; dma->srcu = u;

	i = dma->cmdk; u = dma->cmdu;
	while (u != 0) {
		HIFN_CMDR_SYNC(sc, i,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		if (dma->cmdr[i].l & htole32(HIFN_D_VALID)) {
			HIFN_CMDR_SYNC(sc, i,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			break;
		}
		if (i != HIFN_D_CMD_RSIZE) {
			u--;
			HIFN_CMD_SYNC(sc, i, BUS_DMASYNC_POSTWRITE);
		}
		if (++i == (HIFN_D_CMD_RSIZE + 1))
			i = 0;
	}
	dma->cmdk = i; dma->cmdu = u;

	if (sc->sc_needwakeup) {		/* XXX check high watermark */
		int wakeup = sc->sc_needwakeup & (CRYPTO_SYMQ|CRYPTO_ASYMQ);
#ifdef HIFN_DEBUG
		if (hifn_debug)
			device_printf(sc->sc_dev,
				"wakeup crypto (%x) u %d/%d/%d/%d\n",
				sc->sc_needwakeup,
				dma->cmdu, dma->srcu, dma->dstu, dma->resu);
#endif
		sc->sc_needwakeup &= ~wakeup;
		crypto_unblock(sc->sc_cid, wakeup);
	}
	HIFN_UNLOCK(sc);
}

/*
 * Allocate a new 'session' and return an encoded session id.  'sidp'
 * contains our registration id, and should contain an encoded session
 * id on successful allocation.
 */
static int
hifn_newsession(void *arg, u_int32_t *sidp, struct cryptoini *cri)
{
	struct cryptoini *c;
	struct hifn_softc *sc = arg;
	int i, mac = 0, cry = 0;

	KASSERT(sc != NULL, ("hifn_newsession: null softc"));
	if (sidp == NULL || cri == NULL || sc == NULL)
		return (EINVAL);

	for (i = 0; i < sc->sc_maxses; i++)
		if (sc->sc_sessions[i].hs_state == HS_STATE_FREE)
			break;
	if (i == sc->sc_maxses)
		return (ENOMEM);

	for (c = cri; c != NULL; c = c->cri_next) {
		switch (c->cri_alg) {
		case CRYPTO_MD5:
		case CRYPTO_SHA1:
		case CRYPTO_MD5_HMAC:
		case CRYPTO_SHA1_HMAC:
			if (mac)
				return (EINVAL);
			mac = 1;
			break;
		case CRYPTO_DES_CBC:
		case CRYPTO_3DES_CBC:
			/* XXX this may read fewer, does it matter? */
			read_random(sc->sc_sessions[i].hs_iv, HIFN_IV_LENGTH);
			/*FALLTHROUGH*/
		case CRYPTO_ARC4:
			if (cry)
				return (EINVAL);
			cry = 1;
			break;
		default:
			return (EINVAL);
		}
	}
	if (mac == 0 && cry == 0)
		return (EINVAL);

	*sidp = HIFN_SID(device_get_unit(sc->sc_dev), i);
	sc->sc_sessions[i].hs_state = HS_STATE_USED;

	return (0);
}

/*
 * Deallocate a session.
 * XXX this routine should run a zero'd mac/encrypt key into context ram.
 * XXX to blow away any keys already stored there.
 */
static int
hifn_freesession(void *arg, u_int64_t tid)
{
	struct hifn_softc *sc = arg;
	int session;
	u_int32_t sid = ((u_int32_t) tid) & 0xffffffff;

	KASSERT(sc != NULL, ("hifn_freesession: null softc"));
	if (sc == NULL)
		return (EINVAL);

	session = HIFN_SESSION(sid);
	if (session >= sc->sc_maxses)
		return (EINVAL);

	bzero(&sc->sc_sessions[session], sizeof(sc->sc_sessions[session]));
	return (0);
}

static int
hifn_process(void *arg, struct cryptop *crp, int hint)
{
	struct hifn_softc *sc = arg;
	struct hifn_command *cmd = NULL;
	int session, err;
	struct cryptodesc *crd1, *crd2, *maccrd, *enccrd;

	if (crp == NULL || crp->crp_callback == NULL) {
		hifnstats.hst_invalid++;
		return (EINVAL);
	}
	session = HIFN_SESSION(crp->crp_sid);

	if (sc == NULL || session >= sc->sc_maxses) {
		err = EINVAL;
		goto errout;
	}

	cmd = malloc(sizeof(struct hifn_command), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (cmd == NULL) {
		hifnstats.hst_nomem++;
		err = ENOMEM;
		goto errout;
	}

	if (crp->crp_flags & CRYPTO_F_IMBUF) {
		cmd->src_m = (struct mbuf *)crp->crp_buf;
		cmd->dst_m = (struct mbuf *)crp->crp_buf;
	} else if (crp->crp_flags & CRYPTO_F_IOV) {
		cmd->src_io = (struct uio *)crp->crp_buf;
		cmd->dst_io = (struct uio *)crp->crp_buf;
	} else {
		err = EINVAL;
		goto errout;	/* XXX we don't handle contiguous buffers! */
	}

	crd1 = crp->crp_desc;
	if (crd1 == NULL) {
		err = EINVAL;
		goto errout;
	}
	crd2 = crd1->crd_next;

	if (crd2 == NULL) {
		if (crd1->crd_alg == CRYPTO_MD5_HMAC ||
		    crd1->crd_alg == CRYPTO_SHA1_HMAC ||
		    crd1->crd_alg == CRYPTO_SHA1 ||
		    crd1->crd_alg == CRYPTO_MD5) {
			maccrd = crd1;
			enccrd = NULL;
		} else if (crd1->crd_alg == CRYPTO_DES_CBC ||
		    crd1->crd_alg == CRYPTO_3DES_CBC ||
		    crd1->crd_alg == CRYPTO_ARC4) {
			if ((crd1->crd_flags & CRD_F_ENCRYPT) == 0)
				cmd->base_masks |= HIFN_BASE_CMD_DECODE;
			maccrd = NULL;
			enccrd = crd1;
		} else {
			err = EINVAL;
			goto errout;
		}
	} else {
		if ((crd1->crd_alg == CRYPTO_MD5_HMAC ||
                     crd1->crd_alg == CRYPTO_SHA1_HMAC ||
                     crd1->crd_alg == CRYPTO_MD5 ||
                     crd1->crd_alg == CRYPTO_SHA1) &&
		    (crd2->crd_alg == CRYPTO_DES_CBC ||
		     crd2->crd_alg == CRYPTO_3DES_CBC ||
		     crd2->crd_alg == CRYPTO_ARC4) &&
		    ((crd2->crd_flags & CRD_F_ENCRYPT) == 0)) {
			cmd->base_masks = HIFN_BASE_CMD_DECODE;
			maccrd = crd1;
			enccrd = crd2;
		} else if ((crd1->crd_alg == CRYPTO_DES_CBC ||
		     crd1->crd_alg == CRYPTO_ARC4 ||
		     crd1->crd_alg == CRYPTO_3DES_CBC) &&
		    (crd2->crd_alg == CRYPTO_MD5_HMAC ||
                     crd2->crd_alg == CRYPTO_SHA1_HMAC ||
                     crd2->crd_alg == CRYPTO_MD5 ||
                     crd2->crd_alg == CRYPTO_SHA1) &&
		    (crd1->crd_flags & CRD_F_ENCRYPT)) {
			enccrd = crd1;
			maccrd = crd2;
		} else {
			/*
			 * We cannot order the 7751 as requested
			 */
			err = EINVAL;
			goto errout;
		}
	}

	if (enccrd) {
		cmd->enccrd = enccrd;
		cmd->base_masks |= HIFN_BASE_CMD_CRYPT;
		switch (enccrd->crd_alg) {
		case CRYPTO_ARC4:
			cmd->cry_masks |= HIFN_CRYPT_CMD_ALG_RC4;
			if ((enccrd->crd_flags & CRD_F_ENCRYPT)
			    != sc->sc_sessions[session].hs_prev_op)
				sc->sc_sessions[session].hs_state =
				    HS_STATE_USED;
			break;
		case CRYPTO_DES_CBC:
			cmd->cry_masks |= HIFN_CRYPT_CMD_ALG_DES |
			    HIFN_CRYPT_CMD_MODE_CBC |
			    HIFN_CRYPT_CMD_NEW_IV;
			break;
		case CRYPTO_3DES_CBC:
			cmd->cry_masks |= HIFN_CRYPT_CMD_ALG_3DES |
			    HIFN_CRYPT_CMD_MODE_CBC |
			    HIFN_CRYPT_CMD_NEW_IV;
			break;
		default:
			err = EINVAL;
			goto errout;
		}
		if (enccrd->crd_alg != CRYPTO_ARC4) {
			if (enccrd->crd_flags & CRD_F_ENCRYPT) {
				if (enccrd->crd_flags & CRD_F_IV_EXPLICIT)
					bcopy(enccrd->crd_iv, cmd->iv,
					    HIFN_IV_LENGTH);
				else
					bcopy(sc->sc_sessions[session].hs_iv,
					    cmd->iv, HIFN_IV_LENGTH);

				if ((enccrd->crd_flags & CRD_F_IV_PRESENT)
				    == 0) {
					if (crp->crp_flags & CRYPTO_F_IMBUF)
						m_copyback(cmd->src_m,
						    enccrd->crd_inject,
						    HIFN_IV_LENGTH, cmd->iv);
					else if (crp->crp_flags & CRYPTO_F_IOV)
						cuio_copyback(cmd->src_io,
						    enccrd->crd_inject,
						    HIFN_IV_LENGTH, cmd->iv);
				}
			} else {
				if (enccrd->crd_flags & CRD_F_IV_EXPLICIT)
					bcopy(enccrd->crd_iv, cmd->iv,
					    HIFN_IV_LENGTH);
				else if (crp->crp_flags & CRYPTO_F_IMBUF)
					m_copydata(cmd->src_m,
					    enccrd->crd_inject,
					    HIFN_IV_LENGTH, cmd->iv);
				else if (crp->crp_flags & CRYPTO_F_IOV)
					cuio_copydata(cmd->src_io,
					    enccrd->crd_inject,
					    HIFN_IV_LENGTH, cmd->iv);
			}
		}

		cmd->ck = enccrd->crd_key;
		cmd->cklen = enccrd->crd_klen >> 3;

		if (sc->sc_sessions[session].hs_state == HS_STATE_USED)
			cmd->cry_masks |= HIFN_CRYPT_CMD_NEW_KEY;
	}

	if (maccrd) {
		cmd->maccrd = maccrd;
		cmd->base_masks |= HIFN_BASE_CMD_MAC;

		switch (maccrd->crd_alg) {
		case CRYPTO_MD5:
			cmd->mac_masks |= HIFN_MAC_CMD_ALG_MD5 |
			    HIFN_MAC_CMD_RESULT | HIFN_MAC_CMD_MODE_HASH |
			    HIFN_MAC_CMD_POS_IPSEC;
                       break;
		case CRYPTO_MD5_HMAC:
			cmd->mac_masks |= HIFN_MAC_CMD_ALG_MD5 |
			    HIFN_MAC_CMD_RESULT | HIFN_MAC_CMD_MODE_HMAC |
			    HIFN_MAC_CMD_POS_IPSEC | HIFN_MAC_CMD_TRUNC;
			break;
		case CRYPTO_SHA1:
			cmd->mac_masks |= HIFN_MAC_CMD_ALG_SHA1 |
			    HIFN_MAC_CMD_RESULT | HIFN_MAC_CMD_MODE_HASH |
			    HIFN_MAC_CMD_POS_IPSEC;
			break;
		case CRYPTO_SHA1_HMAC:
			cmd->mac_masks |= HIFN_MAC_CMD_ALG_SHA1 |
			    HIFN_MAC_CMD_RESULT | HIFN_MAC_CMD_MODE_HMAC |
			    HIFN_MAC_CMD_POS_IPSEC | HIFN_MAC_CMD_TRUNC;
			break;
		}

		if ((maccrd->crd_alg == CRYPTO_SHA1_HMAC ||
		     maccrd->crd_alg == CRYPTO_MD5_HMAC) &&
		    sc->sc_sessions[session].hs_state == HS_STATE_USED) {
			cmd->mac_masks |= HIFN_MAC_CMD_NEW_KEY;
			bcopy(maccrd->crd_key, cmd->mac, maccrd->crd_klen >> 3);
			bzero(cmd->mac + (maccrd->crd_klen >> 3),
			    HIFN_MAC_KEY_LENGTH - (maccrd->crd_klen >> 3));
		}
	}

	cmd->crp = crp;
	cmd->session_num = session;
	cmd->softc = sc;

	err = hifn_crypto(sc, cmd, crp, hint);
	if (!err) {
		if (enccrd)
			sc->sc_sessions[session].hs_prev_op =
				enccrd->crd_flags & CRD_F_ENCRYPT;
		if (sc->sc_sessions[session].hs_state == HS_STATE_USED)
			sc->sc_sessions[session].hs_state = HS_STATE_KEY;
		return 0;
	} else if (err == ERESTART) {
		/*
		 * There weren't enough resources to dispatch the request
		 * to the part.  Notify the caller so they'll requeue this
		 * request and resubmit it again soon.
		 */
#ifdef HIFN_DEBUG
		if (hifn_debug)
			device_printf(sc->sc_dev, "requeue request\n");
#endif
		free(cmd, M_DEVBUF);
		sc->sc_needwakeup |= CRYPTO_SYMQ;
		return (err);
	}

errout:
	if (cmd != NULL)
		free(cmd, M_DEVBUF);
	if (err == EINVAL)
		hifnstats.hst_invalid++;
	else
		hifnstats.hst_nomem++;
	crp->crp_etype = err;
	crypto_done(crp);
	return (err);
}

static void
hifn_abort(struct hifn_softc *sc)
{
	struct hifn_dma *dma = sc->sc_dma;
	struct hifn_command *cmd;
	struct cryptop *crp;
	int i, u;

	i = dma->resk; u = dma->resu;
	while (u != 0) {
		cmd = dma->hifn_commands[i];
		KASSERT(cmd != NULL, ("hifn_abort: null command slot %u", i));
		dma->hifn_commands[i] = NULL;
		crp = cmd->crp;

		if ((dma->resr[i].l & htole32(HIFN_D_VALID)) == 0) {
			/* Salvage what we can. */
			u_int8_t *macbuf;

			if (cmd->base_masks & HIFN_BASE_CMD_MAC) {
				macbuf = dma->result_bufs[i];
				macbuf += 12;
			} else
				macbuf = NULL;
			hifnstats.hst_opackets++;
			hifn_callback(sc, cmd, macbuf);
		} else {
			if (cmd->src_map == cmd->dst_map) {
				bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
				    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
			} else {
				bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
				    BUS_DMASYNC_POSTWRITE);
				bus_dmamap_sync(sc->sc_dmat, cmd->dst_map,
				    BUS_DMASYNC_POSTREAD);
			}

			if (cmd->src_m != cmd->dst_m) {
				m_freem(cmd->src_m);
				crp->crp_buf = (caddr_t)cmd->dst_m;
			}

			/* non-shared buffers cannot be restarted */
			if (cmd->src_map != cmd->dst_map) {
				/*
				 * XXX should be EAGAIN, delayed until
				 * after the reset.
				 */
				crp->crp_etype = ENOMEM;
				bus_dmamap_unload(sc->sc_dmat, cmd->dst_map);
				bus_dmamap_destroy(sc->sc_dmat, cmd->dst_map);
			} else
				crp->crp_etype = ENOMEM;

			bus_dmamap_unload(sc->sc_dmat, cmd->src_map);
			bus_dmamap_destroy(sc->sc_dmat, cmd->src_map);

			free(cmd, M_DEVBUF);
			if (crp->crp_etype != EAGAIN)
				crypto_done(crp);
		}

		if (++i == HIFN_D_RES_RSIZE)
			i = 0;
		u--;
	}
	dma->resk = i; dma->resu = u;

	/* Force upload of key next time */
	for (i = 0; i < sc->sc_maxses; i++)
		if (sc->sc_sessions[i].hs_state == HS_STATE_KEY)
			sc->sc_sessions[i].hs_state = HS_STATE_USED;
	
	hifn_reset_board(sc, 1);
	hifn_init_dma(sc);
	hifn_init_pci_registers(sc);
}

static void
hifn_callback(struct hifn_softc *sc, struct hifn_command *cmd, u_int8_t *macbuf)
{
	struct hifn_dma *dma = sc->sc_dma;
	struct cryptop *crp = cmd->crp;
	struct cryptodesc *crd;
	struct mbuf *m;
	int totlen, i, u;

	if (cmd->src_map == cmd->dst_map) {
		bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
		    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);
	} else {
		bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_sync(sc->sc_dmat, cmd->dst_map,
		    BUS_DMASYNC_POSTREAD);
	}

	if (crp->crp_flags & CRYPTO_F_IMBUF) {
		if (cmd->src_m != cmd->dst_m) {
			crp->crp_buf = (caddr_t)cmd->dst_m;
			totlen = cmd->src_mapsize;
			for (m = cmd->dst_m; m != NULL; m = m->m_next) {
				if (totlen < m->m_len) {
					m->m_len = totlen;
					totlen = 0;
				} else
					totlen -= m->m_len;
			}
			cmd->dst_m->m_pkthdr.len = cmd->src_m->m_pkthdr.len;
			m_freem(cmd->src_m);
		}
	}

	if (cmd->sloplen != 0) {
		if (crp->crp_flags & CRYPTO_F_IMBUF)
			m_copyback((struct mbuf *)crp->crp_buf,
			    cmd->src_mapsize - cmd->sloplen,
			    cmd->sloplen, (caddr_t)&dma->slop[cmd->slopidx]);
		else if (crp->crp_flags & CRYPTO_F_IOV)
			cuio_copyback((struct uio *)crp->crp_buf,
			    cmd->src_mapsize - cmd->sloplen,
			    cmd->sloplen, (caddr_t)&dma->slop[cmd->slopidx]);
	}

	i = dma->dstk; u = dma->dstu;
	while (u != 0) {
		if (i == HIFN_D_DST_RSIZE)
			i = 0;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		if (dma->dstr[i].l & htole32(HIFN_D_VALID)) {
			bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			break;
		}
		i++, u--;
	}
	dma->dstk = i; dma->dstu = u;

	hifnstats.hst_obytes += cmd->dst_mapsize;

	if ((cmd->base_masks & (HIFN_BASE_CMD_CRYPT | HIFN_BASE_CMD_DECODE)) ==
	    HIFN_BASE_CMD_CRYPT) {
		for (crd = crp->crp_desc; crd; crd = crd->crd_next) {
			if (crd->crd_alg != CRYPTO_DES_CBC &&
			    crd->crd_alg != CRYPTO_3DES_CBC)
				continue;
			if (crp->crp_flags & CRYPTO_F_IMBUF)
				m_copydata((struct mbuf *)crp->crp_buf,
				    crd->crd_skip + crd->crd_len - HIFN_IV_LENGTH,
				    HIFN_IV_LENGTH,
				    cmd->softc->sc_sessions[cmd->session_num].hs_iv);
			else if (crp->crp_flags & CRYPTO_F_IOV) {
				cuio_copydata((struct uio *)crp->crp_buf,
				    crd->crd_skip + crd->crd_len - HIFN_IV_LENGTH,
				    HIFN_IV_LENGTH,
				    cmd->softc->sc_sessions[cmd->session_num].hs_iv);
			}
			break;
		}
	}

	if (macbuf != NULL) {
		for (crd = crp->crp_desc; crd; crd = crd->crd_next) {
                       int len;

                       if (crd->crd_alg == CRYPTO_MD5)
                               len = 16;
                       else if (crd->crd_alg == CRYPTO_SHA1)
                               len = 20;
                       else if (crd->crd_alg == CRYPTO_MD5_HMAC ||
                           crd->crd_alg == CRYPTO_SHA1_HMAC)
                               len = 12;
                       else
				continue;

			if (crp->crp_flags & CRYPTO_F_IMBUF)
				m_copyback((struct mbuf *)crp->crp_buf,
                                   crd->crd_inject, len, macbuf);
			else if ((crp->crp_flags & CRYPTO_F_IOV) && crp->crp_mac)
				bcopy((caddr_t)macbuf, crp->crp_mac, len);
			break;
		}
	}

	if (cmd->src_map != cmd->dst_map) {
		bus_dmamap_unload(sc->sc_dmat, cmd->dst_map);
		bus_dmamap_destroy(sc->sc_dmat, cmd->dst_map);
	}
	bus_dmamap_unload(sc->sc_dmat, cmd->src_map);
	bus_dmamap_destroy(sc->sc_dmat, cmd->src_map);
	free(cmd, M_DEVBUF);
	crypto_done(crp);
}

/*
 * 7811 PB3 rev/2 parts lock-up on burst writes to Group 0
 * and Group 1 registers; avoid conditions that could create
 * burst writes by doing a read in between the writes.
 *
 * NB: The read we interpose is always to the same register;
 *     we do this because reading from an arbitrary (e.g. last)
 *     register may not always work.
 */
static void
hifn_write_reg_0(struct hifn_softc *sc, bus_size_t reg, u_int32_t val)
{
	if (sc->sc_flags & HIFN_IS_7811) {
		if (sc->sc_bar0_lastreg == reg - 4)
			bus_space_read_4(sc->sc_st0, sc->sc_sh0, HIFN_0_PUCNFG);
		sc->sc_bar0_lastreg = reg;
	}
	bus_space_write_4(sc->sc_st0, sc->sc_sh0, reg, val);
}

static void
hifn_write_reg_1(struct hifn_softc *sc, bus_size_t reg, u_int32_t val)
{
	if (sc->sc_flags & HIFN_IS_7811) {
		if (sc->sc_bar1_lastreg == reg - 4)
			bus_space_read_4(sc->sc_st1, sc->sc_sh1, HIFN_1_REVID);
		sc->sc_bar1_lastreg = reg;
	}
	bus_space_write_4(sc->sc_st1, sc->sc_sh1, reg, val);
}
