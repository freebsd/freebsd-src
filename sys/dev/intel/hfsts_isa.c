/*
 * Copyright (c) 2026 Abdelkader Boudih <seuros@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * hfstsfd(4) - detects HECI hidden via the PCH Function Disable register.
 *
 * The MEI PCI function never enumerates when firmware (coreboot's
 * PCH_DISABLE_MEI1/2) has disabled it, so there is no PCI device instance
 * for hfsts_attach() to run against. This is an isa-attached identify
 * driver instead, modeled on ichwd_identify() in sys/dev/ichwd/ichwd.c,
 * which faces the same problem for the ICH watchdog.
 *
 * RCBA (LPC bridge config offset 0xF0) and FD2 (RCBA+0x3428, bit 1 =
 * MEI1 disabled, bit 2 = MEI2 disabled) use this layout on Ibex Peak,
 * Cougar Point, Panther Point, Lynx Point, and Wildcat Point.  identify()
 * checks the LPC bridge's device ID against hfsts_fd_lpc_ids[] and only adds
 * a child for those generations, instead of assuming that every PCH's
 * disable mechanism or ME capability is known.
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/pmap.h>

#include <dev/pci/pcivar.h>
#include <dev/ichwd/ichwd.h>
#include <dev/intel/hfsts.h>

#define HFSTS_FD_RCBA_OFFSET	0xf0
#define HFSTS_FD_RCBA_EN		0x00000001
#define HFSTS_FD_RCBA_MASK	0xffffc000
#define HFSTS_FD_FD2_OFFSET	0x3428
#define   HFSTS_FD_FD2_MEI1_DIS	0x00000002
#define   HFSTS_FD_FD2_MEI2_DIS	0x00000004
/* Page containing FD2, and FD2's offset within that mapped page. */
#define HFSTS_FD_FD2_PAGE	(HFSTS_FD_FD2_OFFSET & ~(vm_paddr_t)PAGE_MASK)
#define HFSTS_FD_FD2_PAGE_OFF	(HFSTS_FD_FD2_OFFSET & PAGE_MASK)

static const uint16_t hfsts_fd_lpc_ids[] = {
	DEVICEID_LPT0, DEVICEID_LPT1, DEVICEID_LPT2, DEVICEID_LPT3,
	DEVICEID_LPT4, DEVICEID_LPT5, DEVICEID_LPT6, DEVICEID_LPT7,
	DEVICEID_LPT8, DEVICEID_LPT9, DEVICEID_LPT10, DEVICEID_LPT11,
	DEVICEID_LPT12, DEVICEID_LPT13, DEVICEID_LPT14, DEVICEID_LPT15,
	DEVICEID_LPT16, DEVICEID_LPT17, DEVICEID_LPT18, DEVICEID_LPT19,
	DEVICEID_LPT20, DEVICEID_LPT21, DEVICEID_LPT22, DEVICEID_LPT23,
	DEVICEID_LPT24, DEVICEID_LPT25, DEVICEID_LPT26, DEVICEID_LPT27,
	DEVICEID_LPT28, DEVICEID_LPT29, DEVICEID_LPT30, DEVICEID_LPT31,
	DEVICEID_LPT_LP0, DEVICEID_LPT_LP1, DEVICEID_LPT_LP2,
	DEVICEID_LPT_LP3, DEVICEID_LPT_LP4, DEVICEID_LPT_LP5,
	DEVICEID_LPT_LP6, DEVICEID_LPT_LP7,
	DEVICEID_WCPT1, DEVICEID_WCPT2, DEVICEID_WCPT3, DEVICEID_WCPT4,
	DEVICEID_WCPT6,
	DEVICEID_WCPT_LP1, DEVICEID_WCPT_LP2, DEVICEID_WCPT_LP3,
	DEVICEID_WCPT_LP5, DEVICEID_WCPT_LP6, DEVICEID_WCPT_LP7,
	DEVICEID_WCPT_LP9,
	DEVICEID_CPT0, DEVICEID_CPT1, DEVICEID_CPT2, DEVICEID_CPT3,
	DEVICEID_CPT4, DEVICEID_CPT5, DEVICEID_CPT6, DEVICEID_CPT7,
	DEVICEID_CPT8, DEVICEID_CPT9, DEVICEID_CPT10, DEVICEID_CPT11,
	DEVICEID_CPT12, DEVICEID_CPT13, DEVICEID_CPT14, DEVICEID_CPT15,
	DEVICEID_CPT16, DEVICEID_CPT17, DEVICEID_CPT18, DEVICEID_CPT19,
	DEVICEID_CPT20, DEVICEID_CPT21, DEVICEID_CPT22, DEVICEID_CPT23,
	DEVICEID_CPT24, DEVICEID_CPT25, DEVICEID_CPT26, DEVICEID_CPT27,
	DEVICEID_CPT28, DEVICEID_CPT29, DEVICEID_CPT30, DEVICEID_CPT31,
	DEVICEID_PPT0, DEVICEID_PPT1, DEVICEID_PPT2, DEVICEID_PPT3,
	DEVICEID_PPT4, DEVICEID_PPT5, DEVICEID_PPT6, DEVICEID_PPT7,
	DEVICEID_PPT8, DEVICEID_PPT9, DEVICEID_PPT10, DEVICEID_PPT11,
	DEVICEID_PPT12, DEVICEID_PPT13, DEVICEID_PPT14, DEVICEID_PPT15,
	DEVICEID_PPT16, DEVICEID_PPT17, DEVICEID_PPT18, DEVICEID_PPT19,
	DEVICEID_PPT20, DEVICEID_PPT21, DEVICEID_PPT22, DEVICEID_PPT23,
	DEVICEID_PPT24, DEVICEID_PPT25, DEVICEID_PPT26, DEVICEID_PPT27,
	DEVICEID_PPT28, DEVICEID_PPT29, DEVICEID_PPT30, DEVICEID_PPT31,
	DEVICEID_PCH, DEVICEID_PCHM, DEVICEID_P55, DEVICEID_PM55,
	DEVICEID_H55, DEVICEID_QM57, DEVICEID_H57, DEVICEID_HM55,
	DEVICEID_Q57, DEVICEID_HM57, DEVICEID_PCHMSFF, DEVICEID_QS57,
};

