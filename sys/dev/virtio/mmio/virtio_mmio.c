/*-
 * Copyright (c) 2014 Ruslan Bukin <br@bsdpad.com>
 * Copyright (c) 2014 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by Andrew Turner
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * VirtIO MMIO interface.
 * This driver is heavily based on VirtIO PCI interface driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/endian.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/virtio/virtio.h>
#include <dev/virtio/virtqueue.h>
#include <dev/virtio/mmio/virtio_mmio.h>

#include "virtio_mmio_if.h"
#include "virtio_bus_if.h"
#include "virtio_if.h"

struct vtmmio_virtqueue {
	struct virtqueue	*vtv_vq;
	int			 vtv_no_intr;
};

static int	vtmmio_detach(device_t);
static int	vtmmio_suspend(device_t);
static int	vtmmio_resume(device_t);
static int	vtmmio_shutdown(device_t);
static void	vtmmio_driver_added(device_t, driver_t *);
static void	vtmmio_child_detached(device_t, device_t);
static int	vtmmio_read_ivar(device_t, device_t, int, uintptr_t *);
static int	vtmmio_write_ivar(device_t, device_t, int, uintptr_t);
static uint64_t	vtmmio_negotiate_features(device_t, uint64_t);
static int	vtmmio_finalize_features(device_t);
static bool	vtmmio_with_feature(device_t, uint64_t);
static void	vtmmio_set_virtqueue(struct vtmmio_softc *sc,
		    struct virtqueue *vq, uint32_t size);
static int	vtmmio_alloc_virtqueues(device_t, int,
		    struct vq_alloc_info *);
static int	vtmmio_setup_intr(device_t, enum intr_type);
static void	vtmmio_stop(device_t);
static void	vtmmio_poll(device_t);
static int	vtmmio_reinit(device_t, uint64_t);
static void	vtmmio_reinit_complete(device_t);
static void	vtmmio_notify_virtqueue(device_t, uint16_t, bus_size_t);
static int	vtmmio_config_generation(device_t);
static uint8_t	vtmmio_get_status(device_t);
static void	vtmmio_set_status(device_t, uint8_t);
static void	vtmmio_read_dev_config(device_t, bus_size_t, void *, int);
static uint64_t	vtmmio_read_dev_config_8(struct vtmmio_softc *, bus_size_t);
static void	vtmmio_write_dev_config(device_t, bus_size_t, const void *, int);
static void	vtmmio_describe_features(struct vtmmio_softc *, const char *,
		    uint64_t);
static void	vtmmio_probe_and_attach_child(struct vtmmio_softc *);
static int	vtmmio_reinit_virtqueue(struct vtmmio_softc *, int);
static void	vtmmio_free_interrupts(struct vtmmio_softc *);
static void	vtmmio_free_virtqueues(struct vtmmio_softc *);
static void	vtmmio_release_child_resources(struct vtmmio_softc *);
static void	vtmmio_reset(struct vtmmio_softc *);
static void	vtmmio_select_virtqueue(struct vtmmio_softc *, int);
static void	vtmmio_vq_intr(void *);

/*
 * I/O port read/write wrappers.
 */
#define vtmmio_write_config_1(sc, o, v)				\
do {								\
	if (sc->platform != NULL)				\
		VIRTIO_MMIO_PREWRITE(sc->platform, (o), (v));	\
	bus_write_1((sc)->res[0], (o), (v)); 			\
	if (sc->platform != NULL)				\
		VIRTIO_MMIO_NOTE(sc->platform, (o), (v));	\
} while (0)
#define vtmmio_write_config_2(sc, o, v)				\
do {								\
	if (sc->platform != NULL)				\
		VIRTIO_MMIO_PREWRITE(sc->platform, (o), (v));	\
	bus_write_2((sc)->res[0], (o), (v));			\
	if (sc->platform != NULL)				\
		VIRTIO_MMIO_NOTE(sc->platform, (o), (v));	\
} while (0)
#define vtmmio_write_config_4(sc, o, v)				\
do {								\
	if (sc->platform != NULL)				\
		VIRTIO_MMIO_PREWRITE(sc->platform, (o), (v));	\
	bus_write_4((sc)->res[0], (o), (v));			\
	if (sc->platform != NULL)				\
		VIRTIO_MMIO_NOTE(sc->platform, (o), (v));	\
} while (0)

