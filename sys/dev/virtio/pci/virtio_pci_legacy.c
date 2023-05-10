/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011, Bryan Venteicher <bryanv@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Driver for the legacy VirtIO PCI interface. */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/endian.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <dev/virtio/virtio.h>
#include <dev/virtio/virtqueue.h>
#include <dev/virtio/pci/virtio_pci.h>
#include <dev/virtio/pci/virtio_pci_legacy_var.h>

#include "virtio_bus_if.h"
#include "virtio_pci_if.h"
#include "virtio_if.h"

struct vtpci_legacy_softc {
	device_t			 vtpci_dev;
	struct vtpci_common		 vtpci_common;
	int				 vtpci_res_type;
	struct resource			*vtpci_res;
	struct resource			*vtpci_msix_table_res;
	struct resource			*vtpci_msix_pba_res;
};

static int	vtpci_legacy_probe(device_t);
static int	vtpci_legacy_attach(device_t);
static int	vtpci_legacy_detach(device_t);
static int	vtpci_legacy_suspend(device_t);
static int	vtpci_legacy_resume(device_t);
static int	vtpci_legacy_shutdown(device_t);

static void	vtpci_legacy_driver_added(device_t, driver_t *);
static void	vtpci_legacy_child_detached(device_t, device_t);
static int	vtpci_legacy_read_ivar(device_t, device_t, int, uintptr_t *);
static int	vtpci_legacy_write_ivar(device_t, device_t, int, uintptr_t);

static uint8_t	vtpci_legacy_read_isr(device_t);
static uint16_t	vtpci_legacy_get_vq_size(device_t, int);
static bus_size_t vtpci_legacy_get_vq_notify_off(device_t, int);
static void	vtpci_legacy_set_vq(device_t, struct virtqueue *);
static void	vtpci_legacy_disable_vq(device_t, int);
static int	vtpci_legacy_register_cfg_msix(device_t,
		    struct vtpci_interrupt *);
static int	vtpci_legacy_register_vq_msix(device_t, int idx,
		    struct vtpci_interrupt *);

static uint64_t	vtpci_legacy_negotiate_features(device_t, uint64_t);
static int	vtpci_legacy_with_feature(device_t, uint64_t);
static int	vtpci_legacy_alloc_virtqueues(device_t, int, int,
		    struct vq_alloc_info *);
static int	vtpci_legacy_setup_interrupts(device_t, enum intr_type);
static void	vtpci_legacy_stop(device_t);
static int	vtpci_legacy_reinit(device_t, uint64_t);
static void	vtpci_legacy_reinit_complete(device_t);
static void	vtpci_legacy_notify_vq(device_t, uint16_t, bus_size_t);
static void	vtpci_legacy_read_dev_config(device_t, bus_size_t, void *, int);
static void	vtpci_legacy_write_dev_config(device_t, bus_size_t, const void *, int);

static bool	vtpci_legacy_setup_msix(struct vtpci_legacy_softc *sc);
static void	vtpci_legacy_teardown_msix(struct vtpci_legacy_softc *sc);
static int	vtpci_legacy_alloc_resources(struct vtpci_legacy_softc *);
static void	vtpci_legacy_free_resources(struct vtpci_legacy_softc *);

static void	vtpci_legacy_probe_and_attach_child(struct vtpci_legacy_softc *);

static uint8_t	vtpci_legacy_get_status(struct vtpci_legacy_softc *);
static void	vtpci_legacy_set_status(struct vtpci_legacy_softc *, uint8_t);
static void	vtpci_legacy_select_virtqueue(struct vtpci_legacy_softc *, int);
static void	vtpci_legacy_reset(struct vtpci_legacy_softc *);

#define VIRTIO_PCI_LEGACY_CONFIG(_sc) \
    VIRTIO_PCI_CONFIG_OFF(vtpci_is_msix_enabled(&(_sc)->vtpci_common))

#define vtpci_legacy_read_config_1(sc, o) \
    bus_read_1((sc)->vtpci_res, (o))
#define vtpci_legacy_write_config_1(sc, o, v) \
    bus_write_1((sc)->vtpci_res, (o), (v))
