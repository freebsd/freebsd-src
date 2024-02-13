/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2024, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file ice_osdep.h
 * @brief OS compatibility layer
 *
 * Contains various definitions and functions which are part of an OS
 * compatibility layer for sharing code with other operating systems.
 */
#ifndef _ICE_OSDEP_H_
#define _ICE_OSDEP_H_

#include <sys/endian.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/bus_dma.h>
#include <netinet/in.h>
#include <sys/counter.h>
#include <sys/sbuf.h>

#include "ice_alloc.h"

#define ICE_INTEL_VENDOR_ID 0x8086

#define ICE_STR_BUF_LEN 32

struct ice_hw;

device_t ice_hw_to_dev(struct ice_hw *hw);

/* configure hw->debug_mask to enable debug prints */
void ice_debug(struct ice_hw *hw, uint64_t mask, char *fmt, ...) __printflike(3, 4);
void ice_debug_array(struct ice_hw *hw, uint64_t mask, uint32_t rowsize,
		     uint32_t groupsize, uint8_t *buf, size_t len);
void ice_info_fwlog(struct ice_hw *hw, uint32_t rowsize, uint32_t groupsize,
		    uint8_t *buf, size_t len);

#define ice_fls(_n) flsl(_n)

#define ice_info(_hw, _fmt, args...) \
	device_printf(ice_hw_to_dev(_hw), (_fmt), ##args)

#define ice_warn(_hw, _fmt, args...) \
	device_printf(ice_hw_to_dev(_hw), (_fmt), ##args)

#define DIVIDE_AND_ROUND_UP howmany
#define ROUND_UP roundup

uint32_t rd32(struct ice_hw *hw, uint32_t reg);
uint64_t rd64(struct ice_hw *hw, uint32_t reg);
void wr32(struct ice_hw *hw, uint32_t reg, uint32_t val);
void wr64(struct ice_hw *hw, uint32_t reg, uint64_t val);

#define ice_flush(_hw) rd32((_hw), GLGEN_STAT)

MALLOC_DECLARE(M_ICE_OSDEP);

/**
 * ice_calloc - Allocate an array of elementes
 * @hw: the hardware private structure
 * @count: number of elements to allocate
 * @size: the size of each element
 *
 * Allocate memory for an array of items equal to size. Note that the OS
 * compatibility layer assumes all allocation functions will provide zero'd
 * memory.
 */
static inline void *
ice_calloc(struct ice_hw __unused *hw, size_t count, size_t size)
{
	return malloc(count * size, M_ICE_OSDEP, M_ZERO | M_NOWAIT);
}

/**
 * ice_malloc - Allocate memory of a specified size
 * @hw: the hardware private structure
 * @size: the size to allocate
 *
 * Allocates memory of the specified size. Note that the OS compatibility
 * layer assumes that all allocations will provide zero'd memory.
 */
static inline void *
ice_malloc(struct ice_hw __unused *hw, size_t size)
{
	return malloc(size, M_ICE_OSDEP, M_ZERO | M_NOWAIT);
}

/**
 * ice_memdup - Allocate a copy of some other memory
 * @hw: private hardware structure
 * @src: the source to copy from
 * @size: allocation size
 * @dir: the direction of copying
 *
 * Allocate memory of the specified size, and copy bytes from the src to fill
 * it. We don't need to zero this memory as we immediately initialize it by
 * copying from the src pointer.
 */
static inline void *
ice_memdup(struct ice_hw __unused *hw, const void *src, size_t size,
	   enum ice_memcpy_type __unused dir)
{
	void *dst = malloc(size, M_ICE_OSDEP, M_NOWAIT);

	if (dst != NULL)
		memcpy(dst, src, size);

	return dst;
}

/**
 * ice_free - Free previously allocated memory
 * @hw: the hardware private structure
 * @mem: pointer to the memory to free
 *
 * Free memory that was previously allocated by ice_calloc, ice_malloc, or
 * ice_memdup.
 */
static inline void
ice_free(struct ice_hw __unused *hw, void *mem)
{
	free(mem, M_ICE_OSDEP);
}

/* These are macros in order to drop the unused direction enumeration constant */
#define ice_memset(addr, c, len, unused) memset((addr), (c), (len))
#define ice_memcpy(dst, src, len, unused) memcpy((dst), (src), (len))

void ice_usec_delay(uint32_t time, bool sleep);
void ice_msec_delay(uint32_t time, bool sleep);
void ice_msec_pause(uint32_t time);
void ice_msec_spin(uint32_t time);

#define UNREFERENCED_PARAMETER(_p) _p = _p
#define UNREFERENCED_1PARAMETER(_p) do {			\
	UNREFERENCED_PARAMETER(_p);				\
} while (0)
#define UNREFERENCED_2PARAMETER(_p, _q) do {			\
	UNREFERENCED_PARAMETER(_p);				\
	UNREFERENCED_PARAMETER(_q);				\
} while (0)
#define UNREFERENCED_3PARAMETER(_p, _q, _r) do {		\
	UNREFERENCED_PARAMETER(_p);				\
	UNREFERENCED_PARAMETER(_q);				\
	UNREFERENCED_PARAMETER(_r);				\
} while (0)
#define UNREFERENCED_4PARAMETER(_p, _q, _r, _s) do {		\
	UNREFERENCED_PARAMETER(_p);				\
	UNREFERENCED_PARAMETER(_q);				\
	UNREFERENCED_PARAMETER(_r);				\
	UNREFERENCED_PARAMETER(_s);				\
} while (0)
#define UNREFERENCED_5PARAMETER(_p, _q, _r, _s, _t) do {	\
	UNREFERENCED_PARAMETER(_p);				\
	UNREFERENCED_PARAMETER(_q);				\
	UNREFERENCED_PARAMETER(_r);				\
	UNREFERENCED_PARAMETER(_s);				\
	UNREFERENCED_PARAMETER(_t);				\
} while (0)

