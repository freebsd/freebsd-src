/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014, Bryan Venteicher <bryanv@FreeBSD.org>
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

#ifndef _VIRTIO_H_
#define _VIRTIO_H_

#include <dev/virtio/virtio_endian.h>
#include <dev/virtio/virtio_ids.h>
#include <dev/virtio/virtio_config.h>

#ifdef _KERNEL

struct sbuf;
struct vq_alloc_info;

/*
 * Each virtqueue indirect descriptor list must be physically contiguous.
 * To allow us to malloc(9) each list individually, limit the number
 * supported to what will fit in one page. With 4KB pages, this is a limit
 * of 256 descriptors. If there is ever a need for more, we can switch to
 * contigmalloc(9) for the larger allocations, similar to what
 * bus_dmamem_alloc(9) does.
 *
 * Note the sizeof(struct vring_desc) is 16 bytes.
 */
#define VIRTIO_MAX_INDIRECT ((int) (PAGE_SIZE / 16))

/*
 * VirtIO instance variables indices.
 */
#define VIRTIO_IVAR_DEVTYPE		1
#define VIRTIO_IVAR_FEATURE_DESC	2
#define VIRTIO_IVAR_VENDOR		3
#define VIRTIO_IVAR_DEVICE		4
#define VIRTIO_IVAR_SUBVENDOR		5
#define VIRTIO_IVAR_SUBDEVICE		6
#define VIRTIO_IVAR_MODERN		7

struct virtio_feature_desc {
	uint64_t	 vfd_val;
	const char	*vfd_str;
};

#define VIRTIO_DRIVER_MODULE(name, driver, evh, arg)			\
	DRIVER_MODULE(name, virtio_mmio, driver, evh, arg);		\
	DRIVER_MODULE(name, virtio_pci, driver, evh, arg)

struct virtio_pnp_match {
	uint32_t	 device_type;
	const char	*description;
};
#define VIRTIO_SIMPLE_PNPINFO(driver, devtype, desc)			\
	static const struct virtio_pnp_match driver ## _match = {	\
		.device_type = devtype,					\
		.description = desc,					\
	};								\
	MODULE_PNP_INFO("U32:device_type;D:#", virtio_mmio, driver,	\
	    &driver ## _match, 1);					\
	MODULE_PNP_INFO("U32:device_type;D:#", virtio_pci, driver,	\
	    &driver ## _match, 1)
#define VIRTIO_SIMPLE_PROBE(dev, driver)				\
	(virtio_simple_probe(dev, &driver ## _match))

const char *virtio_device_name(uint16_t devid);
void	 virtio_describe(device_t dev, const char *msg,
	     uint64_t features, struct virtio_feature_desc *desc);
int	 virtio_describe_sbuf(struct sbuf *sb, uint64_t features,
	     struct virtio_feature_desc *desc);
uint64_t virtio_filter_transport_features(uint64_t features);
bool	 virtio_bus_is_modern(device_t dev);
void	 virtio_read_device_config_array(device_t dev, bus_size_t offset,
	     void *dst, int size, int count);

/*
 * VirtIO Bus Methods.
 */
void	 virtio_read_ivar(device_t dev, int ivar, uintptr_t *val);
void	 virtio_write_ivar(device_t dev, int ivar, uintptr_t val);
uint64_t virtio_negotiate_features(device_t dev, uint64_t child_features);
int	 virtio_finalize_features(device_t dev);
int	 virtio_alloc_virtqueues(device_t dev, int nvqs,
	     struct vq_alloc_info *info);
int	 virtio_setup_intr(device_t dev, enum intr_type type);
bool	 virtio_with_feature(device_t dev, uint64_t feature);
void	 virtio_stop(device_t dev);
int	 virtio_config_generation(device_t dev);
int	 virtio_reinit(device_t dev, uint64_t features);
void	 virtio_reinit_complete(device_t dev);
int	 virtio_child_pnpinfo(device_t busdev, device_t child, struct sbuf *sb);

/*
 * Read/write a variable amount from the device specific (ie, network)
 * configuration region. This region is encoded in the same endian as
 * the guest.
 */
void	 virtio_read_device_config(device_t dev, bus_size_t offset,
	     void *dst, int length);
void	 virtio_write_device_config(device_t dev, bus_size_t offset,
	     const void *src, int length);

/* Inlined device specific read/write functions for common lengths. */
#define VIRTIO_RDWR_DEVICE_CONFIG(size, type)				\
static inline type							\
__CONCAT(virtio_read_dev_config_,size)(device_t dev,			\
    bus_size_t offset)							\
{									\
	type val;							\
	virtio_read_device_config(dev, offset, &val, sizeof(type));	\
	return (val);							\
}									\
									\
static inline void							\
__CONCAT(virtio_write_dev_config_,size)(device_t dev,			\
    bus_size_t offset, type val)					\
{									\
	virtio_write_device_config(dev, offset, &val, sizeof(type));	\
}

VIRTIO_RDWR_DEVICE_CONFIG(1, uint8_t);
VIRTIO_RDWR_DEVICE_CONFIG(2, uint16_t);
VIRTIO_RDWR_DEVICE_CONFIG(4, uint32_t);

#undef VIRTIO_RDWR_DEVICE_CONFIG

#define VIRTIO_READ_IVAR(name, ivar)					\
static inline int							\
__CONCAT(virtio_get_,name)(device_t dev)				\
{									\
	uintptr_t val;							\
	virtio_read_ivar(dev, ivar, &val);				\
	return ((int) val);						\
}

VIRTIO_READ_IVAR(device_type,	VIRTIO_IVAR_DEVTYPE);
VIRTIO_READ_IVAR(vendor,	VIRTIO_IVAR_VENDOR);
VIRTIO_READ_IVAR(device,	VIRTIO_IVAR_DEVICE);
VIRTIO_READ_IVAR(subvendor,	VIRTIO_IVAR_SUBVENDOR);
VIRTIO_READ_IVAR(subdevice,	VIRTIO_IVAR_SUBDEVICE);
VIRTIO_READ_IVAR(modern,	VIRTIO_IVAR_MODERN);

#undef VIRTIO_READ_IVAR

#define VIRTIO_WRITE_IVAR(name, ivar)					\
static inline void							\
__CONCAT(virtio_set_,name)(device_t dev, void *val)			\
{									\
	virtio_write_ivar(dev, ivar, (uintptr_t) val);			\
}

VIRTIO_WRITE_IVAR(feature_desc,	VIRTIO_IVAR_FEATURE_DESC);

#undef VIRTIO_WRITE_IVAR

static inline int
virtio_simple_probe(device_t dev, const struct virtio_pnp_match *match)
{

	if (virtio_get_device_type(dev) != match->device_type)
		return (ENXIO);
	device_set_desc(dev, match->description);
	return (BUS_PROBE_DEFAULT);
}

#endif /* _KERNEL */

#endif /* _VIRTIO_H_ */
