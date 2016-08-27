/*-
 * Copyright (c) 2015-2016 Landon Fuller <landon@landonf.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/bhnd/bhnd.h>

#include "bhnd_nvram_map.h"

#include "bhnd_pmureg.h"
#include "bhnd_pmuvar.h"

#include "bhnd_pmu_private.h"

/*
 * Broadcom PMU driver.
 * 
 * On modern BHND chipsets, the PMU, GCI, and SRENG (Save/Restore Engine?)
 * register blocks are found within a dedicated PMU core (attached via
 * the AHB 'always on bus').
 * 
 * On earlier chipsets, these register blocks are found at the same
 * offsets within the ChipCommon core.
 */

devclass_t bhnd_pmu_devclass;	/**< bhnd(4) PMU device class */

static int	bhnd_pmu_sysctl_bus_freq(SYSCTL_HANDLER_ARGS);
static int	bhnd_pmu_sysctl_cpu_freq(SYSCTL_HANDLER_ARGS);
static int	bhnd_pmu_sysctl_mem_freq(SYSCTL_HANDLER_ARGS);

#define	BPMU_CLKCTL_READ_4(_pinfo)		\
	bhnd_bus_read_4((_pinfo)->pm_res, (_pinfo)->pm_regs)

#define	BPMU_CLKCTL_WRITE_4(_pinfo, _val)	\
	bhnd_bus_write_4((_pinfo)->pm_res, (_pinfo)->pm_regs, (_val))
	
#define	BPMU_CLKCTL_SET_4(_pinfo, _val, _mask)	\
	BPMU_CLKCTL_WRITE_4((_pinfo),		\
	    ((_val) & (_mask)) | (BPMU_CLKCTL_READ_4(_pinfo) & ~(_mask)))

/**
 * Default bhnd_pmu driver implementation of DEVICE_PROBE().
 */
int
bhnd_pmu_probe(device_t dev)
{
	return (BUS_PROBE_DEFAULT);
}

/**
 * Default bhnd_pmu driver implementation of DEVICE_ATTACH().
 * 
 * @param dev PMU device.
 * @param res The PMU device registers. The driver will maintain a borrowed
 * reference to this resource for the lifetime of the device.
 */
int
bhnd_pmu_attach(device_t dev, struct bhnd_resource *res)
{
	struct bhnd_pmu_softc	*sc;
	struct sysctl_ctx_list	*ctx;
	struct sysctl_oid	*tree;
	devclass_t		 bhnd_class;
	device_t		 core, bus;
	int			 error;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->quirks = 0;
	sc->res = res;

	/* Fetch capability flags */
	sc->caps = bhnd_bus_read_4(sc->res, BHND_PMU_CAP);

	/* Find the bus-attached core */
	bhnd_class = devclass_find("bhnd");
	core = sc->dev;
	while ((bus = device_get_parent(core)) != NULL) {
		if (device_get_devclass(bus) == bhnd_class)
			break;

		core = bus;
	}

	if (core == NULL) {
		device_printf(sc->dev, "bhnd bus not found\n");
		return (ENXIO);
	}

	/* Fetch chip and board info */
	sc->cid = *bhnd_get_chipid(core);

	if ((error = bhnd_read_board_info(core, &sc->board))) {
		device_printf(sc->dev, "error fetching board info: %d\n",
		    error);
		return (ENXIO);
	}

	/* Locate ChipCommon device */
	sc->chipc_dev = bhnd_find_child(bus, BHND_DEVCLASS_CC, 0);
	if (sc->chipc_dev == NULL) {
		device_printf(sc->dev, "chipcommon device not found\n");
		return (ENXIO);
	}

	BPMU_LOCK_INIT(sc);

	/* Set quirk flags */
	switch (sc->cid.chip_id) {
	case BHND_CHIPID_BCM4328:
	case BHND_CHIPID_BCM5354:
		/* HTAVAIL/ALPAVAIL are bitswapped in CLKCTL */
		sc->quirks |= BPMU_QUIRK_CLKCTL_CCS0;
		break;
	default:
		break;
	}

	/* Initialize PMU */
	if ((error = bhnd_pmu_init(sc))) {
		device_printf(sc->dev, "PMU init failed: %d\n", error);
		goto failed;
	}

	/* Set up sysctl nodes */
	ctx = device_get_sysctl_ctx(dev);
	tree = device_get_sysctl_tree(dev);

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "bus_freq", CTLTYPE_UINT | CTLFLAG_RD, sc, 0,
	    bhnd_pmu_sysctl_bus_freq, "IU", "Bus clock frequency");

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "cpu_freq", CTLTYPE_UINT | CTLFLAG_RD, sc, 0,
	    bhnd_pmu_sysctl_cpu_freq, "IU", "CPU clock frequency");
	
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "mem_freq", CTLTYPE_UINT | CTLFLAG_RD, sc, 0,
	    bhnd_pmu_sysctl_mem_freq, "IU", "Memory clock frequency");

	return (0);

