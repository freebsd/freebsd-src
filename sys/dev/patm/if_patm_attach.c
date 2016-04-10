/*-
 * Copyright (c) 2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 * 	All rights reserved.
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
 * Author: Hartmut Brandt <harti@freebsd.org>
 *
 * Driver for IDT77252 based cards like ProSum's.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_natm.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/queue.h>
#include <sys/condvar.h>
#include <vm/uma.h>

#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_atm.h>
#include <net/route.h>
#ifdef ENABLE_BPF
#include <net/bpf.h>
#endif
#include <netinet/in.h>
#include <netinet/if_atm.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/mbpool.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/utopia/utopia.h>
#include <dev/patm/idt77252reg.h>
#include <dev/patm/if_patmvar.h>

MODULE_DEPEND(patm, utopia, 1, 1, 1);
MODULE_DEPEND(patm, pci, 1, 1, 1);
MODULE_DEPEND(patm, atm, 1, 1, 1);
MODULE_DEPEND(patm, libmbpool, 1, 1, 1);

devclass_t patm_devclass;

static int patm_probe(device_t dev);
static int patm_attach(device_t dev);
static int patm_detach(device_t dev);
static device_method_t patm_methods[] = {
	DEVMETHOD(device_probe,		patm_probe),
	DEVMETHOD(device_attach,	patm_attach),
	DEVMETHOD(device_detach,	patm_detach),
	{0,0}
};
static driver_t patm_driver = {
	"patm",
	patm_methods,
	sizeof(struct patm_softc),
};
DRIVER_MODULE(patm, pci, patm_driver, patm_devclass, NULL, 0);

static const struct {
	u_int	devid;
	const char *desc;
} devs[] = {
	{ PCI_DEVICE_IDT77252,	"NICStAR (77222/77252) ATM adapter" },
	{ PCI_DEVICE_IDT77v252,	"NICStAR (77v252) ATM adapter" },
	{ PCI_DEVICE_IDT77v222,	"NICStAR (77v222) ATM adapter" },
	{ 0, NULL }
};

SYSCTL_DECL(_hw_atm);

static int patm_phy_readregs(struct ifatm *, u_int, uint8_t *, u_int *);
static int patm_phy_writereg(struct ifatm *, u_int, u_int, u_int);
static const struct utopia_methods patm_utopia_methods = {
	patm_phy_readregs,
	patm_phy_writereg
};

static void patm_destroy(struct patm_softc *sc);

static int patm_sysctl_istats(SYSCTL_HANDLER_ARGS);
static int patm_sysctl_eeprom(SYSCTL_HANDLER_ARGS);

static void patm_read_eeprom(struct patm_softc *sc);
static int patm_sq_init(struct patm_softc *sc);
static int patm_rbuf_init(struct patm_softc *sc);
static int patm_txmap_init(struct patm_softc *sc);

static void patm_env_getuint(struct patm_softc *, u_int *, const char *);

#ifdef PATM_DEBUG
static int patm_sysctl_regs(SYSCTL_HANDLER_ARGS);
static int patm_sysctl_tsq(SYSCTL_HANDLER_ARGS);
int patm_dump_vc(u_int unit, u_int vc) __unused;
int patm_dump_regs(u_int unit) __unused;
int patm_dump_sram(u_int unit, u_int from, u_int words) __unused;
#endif

/*
 * Probe for a IDT77252 controller
 */
static int
patm_probe(device_t dev)
{
	u_int i;

	if (pci_get_vendor(dev) == PCI_VENDOR_IDT) {
		for (i = 0; devs[i].desc != NULL; i++)
			if (pci_get_device(dev) == devs[i].devid) {
				device_set_desc(dev, devs[i].desc);
				return (BUS_PROBE_DEFAULT);
			}
	}
	return (ENXIO);
}

/*
 * Attach
 */
