/*-
 * SPDX-License-Identifier: ISC
 *
 * Copyright (c) 2020 Dr Robert Harvey Crowston <crowston@protonmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *
 * $FreeBSD$
 *
 */

/*
 * VIA VL805 controller on the Raspberry Pi 4.
 * The VL805 is a generic xhci controller. However, in the newer hardware
 * revisions of the Raspberry Pi 4, it is incapable of loading its own firmware.
 * Instead, the VideoCore GPU must load the firmware into the controller at the
 * appropriate time. This driver is a shim that pre-loads the firmware before
 * handing control to the xhci generic driver.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/unistd.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/priv.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_util.h>

#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>
#include <dev/usb/usb_pci.h>
#include <dev/usb/controller/xhci.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/broadcom/bcm2835/bcm2835_mbox_prop.h>

#define	VL805_FIRMWARE_REG	0x50
#define	PCIE_BUS_SHIFT		20
#define	PCIE_SLOT_SHIFT		15
#define	PCIE_FUNC_SHIFT		12

static int
bcm_xhci_probe(device_t dev)
{
	phandle_t root;
	uint32_t device_id;

	device_id = pci_get_devid(dev);
	if (device_id != 0x34831106) /* VIA VL805 USB 3.0 controller. */
		return (ENXIO);

	/*
	 * The VIA chip is not unique to the Pi, but we only want to use this
	 * driver if the SoC is a Raspberry Pi 4. Walk the device tree to
	 * discover if the system is a Pi 4.
	 */
	root = OF_finddevice("/");
	if (root == -1)
		return (ENXIO);
	if (!ofw_bus_node_is_compatible(root, "raspberrypi,4-model-b"))
		return (ENXIO);

	/*
	 * On the Pi 4, the VIA chip with the firmware-loading limitation is
	 * soldered-on to a particular bus/slot/function. But, it's possible a
	 * user could desolder the VIA chip, replace it with a pci-pci bridge,
	 * then plug in a commodity VIA PCI-e card on the new bridge. In that
	 * case we don't want to try to load the firmware to a commodity
	 * expansion card.
	 */
	if (pci_get_bus(dev) != 1 || pci_get_slot(dev) != 0 ||
	    pci_get_function(dev) != 0 )
		return (ENXIO);

	device_set_desc(dev,
	    "VL805 USB 3.0 controller (on the Raspberry Pi 4b)");

	return (BUS_PROBE_SPECIFIC);
}

static uint32_t
bcm_xhci_check_firmware(device_t dev, bool expect_loaded)
{
	uint32_t revision;
	bool loaded;

	revision = pci_read_config(dev, VL805_FIRMWARE_REG, 4);
	loaded = !(revision == 0 || revision == 0xffffffff);

	if (expect_loaded && !loaded)
		device_printf(dev, "warning: xhci firmware not found.\n");
	else if (bootverbose && !loaded)
		device_printf(dev, "note: xhci firmware not found.\n");
	else if (bootverbose)
		device_printf(dev,
		    "note: xhci firmware detected; firmware is revision %x.\n",
		     revision);

	if (!loaded)
		return 0;

	return (revision);
}

static void
bcm_xhci_install_xhci_firmware(device_t dev)
{
	uint32_t revision, dev_addr;
	int error;

	revision = bcm_xhci_check_firmware(dev, false);
	if (revision > 0) {
		/*
		 * With the pre-June 2020 boot firmware, it does not seem
		 * possible to reload already-installed xhci firmware.
		 */
		return;
	}

	/*
	 * Notify the VideoCore gpu processor that it needs to reload the xhci
	 * firmware into the xhci controller. This needs to happen after the pci
	 * bridge topology is registered with the controller.
	 */
	if (bootverbose)
		device_printf(dev, "note: installing xhci firmware.\n");

	dev_addr =
	    pci_get_bus(dev)      << PCIE_BUS_SHIFT |
	    pci_get_slot(dev)     << PCIE_SLOT_SHIFT |
	    pci_get_function(dev) << PCIE_FUNC_SHIFT;

	error = bcm2835_mbox_notify_xhci_reset(dev_addr);
	if (error)
		device_printf(dev,
		    "warning: xhci firmware install failed (error %d).\n",
		    error);

	DELAY(1000);
	bcm_xhci_check_firmware(dev, true);

	return;
}

static int
bcm_xhci_attach(device_t dev)
{
	struct xhci_softc *sc;
	int error;

	sc = device_get_softc(dev);

	bcm_xhci_install_xhci_firmware(dev);

	error = xhci_pci_attach(dev);
	if (error)
		return (error);

	/* 32 bit DMA is a limitation of the PCI-e controller, not the VL805. */
	sc->sc_bus.dma_bits = 32;
	if (bootverbose)
		device_printf(dev, "note: switched to 32-bit DMA.\n");

	return (0);
}

/*
 * Device method table.
 */
static device_method_t bcm_xhci_methods[] = {
	/* Device interface. */
	DEVMETHOD(device_probe,			bcm_xhci_probe),
	DEVMETHOD(device_attach,		bcm_xhci_attach),
};

DEFINE_CLASS_1(bcm_xhci, bcm_xhci_driver, bcm_xhci_methods,
    sizeof(struct xhci_softc), xhci_pci_driver);

static devclass_t xhci_devclass;
DRIVER_MODULE(bcm_xhci, pci, bcm_xhci_driver, xhci_devclass, 0, 0); MODULE_DEPEND(bcm_xhci, usb, 1, 1, 1);
