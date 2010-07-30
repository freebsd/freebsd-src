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

#include <dev/siba/siba_ids.h>
#include <dev/siba/sibareg.h>
#include <dev/siba/sibavar.h>

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

/*
 * Device identifiers and descriptions.
 */
static struct siba_devid siba_devids[] = {
	{ SIBA_VID_BROADCOM,	SIBA_DEVID_CHIPCOMMON,	SIBA_REV_ANY,
	  "ChipCommon" },
	{ SIBA_VID_BROADCOM,	SIBA_DEVID_SDRAM,	SIBA_REV_ANY,
	  "SDRAM controller" },
	{ SIBA_VID_BROADCOM,	SIBA_DEVID_PCI,		SIBA_REV_ANY,
	  "PCI host interface" },
	{ SIBA_VID_BROADCOM,	SIBA_DEVID_MIPS,	SIBA_REV_ANY,
	  "MIPS core" },
	{ SIBA_VID_BROADCOM,	SIBA_DEVID_ETHERNET,	SIBA_REV_ANY,
	  "Ethernet core" },
	{ SIBA_VID_BROADCOM,	SIBA_DEVID_USB11_HOSTDEV, SIBA_REV_ANY,
	  "USB host controller" },
	{ SIBA_VID_BROADCOM,	SIBA_DEVID_IPSEC,	SIBA_REV_ANY,
	  "IPSEC accelerator" },
	{ SIBA_VID_BROADCOM,	SIBA_DEVID_SDRAMDDR,	SIBA_REV_ANY,
	  "SDRAM/DDR controller" },
	{ SIBA_VID_BROADCOM,	SIBA_DEVID_MIPS_3302,	SIBA_REV_ANY,
	  "MIPS 3302 core" },
	{ 0, 0, 0, NULL }
};

static int	siba_activate_resource(device_t, device_t, int, int,
		    struct resource *);
static device_t	siba_add_child(device_t, int, const char *, int);
static struct resource *
		siba_alloc_resource(device_t, device_t, int, int *, u_long,
		    u_long, u_long, u_int);
static int	siba_attach(device_t);
#ifdef notyet
static void	siba_destroy_devinfo(struct siba_devinfo *);
#endif
static struct siba_devid *
		siba_dev_match(uint16_t, uint16_t, uint8_t);
static struct resource_list *
		siba_get_reslist(device_t, device_t);
static uint8_t	siba_getirq(uint16_t);
static int	siba_print_all_resources(device_t dev);
static int	siba_print_child(device_t, device_t);
static int	siba_probe(device_t);
static void	siba_probe_nomatch(device_t, device_t);
int		siba_read_ivar(device_t, device_t, int, uintptr_t *);
static struct siba_devinfo *
		siba_setup_devinfo(device_t, uint8_t);
int		siba_write_ivar(device_t, device_t, int, uintptr_t);
uint8_t		siba_getncores(device_t, uint16_t);

/*
 * On the Sentry5, the system bus IRQs are the same as the
 * MIPS IRQs. Particular cores are hardwired to certain IRQ lines.
 */
static uint8_t
siba_getirq(uint16_t devid)
{
	uint8_t irq;

	switch (devid) {
	case SIBA_DEVID_CHIPCOMMON:
		irq = 0;
		break;
	case SIBA_DEVID_ETHERNET:
		irq = 1;
		break;
	case SIBA_DEVID_IPSEC:
		irq = 2;
		break;
	case SIBA_DEVID_USB11_HOSTDEV:
		irq = 3;
		break;
	case SIBA_DEVID_PCI:
		irq = 4;
		break;
#if 0
	/*
	 * 5 is reserved for the MIPS on-chip timer interrupt;
	 * it is hard-wired by the tick driver.
	 */
	case SIBA_DEVID_MIPS:
	case SIBA_DEVID_MIPS_3302:
		irq = 5;
		break;
#endif
	default:
		irq = 0xFF;	/* this core does not need an irq */
		break;
	}

	return (irq);
}