/*
 * VirtIO specifies that PCI Configuration area is guest endian. However,
 * since PCI devices are inherently little-endian, on big-endian systems
 * the bus layer transparently converts it to BE. For virtio-legacy, this
 * conversion is undesired, so an extra byte swap is required to fix it.
 */
#define vtpci_legacy_read_config_2(sc, o) \
    le16toh(bus_read_2((sc)->vtpci_res, (o)))
#define vtpci_legacy_read_config_4(sc, o) \
    le32toh(bus_read_4((sc)->vtpci_res, (o)))
#define vtpci_legacy_write_config_2(sc, o, v) \
    bus_write_2((sc)->vtpci_res, (o), (htole16(v)))
#define vtpci_legacy_write_config_4(sc, o, v) \
    bus_write_4((sc)->vtpci_res, (o), (htole32(v)))
/* PCI Header LE. On BE systems the bus layer takes care of byte swapping. */
#define vtpci_legacy_read_header_2(sc, o) \
    bus_read_2((sc)->vtpci_res, (o))
#define vtpci_legacy_read_header_4(sc, o) \
    bus_read_4((sc)->vtpci_res, (o))
#define vtpci_legacy_write_header_2(sc, o, v) \
    bus_write_2((sc)->vtpci_res, (o), (v))
#define vtpci_legacy_write_header_4(sc, o, v) \
    bus_write_4((sc)->vtpci_res, (o), (v))

static device_method_t vtpci_legacy_methods[] = {
	/* Device interface. */
	DEVMETHOD(device_probe,			  vtpci_legacy_probe),
	DEVMETHOD(device_attach,		  vtpci_legacy_attach),
	DEVMETHOD(device_detach,		  vtpci_legacy_detach),
	DEVMETHOD(device_suspend,		  vtpci_legacy_suspend),
	DEVMETHOD(device_resume,		  vtpci_legacy_resume),
	DEVMETHOD(device_shutdown,		  vtpci_legacy_shutdown),

	/* Bus interface. */
	DEVMETHOD(bus_driver_added,		  vtpci_legacy_driver_added),
	DEVMETHOD(bus_child_detached,		  vtpci_legacy_child_detached),
	DEVMETHOD(bus_child_pnpinfo,		  virtio_child_pnpinfo),
	DEVMETHOD(bus_read_ivar,		  vtpci_legacy_read_ivar),
	DEVMETHOD(bus_write_ivar,		  vtpci_legacy_write_ivar),

	/* VirtIO PCI interface. */
	DEVMETHOD(virtio_pci_read_isr,		 vtpci_legacy_read_isr),
	DEVMETHOD(virtio_pci_get_vq_size,	 vtpci_legacy_get_vq_size),
	DEVMETHOD(virtio_pci_get_vq_notify_off,	 vtpci_legacy_get_vq_notify_off),
	DEVMETHOD(virtio_pci_set_vq,		 vtpci_legacy_set_vq),
	DEVMETHOD(virtio_pci_disable_vq,	 vtpci_legacy_disable_vq),
	DEVMETHOD(virtio_pci_register_cfg_msix,  vtpci_legacy_register_cfg_msix),
	DEVMETHOD(virtio_pci_register_vq_msix,	 vtpci_legacy_register_vq_msix),

	/* VirtIO bus interface. */
	DEVMETHOD(virtio_bus_negotiate_features,  vtpci_legacy_negotiate_features),
	DEVMETHOD(virtio_bus_with_feature,	  vtpci_legacy_with_feature),
	DEVMETHOD(virtio_bus_alloc_virtqueues,	  vtpci_legacy_alloc_virtqueues),
	DEVMETHOD(virtio_bus_setup_intr,	  vtpci_legacy_setup_interrupts),
	DEVMETHOD(virtio_bus_stop,		  vtpci_legacy_stop),
	DEVMETHOD(virtio_bus_reinit,		  vtpci_legacy_reinit),
	DEVMETHOD(virtio_bus_reinit_complete,	  vtpci_legacy_reinit_complete),
	DEVMETHOD(virtio_bus_notify_vq,		  vtpci_legacy_notify_vq),
	DEVMETHOD(virtio_bus_read_device_config,  vtpci_legacy_read_dev_config),
	DEVMETHOD(virtio_bus_write_device_config, vtpci_legacy_write_dev_config),

	DEVMETHOD_END
};

