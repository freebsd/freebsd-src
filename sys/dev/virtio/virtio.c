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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sbuf.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/virtio/virtio.h>
#include <dev/virtio/virtio_config.h>
#include <dev/virtio/virtqueue.h>

#include "virtio_bus_if.h"

static int virtio_modevent(module_t, int, void *);
static const char *virtio_feature_name(uint64_t, struct virtio_feature_desc *);

static struct virtio_ident {
	uint16_t	devid;
	const char	*name;
} virtio_ident_table[] = {
	{ VIRTIO_ID_NETWORK,		"Network"			},
	{ VIRTIO_ID_BLOCK,		"Block"				},
	{ VIRTIO_ID_CONSOLE,		"Console"			},
	{ VIRTIO_ID_ENTROPY,		"Entropy"			},
	{ VIRTIO_ID_BALLOON,		"Balloon"			},
	{ VIRTIO_ID_IOMEMORY,		"IOMemory"			},
	{ VIRTIO_ID_RPMSG,		"Remote Processor Messaging"	},
	{ VIRTIO_ID_SCSI,		"SCSI"				},
	{ VIRTIO_ID_9P,			"9P Transport"			},
	{ VIRTIO_ID_RPROC_SERIAL,	"Remote Processor Serial"	},
	{ VIRTIO_ID_CAIF,		"CAIF"				},
	{ VIRTIO_ID_GPU,		"GPU"				},
	{ VIRTIO_ID_INPUT,		"Input"				},
	{ VIRTIO_ID_VSOCK,		"VSOCK Transport"		},
	{ VIRTIO_ID_CRYPTO,		"Crypto"			},
	{ VIRTIO_ID_IOMMU,		"IOMMU"				},
	{ VIRTIO_ID_SOUND,		"Sound"				},
	{ VIRTIO_ID_FS,			"Filesystem"			},
	{ VIRTIO_ID_PMEM,		"Persistent Memory"		},
	{ VIRTIO_ID_RPMB,		"RPMB"				},
	{ VIRTIO_ID_GPIO,		"GPIO"				},

	{ 0, NULL }
};

/* Device independent features. */
static struct virtio_feature_desc virtio_common_feature_desc[] = {
	{ VIRTIO_F_NOTIFY_ON_EMPTY,	"NotifyOnEmpty"		}, /* Legacy */
	{ VIRTIO_F_ANY_LAYOUT,		"AnyLayout"		}, /* Legacy */
	{ VIRTIO_RING_F_INDIRECT_DESC,	"RingIndirectDesc"	},
	{ VIRTIO_RING_F_EVENT_IDX,	"RingEventIdx"		},
	{ VIRTIO_F_BAD_FEATURE,		"BadFeature"		}, /* Legacy */
	{ VIRTIO_F_VERSION_1,		"Version1"		},
	{ VIRTIO_F_IOMMU_PLATFORM,	"IOMMUPlatform"		},

	{ 0, NULL }
};

const char *
virtio_device_name(uint16_t devid)
{
	struct virtio_ident *ident;

	for (ident = virtio_ident_table; ident->name != NULL; ident++) {
		if (ident->devid == devid)
			return (ident->name);
	}

	return (NULL);
}

static const char *
virtio_feature_name(uint64_t val, struct virtio_feature_desc *desc)
{
	int i, j;
	struct virtio_feature_desc *descs[2] = { desc,
	    virtio_common_feature_desc };

	for (i = 0; i < 2; i++) {
		if (descs[i] == NULL)
			continue;

		for (j = 0; descs[i][j].vfd_val != 0; j++) {
			if (val == descs[i][j].vfd_val)
				return (descs[i][j].vfd_str);
		}
	}

	return (NULL);
}

int
virtio_describe_sbuf(struct sbuf *sb, uint64_t features,
    struct virtio_feature_desc *desc)
{
	const char *name;
	uint64_t val;
	int n;

	sbuf_printf(sb, "%#jx", (uintmax_t) features);

	for (n = 0, val = 1ULL << 63; val != 0; val >>= 1) {
		/*
		 * BAD_FEATURE is used to detect broken Linux clients
		 * and therefore is not applicable to FreeBSD.
		 */
		if (((features & val) == 0) || val == VIRTIO_F_BAD_FEATURE)
			continue;

		if (n++ == 0)
			sbuf_cat(sb, " <");
		else
			sbuf_cat(sb, ",");

		name = virtio_feature_name(val, desc);
		if (name == NULL)
			sbuf_printf(sb, "%#jx", (uintmax_t) val);
		else
			sbuf_cat(sb, name);
	}

	if (n > 0)
		sbuf_cat(sb, ">");

	return (sbuf_finish(sb));
}

void
virtio_describe(device_t dev, const char *msg, uint64_t features,
    struct virtio_feature_desc *desc)
{
	struct sbuf sb;
	char *buf;
	int error;

	if ((buf = malloc(1024, M_TEMP, M_NOWAIT)) == NULL) {
		error = ENOMEM;
		goto out;
	}

	sbuf_new(&sb, buf, 1024, SBUF_FIXEDLEN);
	sbuf_printf(&sb, "%s features: ", msg);

	error = virtio_describe_sbuf(&sb, features, desc);
	if (error == 0)
		device_printf(dev, "%s\n", sbuf_data(&sb));