struct hfsts_fd_softc {
	device_t		sc_dev;
	char			sc_status[24];
	uint32_t		sc_fd2;
	uint32_t		sc_mei1_disabled;
	uint32_t		sc_mei2_disabled;
	bool			sc_checked;
};

static bool
hfsts_fd_lpc_known(uint16_t devid)
{
	u_int i;

	for (i = 0; i < nitems(hfsts_fd_lpc_ids); i++) {
		if (hfsts_fd_lpc_ids[i] == devid)
			return (true);
	}
	return (false);
}

static void
hfsts_fd_identify(driver_t *driver, device_t parent)
{
	device_t isab;

	if (device_find_child(parent, driver->name, 0) != NULL)
		return;

	isab = device_get_parent(parent);
	if (!is_pci_device(isab) || pci_get_vendor(isab) != VENDORID_INTEL ||
	    !hfsts_fd_lpc_known(pci_get_device(isab)))
		return;

	/* MEI enumerated on PCI, so it is not firmware-hidden. */
	if (hfsts_pci_present())
		return;

	BUS_ADD_CHILD(parent, 0, driver->name, 0);
}

static int
hfsts_fd_probe(device_t dev)
{

	device_set_desc(dev, "Intel ME Function Disable check");
	return (BUS_PROBE_NOWILDCARD);
}

static void
hfsts_fd_set_status(struct hfsts_fd_softc *sc, const char *status)
{

	strlcpy(sc->sc_status, status, sizeof(sc->sc_status));
}

static int
hfsts_fd_attach(device_t dev)
{
	struct hfsts_fd_softc *sc;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;
	device_t isab;
	vm_paddr_t rcba;
	bus_space_handle_t bsh;
	uint32_t fd2;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	isab = device_get_parent(device_get_parent(dev));

	rcba = pci_read_config(isab, HFSTS_FD_RCBA_OFFSET, 4);
	if ((rcba & HFSTS_FD_RCBA_EN) == 0 || (rcba & HFSTS_FD_RCBA_MASK) == 0) {
		device_printf(dev,
		    "RCBA not enabled, cannot read Function Disable register\n");
		hfsts_fd_set_status(sc, "rcba-disabled");
		goto done;
	}

	bsh = (bus_space_handle_t)pmap_mapdev(
	    (rcba & HFSTS_FD_RCBA_MASK) + HFSTS_FD_FD2_PAGE, PAGE_SIZE);
	fd2 = bus_space_read_4(X86_BUS_SPACE_MEM, bsh, HFSTS_FD_FD2_PAGE_OFF);
	pmap_unmapdev((void *)bsh, PAGE_SIZE);

	sc->sc_fd2 = fd2;
	sc->sc_mei1_disabled = (fd2 & HFSTS_FD_FD2_MEI1_DIS) != 0;
	sc->sc_mei2_disabled = (fd2 & HFSTS_FD_FD2_MEI2_DIS) != 0;
	sc->sc_checked = true;
	hfsts_fd_set_status(sc, "checked");

	if (bootverbose) {
		if (sc->sc_mei1_disabled) {
			device_printf(dev, "HECI1 disabled via PCH Function "
			    "Disable register (silicon present, hidden by "
			    "firmware)\n");
		} else {
			device_printf(dev, "HECI1 not disabled - MEI should "
			    "be enumerable as a normal PCI function\n");
		}
		if (sc->sc_mei2_disabled)
			device_printf(dev, "HECI2 disabled via PCH Function "
			    "Disable register\n");
	}

done:
	ctx = device_get_sysctl_ctx(dev);
	tree = device_get_sysctl_tree(dev);

	SYSCTL_ADD_STRING(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "status",
	    CTLFLAG_RD, sc->sc_status, 0,
	    "Function Disable check result");
	if (!sc->sc_checked)
		return (0);
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "fd2",
	    CTLFLAG_RD, &sc->sc_fd2, 0,
	    "Function Disable 2 register (raw)");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "mei1_disabled",
	    CTLFLAG_RD, &sc->sc_mei1_disabled, 0,
	    "HECI1 disabled via Function Disable register");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "mei2_disabled",
	    CTLFLAG_RD, &sc->sc_mei2_disabled, 0,
	    "HECI2 disabled via Function Disable register");

	return (0);
}

static int
hfsts_fd_detach(device_t dev)
{

	return (0);
}

static device_method_t hfsts_fd_methods[] = {
	DEVMETHOD(device_identify,	hfsts_fd_identify),
	DEVMETHOD(device_probe,	hfsts_fd_probe),
	DEVMETHOD(device_attach,	hfsts_fd_attach),
	DEVMETHOD(device_detach,	hfsts_fd_detach),
	DEVMETHOD_END
};

static driver_t hfsts_fd_driver = {
	"hfstsfd",
	hfsts_fd_methods,
	sizeof(struct hfsts_fd_softc)
};

DRIVER_MODULE(hfstsfd, isa, hfsts_fd_driver, 0, 0);
MODULE_VERSION(hfstsfd, 1);
