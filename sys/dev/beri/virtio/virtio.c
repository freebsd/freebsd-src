/*-
 * Copyright (c) 2014 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 * BERI virtio mmio backend common methods
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/timeet.h>
#include <sys/timetc.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/event.h>
#include <sys/selinfo.h>
#include <sys/endian.h>
#include <sys/rwlock.h>

#include <machine/bus.h>
#include <machine/fdt.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include <dev/beri/virtio/virtio.h>
#include <dev/virtio/virtio.h>
#include <dev/virtio/virtqueue.h>
#include <dev/virtio/virtio_ring.h>

int
vq_ring_ready(struct vqueue_info *vq)
{

	return (vq->vq_flags & VQ_ALLOC);
}

int
vq_has_descs(struct vqueue_info *vq)
{

	return (vq_ring_ready(vq) && vq->vq_last_avail !=
		be16toh(vq->vq_avail->idx));
}

void *
paddr_map(uint32_t offset, uint32_t phys, uint32_t size)
{
	bus_space_handle_t bsh;

	if (bus_space_map(fdtbus_bs_tag, (phys + offset),
			size, 0, &bsh) != 0) {
		panic("Couldn't map 0x%08x\n", (phys + offset));
	}

	return (void *)(bsh);
}

void
paddr_unmap(void *phys, uint32_t size)
{

	bus_space_unmap(fdtbus_bs_tag, (bus_space_handle_t)phys, size);
}

static inline void
_vq_record(uint32_t offs, int i, volatile struct vring_desc *vd,
	struct iovec *iov, int n_iov, uint16_t *flags) {

	if (i >= n_iov)
		return;

	iov[i].iov_base = paddr_map(offs, be64toh(vd->addr),
				be32toh(vd->len));
	iov[i].iov_len = be32toh(vd->len);
	if (flags != NULL)
		flags[i] = be16toh(vd->flags);
}

int
vq_getchain(uint32_t offs, struct vqueue_info *vq,
	struct iovec *iov, int n_iov, uint16_t *flags)
{
	volatile struct vring_desc *vdir, *vindir, *vp;
	int idx, ndesc, n_indir;
	int head, next;
	int i;

	idx = vq->vq_last_avail;
	ndesc = (be16toh(vq->vq_avail->idx) - idx);
	if (ndesc == 0)
		return (0);

	head = be16toh(vq->vq_avail->ring[idx & (vq->vq_qsize - 1)]);
	next = head;

	for (i = 0; i < VQ_MAX_DESCRIPTORS; next = be16toh(vdir->next)) {
		vdir = &vq->vq_desc[next];
		if ((be16toh(vdir->flags) & VRING_DESC_F_INDIRECT) == 0) {
			_vq_record(offs, i, vdir, iov, n_iov, flags);
			i++;
		} else {
			n_indir = be32toh(vdir->len) / 16;
			vindir = paddr_map(offs, be64toh(vdir->addr),
					be32toh(vdir->len));
			next = 0;
			for (;;) {
				vp = &vindir[next];
				_vq_record(offs, i, vp, iov, n_iov, flags);
				i+=1;
				if ((be16toh(vp->flags) & \
					VRING_DESC_F_NEXT) == 0)
					break;
				next = be16toh(vp->next);
			}
			paddr_unmap((void *)vindir, be32toh(vdir->len));
		}

		if ((be16toh(vdir->flags) & VRING_DESC_F_NEXT) == 0)
			return (i);
	}

	return (i);
}

void
vq_relchain(struct vqueue_info *vq, struct iovec *iov, int n, uint32_t iolen)
{
	volatile struct vring_used_elem *vue;
	volatile struct vring_used *vu;
	uint16_t head, uidx, mask;
	int i;

	mask = vq->vq_qsize - 1;
	head = be16toh(vq->vq_avail->ring[vq->vq_last_avail++ & mask]);

	vu = vq->vq_used;
	uidx = be16toh(vu->idx);
	vue = &vu->ring[uidx++ & mask];
	vue->id = htobe16(head);
	vue->len = htobe32(iolen);
	vu->idx = htobe16(uidx);

	/* Clean up */
	for (i = 1; i < (n-1); i++) {
		paddr_unmap((void *)iov[i].iov_base, iov[i].iov_len);
	}
}