static driver_t vtpci_legacy_driver = {
	.name = "virtio_pci",
	.methods = vtpci_legacy_methods,
	.size = sizeof(struct vtpci_legacy_softc)
};

DRIVER_MODULE(virtio_pci_legacy, pci, vtpci_legacy_driver, 0, 0);

static int
vtpci_legacy_probe(device_t dev)
{
	char desc[64];
	const char *name;

	if (pci_get_vendor(dev) != VIRTIO_PCI_VENDORID)
		return (ENXIO);

	if (pci_get_device(dev) < VIRTIO_PCI_DEVICEID_MIN ||
	    pci_get_device(dev) > VIRTIO_PCI_DEVICEID_LEGACY_MAX)
		return (ENXIO);

	if (pci_get_revid(dev) != VIRTIO_PCI_ABI_VERSION)
		return (ENXIO);

	name = virtio_device_name(pci_get_subdevice(dev));
	if (name == NULL)
		name = "Unknown";

	snprintf(desc, sizeof(desc), "VirtIO PCI (legacy) %s adapter", name);
	device_set_desc_copy(dev, desc);

	/* Prefer transitional modern VirtIO PCI. */
	return (BUS_PROBE_LOW_PRIORITY);
}

static int
vtpci_legacy_attach(device_t dev)
{
	struct vtpci_legacy_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->vtpci_dev = dev;
	vtpci_init(&sc->vtpci_common, dev, false);

	error = vtpci_legacy_alloc_resources(sc);
	if (error) {
		device_printf(dev, "cannot map I/O space nor memory space\n");
		return (error);
	}

	if (vtpci_is_msix_available(&sc->vtpci_common) &&
	    !vtpci_legacy_setup_msix(sc)) {
		device_printf(dev, "cannot setup MSI-x resources\n");
		error = ENXIO;
		goto fail;
	}

	vtpci_legacy_reset(sc);

	/* Tell the host we've noticed this device. */
	vtpci_legacy_set_status(sc, VIRTIO_CONFIG_STATUS_ACK);

	error = vtpci_add_child(&sc->vtpci_common);
	if (error)
		goto fail;

	vtpci_legacy_probe_and_attach_child(sc);

	return (0);

fail:
	vtpci_legacy_set_status(sc, VIRTIO_CONFIG_STATUS_FAILED);
	vtpci_legacy_detach(dev);

	return (error);
}

static int
vtpci_legacy_detach(device_t dev)
{
	struct vtpci_legacy_softc *sc;
	int error;

	sc = device_get_softc(dev);

	error = vtpci_delete_child(&sc->vtpci_common);
	if (error)
		return (error);

	vtpci_legacy_reset(sc);
	vtpci_legacy_teardown_msix(sc);
	vtpci_legacy_free_resources(sc);

	return (0);
}

static int
vtpci_legacy_suspend(device_t dev)
{
	return (bus_generic_suspend(dev));
}

static int
vtpci_legacy_resume(device_t dev)
{
	return (bus_generic_resume(dev));
}

static int
vtpci_legacy_shutdown(device_t dev)
{
	(void) bus_generic_shutdown(dev);
	/* Forcibly stop the host device. */
	vtpci_legacy_stop(dev);

	return (0);
}

static void
vtpci_legacy_driver_added(device_t dev, driver_t *driver)
{
	vtpci_legacy_probe_and_attach_child(device_get_softc(dev));
}

static void
vtpci_legacy_child_detached(device_t dev, device_t child)
{
	struct vtpci_legacy_softc *sc;

	sc = device_get_softc(dev);

	vtpci_legacy_reset(sc);
	vtpci_child_detached(&sc->vtpci_common);

	/* After the reset, retell the host we've noticed this device. */
	vtpci_legacy_set_status(sc, VIRTIO_CONFIG_STATUS_ACK);
}

static int
vtpci_legacy_read_ivar(device_t dev, device_t child, int index,
    uintptr_t *result)
{
	struct vtpci_legacy_softc *sc;
	struct vtpci_common *cn;

	sc = device_get_softc(dev);
	cn = &sc->vtpci_common;

	if (vtpci_child_device(cn) != child)
		return (ENOENT);

	switch (index) {
	case VIRTIO_IVAR_DEVTYPE:
		*result = pci_get_subdevice(dev);
		break;
	default:
		return (vtpci_read_ivar(cn, index, result));
	}

	return (0);
}

