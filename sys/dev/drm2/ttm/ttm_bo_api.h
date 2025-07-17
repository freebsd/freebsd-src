/**************************************************************************
 *
 * Copyright (c) 2006-2009 VMware, Inc., Palo Alto, CA., USA
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
/*
 * Authors: Thomas Hellstrom <thellstrom-at-vmware-dot-com>
 */

#ifndef _TTM_BO_API_H_
#define _TTM_BO_API_H_

#include <dev/drm2/drmP.h>

struct ttm_bo_device;

struct drm_mm_node;


/**
 * struct ttm_placement
 *
 * @fpfn:		first valid page frame number to put the object
 * @lpfn:		last valid page frame number to put the object
 * @num_placement:	number of preferred placements
 * @placement:		preferred placements
 * @num_busy_placement:	number of preferred placements when need to evict buffer
 * @busy_placement:	preferred placements when need to evict buffer
 *
 * Structure indicating the placement you request for an object.
 */
struct ttm_placement {
	unsigned	fpfn;
	unsigned	lpfn;
	unsigned	num_placement;
	const uint32_t	*placement;
	unsigned	num_busy_placement;
	const uint32_t	*busy_placement;
};

/**
 * struct ttm_bus_placement
 *
 * @addr:		mapped virtual address
 * @base:		bus base address
 * @is_iomem:		is this io memory ?
 * @size:		size in byte
 * @offset:		offset from the base address
 * @io_reserved_vm:     The VM system has a refcount in @io_reserved_count
 * @io_reserved_count:  Refcounting the numbers of callers to ttm_mem_io_reserve
 *
 * Structure indicating the bus placement of an object.
 */
struct ttm_bus_placement {
	void		*addr;
	unsigned long	base;
	unsigned long	size;
	unsigned long	offset;
	bool		is_iomem;
	bool		io_reserved_vm;
	uint64_t        io_reserved_count;
};


/**
 * struct ttm_mem_reg
 *
 * @mm_node: Memory manager node.
 * @size: Requested size of memory region.
 * @num_pages: Actual size of memory region in pages.
 * @page_alignment: Page alignment.
 * @placement: Placement flags.
 * @bus: Placement on io bus accessible to the CPU
 *
 * Structure indicating the placement and space resources used by a
 * buffer object.
 */

struct ttm_mem_reg {
	void *mm_node;
	unsigned long start;
	unsigned long size;
	unsigned long num_pages;
	uint32_t page_alignment;
	uint32_t mem_type;
	uint32_t placement;
	struct ttm_bus_placement bus;
};

/**
 * enum ttm_bo_type
 *
 * @ttm_bo_type_device:	These are 'normal' buffers that can
 * be mmapped by user space. Each of these bos occupy a slot in the
 * device address space, that can be used for normal vm operations.
 *
 * @ttm_bo_type_kernel: These buffers are like ttm_bo_type_device buffers,
 * but they cannot be accessed from user-space. For kernel-only use.
 *
 * @ttm_bo_type_sg: Buffer made from dmabuf sg table shared with another
 * driver.
 */

enum ttm_bo_type {
	ttm_bo_type_device,
	ttm_bo_type_kernel,
	ttm_bo_type_sg
};

struct ttm_tt;

