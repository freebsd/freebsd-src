/*-
 * Copyright (c) 2011, Bryan Venteicher <bryanv@daemoninthecloset.org>
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

/* Driver for the VirtIO PCI interface. */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <dev/virtio/virtio.h>
#include <dev/virtio/virtqueue.h>
#include <dev/virtio/pci/virtio_pci.h>

#include "virtio_bus_if.h"
#include "virtio_if.h"

struct vtpci_softc {
	device_t			 vtpci_dev;
	struct resource			*vtpci_res;
	struct resource			*vtpci_msix_res;
	uint64_t			 vtpci_features;
	uint32_t			 vtpci_flags;
#define VIRTIO_PCI_FLAG_NO_MSI		 0x0001
#define VIRTIO_PCI_FLAG_MSI		 0x0002
#define VIRTIO_PCI_FLAG_NO_MSIX		 0x0010
#define VIRTIO_PCI_FLAG_MSIX		 0x0020
#define VIRTIO_PCI_FLAG_SHARED_MSIX	 0x0040

	device_t			 vtpci_child_dev;
	struct virtio_feature_desc	*vtpci_child_feat_desc;

	/*
	 * Ideally, each virtqueue that the driver provides a callback for
	 * will receive its own MSIX vector. If there are not sufficient
	 * vectors available, we will then attempt to have all the VQs
	 * share one vector. Note that when using MSIX, the configuration
	 * changed notifications must be on their own vector.
	 *
	 * If MSIX is not available, we will attempt to have the whole
	 * device share one MSI vector, and then, finally, one legacy
	 * interrupt.
	 */
	int				 vtpci_nvqs;
	struct vtpci_virtqueue {
		struct virtqueue *vq;

		/* Index into vtpci_intr_res[] below. Unused, then -1. */
		int		  ires_idx;
	} vtpci_vqx[VIRTIO_MAX_VIRTQUEUES];

	/*
	 * When using MSIX interrupts, the first element of vtpci_intr_res[]
	 * is always the configuration changed notifications. The remaining
	 * element(s) are used for the virtqueues.
	 *
	 * With MSI and legacy interrupts, only the first element of
	 * vtpci_intr_res[] is used.
	 */
	int				 vtpci_nintr_res;
	struct vtpci_intr_resource {
		struct resource	*irq;
		int		 rid;
		void		*intrhand;
	} vtpci_intr_res[1 + VIRTIO_MAX_VIRTQUEUES];
};

static int	vtpci_probe(device_t);
static int	vtpci_attach(device_t);
static int	vtpci_detach(device_t);
static int	vtpci_suspend(device_t);
static int	vtpci_resume(device_t);
static int	vtpci_shutdown(device_t);
static void	vtpci_driver_added(device_t, driver_t *);
static void	vtpci_child_detached(device_t, device_t);
static int	vtpci_read_ivar(device_t, device_t, int, uintptr_t *);
static int	vtpci_write_ivar(device_t, device_t, int, uintptr_t);

static uint64_t	vtpci_negotiate_features(device_t, uint64_t);
static int	vtpci_with_feature(device_t, uint64_t);
static int	vtpci_alloc_virtqueues(device_t, int, int,
		    struct vq_alloc_info *);
static int	vtpci_setup_intr(device_t, enum intr_type);
static void	vtpci_stop(device_t);
static int	vtpci_reinit(device_t, uint64_t);
static void	vtpci_reinit_complete(device_t);
static void	vtpci_notify_virtqueue(device_t, uint16_t);
static uint8_t	vtpci_get_status(device_t);
static void	vtpci_set_status(device_t, uint8_t);
static void	vtpci_read_dev_config(device_t, bus_size_t, void *, int);
static void	vtpci_write_dev_config(device_t, bus_size_t, void *, int);

static void	vtpci_describe_features(struct vtpci_softc *, const char *,
		    uint64_t);
static void	vtpci_probe_and_attach_child(struct vtpci_softc *);

static int	vtpci_alloc_interrupts(struct vtpci_softc *, int, int,
		    struct vq_alloc_info *);
static int	vtpci_alloc_intr_resources(struct vtpci_softc *, int,
		    struct vq_alloc_info *);