#define FIELD_SIZEOF(t, f) (sizeof(((t*)0)->f))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define MAKEMASK(_m, _s) ((_m) << (_s))

#define LIST_HEAD_TYPE ice_list_head
#define LIST_ENTRY_TYPE ice_list_node

/**
 * @struct ice_list_node
 * @brief simplified linked list node API
 *
 * Represents a node in a linked list, which can be embedded into a structure
 * to allow that structure to be inserted into a linked list. Access to the
 * contained structure is done via __containerof
 */
struct ice_list_node {
	LIST_ENTRY(ice_list_node) entries;
};

/**
 * @struct ice_list_head
 * @brief simplified linked list head API
 *
 * Represents the head of a linked list. The linked list should consist of
 * a series of ice_list_node structures embedded into another structure
 * accessed using __containerof. This way, the ice_list_head doesn't need to
 * know the type of the structure it contains.
 */
LIST_HEAD(ice_list_head, ice_list_node);

#define INIT_LIST_HEAD LIST_INIT
/* LIST_EMPTY doesn't need to be changed */
#define LIST_ADD(entry, head) LIST_INSERT_HEAD(head, entry, entries)
#define LIST_ADD_AFTER(entry, elem) LIST_INSERT_AFTER(elem, entry, entries)
#define LIST_DEL(entry) LIST_REMOVE(entry, entries)
#define _osdep_LIST_ENTRY(ptr, type, member) \
	__containerof(ptr, type, member)
#define LIST_FIRST_ENTRY(head, type, member) \
	_osdep_LIST_ENTRY(LIST_FIRST(head), type, member)
#define LIST_NEXT_ENTRY(ptr, unused, member) \
	_osdep_LIST_ENTRY(LIST_NEXT(&(ptr->member), entries), __typeof(*ptr), member)
#define LIST_REPLACE_INIT(old_head, new_head) do {			\
	__typeof(new_head) _new_head = (new_head);			\
	LIST_INIT(_new_head);						\
	LIST_SWAP(old_head, _new_head, ice_list_node, entries);		\
} while (0)

#define LIST_ENTRY_SAFE(_ptr, _type, _member) \
({ __typeof(_ptr) ____ptr = (_ptr); \
   ____ptr ? _osdep_LIST_ENTRY(____ptr, _type, _member) : NULL; \
})

