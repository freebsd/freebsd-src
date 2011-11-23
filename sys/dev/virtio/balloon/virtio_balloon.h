/*
 * This header is BSD licensed so anyone can use the definitions to implement
 * compatible drivers/servers.
 *
 * $FreeBSD$
 */

#ifndef _VIRTIO_BALLOON_H
#define _VIRTIO_BALLOON_H

#include <sys/types.h>

/* Feature bits. */
#define VIRTIO_BALLOON_F_MUST_TELL_HOST	0x1 /* Tell before reclaiming pages */
#define VIRTIO_BALLOON_F_STATS_VQ	0x2 /* Memory stats virtqueue */

/* Size of a PFN in the balloon interface. */
#define VIRTIO_BALLOON_PFN_SHIFT 12

struct virtio_balloon_config {
	/* Number of pages host wants Guest to give up. */
	uint32_t num_pages;

	/* Number of pages we've actually got in balloon. */
	uint32_t actual;
};

#define VIRTIO_BALLOON_S_SWAP_IN  0   /* Amount of memory swapped in */
#define VIRTIO_BALLOON_S_SWAP_OUT 1   /* Amount of memory swapped out */
#define VIRTIO_BALLOON_S_MAJFLT   2   /* Number of major faults */
#define VIRTIO_BALLOON_S_MINFLT   3   /* Number of minor faults */
#define VIRTIO_BALLOON_S_MEMFREE  4   /* Total amount of free memory */
#define VIRTIO_BALLOON_S_MEMTOT   5   /* Total amount of memory */
#define VIRTIO_BALLOON_S_NR       6

struct virtio_balloon_stat {
	uint16_t tag;
	uint64_t val;
} __packed;

#endif /* _VIRTIO_BALLOON_H */