/**
 * struct ttm_buffer_object
 *
 * @bdev: Pointer to the buffer object device structure.
 * @type: The bo type.
 * @destroy: Destruction function. If NULL, kfree is used.
 * @num_pages: Actual number of pages.
 * @addr_space_offset: Address space offset.
 * @acc_size: Accounted size for this object.
 * @kref: Reference count of this buffer object. When this refcount reaches
 * zero, the object is put on the delayed delete list.
 * @list_kref: List reference count of this buffer object. This member is
 * used to avoid destruction while the buffer object is still on a list.
 * Lru lists may keep one refcount, the delayed delete list, and kref != 0
 * keeps one refcount. When this refcount reaches zero,
 * the object is destroyed.
 * @event_queue: Queue for processes waiting on buffer object status change.
 * @mem: structure describing current placement.
 * @persistent_swap_storage: Usually the swap storage is deleted for buffers
 * pinned in physical memory. If this behaviour is not desired, this member
 * holds a pointer to a persistent shmem object.
 * @ttm: TTM structure holding system pages.
 * @evicted: Whether the object was evicted without user-space knowing.
 * @cpu_writes: For synchronization. Number of cpu writers.
 * @lru: List head for the lru list.
 * @ddestroy: List head for the delayed destroy list.
 * @swap: List head for swap LRU list.
 * @val_seq: Sequence of the validation holding the @reserved lock.
 * Used to avoid starvation when many processes compete to validate the
 * buffer. This member is protected by the bo_device::lru_lock.
 * @seq_valid: The value of @val_seq is valid. This value is protected by
 * the bo_device::lru_lock.
 * @reserved: Deadlock-free lock used for synchronization state transitions.
 * @sync_obj: Pointer to a synchronization object.
 * @priv_flags: Flags describing buffer object internal state.
 * @vm_rb: Rb node for the vm rb tree.
 * @vm_node: Address space manager node.
 * @offset: The current GPU offset, which can have different meanings
 * depending on the memory type. For SYSTEM type memory, it should be 0.
 * @cur_placement: Hint of current placement.
 *
 * Base class for TTM buffer object, that deals with data placement and CPU
 * mappings. GPU mappings are really up to the driver, but for simpler GPUs
 * the driver can usually use the placement offset @offset directly as the
 * GPU virtual address. For drivers implementing multiple
 * GPU memory manager contexts, the driver should manage the address space
 * in these contexts separately and use these objects to get the correct
 * placement and caching for these GPU maps. This makes it possible to use
 * these objects for even quite elaborate memory management schemes.
 * The destroy member, the API visibility of this object makes it possible
 * to derive driver specific types.
 */

struct ttm_buffer_object {
	/**
	 * Members constant at init.
	 */

	struct ttm_bo_global *glob;
	struct ttm_bo_device *bdev;
	enum ttm_bo_type type;
	void (*destroy) (struct ttm_buffer_object *);
	unsigned long num_pages;
	uint64_t addr_space_offset;
	size_t acc_size;

	/**
	* Members not needing protection.
	*/

	u_int kref;
	u_int list_kref;
	/* wait_queue_head_t event_queue; */

	/**
	 * Members protected by the bo::reserved lock.
	 */

	struct ttm_mem_reg mem;
	struct vm_object *persistent_swap_storage;
	struct ttm_tt *ttm;
	bool evicted;

	/**
	 * Members protected by the bo::reserved lock only when written to.
	 */

	atomic_t cpu_writers;

	/**
	 * Members protected by the bdev::lru_lock.
	 */

	struct list_head lru;
	struct list_head ddestroy;
	struct list_head swap;
	struct list_head io_reserve_lru;
	uint32_t val_seq;
	bool seq_valid;

	/**
	 * Members protected by the bdev::lru_lock
	 * only when written to.
	 */

	atomic_t reserved;

	/**
	 * Members protected by struct buffer_object_device::fence_lock
	 * In addition, setting sync_obj to anything else
	 * than NULL requires bo::reserved to be held. This allows for
	 * checking NULL while reserved but not holding the mentioned lock.
	 */

	void *sync_obj;
	unsigned long priv_flags;

	/**
	 * Members protected by the bdev::vm_lock
	 */

	RB_ENTRY(ttm_buffer_object) vm_rb;
	struct drm_mm_node *vm_node;


	/**
	 * Special members that are protected by the reserve lock
	 * and the bo::lock when written to. Can be read with
	 * either of these locks held.
	 */

	unsigned long offset;
	uint32_t cur_placement;

	struct sg_table *sg;
};

/**
 * struct ttm_bo_kmap_obj
 *
 * @virtual: The current kernel virtual address.
 * @page: The page when kmap'ing a single page.
 * @bo_kmap_type: Type of bo_kmap.
 *
 * Object describing a kernel mapping. Since a TTM bo may be located
 * in various memory types with various caching policies, the
 * mapping can either be an ioremap, a vmap, a kmap or part of a
 * premapped region.
 */