static int	vtpci_alloc_msi(struct vtpci_softc *);
static int	vtpci_alloc_msix(struct vtpci_softc *, int);
static int	vtpci_register_msix_vector(struct vtpci_softc *, int, int);

static void	vtpci_free_interrupts(struct vtpci_softc *);
static void	vtpci_free_virtqueues(struct vtpci_softc *);
static void	vtpci_release_child_resources(struct vtpci_softc *);
static void	vtpci_reset(struct vtpci_softc *);

static int	vtpci_legacy_intr(void *);
static int	vtpci_vq_shared_intr(void *);
static int	vtpci_vq_intr(void *);
static int	vtpci_config_intr(void *);

/*
 * I/O port read/write wrappers.
 */
#define vtpci_read_config_1(sc, o)	bus_read_1((sc)->vtpci_res, (o))
#define vtpci_read_config_2(sc, o)	bus_read_2((sc)->vtpci_res, (o))
#define vtpci_read_config_4(sc, o)	bus_read_4((sc)->vtpci_res, (o))
#define vtpci_write_config_1(sc, o, v)	bus_write_1((sc)->vtpci_res, (o), (v))
#define vtpci_write_config_2(sc, o, v)	bus_write_2((sc)->vtpci_res, (o), (v))
#define vtpci_write_config_4(sc, o, v)	bus_write_4((sc)->vtpci_res, (o), (v))

/* Tunables. */
static int vtpci_disable_msix = 0;
TUNABLE_INT("hw.virtio.pci.disable_msix", &vtpci_disable_msix);

static device_method_t vtpci_methods[] = {
	/* Device interface. */
	DEVMETHOD(device_probe,			  vtpci_probe),
	DEVMETHOD(device_attach,		  vtpci_attach),
	DEVMETHOD(device_detach,		  vtpci_detach),
	DEVMETHOD(device_suspend,		  vtpci_suspend),
	DEVMETHOD(device_resume,		  vtpci_resume),
	DEVMETHOD(device_shutdown,		  vtpci_shutdown),

	/* Bus interface. */
	DEVMETHOD(bus_driver_added,		  vtpci_driver_added),
	DEVMETHOD(bus_child_detached,		  vtpci_child_detached),
	DEVMETHOD(bus_read_ivar,		  vtpci_read_ivar),
	DEVMETHOD(bus_write_ivar,		  vtpci_write_ivar),

	/* VirtIO bus interface. */
	DEVMETHOD(virtio_bus_negotiate_features,  vtpci_negotiate_features),
	DEVMETHOD(virtio_bus_with_feature,	  vtpci_with_feature),
	DEVMETHOD(virtio_bus_alloc_virtqueues,	  vtpci_alloc_virtqueues),
	DEVMETHOD(virtio_bus_setup_intr,	  vtpci_setup_intr),
	DEVMETHOD(virtio_bus_stop,		  vtpci_stop),
	DEVMETHOD(virtio_bus_reinit,		  vtpci_reinit),
	DEVMETHOD(virtio_bus_reinit_complete,	  vtpci_reinit_complete),
	DEVMETHOD(virtio_bus_notify_vq,		  vtpci_notify_virtqueue),
	DEVMETHOD(virtio_bus_read_device_config,  vtpci_read_dev_config),
	DEVMETHOD(virtio_bus_write_device_config, vtpci_write_dev_config),

	{ 0, 0 }
};

static driver_t vtpci_driver = {
	"virtio_pci",
	vtpci_methods,
	sizeof(struct vtpci_softc)
};

devclass_t vtpci_devclass;

DRIVER_MODULE(virtio_pci, pci, vtpci_driver, vtpci_devclass, 0, 0);
MODULE_VERSION(virtio_pci, 1);
MODULE_DEPEND(virtio_pci, pci, 1, 1, 1);
MODULE_DEPEND(virtio_pci, virtio, 1, 1, 1);