static int
patm_attach(device_t dev)
{
	struct patm_softc *sc;
	int error;
	struct ifnet *ifp;
	int rid;
	u_int a;

	static const struct idt_mmap idt_mmap[4] = IDT_MMAP;

	sc = device_get_softc(dev);

	sc->dev = dev;
#ifdef IATM_DEBUG
	sc->debug = IATM_DEBUG;
#endif
	ifp = sc->ifp = if_alloc(IFT_ATM);
	if (ifp == NULL) {
		return (ENOSPC);
	}

	IFP2IFATM(sc->ifp)->mib.device = ATM_DEVICE_IDTABR25;
	IFP2IFATM(sc->ifp)->mib.serial = 0;
	IFP2IFATM(sc->ifp)->mib.hw_version = 0;
	IFP2IFATM(sc->ifp)->mib.sw_version = 0;
	IFP2IFATM(sc->ifp)->mib.vpi_bits = PATM_VPI_BITS;
	IFP2IFATM(sc->ifp)->mib.vci_bits = 0;	/* set below */
	IFP2IFATM(sc->ifp)->mib.max_vpcs = 0;
	IFP2IFATM(sc->ifp)->mib.max_vccs = 0;	/* set below */
	IFP2IFATM(sc->ifp)->mib.media = IFM_ATM_UNKNOWN;
	IFP2IFATM(sc->ifp)->phy = &sc->utopia;

	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_SIMPLEX;
	ifp->if_init = patm_init;
	ifp->if_ioctl = patm_ioctl;
	ifp->if_start = patm_start;

	/* do this early so we can destroy unconditionally */
	mtx_init(&sc->mtx, device_get_nameunit(dev),
	    MTX_NETWORK_LOCK, MTX_DEF);
	mtx_init(&sc->tst_lock, "tst lock", NULL, MTX_DEF);
	cv_init(&sc->vcc_cv, "vcc_close");

	callout_init(&sc->tst_callout, 1);

	sysctl_ctx_init(&sc->sysctl_ctx);

	/*
	 * Get revision
	 */
	sc->revision = pci_read_config(dev, PCIR_REVID, 4) & 0xf;

	/*
	 * Enable PCI bus master and memory
	 */
	pci_enable_busmaster(dev);

	rid = IDT_PCI_REG_MEMBASE;
	sc->memres = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->memres == NULL) {
		patm_printf(sc, "could not map memory\n");
		error = ENXIO;
		goto fail;
	}
	sc->memh = rman_get_bushandle(sc->memres);
	sc->memt = rman_get_bustag(sc->memres);

	/*
	 * Allocate the interrupt (enable it later)
	 */
	sc->irqid = 0;
	sc->irqres = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->irqid,
	    RF_SHAREABLE | RF_ACTIVE);
	if (sc->irqres == 0) {
		patm_printf(sc, "could not allocate irq\n");
		error = ENXIO;
		goto fail;
	}

	/*
	 * Construct the sysctl tree
	 */
	error = ENOMEM;
	if ((sc->sysctl_tree = SYSCTL_ADD_NODE(&sc->sysctl_ctx,
	    SYSCTL_STATIC_CHILDREN(_hw_atm), OID_AUTO,
	    device_get_nameunit(dev), CTLFLAG_RD, 0, "")) == NULL)
		goto fail;

	if (SYSCTL_ADD_PROC(&sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    OID_AUTO, "istats", CTLTYPE_OPAQUE | CTLFLAG_RD, sc, 0,
	    patm_sysctl_istats, "S", "internal statistics") == NULL)
		goto fail;

	if (SYSCTL_ADD_PROC(&sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    OID_AUTO, "eeprom", CTLTYPE_OPAQUE | CTLFLAG_RD, sc, 0,
	    patm_sysctl_eeprom, "S", "EEPROM contents") == NULL)
		goto fail;

	if (SYSCTL_ADD_UINT(&sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    OID_AUTO, "lbuf_max", CTLFLAG_RD, &sc->lbuf_max,
	    0, "maximum number of large receive buffers") == NULL)
		goto fail;
	patm_env_getuint(sc, &sc->lbuf_max, "lbuf_max");

	if (SYSCTL_ADD_UINT(&sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    OID_AUTO, "max_txmaps", CTLFLAG_RW, &sc->tx_maxmaps,
	    0, "maximum number of TX DMA maps") == NULL)
		goto fail;
	patm_env_getuint(sc, &sc->tx_maxmaps, "tx_maxmaps");

#ifdef PATM_DEBUG
	if (SYSCTL_ADD_UINT(&sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    OID_AUTO, "debug", CTLFLAG_RW, &sc->debug,
	    0, "debug flags") == NULL)
		goto fail;
	sc->debug = PATM_DEBUG;
	patm_env_getuint(sc, &sc->debug, "debug");

	if (SYSCTL_ADD_PROC(&sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    OID_AUTO, "regs", CTLTYPE_OPAQUE | CTLFLAG_RD, sc, 0,
	    patm_sysctl_regs, "S", "registers") == NULL)
		goto fail;

	if (SYSCTL_ADD_PROC(&sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    OID_AUTO, "tsq", CTLTYPE_OPAQUE | CTLFLAG_RD, sc, 0,
	    patm_sysctl_tsq, "S", "TSQ") == NULL)
		goto fail;
