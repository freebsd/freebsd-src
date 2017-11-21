/*-
 * Copyright (c) 2015-2016 Landon Fuller <landon@landonf.org>
 * Copyright (c) 2017 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Landon Fuller
 * under sponsorship from the FreeBSD Foundation.
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

/*
 * PCI-specific implementation for the BHNDB bridge driver.
 * 
 * Provides support for bridging from a PCI parent bus to a BHND-compatible
 * bus (e.g. bcma or siba) via a Broadcom PCI core configured in end-point
 * mode.
 * 
 * This driver handles all initial generic host-level PCI interactions with a
 * PCI/PCIe bridge core operating in endpoint mode. Once the bridged bhnd(4)
 * bus has been enumerated, this driver works in tandem with a core-specific
 * bhnd_pci_hostb driver to manage the PCI core.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/systm.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/bhnd/bhnd.h>
#include <dev/bhnd/bhndreg.h>

#include <dev/bhnd/bhnd_erom.h>
#include <dev/bhnd/bhnd_eromvar.h>

#include <dev/bhnd/siba/sibareg.h>

#include <dev/bhnd/cores/pci/bhnd_pcireg.h>

#include "bhndb_pcireg.h"
#include "bhndb_pcivar.h"
#include "bhndb_private.h"

struct bhndb_pci_eio;

static int		bhndb_pci_alloc_msi(struct bhndb_pci_softc *sc,
			    int *msi_count);
static int		bhndb_pci_read_core_table(device_t dev,
			    struct bhnd_chipid *chipid,
			    struct bhnd_core_info **cores, u_int *ncores,
			    bhnd_erom_class_t **eromcls);
static int		bhndb_pci_add_children(struct bhndb_pci_softc *sc);

static bhnd_devclass_t	bhndb_expected_pci_devclass(device_t dev);
static bool		bhndb_is_pcie_attached(device_t dev);

static int		bhndb_enable_pci_clocks(device_t dev);
static int		bhndb_disable_pci_clocks(device_t dev);

static int		bhndb_pci_compat_setregwin(device_t dev,
			    device_t pci_dev, const struct bhndb_regwin *,
			    bhnd_addr_t);
static int		bhndb_pci_fast_setregwin(device_t dev, device_t pci_dev,
			    const struct bhndb_regwin *, bhnd_addr_t);

static void		bhndb_pci_write_core(struct bhndb_pci_softc *sc,
			    bus_size_t offset, uint32_t value, u_int width);
static uint32_t		bhndb_pci_read_core(struct bhndb_pci_softc *sc,
			    bus_size_t offset, u_int width);

static void		bhndb_init_sromless_pci_config(
			    struct bhndb_pci_softc *sc);

static bus_addr_t	bhndb_pci_sprom_addr(struct bhndb_pci_softc *sc);
static bus_size_t	bhndb_pci_sprom_size(struct bhndb_pci_softc *sc);

static int		bhndb_pci_eio_init(struct bhndb_pci_eio *pio,
			    device_t dev, device_t pci_dev,
			    struct bhndb_host_resources *hr);
static int		bhndb_pci_eio_map(struct bhnd_erom_io *eio,
			    bhnd_addr_t addr, bhnd_size_t size);
static uint32_t		bhndb_pci_eio_read(struct bhnd_erom_io *eio,
			    bhnd_size_t offset, u_int width);

#define	BHNDB_PCI_MSI_COUNT	1

static struct bhndb_pci_quirk	bhndb_pci_quirks[];
static struct bhndb_pci_quirk	bhndb_pcie_quirks[];
static struct bhndb_pci_quirk	bhndb_pcie2_quirks[];

static struct bhndb_pci_core bhndb_pci_cores[] = {
	BHNDB_PCI_CORE(PCI,	BHND_PCI_SRSH_PI_OFFSET,	bhndb_pci_quirks),
	BHNDB_PCI_CORE(PCIE,	BHND_PCIE_SRSH_PI_OFFSET,	bhndb_pcie_quirks),
	BHNDB_PCI_CORE(PCIE2,	BHND_PCIE_SRSH_PI_OFFSET,	bhndb_pcie2_quirks),
	BHNDB_PCI_CORE_END
};

/* bhndb_pci erom I/O instance state */
struct bhndb_pci_eio {
	struct bhnd_erom_io		 eio;
	device_t			 dev;		/**< bridge device */
	device_t			 pci_dev;	/**< parent PCI device */
	struct bhndb_host_resources	*hr;		/**< borrowed reference to host resources */
	const struct bhndb_regwin	*win;		/**< mapped register window, or NULL */
	struct resource			*res;		/**< resource containing the register window, or NULL if no window mapped */
	bhnd_addr_t			 res_target;	/**< current target address (if mapped) */
	bool				 mapped;	/**< true if a valid mapping exists, false otherwise */
	bhnd_addr_t			 addr;		/**< mapped address */
	bhnd_size_t			 size;		/**< mapped size */
};

static struct bhndb_pci_quirk bhndb_pci_quirks[] = {
	/* Backplane interrupt flags must be routed via siba-specific
	 * SIBA_CFG0_INTVEC configuration register; the BHNDB_PCI_INT_MASK
	 * PCI configuration register is unsupported. */
	{{ BHND_MATCH_CHIP_TYPE		(SIBA) },
	 { BHND_MATCH_CORE_REV		(HWREV_LTE(5)) },
		BHNDB_PCI_QUIRK_SIBA_INTVEC },

	/* All PCI core revisions require the SRSH work-around */
	BHNDB_PCI_QUIRK(HWREV_ANY,	BHNDB_PCI_QUIRK_SRSH_WAR),
	BHNDB_PCI_QUIRK_END
};

static struct bhndb_pci_quirk bhndb_pcie_quirks[] = {
	/* All PCIe-G1 core revisions require the SRSH work-around */
	BHNDB_PCI_QUIRK(HWREV_ANY,	BHNDB_PCI_QUIRK_SRSH_WAR),
	BHNDB_PCI_QUIRK_END
};

static struct bhndb_pci_quirk bhndb_pcie2_quirks[] = {
	/* All PCIe-G2 core revisions require the SRSH work-around */
	BHNDB_PCI_QUIRK(HWREV_ANY,	BHNDB_PCI_QUIRK_SRSH_WAR),
	BHNDB_PCI_QUIRK_END
};