#define vtmmio_read_config_1(sc, o) \
	bus_read_1((sc)->res[0], (o))
#define vtmmio_read_config_2(sc, o) \
	bus_read_2((sc)->res[0], (o))
#define vtmmio_read_config_4(sc, o) \
	bus_read_4((sc)->res[0], (o))

static device_method_t vtmmio_methods[] = {
	/* Device interface. */
	DEVMETHOD(device_attach,		  vtmmio_attach),
	DEVMETHOD(device_detach,		  vtmmio_detach),
	DEVMETHOD(device_suspend,		  vtmmio_suspend),
	DEVMETHOD(device_resume,		  vtmmio_resume),
	DEVMETHOD(device_shutdown,		  vtmmio_shutdown),

	/* Bus interface. */
	DEVMETHOD(bus_driver_added,		  vtmmio_driver_added),
	DEVMETHOD(bus_child_detached,		  vtmmio_child_detached),
	DEVMETHOD(bus_child_pnpinfo,		  virtio_child_pnpinfo),
	DEVMETHOD(bus_read_ivar,		  vtmmio_read_ivar),
	DEVMETHOD(bus_write_ivar,		  vtmmio_write_ivar),

	/* VirtIO bus interface. */
	DEVMETHOD(virtio_bus_negotiate_features,  vtmmio_negotiate_features),
	DEVMETHOD(virtio_bus_finalize_features,	  vtmmio_finalize_features),
	DEVMETHOD(virtio_bus_with_feature,	  vtmmio_with_feature),
	DEVMETHOD(virtio_bus_alloc_virtqueues,	  vtmmio_alloc_virtqueues),
	DEVMETHOD(virtio_bus_setup_intr,	  vtmmio_setup_intr),
	DEVMETHOD(virtio_bus_stop,		  vtmmio_stop),
	DEVMETHOD(virtio_bus_poll,		  vtmmio_poll),
	DEVMETHOD(virtio_bus_reinit,		  vtmmio_reinit),
	DEVMETHOD(virtio_bus_reinit_complete,	  vtmmio_reinit_complete),
	DEVMETHOD(virtio_bus_notify_vq,		  vtmmio_notify_virtqueue),
	DEVMETHOD(virtio_bus_config_generation,	  vtmmio_config_generation),
	DEVMETHOD(virtio_bus_read_device_config,  vtmmio_read_dev_config),
	DEVMETHOD(virtio_bus_write_device_config, vtmmio_write_dev_config),

	DEVMETHOD_END
};

DEFINE_CLASS_0(virtio_mmio, vtmmio_driver, vtmmio_methods,
    sizeof(struct vtmmio_softc));

MODULE_VERSION(virtio_mmio, 1);

int
vtmmio_probe(device_t dev)
{
	struct vtmmio_softc *sc;
	int rid;
	uint32_t magic, version;

	sc = device_get_softc(dev);

	rid = 0;
	sc->res[0] = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->res[0] == NULL) {
		device_printf(dev, "Cannot allocate memory window.\n");
		return (ENXIO);
	}

	magic = vtmmio_read_config_4(sc, VIRTIO_MMIO_MAGIC_VALUE);
	if (magic != VIRTIO_MMIO_MAGIC_VIRT) {
		device_printf(dev, "Bad magic value %#x\n", magic);
		bus_release_resource(dev, SYS_RES_MEMORY, rid, sc->res[0]);
		return (ENXIO);
	}

	version = vtmmio_read_config_4(sc, VIRTIO_MMIO_VERSION);
	if (version < 1 || version > 2) {
		device_printf(dev, "Unsupported version: %#x\n", version);
		bus_release_resource(dev, SYS_RES_MEMORY, rid, sc->res[0]);
		return (ENXIO);
	}

	if (vtmmio_read_config_4(sc, VIRTIO_MMIO_DEVICE_ID) == 0) {
		bus_release_resource(dev, SYS_RES_MEMORY, rid, sc->res[0]);
		return (ENXIO);
	}

	bus_release_resource(dev, SYS_RES_MEMORY, rid, sc->res[0]);

	device_set_desc(dev, "VirtIO MMIO adapter");
	return (BUS_PROBE_DEFAULT);
}