#define TTM_BO_MAP_IOMEM_MASK 0x80
struct ttm_bo_kmap_obj {
	void *virtual;
	struct vm_page *page;
	struct sf_buf *sf;
	int num_pages;
	unsigned long size;
	enum {
		ttm_bo_map_iomap        = 1 | TTM_BO_MAP_IOMEM_MASK,
		ttm_bo_map_vmap         = 2,
		ttm_bo_map_kmap         = 3,
		ttm_bo_map_premapped    = 4 | TTM_BO_MAP_IOMEM_MASK,
	} bo_kmap_type;
	struct ttm_buffer_object *bo;
};

/**
 * ttm_bo_reference - reference a struct ttm_buffer_object
 *
 * @bo: The buffer object.
 *
 * Returns a refcounted pointer to a buffer object.
 */

static inline struct ttm_buffer_object *
ttm_bo_reference(struct ttm_buffer_object *bo)
{
	refcount_acquire(&bo->kref);
	return bo;
}

/**
 * ttm_bo_wait - wait for buffer idle.
 *
 * @bo:  The buffer object.
 * @interruptible:  Use interruptible wait.
 * @no_wait:  Return immediately if buffer is busy.
 *
 * This function must be called with the bo::mutex held, and makes
 * sure any previous rendering to the buffer is completed.
 * Note: It might be necessary to block validations before the
 * wait by reserving the buffer.
 * Returns -EBUSY if no_wait is true and the buffer is busy.
 * Returns -ERESTARTSYS if interrupted by a signal.
 */
extern int ttm_bo_wait(struct ttm_buffer_object *bo, bool lazy,
		       bool interruptible, bool no_wait);
/**
 * ttm_bo_validate
 *
 * @bo: The buffer object.
 * @placement: Proposed placement for the buffer object.
 * @interruptible: Sleep interruptible if sleeping.
 * @no_wait_gpu: Return immediately if the GPU is busy.
 *
 * Changes placement and caching policy of the buffer object
 * according proposed placement.
 * Returns
 * -EINVAL on invalid proposed placement.
 * -ENOMEM on out-of-memory condition.
 * -EBUSY if no_wait is true and buffer busy.
 * -ERESTARTSYS if interrupted by a signal.
 */
extern int ttm_bo_validate(struct ttm_buffer_object *bo,
				struct ttm_placement *placement,
				bool interruptible,
				bool no_wait_gpu);

/**
 * ttm_bo_unref
 *
 * @bo: The buffer object.
 *
 * Unreference and clear a pointer to a buffer object.
 */
extern void ttm_bo_unref(struct ttm_buffer_object **bo);


/**
 * ttm_bo_list_ref_sub
 *
 * @bo: The buffer object.
 * @count: The number of references with which to decrease @bo::list_kref;
 * @never_free: The refcount should not reach zero with this operation.
 *
 * Release @count lru list references to this buffer object.
 */
extern void ttm_bo_list_ref_sub(struct ttm_buffer_object *bo, int count,
				bool never_free);

/**
 * ttm_bo_add_to_lru
 *
 * @bo: The buffer object.
 *
 * Add this bo to the relevant mem type lru and, if it's backed by
 * system pages (ttms) to the swap list.
 * This function must be called with struct ttm_bo_global::lru_lock held, and
 * is typically called immediately prior to unreserving a bo.
 */
extern void ttm_bo_add_to_lru(struct ttm_buffer_object *bo);

/**
 * ttm_bo_del_from_lru
 *
 * @bo: The buffer object.
 *
 * Remove this bo from all lru lists used to lookup and reserve an object.
 * This function must be called with struct ttm_bo_global::lru_lock held,
 * and is usually called just immediately after the bo has been reserved to
 * avoid recursive reservation from lru lists.
 */
extern int ttm_bo_del_from_lru(struct ttm_buffer_object *bo);


/**
 * ttm_bo_lock_delayed_workqueue
 *
 * Prevent the delayed workqueue from running.
 * Returns
 * True if the workqueue was queued at the time
 */
extern int ttm_bo_lock_delayed_workqueue(struct ttm_bo_device *bdev);