/**
 * ice_get_list_tail - Return the pointer to the last node in the list
 * @head: the pointer to the head of the list
 *
 * A helper function for implementing LIST_ADD_TAIL and LIST_LAST_ENTRY.
 * Returns the pointer to the last node in the list, or NULL of the list is
 * empty.
 *
 * Note: due to the list implementation this is O(N), where N is the size of
 * the list. An O(1) implementation requires replacing the underlying list
 * datastructure with one that has a tail pointer. This is problematic,
 * because using a simple TAILQ would require that the addition and deletion
 * be given the head of the list.
 */
static inline struct ice_list_node *
ice_get_list_tail(struct ice_list_head *head)
{
	struct ice_list_node *node = LIST_FIRST(head);

	if (node == NULL)
		return NULL;
	while (LIST_NEXT(node, entries) != NULL)
		node = LIST_NEXT(node, entries);

	return node;
}

/* TODO: This is O(N). An O(1) implementation would require a different
 * underlying list structure, such as a circularly linked list. */
#define LIST_ADD_TAIL(entry, head) do {					\
	struct ice_list_node *node = ice_get_list_tail(head);		\
									\
	if (node == NULL) {						\
		LIST_ADD(entry, head);					\
	} else {							\
		LIST_INSERT_AFTER(node, entry, entries);		\
	}								\
} while (0)

#define LIST_LAST_ENTRY(head, type, member) \
	LIST_ENTRY_SAFE(ice_get_list_tail(head), type, member)

#define LIST_FIRST_ENTRY_SAFE(head, type, member) \
	LIST_ENTRY_SAFE(LIST_FIRST(head), type, member)

#define LIST_NEXT_ENTRY_SAFE(ptr, member) \
	LIST_ENTRY_SAFE(LIST_NEXT(&(ptr->member), entries), __typeof(*ptr), member)

#define LIST_FOR_EACH_ENTRY(pos, head, unused, member) \
	for (pos = LIST_FIRST_ENTRY_SAFE(head, __typeof(*pos), member);		\
	    pos;								\
	    pos = LIST_NEXT_ENTRY_SAFE(pos, member))

#define LIST_FOR_EACH_ENTRY_SAFE(pos, n, head, unused, member) \
	for (pos = LIST_FIRST_ENTRY_SAFE(head, __typeof(*pos), member);		\
	     pos && ({ n = LIST_NEXT_ENTRY_SAFE(pos, member); 1; });		\
	     pos = n)

#define STATIC static

#define NTOHS ntohs
#define NTOHL ntohl
#define HTONS htons
#define HTONL htonl
#define LE16_TO_CPU le16toh
#define LE32_TO_CPU le32toh
#define LE64_TO_CPU le64toh
#define CPU_TO_LE16 htole16
#define CPU_TO_LE32 htole32
#define CPU_TO_LE64 htole64
#define CPU_TO_BE16 htobe16
#define CPU_TO_BE32 htobe32

#define SNPRINTF snprintf

/**
 * @typedef u8
 * @brief compatibility typedef for uint8_t
 */
typedef uint8_t  u8;

/**
 * @typedef u16
 * @brief compatibility typedef for uint16_t
 */
typedef uint16_t u16;

/**
 * @typedef u32
 * @brief compatibility typedef for uint32_t
 */
typedef uint32_t u32;

/**
 * @typedef u64
 * @brief compatibility typedef for uint64_t
 */
typedef uint64_t u64;

/**
 * @typedef s8
 * @brief compatibility typedef for int8_t
 */
typedef int8_t  s8;

/**
 * @typedef s16
 * @brief compatibility typedef for int16_t
 */
typedef int16_t s16;

/**
 * @typedef s32
 * @brief compatibility typedef for int32_t
 */
typedef int32_t s32;

/**
 * @typedef s64
 * @brief compatibility typedef for int64_t
 */
typedef int64_t s64;

#define __le16 u16
#define __le32 u32
#define __le64 u64
#define __be16 u16
#define __be32 u32
#define __be64 u64