static int
vtpci_legacy_write_ivar(device_t dev, device_t child, int index, uintptr_t value)
{
	struct vtpci_legacy_softc *sc;
	struct vtpci_common *cn;

	sc = device_get_softc(dev);
	cn = &sc->vtpci_common;

	if (vtpci_child_device(cn) != child)
		return (ENOENT);

	switch (index) {
	default:
		return (vtpci_write_ivar(cn, index, value));
	}

	return (0);
}

static uint64_t
vtpci_legacy_negotiate_features(device_t dev, uint64_t child_features)
{
	struct vtpci_legacy_softc *sc;
	uint64_t host_features, features;

	sc = device_get_softc(dev);
	host_features = vtpci_legacy_read_header_4(sc, VIRTIO_PCI_HOST_FEATURES);

	features = vtpci_negotiate_features(&sc->vtpci_common,
	    child_features, host_features);
	vtpci_legacy_write_header_4(sc, VIRTIO_PCI_GUEST_FEATURES, features);

	return (features);
}

static int
vtpci_legacy_with_feature(device_t dev, uint64_t feature)
{
	struct vtpci_legacy_softc *sc;

	sc = device_get_softc(dev);

	return (vtpci_with_feature(&sc->vtpci_common, feature));
}

static int
vtpci_legacy_alloc_virtqueues(device_t dev, int flags, int nvqs,
    struct vq_alloc_info *vq_info)
{
	struct vtpci_legacy_softc *sc;
	struct vtpci_common *cn;

	sc = device_get_softc(dev);
	cn = &sc->vtpci_common;

	return (vtpci_alloc_virtqueues(cn, flags, nvqs, vq_info));
}

static int
vtpci_legacy_setup_interrupts(device_t dev, enum intr_type type)
{
	struct vtpci_legacy_softc *sc;

	sc = device_get_softc(dev);

	return (vtpci_setup_interrupts(&sc->vtpci_common, type));
}

static void
vtpci_legacy_stop(device_t dev)
{
	vtpci_legacy_reset(device_get_softc(dev));
}

static int
vtpci_legacy_reinit(device_t dev, uint64_t features)
{
	struct vtpci_legacy_softc *sc;
	struct vtpci_common *cn;
	int error;

	sc = device_get_softc(dev);
	cn = &sc->vtpci_common;

	/*
	 * Redrive the device initialization. This is a bit of an abuse of
	 * the specification, but VirtualBox, QEMU/KVM, and BHyVe seem to
	 * play nice.
	 *
	 * We do not allow the host device to change from what was originally
	 * negotiated beyond what the guest driver changed. MSIX state should
	 * not change, number of virtqueues and their size remain the same, etc.
	 * This will need to be rethought when we want to support migration.
	 */

	if (vtpci_legacy_get_status(sc) != VIRTIO_CONFIG_STATUS_RESET)
		vtpci_legacy_stop(dev);

	/*
	 * Quickly drive the status through ACK and DRIVER. The device does
	 * not become usable again until DRIVER_OK in reinit complete.
	 */
	vtpci_legacy_set_status(sc, VIRTIO_CONFIG_STATUS_ACK);
	vtpci_legacy_set_status(sc, VIRTIO_CONFIG_STATUS_DRIVER);

	vtpci_legacy_negotiate_features(dev, features);

	error = vtpci_reinit(cn);
	if (error)
		return (error);

	return (0);
}

static void
vtpci_legacy_reinit_complete(device_t dev)
{
	struct vtpci_legacy_softc *sc;

	sc = device_get_softc(dev);

	vtpci_legacy_set_status(sc, VIRTIO_CONFIG_STATUS_DRIVER_OK);
}

static void
vtpci_legacy_notify_vq(device_t dev, uint16_t queue, bus_size_t offset)
{
	struct vtpci_legacy_softc *sc;

	sc = device_get_softc(dev);
	MPASS(offset == VIRTIO_PCI_QUEUE_NOTIFY);

	vtpci_legacy_write_header_2(sc, offset, queue);
}

static uint8_t
vtpci_legacy_get_status(struct vtpci_legacy_softc *sc)
{
	return (vtpci_legacy_read_config_1(sc, VIRTIO_PCI_STATUS));
}

