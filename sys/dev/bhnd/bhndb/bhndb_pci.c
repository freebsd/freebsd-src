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

#include <dev/bhnd/cores/pci/bhnd_pcireg.h>

#include "bhndb_pcireg.h"
#include "bhndb_pcivar.h"
#include "bhndb_private.h"

static int		bhndb_enable_pci_clocks(struct bhndb_pci_softc *sc);
static int		bhndb_disable_pci_clocks(struct bhndb_pci_softc *sc);

static int		bhndb_pci_compat_setregwin(struct bhndb_pci_softc *,
			    const struct bhndb_regwin *, bhnd_addr_t);
static int		bhndb_pci_fast_setregwin(struct bhndb_pci_softc *,
			    const struct bhndb_regwin *, bhnd_addr_t);

static void		bhndb_init_sromless_pci_config(
			    struct bhndb_pci_softc *sc);

static bus_addr_t	bhndb_pci_sprom_addr(struct bhndb_pci_softc *sc);
static bus_size_t	bhndb_pci_sprom_size(struct bhndb_pci_softc *sc);

/** 
 * Default bhndb_pci implementation of device_probe().
 * 
 * Verifies that the parent is a PCI/PCIe device.
 */
static int
bhndb_pci_probe(device_t dev)
{
	device_t	parent;
	devclass_t	parent_bus;
	devclass_t	pci;

	/* Our parent must be a PCI/PCIe device. */
	pci = devclass_find("pci");
	parent = device_get_parent(dev);
	parent_bus = device_get_devclass(device_get_parent(parent));

	if (parent_bus != pci)
		return (ENXIO);

	device_set_desc(dev, "PCI-BHND bridge");

	return (BUS_PROBE_DEFAULT);
}

static int
bhndb_pci_attach(device_t dev)
{
	struct bhndb_pci_softc	*sc;
	int			 error, reg;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->parent = device_get_parent(dev);

	/* Enable PCI bus mastering */
	pci_enable_busmaster(sc->parent);

	/* Determine our bridge device class */
	sc->pci_devclass = BHND_DEVCLASS_PCI;
	if (pci_find_cap(sc->parent, PCIY_EXPRESS, &reg) == 0)
		sc->pci_devclass = BHND_DEVCLASS_PCIE;

	/* Enable clocks (if supported by this hardware) */
	if ((error = bhndb_enable_pci_clocks(sc)))
		return (error);

	/* Use siba(4)-compatible regwin handling until we know
	 * what kind of bus is attached */
	sc->set_regwin = bhndb_pci_compat_setregwin;

	/* Perform full bridge attach. This should call back into our
	 * bhndb_pci_init_full_config() implementation once the bridged
	 * bhnd(4) bus has been enumerated, but before any devices have been
	 * probed or attached. */
	if ((error = bhndb_attach(dev, sc->pci_devclass)))
		return (error);

	/* If supported, switch to the faster regwin handling */
	if (sc->bhndb.chipid.chip_type != BHND_CHIPTYPE_SIBA) {
		atomic_store_rel_ptr((volatile void *) &sc->set_regwin,
		    (uintptr_t) &bhndb_pci_fast_setregwin);
	}

	return (0);
}

