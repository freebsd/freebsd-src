/*-
 * Copyright (c) 2002-2007 Neterion, Inc.
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
 *
 * $FreeBSD: src/sys/dev/nxge/xgehal/xgehal-mm.c,v 1.1.2.1.4.1 2008/11/25 02:59:29 kensmith Exp $
 */

#include <dev/nxge/include/xge-os-pal.h>
#include <dev/nxge/include/xgehal-mm.h>
#include <dev/nxge/include/xge-debug.h>

/*
 * __hal_mempool_grow
 *
 * Will resize mempool up to %num_allocate value.
 */
xge_hal_status_e
__hal_mempool_grow(xge_hal_mempool_t *mempool, int num_allocate,
	    int *num_allocated)
{
	int i, first_time = mempool->memblocks_allocated == 0 ? 1 : 0;
	int n_items = mempool->items_per_memblock;

	*num_allocated = 0;

	if ((mempool->memblocks_allocated + num_allocate) >
	                    mempool->memblocks_max) {
	    xge_debug_mm(XGE_ERR, "%s",
	              "__hal_mempool_grow: can grow anymore");
	    return XGE_HAL_ERR_OUT_OF_MEMORY;
	}

	for (i = mempool->memblocks_allocated;
	     i < mempool->memblocks_allocated + num_allocate; i++) {
	    int j;
	    int is_last =
	        ((mempool->memblocks_allocated+num_allocate-1) == i);
	    xge_hal_mempool_dma_t *dma_object =
	        mempool->memblocks_dma_arr + i;
	    void *the_memblock;
	    int dma_flags;

	    dma_flags = XGE_OS_DMA_CACHELINE_ALIGNED;
#ifdef XGE_HAL_DMA_DTR_CONSISTENT
	    dma_flags |= XGE_OS_DMA_CONSISTENT;
#else
	    dma_flags |= XGE_OS_DMA_STREAMING;
#endif

	    /* allocate DMA-capable memblock */
	    mempool->memblocks_arr[i] = xge_os_dma_malloc(mempool->pdev,
	                        mempool->memblock_size,
	                    dma_flags,
	                        &dma_object->handle,
	                        &dma_object->acc_handle);
	    if (mempool->memblocks_arr[i] == NULL) {
	        xge_debug_mm(XGE_ERR,
	                  "memblock[%d]: out of DMA memory", i);
	        return XGE_HAL_ERR_OUT_OF_MEMORY;
	    }
	    xge_os_memzero(mempool->memblocks_arr[i],
	    mempool->memblock_size);
	    the_memblock = mempool->memblocks_arr[i];

	    /* allocate memblock's private part. Each DMA memblock
	     * has a space allocated for item's private usage upon
	     * mempool's user request. Each time mempool grows, it will
	     * allocate new memblock and its private part at once.
	     * This helps to minimize memory usage a lot. */
	    mempool->memblocks_priv_arr[i] = xge_os_malloc(mempool->pdev,
	                mempool->items_priv_size * n_items);
	    if (mempool->memblocks_priv_arr[i] == NULL) {
	        xge_os_dma_free(mempool->pdev,
	                  the_memblock,
	                  mempool->memblock_size,
	                  &dma_object->acc_handle,
	                  &dma_object->handle);
	        xge_debug_mm(XGE_ERR,
	                "memblock_priv[%d]: out of virtual memory, "
	                "requested %d(%d:%d) bytes", i,
	            mempool->items_priv_size * n_items,
	            mempool->items_priv_size, n_items);
	        return XGE_HAL_ERR_OUT_OF_MEMORY;
	    }
	    xge_os_memzero(mempool->memblocks_priv_arr[i],
	             mempool->items_priv_size * n_items);

	    /* map memblock to physical memory */
	    dma_object->addr = xge_os_dma_map(mempool->pdev,
	                                    dma_object->handle,
	                    the_memblock,
	                    mempool->memblock_size,
	                    XGE_OS_DMA_DIR_BIDIRECTIONAL,
#ifdef XGE_HAL_DMA_DTR_CONSISTENT
	                        XGE_OS_DMA_CONSISTENT
#else
	                        XGE_OS_DMA_STREAMING
#endif
	                                            );
	    if (dma_object->addr == XGE_OS_INVALID_DMA_ADDR) {
	        xge_os_free(mempool->pdev, mempool->memblocks_priv_arr[i],
	              mempool->items_priv_size *
	                n_items);
	        xge_os_dma_free(mempool->pdev,
	                  the_memblock,
	                  mempool->memblock_size,
	                  &dma_object->acc_handle,
	                  &dma_object->handle);
	        return XGE_HAL_ERR_OUT_OF_MAPPING;
	    }

	    /* fill the items hash array */
	    for (j=0; j<n_items; j++) {
	        int index = i*n_items + j;

	        if (first_time && index >= mempool->items_initial) {
	            break;
	        }

	        mempool->items_arr[index] =
	            ((char *)the_memblock + j*mempool->item_size);

	        /* let caller to do more job on each item */
	        if (mempool->item_func_alloc != NULL) {
	            xge_hal_status_e status;

	            if ((status = mempool->item_func_alloc(
	                mempool,
	                the_memblock,
	                i,
	                dma_object,
	                mempool->items_arr[index],
	                index,
	                is_last,
	                mempool->userdata)) != XGE_HAL_OK) {

	                if (mempool->item_func_free != NULL) {
	                    int k;

	                    for (k=0; k<j; k++) {

	                        index =i*n_items + k;

	                      (void)mempool->item_func_free(
	                         mempool, the_memblock,
	                         i, dma_object,
	                         mempool->items_arr[index],
	                         index, is_last,
	                         mempool->userdata);
	                    }
	                }

	                xge_os_free(mempool->pdev,
	                     mempool->memblocks_priv_arr[i],
	                     mempool->items_priv_size *
	                     n_items);
	                xge_os_dma_unmap(mempool->pdev,
	                     dma_object->handle,
	                     dma_object->addr,
	                     mempool->memblock_size,
	                     XGE_OS_DMA_DIR_BIDIRECTIONAL);
	                xge_os_dma_free(mempool->pdev,
	                     the_memblock,
	                     mempool->memblock_size,
	                     &dma_object->acc_handle,
	                     &dma_object->handle);
	                return status;
	            }
	        }

	        mempool->items_current = index + 1;
	    }

	    xge_debug_mm(XGE_TRACE,
	        "memblock%d: allocated %dk, vaddr 0x"XGE_OS_LLXFMT", "
	        "dma_addr 0x"XGE_OS_LLXFMT, i, mempool->memblock_size / 1024,
	        (unsigned long long)(ulong_t)mempool->memblocks_arr[i],
	        (unsigned long long)dma_object->addr);

	    (*num_allocated)++;

	    if (first_time && mempool->items_current ==
	                    mempool->items_initial) {
	        break;
	    }
	}

	/* increment actual number of allocated memblocks */
	mempool->memblocks_allocated += *num_allocated;

	return XGE_HAL_OK;
}