/**
 * Return the device table entry for @p ci, or NULL if none.
 */
static struct bhndb_pci_core *
bhndb_pci_find_core(struct bhnd_core_info *ci)
{
	for (size_t i = 0; !BHNDB_PCI_IS_CORE_END(&bhndb_pci_cores[i]); i++) {
		struct bhndb_pci_core *entry = &bhndb_pci_cores[i];

		if (bhnd_core_matches(ci, &entry->match))
			return (entry);
	}

	return (NULL);
}

/**
 * Return all quirk flags for the given @p cid and @p ci.
 */
static uint32_t
bhndb_pci_get_core_quirks(struct bhnd_chipid *cid, struct bhnd_core_info *ci)
{
	struct bhndb_pci_core	*entry;
	struct bhndb_pci_quirk	*qtable;
	uint32_t		 quirks;

	quirks = 0;

	/* No core entry? */
	if ((entry = bhndb_pci_find_core(ci)) == NULL)
		return (quirks);

	/* No quirks? */
	if ((qtable = entry->quirks) == NULL)
		return (quirks);

	for (size_t i = 0; !BHNDB_PCI_IS_QUIRK_END(&qtable[i]); i++) {
		struct bhndb_pci_quirk *q = &qtable[i];

		if (!bhnd_chip_matches(cid, &q->chip_desc))
			continue;

		if (!bhnd_core_matches(ci, &q->core_desc))
			continue;

		quirks |= q->quirks;
	}

	return (quirks);
}

/** 
 * Default bhndb_pci implementation of device_probe().
 * 
 * Verifies that the parent is a PCI/PCIe device.
 */
static int
bhndb_pci_probe(device_t dev)
{
	struct bhnd_chipid	 cid;
	struct bhnd_core_info	*cores, hostb_core;
	struct bhndb_pci_core	*entry;
	bhnd_devclass_t		 hostb_devclass;
	u_int			 ncores;
	device_t		 parent;
	devclass_t		 parent_bus, pci;
	int			 error;

	cores = NULL;

	/* Our parent must be a PCI/PCIe device. */
	pci = devclass_find("pci");
	parent = device_get_parent(dev);
	parent_bus = device_get_devclass(device_get_parent(parent));

	if (parent_bus != pci)
		return (ENXIO);

	/* Enable clocks */
	if ((error = bhndb_enable_pci_clocks(dev)))
		return (error);

	/* Identify the chip and enumerate the bridged cores */
	error = bhndb_pci_read_core_table(dev, &cid, &cores, &ncores, NULL);
	if (error)
		goto cleanup;

	/* Search our core table for the host bridge core */
	hostb_devclass = bhndb_expected_pci_devclass(dev);
	error = bhndb_find_hostb_core(cores, ncores, hostb_devclass,
	    &hostb_core);
	if (error)
		goto cleanup;

	/* Look for a matching core table entry */
	if ((entry = bhndb_pci_find_core(&hostb_core)) == NULL) {
		error = ENXIO;
		goto cleanup;
	}

	device_set_desc(dev, "PCI-BHND bridge");

	/* fall-through */
	error = BUS_PROBE_DEFAULT;

cleanup:
	bhndb_disable_pci_clocks(dev);
	if (cores != NULL)
		free(cores, M_BHND);

	return (error);
}

/**
 * Attempt to allocate MSI interrupts, returning the count in @p msi_count
 * on success.
 */
static int
bhndb_pci_alloc_msi(struct bhndb_pci_softc *sc, int *msi_count)
{
	int error, count;

	/* Is MSI available? */
	if (pci_msi_count(sc->parent) < BHNDB_PCI_MSI_COUNT)
		return (ENXIO);

	/* Allocate expected message count */
	count = BHNDB_PCI_MSI_COUNT;
	if ((error = pci_alloc_msi(sc->parent, &count))) {
		device_printf(sc->dev, "failed to allocate MSI interrupts: "
		    "%d\n", error);

		return (error);
	}

	if (count < BHNDB_PCI_MSI_COUNT)
		return (ENXIO);

	*msi_count = count;
	return (0);
}

static int
bhndb_pci_attach(device_t dev)
{
	struct bhndb_pci_softc	*sc;
	struct bhnd_chipid	 cid;
	struct bhnd_core_info	*cores, hostb_core;
	bhnd_erom_class_t	*erom_class;
	u_int			 ncores;
	int			 irq_rid;
	int			 error;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->parent = device_get_parent(dev);
	sc->pci_devclass = bhndb_expected_pci_devclass(dev);
	sc->pci_quirks = 0;
	sc->set_regwin = NULL;

	BHNDB_PCI_LOCK_INIT(sc);

	cores = NULL;

	/* Enable PCI bus mastering */
	pci_enable_busmaster(sc->parent);

	/* Set up PCI interrupt handling */
	if (bhndb_pci_alloc_msi(sc, &sc->msi_count) == 0) {
		/* MSI uses resource IDs starting at 1 */
		irq_rid = 1;

		device_printf(dev, "Using MSI interrupts on %s\n",
		    device_get_nameunit(sc->parent));
	} else {
		sc->msi_count = 0;
		irq_rid = 0;

		device_printf(dev, "Using INTx interrupts on %s\n",
		    device_get_nameunit(sc->parent));
	}

	sc->isrc = bhndb_alloc_intr_isrc(sc->parent, irq_rid, 0, RM_MAX_END, 1,
	    RF_SHAREABLE | RF_ACTIVE);
	if (sc->isrc == NULL) {
		device_printf(sc->dev, "failed to allocate interrupt "
		    "resource\n");
		error = ENXIO;
		goto cleanup;
	}

	/* Enable clocks (if required by this hardware) */
	if ((error = bhndb_enable_pci_clocks(sc->dev)))
		goto cleanup;

	/* Identify the chip and enumerate the bridged cores */
	error = bhndb_pci_read_core_table(dev, &cid, &cores, &ncores,
	    &erom_class);
	if (error)
		goto cleanup;

	/* Select the appropriate register window handler */
	if (cid.chip_type == BHND_CHIPTYPE_SIBA) {
		sc->set_regwin = bhndb_pci_compat_setregwin;
	} else {
		sc->set_regwin = bhndb_pci_fast_setregwin;
	}

	/* Determine our host bridge core and populate our quirk flags */
	error = bhndb_find_hostb_core(cores, ncores, sc->pci_devclass,
	    &hostb_core);
	if (error)
		goto cleanup;

	sc->pci_quirks = bhndb_pci_get_core_quirks(&cid, &hostb_core);

	/* Perform bridge attach */
	error = bhndb_attach(dev, &cid, cores, ncores, &hostb_core, erom_class);
	if (error)
		goto cleanup;

	/* Fix-up power on defaults for SROM-less devices. */
	bhndb_init_sromless_pci_config(sc);

	/* Add any additional child devices */
	if ((error = bhndb_pci_add_children(sc)))
		goto cleanup;

	/* Probe and attach our children */
	if ((error = bus_generic_attach(dev)))
		goto cleanup;

	free(cores, M_BHND);

	return (0);

cleanup:
	device_delete_children(dev);
	bhndb_disable_pci_clocks(sc->dev);

	if (sc->isrc != NULL)
		bhndb_free_intr_isrc(sc->isrc);

	if (sc->msi_count > 0)
		pci_release_msi(dev);

	if (cores != NULL)
		free(cores, M_BHND);

	pci_disable_busmaster(sc->parent);

	BHNDB_PCI_LOCK_DESTROY(sc);

	return (error);
}