/**
 * ttm_bo_unlock_delayed_workqueue
 *
 * Allows the delayed workqueue to run.
 */
extern void ttm_bo_unlock_delayed_workqueue(struct ttm_bo_device *bdev,
					    int resched);

/**
 * ttm_bo_synccpu_write_grab
 *
 * @bo: The buffer object:
 * @no_wait: Return immediately if buffer is busy.
 *
 * Synchronizes a buffer object for CPU RW access. This means
 * command submission that affects the buffer will return -EBUSY
 * until ttm_bo_synccpu_write_release is called.
 *
 * Returns
 * -EBUSY if the buffer is busy and no_wait is true.
 * -ERESTARTSYS if interrupted by a signal.
 */
extern int
ttm_bo_synccpu_write_grab(struct ttm_buffer_object *bo, bool no_wait);

/**
 * ttm_bo_synccpu_write_release:
 *
 * @bo : The buffer object.
 *
 * Releases a synccpu lock.
 */
extern void ttm_bo_synccpu_write_release(struct ttm_buffer_object *bo);

/**
 * ttm_bo_acc_size
 *
 * @bdev: Pointer to a ttm_bo_device struct.
 * @bo_size: size of the buffer object in byte.
 * @struct_size: size of the structure holding buffer object datas
 *
 * Returns size to account for a buffer object
 */
size_t ttm_bo_acc_size(struct ttm_bo_device *bdev,
		       unsigned long bo_size,
		       unsigned struct_size);
size_t ttm_bo_dma_acc_size(struct ttm_bo_device *bdev,
			   unsigned long bo_size,
			   unsigned struct_size);

/**
 * ttm_bo_init
 *
 * @bdev: Pointer to a ttm_bo_device struct.
 * @bo: Pointer to a ttm_buffer_object to be initialized.
 * @size: Requested size of buffer object.
 * @type: Requested type of buffer object.
 * @flags: Initial placement flags.
 * @page_alignment: Data alignment in pages.
 * @interruptible: If needing to sleep to wait for GPU resources,
 * sleep interruptible.
 * @persistent_swap_storage: Usually the swap storage is deleted for buffers
 * pinned in physical memory. If this behaviour is not desired, this member
 * holds a pointer to a persistent shmem object. Typically, this would
 * point to the shmem object backing a GEM object if TTM is used to back a
 * GEM user interface.
 * @acc_size: Accounted size for this object.
 * @destroy: Destroy function. Use NULL for kfree().
 *
 * This function initializes a pre-allocated struct ttm_buffer_object.
 * As this object may be part of a larger structure, this function,
 * together with the @destroy function,
 * enables driver-specific objects derived from a ttm_buffer_object.
 * On successful return, the object kref and list_kref are set to 1.
 * If a failure occurs, the function will call the @destroy function, or
 * kfree() if @destroy is NULL. Thus, after a failure, dereferencing @bo is
 * illegal and will likely cause memory corruption.
 *
 * Returns
 * -ENOMEM: Out of memory.
 * -EINVAL: Invalid placement flags.
 * -ERESTARTSYS: Interrupted by signal while sleeping waiting for resources.
 */

extern int ttm_bo_init(struct ttm_bo_device *bdev,
			struct ttm_buffer_object *bo,
			unsigned long size,
			enum ttm_bo_type type,
			struct ttm_placement *placement,
			uint32_t page_alignment,
			bool interrubtible,
			struct vm_object *persistent_swap_storage,
			size_t acc_size,
			struct sg_table *sg,
			void (*destroy) (struct ttm_buffer_object *));