static int
vtmmio_setup_intr(device_t dev, enum intr_type type)
{
	struct vtmmio_softc *sc;
	int rid;
	int err;

	sc = device_get_softc(dev);

	if (sc->platform != NULL) {
		err = VIRTIO_MMIO_SETUP_INTR(sc->platform, sc->dev,
					vtmmio_vq_intr, sc);
		if (err == 0) {
			/* Okay we have backend-specific interrupts */
			return (0);
		}
	}

	rid = 0;
	sc->res[1] = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
		RF_ACTIVE);
	if (!sc->res[1]) {
		device_printf(dev, "Can't allocate interrupt\n");
		return (ENXIO);
	}

	if (bus_setup_intr(dev, sc->res[1], type | INTR_MPSAFE,
		NULL, vtmmio_vq_intr, sc, &sc->ih)) {
		device_printf(dev, "Can't setup the interrupt\n");
		return (ENXIO);
	}

	return (0);
}

int
vtmmio_attach(device_t dev)
{
	struct vtmmio_softc *sc;
	device_t child;
	int rid;

	sc = device_get_softc(dev);
	sc->dev = dev;

	rid = 0;
	sc->res[0] = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
			RF_ACTIVE);
	if (sc->res[0] == NULL) {
		device_printf(dev, "Cannot allocate memory window.\n");
		return (ENXIO);
	}

	sc->vtmmio_version = vtmmio_read_config_4(sc, VIRTIO_MMIO_VERSION);

	vtmmio_reset(sc);

	/* Tell the host we've noticed this device. */
	vtmmio_set_status(dev, VIRTIO_CONFIG_STATUS_ACK);

	if ((child = device_add_child(dev, NULL, -1)) == NULL) {
		device_printf(dev, "Cannot create child device.\n");
		vtmmio_set_status(dev, VIRTIO_CONFIG_STATUS_FAILED);
		vtmmio_detach(dev);
		return (ENOMEM);
	}

	sc->vtmmio_child_dev = child;
	vtmmio_probe_and_attach_child(sc);

	return (0);
}

static int
vtmmio_detach(device_t dev)
{
	struct vtmmio_softc *sc;
	int error;

	sc = device_get_softc(dev);

	error = bus_generic_detach(dev);
	if (error)
		return (error);

	vtmmio_reset(sc);

	if (sc->res[0] != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, 0,
		    sc->res[0]);
		sc->res[0] = NULL;
	}

	return (0);
}

static int
vtmmio_suspend(device_t dev)
{

	return (bus_generic_suspend(dev));
}

static int
vtmmio_resume(device_t dev)
{

	return (bus_generic_resume(dev));
}

static int
vtmmio_shutdown(device_t dev)
{

	(void) bus_generic_shutdown(dev);

	/* Forcibly stop the host device. */
	vtmmio_stop(dev);

	return (0);
}

static void
vtmmio_driver_added(device_t dev, driver_t *driver)
{
	struct vtmmio_softc *sc;

	sc = device_get_softc(dev);

	vtmmio_probe_and_attach_child(sc);
}

static void
vtmmio_child_detached(device_t dev, device_t child)
{
	struct vtmmio_softc *sc;

	sc = device_get_softc(dev);

	vtmmio_reset(sc);
	vtmmio_release_child_resources(sc);
}