static int
vtpci_probe(device_t dev)
{
	char desc[36];
	const char *name;

	if (pci_get_vendor(dev) != VIRTIO_PCI_VENDORID)
		return (ENXIO);

	if (pci_get_device(dev) < VIRTIO_PCI_DEVICEID_MIN ||
	    pci_get_device(dev) > VIRTIO_PCI_DEVICEID_MAX)
		return (ENXIO);

	if (pci_get_revid(dev) != VIRTIO_PCI_ABI_VERSION)
		return (ENXIO);

	name = virtio_device_name(pci_get_subdevice(dev));
	if (name == NULL)
		name = "Unknown";

	snprintf(desc, sizeof(desc), "VirtIO PCI %s adapter", name);
	device_set_desc_copy(dev, desc);

	return (BUS_PROBE_DEFAULT);
}

static int
vtpci_attach(device_t dev)
{
	struct vtpci_softc *sc;
	device_t child;
	int rid;

	sc = device_get_softc(dev);
	sc->vtpci_dev = dev;

	pci_enable_busmaster(dev);

	rid = PCIR_BAR(0);
	sc->vtpci_res = bus_alloc_resource_any(dev, SYS_RES_IOPORT, &rid,
	    RF_ACTIVE);
	if (sc->vtpci_res == NULL) {
		device_printf(dev, "cannot map I/O space\n");
		return (ENXIO);
	}

	if (pci_find_extcap(dev, PCIY_MSI, NULL) != 0)
		sc->vtpci_flags |= VIRTIO_PCI_FLAG_NO_MSI;

	if (pci_find_extcap(dev, PCIY_MSIX, NULL) == 0) {
		rid = PCIR_BAR(1);
		sc->vtpci_msix_res = bus_alloc_resource_any(dev,
		    SYS_RES_MEMORY, &rid, RF_ACTIVE);
	}

	if (sc->vtpci_msix_res == NULL)
		sc->vtpci_flags |= VIRTIO_PCI_FLAG_NO_MSIX;

	vtpci_reset(sc);

	/* Tell the host we've noticed this device. */
	vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_ACK);

	if ((child = device_add_child(dev, NULL, -1)) == NULL) {
		device_printf(dev, "cannot create child device\n");
		vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_FAILED);
		vtpci_detach(dev);
		return (ENOMEM);
	}

	sc->vtpci_child_dev = child;
	vtpci_probe_and_attach_child(sc);

	return (0);
}

static int
vtpci_detach(device_t dev)
{
	struct vtpci_softc *sc;
	device_t child;
	int error;

	sc = device_get_softc(dev);

	if ((child = sc->vtpci_child_dev) != NULL) {
		error = device_delete_child(dev, child);
		if (error)
			return (error);
		sc->vtpci_child_dev = NULL;
	}

	vtpci_reset(sc);

	if (sc->vtpci_msix_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, PCIR_BAR(1),
		    sc->vtpci_msix_res);
		sc->vtpci_msix_res = NULL;
	}

	if (sc->vtpci_res != NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT, PCIR_BAR(0),
		    sc->vtpci_res);
		sc->vtpci_res = NULL;
	}

	return (0);
}

static int
vtpci_suspend(device_t dev)
{

	return (bus_generic_suspend(dev));
}

static int
vtpci_resume(device_t dev)
{

	return (bus_generic_resume(dev));
}

static int
vtpci_shutdown(device_t dev)
{

	(void) bus_generic_shutdown(dev);
	/* Forcibly stop the host device. */
	vtpci_stop(dev);

	return (0);
}

static void
vtpci_driver_added(device_t dev, driver_t *driver)
{
	struct vtpci_softc *sc;

	sc = device_get_softc(dev);

	vtpci_probe_and_attach_child(sc);
}

static void
vtpci_child_detached(device_t dev, device_t child)
{
	struct vtpci_softc *sc;

	sc = device_get_softc(dev);

	vtpci_reset(sc);
	vtpci_release_child_resources(sc);
}

static int
vtpci_read_ivar(device_t dev, device_t child, int index, uintptr_t *result)
{
	struct vtpci_softc *sc;

	sc = device_get_softc(dev);

	if (sc->vtpci_child_dev != child)
		return (ENOENT);

	switch (index) {
	case VIRTIO_IVAR_DEVTYPE:
		*result = pci_get_subdevice(dev);
		break;
	default:
		return (ENOENT);
	}

	return (0);
}

