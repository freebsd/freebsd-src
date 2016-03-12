/*-
 * Copyright (c) 2007 Bruce M. Simpson.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <dev/bhnd/bhnd_ids.h>
#include <dev/bhnd/cores/bhnd_chipcreg.h>

#include "sibareg.h"
#include "sibavar.h"

/*
 * Supports siba(4) attachment to a MIPS nexus bus.
 * 
 * This driver is a direct port of Bruce M. Simpson' original siba(4) to the
 * bhnd(4) bus infrastructure.
 */

/*
 * TODO: De-mipsify this code.
 * TODO: cpu clock calculation. -> move to siba_cc instance
 * TODO: Hardwire IRQs for attached cores on siba at probe time.
 * TODO: Support detach.
 * TODO: Power management.
 * TODO: code cleanup.
 * TODO: Support deployments of siba other than as a system bus.
 */

#ifndef MIPS_MEM_RID
#define MIPS_MEM_RID 0x20
#endif

extern int rman_debug;

static struct rman mem_rman; 	/* XXX move to softc */

static int siba_debug = 1;
static const char descfmt[] = "Sonics SiliconBackplane rev %s";
#define SIBA_DEVDESCLEN sizeof(descfmt) + 8

static int	siba_nexus_activate_resource(device_t, device_t, int, int,
		    struct resource *);
static struct resource *
		siba_nexus_alloc_resource(device_t, device_t, int, int *,
		    rman_res_t, rman_res_t, rman_res_t, u_int);
static int	siba_nexus_attach(device_t);
#ifdef notyet
static uint8_t	siba_nexus_getirq(uint16_t);
#endif
static int	siba_nexus_probe(device_t);

struct siba_nexus_softc {
	struct siba_softc		parent_sc;

	device_t			siba_dev;	/* Device ID */
	struct bhnd_chipid		siba_cid;
	struct resource			*siba_mem_res;
	bus_space_tag_t			siba_mem_bt;
	bus_space_handle_t		siba_mem_bh;
	bus_addr_t			siba_maddr;
	bus_size_t			siba_msize;
};

// TODO - depends on bhnd(4) IRQ support
#ifdef notyet
/*
 * On the Sentry5, the system bus IRQs are the same as the
 * MIPS IRQs. Particular cores are hardwired to certain IRQ lines.
 */
static uint8_t
siba_nexus_getirq(uint16_t devid)
{
	uint8_t irq;

	switch (devid) {
	case BHND_COREID_CC:
		irq = 0;
		break;
	case BHND_COREID_ENET:
		irq = 1;
		break;
	case BHND_COREID_IPSEC:
		irq = 2;
		break;
	case BHND_COREID_USB:
		irq = 3;
		break;
	case BHND_COREID_PCI:
		irq = 4;
		break;
#if 0
	/*
	 * 5 is reserved for the MIPS on-chip timer interrupt;
	 * it is hard-wired by the tick driver.
	 */
	case BHND_COREID_MIPS:
	case BHND_COREID_MIPS33:
		irq = 5;
		break;
#endif
	default:
		irq = 0xFF;	/* this core does not need an irq */
		break;
	}

	return (irq);
}
#endif