static int
vtmmio_read_ivar(device_t dev, device_t child, int index, uintptr_t *result)
{
	struct vtmmio_softc *sc;

	sc = device_get_softc(dev);

	if (sc->vtmmio_child_dev != child)
		return (ENOENT);

	switch (index) {
	case VIRTIO_IVAR_DEVTYPE:
	case VIRTIO_IVAR_SUBDEVICE:
		*result = vtmmio_read_config_4(sc, VIRTIO_MMIO_DEVICE_ID);
		break;
	case VIRTIO_IVAR_VENDOR:
		*result = vtmmio_read_config_4(sc, VIRTIO_MMIO_VENDOR_ID);
		break;
	case VIRTIO_IVAR_SUBVENDOR:
	case VIRTIO_IVAR_DEVICE:
		/*
		 * Dummy value for fields not present in this bus.  Used by
		 * bus-agnostic virtio_child_pnpinfo.
		 */
		*result = 0;
		break;
	case VIRTIO_IVAR_MODERN:
		/*
		 * There are several modern (aka MMIO v2) spec compliance
		 * issues with this driver, but keep the status quo.
		 */
		*result = sc->vtmmio_version > 1;
		break;
	default:
		return (ENOENT);
	}

	return (0);
}

static int
vtmmio_write_ivar(device_t dev, device_t child, int index, uintptr_t value)
{
	struct vtmmio_softc *sc;

	sc = device_get_softc(dev);

	if (sc->vtmmio_child_dev != child)
		return (ENOENT);

	switch (index) {
	case VIRTIO_IVAR_FEATURE_DESC:
		sc->vtmmio_child_feat_desc = (void *) value;
		break;
	default:
		return (ENOENT);
	}

	return (0);
}

static uint64_t
vtmmio_negotiate_features(device_t dev, uint64_t child_features)
{
	struct vtmmio_softc *sc;
	uint64_t host_features, features;

	sc = device_get_softc(dev);

	if (sc->vtmmio_version > 1) {
		child_features |= VIRTIO_F_VERSION_1;
	}

	vtmmio_write_config_4(sc, VIRTIO_MMIO_HOST_FEATURES_SEL, 1);
	host_features = vtmmio_read_config_4(sc, VIRTIO_MMIO_HOST_FEATURES);
	host_features <<= 32;

	vtmmio_write_config_4(sc, VIRTIO_MMIO_HOST_FEATURES_SEL, 0);
	host_features |= vtmmio_read_config_4(sc, VIRTIO_MMIO_HOST_FEATURES);

	vtmmio_describe_features(sc, "host", host_features);

	/*
	 * Limit negotiated features to what the driver, virtqueue, and
	 * host all support.
	 */
	features = host_features & child_features;
	features = virtio_filter_transport_features(features);
	sc->vtmmio_features = features;

	vtmmio_describe_features(sc, "negotiated", features);

	vtmmio_write_config_4(sc, VIRTIO_MMIO_GUEST_FEATURES_SEL, 1);
	vtmmio_write_config_4(sc, VIRTIO_MMIO_GUEST_FEATURES, features >> 32);

	vtmmio_write_config_4(sc, VIRTIO_MMIO_GUEST_FEATURES_SEL, 0);
	vtmmio_write_config_4(sc, VIRTIO_MMIO_GUEST_FEATURES, features);

	return (features);
}

static int
vtmmio_finalize_features(device_t dev)
{
	struct vtmmio_softc *sc;
	uint8_t status;

	sc = device_get_softc(dev);

	if (sc->vtmmio_version > 1) {
		/*
		 * Must re-read the status after setting it to verify the
		 * negotiated features were accepted by the device.
		 */
		vtmmio_set_status(dev, VIRTIO_CONFIG_S_FEATURES_OK);

		status = vtmmio_get_status(dev);
		if ((status & VIRTIO_CONFIG_S_FEATURES_OK) == 0) {
			device_printf(dev, "desired features were not accepted\n");
			return (ENOTSUP);
		}
	}

	return (0);
}

static bool
vtmmio_with_feature(device_t dev, uint64_t feature)
{
	struct vtmmio_softc *sc;

	sc = device_get_softc(dev);

	return ((sc->vtmmio_features & feature) != 0);
}

