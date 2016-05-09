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

/*
 * Broadcom ChipCommon driver.
 * 
 * With the exception of some very early chipsets, the ChipCommon core
 * has been included in all HND SoCs and chipsets based on the siba(4) 
 * and bcma(4) interconnects, providing a common interface to chipset 
 * identification, bus enumeration, UARTs, clocks, watchdog interrupts, GPIO, 
 * flash, etc.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/bhnd/bhnd.h>

#include "bhnd_nvram_if.h"

#include "chipcreg.h"
#include "chipcvar.h"

devclass_t bhnd_chipc_devclass;	/**< bhnd(4) chipcommon device class */

static const struct resource_spec chipc_rspec[CHIPC_MAX_RSPEC] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, -1, 0 }
};

static struct bhnd_device_quirk chipc_quirks[];

/* Supported device identifiers */
static const struct bhnd_device chipc_devices[] = {
	BHND_DEVICE(CC, "CC", chipc_quirks),
	BHND_DEVICE_END
};


/* Device quirks table */
static struct bhnd_device_quirk chipc_quirks[] = {
	{ BHND_HWREV_GTE	(32),	CHIPC_QUIRK_SUPPORTS_SPROM },
	{ BHND_HWREV_GTE	(35),	CHIPC_QUIRK_SUPPORTS_NFLASH },
	BHND_DEVICE_QUIRK_END
};

/* Chip-specific quirks table */
static struct bhnd_chip_quirk chipc_chip_quirks[] = {
	/* 4331 12x9 packages */
	{{ BHND_CHIP_IP(4331, 4331TN) },
		CHIPC_QUIRK_4331_GPIO2_5_MUX_SPROM
	},
	{{ BHND_CHIP_IP(4331, 4331TNA0) },
		CHIPC_QUIRK_4331_GPIO2_5_MUX_SPROM
	},

	/* 4331 12x12 packages */
	{{ BHND_CHIP_IPR(4331, 4331TT, HWREV_GTE(1)) },
		CHIPC_QUIRK_4331_EXTPA2_MUX_SPROM
	},

	/* 4331 (all packages/revisions) */
	{{ BHND_CHIP_ID(4331) },
		CHIPC_QUIRK_4331_EXTPA_MUX_SPROM
	},

	/* 4360 family (all revs <= 2) */
	{{ BHND_CHIP_IR(4352, HWREV_LTE(2)) },
		CHIPC_QUIRK_4360_FEM_MUX_SPROM },
	{{ BHND_CHIP_IR(43460, HWREV_LTE(2)) },
		CHIPC_QUIRK_4360_FEM_MUX_SPROM },
	{{ BHND_CHIP_IR(43462, HWREV_LTE(2)) },
		CHIPC_QUIRK_4360_FEM_MUX_SPROM },
	{{ BHND_CHIP_IR(43602, HWREV_LTE(2)) },
		CHIPC_QUIRK_4360_FEM_MUX_SPROM },

	BHND_CHIP_QUIRK_END
};

