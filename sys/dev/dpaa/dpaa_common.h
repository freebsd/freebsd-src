/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Justin Hibbits
 */

#ifndef	DPAA_COMMON_H
#define	DPAA_COMMON_H

#include <machine/atomic.h>

int	dpaa_map_private_memory(device_t dev, int idx, const char *compat,
    vm_paddr_t *addrp, size_t *sizep);

struct dpaa_fd {
	uint64_t liodn:8;
	uint64_t bpid:8;
	uint64_t eliodn:4;
	uint64_t _rsvd1:4;
	uint64_t addr:40;
	uint32_t format:3;
	uint32_t offset:9;
	uint32_t length:20;
	uint32_t cmd_stat;
} __packed;

#define	DPAA_FD_FORMAT_SHORT_MBSF	4

#define	DPAA_FD_GET_ADDR(fd)	((void *)PHYS_TO_DMAP(fd->addr))

struct dpaa_sgte {
	uint64_t addr;
	uint32_t extension:1;
	uint32_t final:1;
	uint32_t length:30;
	uint16_t bpid;
	uint16_t offset;
} __packed;
struct qman_fqr;


#define	DPAA_NUM_OF_SG_TABLE_ENTRY	16

/*
 * Ring API infrastructure
 *
 * BMan and QMan both use cache-enabled rings.  Abstract this away to a more
 * generalized interface to reduce code copying.
 *
 * Requirements:
 * - Before calling <ring>_init() the ring base (ring->ring) must be initialized
 *   to the base of the ring.
 */
#define	DPAA_RING_DECLARE(pfx)			\
	struct pfx##_ring {			\
		struct pfx##_entry *ring;	\
		struct pfx##_entry *cursor;	\
		uint8_t vbit;			\
		uint8_t	avail;			\
		uint8_t ci;			\
		uint8_t	ithresh;		\
	}

/*
 * Ring functions:
 *
 * ring_cyc_diff() -- get the (circular) difference of `l - f`
 * ring_ring_init() -- Set up the ring structures. Portal must be
 *                     initialized beforehand, and ring->ring must be nonzero.
 * ring_CARRYCLEAR() -- stealth math to do circular roll-over
 * ring_INC() -- Increment the cursor within the ring
 * ring_update() -- Update ring entry availability count
 * ring_start() -- Reserve the next entry in the ring if available.
 * ring_commit() -- Commit the reserved ring entry by setting the verb and
 *                  AVB bit
 */
#define	DPAA_RING(pfx,sz,pi_e,ci_e,pi_i,ci_i)				\
static inline int							\
pfx##_cyc_diff(uint8_t size, uint8_t f, uint8_t l)			\
{									\
	if (f <= l)							\
		return (uint8_t)(l - f);				\
	return (uint8_t)(l + size - f);					\
}									\
static inline void							\
pfx##_ring_init(struct pfx##_ring *ring, struct dpaa_portal_softc *portal)\
{									\
	uint32_t pi = *(uint32_t*)(portal->sc_ci_va + pi_i) & (sz - 1);	\
	uint32_t ci = *(uint32_t*)(portal->sc_ci_va + ci_i);		\
	ring->ci = ci & (sz - 1);					\
	ring->vbit = !!(ci & sz) << 7;					\
	ring->cursor = ring->ring + pi;					\
	ring->avail = sz - 1 - pfx##_cyc_diff(sz, ring->ci, pi);	\
}									\
static inline void *							\
pfx##_CARRYCLEAR(struct pfx##_entry *p)					\
{									\
	return ((void *)((uintptr_t)p & (~(uintptr_t)(sz << 6))));	\
}									\
static inline void							\
pfx##_INC(struct pfx##_ring *ring)					\
{									\
	struct pfx##_entry *partial = ring->cursor + 1;			\
	ring->cursor = pfx##_CARRYCLEAR(partial);			\
	if (partial != ring->cursor)					\
		ring->vbit ^= 0x80;					\
}									\
static inline uint8_t							\
pfx##_update(struct pfx##_ring *ring, struct dpaa_portal_softc *portal)	\
{									\
	uint8_t diff, old_ci = ring->ci;				\
	ring->ci = *(uint32_t*)(portal->sc_ci_va + ci_i) & (sz - 1);	\
	diff = pfx##_cyc_diff(sz, old_ci, ring->ci);			\
	ring->avail += diff;						\
	return (diff);							\
}									\
static inline struct pfx##_entry * __unused				\
pfx##_start(struct pfx##_ring *ring, struct dpaa_portal_softc *portal)	\
{									\
	if (ring->avail <= 1) {						\
		pfx##_update(ring, portal);				\
		if (ring->avail == 0)					\
			return (NULL);					\
	}								\
	dpaa_zero_line(ring->cursor);					\
	return (ring->cursor);						\
}									\
static inline void __unused						\
pfx##_commit(struct pfx##_ring *ring, uint8_t verb)			\
{									\
	struct pfx##_entry *entry = ring->cursor;			\
	dpaa_lw_barrier();						\
	entry->verb = verb | ring->vbit;				\
	dpaa_flush_line(entry);						\
	pfx##_INC(ring);						\
	ring->avail--;							\
} struct hack

#ifdef	__powerpc__
static inline void
dpaa_flush_line(void *line)
{
	__asm __volatile ("dcbf 0, %0" :: "r"(line) : "memory");
}

static inline void
dpaa_zero_line(void *line)
{
	__asm __volatile ("dcbz 0, %0" :: "r"(line) : "memory");
}

static inline void
dpaa_touch_line(void *line)
{
	__asm __volatile ("dcbt 0, %0" :: "r"(line) : "memory");
}

static inline void
dpaa_lw_barrier(void)
{
	powerpc_lwsync();
}
#endif

#endif