static int
siba_nexus_probe(device_t dev)
{
	struct siba_nexus_softc *sc = device_get_softc(dev);
	struct bhnd_core_info cc;
	uint32_t idhi, idlo;
	int error, rid;

	sc->siba_dev = dev;

	//rman_debug = 1;	/* XXX */

	/*
	 * Read the ChipCommon info using the hints the kernel
	 * was compiled with.
	 */
	rid = MIPS_MEM_RID;
	sc->siba_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->siba_mem_res == NULL) {
		device_printf(dev, "unable to allocate probe aperture\n");
		return (ENXIO);
	}
	sc->siba_mem_bt = rman_get_bustag(sc->siba_mem_res);
	sc->siba_mem_bh = rman_get_bushandle(sc->siba_mem_res);
	sc->siba_maddr = rman_get_start(sc->siba_mem_res);
	sc->siba_msize = rman_get_size(sc->siba_mem_res);

	if (siba_debug) {
		device_printf(dev, "start %08x len %08x\n",
		    sc->siba_maddr, sc->siba_msize);
	}

	idlo = bus_read_4(sc->siba_mem_res, SIBA_IDLOW);
	idhi = bus_read_4(sc->siba_mem_res, SIBA_IDHIGH);
	cc = siba_parse_core_info(idhi, 0, 0);

	if (siba_debug) {
		device_printf(dev, "idhi = %08x\n", idhi);
		device_printf(dev, " chipcore id = %08x\n", cc.device);
	}

	/*
	 * For now, check that the first core is the ChipCommon core.
	 */
	if (bhnd_core_class(&cc) != BHND_DEVCLASS_CC) {
		if (siba_debug)
			device_printf(dev, "first core is not ChipCommon\n");
		return (ENXIO);
	}

	/*
	 * Determine backplane revision and set description string.
	 */
	uint32_t rev;
	char *revp;
	char descbuf[SIBA_DEVDESCLEN];

	rev = SIBA_REG_GET(idlo, IDL_SBREV);
	revp = "unknown";
	if (rev == SIBA_IDL_SBREV_2_2)
		revp = "2.2";
	else if (rev == SIBA_IDL_SBREV_2_3)
		revp = "2.3";

	(void)snprintf(descbuf, sizeof(descbuf), descfmt, revp);
	device_set_desc_copy(dev, descbuf);

	/*
	 * Determine how many cores are present on this siba bus, so
	 * that we may map them all.
	 */
	uint32_t ccidreg;

	ccidreg = bus_read_4(sc->siba_mem_res, CHIPC_ID);
	sc->siba_cid = bhnd_parse_chipid(ccidreg, sc->siba_maddr);
	if (siba_debug) {
		device_printf(dev, "ccid = %08x, cc_id = %04x, cc_rev = %04x\n",
		     ccidreg, sc->siba_cid.chip_id, sc->siba_cid.chip_rev);
	}

	if (sc->siba_cid.ncores == 0)
		sc->siba_cid.ncores = siba_get_ncores(&sc->siba_cid);

	if (siba_debug) {
		device_printf(dev, "%d cores detected.\n", sc->siba_cid.ncores);
	}

	/*
	 * Now we know how many cores are on this siba, release the
	 * mapping and allocate a new mapping spanning all cores on the bus.
	 */
	rid = MIPS_MEM_RID;
	error = bus_release_resource(dev, SYS_RES_MEMORY, rid,
	    sc->siba_mem_res);
	if (error != 0) {
		device_printf(dev, "error %d releasing resource\n", error);
		return (ENXIO);
	}

	return (0);
}

static int
siba_nexus_attach(device_t dev)
{
	struct siba_nexus_softc	*sc = device_get_softc(dev);
	uint32_t total;
	int error, rid;

	if (siba_debug)
		printf("%s: entry\n", __func__);

	/* Enumerate the bus. */
	if ((error = siba_add_children(dev, &sc->siba_cid)))
		return (error);

	/* Allocate full core aperture */
	total = sc->siba_cid.ncores;
	sc->siba_mem_res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
	    sc->siba_maddr, sc->siba_maddr + total - 1, total, RF_ACTIVE);
	if (sc->siba_mem_res == NULL) {
		device_printf(dev, "unable to allocate entire aperture\n");
		return (ENXIO);
	}
	sc->siba_mem_bt = rman_get_bustag(sc->siba_mem_res);
	sc->siba_mem_bh = rman_get_bushandle(sc->siba_mem_res);
	sc->siba_maddr = rman_get_start(sc->siba_mem_res);
	sc->siba_msize = rman_get_size(sc->siba_mem_res);

	if (siba_debug) {
		device_printf(dev, "after remapping: start %08x len %08x\n",
		    sc->siba_maddr, sc->siba_msize);
	}
	bus_set_resource(dev, SYS_RES_MEMORY, rid, sc->siba_maddr,
	    sc->siba_msize);

	/*
	 * We need a manager for the space we claim on nexus to
	 * satisfy requests from children.
	 * We need to keep the source reservation we took because
	 * otherwise it may be claimed elsewhere.
	 * XXX move to softc
	 */
	mem_rman.rm_start = sc->siba_maddr;
	mem_rman.rm_end = sc->siba_maddr + sc->siba_msize - 1;
	mem_rman.rm_type = RMAN_ARRAY;
	mem_rman.rm_descr = "SiBa I/O memory addresses";
	if (rman_init(&mem_rman) != 0 ||
	    rman_manage_region(&mem_rman, mem_rman.rm_start,
		mem_rman.rm_end) != 0) {
		panic("%s: mem_rman", __func__);
	}

	return (siba_attach(dev));
}

static const struct bhnd_chipid *
siba_nexus_get_chipid(device_t dev, device_t child) {
	struct siba_nexus_softc	*sc = device_get_softc(dev);
	return (&sc->siba_cid);
}