static void
vtpci_legacy_set_status(struct vtpci_legacy_softc *sc, uint8_t status)
{
	if (status != VIRTIO_CONFIG_STATUS_RESET)
		status |= vtpci_legacy_get_status(sc);

	vtpci_legacy_write_config_1(sc, VIRTIO_PCI_STATUS, status);
}

static void
vtpci_legacy_read_dev_config(device_t dev, bus_size_t offset,
    void *dst, int length)
{
	struct vtpci_legacy_softc *sc;
	bus_size_t off;
	uint8_t *d;
	int i;

	sc = device_get_softc(dev);
	off = VIRTIO_PCI_LEGACY_CONFIG(sc) + offset;

	d = dst;
	for (i = 0; i < length; i++) {
		d[i] = vtpci_legacy_read_config_1(sc, off + i);
	}
}

static void
vtpci_legacy_write_dev_config(device_t dev, bus_size_t offset,
    const void *src, int length)
{
	struct vtpci_legacy_softc *sc;
	bus_size_t off;
	const uint8_t *s;
	int i;

	sc = device_get_softc(dev);
	off = VIRTIO_PCI_LEGACY_CONFIG(sc) + offset;

	s = src;
	for (i = 0; i < length; i++) {
		vtpci_legacy_write_config_1(sc, off + i, s[i]);
	}
}

static bool
vtpci_legacy_setup_msix(struct vtpci_legacy_softc *sc)
{
	device_t dev;
	int rid, table_rid;

	dev = sc->vtpci_dev;

	rid = table_rid = pci_msix_table_bar(dev);
	if (rid != PCIR_BAR(0)) {
		sc->vtpci_msix_table_res = bus_alloc_resource_any(
		    dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
		if (sc->vtpci_msix_table_res == NULL)
			return (false);
	}

	rid = pci_msix_pba_bar(dev);
	if (rid != table_rid && rid != PCIR_BAR(0)) {
		sc->vtpci_msix_pba_res = bus_alloc_resource_any(
		    dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
		if (sc->vtpci_msix_pba_res == NULL)
			return (false);
	}

	return (true);
}

static void
vtpci_legacy_teardown_msix(struct vtpci_legacy_softc *sc)
{
	device_t dev;

	dev = sc->vtpci_dev;

	if (sc->vtpci_msix_pba_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->vtpci_msix_pba_res),
		    sc->vtpci_msix_pba_res);
		sc->vtpci_msix_pba_res = NULL;
	}
	if (sc->vtpci_msix_table_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->vtpci_msix_table_res),
		    sc->vtpci_msix_table_res);
		sc->vtpci_msix_table_res = NULL;
	}
}

static int
vtpci_legacy_alloc_resources(struct vtpci_legacy_softc *sc)
{
	const int res_types[] = { SYS_RES_IOPORT, SYS_RES_MEMORY };
	device_t dev;
	int rid, i;

	dev = sc->vtpci_dev;
	
	/*
	 * Most hypervisors export the common configuration structure in IO
	 * space, but some use memory space; try both.
	 */
	for (i = 0; nitems(res_types); i++) {
		rid = PCIR_BAR(0);
		sc->vtpci_res_type = res_types[i];
		sc->vtpci_res = bus_alloc_resource_any(dev, res_types[i], &rid,
		    RF_ACTIVE);
		if (sc->vtpci_res != NULL)
			break;
	}
	if (sc->vtpci_res == NULL)
		return (ENXIO);

	return (0);
}

static void
vtpci_legacy_free_resources(struct vtpci_legacy_softc *sc)
{
	device_t dev;

	dev = sc->vtpci_dev;

	if (sc->vtpci_res != NULL) {
		bus_release_resource(dev, sc->vtpci_res_type, PCIR_BAR(0),
		    sc->vtpci_res);
		sc->vtpci_res = NULL;
	}
}