	sbuf_delete(&sb);
	free(buf, M_TEMP);

out:
	if (error != 0) {
		device_printf(dev, "%s features: %#jx\n", msg,
		    (uintmax_t) features);
	}
}

uint64_t
virtio_filter_transport_features(uint64_t features)
{
	uint64_t transport, mask;

	transport = (1ULL <<
	    (VIRTIO_TRANSPORT_F_END - VIRTIO_TRANSPORT_F_START)) - 1;
	transport <<= VIRTIO_TRANSPORT_F_START;

	mask = -1ULL & ~transport;
	mask |= VIRTIO_RING_F_INDIRECT_DESC;
	mask |= VIRTIO_RING_F_EVENT_IDX;
	mask |= VIRTIO_F_VERSION_1;

	return (features & mask);
}

bool
virtio_bus_is_modern(device_t dev)
{
	uintptr_t modern;

	virtio_read_ivar(dev, VIRTIO_IVAR_MODERN, &modern);
	return (modern != 0);
}

void
virtio_read_device_config_array(device_t dev, bus_size_t offset, void *dst,
    int size, int count)
{
	int i, gen;

	do {
		gen = virtio_config_generation(dev);

		for (i = 0; i < count; i++) {
			virtio_read_device_config(dev, offset + i * size,
			    (uint8_t *) dst + i * size, size);
		}
	} while (gen != virtio_config_generation(dev));
}

/*
 * VirtIO bus method wrappers.
 */

void
virtio_read_ivar(device_t dev, int ivar, uintptr_t *val)
{

	*val = -1;
	BUS_READ_IVAR(device_get_parent(dev), dev, ivar, val);
}

void
virtio_write_ivar(device_t dev, int ivar, uintptr_t val)
{

	BUS_WRITE_IVAR(device_get_parent(dev), dev, ivar, val);
}

uint64_t
virtio_negotiate_features(device_t dev, uint64_t child_features)
{

	return (VIRTIO_BUS_NEGOTIATE_FEATURES(device_get_parent(dev),
	    child_features));
}

int
virtio_finalize_features(device_t dev)
{

	return (VIRTIO_BUS_FINALIZE_FEATURES(device_get_parent(dev)));
}

int
virtio_alloc_virtqueues(device_t dev, int nvqs,
    struct vq_alloc_info *info)
{

	return (VIRTIO_BUS_ALLOC_VIRTQUEUES(device_get_parent(dev), nvqs, info));
}

int
virtio_setup_intr(device_t dev, enum intr_type type)
{

	return (VIRTIO_BUS_SETUP_INTR(device_get_parent(dev), type));
}

bool
virtio_with_feature(device_t dev, uint64_t feature)
{

	return (VIRTIO_BUS_WITH_FEATURE(device_get_parent(dev), feature));
}

void
virtio_stop(device_t dev)
{

	VIRTIO_BUS_STOP(device_get_parent(dev));
}

int
virtio_reinit(device_t dev, uint64_t features)
{

	return (VIRTIO_BUS_REINIT(device_get_parent(dev), features));
}

void
virtio_reinit_complete(device_t dev)
{

	VIRTIO_BUS_REINIT_COMPLETE(device_get_parent(dev));
}

int
virtio_config_generation(device_t dev)
{

	return (VIRTIO_BUS_CONFIG_GENERATION(device_get_parent(dev)));
}

void
virtio_read_device_config(device_t dev, bus_size_t offset, void *dst, int len)
{

	VIRTIO_BUS_READ_DEVICE_CONFIG(device_get_parent(dev),
	    offset, dst, len);
}

void
virtio_write_device_config(device_t dev, bus_size_t offset, const void *dst, int len)
{

	VIRTIO_BUS_WRITE_DEVICE_CONFIG(device_get_parent(dev),
	    offset, dst, len);
}

int
virtio_child_pnpinfo(device_t busdev __unused, device_t child, struct sbuf *sb)
{

	/*
	 * All of these PCI fields will be only 16 bits, but on the vtmmio bus
	 * the corresponding fields (only "vendor" and "device_type") are 32
	 * bits.  Many virtio drivers can attach below either bus.
	 * Gratuitously expand these two fields to 32-bits to allow sharing PNP
	 * match table data between the mostly-similar buses.
	 *
	 * Subdevice and device_type are redundant in both buses, so I don't
	 * see a lot of PNP utility in exposing the same value under a
	 * different name.
	 */
	sbuf_printf(sb, "vendor=0x%08x device=0x%04x subvendor=0x%04x "
	    "device_type=0x%08x", (unsigned)virtio_get_vendor(child),
	    (unsigned)virtio_get_device(child),
	    (unsigned)virtio_get_subvendor(child),
	    (unsigned)virtio_get_device_type(child));
	return (0);
}

static int
virtio_modevent(module_t mod, int type, void *unused)
{
	int error;

	switch (type) {
	case MOD_LOAD:
	case MOD_QUIESCE:
	case MOD_UNLOAD:
	case MOD_SHUTDOWN:
		error = 0;
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

static moduledata_t virtio_mod = {
	"virtio",
	virtio_modevent,
	0
};

DECLARE_MODULE(virtio, virtio_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);
MODULE_VERSION(virtio, 1);