static void
vtmmio_set_virtqueue(struct vtmmio_softc *sc, struct virtqueue *vq,
    uint32_t size)
{
	vm_paddr_t paddr;

	vtmmio_write_config_4(sc, VIRTIO_MMIO_QUEUE_NUM, size);

	if (sc->vtmmio_version == 1) {
		vtmmio_write_config_4(sc, VIRTIO_MMIO_QUEUE_ALIGN,
		    VIRTIO_MMIO_VRING_ALIGN);
		paddr = virtqueue_paddr(vq);
		vtmmio_write_config_4(sc, VIRTIO_MMIO_QUEUE_PFN,
		    paddr >> PAGE_SHIFT);
	} else {
		paddr = virtqueue_desc_paddr(vq);
		vtmmio_write_config_4(sc, VIRTIO_MMIO_QUEUE_DESC_LOW,
		    paddr);
		vtmmio_write_config_4(sc, VIRTIO_MMIO_QUEUE_DESC_HIGH,
		    ((uint64_t)paddr) >> 32);

		paddr = virtqueue_avail_paddr(vq);
		vtmmio_write_config_4(sc, VIRTIO_MMIO_QUEUE_AVAIL_LOW,
		    paddr);
		vtmmio_write_config_4(sc, VIRTIO_MMIO_QUEUE_AVAIL_HIGH,
		    ((uint64_t)paddr) >> 32);

		paddr = virtqueue_used_paddr(vq);
		vtmmio_write_config_4(sc, VIRTIO_MMIO_QUEUE_USED_LOW,
		    paddr);
		vtmmio_write_config_4(sc, VIRTIO_MMIO_QUEUE_USED_HIGH,
		    ((uint64_t)paddr) >> 32);

		vtmmio_write_config_4(sc, VIRTIO_MMIO_QUEUE_READY, 1);
	}
}

static int
vtmmio_alloc_virtqueues(device_t dev, int nvqs,
    struct vq_alloc_info *vq_info)
{
	struct vtmmio_virtqueue *vqx;
	struct vq_alloc_info *info;
	struct vtmmio_softc *sc;
	struct virtqueue *vq;
	uint32_t size;
	int idx, error;

	sc = device_get_softc(dev);

	if (sc->vtmmio_nvqs != 0)
		return (EALREADY);
	if (nvqs <= 0)
		return (EINVAL);

	sc->vtmmio_vqs = malloc(nvqs * sizeof(struct vtmmio_virtqueue),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->vtmmio_vqs == NULL)
		return (ENOMEM);

	if (sc->vtmmio_version == 1) {
		vtmmio_write_config_4(sc, VIRTIO_MMIO_GUEST_PAGE_SIZE,
		    (1 << PAGE_SHIFT));
	}

	for (idx = 0; idx < nvqs; idx++) {
		vqx = &sc->vtmmio_vqs[idx];
		info = &vq_info[idx];

		vtmmio_select_virtqueue(sc, idx);
		size = vtmmio_read_config_4(sc, VIRTIO_MMIO_QUEUE_NUM_MAX);

		error = virtqueue_alloc(dev, idx, size,
		    VIRTIO_MMIO_QUEUE_NOTIFY, VIRTIO_MMIO_VRING_ALIGN,
		    ~(vm_paddr_t)0, info, &vq);
		if (error) {
			device_printf(dev,
			    "cannot allocate virtqueue %d: %d\n",
			    idx, error);
			break;
		}

		vtmmio_set_virtqueue(sc, vq, size);

		vqx->vtv_vq = *info->vqai_vq = vq;
		vqx->vtv_no_intr = info->vqai_intr == NULL;

		sc->vtmmio_nvqs++;
	}

	if (error)
		vtmmio_free_virtqueues(sc);

	return (error);
}

static void
vtmmio_stop(device_t dev)
{

	vtmmio_reset(device_get_softc(dev));
}

static void
vtmmio_poll(device_t dev)
{
	struct vtmmio_softc *sc;

	sc = device_get_softc(dev);

	if (sc->platform != NULL)
		VIRTIO_MMIO_POLL(sc->platform);
}