#endif

	patm_reset(sc);

	/*
	 * Detect and attach the phy.
	 */
	patm_debug(sc, ATTACH, "attaching utopia");
	IFP2IFATM(sc->ifp)->phy = &sc->utopia;
	utopia_attach(&sc->utopia, IFP2IFATM(sc->ifp), &sc->media, &sc->mtx,
	    &sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
	    &patm_utopia_methods);

	/*
	 * Start the PHY because we need the autodetection
	 */
	patm_debug(sc, ATTACH, "starting utopia");
	mtx_lock(&sc->mtx);
	utopia_start(&sc->utopia);
	utopia_reset(&sc->utopia);
	mtx_unlock(&sc->mtx);

	/* Read EEPROM */
	patm_read_eeprom(sc);

	/* analyze it */
	if (strncmp(sc->eeprom + PATM_PROATM_NAME_OFFSET, PATM_PROATM_NAME,
	    strlen(PATM_PROATM_NAME)) == 0) {
		if (sc->utopia.chip->type == UTP_TYPE_IDT77105) {
			IFP2IFATM(sc->ifp)->mib.device = ATM_DEVICE_PROATM25;
			IFP2IFATM(sc->ifp)->mib.pcr = ATM_RATE_25_6M;
			IFP2IFATM(sc->ifp)->mib.media = IFM_ATM_UTP_25;
			sc->flags |= PATM_25M;
			patm_printf(sc, "ProATM 25 interface; ");

		} else {
			/* cannot really know which media */
			IFP2IFATM(sc->ifp)->mib.device = ATM_DEVICE_PROATM155;
			IFP2IFATM(sc->ifp)->mib.pcr = ATM_RATE_155M;
			IFP2IFATM(sc->ifp)->mib.media = IFM_ATM_MM_155;
			patm_printf(sc, "ProATM 155 interface; ");
		}

		bcopy(sc->eeprom + PATM_PROATM_MAC_OFFSET, IFP2IFATM(sc->ifp)->mib.esi,
		    sizeof(IFP2IFATM(sc->ifp)->mib.esi));

	} else {
		if (sc->utopia.chip->type == UTP_TYPE_IDT77105) {
			IFP2IFATM(sc->ifp)->mib.device = ATM_DEVICE_IDTABR25;
			IFP2IFATM(sc->ifp)->mib.pcr = ATM_RATE_25_6M;
			IFP2IFATM(sc->ifp)->mib.media = IFM_ATM_UTP_25;
			sc->flags |= PATM_25M;
			patm_printf(sc, "IDT77252 25MBit interface; ");

		} else {
			/* cannot really know which media */
			IFP2IFATM(sc->ifp)->mib.device = ATM_DEVICE_IDTABR155;
			IFP2IFATM(sc->ifp)->mib.pcr = ATM_RATE_155M;
			IFP2IFATM(sc->ifp)->mib.media = IFM_ATM_MM_155;
			patm_printf(sc, "IDT77252 155MBit interface; ");
		}

		bcopy(sc->eeprom + PATM_IDT_MAC_OFFSET, IFP2IFATM(sc->ifp)->mib.esi,
		    sizeof(IFP2IFATM(sc->ifp)->mib.esi));
	}
	printf("idt77252 Rev. %c; %s PHY\n", 'A' + sc->revision,
	    sc->utopia.chip->name);

	utopia_reset_media(&sc->utopia);
	utopia_init_media(&sc->utopia);

	/*
	 * Determine RAM size
	 */
	for (a = 0; a < 0x20000; a++)
		patm_sram_write(sc, a, 0);
	patm_sram_write(sc, 0, 0xdeadbeef);
	if (patm_sram_read(sc, 0x4004) == 0xdeadbeef)
		sc->mmap = &idt_mmap[0];
	else if (patm_sram_read(sc, 0x8000) == 0xdeadbeef)
		sc->mmap = &idt_mmap[1];
	else if (patm_sram_read(sc, 0x20000) == 0xdeadbeef)
		sc->mmap = &idt_mmap[2];
	else
		sc->mmap = &idt_mmap[3];

	IFP2IFATM(sc->ifp)->mib.vci_bits = sc->mmap->vcbits - IFP2IFATM(sc->ifp)->mib.vpi_bits;
	IFP2IFATM(sc->ifp)->mib.max_vccs = sc->mmap->max_conn;
	patm_sram_write(sc, 0, 0);
	patm_printf(sc, "%uK x 32 SRAM; %u connections\n", sc->mmap->sram,
	    sc->mmap->max_conn);

	/* initialize status queues */
	error = patm_sq_init(sc);
	if (error != 0)
		goto fail;

	/* get TST */
	sc->tst_soft = malloc(sizeof(uint32_t) * sc->mmap->tst_size,
	    M_DEVBUF, M_WAITOK);

	/* allocate all the receive buffer stuff */
	error = patm_rbuf_init(sc);
	if (error != 0)
		goto fail;

	/*
	 * Allocate SCD tag
	 *
	 * Don't use BUS_DMA_ALLOCNOW, because we never need bouncing with
	 * bus_dmamem_alloc()
	 */
	error = bus_dma_tag_create(bus_get_dma_tag(dev), PAGE_SIZE, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR,
	    NULL, NULL, sizeof(struct patm_scd), 1,
	    sizeof(struct patm_scd), 0, NULL, NULL, &sc->scd_tag);
	if (error) {
		patm_printf(sc, "SCD DMA tag create %d\n", error);
		goto fail;
	}
	LIST_INIT(&sc->scd_list);

	/* allocate VCC zone and pointers */
	if ((sc->vcc_zone = uma_zcreate("PATM vccs", sizeof(struct patm_vcc),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0)) == NULL) {
		patm_printf(sc, "cannot allocate zone for vccs\n");
		goto fail;
	}
	sc->vccs = malloc(sizeof(sc->vccs[0]) * sc->mmap->max_conn,
	    M_DEVBUF, M_WAITOK | M_ZERO);

	/* allocate transmission resources */
	error = patm_txmap_init(sc);
	if (error != 0)
		goto fail;

	/* poll while we are not running */
	sc->utopia.flags |= UTP_FL_POLL_CARRIER;

	patm_debug(sc, ATTACH, "attaching interface");
	atm_ifattach(ifp);