static int
vtpci_write_ivar(device_t dev, device_t child, int index, uintptr_t value)
{
	struct vtpci_softc *sc;

	sc = device_get_softc(dev);

	if (sc->vtpci_child_dev != child)
		return (ENOENT);

	switch (index) {
	case VIRTIO_IVAR_FEATURE_DESC:
		sc->vtpci_child_feat_desc = (void *) value;
		break;
	default:
		return (ENOENT);
	}

	return (0);
}

static uint64_t
vtpci_negotiate_features(device_t dev, uint64_t child_features)
{
	struct vtpci_softc *sc;
	uint64_t host_features, features;

	sc = device_get_softc(dev);

	host_features = vtpci_read_config_4(sc, VIRTIO_PCI_HOST_FEATURES);
	vtpci_describe_features(sc, "host", host_features);

	/*
	 * Limit negotiated features to what the driver, virtqueue, and
	 * host all support.
	 */
	features = host_features & child_features;
	features = virtqueue_filter_features(features);
	sc->vtpci_features = features;

	vtpci_describe_features(sc, "negotiated", features);
	vtpci_write_config_4(sc, VIRTIO_PCI_GUEST_FEATURES, features);

	return (features);
}

static int
vtpci_with_feature(device_t dev, uint64_t feature)
{
	struct vtpci_softc *sc;

	sc = device_get_softc(dev);

	return ((sc->vtpci_features & feature) != 0);
}

static int
vtpci_alloc_virtqueues(device_t dev, int flags, int nvqs,
    struct vq_alloc_info *vq_info)
{
	struct vtpci_softc *sc;
	struct vtpci_virtqueue *vqx;
	struct vq_alloc_info *info;
	int queue, error;
	uint16_t vq_size;

	sc = device_get_softc(dev);

	if (sc->vtpci_nvqs != 0 || nvqs <= 0 ||
	    nvqs > VIRTIO_MAX_VIRTQUEUES)
		return (EINVAL);

	error = vtpci_alloc_interrupts(sc, flags, nvqs, vq_info);
	if (error) {
		device_printf(dev, "cannot allocate interrupts\n");
		return (error);
	}

	if (sc->vtpci_flags & VIRTIO_PCI_FLAG_MSIX) {
		error = vtpci_register_msix_vector(sc,
		    VIRTIO_MSI_CONFIG_VECTOR, 0);
		if (error)
			return (error);
	}

	for (queue = 0; queue < nvqs; queue++) {
		vqx = &sc->vtpci_vqx[queue];
		info = &vq_info[queue];

		vtpci_write_config_2(sc, VIRTIO_PCI_QUEUE_SEL, queue);

		vq_size = vtpci_read_config_2(sc, VIRTIO_PCI_QUEUE_NUM);
		error = virtqueue_alloc(dev, queue, vq_size,
		    VIRTIO_PCI_VRING_ALIGN, 0xFFFFFFFFUL, info, &vqx->vq);
		if (error)
			return (error);

		if (sc->vtpci_flags & VIRTIO_PCI_FLAG_MSIX) {
			error = vtpci_register_msix_vector(sc,
			    VIRTIO_MSI_QUEUE_VECTOR, vqx->ires_idx);
			if (error)
				return (error);
		}

		vtpci_write_config_4(sc, VIRTIO_PCI_QUEUE_PFN,
		    virtqueue_paddr(vqx->vq) >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);

		*info->vqai_vq = vqx->vq;
		sc->vtpci_nvqs++;
	}

	return (0);
}

