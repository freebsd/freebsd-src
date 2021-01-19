/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2017, Bryan Venteicher <bryanv@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#ifndef _VIRTIO_PCI_H
#define _VIRTIO_PCI_H

struct vtpci_interrupt {
	struct resource		*vti_irq;
	int			 vti_rid;
	void			*vti_handler;
};

struct vtpci_virtqueue {
	struct virtqueue	*vtv_vq;
	int			 vtv_no_intr;
	int			 vtv_notify_offset;
};

struct vtpci_common {
	device_t			 vtpci_dev;
	uint64_t			 vtpci_host_features;
	uint64_t			 vtpci_features;
	struct vtpci_virtqueue		*vtpci_vqs;
	int				 vtpci_nvqs;

	uint32_t			 vtpci_flags;
#define VTPCI_FLAG_NO_MSI		0x0001
#define VTPCI_FLAG_NO_MSIX		0x0002
#define VTPCI_FLAG_MODERN		0x0004
#define VTPCI_FLAG_INTX			0x1000
#define VTPCI_FLAG_MSI			0x2000
#define VTPCI_FLAG_MSIX			0x4000
#define VTPCI_FLAG_SHARED_MSIX		0x8000
#define VTPCI_FLAG_ITYPE_MASK		0xF000

	/* The VirtIO PCI "bus" will only ever have one child. */
	device_t			 vtpci_child_dev;
	struct virtio_feature_desc	*vtpci_child_feat_desc;

	/*
	 * Ideally, each virtqueue that the driver provides a callback for will
	 * receive its own MSIX vector. If there are not sufficient vectors
	 * available, then attempt to have all the VQs share one vector. For
	 * MSIX, the configuration changed notifications must be on their own
	 * vector.
	 *
	 * If MSIX is not available, attempt to have the whole device share
	 * one MSI vector, and then, finally, one intx interrupt.
	 */
	struct vtpci_interrupt		 vtpci_device_interrupt;
	struct vtpci_interrupt		*vtpci_msix_vq_interrupts;
	int				 vtpci_nmsix_resources;
};

extern int vtpci_disable_msix;

static inline device_t
vtpci_child_device(struct vtpci_common *cn)
{
	return (cn->vtpci_child_dev);
}

static inline bool
vtpci_is_msix_available(struct vtpci_common *cn)
{
	return ((cn->vtpci_flags & VTPCI_FLAG_NO_MSIX) == 0);
}

static inline bool
vtpci_is_msix_enabled(struct vtpci_common *cn)
{
	return ((cn->vtpci_flags & VTPCI_FLAG_MSIX) != 0);
}

static inline bool
vtpci_is_modern(struct vtpci_common *cn)
{
	return ((cn->vtpci_flags & VTPCI_FLAG_MODERN) != 0);
}

static inline int
vtpci_virtqueue_count(struct vtpci_common *cn)
{
	return (cn->vtpci_nvqs);
}

void	vtpci_init(struct vtpci_common *cn, device_t dev, bool modern);
int	vtpci_add_child(struct vtpci_common *cn);
int	vtpci_delete_child(struct vtpci_common *cn);
void	vtpci_child_detached(struct vtpci_common *cn);
int	vtpci_reinit(struct vtpci_common *cn);

uint64_t vtpci_negotiate_features(struct vtpci_common *cn,
	     uint64_t child_features, uint64_t host_features);
int	 vtpci_with_feature(struct vtpci_common *cn, uint64_t feature);

int	vtpci_read_ivar(struct vtpci_common *cn, int index, uintptr_t *result);
int	vtpci_write_ivar(struct vtpci_common *cn, int index, uintptr_t value);

int	vtpci_alloc_virtqueues(struct vtpci_common *cn, int flags, int nvqs,
	    struct vq_alloc_info *vq_info);
int	vtpci_setup_interrupts(struct vtpci_common *cn, enum intr_type type);
void	vtpci_release_child_resources(struct vtpci_common *cn);

#endif /* _VIRTIO_PCI_H */