#ifdef ENABLE_BPF
	bpfattach(ifp, DLT_ATM_RFC1483, sizeof(struct atmllc));
#endif

	patm_debug(sc, ATTACH, "attaching interrupt handler");
	error = bus_setup_intr(dev, sc->irqres, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, patm_intr, sc, &sc->ih);
	if (error != 0) {
		patm_printf(sc, "could not setup interrupt\n");
		atm_ifdetach(sc->ifp);
		if_free(sc->ifp);
		goto fail;
	}

	return (0);

  fail:
	patm_destroy(sc);
	return (error);
}

/*
 * Detach
 */
static int
patm_detach(device_t dev)
{
	struct patm_softc *sc;

	sc = device_get_softc(dev);

	mtx_lock(&sc->mtx);
	patm_stop(sc);
	if (sc->utopia.state & UTP_ST_ATTACHED) {
		patm_debug(sc, ATTACH, "detaching utopia");
		utopia_stop(&sc->utopia);
		utopia_detach(&sc->utopia);
	}
	mtx_unlock(&sc->mtx);

	atm_ifdetach(sc->ifp);

	patm_destroy(sc);

	return (0);
}

/*
 * Destroy everything. Assume we are stopped.
 */
static void
patm_destroy(struct patm_softc *sc)
{
	u_int i;
	struct patm_txmap *map;

	if (sc->ih != NULL)
		bus_teardown_intr(sc->dev, sc->irqres, sc->ih);

	if (sc->tx_mapzone != NULL) {
		/* all maps must be free */
		while ((map = SLIST_FIRST(&sc->tx_maps_free)) != NULL) {
			bus_dmamap_destroy(sc->tx_tag, map->map);
			SLIST_REMOVE_HEAD(&sc->tx_maps_free, link);
			uma_zfree(sc->tx_mapzone, map);
		}
		uma_zdestroy(sc->tx_mapzone);
	}

	if (sc->scd_tag != NULL)
		bus_dma_tag_destroy(sc->scd_tag);

	if (sc->tx_tag != NULL)
		bus_dma_tag_destroy(sc->scd_tag);

	if (sc->vccs != NULL) {
		for (i = 0; i < sc->mmap->max_conn; i++)
			if (sc->vccs[i] != NULL)
				uma_zfree(sc->vcc_zone, sc->vccs[i]);
		free(sc->vccs, M_DEVBUF);
	}
	if (sc->vcc_zone != NULL)
		uma_zdestroy(sc->vcc_zone);

	if (sc->lbufs != NULL) {
		for (i = 0; i < sc->lbuf_max; i++)
			bus_dmamap_destroy(sc->lbuf_tag, sc->lbufs[i].map);
		free(sc->lbufs, M_DEVBUF);
	}

	if (sc->lbuf_tag != NULL)
		bus_dma_tag_destroy(sc->lbuf_tag);

	if (sc->sbuf_pool != NULL)
		mbp_destroy(sc->sbuf_pool);
	if (sc->vbuf_pool != NULL)
		mbp_destroy(sc->vbuf_pool);

	if (sc->sbuf_tag != NULL)
		bus_dma_tag_destroy(sc->sbuf_tag);

	if (sc->tst_soft != NULL)
		free(sc->tst_soft, M_DEVBUF);

	/*
	 * Free all status queue memory resources
	 */
	if (sc->tsq != NULL) {
		bus_dmamap_unload(sc->sq_tag, sc->sq_map);
		bus_dmamem_free(sc->sq_tag, sc->tsq, sc->sq_map);
		bus_dma_tag_destroy(sc->sq_tag);
	}

	if (sc->irqres != NULL)
		bus_release_resource(sc->dev, SYS_RES_IRQ,
		    sc->irqid, sc->irqres);
	if (sc->memres != NULL)
		bus_release_resource(sc->dev, SYS_RES_MEMORY,
		    IDT_PCI_REG_MEMBASE, sc->memres);

	/* this was initialize unconditionally */
	sysctl_ctx_free(&sc->sysctl_ctx);
	cv_destroy(&sc->vcc_cv);
	mtx_destroy(&sc->tst_lock);
	mtx_destroy(&sc->mtx);

	if (sc->ifp != NULL)
		if_free(sc->ifp);
}