/* quirk and capability flag convenience macros */
#define	CHIPC_QUIRK(_sc, _name)	\
    ((_sc)->quirks & CHIPC_QUIRK_ ## _name)
    
#define CHIPC_CAP(_sc, _name)	\
    ((_sc)->caps & CHIPC_ ## _name)

#define	CHIPC_ASSERT_QUIRK(_sc, name)	\
    KASSERT(CHIPC_QUIRK((_sc), name), ("quirk " __STRING(_name) " not set"))

#define	CHIPC_ASSERT_CAP(_sc, name)	\
    KASSERT(CHIPC_CAP((_sc), name), ("capability " __STRING(_name) " not set"))

static bhnd_nvram_src_t	chipc_nvram_identify(struct chipc_softc *sc);
static int		chipc_sprom_init(struct chipc_softc *);
static int		chipc_enable_sprom_pins(struct chipc_softc *);
static int		chipc_disable_sprom_pins(struct chipc_softc *);


static int
chipc_probe(device_t dev)
{
	const struct bhnd_device *id;

	id = bhnd_device_lookup(dev, chipc_devices, sizeof(chipc_devices[0]));
	if (id == NULL)
		return (ENXIO);

	bhnd_set_default_core_desc(dev);
	return (BUS_PROBE_DEFAULT);
}

static int
chipc_attach(device_t dev)
{
	struct chipc_softc		*sc;
	bhnd_addr_t			 enum_addr;
	uint32_t			 ccid_reg;
	uint8_t				 chip_type;
	int				 error;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->quirks = bhnd_device_quirks(dev, chipc_devices,
	    sizeof(chipc_devices[0]));
	sc->quirks |= bhnd_chip_quirks(dev, chipc_chip_quirks);
	
	CHIPC_LOCK_INIT(sc);

	/* Allocate bus resources */
	memcpy(sc->rspec, chipc_rspec, sizeof(sc->rspec));
	if ((error = bhnd_alloc_resources(dev, sc->rspec, sc->res)))
		return (error);

	sc->core = sc->res[0];
	
	/* Fetch our chipset identification data */
	ccid_reg = bhnd_bus_read_4(sc->core, CHIPC_ID);
	chip_type = CHIPC_GET_ATTR(ccid_reg, ID_BUS);

	switch (chip_type) {
	case BHND_CHIPTYPE_SIBA:
		/* enumeration space starts at the ChipCommon register base. */
		enum_addr = rman_get_start(sc->core->res);
		break;
	case BHND_CHIPTYPE_BCMA:
	case BHND_CHIPTYPE_BCMA_ALT:
		enum_addr = bhnd_bus_read_4(sc->core, CHIPC_EROMPTR);
		break;
	default:
		device_printf(dev, "unsupported chip type %hhu\n", chip_type);
		error = ENODEV;
		goto cleanup;
	}

	sc->ccid = bhnd_parse_chipid(ccid_reg, enum_addr);

	/* Fetch capability and status register values */
	sc->caps = bhnd_bus_read_4(sc->core, CHIPC_CAPABILITIES);
	sc->cst = bhnd_bus_read_4(sc->core, CHIPC_CHIPST);

	/* Identify NVRAM source */
	sc->nvram_src = chipc_nvram_identify(sc);

	/* Read NVRAM data */
	switch (sc->nvram_src) {
	case BHND_NVRAM_SRC_OTP:
		// TODO (requires access to OTP hardware)
		device_printf(sc->dev, "NVRAM-OTP unsupported\n");
		break;

	case BHND_NVRAM_SRC_NFLASH:
		// TODO (requires access to NFLASH hardware)
		device_printf(sc->dev, "NVRAM-NFLASH unsupported\n");
		break;

	case BHND_NVRAM_SRC_SPROM:
		if ((error = chipc_sprom_init(sc)))
			goto cleanup;
		break;

	case BHND_NVRAM_SRC_UNKNOWN:
		/* Handled externally */
		break;
	}

	return (0);
	
cleanup:
	bhnd_release_resources(dev, sc->rspec, sc->res);
	CHIPC_LOCK_DESTROY(sc);
	return (error);
}

static int
chipc_detach(device_t dev)
{
	struct chipc_softc	*sc;

	sc = device_get_softc(dev);
	bhnd_release_resources(dev, sc->rspec, sc->res);
	bhnd_sprom_fini(&sc->sprom);

	CHIPC_LOCK_DESTROY(sc);

	return (0);
}

static int
chipc_suspend(device_t dev)
{
	return (0);
}

static int
chipc_resume(device_t dev)
{
	return (0);
}

/**
 * Initialize local SPROM shadow, if required.
 * 
 * @param sc chipc driver state.
 */
static int
chipc_sprom_init(struct chipc_softc *sc)
{
	int	error;

	KASSERT(sc->nvram_src == BHND_NVRAM_SRC_SPROM,
	    ("non-SPROM source (%u)\n", sc->nvram_src));

	/* Enable access to the SPROM */
	CHIPC_LOCK(sc);
	if ((error = chipc_enable_sprom_pins(sc)))
		goto failed;

	/* Initialize SPROM parser */
	error = bhnd_sprom_init(&sc->sprom, sc->core, CHIPC_SPROM_OTP);
	if (error) {
		device_printf(sc->dev, "SPROM identification failed: %d\n",
			error);

		chipc_disable_sprom_pins(sc);
		goto failed;
	}

	/* Drop access to the SPROM lines */
	if ((error = chipc_disable_sprom_pins(sc))) {
		bhnd_sprom_fini(&sc->sprom);
		goto failed;
	}
	CHIPC_UNLOCK(sc);

	return (0);

failed:
	CHIPC_UNLOCK(sc);
	return (error);
}

/**
 * Determine the NVRAM data source for this device.
 *
 * @param sc chipc driver state.
 */
static bhnd_nvram_src_t
chipc_nvram_identify(struct chipc_softc *sc)
{
	uint32_t		 srom_ctrl;

	/* Very early devices vend SPROM/OTP/CIS (if at all) via the
	 * host bridge interface instead of ChipCommon. */
	if (!CHIPC_QUIRK(sc, SUPPORTS_SPROM))
		return (BHND_NVRAM_SRC_UNKNOWN);

	/*
	 * Later chipset revisions standardized the SPROM capability flags and
	 * register interfaces.
	 * 
	 * We check for hardware presence in order of precedence. For example,
	 * SPROM is is always used in preference to internal OTP if found.
	 */
	if (CHIPC_CAP(sc, CAP_SPROM)) {
		srom_ctrl = bhnd_bus_read_4(sc->core, CHIPC_SPROM_CTRL);
		if (srom_ctrl & CHIPC_SRC_PRESENT)
			return (BHND_NVRAM_SRC_SPROM);
	}

	/* Check for OTP */
	if (CHIPC_CAP(sc, CAP_OTP_SIZE))
		return (BHND_NVRAM_SRC_OTP);

	/*
	 * Finally, Northstar chipsets (and possibly other chipsets?) support
	 * external NAND flash. 
	 */
	if (CHIPC_QUIRK(sc, SUPPORTS_NFLASH) && CHIPC_CAP(sc, CAP_NFLASH))
		return (BHND_NVRAM_SRC_NFLASH);

	/* No NVRAM hardware capability declared */
	return (BHND_NVRAM_SRC_UNKNOWN);
}


/**
 * If required by this device, enable access to the SPROM.
 * 
 * @param sc chipc driver state.
 */
static int
chipc_enable_sprom_pins(struct chipc_softc *sc)
{
	uint32_t cctrl;
	
	CHIPC_LOCK_ASSERT(sc, MA_OWNED);

	/* Nothing to do? */
	if (!CHIPC_QUIRK(sc, MUX_SPROM))
		return (0);

	cctrl = bhnd_bus_read_4(sc->core, CHIPC_CHIPCTRL);

	/* 4331 devices */
	if (CHIPC_QUIRK(sc, 4331_EXTPA_MUX_SPROM)) {
		cctrl &= ~CHIPC_CCTRL4331_EXTPA_EN;

		if (CHIPC_QUIRK(sc, 4331_GPIO2_5_MUX_SPROM))
			cctrl &= ~CHIPC_CCTRL4331_EXTPA_ON_GPIO2_5;

		if (CHIPC_QUIRK(sc, 4331_EXTPA2_MUX_SPROM))
			cctrl &= ~CHIPC_CCTRL4331_EXTPA_EN2;

		bhnd_bus_write_4(sc->core, CHIPC_CHIPCTRL, cctrl);
		return (0);
	}

	/* 4360 devices */
	if (CHIPC_QUIRK(sc, 4360_FEM_MUX_SPROM)) {
		/* Unimplemented */
	}

	/* Refuse to proceed on unsupported devices with muxed SPROM pins */
	device_printf(sc->dev, "muxed sprom lines on unrecognized device\n");
	return (ENXIO);
}

/**
 * If required by this device, revert any GPIO/pin configuration applied
 * to allow SPROM access.
 * 
 * @param sc chipc driver state.
 */
static int
chipc_disable_sprom_pins(struct chipc_softc *sc)
{
	uint32_t cctrl;

	CHIPC_LOCK_ASSERT(sc, MA_OWNED);

	/* Nothing to do? */
	if (!CHIPC_QUIRK(sc, MUX_SPROM))
		return (0);

	cctrl = bhnd_bus_read_4(sc->core, CHIPC_CHIPCTRL);

	/* 4331 devices */
	if (CHIPC_QUIRK(sc, 4331_EXTPA_MUX_SPROM)) {
		cctrl |= CHIPC_CCTRL4331_EXTPA_EN;

		if (CHIPC_QUIRK(sc, 4331_GPIO2_5_MUX_SPROM))
			cctrl |= CHIPC_CCTRL4331_EXTPA_ON_GPIO2_5;

		if (CHIPC_QUIRK(sc, 4331_EXTPA2_MUX_SPROM))
			cctrl |= CHIPC_CCTRL4331_EXTPA_EN2;

		bhnd_bus_write_4(sc->core, CHIPC_CHIPCTRL, cctrl);
		return (0);
	}

	/* 4360 devices */
	if (CHIPC_QUIRK(sc, 4360_FEM_MUX_SPROM)) {
		/* Unimplemented */
	}
	
	/* Refuse to proceed on unsupported devices with muxed SPROM pins */
	device_printf(sc->dev, "muxed sprom lines on unrecognized device\n");
	return (ENXIO);
}

static bhnd_nvram_src_t
chipc_nvram_src(device_t dev)
{
	struct chipc_softc *sc = device_get_softc(dev);
	return (sc->nvram_src);
}

static int
chipc_nvram_getvar(device_t dev, const char *name, void *buf, size_t *len)
{
	struct chipc_softc	*sc;
	int			 error;

	sc = device_get_softc(dev);

	switch (sc->nvram_src) {
	case BHND_NVRAM_SRC_SPROM:
		CHIPC_LOCK(sc);
		error = bhnd_sprom_getvar(&sc->sprom, name, buf, len);
		CHIPC_UNLOCK(sc);
		return (error);

	case BHND_NVRAM_SRC_OTP:
	case BHND_NVRAM_SRC_NFLASH:
		/* Currently unsupported */
		return (ENXIO);

	case BHND_NVRAM_SRC_UNKNOWN:
		return (ENODEV);
	}

	/* Unknown NVRAM source */
	return (ENODEV);
}

static int
chipc_nvram_setvar(device_t dev, const char *name, const void *buf,
    size_t len)
{
	struct chipc_softc	*sc;
	int			 error;

	sc = device_get_softc(dev);

	switch (sc->nvram_src) {
	case BHND_NVRAM_SRC_SPROM:
		CHIPC_LOCK(sc);
		error = bhnd_sprom_setvar(&sc->sprom, name, buf, len);
		CHIPC_UNLOCK(sc);
		return (error);

	case BHND_NVRAM_SRC_OTP:
	case BHND_NVRAM_SRC_NFLASH:
		/* Currently unsupported */
		return (ENXIO);

	case BHND_NVRAM_SRC_UNKNOWN:
	default:
		return (ENODEV);
	}

	/* Unknown NVRAM source */
	return (ENODEV);
}

static device_method_t chipc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		chipc_probe),
	DEVMETHOD(device_attach,	chipc_attach),
	DEVMETHOD(device_detach,	chipc_detach),
	DEVMETHOD(device_suspend,	chipc_suspend),
	DEVMETHOD(device_resume,	chipc_resume),
	
	/* ChipCommon interface */
	DEVMETHOD(bhnd_chipc_nvram_src,	chipc_nvram_src),

	/* NVRAM interface */
	DEVMETHOD(bhnd_nvram_getvar,	chipc_nvram_getvar),
	DEVMETHOD(bhnd_nvram_setvar,	chipc_nvram_setvar),

	DEVMETHOD_END
};

DEFINE_CLASS_0(bhnd_chipc, chipc_driver, chipc_methods, sizeof(struct chipc_softc));
DRIVER_MODULE(bhnd_chipc, bhnd, chipc_driver, bhnd_chipc_devclass, 0, 0);
MODULE_DEPEND(bhnd_chipc, bhnd, 1, 1, 1);
MODULE_VERSION(bhnd_chipc, 1);