static int
bhndb_pci_detach(device_t dev)
{
	struct bhndb_pci_softc	*sc;
	int			 error;

	sc = device_get_softc(dev);

	/* Attempt to detach our children */
	if ((error = bus_generic_detach(dev)))
		return (error);

	/* Perform generic bridge detach */
	if ((error = bhndb_generic_detach(dev)))
		return (error);

	/* Disable clocks (if required by this hardware) */
	if ((error = bhndb_disable_pci_clocks(sc->dev)))
		return (error);

	/* Free our interrupt resources */
	bhndb_free_intr_isrc(sc->isrc);

	/* Release MSI interrupts */
	if (sc->msi_count > 0)
		pci_release_msi(dev);

	/* Disable PCI bus mastering */
	pci_disable_busmaster(sc->parent);

	BHNDB_PCI_LOCK_DESTROY(sc);

	return (0);
}

/**
 * Use the generic PCI bridge hardware configuration to enumerate the bridged
 * bhnd(4) bus' core table.
 * 
 * @note This function may be safely called prior to device attach, (e.g.
 * from DEVICE_PROBE).
 * @note This function requires exclusive ownership over allocating and 
 * configuring host bridge resources, and should only be called prior to
 * completion of device attach and full configuration of the bridge.
 * 
 * @param	dev		The bhndb_pci bridge device.
 * @param[out]	chipid		On success, the parsed chip identification.
 * @param[out]	cores		On success, the enumerated core table. The
 *				caller is responsible for freeing this table via
 *				bhndb_pci_free_core_table().
 * @param[out]	ncores		On success, the number of cores found in
 *				@p cores.
 * @param[out]	eromcls		On success, a pointer to the erom class used to
 *				parse the device enumeration table. This
 *				argument may be NULL if the class is not
 *				desired.
 * 
 * @retval 0		success
 * @retval non-zero	if enumerating the bridged bhnd(4) bus fails, a regular
 * 			unix error code will be returned.
 */
static int
bhndb_pci_read_core_table(device_t dev, struct bhnd_chipid *chipid,
    struct bhnd_core_info **cores, u_int *ncores,
    bhnd_erom_class_t **eromcls)
{
	const struct bhndb_hwcfg	*cfg;
	struct bhndb_host_resources	*hr;
	struct bhndb_pci_eio		 pio;
	struct bhnd_core_info		*erom_cores;
	const struct bhnd_chipid	*hint;
	struct bhnd_chipid		 cid;
	bhnd_erom_class_t		*erom_class;
	bhnd_erom_t			*erom;
	device_t			 parent_dev;
	u_int				 erom_ncores;
	int				 error;

	parent_dev = device_get_parent(dev);
	erom = NULL;
	erom_cores = NULL;

	/* Fetch our chipid hint (if any) and generic hardware configuration */
	cfg = BHNDB_BUS_GET_GENERIC_HWCFG(parent_dev, dev);
	hint = BHNDB_BUS_GET_CHIPID(parent_dev, dev);

	/* Allocate our host resources */
	if ((error = bhndb_alloc_host_resources(parent_dev, cfg, &hr)))
		return (error);

	/* Initialize our erom I/O state */
	if ((error = bhndb_pci_eio_init(&pio, dev, parent_dev, hr)))
		goto failed;

	/* Map the first bus core from our bridged bhnd(4) bus */
	error = bhndb_pci_eio_map(&pio.eio, BHND_DEFAULT_CHIPC_ADDR,
	    BHND_DEFAULT_CORE_SIZE);
	if (error)
		goto failed;

	/* Probe for a usable EROM class, and read the chip identifier */
	erom_class = bhnd_erom_probe_driver_classes(device_get_devclass(dev),
	    &pio.eio, hint, &cid);
	if (erom_class == NULL) {
		device_printf(dev, "device enumeration unsupported; no "
		    "compatible driver found\n");

		error = ENXIO;
		goto failed;
	}

	/* Allocate EROM parser */
	if ((erom = bhnd_erom_alloc(erom_class, &cid, &pio.eio)) == NULL) {
		device_printf(dev, "failed to allocate device enumeration "
		    "table parser\n");
		error = ENXIO;
		goto failed;
	}

	/* Read the full core table */
	error = bhnd_erom_get_core_table(erom, &erom_cores, &erom_ncores);
	if (error) {
		device_printf(dev, "error fetching core table: %d\n", error);
		goto failed;
	}

	/* Provide the results to our caller */
	*cores = malloc(sizeof(erom_cores[0]) * erom_ncores, M_BHND, M_WAITOK);
	memcpy(*cores, erom_cores, sizeof(erom_cores[0]) * erom_ncores);
	*ncores = erom_ncores;

	*chipid = cid;
	if (eromcls != NULL)
		*eromcls = erom_class;

	/* Clean up */
	bhnd_erom_free_core_table(erom, erom_cores);
	bhnd_erom_free(erom);
	bhndb_release_host_resources(hr);

	return (0);

failed:
	if (erom_cores != NULL)
		bhnd_erom_free_core_table(erom, erom_cores);

	if (erom != NULL)
		bhnd_erom_free(erom);

	bhndb_release_host_resources(hr);
	return (error);
}