/*
 * xge_hal_mempool_create
 * @memblock_size:
 * @items_initial:
 * @items_max:
 * @item_size:
 * @item_func:
 *
 * This function will create memory pool object. Pool may grow but will
 * never shrink. Pool consists of number of dynamically allocated blocks
 * with size enough to hold %items_initial number of items. Memory is
 * DMA-able but client must map/unmap before interoperating with the device.
 * See also: xge_os_dma_map(), xge_hal_dma_unmap(), xge_hal_status_e{}.
 */
xge_hal_mempool_t*
__hal_mempool_create(pci_dev_h pdev, int memblock_size, int item_size,
	    int items_priv_size, int items_initial, int items_max,
	    xge_hal_mempool_item_f item_func_alloc,
	    xge_hal_mempool_item_f item_func_free, void *userdata)
{
	xge_hal_status_e status;
	int memblocks_to_allocate;
	xge_hal_mempool_t *mempool;
	int allocated;

	if (memblock_size < item_size) {
	    xge_debug_mm(XGE_ERR,
	        "memblock_size %d < item_size %d: misconfiguration",
	        memblock_size, item_size);
	    return NULL;
	}

	mempool = (xge_hal_mempool_t *) \
	        xge_os_malloc(pdev, sizeof(xge_hal_mempool_t));
	if (mempool == NULL) {
	    xge_debug_mm(XGE_ERR, "mempool allocation failure");
	    return NULL;
	}
	xge_os_memzero(mempool, sizeof(xge_hal_mempool_t));

	mempool->pdev           = pdev;
	mempool->memblock_size      = memblock_size;
	mempool->items_max      = items_max;
	mempool->items_initial      = items_initial;
	mempool->item_size      = item_size;
	mempool->items_priv_size    = items_priv_size;
	mempool->item_func_alloc    = item_func_alloc;
	mempool->item_func_free     = item_func_free;
	mempool->userdata       = userdata;

	mempool->memblocks_allocated = 0;

	mempool->items_per_memblock = memblock_size / item_size;

	mempool->memblocks_max = (items_max + mempool->items_per_memblock - 1) /
	                mempool->items_per_memblock;

	/* allocate array of memblocks */
	mempool->memblocks_arr = (void ** ) xge_os_malloc(mempool->pdev,
	                sizeof(void*) * mempool->memblocks_max);
	if (mempool->memblocks_arr == NULL) {
	    xge_debug_mm(XGE_ERR, "memblocks_arr allocation failure");
	    __hal_mempool_destroy(mempool);
	    return NULL;
	}
	xge_os_memzero(mempool->memblocks_arr,
	        sizeof(void*) * mempool->memblocks_max);

	/* allocate array of private parts of items per memblocks */
	mempool->memblocks_priv_arr = (void **) xge_os_malloc(mempool->pdev,
	                sizeof(void*) * mempool->memblocks_max);
	if (mempool->memblocks_priv_arr == NULL) {
	    xge_debug_mm(XGE_ERR, "memblocks_priv_arr allocation failure");
	    __hal_mempool_destroy(mempool);
	    return NULL;
	}
	xge_os_memzero(mempool->memblocks_priv_arr,
	        sizeof(void*) * mempool->memblocks_max);

	/* allocate array of memblocks DMA objects */
	mempool->memblocks_dma_arr =
	    (xge_hal_mempool_dma_t *) xge_os_malloc(mempool->pdev,
	    sizeof(xge_hal_mempool_dma_t) * mempool->memblocks_max);

	if (mempool->memblocks_dma_arr == NULL) {
	    xge_debug_mm(XGE_ERR, "memblocks_dma_arr allocation failure");
	    __hal_mempool_destroy(mempool);
	    return NULL;
	}
	xge_os_memzero(mempool->memblocks_dma_arr,
	         sizeof(xge_hal_mempool_dma_t) * mempool->memblocks_max);

	/* allocate hash array of items */
	mempool->items_arr = (void **) xge_os_malloc(mempool->pdev,
	             sizeof(void*) * mempool->items_max);
	if (mempool->items_arr == NULL) {
	    xge_debug_mm(XGE_ERR, "items_arr allocation failure");
	    __hal_mempool_destroy(mempool);
	    return NULL;
	}
	xge_os_memzero(mempool->items_arr, sizeof(void *) * mempool->items_max);

	mempool->shadow_items_arr = (void **) xge_os_malloc(mempool->pdev,
	                            sizeof(void*) *  mempool->items_max);
	if (mempool->shadow_items_arr == NULL) {
	    xge_debug_mm(XGE_ERR, "shadow_items_arr allocation failure");
	    __hal_mempool_destroy(mempool);
	    return NULL;
	}
	xge_os_memzero(mempool->shadow_items_arr,
	         sizeof(void *) * mempool->items_max);

	/* calculate initial number of memblocks */
	memblocks_to_allocate = (mempool->items_initial +
	             mempool->items_per_memblock - 1) /
	                    mempool->items_per_memblock;

	xge_debug_mm(XGE_TRACE, "allocating %d memblocks, "
	        "%d items per memblock", memblocks_to_allocate,
	        mempool->items_per_memblock);

	/* pre-allocate the mempool */
	status = __hal_mempool_grow(mempool, memblocks_to_allocate, &allocated);
	xge_os_memcpy(mempool->shadow_items_arr, mempool->items_arr,
	        sizeof(void*) * mempool->items_max);
	if (status != XGE_HAL_OK) {
	    xge_debug_mm(XGE_ERR, "mempool_grow failure");
	    __hal_mempool_destroy(mempool);
	    return NULL;
	}

	xge_debug_mm(XGE_TRACE,
	    "total: allocated %dk of DMA-capable memory",
	    mempool->memblock_size * allocated / 1024);

	return mempool;
}

