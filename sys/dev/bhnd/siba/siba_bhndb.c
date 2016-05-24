/*-
 * Copyright (c) 2015 Landon Fuller <landon@landonf.org>
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
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/systm.h>

#include <dev/bhnd/bhnd_ids.h>
#include <dev/bhnd/bhndb/bhndbvar.h>
#include <dev/bhnd/bhndb/bhndb_hwdata.h>

#include "sibareg.h"
#include "sibavar.h"

/*
 * Supports attachment of siba(4) bus devices via a bhndb bridge.
 */

//
// TODO: PCI rev < 6 interrupt handling
//
// On early PCI cores (rev < 6) interrupt masking is handled via interconnect
// configuration registers (SBINTVEC), rather than the PCI_INT_MASK
// config register.
//
// On those devices, we should handle interrupts locally using SBINTVEC, rather
// than delegating to our parent bhndb device.
//

static int	siba_bhndb_wars_hwup(struct siba_softc *sc);

/* Bridge-specific core device quirks */
enum {
	/** When PCIe-bridged, the D11 core's initiator request
	 *  timeout must be disabled to prevent D11 from entering a
	 *  RESP_TIMEOUT error state. */
	SIBA_QUIRK_PCIE_D11_SB_TIMEOUT	= (1<<0)
};

static struct bhnd_device_quirk bridge_quirks[] = {
	BHND_CHIP_QUIRK(4311, HWREV_EQ(2), SIBA_QUIRK_PCIE_D11_SB_TIMEOUT),
	BHND_CHIP_QUIRK(4312, HWREV_EQ(0), SIBA_QUIRK_PCIE_D11_SB_TIMEOUT),
};

static struct bhnd_device bridge_devs[] = {
	BHND_DEVICE(PCI, NULL, bridge_quirks),
};

static int
siba_bhndb_probe(device_t dev)
{
	const struct bhnd_chipid *cid;

	/* Check bus type */
	cid = BHNDB_GET_CHIPID(device_get_parent(dev), dev);
	if (cid->chip_type != BHND_CHIPTYPE_SIBA)
		return (ENXIO);

	/* Delegate to default probe implementation */
	return (siba_probe(dev));
}

static int
siba_bhndb_attach(device_t dev)
{
	struct siba_softc		*sc;
	const struct bhnd_chipid	*chipid;
	int				 error;

	sc = device_get_softc(dev);

	/* Enumerate our children. */
	chipid = BHNDB_GET_CHIPID(device_get_parent(dev), dev);
	if ((error = siba_add_children(dev, chipid)))
		return (error);

	/* Initialize full bridge configuration */
	error = BHNDB_INIT_FULL_CONFIG(device_get_parent(dev), dev,
	    bhndb_siba_priority_table);
	if (error)
		return (error);

	/* Ask our parent bridge to find the corresponding bridge core */
	sc->hostb_dev = BHNDB_FIND_HOSTB_DEVICE(device_get_parent(dev), dev);

	/* Call our superclass' implementation */
	if ((error = siba_attach(dev)))
		return (error);

	/* Apply attach/resume work-arounds */
	if ((error = siba_bhndb_wars_hwup(sc)))
		return (error);

	return (0);
}

static int
siba_bhndb_resume(device_t dev)
{
	struct siba_softc	*sc;
	int			 error;

	sc = device_get_softc(dev);

	/* Apply attach/resume work-arounds */
	if ((error = siba_bhndb_wars_hwup(sc)))
		return (error);

	/* Call our superclass' implementation */
	return (siba_resume(dev));
}

/* Suspend all references to the device's cfg register blocks */
static void
siba_bhndb_suspend_cfgblocks(device_t dev, struct siba_devinfo *dinfo) {
	for (u_int i = 0; i < dinfo->core_id.num_cfg_blocks; i++) {
		if (dinfo->cfg[i] == NULL)
			continue;

		BHNDB_SUSPEND_RESOURCE(device_get_parent(dev), dev,
		    SYS_RES_MEMORY, dinfo->cfg[i]->res);
	}
}

static int
siba_bhndb_suspend_child(device_t dev, device_t child)
{
	struct siba_devinfo	*dinfo;
	int			 error;

	if (device_get_parent(child) != dev)
		BUS_SUSPEND_CHILD(device_get_parent(dev), child);

	dinfo = device_get_ivars(child);

	/* Suspend the child */
	if ((error = bhnd_generic_br_suspend_child(dev, child)))
		return (error);

	/* Suspend resource references to the child's config registers */
	siba_bhndb_suspend_cfgblocks(dev, dinfo);
	
	return (0);
}