static int
bhndb_pci_add_children(struct bhndb_pci_softc *sc)
{
	bus_size_t		 nv_sz;
	int			 error;

	/**
	 * If SPROM is mapped directly into BAR0, add child NVRAM
	 * device.
	 */
	nv_sz = bhndb_pci_sprom_size(sc);
	if (nv_sz > 0) {
		struct bhndb_devinfo	*dinfo;
		device_t		 child;

		if (bootverbose) {
			device_printf(sc->dev, "found SPROM (%ju bytes)\n",
			    (uintmax_t)nv_sz);
		}

		/* Add sprom device, ordered early enough to be available
		 * before the bridged bhnd(4) bus is attached. */
		child = BUS_ADD_CHILD(sc->dev,
		    BHND_PROBE_ROOT + BHND_PROBE_ORDER_EARLY, "bhnd_nvram", -1);
		if (child == NULL) {
			device_printf(sc->dev, "failed to add sprom device\n");
			return (ENXIO);
		}

		/* Initialize device address space and resource covering the
		 * BAR0 SPROM shadow. */
		dinfo = device_get_ivars(child);
		dinfo->addrspace = BHNDB_ADDRSPACE_NATIVE;

		error = bus_set_resource(child, SYS_RES_MEMORY, 0,
		    bhndb_pci_sprom_addr(sc), nv_sz);
		if (error) {
			device_printf(sc->dev,
			    "failed to register sprom resources\n");
			return (error);
		}
	}

	return (0);
}

static const struct bhndb_regwin *
bhndb_pci_sprom_regwin(struct bhndb_pci_softc *sc)
{
	struct bhndb_resources		*bres;
	const struct bhndb_hwcfg	*cfg;
	const struct bhndb_regwin	*sprom_win;

	bres = sc->bhndb.bus_res;
	cfg = bres->cfg;

	sprom_win = bhndb_regwin_find_type(cfg->register_windows,
	    BHNDB_REGWIN_T_SPROM, BHNDB_PCI_V0_BAR0_SPROM_SIZE);

	return (sprom_win);
}

static bus_addr_t
bhndb_pci_sprom_addr(struct bhndb_pci_softc *sc)
{
	const struct bhndb_regwin	*sprom_win;
	struct resource			*r;

	/* Fetch the SPROM register window */
	sprom_win = bhndb_pci_sprom_regwin(sc);
	KASSERT(sprom_win != NULL, ("requested sprom address on PCI_V2+"));

	/* Fetch the associated resource */
	r = bhndb_host_resource_for_regwin(sc->bhndb.bus_res->res, sprom_win);
	KASSERT(r != NULL, ("missing resource for sprom window\n"));

	return (rman_get_start(r) + sprom_win->win_offset);
}

static bus_size_t
bhndb_pci_sprom_size(struct bhndb_pci_softc *sc)
{
	const struct bhndb_regwin	*sprom_win;
	uint32_t			 sctl;
	bus_size_t			 sprom_sz;

	sprom_win = bhndb_pci_sprom_regwin(sc);

	/* PCI_V2 and later devices map SPROM/OTP via ChipCommon */
	if (sprom_win == NULL)
		return (0);

	/* Determine SPROM size */
	sctl = pci_read_config(sc->parent, BHNDB_PCI_SPROM_CONTROL, 4);
	if (sctl & BHNDB_PCI_SPROM_BLANK)
		return (0);

	switch (sctl & BHNDB_PCI_SPROM_SZ_MASK) {
	case BHNDB_PCI_SPROM_SZ_1KB:
		sprom_sz = (1 * 1024);
		break;

	case BHNDB_PCI_SPROM_SZ_4KB:
		sprom_sz = (4 * 1024);
		break;

	case BHNDB_PCI_SPROM_SZ_16KB:
		sprom_sz = (16 * 1024);
		break;

	case BHNDB_PCI_SPROM_SZ_RESERVED:
	default:
		device_printf(sc->dev, "invalid PCI sprom size 0x%x\n", sctl);
		return (0);
	}

	if (sprom_sz > sprom_win->win_size) {
		device_printf(sc->dev,
		    "PCI sprom size (0x%x) overruns defined register window\n",
		    sctl);
		return (0);
	}

	return (sprom_sz);
}

/**
 * Return the host resource providing a static mapping of the PCI core's
 * registers.
 * 
 * @param	sc	bhndb PCI driver state.
 * @param[out]	res	On success, the host resource containing our PCI
 *			core's register window.
 * @param[out]	offset	On success, the offset of the PCI core registers within
 * 			@p res.
 *
 * @retval 0		success
 * @retval ENXIO	if a valid static register window mapping the PCI core
 *			registers is not available.
 */
static int
bhndb_pci_get_core_regs(struct bhndb_pci_softc *sc, struct resource **res,
    bus_size_t *offset)
{
	const struct bhndb_regwin	*win;
	struct resource			*r;

	/* Locate the static register window mapping the PCI core */
	win = bhndb_regwin_find_core(sc->bhndb.bus_res->cfg->register_windows,
	    sc->pci_devclass, 0, BHND_PORT_DEVICE, 0, 0);
	if (win == NULL) {
		device_printf(sc->dev, "missing PCI core register window\n");
		return (ENXIO);
	}

	/* Fetch the resource containing the register window */
	r = bhndb_host_resource_for_regwin(sc->bhndb.bus_res->res, win);
	if (r == NULL) {
		device_printf(sc->dev, "missing PCI core register resource\n");
		return (ENXIO);
	}

	*res = r;
	*offset = win->win_offset;

	return (0);
}

/**
 * Write a 1, 2, or 4 byte data item to the PCI core's registers at @p offset.
 * 
 * @param sc		bhndb PCI driver state.
 * @param offset	register write offset.
 * @param value		value to be written.
 * @param width		item width (1, 2, or 4 bytes).
 */