/*
 * Try to find a variable in the environment and parse it as an unsigned
 * integer.
 */
static void
patm_env_getuint(struct patm_softc *sc, u_int *var, const char *name)
{
	char full[IFNAMSIZ + 3 + 20];
	char *val, *end;
	u_long u;

	snprintf(full, sizeof(full), "hw.%s.%s",
	    device_get_nameunit(sc->dev), name);

	if ((val = kern_getenv(full)) != NULL) {
		u = strtoul(val, &end, 0);
		if (end > val && *end == '\0') {
			if (bootverbose)
				patm_printf(sc, "%s=%lu\n", full, u);
			*var = u;
		}
		freeenv(val);
	}
}

/*
 * Sysctl handler for internal statistics
 *
 * LOCK: unlocked, needed
 */
static int
patm_sysctl_istats(SYSCTL_HANDLER_ARGS)
{
	struct patm_softc *sc = arg1;
	uint32_t *ret;
	int error;

	ret = malloc(sizeof(sc->stats), M_TEMP, M_WAITOK);

	mtx_lock(&sc->mtx);
	bcopy(&sc->stats, ret, sizeof(sc->stats));
	mtx_unlock(&sc->mtx);

	error = SYSCTL_OUT(req, ret, sizeof(sc->stats));
	free(ret, M_TEMP);

	return (error);
}

/*
 * Sysctl handler for EEPROM
 *
 * LOCK: unlocked, needed
 */
static int
patm_sysctl_eeprom(SYSCTL_HANDLER_ARGS)
{
	struct patm_softc *sc = arg1;
	void *ret;
	int error;

	ret = malloc(sizeof(sc->eeprom), M_TEMP, M_WAITOK);

	mtx_lock(&sc->mtx);
	bcopy(sc->eeprom, ret, sizeof(sc->eeprom));
	mtx_unlock(&sc->mtx);

	error = SYSCTL_OUT(req, ret, sizeof(sc->eeprom));
	free(ret, M_TEMP);

	return (error);
}

/*
 * Read the EEPROM. We assume that this is a XIRCOM 25020
 */
static void
patm_read_eeprom(struct patm_softc *sc)
{
	u_int gp;
	uint8_t byte;
	int i, addr;

	static const uint32_t tab[] = {
		/* CS transition to reset the chip */
		IDT_GP_EECS | IDT_GP_EESCLK,	0,
		/* read command 0x03 */
		IDT_GP_EESCLK,			0,
		IDT_GP_EESCLK,			0,
		IDT_GP_EESCLK,			0,
		IDT_GP_EESCLK,			0,
		IDT_GP_EESCLK,			0,
		IDT_GP_EESCLK,			IDT_GP_EEDO,
		IDT_GP_EESCLK | IDT_GP_EEDO,	IDT_GP_EEDO,
		IDT_GP_EESCLK | IDT_GP_EEDO,	0,
		/* address 0x00 */
		IDT_GP_EESCLK,			0,
		IDT_GP_EESCLK,			0,
		IDT_GP_EESCLK,			0,
		IDT_GP_EESCLK,			0,
		IDT_GP_EESCLK,			0,
		IDT_GP_EESCLK,			0,
		IDT_GP_EESCLK,			0,
		IDT_GP_EESCLK,			0,
	};

	/* go to a known state (chip enabled) */
	gp = patm_nor_read(sc, IDT_NOR_GP);
	gp &= ~(IDT_GP_EESCLK | IDT_GP_EECS | IDT_GP_EEDO);

	for (i = 0; i < sizeof(tab) / sizeof(tab[0]); i++) {
		patm_nor_write(sc, IDT_NOR_GP, gp | tab[i]);
		DELAY(40);
	}

	/* read out the prom */
	for (addr = 0; addr < 256; addr++) {
		byte = 0;
		for (i = 0; i < 8; i++) {
			byte <<= 1;
			if (patm_nor_read(sc, IDT_NOR_GP) & IDT_GP_EEDI)
				byte |= 1;
			/* rising CLK */
			patm_nor_write(sc, IDT_NOR_GP, gp | IDT_GP_EESCLK);
			DELAY(40);
			/* falling clock */
			patm_nor_write(sc, IDT_NOR_GP, gp);
			DELAY(40);
		}
		sc->eeprom[addr] = byte;
	}
}