static int
siba_bhndb_resume_child(device_t dev, device_t child)
{
	struct siba_devinfo	*dinfo;
	int			 error;

	if (device_get_parent(child) != dev)
		BUS_SUSPEND_CHILD(device_get_parent(dev), child);

	if (!device_is_suspended(child))
		return (EBUSY);

	dinfo = device_get_ivars(child);

	/* Resume all resource references to the child's config registers */
	for (u_int i = 0; i < dinfo->core_id.num_cfg_blocks; i++) {
		if (dinfo->cfg[i] == NULL)
			continue;

		error = BHNDB_RESUME_RESOURCE(device_get_parent(dev), dev,
		    SYS_RES_MEMORY, dinfo->cfg[i]->res);
		if (error) {
			siba_bhndb_suspend_cfgblocks(dev, dinfo);
			return (error);
		}
	}

	/* Resume the child */
	if ((error = bhnd_generic_br_resume_child(dev, child))) {
		siba_bhndb_suspend_cfgblocks(dev, dinfo);
		return (error);
	}

	return (0);
}

/* Work-around implementation for SIBA_QUIRK_PCIE_D11_SB_TIMEOUT */
static int
siba_bhndb_wars_pcie_clear_d11_timeout(struct siba_softc *sc)
{
	struct siba_devinfo	*dinfo;
	device_t		 d11;
	uint32_t		 imcfg;

	/* Only applies when bridged by PCIe */
	if (bhnd_get_class(sc->hostb_dev) != BHND_DEVCLASS_PCIE)
		return (0);

	/* Only applies if there's a D11 core */
	d11 = bhnd_match_child(sc->dev, &(struct bhnd_core_match) {
		BHND_MATCH_CORE(BHND_MFGID_BCM, BHND_COREID_D11),
		BHND_MATCH_CORE_UNIT(0)
	});
	if (d11 == NULL)
		return (0);

	/* Clear initiator timeout in D11's CFG0 block */
	dinfo = device_get_ivars(d11);
	KASSERT(dinfo->cfg[0] != NULL, ("missing core config mapping"));

	imcfg = bhnd_bus_read_4(dinfo->cfg[0], SIBA_CFG0_IMCONFIGLOW);
	imcfg &= ~SIBA_IMCL_RTO_MASK;

	bhnd_bus_write_4(dinfo->cfg[0], SIBA_CFG0_IMCONFIGLOW, imcfg);

	return (0);
}

/**
 * Apply any hardware workarounds that are required upon attach or resume
 * of the bus.
 */
static int
siba_bhndb_wars_hwup(struct siba_softc *sc)
{
	uint32_t		 quirks;
	int			 error;

	quirks = bhnd_device_quirks(sc->hostb_dev, bridge_devs,
	    sizeof(bridge_devs[0]));

	if (quirks & SIBA_QUIRK_PCIE_D11_SB_TIMEOUT) {
		if ((error = siba_bhndb_wars_pcie_clear_d11_timeout(sc)))
			return (error);
	}

	return (0);
}


static device_method_t siba_bhndb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			siba_bhndb_probe),
	DEVMETHOD(device_attach,		siba_bhndb_attach),
	DEVMETHOD(device_resume,		siba_bhndb_resume),

	/* Bus interface */
	DEVMETHOD(bus_suspend_child,		siba_bhndb_suspend_child),
	DEVMETHOD(bus_resume_child,		siba_bhndb_resume_child),

	DEVMETHOD_END
};

DEFINE_CLASS_2(bhnd, siba_bhndb_driver, siba_bhndb_methods,
    sizeof(struct siba_softc), bhnd_bhndb_driver, siba_driver);

DRIVER_MODULE(siba_bhndb, bhndb, siba_bhndb_driver, bhnd_devclass, NULL, NULL);
 
MODULE_VERSION(siba_bhndb, 1);
MODULE_DEPEND(siba_bhndb, siba, 1, 1, 1);
MODULE_DEPEND(siba_bhndb, bhnd, 1, 1, 1);
MODULE_DEPEND(siba_bhndb, bhndb, 1, 1, 1);