static void
bhndb_pci_write_core(struct bhndb_pci_softc *sc, bus_size_t offset,
    uint32_t value, u_int width)
{
	struct resource	*r;
	bus_size_t	 r_offset;
	int		 error;

	if ((error = bhndb_pci_get_core_regs(sc, &r, &r_offset)))
		panic("no PCI core registers: %d", error);

	switch (width) {
	case 1:
		bus_write_1(r, r_offset + offset, value);
		break;
	case 2:
		bus_write_2(r, r_offset + offset, value);
		break;
	case 4:
		bus_write_4(r, r_offset + offset, value);
		break;
	default:
		panic("invalid width: %u", width);
	}
}

/**
 * Read a 1, 2, or 4 byte data item from the PCI core's registers
 * at @p offset.
 * 
 * @param sc		bhndb PCI driver state.
 * @param offset	register read offset.
 * @param width		item width (1, 2, or 4 bytes).
 */
static uint32_t
bhndb_pci_read_core(struct bhndb_pci_softc *sc, bus_size_t offset, u_int width)
{
	struct resource	*r;
	bus_size_t	 r_offset;
	int		 error;

	if ((error = bhndb_pci_get_core_regs(sc, &r, &r_offset)))
		panic("no PCI core registers: %d", error);

	switch (width) {
	case 1:
		return (bus_read_1(r, r_offset + offset));
	case 2:
		return (bus_read_2(r, r_offset + offset));
	case 4:
		return (bus_read_4(r, r_offset + offset));
	default:
		panic("invalid width: %u", width);
	}
}

/*
 * On devices without a SROM, the PCI(e) cores will be initialized with
 * their Power-on-Reset defaults; this can leave two of the BAR0 PCI windows
 * mapped to the wrong core.
 * 
 * This function updates the SROM shadow to point the BAR0 windows at the
 * current PCI core.
 * 
 * Applies to all PCI/PCIe revisions.
 */
static void
bhndb_init_sromless_pci_config(struct bhndb_pci_softc *sc)
{
	const struct bhndb_pci_core	*pci_core;
	bus_size_t			 srsh_offset;
	u_int				 pci_cidx, sprom_cidx;
	uint16_t			 val;

	if ((sc->pci_quirks & BHNDB_PCI_QUIRK_SRSH_WAR) == 0)
		return;

	/* Determine the correct register offset for our PCI core */
	pci_core = bhndb_pci_find_core(&sc->bhndb.bridge_core);
	KASSERT(pci_core != NULL, ("missing core table entry"));

	srsh_offset = pci_core->srsh_offset;

	/* Fetch the SPROM's configured core index */
	val = bhndb_pci_read_core(sc, srsh_offset, sizeof(val));
	sprom_cidx = (val & BHND_PCI_SRSH_PI_MASK) >> BHND_PCI_SRSH_PI_SHIFT;

	/* If it doesn't match host bridge's core index, update the index
	 * value */
	pci_cidx = sc->bhndb.bridge_core.core_idx;
	if (sprom_cidx != pci_cidx) {
		val &= ~BHND_PCI_SRSH_PI_MASK;
		val |= (pci_cidx << BHND_PCI_SRSH_PI_SHIFT);
		bhndb_pci_write_core(sc, srsh_offset, val, sizeof(val));
	}
}

static int
bhndb_pci_resume(device_t dev)
{
	struct bhndb_pci_softc	*sc;
	int			 error;

	sc = device_get_softc(dev);
	
	/* Enable clocks (if supported by this hardware) */
	if ((error = bhndb_enable_pci_clocks(sc->dev)))
		return (error);

	/* Perform resume */
	return (bhndb_generic_resume(dev));
}

static int
bhndb_pci_suspend(device_t dev)
{
	struct bhndb_pci_softc	*sc;
	int			 error;

	sc = device_get_softc(dev);
	
	/* Disable clocks (if supported by this hardware) */
	if ((error = bhndb_disable_pci_clocks(sc->dev)))
		return (error);

	/* Perform suspend */
	return (bhndb_generic_suspend(dev));
}

static int
bhndb_pci_set_window_addr(device_t dev, const struct bhndb_regwin *rw,
    bhnd_addr_t addr)
{
	struct bhndb_pci_softc *sc = device_get_softc(dev);
	return (sc->set_regwin(sc->dev, sc->parent, rw, addr));
}

/**
 * A siba(4) and bcma(4)-compatible bhndb_set_window_addr implementation.
 * 
 * On siba(4) devices, it's possible that writing a PCI window register may
 * not succeed; it's necessary to immediately read the configuration register
 * and retry if not set to the desired value.
 * 
 * This is not necessary on bcma(4) devices, but other than the overhead of
 * validating the register, there's no harm in performing the verification.
 */
static int
bhndb_pci_compat_setregwin(device_t dev, device_t pci_dev,
    const struct bhndb_regwin *rw, bhnd_addr_t addr)
{
	int		error;
	int		reg;

	if (rw->win_type != BHNDB_REGWIN_T_DYN)
		return (ENODEV);

	reg = rw->d.dyn.cfg_offset;
	for (u_int i = 0; i < BHNDB_PCI_BARCTRL_WRITE_RETRY; i++) {
		if ((error = bhndb_pci_fast_setregwin(dev, pci_dev, rw, addr)))
			return (error);

		if (pci_read_config(pci_dev, reg, 4) == addr)
			return (0);

		DELAY(10);
	}

	/* Unable to set window */
	return (ENODEV);
}

/**
 * A bcma(4)-only bhndb_set_window_addr implementation.
 */
static int
bhndb_pci_fast_setregwin(device_t dev, device_t pci_dev,
    const struct bhndb_regwin *rw, bhnd_addr_t addr)
{
	/* The PCI bridge core only supports 32-bit addressing, regardless
	 * of the bus' support for 64-bit addressing */
	if (addr > UINT32_MAX)
		return (ERANGE);

	switch (rw->win_type) {
	case BHNDB_REGWIN_T_DYN:
		/* Addresses must be page aligned */
		if (addr % rw->win_size != 0)
			return (EINVAL);

		pci_write_config(pci_dev, rw->d.dyn.cfg_offset, addr, 4);
		break;
	default:
		return (ENODEV);
	}

	return (0);
}