static int
vtpci_setup_intr(device_t dev, enum intr_type type)
{
	struct vtpci_softc *sc;
	struct vtpci_intr_resource *ires;
	struct vtpci_virtqueue *vqx;
	int i, flags, error;

	sc = device_get_softc(dev);
	flags = type | INTR_MPSAFE;
	ires = &sc->vtpci_intr_res[0];

	if ((sc->vtpci_flags & VIRTIO_PCI_FLAG_MSIX) == 0) {
		error = bus_setup_intr(dev, ires->irq, flags,
		    vtpci_legacy_intr, NULL, sc, &ires->intrhand);

		return (error);
	}

	error = bus_setup_intr(dev, ires->irq, flags, vtpci_config_intr,
	    NULL, sc, &ires->intrhand);
	if (error)
		return (error);

	if (sc->vtpci_flags & VIRTIO_PCI_FLAG_SHARED_MSIX) {
		ires = &sc->vtpci_intr_res[1];
		error = bus_setup_intr(dev, ires->irq, flags,
		    vtpci_vq_shared_intr, NULL, sc, &ires->intrhand);

		return (error);
	}

	/* Setup an interrupt handler for each virtqueue. */
	for (i = 0; i < sc->vtpci_nvqs; i++) {
		vqx = &sc->vtpci_vqx[i];
		if (vqx->ires_idx < 1)
			continue;

		ires = &sc->vtpci_intr_res[vqx->ires_idx];
		error = bus_setup_intr(dev, ires->irq, flags,
		    vtpci_vq_intr, NULL, vqx->vq, &ires->intrhand);
		if (error)
			return (error);
	}

	return (0);
}

static void
vtpci_stop(device_t dev)
{

	vtpci_reset(device_get_softc(dev));
}

static int
vtpci_reinit(device_t dev, uint64_t features)
{
	struct vtpci_softc *sc;
	struct vtpci_virtqueue *vqx;
	struct virtqueue *vq;
	int queue, error;
	uint16_t vq_size;

	sc = device_get_softc(dev);

	/*
	 * Redrive the device initialization. This is a bit of an abuse
	 * of the specification, but both VirtualBox and QEMU/KVM seem
	 * to play nice. We do not allow the host device to change from
	 * what was originally negotiated beyond what the guest driver
	 * changed (MSIX state should not change, number of virtqueues
	 * and their size remain the same, etc).
	 */

	if (vtpci_get_status(dev) != VIRTIO_CONFIG_STATUS_RESET)
		vtpci_stop(dev);

	/*
	 * Quickly drive the status through ACK and DRIVER. The device
	 * does not become usable again until vtpci_reinit_complete().
	 */
	vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_ACK);
	vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_DRIVER);

	vtpci_negotiate_features(dev, features);

	if (sc->vtpci_flags & VIRTIO_PCI_FLAG_MSIX) {
		error = vtpci_register_msix_vector(sc,
		    VIRTIO_MSI_CONFIG_VECTOR, 0);
		if (error)
			return (error);
	}

	for (queue = 0; queue < sc->vtpci_nvqs; queue++) {
		vqx = &sc->vtpci_vqx[queue];
		vq = vqx->vq;

		KASSERT(vq != NULL, ("vq %d not allocated", queue));
		vtpci_write_config_2(sc, VIRTIO_PCI_QUEUE_SEL, queue);

		vq_size = vtpci_read_config_2(sc, VIRTIO_PCI_QUEUE_NUM);
		error = virtqueue_reinit(vq, vq_size);
		if (error)
			return (error);

		if (sc->vtpci_flags & VIRTIO_PCI_FLAG_MSIX) {
			error = vtpci_register_msix_vector(sc,
			    VIRTIO_MSI_QUEUE_VECTOR, vqx->ires_idx);
			if (error)
				return (error);
		}

		vtpci_write_config_4(sc, VIRTIO_PCI_QUEUE_PFN,
		    virtqueue_paddr(vqx->vq) >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);
	}

	return (0);
}

static void
vtpci_reinit_complete(device_t dev)
{

	vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_DRIVER_OK);
}

static void
vtpci_notify_virtqueue(device_t dev, uint16_t queue)
{
	struct vtpci_softc *sc;

	sc = device_get_softc(dev);

	vtpci_write_config_2(sc, VIRTIO_PCI_QUEUE_NOTIFY, queue);
}

static uint8_t
vtpci_get_status(device_t dev)
{
	struct vtpci_softc *sc;

	sc = device_get_softc(dev);

	return (vtpci_read_config_1(sc, VIRTIO_PCI_STATUS));
}

