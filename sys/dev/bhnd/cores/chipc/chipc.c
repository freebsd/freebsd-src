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
	BHND_DEVICE(CC, NULL, chipc_quirks),
	BHND_DEVICE_END
};


/* Device quirks table */
static struct bhnd_device_quirk chipc_quirks[] = {
	{ BHND_HWREV_RANGE	(0,	21),	CHIPC_QUIRK_ALWAYS_HAS_SPROM },
	{ BHND_HWREV_EQ		(22),		CHIPC_QUIRK_SPROM_CHECK_CST_R22 },
	{ BHND_HWREV_RANGE	(23,	31),	CHIPC_QUIRK_SPROM_CHECK_CST_R23 },
	{ BHND_HWREV_GTE	(35),		CHIPC_QUIRK_SUPPORTS_NFLASH },
	BHND_DEVICE_QUIRK_END
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

	// TODO
	switch (bhnd_chipc_nvram_src(dev)) {
	case BHND_NVRAM_SRC_CIS:
		device_printf(dev, "NVRAM source: CIS\n");
		break;
	case BHND_NVRAM_SRC_SPROM:
		device_printf(dev, "NVRAM source: SPROM\n");
		break;
	case BHND_NVRAM_SRC_OTP:
		device_printf(dev, "NVRAM source: OTP\n");
		break;
	case BHND_NVRAM_SRC_NFLASH:
		device_printf(dev, "NVRAM source: NFLASH\n");
		break;
	case BHND_NVRAM_SRC_NONE:
		device_printf(dev, "NVRAM source: NONE\n");
		break;
	}

	return (0);
	
cleanup:
	bhnd_release_resources(dev, sc->rspec, sc->res);
	return (error);
}

static int
chipc_detach(device_t dev)
{
	struct chipc_softc	*sc;

	sc = device_get_softc(dev);
	bhnd_release_resources(dev, sc->rspec, sc->res);

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
 * Use device-specific ChipStatus flags to determine the preferred NVRAM
 * data source.
 */
static bhnd_nvram_src_t
chipc_nvram_src_chipst(struct chipc_softc *sc)
{
	uint8_t		nvram_sel;

	CHIPC_ASSERT_QUIRK(sc, SPROM_CHECK_CHIPST);

	if (CHIPC_QUIRK(sc, SPROM_CHECK_CST_R22)) {
		// TODO: On these devices, the official driver code always
		// assumes SPROM availability if CHIPC_CST_OTP_SEL is not
		// set; we must review against the actual behavior of our
		// BCM4312 hardware
		nvram_sel = CHIPC_GET_ATTR(sc->cst, CST_SPROM_OTP_SEL_R22);
	} else if (CHIPC_QUIRK(sc, SPROM_CHECK_CST_R23)) {
		nvram_sel = CHIPC_GET_ATTR(sc->cst, CST_SPROM_OTP_SEL_R23);
	} else {
		panic("invalid CST OTP/SPROM chipc quirk flags");
	}
	device_printf(sc->dev, "querying chipst for 0x%x, 0x%x\n", sc->ccid.chip_id, sc->cst);

	switch (nvram_sel) {
	case CHIPC_CST_DEFCIS_SEL:
		return (BHND_NVRAM_SRC_CIS);

	case CHIPC_CST_SPROM_SEL:
	case CHIPC_CST_OTP_PWRDN:
		return (BHND_NVRAM_SRC_SPROM);

	case CHIPC_CST_OTP_SEL:
		return (BHND_NVRAM_SRC_OTP);

	default:
		device_printf(sc->dev, "unrecognized OTP/SPROM type 0x%hhx",
		    nvram_sel);
		return (BHND_NVRAM_SRC_NONE);
	}
}

/**
 * Determine the preferred NVRAM data source.
 */
static bhnd_nvram_src_t
chipc_nvram_src(device_t dev)
{
	struct chipc_softc	*sc;
	uint32_t		 srom_ctrl;

	sc = device_get_softc(dev);

	/* Very early devices always included a SPROM */
	if (CHIPC_QUIRK(sc, ALWAYS_HAS_SPROM))
		return (BHND_NVRAM_SRC_SPROM);

	/* Most other early devices require checking ChipStatus flags */
	if (CHIPC_QUIRK(sc, SPROM_CHECK_CHIPST))
		return (chipc_nvram_src_chipst(sc));

	/*
	 * Later chipset revisions standardized the NVRAM capability flags and
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
	return (BHND_NVRAM_SRC_NONE);
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

	DEVMETHOD_END
};

DEFINE_CLASS_0(bhnd_chipc, chipc_driver, chipc_methods, sizeof(struct chipc_softc));
DRIVER_MODULE(bhnd_chipc, bhnd, chipc_driver, bhnd_chipc_devclass, 0, 0);
MODULE_VERSION(bhnd_chipc, 1);