#define ice_hweight8(x) bitcount16((u8)x)
#define ice_hweight16(x) bitcount16(x)
#define ice_hweight32(x) bitcount32(x)
#define ice_hweight64(x) bitcount64(x)

/**
 * @struct ice_dma_mem
 * @brief DMA memory allocation
 *
 * Contains DMA allocation bits, used to simplify DMA allocations.
 */
struct ice_dma_mem {
	void *va;
	uint64_t pa;
	size_t size;

	bus_dma_tag_t		tag;
	bus_dmamap_t		map;
	bus_dma_segment_t	seg;
};


void * ice_alloc_dma_mem(struct ice_hw *hw, struct ice_dma_mem *mem, u64 size);
void ice_free_dma_mem(struct ice_hw __unused *hw, struct ice_dma_mem *mem);

/**
 * @struct ice_lock
 * @brief simplified lock API
 *
 * Contains a simple lock implementation used to lock various resources.
 */
struct ice_lock {
	struct mtx mutex;
	char name[ICE_STR_BUF_LEN];
};

extern u16 ice_lock_count;

/**
 * ice_init_lock - Initialize a lock for use
 * @lock: the lock memory to initialize
 *
 * OS compatibility layer to provide a simple locking mechanism. We use
 * a mutex for this purpose.
 */
static inline void
ice_init_lock(struct ice_lock *lock)
{
	/*
	 * Make each lock unique by incrementing a counter each time this
	 * function is called. Use of a u16 allows 65535 possible locks before
	 * we'd hit a duplicate.
	 */
	memset(lock->name, 0, sizeof(lock->name));
	snprintf(lock->name, ICE_STR_BUF_LEN, "ice_lock_%u", ice_lock_count++);
	mtx_init(&lock->mutex, lock->name, NULL, MTX_DEF);
}

/**
 * ice_acquire_lock - Acquire the lock
 * @lock: the lock to acquire
 *
 * Acquires the mutex specified by the lock pointer.
 */
static inline void
ice_acquire_lock(struct ice_lock *lock)
{
	mtx_lock(&lock->mutex);
}

/**
 * ice_release_lock - Release the lock
 * @lock: the lock to release
 *
 * Releases the mutex specified by the lock pointer.
 */
static inline void
ice_release_lock(struct ice_lock *lock)
{
	mtx_unlock(&lock->mutex);
}

/**
 * ice_destroy_lock - Destroy the lock to de-allocate it
 * @lock: the lock to destroy
 *
 * Destroys a previously initialized lock. We only do this if the mutex was
 * previously initialized.
 */
static inline void
ice_destroy_lock(struct ice_lock *lock)
{
	if (mtx_initialized(&lock->mutex))
		mtx_destroy(&lock->mutex);
	memset(lock->name, 0, sizeof(lock->name));
}

/* Some function parameters are unused outside of MPASS/KASSERT macros. Rather
 * than marking these as __unused all the time, mark them as __invariant_only,
 * and define this to __unused when INVARIANTS is disabled. Otherwise, define
 * it empty so that __invariant_only parameters are caught as unused by the
 * INVARIANTS build.
 */
#ifndef INVARIANTS
#define __invariant_only __unused
#else
#define __invariant_only
#endif

#define __ALWAYS_UNUSED __unused

/**
 * ice_ilog2 - Calculate the integer log base 2 of a 64bit value
 * @n: 64bit number
 *
 * Calculates the integer log base 2 of a 64bit value, rounded down.
 *
 * @remark The integer log base 2 of zero is technically undefined, but this
 * function will return 0 in that case.
 *
 */
static inline int
ice_ilog2(u64 n) {
	if (n == 0)
		return 0;
	return flsll(n) - 1;
}

/**
 * ice_is_pow2 - Check if the value is a power of 2
 * @n: 64bit number
 *
 * Check if the given value is a power of 2.
 *
 * @remark FreeBSD's powerof2 function treats zero as a power of 2, while this
 * function does not.
 *
 * @returns true or false
 */
static inline bool
ice_is_pow2(u64 n) {
	if (n == 0)
		return false;
	return powerof2(n);
}
#endif /* _ICE_OSDEP_H_ */