static void
vtpci_set_status(device_t dev, uint8_t status)
{
	struct vtpci_softc *sc;

	sc = device_get_softc(dev);

	if (status != VIRTIO_CONFIG_STATUS_RESET)
		status |= vtpci_get_status(dev);

	vtpci_write_config_1(sc, VIRTIO_PCI_STATUS, status);
}

static void
vtpci_read_dev_config(device_t dev, bus_size_t offset,
    void *dst, int length)
{
	struct vtpci_softc *sc;
	bus_size_t off;
	uint8_t *d;
	int size;

	sc = device_get_softc(dev);
	off = VIRTIO_PCI_CONFIG(sc) + offset;

	for (d = dst; length > 0; d += size, off += size, length -= size) {
		if (length >= 4) {
			size = 4;
			*(uint32_t *)d = vtpci_read_config_4(sc, off);
		} else if (length >= 2) {
			size = 2;
			*(uint16_t *)d = vtpci_read_config_2(sc, off);
		} else {
			size = 1;
			*d = vtpci_read_config_1(sc, off);
		}
	}
}

static void
vtpci_write_dev_config(device_t dev, bus_size_t offset,
    void *src, int length)
{
	struct vtpci_softc *sc;
	bus_size_t off;
	uint8_t *s;
	int size;

	sc = device_get_softc(dev);
	off = VIRTIO_PCI_CONFIG(sc) + offset;

	for (s = src; length > 0; s += size, off += size, length -= size) {
		if (length >= 4) {
			size = 4;
			vtpci_write_config_4(sc, off, *(uint32_t *)s);
		} else if (length >= 2) {
			size = 2;
			vtpci_write_config_2(sc, off, *(uint16_t *)s);
		} else {
			size = 1;
			vtpci_write_config_1(sc, off, *s);
		}
	}
}

static void
vtpci_describe_features(struct vtpci_softc *sc, const char *msg,
    uint64_t features)
{
	device_t dev, child;

	dev = sc->vtpci_dev;
	child = sc->vtpci_child_dev;

	if (device_is_attached(child) && bootverbose == 0)
		return;

	virtio_describe(dev, msg, features, sc->vtpci_child_feat_desc);
}

static void
vtpci_probe_and_attach_child(struct vtpci_softc *sc)
{
	device_t dev, child;

	dev = sc->vtpci_dev;
	child = sc->vtpci_child_dev;

	if (child == NULL)
		return;

	if (device_get_state(child) != DS_NOTPRESENT)
		return;

	if (device_probe(child) != 0)
		return;

	vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_DRIVER);
	if (device_attach(child) != 0) {
		vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_FAILED);
		vtpci_reset(sc);
		vtpci_release_child_resources(sc);

		/* Reset status for future attempt. */
		vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_ACK);
	} else
		vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_DRIVER_OK);
}

static int
vtpci_alloc_interrupts(struct vtpci_softc *sc, int flags, int nvqs,
    struct vq_alloc_info *vq_info)
{
	int i, nvectors, error;

	/*
	 * Only allocate a vector for virtqueues that are actually
	 * expecting an interrupt.
	 */
	for (nvectors = 0, i = 0; i < nvqs; i++)
		if (vq_info[i].vqai_intr != NULL)
			nvectors++;

	if (vtpci_disable_msix != 0 ||
	    sc->vtpci_flags & VIRTIO_PCI_FLAG_NO_MSIX ||
	    flags & VIRTIO_ALLOC_VQS_DISABLE_MSIX ||
	    vtpci_alloc_msix(sc, nvectors) != 0) {
		/*
		 * Use MSI interrupts if available. Otherwise, we fallback
		 * to legacy interrupts.
		 */
		if ((sc->vtpci_flags & VIRTIO_PCI_FLAG_NO_MSI) == 0 &&
		    vtpci_alloc_msi(sc) == 0)
			sc->vtpci_flags |= VIRTIO_PCI_FLAG_MSI;

		sc->vtpci_nintr_res = 1;
	}

	error = vtpci_alloc_intr_resources(sc, nvqs, vq_info);

	return (error);
}

static int
vtpci_alloc_intr_resources(struct vtpci_softc *sc, int nvqs,
    struct vq_alloc_info *vq_info)
{
	device_t dev;
	struct resource *irq;
	struct vtpci_virtqueue *vqx;
	int i, rid, flags, res_idx;

