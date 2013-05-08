/*-
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_VIRTIO_H_
#define	_VIRTIO_H_

#define VRING_ALIGN	4096

#define VRING_DESC_F_NEXT	(1 << 0)
#define VRING_DESC_F_WRITE	(1 << 1)
#define VRING_DESC_F_INDIRECT	(1 << 2)

#define VRING_AVAIL_F_NO_INTERRUPT   1
#define VIRTIO_MSI_NO_VECTOR	0xFFFF

struct virtio_desc {
	uint64_t	vd_addr;
	uint32_t	vd_len;
	uint16_t	vd_flags;
	uint16_t	vd_next;
} __packed;

struct virtio_used {
	uint32_t	vu_idx;
	uint32_t	vu_tlen;
} __packed;

/*
 * PFN register shift amount
 */
#define VRING_PFN		12

/*
 * Virtio device types
 */
#define VIRTIO_TYPE_NET		1
#define VIRTIO_TYPE_BLOCK	2

/*
 * PCI vendor/device IDs
 */
#define VIRTIO_VENDOR		0x1AF4
#define VIRTIO_DEV_NET		0x1000
#define VIRTIO_DEV_BLOCK	0x1001

/*
 * PCI config space constants
 */
#define VTCFG_R_HOSTCAP		0
#define VTCFG_R_GUESTCAP	4
#define VTCFG_R_PFN		8
#define VTCFG_R_QNUM		12
#define VTCFG_R_QSEL		14
#define VTCFG_R_QNOTIFY		16
#define VTCFG_R_STATUS		18
#define VTCFG_R_ISR		19
#define VTCFG_R_CFGVEC		20
#define VTCFG_R_QVEC		22
#define VTCFG_R_CFG0		20	/* No MSI-X */
#define VTCFG_R_CFG1		24	/* With MSI-X */
#define VTCFG_R_MSIX		20

/* Feature flags */
#define	VIRTIO_F_NOTIFY_ON_EMPTY	(1 << 24)

/* From section 2.3, "Virtqueue Configuration", of the virtio specification */
static inline u_int
vring_size(u_int qsz)
{
	u_int size;

	size = sizeof(struct virtio_desc) * qsz + sizeof(uint16_t) * (3 + qsz);
	size = roundup2(size, VRING_ALIGN);

	size += sizeof(uint16_t) * 3 + sizeof(struct virtio_used) * qsz;
	size = roundup2(size, VRING_ALIGN);

	return (size);
}

#endif	/* _VIRTIO_H_ */