/*
 * PHY access read
 */
static int
patm_phy_readregs(struct ifatm *ifatm, u_int reg, uint8_t *val, u_int *n)
{
	struct patm_softc *sc = ifatm->ifp->if_softc;
	u_int cnt = *n;

	if (reg >= 0x100)
		return (EINVAL);

	patm_cmd_wait(sc);
	while (reg < 0x100 && cnt > 0) {
		patm_nor_write(sc, IDT_NOR_CMD, IDT_MKCMD_RUTIL(1, 0, reg));
		patm_cmd_wait(sc);
		*val = patm_nor_read(sc, IDT_NOR_D0);
		patm_debug(sc, PHY, "phy(%02x)=%02x", reg, *val);
		val++;
		reg++;
		cnt--;
	}
	*n = *n - cnt;
	return (0);
}

/*
 * Write PHY reg
 */
static int
patm_phy_writereg(struct ifatm *ifatm, u_int reg, u_int mask, u_int val)
{
	struct patm_softc *sc = ifatm->ifp->if_softc;
	u_int old, new;

	if (reg >= 0x100)
		return (EINVAL);

	patm_cmd_wait(sc);
	patm_nor_write(sc, IDT_NOR_CMD, IDT_MKCMD_RUTIL(1, 0, reg));
	patm_cmd_wait(sc);

	old = patm_nor_read(sc, IDT_NOR_D0);
	new = (old & ~mask) | (val & mask);
	patm_debug(sc, PHY, "phy(%02x) %02x -> %02x", reg, old, new);
	    
	patm_nor_write(sc, IDT_NOR_D0, new);
	patm_nor_write(sc, IDT_NOR_CMD, IDT_MKCMD_WUTIL(1, 0, reg));
	patm_cmd_wait(sc);

	return (0);
}

/*
 * Allocate a large chunk of DMA able memory for the transmit
 * and receive status queues. We align this to a page boundary
 * to ensure the alignment.
 */
static int
patm_sq_init(struct patm_softc *sc)
{
	int error;
	void *p;

	/* compute size of the two queues */
	sc->sq_size = IDT_TSQ_SIZE * IDT_TSQE_SIZE +
	    PATM_RSQ_SIZE * IDT_RSQE_SIZE +
	    IDT_RAWHND_SIZE;

	patm_debug(sc, ATTACH,
	    "allocating status queues (%zu) ...", sc->sq_size);

	/*
	 * allocate tag
	 * Don't use BUS_DMA_ALLOCNOW, because we never need bouncing with
	 * bus_dmamem_alloc()
	 */
	error = bus_dma_tag_create(bus_get_dma_tag(sc->dev),
	    PATM_SQ_ALIGNMENT, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR,
	    NULL, NULL, sc->sq_size, 1, sc->sq_size,
	    0, NULL, NULL, &sc->sq_tag);
	if (error) {
		patm_printf(sc, "memory DMA tag create %d\n", error);
		return (error);
	}

	/* allocate memory */
	error = bus_dmamem_alloc(sc->sq_tag, &p, 0, &sc->sq_map);
	if (error) {
		patm_printf(sc, "memory DMA alloc %d\n", error);
		bus_dma_tag_destroy(sc->sq_tag);
		return (error);
	}

	/* map it */
	sc->tsq_phy = 0x1fff;
	error = bus_dmamap_load(sc->sq_tag, sc->sq_map, p,
	    sc->sq_size, patm_load_callback, &sc->tsq_phy, BUS_DMA_NOWAIT);
	if (error) {
		patm_printf(sc, "memory DMA map load %d\n", error);
		bus_dmamem_free(sc->sq_tag, p, sc->sq_map);
		bus_dma_tag_destroy(sc->sq_tag);
		return (error);
	}

	/* set queue start */
	sc->tsq = p;
	sc->rsq = (void *)((char *)p + IDT_TSQ_SIZE * IDT_TSQE_SIZE);
	sc->rsq_phy = sc->tsq_phy + IDT_TSQ_SIZE * IDT_TSQE_SIZE;
	sc->rawhnd = (void *)((char *)sc->rsq + PATM_RSQ_SIZE * IDT_RSQE_SIZE);
	sc->rawhnd_phy = sc->rsq_phy + PATM_RSQ_SIZE * IDT_RSQE_SIZE;

	return (0);
}