	dev = sc->vtpci_dev;
	flags = RF_ACTIVE;

	if ((sc->vtpci_flags &
	    (VIRTIO_PCI_FLAG_MSI | VIRTIO_PCI_FLAG_MSIX)) == 0) {
		rid = 0;
		flags |= RF_SHAREABLE;
	} else
		rid = 1;

	for (i = 0; i < sc->vtpci_nintr_res; i++) {
		irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, flags);
		if (irq == NULL)
			return (ENXIO);

		sc->vtpci_intr_res[i].irq = irq;
		sc->vtpci_intr_res[i].rid = rid++;
	}

	/*
	 * Map the virtqueue into the correct index in vq_intr_res[]. Note the
	 * first index is reserved for configuration changes notifications.
	 */
	for (i = 0, res_idx = 1; i < nvqs; i++) {
		vqx = &sc->vtpci_vqx[i];

		if (sc->vtpci_flags & VIRTIO_PCI_FLAG_MSIX) {
			if (vq_info[i].vqai_intr == NULL)
				vqx->ires_idx = -1;
			else if (sc->vtpci_flags & VIRTIO_PCI_FLAG_SHARED_MSIX)
				vqx->ires_idx = res_idx;
			else
				vqx->ires_idx = res_idx++;
		} else
			vqx->ires_idx = -1;
	}

	return (0);
}

static int
vtpci_alloc_msi(struct vtpci_softc *sc)
{
	device_t dev;
	int nmsi, cnt;

	dev = sc->vtpci_dev;
	nmsi = pci_msi_count(dev);

	if (nmsi < 1)
		return (1);

	cnt = 1;
	if (pci_alloc_msi(dev, &cnt) == 0 && cnt == 1)
		return (0);

	return (1);
}

static int
vtpci_alloc_msix(struct vtpci_softc *sc, int nvectors)
{
	device_t dev;
	int nmsix, cnt, required;

	dev = sc->vtpci_dev;

	nmsix = pci_msix_count(dev);
	if (nmsix < 1)
		return (1);

	/* An additional vector is needed for the config changes. */
	required = nvectors + 1;
	if (nmsix >= required) {
		cnt = required;
		if (pci_alloc_msix(dev, &cnt) == 0 && cnt >= required)
			goto out;

		pci_release_msi(dev);
	}

	/* Attempt shared MSIX configuration. */
	required = 2;
	if (nmsix >= required) {
		cnt = required;
		if (pci_alloc_msix(dev, &cnt) == 0 && cnt >= required) {
			sc->vtpci_flags |= VIRTIO_PCI_FLAG_SHARED_MSIX;
			goto out;
		}

		pci_release_msi(dev);
	}

	return (1);

out:
	sc->vtpci_nintr_res = required;
	sc->vtpci_flags |= VIRTIO_PCI_FLAG_MSIX;

	if (bootverbose) {
		if (sc->vtpci_flags & VIRTIO_PCI_FLAG_SHARED_MSIX)
			device_printf(dev, "using shared virtqueue MSIX\n");
		else
			device_printf(dev, "using per virtqueue MSIX\n");
	}

	return (0);
}

static int
vtpci_register_msix_vector(struct vtpci_softc *sc, int offset, int res_idx)
{
	device_t dev;
	uint16_t vector;

	dev = sc->vtpci_dev;

	if (offset != VIRTIO_MSI_CONFIG_VECTOR &&
	    offset != VIRTIO_MSI_QUEUE_VECTOR)
		return (EINVAL);

	if (res_idx != -1) {
		/* Map from rid to host vector. */
		vector = sc->vtpci_intr_res[res_idx].rid - 1;
	} else
		vector = VIRTIO_MSI_NO_VECTOR;

	/* The first resource is special; make sure it is used correctly. */
	if (res_idx == 0) {
		KASSERT(vector == 0, ("unexpected config vector"));
		KASSERT(offset == VIRTIO_MSI_CONFIG_VECTOR,
		    ("unexpected config offset"));
	}

	vtpci_write_config_2(sc, offset, vector);

	if (vtpci_read_config_2(sc, offset) != vector) {
		device_printf(dev, "insufficient host resources for "
		    "MSIX interrupts\n");
		return (ENODEV);
	}

	return (0);
}