/**
 * ttm_bo_synccpu_object_init
 *
 * @bdev: Pointer to a ttm_bo_device struct.
 * @bo: Pointer to a ttm_buffer_object to be initialized.
 * @size: Requested size of buffer object.
 * @type: Requested type of buffer object.
 * @flags: Initial placement flags.
 * @page_alignment: Data alignment in pages.
 * @interruptible: If needing to sleep while waiting for GPU resources,
 * sleep interruptible.
 * @persistent_swap_storage: Usually the swap storage is deleted for buffers
 * pinned in physical memory. If this behaviour is not desired, this member
 * holds a pointer to a persistent shmem object. Typically, this would
 * point to the shmem object backing a GEM object if TTM is used to back a
 * GEM user interface.
 * @p_bo: On successful completion *p_bo points to the created object.
 *
 * This function allocates a ttm_buffer_object, and then calls ttm_bo_init
 * on that object. The destroy function is set to kfree().
 * Returns
 * -ENOMEM: Out of memory.
 * -EINVAL: Invalid placement flags.
 * -ERESTARTSYS: Interrupted by signal while waiting for resources.
 */

extern int ttm_bo_create(struct ttm_bo_device *bdev,
				unsigned long size,
				enum ttm_bo_type type,
				struct ttm_placement *placement,
				uint32_t page_alignment,
				bool interruptible,
				struct vm_object *persistent_swap_storage,
				struct ttm_buffer_object **p_bo);

/**
 * ttm_bo_check_placement
 *
 * @bo:		the buffer object.
 * @placement:	placements
 *
 * Performs minimal validity checking on an intended change of
 * placement flags.
 * Returns
 * -EINVAL: Intended change is invalid or not allowed.
 */
extern int ttm_bo_check_placement(struct ttm_buffer_object *bo,
					struct ttm_placement *placement);

/**
 * ttm_bo_init_mm
 *
 * @bdev: Pointer to a ttm_bo_device struct.
 * @mem_type: The memory type.
 * @p_size: size managed area in pages.
 *
 * Initialize a manager for a given memory type.
 * Note: if part of driver firstopen, it must be protected from a
 * potentially racing lastclose.
 * Returns:
 * -EINVAL: invalid size or memory type.
 * -ENOMEM: Not enough memory.
 * May also return driver-specified errors.
 */

extern int ttm_bo_init_mm(struct ttm_bo_device *bdev, unsigned type,
				unsigned long p_size);
/**
 * ttm_bo_clean_mm
 *
 * @bdev: Pointer to a ttm_bo_device struct.
 * @mem_type: The memory type.
 *
 * Take down a manager for a given memory type after first walking
 * the LRU list to evict any buffers left alive.
 *
 * Normally, this function is part of lastclose() or unload(), and at that
 * point there shouldn't be any buffers left created by user-space, since
 * there should've been removed by the file descriptor release() method.
 * However, before this function is run, make sure to signal all sync objects,
 * and verify that the delayed delete queue is empty. The driver must also
 * make sure that there are no NO_EVICT buffers present in this memory type
 * when the call is made.
 *
 * If this function is part of a VT switch, the caller must make sure that
 * there are no appications currently validating buffers before this
 * function is called. The caller can do that by first taking the
 * struct ttm_bo_device::ttm_lock in write mode.
 *
 * Returns:
 * -EINVAL: invalid or uninitialized memory type.
 * -EBUSY: There are still buffers left in this memory type.
 */

extern int ttm_bo_clean_mm(struct ttm_bo_device *bdev, unsigned mem_type);

/**
 * ttm_bo_evict_mm
 *
 * @bdev: Pointer to a ttm_bo_device struct.
 * @mem_type: The memory type.
 *
 * Evicts all buffers on the lru list of the memory type.
 * This is normally part of a VT switch or an
 * out-of-memory-space-due-to-fragmentation handler.
 * The caller must make sure that there are no other processes
 * currently validating buffers, and can do that by taking the
 * struct ttm_bo_device::ttm_lock in write mode.
 *
 * Returns:
 * -EINVAL: Invalid or uninitialized memory type.
 * -ERESTARTSYS: The call was interrupted by a signal while waiting to
 * evict a buffer.
 */

extern int ttm_bo_evict_mm(struct ttm_bo_device *bdev, unsigned mem_type);

/**
 * ttm_kmap_obj_virtual
 *
 * @map: A struct ttm_bo_kmap_obj returned from ttm_bo_kmap.
 * @is_iomem: Pointer to an integer that on return indicates 1 if the
 * virtual map is io memory, 0 if normal memory.
 *
 * Returns the virtual address of a buffer object area mapped by ttm_bo_kmap.
 * If *is_iomem is 1 on return, the virtual address points to an io memory area,
 * that should strictly be accessed by the iowriteXX() and similar functions.
 */

