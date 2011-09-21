/*
 * This header is BSD licensed so anyone can use the definitions
 * to implement compatible drivers/servers.
 *
 * Copyright Rusty Russell IBM Corporation 2007.
 */
/* $FreeBSD$ */

#ifndef VIRTIO_RING_H
#define	VIRTIO_RING_H

#include <sys/types.h>

/* This marks a buffer as continuing via the next field. */
#define VRING_DESC_F_NEXT       1
/* This marks a buffer as write-only (otherwise read-only). */
#define VRING_DESC_F_WRITE      2
/* This means the buffer contains a list of buffer descriptors. */
#define VRING_DESC_F_INDIRECT	4

/* The Host uses this in used->flags to advise the Guest: don't kick me
 * when you add a buffer.  It's unreliable, so it's simply an
 * optimization.  Guest will still kick if it's out of buffers. */
#define VRING_USED_F_NO_NOTIFY  1
/* The Guest uses this in avail->flags to advise the Host: don't
 * interrupt me when you consume a buffer.  It's unreliable, so it's
 * simply an optimization.  */
#define VRING_AVAIL_F_NO_INTERRUPT      1

/* VirtIO ring descriptors: 16 bytes.
 * These can chain together via "next". */
struct vring_desc {
        /* Address (guest-physical). */
        uint64_t addr;
        /* Length. */
        uint32_t len;
        /* The flags as indicated above. */
        uint16_t flags;
        /* We chain unused descriptors via this, too. */
        uint16_t next;
};

struct vring_avail {
        uint16_t flags;
        uint16_t idx;
        uint16_t ring[0];
};

/* uint32_t is used here for ids for padding reasons. */
struct vring_used_elem {
        /* Index of start of used descriptor chain. */
        uint32_t id;
        /* Total length of the descriptor chain which was written to. */
        uint32_t len;
};

struct vring_used {
        uint16_t flags;
        uint16_t idx;
        struct vring_used_elem ring[0];
};

struct vring {
	unsigned int num;

	struct vring_desc *desc;
	struct vring_avail *avail;
	struct vring_used *used;
};

/* The standard layout for the ring is a continuous chunk of memory which
 * looks like this.  We assume num is a power of 2.
 *
 * struct vring {
 *      // The actual descriptors (16 bytes each)
 *      struct vring_desc desc[num];
 *
 *      // A ring of available descriptor heads with free-running index.
 *      __u16 avail_flags;
 *      __u16 avail_idx;
 *      __u16 available[num];
 *
 *      // Padding to the next align boundary.
 *      char pad[];
 *
 *      // A ring of used descriptor heads with free-running index.
 *      __u16 used_flags;
 *      __u16 used_idx;
 *      struct vring_used_elem used[num];
 * };
 *
 * NOTE: for VirtIO PCI, align is 4096.
 */

static inline int
vring_size(unsigned int num, unsigned long align)
{
	int size;

	size = num * sizeof(struct vring_desc);
	size += sizeof(struct vring_avail) + (num * sizeof(uint16_t));
	size = (size + align - 1) & ~(align - 1);
	size += sizeof(struct vring_used) +
	    (num * sizeof(struct vring_used_elem));
	return (size);
}

static inline void
vring_init(struct vring *vr, unsigned int num, uint8_t *p,
    unsigned long align)
{
        vr->num = num;
        vr->desc = (struct vring_desc *) p;
        vr->avail = (struct vring_avail *) (p +
	    num * sizeof(struct vring_desc));
        vr->used = (void *)
	    (((unsigned long) &vr->avail->ring[num] + align-1) & ~(align-1));
}
#endif /* VIRTIO_RING_H */