/*
 * xge_hal_mempool_destroy
 */
void
__hal_mempool_destroy(xge_hal_mempool_t *mempool)
{
	int i, j;

	for (i=0; i<mempool->memblocks_allocated; i++) {
	    xge_hal_mempool_dma_t *dma_object;

	    xge_assert(mempool->memblocks_arr[i]);
	    xge_assert(mempool->memblocks_dma_arr + i);

	    dma_object = mempool->memblocks_dma_arr + i;

	    for (j=0; j<mempool->items_per_memblock; j++) {
	        int index = i*mempool->items_per_memblock + j;

	        /* to skip last partially filled(if any) memblock */
	        if (index >= mempool->items_current) {
	            break;
	        }

	        /* let caller to do more job on each item */
	        if (mempool->item_func_free != NULL) {

	            mempool->item_func_free(mempool,
	                mempool->memblocks_arr[i],
	                i, dma_object,
	                mempool->shadow_items_arr[index],
	                index, /* unused */ -1,
	                mempool->userdata);
	        }
	    }

	    xge_os_dma_unmap(mempool->pdev,
	               dma_object->handle, dma_object->addr,
	           mempool->memblock_size, XGE_OS_DMA_DIR_BIDIRECTIONAL);

	    xge_os_free(mempool->pdev, mempool->memblocks_priv_arr[i],
	        mempool->items_priv_size * mempool->items_per_memblock);

	    xge_os_dma_free(mempool->pdev, mempool->memblocks_arr[i],
	              mempool->memblock_size, &dma_object->acc_handle,
	              &dma_object->handle);
	}

	if (mempool->items_arr) {
	    xge_os_free(mempool->pdev, mempool->items_arr, sizeof(void*) *
	              mempool->items_max);
	}

	if (mempool->shadow_items_arr) {
	    xge_os_free(mempool->pdev, mempool->shadow_items_arr,
	          sizeof(void*) * mempool->items_max);
	}

	if (mempool->memblocks_dma_arr) {
	    xge_os_free(mempool->pdev, mempool->memblocks_dma_arr,
	              sizeof(xge_hal_mempool_dma_t) *
	             mempool->memblocks_max);
	}

	if (mempool->memblocks_priv_arr) {
	    xge_os_free(mempool->pdev, mempool->memblocks_priv_arr,
	              sizeof(void*) * mempool->memblocks_max);
	}

	if (mempool->memblocks_arr) {
	    xge_os_free(mempool->pdev, mempool->memblocks_arr,
	              sizeof(void*) * mempool->memblocks_max);
	}

	xge_os_free(mempool->pdev, mempool, sizeof(xge_hal_mempool_t));
}