/*
 * Initialize all receive buffer stuff
 */
static int
patm_rbuf_init(struct patm_softc *sc)
{
	u_int i;
	int error;

	patm_debug(sc, ATTACH, "allocating Rx buffer resources ...");
	/*
	 * Create a tag for small buffers. We allocate these page wise.
	 * Don't use BUS_DMA_ALLOCNOW, because we never need bouncing with
	 * bus_dmamem_alloc()
	 */
	if ((error = bus_dma_tag_create(bus_get_dma_tag(sc->dev), PAGE_SIZE, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    SMBUF_PAGE_SIZE, 1, SMBUF_PAGE_SIZE, 0,
	    NULL, NULL, &sc->sbuf_tag)) != 0) {
		patm_printf(sc, "sbuf DMA tag create %d\n", error);
		return (error);
	}

	error = mbp_create(&sc->sbuf_pool, "patm sbufs", sc->sbuf_tag,
	    SMBUF_MAX_PAGES, SMBUF_PAGE_SIZE, SMBUF_CHUNK_SIZE);
	if (error != 0) {
		patm_printf(sc, "smbuf pool create %d\n", error);
		return (error);
	}

	error = mbp_create(&sc->vbuf_pool, "patm vbufs", sc->sbuf_tag,
	    VMBUF_MAX_PAGES, SMBUF_PAGE_SIZE, VMBUF_CHUNK_SIZE);
	if (error != 0) {
		patm_printf(sc, "vmbuf pool create %d\n", error);
		return (error);
	}

	/*
	 * Create a tag for large buffers.
	 * Don't use BUS_DMA_ALLOCNOW, because it makes no sense with multiple
	 * maps using one tag. Rather use BUS_DMA_NOWAIT when loading the map
	 * to prevent EINPROGRESS.
	 */
	if ((error = bus_dma_tag_create(bus_get_dma_tag(sc->dev), 4, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    MCLBYTES, 1, MCLBYTES, 0, 
	    NULL, NULL, &sc->lbuf_tag)) != 0) {
		patm_printf(sc, "lbuf DMA tag create %d\n", error);
		return (error);
	}

	if (sc->lbuf_max < IDT_FBQ_SIZE)
		sc->lbuf_max = LMBUF_MAX;
	sc->lbufs = malloc(sizeof(sc->lbufs[0]) * sc->lbuf_max,
	    M_DEVBUF, M_ZERO | M_WAITOK);

	SLIST_INIT(&sc->lbuf_free_list);
	for (i = 0; i < sc->lbuf_max; i++) {
		struct lmbuf *b = &sc->lbufs[i];

		error = bus_dmamap_create(sc->lbuf_tag, 0, &b->map);
		if (error) {
			/* must deallocate here, because a test for NULL
			 * does not work on most archs */
			while (i-- > 0)
				bus_dmamap_destroy(sc->lbuf_tag,
				    sc->lbufs[i].map);
			free(sc->lbufs, M_DEVBUF);
			sc->lbufs = NULL;
			return (error);
		}
		b->handle = i;
		SLIST_INSERT_HEAD(&sc->lbuf_free_list, b, link);
	}

	return (0);
}

/*
 * Allocate everything needed for the transmission maps.
 */
static int
patm_txmap_init(struct patm_softc *sc)
{
	int error;
	struct patm_txmap *map;

	/* get transmission tag */
	error = bus_dma_tag_create(bus_get_dma_tag(sc->dev), 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR,
	    NULL, NULL, 65536, IDT_SCQ_SIZE - 1, 65536,
	    0, NULL, NULL, &sc->tx_tag);
	if (error) {
		patm_printf(sc, "cannot allocate TX tag %d\n", error);
		return (error);
	}

	if ((sc->tx_mapzone = uma_zcreate("PATM tx maps",
	    sizeof(struct patm_txmap), NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0)) == NULL)
		return (ENOMEM);

	if (sc->tx_maxmaps < PATM_CFG_TXMAPS_MAX)
		sc->tx_maxmaps = PATM_CFG_TXMAPS_MAX;
	sc->tx_nmaps = PATM_CFG_TXMAPS_INIT;

	for (sc->tx_nmaps = 0; sc->tx_nmaps < PATM_CFG_TXMAPS_INIT;
	    sc->tx_nmaps++) {
		map = uma_zalloc(sc->tx_mapzone, M_WAITOK);
		error = bus_dmamap_create(sc->tx_tag, 0, &map->map);
		if (error) {
			uma_zfree(sc->tx_mapzone, map);
			return (ENOMEM);
		}
		SLIST_INSERT_HEAD(&sc->tx_maps_free, map, link);
	}

	return (0);
}

