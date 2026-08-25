/*
 * Copyright (c) 2025 The FreeBSD Foundation
 *
 * This software was developed by Björn Zeeb under sponsorship from
 * the FreeBSD Foundation.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>

#include <linux/pci.h>
#include <dev/pci/pcivar.h>
#include "linux_80211.h"

#include <net80211/ieee80211_var.h>

struct lkpi_80211_pm_softc {
	/* PCI */
	int  (*suspend) (struct pci_dev *pdev, pm_message_t state);
	int  (*resume) (struct pci_dev *pdev);
};

static int
lkpi_80211_pm_suspend(struct pci_dev *pdev, pm_message_t state)
{
	const struct dev_pm_ops *pmops;
	struct lkpi_80211_pm_softc *sc;
	struct ieee80211com *ic;
	device_t dev;
	int error;

	dev = device_find_child(pdev->dev.bsddev, "lkpi80211_pm",
	    DEVICE_UNIT_ANY);
	if (dev == NULL) {
		/* Must not happen, so abort suspend if it does. */
		device_printf(pdev->dev.bsddev,
		    "%s: cannot find lkpi80211_pm child for %s\n",
		    __func__, device_get_name(pdev->dev.bsddev));
		return (ENXIO);
	}
	sc = device_get_softc(dev);
	error = 0;

	/* Call order: wireless then pdev. */

	ic = ieee80211_find_com(device_get_nameunit(pdev->dev.bsddev));
	if (ic != NULL) {
		error = lkpi_80211_suspend(ic, state);
	} else {
		device_printf(pdev->dev.bsddev,
		    "%s: WARNING: wireless device not found\n", __func__);
	}
	if (error != 0)
		goto err;

	/* Logic duplicated from linux_pci_suspend(). */
	pmops = pdev->pdrv->driver.pm;
	if (sc->suspend != NULL)
		error = sc->suspend(pdev, state);
	else if (pmops != NULL && pmops->suspend != NULL) {
		error = -pmops->suspend(&pdev->dev);
		if (error == 0 && pmops->suspend_late != NULL)
			error = -pmops->suspend_late(&pdev->dev);
		if (error == 0 && pmops->suspend_noirq != NULL)
			error = -pmops->suspend_noirq(&pdev->dev);
	}

err:
	if (error < 0)
		error = -error;

	if (error != 0)
		device_printf(pdev->dev.bsddev,
		    "%s: WARNING: SUSPEND FAILED: %d\n", __func__, error);

	return (error);
}

static int
lkpi_80211_pm_resume(struct pci_dev *pdev)
{
	const struct dev_pm_ops *pmops;
	struct lkpi_80211_pm_softc *sc;
	struct ieee80211com *ic;
	device_t dev;
	int error;

	dev = device_find_child(pdev->dev.bsddev, "lkpi80211_pm",
	    DEVICE_UNIT_ANY);
	if (dev == NULL) {
		/* Must not happen, so abort suspend if it does. */
		device_printf(pdev->dev.bsddev,
		    "%s: cannot find lkpi80211_pm child\n", __func__);
		return (ENXIO);
	}
	sc = device_get_softc(dev);
	error = 0;

	/* Call order: pdev then wireless. */

	/* Logic duplicated from linux_pci_resume(). */
	pmops = pdev->pdrv->driver.pm;
	if (sc->resume != NULL) {
		error = sc->resume(pdev);
	} else if (pmops != NULL && pmops->resume != NULL) {
		if (pmops->resume_early != NULL)
			error = -pmops->resume_early(&pdev->dev);
		if (error == 0 && pmops->resume != NULL)
			error = -pmops->resume(&pdev->dev);
	}
	if (error != 0)
		device_printf(pdev->dev.bsddev, "%s: resume failed!\n", __func__);
	/* Do not error out but give wireless also a chance. */

	ic = ieee80211_find_com(device_get_nameunit(pdev->dev.bsddev));
	if (ic != NULL) {
		error = lkpi_80211_resume(ic);
	} else {
		device_printf(pdev->dev.bsddev,
		    "%s: WARNING: wireless device not found\n", __func__);
	}

	if (error < 0)
		error = -error;

	return (error);
}

/* -------------------------------------------------------------------------- */
static void
lkpi_80211_pm_identify(driver_t *driver, device_t parent)
{

	/* Make sure we're not being doubly invoked per parent. */
	if (device_find_child(parent, driver->name, DEVICE_UNIT_ANY) != NULL)
		return;

	/* Make sure this is PCI for now. */
	if (!is_pci_device(parent))
		return;

	if (BUS_ADD_CHILD(parent, 0, driver->name, DEVICE_UNIT_ANY) == NULL)
		device_printf(parent, "%s: failed to add child\n", __func__);
}

static int
lkpi_80211_pm_probe(device_t dev)
{
	device_set_descf(dev, "LinuxKPI 802.11 %s mac80211 PM",
	    device_get_nameunit(device_get_parent(dev)));
	return (BUS_PROBE_DEFAULT);
}

static int
lkpi_80211_pm_attach(device_t dev)
{
	struct lkpi_80211_pm_softc *sc;
	struct pci_dev *pdev;

	sc = device_get_softc(dev);
	pdev = device_get_softc(device_get_parent(dev));

	/* Intercept the driver suspend/resume calls. */
	sc->suspend = pdev->pdrv->suspend;
	pdev->pdrv->suspend = lkpi_80211_pm_suspend;
	sc->resume = pdev->pdrv->resume;
	pdev->pdrv->resume = lkpi_80211_pm_resume;

	return (0);
}

static int
lkpi_80211_pm_detach(device_t dev)
{
	struct lkpi_80211_pm_softc *sc;
	struct pci_dev *pdev;

	sc = device_get_softc(dev);
	pdev = device_get_softc(device_get_parent(dev));

	/* Restore the original notifications. */
	pdev->pdrv->suspend = sc->suspend;
	pdev->pdrv->resume = sc->resume;

	return (0);
}

static device_method_t lkpi_80211_pm_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	lkpi_80211_pm_identify),
	DEVMETHOD(device_probe,		lkpi_80211_pm_probe),
	DEVMETHOD(device_attach,	lkpi_80211_pm_attach),
	DEVMETHOD(device_detach,	lkpi_80211_pm_detach),
	/*
	 * Do not think about device_suspend/resume here.
	 * We are not a PCI device and LinuxKPI PCI linux_pci_suspend/resume
	 * are getting the notifications so we have to hijack the
	 * LinuxKPI upcalls.
	 */

	DEVMETHOD_END
};

driver_t lkpi_80211_pm_driver = {
	"lkpi80211_pm",
	lkpi_80211_pm_methods,
	sizeof(struct lkpi_80211_pm_softc),
};

MODULE_DEPEND(lkpi80211_pm, linuxkpi_wlan, 1, 1, 1);
MODULE_VERSION(lkpi80211_pm, 1);
