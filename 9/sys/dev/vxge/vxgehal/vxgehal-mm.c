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

#include <dev/vxge/vxgehal/vxgehal.h>

/*
 * __hal_mempool_grow
 *
 * Will resize mempool up to %num_allocate value.
 */
static vxge_hal_status_e
__hal_mempool_grow(
    vxge_hal_mempool_t *mempool,
    u32 num_allocate,
    u32 *num_allocated)
{
	u32 i, j, k, item_index, is_last;
	u32 first_time = mempool->memblocks_allocated == 0 ? 1 : 0;
	u32 n_items = mempool->items_per_memblock;
	u32 start_block_idx = mempool->memblocks_allocated;
	u32 end_block_idx = mempool->memblocks_allocated + num_allocate;
	__hal_device_t *hldev;

	vxge_assert(mempool != NULL);

	hldev = (__hal_device_t *) mempool->devh;

	vxge_hal_trace_log_mm("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mm(
	    "mempool = 0x"VXGE_OS_STXFMT", num_allocate = %d, "
	    "num_allocated = 0x"VXGE_OS_STXFMT, (ptr_t) mempool,
	    num_allocate, (ptr_t) num_allocated);

	*num_allocated = 0;

	if (end_block_idx > mempool->memblocks_max) {
		vxge_hal_err_log_mm("%s",
		    "__hal_mempool_grow: can grow anymore");
		vxge_hal_trace_log_mm("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_OUT_OF_MEMORY);
		return (VXGE_HAL_ERR_OUT_OF_MEMORY);
	}

	for (i = start_block_idx; i < end_block_idx; i++) {

		void *the_memblock;
		vxge_hal_mempool_dma_t *dma_object;

		is_last = ((end_block_idx - 1) == i);
		dma_object = mempool->memblocks_dma_arr + i;

		/*
		 * allocate memblock's private part. Each DMA memblock
		 * has a space allocated for item's private usage upon
		 * mempool's user request. Each time mempool grows, it will
		 * allocate new memblock and its private part at once.
		 * This helps to minimize memory usage a lot.
		 */
		mempool->memblocks_priv_arr[i] = vxge_os_malloc(
		    ((__hal_device_t *) mempool->devh)->header.pdev,
		    mempool->items_priv_size * n_items);
		if (mempool->memblocks_priv_arr[i] == NULL) {

			vxge_hal_err_log_mm("memblock_priv[%d]: \
			    out of virtual memory, "
			    "requested %d(%d:%d) bytes", i,
			    mempool->items_priv_size * n_items,
			    mempool->items_priv_size, n_items);
			vxge_hal_trace_log_mm("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__,
			    VXGE_HAL_ERR_OUT_OF_MEMORY);
			return (VXGE_HAL_ERR_OUT_OF_MEMORY);

		}

		vxge_os_memzero(mempool->memblocks_priv_arr[i],
		    mempool->items_priv_size * n_items);

		/* allocate DMA-capable memblock */
		mempool->memblocks_arr[i] =
		    __hal_blockpool_malloc(mempool->devh,
		    mempool->memblock_size,
		    &dma_object->addr,
		    &dma_object->handle,
		    &dma_object->acc_handle);
		if (mempool->memblocks_arr[i] == NULL) {
			vxge_os_free(
			    ((__hal_device_t *) mempool->devh)->header.pdev,
			    mempool->memblocks_priv_arr[i],
			    mempool->items_priv_size * n_items);
			vxge_hal_err_log_mm("memblock[%d]: \
			    out of DMA memory", i);
			vxge_hal_trace_log_mm("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__,
			    VXGE_HAL_ERR_OUT_OF_MEMORY);
			return (VXGE_HAL_ERR_OUT_OF_MEMORY);
		}

		(*num_allocated)++;
		mempool->memblocks_allocated++;

		vxge_os_memzero(mempool->memblocks_arr[i],
		    mempool->memblock_size);

		the_memblock = mempool->memblocks_arr[i];

		/* fill the items hash array */
		for (j = 0; j < n_items; j++) {
			item_index = i * n_items + j;

			if (first_time && (item_index >= mempool->items_initial))
				break;

			mempool->items_arr[item_index] =
			    ((char *) the_memblock + j *mempool->item_size);

			/* let caller to do more job on each item */
			if (mempool->item_func_alloc != NULL) {
				vxge_hal_status_e status;

				if ((status = mempool->item_func_alloc(
				    mempool,
				    the_memblock,
				    i,
				    dma_object,
				    mempool->items_arr[item_index],
				    item_index,
				    is_last,
				    mempool->userdata)) != VXGE_HAL_OK) {

					if (mempool->item_func_free != NULL) {

						for (k = 0; k < j; k++) {

							item_index = i * n_items + k;

							(void) mempool->item_func_free(
							    mempool,
							    the_memblock,
							    i, dma_object,
							    mempool->items_arr[item_index],
							    item_index, is_last,
							    mempool->userdata);
						}
					}

					vxge_os_free(((__hal_device_t *)
					    mempool->devh)->header.pdev,
					    mempool->memblocks_priv_arr[i],
					    mempool->items_priv_size *
					    n_items);

					__hal_blockpool_free(mempool->devh,
					    the_memblock,
					    mempool->memblock_size,
					    &dma_object->addr,
					    &dma_object->handle,
					    &dma_object->acc_handle);

					(*num_allocated)--;
					mempool->memblocks_allocated--;
					return (status);
				}
			}

			mempool->items_current = item_index + 1;
		}

		vxge_hal_info_log_mm(
		    "memblock%d: allocated %dk, vaddr 0x"VXGE_OS_STXFMT", "
		    "dma_addr 0x"VXGE_OS_STXFMT,
		    i, mempool->memblock_size / 1024,
		    (ptr_t) mempool->memblocks_arr[i], dma_object->addr);

		if (first_time && mempool->items_current ==
		    mempool->items_initial) {
			break;
		}
	}

	vxge_hal_trace_log_mm("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_mempool_create
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
 * See also: vxge_os_dma_map(), vxge_hal_dma_unmap(), vxge_hal_status_e {}.
 */
vxge_hal_mempool_t *
vxge_hal_mempool_create(
    vxge_hal_device_h devh,
    u32 memblock_size,
    u32 item_size,
    u32 items_priv_size,
    u32 items_initial,
    u32 items_max,
    vxge_hal_mempool_item_f item_func_alloc,
    vxge_hal_mempool_item_f item_func_free,
    void *userdata)
{
	vxge_hal_status_e status;
	u32 memblocks_to_allocate;
	vxge_hal_mempool_t *mempool;
	__hal_device_t *hldev;
	u32 allocated;

	vxge_assert(devh != NULL);

	hldev = (__hal_device_t *) devh;

	vxge_hal_trace_log_mm("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mm(
	    "devh = 0x"VXGE_OS_STXFMT", memblock_size = %d, item_size = %d, "
	    "items_priv_size = %d, items_initial = %d, items_max = %d, "
	    "item_func_alloc = 0x"VXGE_OS_STXFMT", "
	    "item_func_free = 0x"VXGE_OS_STXFMT", "
	    "userdata = 0x"VXGE_OS_STXFMT, (ptr_t) devh,
	    memblock_size, item_size, items_priv_size,
	    items_initial, items_max, (ptr_t) item_func_alloc,
	    (ptr_t) item_func_free, (ptr_t) userdata);

	if (memblock_size < item_size) {
		vxge_hal_err_log_mm(
		    "memblock_size %d < item_size %d: misconfiguration",
		    memblock_size, item_size);
		vxge_hal_trace_log_mm("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_FAIL);
		return (NULL);
	}

	mempool = (vxge_hal_mempool_t *) vxge_os_malloc(
	    ((__hal_device_t *) devh)->header.pdev, sizeof(vxge_hal_mempool_t));
	if (mempool == NULL) {
		vxge_hal_trace_log_mm("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_OUT_OF_MEMORY);
		return (NULL);
	}
	vxge_os_memzero(mempool, sizeof(vxge_hal_mempool_t));

	mempool->devh = devh;
	mempool->memblock_size = memblock_size;
	mempool->items_max = items_max;
	mempool->items_initial = items_initial;
	mempool->item_size = item_size;
	mempool->items_priv_size = items_priv_size;
	mempool->item_func_alloc = item_func_alloc;
	mempool->item_func_free = item_func_free;
	mempool->userdata = userdata;

	mempool->memblocks_allocated = 0;

	if (memblock_size != VXGE_OS_HOST_PAGE_SIZE)
		mempool->dma_flags = VXGE_OS_DMA_CACHELINE_ALIGNED;

#if defined(VXGE_HAL_DMA_CONSISTENT)
	mempool->dma_flags |= VXGE_OS_DMA_CONSISTENT;
#else
	mempool->dma_flags |= VXGE_OS_DMA_STREAMING;
#endif

	mempool->items_per_memblock = memblock_size / item_size;

	mempool->memblocks_max = (items_max + mempool->items_per_memblock - 1) /
	    mempool->items_per_memblock;

	/* allocate array of memblocks */
	mempool->memblocks_arr = (void **)vxge_os_malloc(
	    ((__hal_device_t *) mempool->devh)->header.pdev,
	    sizeof(void *) * mempool->memblocks_max);
	if (mempool->memblocks_arr == NULL) {
		vxge_hal_mempool_destroy(mempool);
		vxge_hal_trace_log_mm("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_OUT_OF_MEMORY);
		return (NULL);
	}
	vxge_os_memzero(mempool->memblocks_arr,
	    sizeof(void *) * mempool->memblocks_max);

	/* allocate array of private parts of items per memblocks */
	mempool->memblocks_priv_arr = (void **)vxge_os_malloc(
	    ((__hal_device_t *) mempool->devh)->header.pdev,
	    sizeof(void *) * mempool->memblocks_max);
	if (mempool->memblocks_priv_arr == NULL) {
		vxge_hal_mempool_destroy(mempool);
		vxge_hal_trace_log_mm("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_OUT_OF_MEMORY);
		return (NULL);
	}
	vxge_os_memzero(mempool->memblocks_priv_arr,
	    sizeof(void *) * mempool->memblocks_max);

	/* allocate array of memblocks DMA objects */
	mempool->memblocks_dma_arr =
	    (vxge_hal_mempool_dma_t *) vxge_os_malloc(
	    ((__hal_device_t *) mempool->devh)->header.pdev,
	    sizeof(vxge_hal_mempool_dma_t) * mempool->memblocks_max);

	if (mempool->memblocks_dma_arr == NULL) {
		vxge_hal_mempool_destroy(mempool);
		vxge_hal_trace_log_mm("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_OUT_OF_MEMORY);
		return (NULL);
	}
	vxge_os_memzero(mempool->memblocks_dma_arr,
	    sizeof(vxge_hal_mempool_dma_t) * mempool->memblocks_max);

	/* allocate hash array of items */
	mempool->items_arr = (void **)vxge_os_malloc(
	    ((__hal_device_t *) mempool->devh)->header.pdev,
	    sizeof(void *) * mempool->items_max);
	if (mempool->items_arr == NULL) {
		vxge_hal_mempool_destroy(mempool);
		vxge_hal_trace_log_mm("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_OUT_OF_MEMORY);
		return (NULL);
	}
	vxge_os_memzero(mempool->items_arr,
	    sizeof(void *) * mempool->items_max);

	mempool->shadow_items_arr = (void **)vxge_os_malloc(
	    ((__hal_device_t *) mempool->devh)->header.pdev,
	    sizeof(void *) * mempool->items_max);
	if (mempool->shadow_items_arr == NULL) {
		vxge_hal_mempool_destroy(mempool);
		vxge_hal_trace_log_mm("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_OUT_OF_MEMORY);
		return (NULL);
	}
	vxge_os_memzero(mempool->shadow_items_arr,
	    sizeof(void *) * mempool->items_max);

	/* calculate initial number of memblocks */
	memblocks_to_allocate = (mempool->items_initial +
	    mempool->items_per_memblock - 1) /
	    mempool->items_per_memblock;

	vxge_hal_info_log_mm("allocating %d memblocks, "
	    "%d items per memblock", memblocks_to_allocate,
	    mempool->items_per_memblock);

	/* pre-allocate the mempool */
	status = __hal_mempool_grow(mempool, memblocks_to_allocate, &allocated);
	vxge_os_memcpy(mempool->shadow_items_arr, mempool->items_arr,
	    sizeof(void *) * mempool->items_max);
	if (status != VXGE_HAL_OK) {
		vxge_hal_mempool_destroy(mempool);
		vxge_hal_trace_log_mm("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_OUT_OF_MEMORY);
		return (NULL);
	}

	vxge_hal_info_log_mm(
	    "total: allocated %dk of DMA-capable memory",
	    mempool->memblock_size * allocated / 1024);

	vxge_hal_trace_log_mm("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);

	return (mempool);
}

/*
 * vxge_hal_mempool_destroy
 */
void
vxge_hal_mempool_destroy(
    vxge_hal_mempool_t *mempool)
{
	u32 i, j, item_index;
	__hal_device_t *hldev;

	vxge_assert(mempool != NULL);

	hldev = (__hal_device_t *) mempool->devh;

	vxge_hal_trace_log_mm("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mm("mempool = 0x"VXGE_OS_STXFMT,
	    (ptr_t) mempool);

	for (i = 0; i < mempool->memblocks_allocated; i++) {
		vxge_hal_mempool_dma_t *dma_object;

		vxge_assert(mempool->memblocks_arr[i]);
		vxge_assert(mempool->memblocks_dma_arr + i);

		dma_object = mempool->memblocks_dma_arr + i;

		for (j = 0; j < mempool->items_per_memblock; j++) {
			item_index = i * mempool->items_per_memblock + j;

			/* to skip last partially filled(if any) memblock */
			if (item_index >= mempool->items_current)
				break;

			/* let caller to do more job on each item */
			if (mempool->item_func_free != NULL) {

				mempool->item_func_free(mempool,
				    mempool->memblocks_arr[i],
				    i, dma_object,
				    mempool->shadow_items_arr[item_index],
				    item_index, /* unused */ -1,
				    mempool->userdata);
			}
		}

		vxge_os_free(hldev->header.pdev,
		    mempool->memblocks_priv_arr[i],
		    mempool->items_priv_size * mempool->items_per_memblock);

		__hal_blockpool_free(hldev,
		    mempool->memblocks_arr[i],
		    mempool->memblock_size,
		    &dma_object->addr,
		    &dma_object->handle,
		    &dma_object->acc_handle);
	}

	if (mempool->items_arr) {
		vxge_os_free(hldev->header.pdev,
		    mempool->items_arr, sizeof(void *) * mempool->items_max);
	}

	if (mempool->shadow_items_arr) {
		vxge_os_free(hldev->header.pdev,
		    mempool->shadow_items_arr,
		    sizeof(void *) * mempool->items_max);
	}

	if (mempool->memblocks_dma_arr) {
		vxge_os_free(hldev->header.pdev,
		    mempool->memblocks_dma_arr,
		    sizeof(vxge_hal_mempool_dma_t) *
		    mempool->memblocks_max);
	}

	if (mempool->memblocks_priv_arr) {
		vxge_os_free(hldev->header.pdev,
		    mempool->memblocks_priv_arr,
		    sizeof(void *) * mempool->memblocks_max);
	}

	if (mempool->memblocks_arr) {
		vxge_os_free(hldev->header.pdev,
		    mempool->memblocks_arr,
		    sizeof(void *) * mempool->memblocks_max);
	}

	vxge_os_free(hldev->header.pdev,
	    mempool, sizeof(vxge_hal_mempool_t));

	vxge_hal_trace_log_mm("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_check_alignment - Check buffer alignment	and	calculate the
 * "misaligned"	portion.
 * @dma_pointer: DMA address of	the	buffer.
 * @size: Buffer size, in bytes.
 * @alignment: Alignment "granularity" (see	below),	in bytes.
 * @copy_size: Maximum number of bytes to "extract"	from the buffer
 * (in order to	spost it as	a separate scatter-gather entry). See below.
 *
 * Check buffer	alignment and calculate	"misaligned" portion, if exists.
 * The buffer is considered	aligned	if its address is multiple of
 * the specified @alignment. If	this is	the	case,
 * vxge_hal_check_alignment() returns zero.
 * Otherwise, vxge_hal_check_alignment()	uses the last argument,
 * @copy_size,
 * to calculate	the	size to	"extract" from the buffer. The @copy_size
 * may or may not be equal @alignment. The difference between these	two
 * arguments is	that the @alignment is used to make the decision: aligned
 * or not aligned. While the @copy_size	is used	to calculate the portion
 * of the buffer to "extract", i.e.	to post	as a separate entry in the
 * transmit descriptor.	For example, the combination
 * @alignment = 8 and @copy_size = 64 will work	okay on	AMD Opteron boxes.
 *
 * Note: @copy_size should be a	multiple of @alignment.	In many	practical
 * cases @copy_size and	@alignment will	probably be equal.
 *
 * See also: vxge_hal_fifo_txdl_buffer_set_aligned().
 */
u32
vxge_hal_check_alignment(
    dma_addr_t dma_pointer,
    u32 size,
    u32 alignment,
    u32 copy_size)
{
	u32 misaligned_size;

	misaligned_size = (int)(dma_pointer & (alignment - 1));
	if (!misaligned_size) {
		return (0);
	}

	if (size > copy_size) {
		misaligned_size = (int)(dma_pointer & (copy_size - 1));
		misaligned_size = copy_size - misaligned_size;
	} else {
		misaligned_size = size;
	}

	return (misaligned_size);
}