static struct resource *
siba_nexus_alloc_resource(device_t bus, device_t child, int type, int *rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct resource			*rv;
	struct resource_list		*rl;
	struct resource_list_entry	*rle;
	int				 isdefault, needactivate;

#if 0
	if (siba_debug)
		printf("%s: entry\n", __func__);
#endif

	isdefault = (start == 0UL && end == ~0UL && count == 1);
	needactivate = flags & RF_ACTIVE;
	rl = BUS_GET_RESOURCE_LIST(bus, child);
	rle = NULL;

	if (isdefault) {
		rle = resource_list_find(rl, type, *rid);
		if (rle == NULL)
			return (NULL);
		if (rle->res != NULL)
			panic("%s: resource entry is busy", __func__);
		start = rle->start;
		end = rle->end;
		count = rle->count;
	}

	/*
	 * If the request is for a resource which we manage,
	 * attempt to satisfy the allocation ourselves.
	 */
	if (type == SYS_RES_MEMORY &&
	    start >= mem_rman.rm_start && end <= mem_rman.rm_end) {

		rv = rman_reserve_resource(&mem_rman, start, end, count,
		    flags, child);
		if (rv == 0) {
			printf("%s: could not reserve resource\n", __func__);
			return (0);
		}

		rman_set_rid(rv, *rid);

		if (needactivate) {
			if (bus_activate_resource(child, type, *rid, rv)) {
				printf("%s: could not activate resource\n",
				    __func__);
				rman_release_resource(rv);
				return (0);
			}
		}

		return (rv);
	}

	/*
	 * Pass the request to the parent, usually MIPS nexus.
	 */
	if (siba_debug)
		printf("%s: proxying request to parent\n", __func__);
	return (resource_list_alloc(rl, bus, child, type, rid,
	    start, end, count, flags));
}

/*
 * The parent bus is responsible for resource activation; in the
 * case of MIPS, this boils down to setting the virtual address and
 * bus handle by mapping the physical address into KSEG1.
 */
static int
siba_nexus_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{
	return (BUS_ACTIVATE_RESOURCE(device_get_parent(bus), child, type,
	    rid, r));
}

// TODO - depends on bhnd(4) IRQ support
#ifdef notyet
static struct siba_devinfo *
siba_nexus_setup_devinfo(device_t dev, uint8_t idx)
{
	struct siba_nexus_softc *sc = device_get_softc(dev);
	struct siba_devinfo *sdi;
	uint32_t idlo, idhi, rev;
	uint16_t vendorid, devid;
	bus_addr_t baseaddr;

	sdi = malloc(sizeof(*sdi), M_DEVBUF, M_WAITOK | M_ZERO);
	resource_list_init(&sdi->sdi_rl);

	idlo = siba_mips_read_4(sc, idx, SIBA_IDLOW);
	idhi = siba_mips_read_4(sc, idx, SIBA_IDHIGH);

	vendorid = (idhi & SIBA_IDHIGH_VENDORMASK) >>
	    SIBA_IDHIGH_VENDOR_SHIFT;
	devid = ((idhi & 0x8ff0) >> 4);
	rev = (idhi & SIBA_IDHIGH_REVLO);
	rev |= (idhi & SIBA_IDHIGH_REVHI) >> SIBA_IDHIGH_REVHI_SHIFT;

	sdi->sdi_vid = vendorid;
	sdi->sdi_devid = devid;
	sdi->sdi_rev = rev;
	sdi->sdi_idx = idx;
	sdi->sdi_irq = siba_getirq(devid);

	/*
	 * Determine memory window on bus and irq if one is needed.
	 */
	baseaddr = sc->siba_maddr + (idx * SIBA_CORE_SIZE);
	resource_list_add(&sdi->sdi_rl, SYS_RES_MEMORY,
	    MIPS_MEM_RID, /* XXX */
	    baseaddr, baseaddr + SIBA_CORE_LEN - 1, SIBA_CORE_LEN);

	if (sdi->sdi_irq != 0xff) {
		resource_list_add(&sdi->sdi_rl, SYS_RES_IRQ,
		    0, sdi->sdi_irq, sdi->sdi_irq, 1);
	}

	return (sdi);
}
#endif

static device_method_t siba_nexus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_attach,	siba_nexus_attach),
	DEVMETHOD(device_probe,		siba_nexus_probe),

	/* Bus interface */
	DEVMETHOD(bus_activate_resource,siba_nexus_activate_resource),
	DEVMETHOD(bus_alloc_resource,	siba_nexus_alloc_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	/* bhnd interface */
	DEVMETHOD(bhnd_get_chipid,	siba_nexus_get_chipid),

	DEVMETHOD_END
};

DEFINE_CLASS_1(bhnd, siba_nexus_driver, siba_nexus_methods,
    sizeof(struct siba_nexus_softc), siba_driver);

DRIVER_MODULE(siba_nexus, nexus, siba_driver, bhnd_devclass, 0, 0);