static int
bhndb_pci_init_full_config(device_t dev, device_t child,
    const struct bhndb_hw_priority *hw_prio_table)
{
	struct bhndb_pci_softc	*sc;
	device_t		 nv_dev;
	bus_size_t		 nv_sz;
	int			 error;

	sc = device_get_softc(dev);

	/* Let our parent perform standard initialization first */
	if ((error = bhndb_generic_init_full_config(dev, child, hw_prio_table)))
		return (error);

	/* Fix-up power on defaults for SROM-less devices. */
	bhndb_init_sromless_pci_config(sc);

	/* If SPROM is mapped directly into BAR0, add NVRAM device. */
	nv_sz = bhndb_pci_sprom_size(sc);
	if (nv_sz > 0) {
		struct bhndb_devinfo	*dinfo;
		const char		*dname;

		if (bootverbose) {
			device_printf(dev, "found SPROM (%u bytes)\n",
			    (unsigned int) nv_sz);
		}

		/* Add sprom device */
		dname = "bhnd_nvram";
		if ((nv_dev = BUS_ADD_CHILD(dev, 0, dname, -1)) == NULL) {
			device_printf(dev, "failed to add sprom device\n");
			return (ENXIO);
		}

		/* Initialize device address space and resource covering the
		 * BAR0 SPROM shadow. */
		dinfo = device_get_ivars(nv_dev);
		dinfo->addrspace = BHNDB_ADDRSPACE_NATIVE;
		error = bus_set_resource(nv_dev, SYS_RES_MEMORY, 0,
		    bhndb_pci_sprom_addr(sc), nv_sz);

		if (error) {
			device_printf(dev,
			    "failed to register sprom resources\n");
			return (error);
		}

		/* Attach the device */
		if ((error = device_probe_and_attach(nv_dev))) {
			device_printf(dev, "sprom attach failed\n");
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
	r = bhndb_find_regwin_resource(sc->bhndb.bus_res, sprom_win);
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
	struct bhndb_resources		*bres;
	const struct bhndb_hwcfg	*cfg;
	const struct bhndb_regwin	*win;
	struct resource			*core_regs;
	bus_size_t			 srom_offset;
	u_int				 pci_cidx, sprom_cidx;
	uint16_t			 val;

	bres = sc->bhndb.bus_res;
	cfg = bres->cfg;

	if (bhnd_get_vendor(sc->bhndb.hostb_dev) != BHND_MFGID_BCM)
		return;

	switch (bhnd_get_device(sc->bhndb.hostb_dev)) {
	case BHND_COREID_PCI:
		srom_offset = BHND_PCI_SRSH_PI_OFFSET;
		break;
	case BHND_COREID_PCIE:
		srom_offset = BHND_PCIE_SRSH_PI_OFFSET;
		break;
	default:
		device_printf(sc->dev, "unsupported PCI host bridge device\n");
		return;
	}

	/* Locate the static register window mapping the PCI core */
	win = bhndb_regwin_find_core(cfg->register_windows, sc->pci_devclass,
	    0, BHND_PORT_DEVICE, 0, 0);
	if (win == NULL) {
		device_printf(sc->dev, "missing PCI core register window\n");
		return;
	}

	/* Fetch the resource containing the register window */
	core_regs = bhndb_find_regwin_resource(bres, win);
	if (core_regs == NULL) {
		device_printf(sc->dev, "missing PCI core register resource\n");
		return;
	}

	/* Fetch the SPROM's configured core index */
	val = bus_read_2(core_regs, win->win_offset + srom_offset);
	sprom_cidx = (val & BHND_PCI_SRSH_PI_MASK) >> BHND_PCI_SRSH_PI_SHIFT;

	/* If it doesn't match host bridge's core index, update the index
	 * value */
	pci_cidx = bhnd_get_core_index(sc->bhndb.hostb_dev);
	if (sprom_cidx != pci_cidx) {
		val &= ~BHND_PCI_SRSH_PI_MASK;
		val |= (pci_cidx << BHND_PCI_SRSH_PI_SHIFT);
		bus_write_2(core_regs,
		    win->win_offset + srom_offset, val);
	}
}

static int
bhndb_pci_resume(device_t dev)
{
	struct bhndb_pci_softc	*sc;
	int			 error;

	sc = device_get_softc(dev);
	
	/* Enable clocks (if supported by this hardware) */
	if ((error = bhndb_enable_pci_clocks(sc)))
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
	if ((error = bhndb_disable_pci_clocks(sc)))
		return (error);

	/* Perform suspend */
	return (bhndb_generic_suspend(dev));
}

static int
bhndb_pci_detach(device_t dev)
{
	struct bhndb_pci_softc	*sc;
	int			 error;

	sc = device_get_softc(dev);

	/* Disable clocks (if supported by this hardware) */
	if ((error = bhndb_disable_pci_clocks(sc)))
		return (error);

	/* Perform detach */
	if ((error = bhndb_generic_detach(dev)))
		return (error);

	/* Disable PCI bus mastering */
	pci_disable_busmaster(sc->parent);

	return (0);
}

static int
bhndb_pci_set_window_addr(device_t dev, const struct bhndb_regwin *rw,
    bhnd_addr_t addr)
{
	struct bhndb_pci_softc *sc = device_get_softc(dev);
	return (sc->set_regwin(sc, rw, addr));
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
bhndb_pci_compat_setregwin(struct bhndb_pci_softc *sc,
    const struct bhndb_regwin *rw, bhnd_addr_t addr)
{
	int		error;
	int		reg;

	if (rw->win_type != BHNDB_REGWIN_T_DYN)
		return (ENODEV);

	reg = rw->d.dyn.cfg_offset;
	for (u_int i = 0; i < BHNDB_PCI_BARCTRL_WRITE_RETRY; i++) {
		if ((error = bhndb_pci_fast_setregwin(sc, rw, addr)))
			return (error);

		if (pci_read_config(sc->parent, reg, 4) == addr)
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
bhndb_pci_fast_setregwin(struct bhndb_pci_softc *sc,
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

		pci_write_config(sc->parent, rw->d.dyn.cfg_offset, addr, 4);
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
 * Enable externally managed clocks, if required.
 * 
 * Some PCI chipsets (BCM4306, possibly others) chips do not support
 * the idle low-power clock. Clocking must be bootstrapped at
 * attach/resume by directly adjusting GPIO registers exposed in the
 * PCI config space, and correspondingly, explicitly shutdown at
 * detach/suspend.
 * 
 * @param sc Bridge driver state.
 */
static int
bhndb_enable_pci_clocks(struct bhndb_pci_softc *sc)
{
	uint32_t		gpio_in, gpio_out, gpio_en;
	uint32_t		gpio_flags;
	uint16_t		pci_status;

	/* Only supported and required on PCI devices */
	if (sc->pci_devclass != BHND_DEVCLASS_PCI)
		return (0);

	/* Read state of XTAL pin */
	gpio_in = pci_read_config(sc->parent, BHNDB_PCI_GPIO_IN, 4);
	if (gpio_in & BHNDB_PCI_GPIO_XTAL_ON)
		return (0); /* already enabled */

	/* Fetch current config */
	gpio_out = pci_read_config(sc->parent, BHNDB_PCI_GPIO_OUT, 4);
	gpio_en = pci_read_config(sc->parent, BHNDB_PCI_GPIO_OUTEN, 4);

	/* Set PLL_OFF/XTAL_ON pins to HIGH and enable both pins */
	gpio_flags = (BHNDB_PCI_GPIO_PLL_OFF|BHNDB_PCI_GPIO_XTAL_ON);
	gpio_out |= gpio_flags;
	gpio_en |= gpio_flags;

	pci_write_config(sc->parent, BHNDB_PCI_GPIO_OUT, gpio_out, 4);
	pci_write_config(sc->parent, BHNDB_PCI_GPIO_OUTEN, gpio_en, 4);
	DELAY(1000);

	/* Reset PLL_OFF */
	gpio_out &= ~BHNDB_PCI_GPIO_PLL_OFF;
	pci_write_config(sc->parent, BHNDB_PCI_GPIO_OUT, gpio_out, 4);
	DELAY(5000);

	/* Clear any PCI 'sent target-abort' flag. */
	pci_status = pci_read_config(sc->parent, PCIR_STATUS, 2);
	pci_status &= ~PCIM_STATUS_STABORT;
	pci_write_config(sc->parent, PCIR_STATUS, pci_status, 2);

	return (0);
}

/**
 * Disable externally managed clocks, if required.
 * 
 * @param sc Bridge driver state.
 */
static int
bhndb_disable_pci_clocks(struct bhndb_pci_softc *sc)
{
	uint32_t	gpio_out, gpio_en;

	/* Only supported and required on PCI devices */
	if (sc->pci_devclass != BHND_DEVCLASS_PCI)
		return (0);

	/* Fetch current config */
	gpio_out = pci_read_config(sc->parent, BHNDB_PCI_GPIO_OUT, 4);
	gpio_en = pci_read_config(sc->parent, BHNDB_PCI_GPIO_OUTEN, 4);

	/* Set PLL_OFF to HIGH, XTAL_ON to LOW. */
	gpio_out &= ~BHNDB_PCI_GPIO_XTAL_ON;
	gpio_out |= BHNDB_PCI_GPIO_PLL_OFF;
	pci_write_config(sc->parent, BHNDB_PCI_GPIO_OUT, gpio_out, 4);

	/* Enable both output pins */
	gpio_en |= (BHNDB_PCI_GPIO_PLL_OFF|BHNDB_PCI_GPIO_XTAL_ON);
	pci_write_config(sc->parent, BHNDB_PCI_GPIO_OUTEN, gpio_en, 4);

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
	if (sc->pci_devclass != BHND_DEVCLASS_PCI)
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
	if (sc->pci_devclass != BHND_DEVCLASS_PCI)
		return (ENODEV);

	/* Only HT is supported */
	if (clock != BHND_CLOCK_HT)
		return (ENXIO);

	return (bhndb_disable_pci_clocks(sc));
}

static int
bhndb_pci_pwrctl_ungate_clock(device_t dev, device_t child,
	bhnd_clock clock)
{
	struct bhndb_pci_softc *sc = device_get_softc(dev);

	/* Only supported on PCI devices */
	if (sc->pci_devclass != BHND_DEVCLASS_PCI)
		return (ENODEV);

	/* Only HT is supported */
	if (clock != BHND_CLOCK_HT)
		return (ENXIO);

	return (bhndb_enable_pci_clocks(sc));
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
	DEVMETHOD(bhndb_init_full_config,	bhndb_pci_init_full_config),
	DEVMETHOD(bhndb_set_window_addr,	bhndb_pci_set_window_addr),
	DEVMETHOD(bhndb_populate_board_info,	bhndb_pci_populate_board_info),

	DEVMETHOD_END
};

DEFINE_CLASS_1(bhndb, bhndb_pci_driver, bhndb_pci_methods,
    sizeof(struct bhndb_pci_softc), bhndb_driver);

MODULE_VERSION(bhndb_pci, 1);
MODULE_DEPEND(bhndb_pci, bhnd_pci_hostb, 1, 1, 1);
MODULE_DEPEND(bhndb_pci, pci, 1, 1, 1);
MODULE_DEPEND(bhndb_pci, bhndb, 1, 1, 1);
MODULE_DEPEND(bhndb_pci, bhnd, 1, 1, 1);