static int
vtmmio_reinit(device_t dev, uint64_t features)
{
	struct vtmmio_softc *sc;
	int idx, error;

	sc = device_get_softc(dev);

	if (vtmmio_get_status(dev) != VIRTIO_CONFIG_STATUS_RESET)
		vtmmio_stop(dev);

	/*
	 * Quickly drive the status through ACK and DRIVER. The device
	 * does not become usable again until vtmmio_reinit_complete().
	 */
	vtmmio_set_status(dev, VIRTIO_CONFIG_STATUS_ACK);
	vtmmio_set_status(dev, VIRTIO_CONFIG_STATUS_DRIVER);

	/*
	 * TODO: Check that features are not added as to what was
	 * originally negotiated.
	 */
	vtmmio_negotiate_features(dev, features);
	error = vtmmio_finalize_features(dev);
	if (error) {
		device_printf(dev, "cannot finalize features during reinit\n");
		return (error);
	}

	if (sc->vtmmio_version == 1) {
		vtmmio_write_config_4(sc, VIRTIO_MMIO_GUEST_PAGE_SIZE,
		    (1 << PAGE_SHIFT));
	}

	for (idx = 0; idx < sc->vtmmio_nvqs; idx++) {
		error = vtmmio_reinit_virtqueue(sc, idx);
		if (error)
			return (error);
	}

	return (0);
}

static void
vtmmio_reinit_complete(device_t dev)
{

	vtmmio_set_status(dev, VIRTIO_CONFIG_STATUS_DRIVER_OK);
}

static void
vtmmio_notify_virtqueue(device_t dev, uint16_t queue, bus_size_t offset)
{
	struct vtmmio_softc *sc;

	sc = device_get_softc(dev);
	MPASS(offset == VIRTIO_MMIO_QUEUE_NOTIFY);

	vtmmio_write_config_4(sc, offset, queue);
}

static int
vtmmio_config_generation(device_t dev)
{
	struct vtmmio_softc *sc;
	uint32_t gen;

	sc = device_get_softc(dev);

	if (sc->vtmmio_version > 1)
		gen = vtmmio_read_config_4(sc, VIRTIO_MMIO_CONFIG_GENERATION);
	else
		gen = 0;

	return (gen);
}

static uint8_t
vtmmio_get_status(device_t dev)
{
	struct vtmmio_softc *sc;

	sc = device_get_softc(dev);

	return (vtmmio_read_config_4(sc, VIRTIO_MMIO_STATUS));
}

static void
vtmmio_set_status(device_t dev, uint8_t status)
{
	struct vtmmio_softc *sc;

	sc = device_get_softc(dev);

	if (status != VIRTIO_CONFIG_STATUS_RESET)
		status |= vtmmio_get_status(dev);

	vtmmio_write_config_4(sc, VIRTIO_MMIO_STATUS, status);
}

static void
vtmmio_read_dev_config(device_t dev, bus_size_t offset,
    void *dst, int length)
{
	struct vtmmio_softc *sc;
	bus_size_t off;
	uint8_t *d;
	int size;

	sc = device_get_softc(dev);
	off = VIRTIO_MMIO_CONFIG + offset;

	/*
	 * The non-legacy MMIO specification adds the following restriction:
	 *
	 *   4.2.2.2: For the device-specific configuration space, the driver
	 *   MUST use 8 bit wide accesses for 8 bit wide fields, 16 bit wide
	 *   and aligned accesses for 16 bit wide fields and 32 bit wide and
	 *   aligned accesses for 32 and 64 bit wide fields.
	 *
	 * The endianness also varies between non-legacy and legacy:
	 *
	 *   2.4: Note: The device configuration space uses the little-endian
	 *   format for multi-byte fields.
	 *
	 *   2.4.3: Note that for legacy interfaces, device configuration space
	 *   is generally the guest’s native endian, rather than PCI’s
	 *   little-endian. The correct endian-ness is documented for each
	 *   device.
	 */
	if (sc->vtmmio_version > 1) {
		switch (length) {
		case 1:
			*(uint8_t *)dst = vtmmio_read_config_1(sc, off);
			break;
		case 2:
			*(uint16_t *)dst =
			    le16toh(vtmmio_read_config_2(sc, off));
			break;
		case 4:
			*(uint32_t *)dst =
			    le32toh(vtmmio_read_config_4(sc, off));
			break;
		case 8:
			*(uint64_t *)dst = vtmmio_read_dev_config_8(sc, off);
			break;
		default:
			panic("%s: invalid length %d\n", __func__, length);
		}

		return;
	}

	for (d = dst; length > 0; d += size, off += size, length -= size) {
#ifdef ALLOW_WORD_ALIGNED_ACCESS
		if (length >= 4) {
			size = 4;
			*(uint32_t *)d = vtmmio_read_config_4(sc, off);
		} else if (length >= 2) {
			size = 2;
			*(uint16_t *)d = vtmmio_read_config_2(sc, off);
		} else
#endif
		{
			size = 1;
			*d = vtmmio_read_config_1(sc, off);
		}
	}
}