static int
bhndb_pci_populate_board_info(device_t dev, device_t child,
    struct bhnd_board_info *info)
{
	struct bhndb_pci_softc	*sc;

	sc = device_get_softc(dev);

	/* 
	 * On a subset of Apple BCM4360 modules, always prefer the
	 * PCI subdevice to the SPROM-supplied boardtype.
	 * 
	 * TODO:
	 * 
	 * Broadcom's own drivers implement this override, and then later use
	 * the remapped BCM4360 board type to determine the required
	 * board-specific workarounds.
	 * 
	 * Without access to this hardware, it's unclear why this mapping
	 * is done, and we must do the same. If we can survey the hardware
	 * in question, it may be possible to replace this behavior with
	 * explicit references to the SPROM-supplied boardtype(s) in our
	 * quirk definitions.
	 */
	if (pci_get_subvendor(sc->parent) == PCI_VENDOR_APPLE) {
		switch (info->board_type) {
		case BHND_BOARD_BCM94360X29C:
		case BHND_BOARD_BCM94360X29CP2:
		case BHND_BOARD_BCM94360X51:
		case BHND_BOARD_BCM94360X51P2:
			info->board_type = 0;	/* allow override below */
			break;
		default:
			break;
		}
	}

	/* If NVRAM did not supply vendor/type info, provide the PCI
	 * subvendor/subdevice values. */
	if (info->board_vendor == 0)
		info->board_vendor = pci_get_subvendor(sc->parent);

	if (info->board_type == 0)
		info->board_type = pci_get_subdevice(sc->parent);

	return (0);
}

/**
 * Examine the bridge device @p dev and return the expected host bridge
 * device class.
 *
 * @param dev The bhndb bridge device
 */
static bhnd_devclass_t
bhndb_expected_pci_devclass(device_t dev)
{
	if (bhndb_is_pcie_attached(dev))
		return (BHND_DEVCLASS_PCIE);
	else
		return (BHND_DEVCLASS_PCI);
}

/**
 * Return true if the bridge device @p dev is attached via PCIe,
 * false otherwise.
 *
 * @param dev The bhndb bridge device
 */
static bool
bhndb_is_pcie_attached(device_t dev)
{
	int reg;

	if (pci_find_cap(device_get_parent(dev), PCIY_EXPRESS, &reg) == 0)
		return (true);

	return (false);
}

/**
 * Enable externally managed clocks, if required.
 * 
 * Some PCI chipsets (BCM4306, possibly others) chips do not support
 * the idle low-power clock. Clocking must be bootstrapped at
 * attach/resume by directly adjusting GPIO registers exposed in the
 * PCI config space, and correspondingly, explicitly shutdown at
 * detach/suspend.
 *
 * @note This function may be safely called prior to device attach, (e.g.
 * from DEVICE_PROBE).
 *
 * @param dev The bhndb bridge device
 */
static int
bhndb_enable_pci_clocks(device_t dev)
{
	device_t		pci_dev;
	uint32_t		gpio_in, gpio_out, gpio_en;
	uint32_t		gpio_flags;
	uint16_t		pci_status;

	pci_dev = device_get_parent(dev);

	/* Only supported and required on PCI devices */
	if (!bhndb_is_pcie_attached(dev))
		return (0);

	/* Read state of XTAL pin */
	gpio_in = pci_read_config(pci_dev, BHNDB_PCI_GPIO_IN, 4);
	if (gpio_in & BHNDB_PCI_GPIO_XTAL_ON)
		return (0); /* already enabled */

	/* Fetch current config */
	gpio_out = pci_read_config(pci_dev, BHNDB_PCI_GPIO_OUT, 4);
	gpio_en = pci_read_config(pci_dev, BHNDB_PCI_GPIO_OUTEN, 4);

	/* Set PLL_OFF/XTAL_ON pins to HIGH and enable both pins */
	gpio_flags = (BHNDB_PCI_GPIO_PLL_OFF|BHNDB_PCI_GPIO_XTAL_ON);
	gpio_out |= gpio_flags;
	gpio_en |= gpio_flags;

	pci_write_config(pci_dev, BHNDB_PCI_GPIO_OUT, gpio_out, 4);
	pci_write_config(pci_dev, BHNDB_PCI_GPIO_OUTEN, gpio_en, 4);
	DELAY(1000);

	/* Reset PLL_OFF */
	gpio_out &= ~BHNDB_PCI_GPIO_PLL_OFF;
	pci_write_config(pci_dev, BHNDB_PCI_GPIO_OUT, gpio_out, 4);
	DELAY(5000);

	/* Clear any PCI 'sent target-abort' flag. */
	pci_status = pci_read_config(pci_dev, PCIR_STATUS, 2);
	pci_status &= ~PCIM_STATUS_STABORT;
	pci_write_config(pci_dev, PCIR_STATUS, pci_status, 2);

	return (0);
}

/**
 * Disable externally managed clocks, if required.
 *
 * This function may be safely called prior to device attach, (e.g.
 * from DEVICE_PROBE).
 *
 * @param dev The bhndb bridge device
 */
static int
bhndb_disable_pci_clocks(device_t dev)
{
	device_t	pci_dev;
	uint32_t	gpio_out, gpio_en;

	pci_dev = device_get_parent(dev);

	/* Only supported and required on PCI devices */
	if (bhndb_is_pcie_attached(dev))
		return (0);

	/* Fetch current config */
	gpio_out = pci_read_config(pci_dev, BHNDB_PCI_GPIO_OUT, 4);
	gpio_en = pci_read_config(pci_dev, BHNDB_PCI_GPIO_OUTEN, 4);

	/* Set PLL_OFF to HIGH, XTAL_ON to LOW. */
	gpio_out &= ~BHNDB_PCI_GPIO_XTAL_ON;
	gpio_out |= BHNDB_PCI_GPIO_PLL_OFF;
	pci_write_config(pci_dev, BHNDB_PCI_GPIO_OUT, gpio_out, 4);

	/* Enable both output pins */
	gpio_en |= (BHNDB_PCI_GPIO_PLL_OFF|BHNDB_PCI_GPIO_XTAL_ON);
	pci_write_config(pci_dev, BHNDB_PCI_GPIO_OUTEN, gpio_en, 4);

	return (0);
}