#ifdef PATM_DEBUG

/*
 * Sysctl handler for REGS
 *
 * LOCK: unlocked, needed
 */
static int
patm_sysctl_regs(SYSCTL_HANDLER_ARGS)
{
	struct patm_softc *sc = arg1;
	uint32_t *ret;
	int error, i;

	ret = malloc(IDT_NOR_END, M_TEMP, M_WAITOK);

	mtx_lock(&sc->mtx);
	for (i = 0; i < IDT_NOR_END; i += 4)
		ret[i / 4] = patm_nor_read(sc, i);
	mtx_unlock(&sc->mtx);

	error = SYSCTL_OUT(req, ret, IDT_NOR_END);
	free(ret, M_TEMP);

	return (error);
}

/*
 * Sysctl handler for TSQ
 *
 * LOCK: unlocked, needed
 */
static int
patm_sysctl_tsq(SYSCTL_HANDLER_ARGS)
{
	struct patm_softc *sc = arg1;
	void *ret;
	int error;

	ret = malloc(IDT_TSQ_SIZE * IDT_TSQE_SIZE, M_TEMP, M_WAITOK);

	mtx_lock(&sc->mtx);
	memcpy(ret, sc->tsq, IDT_TSQ_SIZE * IDT_TSQE_SIZE);
	mtx_unlock(&sc->mtx);

	error = SYSCTL_OUT(req, ret, IDT_TSQ_SIZE * IDT_TSQE_SIZE);
	free(ret, M_TEMP);

	return (error);
}

/*
 * debugging
 */
static struct patm_softc *
patm_dump_unit(u_int unit)
{
	devclass_t dc;
	struct patm_softc *sc;

	dc = devclass_find("patm");
	if (dc == NULL) {
		printf("%s: can't find devclass\n", __func__);
		return (NULL);
	}
	sc = devclass_get_softc(dc, unit);
	if (sc == NULL) {
		printf("%s: invalid unit number: %d\n", __func__, unit);
		return (NULL);
	}
	return (sc);
}

int
patm_dump_vc(u_int unit, u_int vc)
{
	struct patm_softc *sc;
	uint32_t tct[8];
	uint32_t rct[4];
	uint32_t scd[12];
	u_int i;

	if ((sc = patm_dump_unit(unit)) == NULL)
		return (0);

	for (i = 0; i < 8; i++)
		tct[i] = patm_sram_read(sc, vc * 8 + i);
	for (i = 0; i < 4; i++)
		rct[i] = patm_sram_read(sc, sc->mmap->rct + vc * 4 + i);
	for (i = 0; i < 12; i++)
		scd[i] = patm_sram_read(sc, (tct[0] & 0x7ffff) + i);

	printf("TCT%3u: %08x %08x %08x %08x  %08x %08x %08x %08x\n", vc,
	    tct[0], tct[1], tct[2], tct[3], tct[4], tct[5], tct[6], tct[7]);
	printf("RCT%3u: %08x %08x %08x %08x\n", vc,
	    rct[0], rct[1], rct[2], rct[3]);
	printf("SCD%3u: %08x %08x %08x %08x  %08x %08x %08x %08x\n", vc,
	    scd[0], scd[1], scd[2], scd[3], scd[4], scd[5], scd[6], scd[7]);
	printf("        %08x %08x %08x %08x\n",
	    scd[8], scd[9], scd[10], scd[11]);

	return (0);
}

int
patm_dump_regs(u_int unit)
{
	struct patm_softc *sc;
	u_int i;

	if ((sc = patm_dump_unit(unit)) == NULL)
		return (0);

	for (i = 0; i <= IDT_NOR_DNOW; i += 4)
		printf("%x: %08x\n", i, patm_nor_read(sc, i));

	return (0);
}

int
patm_dump_sram(u_int unit, u_int from, u_int words)
{
	struct patm_softc *sc;
	u_int i;

	if ((sc = patm_dump_unit(unit)) == NULL)
		return (0);

	for (i = 0; i < words; i++) {
		if (i % 8 == 0)
			printf("%05x:", from + i);
		printf(" %08x", patm_sram_read(sc, from + i));
		if (i % 8 == 7)
			printf("\n");
	}
	if (i % 8 != 0)
		printf("\n");
	return (0);
}
#endif