static uint64_t
vtmmio_read_dev_config_8(struct vtmmio_softc *sc, bus_size_t off)
{
	device_t dev;
	int gen;
	uint32_t val0, val1;

	dev = sc->dev;

	do {
		gen = vtmmio_config_generation(dev);
		val0 = le32toh(vtmmio_read_config_4(sc, off));
		val1 = le32toh(vtmmio_read_config_4(sc, off + 4));
	} while (gen != vtmmio_config_generation(dev));

	return (((uint64_t) val1 << 32) | val0);
}

static void
vtmmio_write_dev_config(device_t dev, bus_size_t offset,
    const void *src, int length)
{
	struct vtmmio_softc *sc;
	bus_size_t off;
	const uint8_t *s;
	int size;

	sc = device_get_softc(dev);
	off = VIRTIO_MMIO_CONFIG + offset;

	/*
	 * The non-legacy MMIO specification adds size and alignment
	 * restrctions. It also changes the endianness from native-endian to
	 * little-endian. See vtmmio_read_dev_config.
	 */
	if (sc->vtmmio_version > 1) {
		switch (length) {
		case 1:
			vtmmio_write_config_1(sc, off, *(const uint8_t *)src);
			break;
		case 2:
			vtmmio_write_config_2(sc, off,
			    htole16(*(const uint16_t *)src));
			break;
		case 4:
			vtmmio_write_config_4(sc, off,
			    htole32(*(const uint32_t *)src));
			break;
		case 8:
			vtmmio_write_config_4(sc, off,
			    htole32(*(const uint64_t *)src));
			vtmmio_write_config_4(sc, off + 4,
			    htole32((*(const uint64_t *)src) >> 32));
			break;
		default:
			panic("%s: invalid length %d\n", __func__, length);
		}

		return;
	}

	for (s = src; length > 0; s += size, off += size, length -= size) {
#ifdef ALLOW_WORD_ALIGNED_ACCESS
		if (length >= 4) {
			size = 4;
			vtmmio_write_config_4(sc, off, *(uint32_t *)s);
		} else if (length >= 2) {
			size = 2;
			vtmmio_write_config_2(sc, off, *(uint16_t *)s);
		} else
#endif
		{
			size = 1;
			vtmmio_write_config_1(sc, off, *s);
		}
	}
}

static void
vtmmio_describe_features(struct vtmmio_softc *sc, const char *msg,
    uint64_t features)
{
	device_t dev, child;

	dev = sc->dev;
	child = sc->vtmmio_child_dev;

	if (device_is_attached(child) || bootverbose == 0)
		return;

	virtio_describe(dev, msg, features, sc->vtmmio_child_feat_desc);
}

static void
vtmmio_probe_and_attach_child(struct vtmmio_softc *sc)
{
	device_t dev, child;

	dev = sc->dev;
	child = sc->vtmmio_child_dev;

	if (child == NULL)
		return;

	if (device_get_state(child) != DS_NOTPRESENT) {
		return;
	}

	if (device_probe(child) != 0) {
		return;
	}

	vtmmio_set_status(dev, VIRTIO_CONFIG_STATUS_DRIVER);
	if (device_attach(child) != 0) {
		vtmmio_set_status(dev, VIRTIO_CONFIG_STATUS_FAILED);
		vtmmio_reset(sc);
		vtmmio_release_child_resources(sc);
		/* Reset status for future attempt. */
		vtmmio_set_status(dev, VIRTIO_CONFIG_STATUS_ACK);
	} else {
		vtmmio_set_status(dev, VIRTIO_CONFIG_STATUS_DRIVER_OK);
		VIRTIO_ATTACH_COMPLETED(child);
	}
}