static bhnd_clksrc
bhndb_pci_pwrctl_get_clksrc(device_t dev, device_t child,
	bhnd_clock clock)
{
	struct bhndb_pci_softc	*sc;
	uint32_t		 gpio_out;

	sc = device_get_softc(dev);

	/* Only supported on PCI devices */
	if (bhndb_is_pcie_attached(sc->dev))
		return (ENODEV);

	/* Only ILP is supported */
	if (clock != BHND_CLOCK_ILP)
		return (ENXIO);

	gpio_out = pci_read_config(sc->parent, BHNDB_PCI_GPIO_OUT, 4);
	if (gpio_out & BHNDB_PCI_GPIO_SCS)
		return (BHND_CLKSRC_PCI);
	else
		return (BHND_CLKSRC_XTAL);
}

static int
bhndb_pci_pwrctl_gate_clock(device_t dev, device_t child,
	bhnd_clock clock)
{
	struct bhndb_pci_softc *sc = device_get_softc(dev);

	/* Only supported on PCI devices */
	if (bhndb_is_pcie_attached(sc->dev))
		return (ENODEV);

	/* Only HT is supported */
	if (clock != BHND_CLOCK_HT)
		return (ENXIO);

	return (bhndb_disable_pci_clocks(sc->dev));
}

static int
bhndb_pci_pwrctl_ungate_clock(device_t dev, device_t child,
	bhnd_clock clock)
{
	struct bhndb_pci_softc *sc = device_get_softc(dev);

	/* Only supported on PCI devices */
	if (bhndb_is_pcie_attached(sc->dev))
		return (ENODEV);

	/* Only HT is supported */
	if (clock != BHND_CLOCK_HT)
		return (ENXIO);

	return (bhndb_enable_pci_clocks(sc->dev));
}

/**
 * BHNDB_MAP_INTR_ISRC()
 */
static int
bhndb_pci_map_intr_isrc(device_t dev, struct resource *irq,
    struct bhndb_intr_isrc **isrc)
{
	struct bhndb_pci_softc *sc = device_get_softc(dev);

	/* There's only one bridged interrupt to choose from */
	*isrc = sc->isrc;
	return (0);
}

/* siba-specific implementation of BHNDB_ROUTE_INTERRUPTS() */
static int
bhndb_pci_route_siba_interrupts(struct bhndb_pci_softc *sc, device_t child)
{
	uint32_t	sbintvec;
	u_int		ivec;
	int		error;

	KASSERT(sc->pci_quirks & BHNDB_PCI_QUIRK_SIBA_INTVEC,
	    ("route_siba_interrupts not supported by this hardware"));

	/* Fetch the sbflag# for the child */
	if ((error = bhnd_get_intr_ivec(child, 0, &ivec)))
		return (error);

	if (ivec > (sizeof(sbintvec)*8) - 1 /* aka '31' */) {
		/* This should never be an issue in practice */
		device_printf(sc->dev, "cannot route interrupts to high "
		    "sbflag# %u\n", ivec);
		return (ENXIO);
	}

	BHNDB_PCI_LOCK(sc);

	sbintvec = bhndb_pci_read_core(sc, SB0_REG_ABS(SIBA_CFG0_INTVEC), 4);
	sbintvec |= (1 << ivec);
	bhndb_pci_write_core(sc, SB0_REG_ABS(SIBA_CFG0_INTVEC), sbintvec, 4);

	BHNDB_PCI_UNLOCK(sc);

	return (0);
}

/* BHNDB_ROUTE_INTERRUPTS() */
static int
bhndb_pci_route_interrupts(device_t dev, device_t child)
{
	struct bhndb_pci_softc	*sc;
	struct bhnd_core_info	 core;
	uint32_t		 core_bit;
	uint32_t		 intmask;

	sc = device_get_softc(dev);

	if (sc->pci_quirks & BHNDB_PCI_QUIRK_SIBA_INTVEC)
		return (bhndb_pci_route_siba_interrupts(sc, child));

	core = bhnd_get_core_info(child);
	if (core.core_idx > BHNDB_PCI_SBIM_COREIDX_MAX) {
		/* This should never be an issue in practice */
		device_printf(dev, "cannot route interrupts to high core "
		    "index %u\n", core.core_idx);
		return (ENXIO);
	}

	BHNDB_PCI_LOCK(sc);

	core_bit = (1<<core.core_idx) << BHNDB_PCI_SBIM_SHIFT;
	intmask = pci_read_config(sc->parent, BHNDB_PCI_INT_MASK, 4);
	intmask |= core_bit;
	pci_write_config(sc->parent, BHNDB_PCI_INT_MASK, intmask, 4);

	BHNDB_PCI_UNLOCK(sc);

	return (0);
}

/**
 * Initialize a new bhndb PCI bridge EROM I/O instance. This EROM I/O
 * implementation supports mapping of the device enumeration table via the
 * @p hr host resources.
 * 
 * @param pio		The instance to be initialized.
 * @param dev		The bridge device.
 * @param pci_dev	The bridge's parent PCI device.
 * @param hr		The host resources to be used to map the device
 *			enumeration table.
 */
static int
bhndb_pci_eio_init(struct bhndb_pci_eio *pio, device_t dev, device_t pci_dev,
    struct bhndb_host_resources *hr)
{
	memset(&pio->eio, sizeof(pio->eio), 0);
	pio->eio.map = bhndb_pci_eio_map;
	pio->eio.read = bhndb_pci_eio_read;
	pio->eio.fini = NULL;

	pio->dev = dev;
	pio->pci_dev = pci_dev;
	pio->hr = hr;
	pio->win = NULL;
	pio->res = NULL;

	return (0);
}

/**
 * Attempt to adjust the dynamic register window backing @p pio to permit
 * reading @p size bytes at @p addr.
 * 
 * If @p addr or @p size fall outside the existing mapped range, or if
 * @p pio is not backed by a dynamic register window, ENXIO will be returned.
 * 
 * @param pio	The bhndb PCI erom I/O state to be modified.
 * @param addr	The address to be include
 */