static void
vtpci_free_interrupts(struct vtpci_softc *sc)
{
	device_t dev;
	struct vtpci_intr_resource *ires;
	int i;

	dev = sc->vtpci_dev;
	sc->vtpci_nintr_res = 0;

	if (sc->vtpci_flags & (VIRTIO_PCI_FLAG_MSI | VIRTIO_PCI_FLAG_MSIX)) {
		pci_release_msi(dev);
		sc->vtpci_flags &= ~(VIRTIO_PCI_FLAG_MSI |
		    VIRTIO_PCI_FLAG_MSIX | VIRTIO_PCI_FLAG_SHARED_MSIX);
	}

	for (i = 0; i < 1 + VIRTIO_MAX_VIRTQUEUES; i++) {
		ires = &sc->vtpci_intr_res[i];

		if (ires->intrhand != NULL) {
			bus_teardown_intr(dev, ires->irq, ires->intrhand);
			ires->intrhand = NULL;
		}

		if (ires->irq != NULL) {
			bus_release_resource(dev, SYS_RES_IRQ, ires->rid,
			    ires->irq);
			ires->irq = NULL;
		}

		ires->rid = -1;
	}
}

static void
vtpci_free_virtqueues(struct vtpci_softc *sc)
{
	struct vtpci_virtqueue *vqx;
	int i;

	sc->vtpci_nvqs = 0;

	for (i = 0; i < VIRTIO_MAX_VIRTQUEUES; i++) {
		vqx = &sc->vtpci_vqx[i];

		if (vqx->vq != NULL) {
			virtqueue_free(vqx->vq);
			vqx->vq = NULL;
		}
	}
}

static void
vtpci_release_child_resources(struct vtpci_softc *sc)
{

	vtpci_free_interrupts(sc);
	vtpci_free_virtqueues(sc);
}

static void
vtpci_reset(struct vtpci_softc *sc)
{

	/*
	 * Setting the status to RESET sets the host device to
	 * the original, uninitialized state.
	 */
	vtpci_set_status(sc->vtpci_dev, VIRTIO_CONFIG_STATUS_RESET);
}

static int
vtpci_legacy_intr(void *xsc)
{
	struct vtpci_softc *sc;
	struct vtpci_virtqueue *vqx;
	int i;
	uint8_t isr;

	sc = xsc;
	vqx = &sc->vtpci_vqx[0];

	/* Reading the ISR also clears it. */
	isr = vtpci_read_config_1(sc, VIRTIO_PCI_ISR);

	if (isr & VIRTIO_PCI_ISR_CONFIG)
		vtpci_config_intr(sc);

	if (isr & VIRTIO_PCI_ISR_INTR)
		for (i = 0; i < sc->vtpci_nvqs; i++, vqx++)
			virtqueue_intr(vqx->vq);

	return (isr ? FILTER_HANDLED : FILTER_STRAY);
}

static int
vtpci_vq_shared_intr(void *xsc)
{
	struct vtpci_softc *sc;
	struct vtpci_virtqueue *vqx;
	int i, rc;

	rc = 0;
	sc = xsc;
	vqx = &sc->vtpci_vqx[0];

	for (i = 0; i < sc->vtpci_nvqs; i++, vqx++)
		rc |= virtqueue_intr(vqx->vq);

	return (rc ? FILTER_HANDLED : FILTER_STRAY);
}

static int
vtpci_vq_intr(void *xvq)
{
	struct virtqueue *vq;
	int rc;

	vq = xvq;
	rc = virtqueue_intr(vq);

	return (rc ? FILTER_HANDLED : FILTER_STRAY);
}

static int
vtpci_config_intr(void *xsc)
{
	struct vtpci_softc *sc;
	device_t child;
	int rc;

	rc = 0;
	sc = xsc;
	child = sc->vtpci_child_dev;

	if (child != NULL)
		rc = VIRTIO_CONFIG_CHANGE(child);

	return (rc ? FILTER_HANDLED : FILTER_STRAY);
}