static int
vtmmio_reinit_virtqueue(struct vtmmio_softc *sc, int idx)
{
	struct vtmmio_virtqueue *vqx;
	struct virtqueue *vq;
	int error;
	uint16_t size;

	vqx = &sc->vtmmio_vqs[idx];
	vq = vqx->vtv_vq;

	KASSERT(vq != NULL, ("%s: vq %d not allocated", __func__, idx));

	vtmmio_select_virtqueue(sc, idx);
	size = vtmmio_read_config_4(sc, VIRTIO_MMIO_QUEUE_NUM_MAX);

	error = virtqueue_reinit(vq, size);
	if (error)
		return (error);

	vtmmio_set_virtqueue(sc, vq, size);

	return (0);
}

static void
vtmmio_free_interrupts(struct vtmmio_softc *sc)
{

	if (sc->ih != NULL)
		bus_teardown_intr(sc->dev, sc->res[1], sc->ih);

	if (sc->res[1] != NULL)
		bus_release_resource(sc->dev, SYS_RES_IRQ, 0, sc->res[1]);
}

static void
vtmmio_free_virtqueues(struct vtmmio_softc *sc)
{
	struct vtmmio_virtqueue *vqx;
	int idx;

	for (idx = 0; idx < sc->vtmmio_nvqs; idx++) {
		vqx = &sc->vtmmio_vqs[idx];

		vtmmio_select_virtqueue(sc, idx);
		if (sc->vtmmio_version > 1) {
			vtmmio_write_config_4(sc, VIRTIO_MMIO_QUEUE_READY, 0);
			vtmmio_read_config_4(sc, VIRTIO_MMIO_QUEUE_READY);
		} else
			vtmmio_write_config_4(sc, VIRTIO_MMIO_QUEUE_PFN, 0);

		virtqueue_free(vqx->vtv_vq);
		vqx->vtv_vq = NULL;
	}

	free(sc->vtmmio_vqs, M_DEVBUF);
	sc->vtmmio_vqs = NULL;
	sc->vtmmio_nvqs = 0;
}

static void
vtmmio_release_child_resources(struct vtmmio_softc *sc)
{

	vtmmio_free_interrupts(sc);
	vtmmio_free_virtqueues(sc);
}

static void
vtmmio_reset(struct vtmmio_softc *sc)
{

	/*
	 * Setting the status to RESET sets the host device to
	 * the original, uninitialized state.
	 */
	vtmmio_set_status(sc->dev, VIRTIO_CONFIG_STATUS_RESET);
}

static void
vtmmio_select_virtqueue(struct vtmmio_softc *sc, int idx)
{

	vtmmio_write_config_4(sc, VIRTIO_MMIO_QUEUE_SEL, idx);
}

static void
vtmmio_vq_intr(void *arg)
{
	struct vtmmio_virtqueue *vqx;
	struct vtmmio_softc *sc;
	struct virtqueue *vq;
	uint32_t status;
	int idx;

	sc = arg;

	status = vtmmio_read_config_4(sc, VIRTIO_MMIO_INTERRUPT_STATUS);
	vtmmio_write_config_4(sc, VIRTIO_MMIO_INTERRUPT_ACK, status);

	/* The config changed */
	if (status & VIRTIO_MMIO_INT_CONFIG)
		if (sc->vtmmio_child_dev != NULL)
			VIRTIO_CONFIG_CHANGE(sc->vtmmio_child_dev);

	/* Notify all virtqueues. */
	if (status & VIRTIO_MMIO_INT_VRING) {
		for (idx = 0; idx < sc->vtmmio_nvqs; idx++) {
			vqx = &sc->vtmmio_vqs[idx];
			if (vqx->vtv_no_intr == 0) {
				vq = vqx->vtv_vq;
				virtqueue_intr(vq);
			}
		}
	}
}
