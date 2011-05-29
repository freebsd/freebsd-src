/*-
 * Copyright(c) 2002-2011 Exar Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification are permitted provided the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. Neither the name of the Exar Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*$FreeBSD$*/

#ifndef	VXGE_HAL_MM_H
#define	VXGE_HAL_MM_H

__EXTERN_BEGIN_DECLS

typedef void *vxge_hal_mempool_h;

/*
 * struct vxge_hal_mempool_dma_t - Represents DMA objects passed to the
 * caller.
 */
typedef struct vxge_hal_mempool_dma_t {
	dma_addr_t			addr;
	pci_dma_h			handle;
	pci_dma_acc_h			acc_handle;
} vxge_hal_mempool_dma_t;

/*
 * vxge_hal_mempool_item_f  - Mempool item alloc/free callback
 * @mempoolh: Memory pool handle.
 * @memblock: Address of memory block
 * @memblock_index: Index of memory block
 * @item: Item that gets allocated or freed.
 * @index: Item's index in the memory pool.
 * @is_last: True, if this item is the last one in the pool; false - otherwise.
 * userdata: Per-pool user context.
 *
 * Memory pool allocation/deallocation callback.
 */
typedef vxge_hal_status_e (*vxge_hal_mempool_item_f) (
	vxge_hal_mempool_h	mempoolh,
	void			*memblock,
	u32			memblock_index,
	vxge_hal_mempool_dma_t	*dma_object,
	void			*item,
	u32			index,
	u32			is_last,
	void			*userdata);

/*
 * struct vxge_hal_mempool_t - Memory pool.
 */
typedef struct vxge_hal_mempool_t {
	vxge_hal_mempool_item_f	item_func_alloc;
	vxge_hal_mempool_item_f	item_func_free;
	void			*userdata;
	void			**memblocks_arr;
	void			**memblocks_priv_arr;
	vxge_hal_mempool_dma_t	*memblocks_dma_arr;
	vxge_hal_device_h	devh;
	u32			memblock_size;
	u32			memblocks_max;
	u32			memblocks_allocated;
	u32			item_size;
	u32			items_max;
	u32			items_initial;
	u32			items_current;
	u32			items_per_memblock;
	u32			dma_flags;
	void			**items_arr;
	void			**shadow_items_arr;
	u32			items_priv_size;
} vxge_hal_mempool_t;

/*
 * __hal_mempool_item_count - Returns number of items in the mempool
 */
static inline u32
/* LINTED */
__hal_mempool_item_count(
    vxge_hal_mempool_t *mempool)
{
	return (mempool->items_current);
}

/*
 * __hal_mempool_item - Returns pointer to the item in the mempool
 * items array.
 */
static inline void *
/* LINTED */
__hal_mempool_item(
    vxge_hal_mempool_t *mempool,
    u32 items_index)
{
	return (mempool->items_arr[items_index]);
}

/*
 * __hal_mempool_item_priv - will return pointer on per item private space
 */
static inline void*
/* LINTED */
__hal_mempool_item_priv(
    vxge_hal_mempool_t *mempool,
    u32 memblock_idx,
    void *item,
    u32 *memblock_item_idx)
{
	ptrdiff_t offset;
	void *memblock = mempool->memblocks_arr[memblock_idx];

	vxge_assert(memblock);

	/* LINTED */
	offset = (u32) ((u8 *) item - (u8 *) memblock);
	vxge_assert(offset >= 0 && (u32) offset < mempool->memblock_size);

	(*memblock_item_idx) = (u32) offset / mempool->item_size;
	vxge_assert((*memblock_item_idx) < mempool->items_per_memblock);

	return ((u8 *) mempool->memblocks_priv_arr[memblock_idx] +
	    (*memblock_item_idx) * mempool->items_priv_size);
}

/*
 * __hal_mempool_items_arr - will return pointer to the items array in the
 * mempool.
 */
static inline void *
/* LINTED */
__hal_mempool_items_arr(
    vxge_hal_mempool_t *mempool)
{
	return (mempool->items_arr);
}

/*
 * __hal_mempool_memblock - will return pointer to the memblock in the
 * mempool memblocks array.
 */
static inline void *
/* LINTED */
__hal_mempool_memblock(
    vxge_hal_mempool_t *mempool,
    u32 memblock_idx)
{
	vxge_assert(mempool->memblocks_arr[memblock_idx]);
	return (mempool->memblocks_arr[memblock_idx]);
}

/*
 * __hal_mempool_memblock_dma - will return pointer to the dma block
 * corresponds to the memblock(identified by memblock_idx) in the mempool.
 */
static inline vxge_hal_mempool_dma_t *
/* LINTED */
__hal_mempool_memblock_dma(
    vxge_hal_mempool_t *mempool,
    u32 memblock_idx)
{
	return (mempool->memblocks_dma_arr + memblock_idx);
}

vxge_hal_mempool_t *
vxge_hal_mempool_create(
    vxge_hal_device_h devh,
    u32 memblock_size,
    u32 item_size,
    u32 private_size,
    u32 items_initial,
    u32 items_max,
    vxge_hal_mempool_item_f item_func_alloc,
    vxge_hal_mempool_item_f item_func_free,
    void *userdata);

void
vxge_hal_mempool_destroy(
    vxge_hal_mempool_t *mempool);


__EXTERN_END_DECLS

#endif	/* VXGE_HAL_MM_H */