static void
vtpci_legacy_probe_and_attach_child(struct vtpci_legacy_softc *sc)
{
	device_t dev, child;

	dev = sc->vtpci_dev;
	child = vtpci_child_device(&sc->vtpci_common);

	if (child == NULL || device_get_state(child) != DS_NOTPRESENT)
		return;

	if (device_probe(child) != 0)
		return;

	vtpci_legacy_set_status(sc, VIRTIO_CONFIG_STATUS_DRIVER);

	if (device_attach(child) != 0) {
		vtpci_legacy_set_status(sc, VIRTIO_CONFIG_STATUS_FAILED);
		/* Reset status for future attempt. */
		vtpci_legacy_child_detached(dev, child);
	} else {
		vtpci_legacy_set_status(sc, VIRTIO_CONFIG_STATUS_DRIVER_OK);
		VIRTIO_ATTACH_COMPLETED(child);
	}
}

static int
vtpci_legacy_register_msix(struct vtpci_legacy_softc *sc, int offset,
    struct vtpci_interrupt *intr)
{
	uint16_t vector;

	if (intr != NULL) {
		/* Map from guest rid to host vector. */
		vector = intr->vti_rid - 1;
	} else
		vector = VIRTIO_MSI_NO_VECTOR;

	vtpci_legacy_write_header_2(sc, offset, vector);
	return (vtpci_legacy_read_header_2(sc, offset) == vector ? 0 : ENODEV);
}

static int
vtpci_legacy_register_cfg_msix(device_t dev, struct vtpci_interrupt *intr)
{
	struct vtpci_legacy_softc *sc;
	int error;

	sc = device_get_softc(dev);

	error = vtpci_legacy_register_msix(sc, VIRTIO_MSI_CONFIG_VECTOR, intr);
	if (error) {
		device_printf(dev,
		    "unable to register config MSIX interrupt\n");
		return (error);
	}

	return (0);
}

static int
vtpci_legacy_register_vq_msix(device_t dev, int idx,
    struct vtpci_interrupt *intr)
{
	struct vtpci_legacy_softc *sc;
	int error;

	sc = device_get_softc(dev);

	vtpci_legacy_select_virtqueue(sc, idx);
	error = vtpci_legacy_register_msix(sc, VIRTIO_MSI_QUEUE_VECTOR, intr);
	if (error) {
		device_printf(dev,
		    "unable to register virtqueue MSIX interrupt\n");
		return (error);
	}

	return (0);
}

static void
vtpci_legacy_reset(struct vtpci_legacy_softc *sc)
{
	/*
	 * Setting the status to RESET sets the host device to the
	 * original, uninitialized state.
	 */
	vtpci_legacy_set_status(sc, VIRTIO_CONFIG_STATUS_RESET);
	(void) vtpci_legacy_get_status(sc);
}

static void
vtpci_legacy_select_virtqueue(struct vtpci_legacy_softc *sc, int idx)
{
	vtpci_legacy_write_header_2(sc, VIRTIO_PCI_QUEUE_SEL, idx);
}

static uint8_t
vtpci_legacy_read_isr(device_t dev)
{
	struct vtpci_legacy_softc *sc;

	sc = device_get_softc(dev);

	return (vtpci_legacy_read_config_1(sc, VIRTIO_PCI_ISR));
}

static uint16_t
vtpci_legacy_get_vq_size(device_t dev, int idx)
{
	struct vtpci_legacy_softc *sc;

	sc = device_get_softc(dev);

	vtpci_legacy_select_virtqueue(sc, idx);
	return (vtpci_legacy_read_header_2(sc, VIRTIO_PCI_QUEUE_NUM));
}

static bus_size_t
vtpci_legacy_get_vq_notify_off(device_t dev, int idx)
{
	return (VIRTIO_PCI_QUEUE_NOTIFY);
}

static void
vtpci_legacy_set_vq(device_t dev, struct virtqueue *vq)
{
	struct vtpci_legacy_softc *sc;

	sc = device_get_softc(dev);

	vtpci_legacy_select_virtqueue(sc, virtqueue_index(vq));
	vtpci_legacy_write_header_4(sc, VIRTIO_PCI_QUEUE_PFN,
	    virtqueue_paddr(vq) >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);
}

static void
vtpci_legacy_disable_vq(device_t dev, int idx)
{
	struct vtpci_legacy_softc *sc;

	sc = device_get_softc(dev);

	vtpci_legacy_select_virtqueue(sc, idx);
	vtpci_legacy_write_header_4(sc, VIRTIO_PCI_QUEUE_PFN, 0);
}