failed:
	BPMU_LOCK_DESTROY(sc);
	return (error);
}

/**
 * Default bhnd_pmu driver implementation of DEVICE_DETACH().
 */
int
bhnd_pmu_detach(device_t dev)
{
	struct bhnd_pmu_softc	*sc;

	sc = device_get_softc(dev);

	BPMU_LOCK_DESTROY(sc);

	return (0);
}

/**
 * Default bhnd_pmu driver implementation of DEVICE_SUSPEND().
 */
int
bhnd_pmu_suspend(device_t dev)
{
	return (0);
}

/**
 * Default bhnd_pmu driver implementation of DEVICE_RESUME().
 */
int
bhnd_pmu_resume(device_t dev)
{
	struct bhnd_pmu_softc	*sc;
	int			 error;

	sc = device_get_softc(dev);

	/* Re-initialize PMU */
	if ((error = bhnd_pmu_init(sc))) {
		device_printf(sc->dev, "PMU init failed: %d\n", error);
		return (error);
	}

	return (0);
}

static int
bhnd_pmu_sysctl_bus_freq(SYSCTL_HANDLER_ARGS)
{
	struct bhnd_pmu_softc	*sc;
	uint32_t		 freq;
	
	sc = arg1;

	BPMU_LOCK(sc);
	freq = bhnd_pmu_si_clock(sc);
	BPMU_UNLOCK(sc);

	return (sysctl_handle_32(oidp, NULL, freq, req));
}

static int
bhnd_pmu_sysctl_cpu_freq(SYSCTL_HANDLER_ARGS)
{
	struct bhnd_pmu_softc	*sc;
	uint32_t		 freq;
	
	sc = arg1;

	BPMU_LOCK(sc);
	freq = bhnd_pmu_cpu_clock(sc);
	BPMU_UNLOCK(sc);

	return (sysctl_handle_32(oidp, NULL, freq, req));
}

static int
bhnd_pmu_sysctl_mem_freq(SYSCTL_HANDLER_ARGS)
{
	struct bhnd_pmu_softc	*sc;
	uint32_t		 freq;
	
	sc = arg1;

	BPMU_LOCK(sc);
	freq = bhnd_pmu_mem_clock(sc);
	BPMU_UNLOCK(sc);

	return (sysctl_handle_32(oidp, NULL, freq, req));
}

static int
bhnd_pmu_core_req_clock(device_t dev, struct bhnd_core_pmu_info *pinfo,
    bhnd_clock clock)
{
	struct bhnd_pmu_softc	*sc;
	uint32_t		 avail;
	uint32_t		 req;

	sc = device_get_softc(dev);

	avail = 0x0;
	req = 0x0;

	switch (clock) {
	case BHND_CLOCK_DYN:
		break;
	case BHND_CLOCK_ILP:
		req |= BHND_CCS_FORCEILP;
		break;
	case BHND_CLOCK_ALP:
		req |= BHND_CCS_FORCEALP;
		avail |= BHND_CCS_ALPAVAIL;
		break;
	case BHND_CLOCK_HT:
		req |= BHND_CCS_FORCEHT;
		avail |= BHND_CCS_HTAVAIL;
		break;
	default:
		device_printf(dev, "%s requested unknown clock: %#x\n",
		    device_get_nameunit(pinfo->pm_dev), clock);
		return (ENODEV);
	}

	BPMU_LOCK(sc);

	/* Issue request */
	BPMU_CLKCTL_SET_4(pinfo, req, BHND_CCS_FORCE_MASK);

	/* Wait for clock availability */
	bhnd_pmu_wait_clkst(sc, pinfo->pm_dev, pinfo->pm_res, pinfo->pm_regs,
	    avail, avail);

	BPMU_UNLOCK(sc);

	return (0);
}