static int
siba_probe(device_t dev)
{
	struct siba_softc *sc = device_get_softc(dev);
	uint32_t idlo, idhi;
	uint16_t ccid;
	int rid;

	sc->siba_dev = dev;

	//rman_debug = 1;	/* XXX */

	/*
	 * Map the ChipCommon register set using the hints the kernel
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

	idlo = siba_mips_read_4(sc, 0, SIBA_IDLOW);
	idhi = siba_mips_read_4(sc, 0, SIBA_IDHIGH);
	ccid = ((idhi & 0x8ff0) >> 4);
	if (siba_debug) {
		device_printf(dev, "idlo = %08x\n", idlo);
		device_printf(dev, "idhi = %08x\n", idhi);
		device_printf(dev, " chipcore id = %08x\n", ccid);
	}

	/*
	 * For now, check that the first core is the ChipCommon core.
	 */
	if (ccid != SIBA_DEVID_CHIPCOMMON) {
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

	rev = idlo & 0xF0000000;
	revp = "unknown";
	if (rev == 0x00000000)
		revp = "2.2";
	else if (rev == 0x10000000)
		revp = "2.3";

	(void)snprintf(descbuf, sizeof(descbuf), descfmt, revp);
	device_set_desc_copy(dev, descbuf);

	/*
	 * Determine how many cores are present on this siba bus, so
	 * that we may map them all.
	 */
	uint32_t ccidreg;
	uint16_t cc_id;
	uint16_t cc_rev;

	ccidreg = siba_mips_read_4(sc, 0, SIBA_CC_CHIPID);
	cc_id = (ccidreg & SIBA_CC_IDMASK);
	cc_rev = (ccidreg & SIBA_CC_REVMASK) >> SIBA_CC_REVSHIFT;
	if (siba_debug) {
		device_printf(dev, "ccid = %08x, cc_id = %04x, cc_rev = %04x\n",
		     ccidreg, cc_id, cc_rev);
	}

	sc->siba_ncores = siba_getncores(dev, cc_id);
	if (siba_debug) {
		device_printf(dev, "%d cores detected.\n", sc->siba_ncores);
	}

	/*
	 * Now we know how many cores are on this siba, release the
	 * mapping and allocate a new mapping spanning all cores on the bus.
	 */
	rid = MIPS_MEM_RID;
	int result;
	result = bus_release_resource(dev, SYS_RES_MEMORY, rid,
	    sc->siba_mem_res);
	if (result != 0) {
		device_printf(dev, "error %d releasing resource\n", result);
		return (ENXIO);
	}

	uint32_t total;
	total = sc->siba_ncores * SIBA_CORE_LEN;

	/* XXX Don't allocate the entire window until we
	 * enumerate the bus. Once the bus has been enumerated,
	 * and instance variables/children instantiated + populated,
	 * release the resource so children may attach.
	 */
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

	return (0);
}

static int
siba_attach(device_t dev)
{
	struct siba_softc	*sc = device_get_softc(dev);
	struct siba_devinfo	*sdi;
	device_t		 child;
	int			 idx;

	if (siba_debug)
		printf("%s: entry\n", __func__);

	bus_generic_probe(dev);

	/*
	 * Now that all bus space is mapped and visible to the CPU,
	 * enumerate its children.
	 * NB: only one core may be mapped at any time if the siba bus
	 * is the child of a PCI or PCMCIA bus.
	 */
	for (idx = 0; idx < sc->siba_ncores; idx++) {
		sdi = siba_setup_devinfo(dev, idx);
		child = device_add_child(dev, NULL, -1);
		if (child == NULL)
			panic("%s: device_add_child() failed\n", __func__);
		device_set_ivars(child, sdi);
	}

	return (bus_generic_attach(dev));
}

static struct siba_devid *
siba_dev_match(uint16_t vid, uint16_t devid, uint8_t rev)
{
	size_t			 i, bound;
	struct siba_devid	*sd;

	bound = sizeof(siba_devids) / sizeof(struct siba_devid);
	sd = &siba_devids[0];
	for (i = 0; i < bound; i++, sd++) {
		if (((vid == SIBA_VID_ANY) || (vid == sd->sd_vendor)) &&
		    ((devid == SIBA_DEVID_ANY) || (devid == sd->sd_device)) &&
		    ((rev == SIBA_REV_ANY) || (rev == sd->sd_rev) ||
		     (sd->sd_rev == SIBA_REV_ANY)))
			break;
	}
	if (i == bound)
		sd = NULL;

	return (sd);
}

static int
siba_print_child(device_t bus, device_t child)
{
	int retval = 0;

	retval += bus_print_child_header(bus, child);
	retval += siba_print_all_resources(child);
	if (device_get_flags(child))
		retval += printf(" flags %#x", device_get_flags(child));
	retval += printf(" on %s\n", device_get_nameunit(bus));

	return (retval);
}

static struct resource *
siba_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
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
siba_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{

	return (BUS_ACTIVATE_RESOURCE(device_get_parent(bus), child, type,
	    rid, r));
}

static struct siba_devinfo *
siba_setup_devinfo(device_t dev, uint8_t idx)
{
	struct siba_softc *sc = device_get_softc(dev);
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
	baseaddr = sc->siba_maddr + (idx * SIBA_CORE_LEN);
	resource_list_add(&sdi->sdi_rl, SYS_RES_MEMORY,
	    MIPS_MEM_RID, /* XXX */
	    baseaddr, baseaddr + SIBA_CORE_LEN - 1, SIBA_CORE_LEN);

	if (sdi->sdi_irq != 0xff) {
		resource_list_add(&sdi->sdi_rl, SYS_RES_IRQ,
		    0, sdi->sdi_irq, sdi->sdi_irq, 1);
	}

	return (sdi);
}

#ifdef notyet
static void
siba_destroy_devinfo(struct siba_devinfo *sdi)
{

	resource_list_free(&sdi->sdi_rl);
	free(sdi, M_DEVBUF);
}
#endif

/* XXX is this needed? */
static device_t
siba_add_child(device_t dev, int order, const char *name, int unit)
{
#if 1

	device_printf(dev, "%s: entry\n", __func__);
	return (NULL);
#else
	device_t child;
	struct siba_devinfo *sdi;

	child = device_add_child_ordered(dev, order, name, unit);
	if (child == NULL)
		return (NULL);

	sdi = malloc(sizeof(struct siba_devinfo), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (sdi == NULL)
		return (NULL);

	device_set_ivars(child, sdi);
	return (child);
#endif
}

int
siba_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct siba_devinfo *sdi;

	sdi = device_get_ivars(child);

	switch (which) {
	case SIBA_IVAR_VENDOR:
		*result = sdi->sdi_vid;
		break;
	case SIBA_IVAR_DEVICE:
		*result = sdi->sdi_devid;
		break;
	case SIBA_IVAR_REVID:
		*result = sdi->sdi_rev;
		break;
	case SIBA_IVAR_CORE_INDEX:
		*result = sdi->sdi_idx;
		break;
	default:
		return (ENOENT);
	}

	return (0);
}

int
siba_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{

	return (EINVAL);
}

static void
siba_probe_nomatch(device_t dev, device_t child)
{

	/*
	 * Announce devices which weren't attached after we probed the bus.
	 */
	if (siba_debug) {
		struct siba_devid *sd;

		sd = siba_dev_match(siba_get_vendor(child),
		    siba_get_device(child), SIBA_REV_ANY);
		if (sd != NULL && sd->sd_desc != NULL) {
			device_printf(dev, "<%s> "
			    "at device %d (no driver attached)\n",
			    sd->sd_desc, siba_get_core_index(child));
		} else {
			device_printf(dev, "<0x%04x, 0x%04x> "
			    "at device %d (no driver attached)\n",
			    siba_get_vendor(child), siba_get_device(child),
			    siba_get_core_index(child));
		}
	}
}

static int
siba_print_all_resources(device_t dev)
{
	struct siba_devinfo *sdi = device_get_ivars(dev);
	struct resource_list *rl = &sdi->sdi_rl;
	int retval = 0;

	if (STAILQ_FIRST(rl))
		retval += printf(" at");

	retval += resource_list_print_type(rl, "mem", SYS_RES_MEMORY, "%#lx");
	retval += resource_list_print_type(rl, "irq", SYS_RES_IRQ, "%ld");

	return (retval);
}

static struct resource_list *
siba_get_reslist(device_t dev, device_t child)
{
	struct siba_devinfo *sdi = device_get_ivars(child);

	return (&sdi->sdi_rl);
}

static device_method_t siba_methods[] = {
	/* Device interface */
	DEVMETHOD(device_attach,	siba_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_probe,		siba_probe),
	DEVMETHOD(device_resume,	bus_generic_resume),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),

	/* Bus interface */
	DEVMETHOD(bus_activate_resource,siba_activate_resource),
	DEVMETHOD(bus_add_child,	siba_add_child),
	DEVMETHOD(bus_alloc_resource,	siba_alloc_resource),
	DEVMETHOD(bus_get_resource_list,siba_get_reslist),
	DEVMETHOD(bus_print_child,	siba_print_child),
	DEVMETHOD(bus_probe_nomatch,	siba_probe_nomatch),
	DEVMETHOD(bus_read_ivar,	siba_read_ivar),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_write_ivar,	siba_write_ivar),

	KOBJMETHOD_END
};

static driver_t siba_driver = {
	"siba",
	siba_methods,
	sizeof(struct siba_softc),
};
static devclass_t siba_devclass;

DRIVER_MODULE(siba, nexus, siba_driver, siba_devclass, 0, 0);