static int
bhndb_pci_eio_adjust_mapping(struct bhndb_pci_eio *pio, bhnd_addr_t addr,
    bhnd_size_t size)
{
	bhnd_addr_t	 target;
	bhnd_size_t	 offset;
	int		 error;


	KASSERT(pio->win != NULL, ("missing register window"));
	KASSERT(pio->res != NULL, ("missing regwin resource"));
	KASSERT(pio->win->win_type == BHNDB_REGWIN_T_DYN,
	    ("unexpected window type %d", pio->win->win_type));

	/* The requested subrange must fall within the total mapped range */
	if (addr < pio->addr || (addr - pio->addr) > pio->size ||
	    size > pio->size || (addr - pio->addr) - pio->size < size)
	{
		return (ENXIO);
	}

	/* Do we already have a useable mapping? */
	if (addr >= pio->res_target &&
	    addr <= pio->res_target + pio->win->win_size &&
	    (pio->res_target + pio->win->win_size) - addr >= size)
	{
		return (0);
	}

	/* Page-align the target address */
	offset = addr % pio->win->win_size;
	target = addr - offset;

	/* Configure the register window */
	error = bhndb_pci_compat_setregwin(pio->dev, pio->pci_dev, pio->win,
	    target);
	if (error) {
		device_printf(pio->dev, "failed to configure dynamic register "
		    "window: %d\n", error);
		return (error);
	}

	pio->res_target = target;
	return (0);
}

/* bhnd_erom_io_map() implementation */
static int
bhndb_pci_eio_map(struct bhnd_erom_io *eio, bhnd_addr_t addr,
    bhnd_size_t size)
{
	struct bhndb_pci_eio		*pio;
	const struct bhndb_regwin	*regwin;
	struct resource			*r;
	bhnd_addr_t			 target;
	bhnd_size_t			 offset;
	int				 error;

	pio = (struct bhndb_pci_eio *)eio;

	/* Locate a useable dynamic register window */
	regwin = bhndb_regwin_find_type(pio->hr->cfg->register_windows,
	    BHNDB_REGWIN_T_DYN, MIN(size, BHND_DEFAULT_CORE_SIZE));
	if (regwin == NULL) {
		device_printf(pio->dev, "unable to map %#jx+%#jx; no "
		    "usable dynamic register window found\n", addr, size);
		return (ENXIO);
	}

	/* Locate the host resource mapping our register window */
	if ((r = bhndb_host_resource_for_regwin(pio->hr, regwin)) == NULL) {
		device_printf(pio->dev, "unable to map %#jx+%#jx; no "
		    "usable register resource found\n", addr, size);
		return (ENXIO);
	}

	/* Page-align the target address */
	offset = addr % regwin->win_size;
	target = addr - offset;

	/* Configure the register window */
	error = bhndb_pci_compat_setregwin(pio->dev, pio->pci_dev, regwin,
	    target);
	if (error) {
		device_printf(pio->dev, "failed to configure dynamic register "
		    "window: %d\n", error);
		return (error);
	}

	/* Update our mapping state */
	pio->win = regwin;
	pio->res = r;
	pio->addr = addr;
	pio->size = size;
	pio->res_target = target;

	return (0);
}

/* bhnd_erom_io_read() implementation */
static uint32_t
bhndb_pci_eio_read(struct bhnd_erom_io *eio, bhnd_size_t offset, u_int width)
{
	struct bhndb_pci_eio		*pio;
	bhnd_addr_t			 addr;
	bus_size_t			 res_offset;
	int				 error;

	pio = (struct bhndb_pci_eio *)eio;

	/* Calculate absolute address */
	if (BHND_SIZE_MAX - offset < pio->addr) {
		device_printf(pio->dev, "invalid offset %#jx+%#jx\n", pio->addr,
		    offset);
		return (UINT32_MAX);
	}

	addr = pio->addr + offset;

	/* Adjust the mapping for our read */
	if ((error = bhndb_pci_eio_adjust_mapping(pio, addr, width))) {
		device_printf(pio->dev, "failed to adjust register mapping: "
		    "%d\n", error);
		return (UINT32_MAX);
	}

	KASSERT(pio->res_target <= addr, ("invalid mapping (%#jx vs. %#jx)",
	    pio->res_target, addr));

	/* Determine the actual read offset within our register window
	 * resource */
	res_offset = (addr - pio->res_target) + pio->win->win_offset;

	/* Perform our read */
	switch (width) {
	case 1:
		return (bus_read_1(pio->res, res_offset));
	case 2:
		return (bus_read_2(pio->res, res_offset));
	case 4:
		return (bus_read_4(pio->res, res_offset));
	default:
		panic("unsupported width: %u", width);
	}
}

static device_method_t bhndb_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			bhndb_pci_probe),
	DEVMETHOD(device_attach,		bhndb_pci_attach),
	DEVMETHOD(device_resume,		bhndb_pci_resume),
	DEVMETHOD(device_suspend,		bhndb_pci_suspend),
	DEVMETHOD(device_detach,		bhndb_pci_detach),

	/* BHND interface */
	DEVMETHOD(bhnd_bus_pwrctl_get_clksrc,	bhndb_pci_pwrctl_get_clksrc),
	DEVMETHOD(bhnd_bus_pwrctl_gate_clock,	bhndb_pci_pwrctl_gate_clock),
	DEVMETHOD(bhnd_bus_pwrctl_ungate_clock,	bhndb_pci_pwrctl_ungate_clock),

	/* BHNDB interface */
	DEVMETHOD(bhndb_set_window_addr,	bhndb_pci_set_window_addr),
	DEVMETHOD(bhndb_populate_board_info,	bhndb_pci_populate_board_info),
	DEVMETHOD(bhndb_map_intr_isrc,		bhndb_pci_map_intr_isrc),
	DEVMETHOD(bhndb_route_interrupts,	bhndb_pci_route_interrupts),

	DEVMETHOD_END
};

DEFINE_CLASS_1(bhndb, bhndb_pci_driver, bhndb_pci_methods,
    sizeof(struct bhndb_pci_softc), bhndb_driver);

MODULE_VERSION(bhndb_pci, 1);
MODULE_DEPEND(bhndb_pci, bhnd_pci_hostb, 1, 1, 1);
MODULE_DEPEND(bhndb_pci, pci, 1, 1, 1);
MODULE_DEPEND(bhndb_pci, bhndb, 1, 1, 1);
MODULE_DEPEND(bhndb_pci, bhnd, 1, 1, 1);