static int
bhnd_pmu_core_en_clocks(device_t dev, struct bhnd_core_pmu_info *pinfo,
    uint32_t clocks)
{
	struct bhnd_pmu_softc	*sc;
	uint32_t		 avail;
	uint32_t		 req;

	sc = device_get_softc(dev);

	avail = 0x0;
	req = 0x0;

	/* Build clock request flags */
	if (clocks & BHND_CLOCK_DYN)		/* nothing to enable */
		clocks &= ~BHND_CLOCK_DYN;

	if (clocks & BHND_CLOCK_ILP)		/* nothing to enable */
		clocks &= ~BHND_CLOCK_ILP;

	if (clocks & BHND_CLOCK_ALP) {
		req |= BHND_CCS_ALPAREQ;
		avail |= BHND_CCS_ALPAVAIL;
		clocks &= ~BHND_CLOCK_ALP;
	}

	if (clocks & BHND_CLOCK_HT) {
		req |= BHND_CCS_HTAREQ;
		avail |= BHND_CCS_HTAVAIL;
		clocks &= ~BHND_CLOCK_HT;
	}

	/* Check for unknown clock values */
	if (clocks != 0x0) {
		device_printf(dev, "%s requested unknown clocks: %#x\n",
		    device_get_nameunit(pinfo->pm_dev), clocks);
		return (ENODEV);
	}

	BPMU_LOCK(sc);

	/* Issue request */
	BPMU_CLKCTL_SET_4(pinfo, req, BHND_CCS_AREQ_MASK);

	/* Wait for clock availability */
	bhnd_pmu_wait_clkst(sc, pinfo->pm_dev, pinfo->pm_res, pinfo->pm_regs,
	    avail, avail);

	BPMU_UNLOCK(sc);

	return (0);
}

static int
bhnd_pmu_core_req_ext_rsrc(device_t dev, struct bhnd_core_pmu_info *pinfo,
    u_int rsrc)
{
	struct bhnd_pmu_softc	*sc;
	uint32_t		 req;
	uint32_t		 avail;

	sc = device_get_softc(dev);

	if (rsrc > BHND_CCS_ERSRC_MAX)
		return (EINVAL);

	req = BHND_PMU_SET_BITS((1<<rsrc), BHND_CCS_ERSRC_REQ);
	avail = BHND_PMU_SET_BITS((1<<rsrc), BHND_CCS_ERSRC_STS);

	BPMU_LOCK(sc);

	/* Write request */
	BPMU_CLKCTL_SET_4(pinfo, req, req);

	/* Wait for resource availability */
	bhnd_pmu_wait_clkst(sc, pinfo->pm_dev, pinfo->pm_res, pinfo->pm_regs,
	    avail, avail);

	BPMU_UNLOCK(sc);

	return (0);	
}

static int
bhnd_pmu_core_release_ext_rsrc(device_t dev, struct bhnd_core_pmu_info *pinfo,
    u_int rsrc)
{
	struct bhnd_pmu_softc	*sc;
	uint32_t		 mask;

	sc = device_get_softc(dev);

	if (rsrc > BHND_CCS_ERSRC_MAX)
		return (EINVAL);

	mask = BHND_PMU_SET_BITS((1<<rsrc), BHND_CCS_ERSRC_REQ);

	/* Clear request */
	BPMU_LOCK(sc);
	BPMU_CLKCTL_SET_4(pinfo, 0x0, mask);
	BPMU_UNLOCK(sc);

	return (0);	
}

static int
bhnd_pmu_core_release(device_t dev, struct bhnd_core_pmu_info *pinfo)
{
	struct bhnd_pmu_softc	*sc;

	sc = device_get_softc(dev);

	BPMU_LOCK(sc);

	/* Clear all FORCE, AREQ, and ERSRC flags */
	BPMU_CLKCTL_SET_4(pinfo, 0x0,
	    BHND_CCS_FORCE_MASK | BHND_CCS_AREQ_MASK | BHND_CCS_ERSRC_REQ_MASK);

	BPMU_UNLOCK(sc);

	return (0);
}

static device_method_t bhnd_pmu_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			bhnd_pmu_probe),
	DEVMETHOD(device_detach,		bhnd_pmu_detach),
	DEVMETHOD(device_suspend,		bhnd_pmu_suspend),
	DEVMETHOD(device_resume,		bhnd_pmu_resume),

	/* BHND PMU interface */
	DEVMETHOD(bhnd_pmu_core_req_clock,	bhnd_pmu_core_req_clock),
	DEVMETHOD(bhnd_pmu_core_en_clocks,	bhnd_pmu_core_en_clocks),
	DEVMETHOD(bhnd_pmu_core_req_ext_rsrc,	bhnd_pmu_core_req_ext_rsrc),
	DEVMETHOD(bhnd_pmu_core_release_ext_rsrc, bhnd_pmu_core_release_ext_rsrc),
	DEVMETHOD(bhnd_pmu_core_release,	bhnd_pmu_core_release),

	DEVMETHOD_END
};

DEFINE_CLASS_0(bhnd_pmu, bhnd_pmu_driver, bhnd_pmu_methods, sizeof(struct bhnd_pmu_softc));
MODULE_VERSION(bhnd_pmu, 1);