static inline void *ttm_kmap_obj_virtual(struct ttm_bo_kmap_obj *map,
					 bool *is_iomem)
{
	*is_iomem = !!(map->bo_kmap_type & TTM_BO_MAP_IOMEM_MASK);
	return map->virtual;
}

/**
 * ttm_bo_kmap
 *
 * @bo: The buffer object.
 * @start_page: The first page to map.
 * @num_pages: Number of pages to map.
 * @map: pointer to a struct ttm_bo_kmap_obj representing the map.
 *
 * Sets up a kernel virtual mapping, using ioremap, vmap or kmap to the
 * data in the buffer object. The ttm_kmap_obj_virtual function can then be
 * used to obtain a virtual address to the data.
 *
 * Returns
 * -ENOMEM: Out of memory.
 * -EINVAL: Invalid range.
 */

extern int ttm_bo_kmap(struct ttm_buffer_object *bo, unsigned long start_page,
		       unsigned long num_pages, struct ttm_bo_kmap_obj *map);

/**
 * ttm_bo_kunmap
 *
 * @map: Object describing the map to unmap.
 *
 * Unmaps a kernel map set up by ttm_bo_kmap.
 */

extern void ttm_bo_kunmap(struct ttm_bo_kmap_obj *map);

/**
 * ttm_fbdev_mmap - mmap fbdev memory backed by a ttm buffer object.
 *
 * @vma:       vma as input from the fbdev mmap method.
 * @bo:        The bo backing the address space. The address space will
 * have the same size as the bo, and start at offset 0.
 *
 * This function is intended to be called by the fbdev mmap method
 * if the fbdev address space is to be backed by a bo.
 */

/* XXXKIB
extern int ttm_fbdev_mmap(struct vm_area_struct *vma,
			  struct ttm_buffer_object *bo);
*/
/**
 * ttm_bo_mmap - mmap out of the ttm device address space.
 *
 * @filp:      filp as input from the mmap method.
 * @vma:       vma as input from the mmap method.
 * @bdev:      Pointer to the ttm_bo_device with the address space manager.
 *
 * This function is intended to be called by the device mmap method.
 * if the device address space is to be backed by the bo manager.
 */
/* XXXKIB
extern int ttm_bo_mmap(struct file *filp, struct vm_area_struct *vma,
		       struct ttm_bo_device *bdev);
*/
/**
 * ttm_bo_io
 *
 * @bdev:      Pointer to the struct ttm_bo_device.
 * @filp:      Pointer to the struct file attempting to read / write.
 * @wbuf:      User-space pointer to address of buffer to write. NULL on read.
 * @rbuf:      User-space pointer to address of buffer to read into.
 * Null on write.
 * @count:     Number of bytes to read / write.
 * @f_pos:     Pointer to current file position.
 * @write:     1 for read, 0 for write.
 *
 * This function implements read / write into ttm buffer objects, and is
 * intended to
 * be called from the fops::read and fops::write method.
 * Returns:
 * See man (2) write, man(2) read. In particular,
 * the function may return -ERESTARTSYS if
 * interrupted by a signal.
 */

extern ssize_t ttm_bo_io(struct ttm_bo_device *bdev, struct file *filp,
			 const char *wbuf, char *rbuf,
			 size_t count, off_t *f_pos, bool write);

extern void ttm_bo_swapout_all(struct ttm_bo_device *bdev);

/**
 * ttm_bo_is_reserved - return an indication if a ttm buffer object is reserved
 *
 * @bo:     The buffer object to check.
 *
 * This function returns an indication if a bo is reserved or not, and should
 * only be used to print an error when it is not from incorrect api usage, since
 * there's no guarantee that it is the caller that is holding the reservation.
 */
static inline bool ttm_bo_is_reserved(struct ttm_buffer_object *bo)
{
	return atomic_read(&bo->reserved);
}

#endif
