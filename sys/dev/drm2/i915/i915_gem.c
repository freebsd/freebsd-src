/*
 * Copyright © 2008 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 * Copyright (c) 2011 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Konstantin Belousov under sponsorship from
 * the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <dev/drm2/drmP.h>
#include <dev/drm2/drm.h>
#include <dev/drm2/i915/i915_drm.h>
#include <dev/drm2/i915/i915_drv.h>
#include <dev/drm2/i915/intel_drv.h>
#include <dev/drm2/i915/intel_ringbuffer.h>

#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/sf_buf.h>

#include <vm/vm.h>
#include <vm/vm_pageout.h>

#include <machine/md_var.h>

#define __user
#define __force
#define __iomem
#define	__must_check
#define	to_user_ptr(x) ((void *)(uintptr_t)(x))
#define	offset_in_page(x) ((x) & PAGE_MASK)
#define	page_to_phys(x) VM_PAGE_TO_PHYS(x)

static void i915_gem_object_flush_gtt_write_domain(struct drm_i915_gem_object *obj);
static void i915_gem_object_flush_cpu_write_domain(struct drm_i915_gem_object *obj);
static __must_check int i915_gem_object_bind_to_gtt(struct drm_i915_gem_object *obj,
						    unsigned alignment,
						    bool map_and_fenceable);
static int i915_gem_phys_pwrite(struct drm_device *dev,
				struct drm_i915_gem_object *obj,
				struct drm_i915_gem_pwrite *args,
				struct drm_file *file);

static void i915_gem_write_fence(struct drm_device *dev, int reg,
				 struct drm_i915_gem_object *obj);
static void i915_gem_object_update_fence(struct drm_i915_gem_object *obj,
					 struct drm_i915_fence_reg *fence,
					 bool enable);

static void i915_gem_lowmem(void *arg);
static void i915_gem_object_truncate(struct drm_i915_gem_object *obj);

static int i915_gem_object_get_pages_range(struct drm_i915_gem_object *obj,
    off_t start, off_t end);
static void i915_gem_object_put_pages_range(struct drm_i915_gem_object *obj,
    off_t start, off_t end);

static vm_page_t i915_gem_wire_page(vm_object_t object, vm_pindex_t pindex,
    bool *fresh);

MALLOC_DEFINE(DRM_I915_GEM, "i915gem", "Allocations from i915 gem");
long i915_gem_wired_pages_cnt;

static bool cpu_cache_is_coherent(struct drm_device *dev,
				  enum i915_cache_level level)
{
	return HAS_LLC(dev) || level != I915_CACHE_NONE;
}

static bool cpu_write_needs_clflush(struct drm_i915_gem_object *obj)
{
	if (!cpu_cache_is_coherent(obj->base.dev, obj->cache_level))
		return true;

	return obj->pin_display;
}

static inline void i915_gem_object_fence_lost(struct drm_i915_gem_object *obj)
{
	if (obj->tiling_mode)
		i915_gem_release_mmap(obj);

	/* As we do not have an associated fence register, we will force
	 * a tiling change if we ever need to acquire one.
	 */
	obj->fence_dirty = false;
	obj->fence_reg = I915_FENCE_REG_NONE;
}

/* some bookkeeping */
static void i915_gem_info_add_obj(struct drm_i915_private *dev_priv,
				  size_t size)
{
	dev_priv->mm.object_count++;
	dev_priv->mm.object_memory += size;
}

static void i915_gem_info_remove_obj(struct drm_i915_private *dev_priv,
				     size_t size)
{
	dev_priv->mm.object_count--;
	dev_priv->mm.object_memory -= size;
}

static int
i915_gem_wait_for_error(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	if (!atomic_load_acq_int(&dev_priv->mm.wedged))
		return (0);

	mtx_lock(&dev_priv->error_completion_lock);
	while (dev_priv->error_completion == 0) {
		ret = -msleep(&dev_priv->error_completion,
		    &dev_priv->error_completion_lock, PCATCH, "915wco", 0);
		if (ret == -ERESTART)
			ret = -ERESTARTSYS;
		if (ret != 0) {
			mtx_unlock(&dev_priv->error_completion_lock);
			return (ret);
		}
	}
	mtx_unlock(&dev_priv->error_completion_lock);

	if (atomic_load_acq_int(&dev_priv->mm.wedged)) {
		/* GPU is hung, bump the completion count to account for
		 * the token we just consumed so that we never hit zero and
		 * end up waiting upon a subsequent completion event that
		 * will never happen.
		 */
		mtx_lock(&dev_priv->error_completion_lock);
		dev_priv->error_completion++;
		mtx_unlock(&dev_priv->error_completion_lock);
	}
	return 0;
}

int i915_mutex_lock_interruptible(struct drm_device *dev)
{
	int ret;

	ret = i915_gem_wait_for_error(dev);
	if (ret)
		return ret;

	/*
	 * interruptible shall it be. might indeed be if dev_lock is
	 * changed to sx
	 */
	ret = -sx_xlock_sig(&dev->dev_struct_lock);
	if (ret)
		return ret;

	return 0;
}

static inline bool
i915_gem_object_is_inactive(struct drm_i915_gem_object *obj)
{
	return !obj->active;
}

int
i915_gem_init_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file)
{
	struct drm_i915_gem_init *args = data;
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret;

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		return -ENODEV;

	if (args->gtt_start >= args->gtt_end ||
	    (args->gtt_end | args->gtt_start) & (PAGE_SIZE - 1))
		return -EINVAL;

	if (mtx_initialized(&dev_priv->mm.gtt_space.unused_lock))
		return -EBUSY;

	/* GEM with user mode setting was never supported on ilk and later. */
	if (INTEL_INFO(dev)->gen >= 5)
		return -ENODEV;

	/*
	 * XXXKIB. The second-time initialization should be guarded
	 * against.
	 */
	DRM_LOCK(dev);
	ret = i915_gem_init_global_gtt(dev, args->gtt_start,
				 args->gtt_end, args->gtt_end);
	DRM_UNLOCK(dev);

	return ret;
}

int
i915_gem_get_aperture_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *file)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_gem_get_aperture *args = data;
	struct drm_i915_gem_object *obj;
	size_t pinned;

	pinned = 0;
	DRM_LOCK(dev);
	list_for_each_entry(obj, &dev_priv->mm.gtt_list, gtt_list)
		if (obj->pin_count)
			pinned += obj->gtt_space->size;
	DRM_UNLOCK(dev);

	args->aper_size = dev_priv->mm.gtt_total;
	args->aper_available_size = args->aper_size - pinned;

	return 0;
}

static int
i915_gem_create(struct drm_file *file,
		struct drm_device *dev,
		uint64_t size,
		uint32_t *handle_p)
{
	struct drm_i915_gem_object *obj;
	int ret;
	u32 handle;

	size = roundup(size, PAGE_SIZE);
	if (size == 0)
		return -EINVAL;

	/* Allocate the new object */
	obj = i915_gem_alloc_object(dev, size);
	if (obj == NULL)
		return -ENOMEM;

	ret = drm_gem_handle_create(file, &obj->base, &handle);
	if (ret) {
		drm_gem_object_release(&obj->base);
		i915_gem_info_remove_obj(dev->dev_private, obj->base.size);
		free(obj, DRM_I915_GEM);
		return ret;
	}

	/* drop reference from allocate - handle holds it now */
	drm_gem_object_unreference(&obj->base);
	CTR2(KTR_DRM, "object_create %p %x", obj, size);

	*handle_p = handle;
	return 0;
}

int
i915_gem_dumb_create(struct drm_file *file,
		     struct drm_device *dev,
		     struct drm_mode_create_dumb *args)
{
	/* have to work out size/pitch and return them */
	args->pitch = roundup2(args->width * ((args->bpp + 7) / 8), 64);
	args->size = args->pitch * args->height;
	return i915_gem_create(file, dev,
			       args->size, &args->handle);
}

int i915_gem_dumb_destroy(struct drm_file *file,
			  struct drm_device *dev,
			  uint32_t handle)
{
	return drm_gem_handle_delete(file, handle);
}

/**
 * Creates a new mm object and returns a handle to it.
 */
int
i915_gem_create_ioctl(struct drm_device *dev, void *data,
		      struct drm_file *file)
{
	struct drm_i915_gem_create *args = data;

	return i915_gem_create(file, dev,
			       args->size, &args->handle);
}

static int i915_gem_object_needs_bit17_swizzle(struct drm_i915_gem_object *obj)
{
	drm_i915_private_t *dev_priv = obj->base.dev->dev_private;

	return dev_priv->mm.bit_6_swizzle_x == I915_BIT_6_SWIZZLE_9_10_17 &&
		obj->tiling_mode != I915_TILING_NONE;
}

static inline int
__copy_to_user_inatomic(void __user *to, const void *from, unsigned n)
{
	return (copyout_nofault(from, to, n) != 0 ? n : 0);
}
static inline unsigned long
__copy_from_user_inatomic_nocache(void *to, const void __user *from,
    unsigned long n)
{

	/*
	 * XXXKIB.  Equivalent Linux function is implemented using
	 * MOVNTI for aligned moves.  For unaligned head and tail,
	 * normal move is performed.  As such, it is not incorrect, if
	 * only somewhat slower, to use normal copyin.  All uses
	 * except shmem_pwrite_fast() have the destination mapped WC.
	 */
	return ((copyin_nofault(__DECONST(void *, from), to, n) != 0 ? n : 0));
}
static inline int
fault_in_multipages_readable(const char __user *uaddr, int size)
{
	char c;
	int ret = 0;
	const char __user *end = uaddr + size - 1;

	if (unlikely(size == 0))
		return ret;

	while (uaddr <= end) {
		ret = -copyin(uaddr, &c, 1);
		if (ret != 0)
			return -EFAULT;
		uaddr += PAGE_SIZE;
	}

	/* Check whether the range spilled into the next page. */
	if (((unsigned long)uaddr & ~PAGE_MASK) ==
			((unsigned long)end & ~PAGE_MASK)) {
		ret = -copyin(end, &c, 1);
	}

	return ret;
}

static inline int
fault_in_multipages_writeable(char __user *uaddr, int size)
{
	int ret = 0;
	char __user *end = uaddr + size - 1;

	if (unlikely(size == 0))
		return ret;

	/*
	 * Writing zeroes into userspace here is OK, because we know that if
	 * the zero gets there, we'll be overwriting it.
	 */
	while (uaddr <= end) {
		ret = subyte(uaddr, 0);
		if (ret != 0)
			return -EFAULT;
		uaddr += PAGE_SIZE;
	}

	/* Check whether the range spilled into the next page. */
	if (((unsigned long)uaddr & ~PAGE_MASK) ==
			((unsigned long)end & ~PAGE_MASK))
		ret = subyte(end, 0);

	return ret;
}

static inline int
__copy_to_user_swizzled(char __user *cpu_vaddr,
			const char *gpu_vaddr, int gpu_offset,
			int length)
{
	int ret, cpu_offset = 0;

	while (length > 0) {
		int cacheline_end = roundup2(gpu_offset + 1, 64);
		int this_length = min(cacheline_end - gpu_offset, length);
		int swizzled_gpu_offset = gpu_offset ^ 64;

		ret = __copy_to_user(cpu_vaddr + cpu_offset,
				     gpu_vaddr + swizzled_gpu_offset,
				     this_length);
		if (ret)
			return ret + length;

		cpu_offset += this_length;
		gpu_offset += this_length;
		length -= this_length;
	}

	return 0;
}

static inline int
__copy_from_user_swizzled(char *gpu_vaddr, int gpu_offset,
			  const char __user *cpu_vaddr,
			  int length)
{
	int ret, cpu_offset = 0;

	while (length > 0) {
		int cacheline_end = roundup2(gpu_offset + 1, 64);
		int this_length = min(cacheline_end - gpu_offset, length);
		int swizzled_gpu_offset = gpu_offset ^ 64;

		ret = __copy_from_user(gpu_vaddr + swizzled_gpu_offset,
				       cpu_vaddr + cpu_offset,
				       this_length);
		if (ret)
			return ret + length;

		cpu_offset += this_length;
		gpu_offset += this_length;
		length -= this_length;
	}

	return 0;
}

/* Per-page copy function for the shmem pread fastpath.
 * Flushes invalid cachelines before reading the target if
 * needs_clflush is set. */
static int
shmem_pread_fast(vm_page_t page, int shmem_page_offset, int page_length,
		 char __user *user_data,
		 bool page_do_bit17_swizzling, bool needs_clflush)
{
	char *vaddr;
	struct sf_buf *sf;
	int ret;

	if (unlikely(page_do_bit17_swizzling))
		return -EINVAL;

	sched_pin();
	sf = sf_buf_alloc(page, SFB_NOWAIT | SFB_CPUPRIVATE);
	if (sf == NULL) {
		sched_unpin();
		return (-EFAULT);
	}
	vaddr = (char *)sf_buf_kva(sf);
	if (needs_clflush)
		drm_clflush_virt_range(vaddr + shmem_page_offset,
				       page_length);
	ret = __copy_to_user_inatomic(user_data,
				      vaddr + shmem_page_offset,
				      page_length);
	sf_buf_free(sf);
	sched_unpin();

	return ret ? -EFAULT : 0;
}

static void
shmem_clflush_swizzled_range(char *addr, unsigned long length,
			     bool swizzled)
{
	if (unlikely(swizzled)) {
		unsigned long start = (unsigned long) addr;
		unsigned long end = (unsigned long) addr + length;

		/* For swizzling simply ensure that we always flush both
		 * channels. Lame, but simple and it works. Swizzled
		 * pwrite/pread is far from a hotpath - current userspace
		 * doesn't use it at all. */
		start = rounddown2(start, 128);
		end = roundup2(end, 128);

		drm_clflush_virt_range((void *)start, end - start);
	} else {
		drm_clflush_virt_range(addr, length);
	}

}

/* Only difference to the fast-path function is that this can handle bit17
 * and uses non-atomic copy and kmap functions. */
static int
shmem_pread_slow(vm_page_t page, int shmem_page_offset, int page_length,
		 char __user *user_data,
		 bool page_do_bit17_swizzling, bool needs_clflush)
{
	char *vaddr;
	struct sf_buf *sf;
	int ret;

	sf = sf_buf_alloc(page, 0);
	vaddr = (char *)sf_buf_kva(sf);
	if (needs_clflush)
		shmem_clflush_swizzled_range(vaddr + shmem_page_offset,
					     page_length,
					     page_do_bit17_swizzling);

	if (page_do_bit17_swizzling)
		ret = __copy_to_user_swizzled(user_data,
					      vaddr, shmem_page_offset,
					      page_length);
	else
		ret = __copy_to_user(user_data,
				     vaddr + shmem_page_offset,
				     page_length);
	sf_buf_free(sf);

	return ret ? - EFAULT : 0;
}

static int
i915_gem_shmem_pread(struct drm_device *dev,
		     struct drm_i915_gem_object *obj,
		     struct drm_i915_gem_pread *args,
		     struct drm_file *file)
{
	char __user *user_data;
	ssize_t remain, sremain;
	off_t offset, soffset;
	int shmem_page_offset, page_length, ret = 0;
	int obj_do_bit17_swizzling, page_do_bit17_swizzling;
	int prefaulted = 0;
	int needs_clflush = 0;

	user_data = to_user_ptr(args->data_ptr);
	sremain = remain = args->size;

	obj_do_bit17_swizzling = i915_gem_object_needs_bit17_swizzle(obj);

	if (!(obj->base.read_domains & I915_GEM_DOMAIN_CPU)) {
		/* If we're not in the cpu read domain, set ourself into the gtt
		 * read domain and manually flush cachelines (if required). This
		 * optimizes for the case when the gpu will dirty the data
		 * anyway again before the next pread happens. */
		needs_clflush = !cpu_cache_is_coherent(dev, obj->cache_level);
		ret = i915_gem_object_set_to_gtt_domain(obj, false);
		if (ret)
			return ret;
	}

	soffset = offset = args->offset;
	ret = i915_gem_object_get_pages_range(obj, soffset, soffset + sremain);
	if (ret)
		return ret;

	i915_gem_object_pin_pages(obj);

	VM_OBJECT_WLOCK(obj->base.vm_obj);
	for (vm_page_t page = vm_page_find_least(obj->base.vm_obj,
	    OFF_TO_IDX(offset));; page = vm_page_next(page)) {
		VM_OBJECT_WUNLOCK(obj->base.vm_obj);

		if (remain <= 0)
			break;

		/* Operation in this page
		 *
		 * shmem_page_offset = offset within page in shmem file
		 * page_length = bytes to copy for this page
		 */
		shmem_page_offset = offset_in_page(offset);
		page_length = remain;
		if ((shmem_page_offset + page_length) > PAGE_SIZE)
			page_length = PAGE_SIZE - shmem_page_offset;

		page_do_bit17_swizzling = obj_do_bit17_swizzling &&
			(page_to_phys(page) & (1 << 17)) != 0;

		ret = shmem_pread_fast(page, shmem_page_offset, page_length,
				       user_data, page_do_bit17_swizzling,
				       needs_clflush);
		if (ret == 0)
			goto next_page;

		DRM_UNLOCK(dev);

		if (likely(!i915_prefault_disable) && !prefaulted) {
			ret = fault_in_multipages_writeable(user_data, remain);
			/* Userspace is tricking us, but we've already clobbered
			 * its pages with the prefault and promised to write the
			 * data up to the first fault. Hence ignore any errors
			 * and just continue. */
			(void)ret;
			prefaulted = 1;
		}

		ret = shmem_pread_slow(page, shmem_page_offset, page_length,
				       user_data, page_do_bit17_swizzling,
				       needs_clflush);

		DRM_LOCK(dev);

next_page:
		vm_page_reference(page);

		if (ret)
			goto out;

		remain -= page_length;
		user_data += page_length;
		offset += page_length;
		VM_OBJECT_WLOCK(obj->base.vm_obj);
	}

out:
	i915_gem_object_unpin_pages(obj);
	i915_gem_object_put_pages_range(obj, soffset, soffset + sremain);

	return ret;
}

/**
 * Reads data from the object referenced by handle.
 *
 * On error, the contents of *data are undefined.
 */
int
i915_gem_pread_ioctl(struct drm_device *dev, void *data,
		     struct drm_file *file)
{
	struct drm_i915_gem_pread *args = data;
	struct drm_i915_gem_object *obj;
	int ret = 0;

	if (args->size == 0)
		return 0;

	if (!useracc(to_user_ptr(args->data_ptr), args->size, VM_PROT_WRITE))
		return -EFAULT;

	ret = i915_mutex_lock_interruptible(dev);
	if (ret)
		return ret;

	obj = to_intel_bo(drm_gem_object_lookup(dev, file, args->handle));
	if (&obj->base == NULL) {
		ret = -ENOENT;
		goto unlock;
	}

	/* Bounds check source.  */
	if (args->offset > obj->base.size ||
	    args->size > obj->base.size - args->offset) {
		ret = -EINVAL;
		goto out;
	}

#if 1
	KIB_NOTYET();
#else
	/* prime objects have no backing filp to GEM pread/pwrite
	 * pages from.
	 */
	if (!obj->base.filp) {
		ret = -EINVAL;
		goto out;
	}
#endif

	CTR3(KTR_DRM, "pread %p %jx %jx", obj, args->offset, args->size);

	ret = i915_gem_shmem_pread(dev, obj, args, file);

out:
	drm_gem_object_unreference(&obj->base);
unlock:
	DRM_UNLOCK(dev);
	return ret;
}

/* This is the fast write path which cannot handle
 * page faults in the source data
 */

static inline int
fast_user_write(struct drm_device *dev,
		off_t page_base, int page_offset,
		char __user *user_data,
		int length)
{
	void __iomem *vaddr_atomic;
	void *vaddr;
	unsigned long unwritten;

	vaddr_atomic = pmap_mapdev_attr(dev->agp->base + page_base,
	    length, PAT_WRITE_COMBINING);
	/* We can use the cpu mem copy function because this is X86. */
	vaddr = (char __force*)vaddr_atomic + page_offset;
	unwritten = __copy_from_user_inatomic_nocache(vaddr,
						      user_data, length);
	pmap_unmapdev((vm_offset_t)vaddr_atomic, length);
	return unwritten;
}

/**
 * This is the fast pwrite path, where we copy the data directly from the
 * user into the GTT, uncached.
 */
static int
i915_gem_gtt_pwrite_fast(struct drm_device *dev,
			 struct drm_i915_gem_object *obj,
			 struct drm_i915_gem_pwrite *args,
			 struct drm_file *file)
{
	ssize_t remain;
	off_t offset, page_base;
	char __user *user_data;
	int page_offset, page_length, ret;

	ret = i915_gem_object_pin(obj, 0, true);
	/* XXXKIB ret = i915_gem_obj_ggtt_pin(obj, 0, true, true); */
	if (ret)
		goto out;

	ret = i915_gem_object_set_to_gtt_domain(obj, true);
	if (ret)
		goto out_unpin;

	ret = i915_gem_object_put_fence(obj);
	if (ret)
		goto out_unpin;

	user_data = to_user_ptr(args->data_ptr);
	remain = args->size;

	offset = obj->gtt_offset + args->offset;

	while (remain > 0) {
		/* Operation in this page
		 *
		 * page_base = page offset within aperture
		 * page_offset = offset within page
		 * page_length = bytes to copy for this page
		 */
		page_base = offset & ~PAGE_MASK;
		page_offset = offset_in_page(offset);
		page_length = remain;
		if ((page_offset + remain) > PAGE_SIZE)
			page_length = PAGE_SIZE - page_offset;

		/* If we get a fault while copying data, then (presumably) our
		 * source page isn't available.  Return the error and we'll
		 * retry in the slow path.
		 */
		if (fast_user_write(dev, page_base,
				    page_offset, user_data, page_length)) {
			ret = -EFAULT;
			goto out_unpin;
		}

		remain -= page_length;
		user_data += page_length;
		offset += page_length;
	}

out_unpin:
	i915_gem_object_unpin(obj);
out:
	return ret;
}

/* Per-page copy function for the shmem pwrite fastpath.
 * Flushes invalid cachelines before writing to the target if
 * needs_clflush_before is set and flushes out any written cachelines after
 * writing if needs_clflush is set. */
static int
shmem_pwrite_fast(vm_page_t page, int shmem_page_offset, int page_length,
		  char __user *user_data,
		  bool page_do_bit17_swizzling,
		  bool needs_clflush_before,
		  bool needs_clflush_after)
{
	char *vaddr;
	struct sf_buf *sf;
	int ret;

	if (unlikely(page_do_bit17_swizzling))
		return -EINVAL;

	sched_pin();
	sf = sf_buf_alloc(page, SFB_NOWAIT | SFB_CPUPRIVATE);
	if (sf == NULL) {
		sched_unpin();
		return (-EFAULT);
	}
	vaddr = (char *)sf_buf_kva(sf);
	if (needs_clflush_before)
		drm_clflush_virt_range(vaddr + shmem_page_offset,
				       page_length);
	ret = __copy_from_user_inatomic_nocache(vaddr + shmem_page_offset,
						user_data,
						page_length);
	if (needs_clflush_after)
		drm_clflush_virt_range(vaddr + shmem_page_offset,
				       page_length);
	sf_buf_free(sf);
	sched_unpin();

	return ret ? -EFAULT : 0;
}

/* Only difference to the fast-path function is that this can handle bit17
 * and uses non-atomic copy and kmap functions. */
static int
shmem_pwrite_slow(vm_page_t page, int shmem_page_offset, int page_length,
		  char __user *user_data,
		  bool page_do_bit17_swizzling,
		  bool needs_clflush_before,
		  bool needs_clflush_after)
{
	char *vaddr;
	struct sf_buf *sf;
	int ret;

	sf = sf_buf_alloc(page, 0);
	vaddr = (char *)sf_buf_kva(sf);
	if (unlikely(needs_clflush_before || page_do_bit17_swizzling))
		shmem_clflush_swizzled_range(vaddr + shmem_page_offset,
					     page_length,
					     page_do_bit17_swizzling);
	if (page_do_bit17_swizzling)
		ret = __copy_from_user_swizzled(vaddr, shmem_page_offset,
						user_data,
						page_length);
	else
		ret = __copy_from_user(vaddr + shmem_page_offset,
				       user_data,
				       page_length);
	if (needs_clflush_after)
		shmem_clflush_swizzled_range(vaddr + shmem_page_offset,
					     page_length,
					     page_do_bit17_swizzling);
	sf_buf_free(sf);

	return ret ? -EFAULT : 0;
}

static int
i915_gem_shmem_pwrite(struct drm_device *dev,
		      struct drm_i915_gem_object *obj,
		      struct drm_i915_gem_pwrite *args,
		      struct drm_file *file)
{
	ssize_t remain, sremain;
	off_t offset, soffset;
	char __user *user_data;
	int shmem_page_offset, page_length, ret = 0;
	int obj_do_bit17_swizzling, page_do_bit17_swizzling;
	int hit_slowpath = 0;
	int needs_clflush_after = 0;
	int needs_clflush_before = 0;

	user_data = to_user_ptr(args->data_ptr);
	sremain = remain = args->size;

	obj_do_bit17_swizzling = i915_gem_object_needs_bit17_swizzle(obj);

	if (obj->base.write_domain != I915_GEM_DOMAIN_CPU) {
		/* If we're not in the cpu write domain, set ourself into the gtt
		 * write domain and manually flush cachelines (if required). This
		 * optimizes for the case when the gpu will use the data
		 * right away and we therefore have to clflush anyway. */
		needs_clflush_after = cpu_write_needs_clflush(obj);
		ret = i915_gem_object_set_to_gtt_domain(obj, true);
		if (ret)
			return ret;
	}
	/* Same trick applies to invalidate partially written cachelines read
	 * before writing. */
	if ((obj->base.read_domains & I915_GEM_DOMAIN_CPU) == 0)
		needs_clflush_before =
			!cpu_cache_is_coherent(dev, obj->cache_level);

	soffset = offset = args->offset;
	ret = i915_gem_object_get_pages_range(obj, soffset, soffset + sremain);
	if (ret)
		return ret;

	i915_gem_object_pin_pages(obj);

	obj->dirty = 1;

	VM_OBJECT_WLOCK(obj->base.vm_obj);
	for (vm_page_t page = vm_page_find_least(obj->base.vm_obj,
	    OFF_TO_IDX(offset));; page = vm_page_next(page)) {
		VM_OBJECT_WUNLOCK(obj->base.vm_obj);
		int partial_cacheline_write;

		if (remain <= 0)
			break;

		/* Operation in this page
		 *
		 * shmem_page_offset = offset within page in shmem file
		 * page_length = bytes to copy for this page
		 */
		shmem_page_offset = offset_in_page(offset);

		page_length = remain;
		if ((shmem_page_offset + page_length) > PAGE_SIZE)
			page_length = PAGE_SIZE - shmem_page_offset;

		/* If we don't overwrite a cacheline completely we need to be
		 * careful to have up-to-date data by first clflushing. Don't
		 * overcomplicate things and flush the entire patch. */
		partial_cacheline_write = needs_clflush_before &&
			((shmem_page_offset | page_length)
				& (cpu_clflush_line_size - 1));

		page_do_bit17_swizzling = obj_do_bit17_swizzling &&
			(page_to_phys(page) & (1 << 17)) != 0;

		ret = shmem_pwrite_fast(page, shmem_page_offset, page_length,
					user_data, page_do_bit17_swizzling,
					partial_cacheline_write,
					needs_clflush_after);
		if (ret == 0)
			goto next_page;

		hit_slowpath = 1;
		DRM_UNLOCK(dev);
		ret = shmem_pwrite_slow(page, shmem_page_offset, page_length,
					user_data, page_do_bit17_swizzling,
					partial_cacheline_write,
					needs_clflush_after);

		DRM_LOCK(dev);

next_page:
		vm_page_dirty(page);
		vm_page_reference(page);

		if (ret)
			goto out;

		remain -= page_length;
		user_data += page_length;
		offset += page_length;
		VM_OBJECT_WLOCK(obj->base.vm_obj);
	}

out:
	i915_gem_object_unpin_pages(obj);
	i915_gem_object_put_pages_range(obj, soffset, soffset + sremain);

	if (hit_slowpath) {
		/*
		 * Fixup: Flush cpu caches in case we didn't flush the dirty
		 * cachelines in-line while writing and the object moved
		 * out of the cpu write domain while we've dropped the lock.
		 */
		if (!needs_clflush_after &&
		    obj->base.write_domain != I915_GEM_DOMAIN_CPU) {
			i915_gem_clflush_object(obj);
			i915_gem_chipset_flush(dev);
		}
	}

	if (needs_clflush_after)
		i915_gem_chipset_flush(dev);

	return ret;
}

/**
 * Writes data to the object referenced by handle.
 *
 * On error, the contents of the buffer that were to be modified are undefined.
 */
int
i915_gem_pwrite_ioctl(struct drm_device *dev, void *data,
		      struct drm_file *file)
{
	struct drm_i915_gem_pwrite *args = data;
	struct drm_i915_gem_object *obj;
	int ret;

	if (args->size == 0)
		return 0;

	if (!useracc(to_user_ptr(args->data_ptr), args->size, VM_PROT_READ))
		return -EFAULT;

	if (likely(!i915_prefault_disable)) {
		ret = fault_in_multipages_readable(to_user_ptr(args->data_ptr),
						   args->size);
		if (ret)
			return -EFAULT;
	}

	ret = i915_mutex_lock_interruptible(dev);
	if (ret)
		return ret;

	obj = to_intel_bo(drm_gem_object_lookup(dev, file, args->handle));
	if (&obj->base == NULL) {
		ret = -ENOENT;
		goto unlock;
	}

	/* Bounds check destination. */
	if (args->offset > obj->base.size ||
	    args->size > obj->base.size - args->offset) {
		ret = -EINVAL;
		goto out;
	}

#if 1
	KIB_NOTYET();
#else
	/* prime objects have no backing filp to GEM pread/pwrite
	 * pages from.
	 */
	if (!obj->base.filp) {
		ret = -EINVAL;
		goto out;
	}
#endif

	CTR3(KTR_DRM, "pwrite %p %jx %jx", obj, args->offset, args->size);

	ret = -EFAULT;
	/* We can only do the GTT pwrite on untiled buffers, as otherwise
	 * it would end up going through the fenced access, and we'll get
	 * different detiling behavior between reading and writing.
	 * pread/pwrite currently are reading and writing from the CPU
	 * perspective, requiring manual detiling by the client.
	 */
	if (obj->phys_obj) {
		ret = i915_gem_phys_pwrite(dev, obj, args, file);
		goto out;
	}

	if (obj->tiling_mode == I915_TILING_NONE &&
	    obj->base.write_domain != I915_GEM_DOMAIN_CPU &&
	    cpu_write_needs_clflush(obj)) {
		ret = i915_gem_gtt_pwrite_fast(dev, obj, args, file);
		/* Note that the gtt paths might fail with non-page-backed user
		 * pointers (e.g. gtt mappings when moving data between
		 * textures). Fallback to the shmem path in that case. */
	}

	if (ret == -EFAULT || ret == -ENOSPC)
		ret = i915_gem_shmem_pwrite(dev, obj, args, file);

out:
	drm_gem_object_unreference(&obj->base);
unlock:
	DRM_UNLOCK(dev);
	return ret;
}

static int
i915_gem_check_wedge(struct drm_i915_private *dev_priv)
{
	DRM_LOCK_ASSERT(dev_priv->dev);

	if (atomic_load_acq_int(&dev_priv->mm.wedged) != 0) {
		bool recovery_complete;

		/* Give the error handler a chance to run. */
		mtx_lock(&dev_priv->error_completion_lock);
		recovery_complete = (&dev_priv->error_completion) > 0;
		mtx_unlock(&dev_priv->error_completion_lock);

		return (recovery_complete ? -EIO : -EAGAIN);
	}

	return 0;
}

/*
 * Compare seqno against outstanding lazy request. Emit a request if they are
 * equal.
 */
static int
i915_gem_check_olr(struct intel_ring_buffer *ring, u32 seqno)
{
	int ret;

	DRM_LOCK_ASSERT(ring->dev);

	ret = 0;
	if (seqno == ring->outstanding_lazy_request) {
		struct drm_i915_gem_request *request;

		request = malloc(sizeof(*request), DRM_I915_GEM,
		    M_WAITOK | M_ZERO);

		ret = i915_add_request(ring, NULL, request);
		if (ret != 0) {
			free(request, DRM_I915_GEM);
			return ret;
		}

		MPASS(seqno == request->seqno);
	}
	return ret;
}

static int __wait_seqno(struct intel_ring_buffer *ring, u32 seqno,
			bool interruptible)
{
	drm_i915_private_t *dev_priv = ring->dev->dev_private;
	int ret = 0, flags;

	if (i915_seqno_passed(ring->get_seqno(ring), seqno))
		return 0;

	CTR2(KTR_DRM, "request_wait_begin %s %d", ring->name, seqno);

	mtx_lock(&dev_priv->irq_lock);
	if (!ring->irq_get(ring)) {
		mtx_unlock(&dev_priv->irq_lock);
		return -ENODEV;
	}

	flags = interruptible ? PCATCH : 0;
	while (!i915_seqno_passed(ring->get_seqno(ring), seqno)
	    && !atomic_load_acq_int(&dev_priv->mm.wedged) &&
	    ret == 0) {
		ret = -msleep(ring, &dev_priv->irq_lock, flags, "915gwr", 0);
		if (ret == -ERESTART)
			ret = -ERESTARTSYS;
	}
	ring->irq_put(ring);
	mtx_unlock(&dev_priv->irq_lock);

	CTR3(KTR_DRM, "request_wait_end %s %d %d", ring->name, seqno, ret);

	return ret;
}

/**
 * Waits for a sequence number to be signaled, and cleans up the
 * request and object lists appropriately for that event.
 */
int
i915_wait_request(struct intel_ring_buffer *ring, uint32_t seqno)
{
	struct drm_device *dev = ring->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	KASSERT(seqno != 0, ("Zero seqno"));

	ret = i915_gem_check_wedge(dev_priv);
	if (ret)
		return ret;

	ret = i915_gem_check_olr(ring, seqno);
	if (ret)
		return ret;

	ret = __wait_seqno(ring, seqno, dev_priv->mm.interruptible);
	if (atomic_load_acq_int(&dev_priv->mm.wedged))
		ret = -EAGAIN;

	return ret;
}

/**
 * Ensures that all rendering to the object has completed and the object is
 * safe to unbind from the GTT or access from the CPU.
 */
static __must_check int
i915_gem_object_wait_rendering(struct drm_i915_gem_object *obj)
{
	int ret;

	KASSERT((obj->base.write_domain & I915_GEM_GPU_DOMAINS) == 0,
	    ("In GPU write domain"));

	CTR5(KTR_DRM, "object_wait_rendering %p %s %x %d %d", obj,
	    obj->ring != NULL ? obj->ring->name : "none", obj->gtt_offset,
	    obj->active, obj->last_rendering_seqno);
	if (obj->active) {
		ret = i915_wait_request(obj->ring, obj->last_rendering_seqno);
		if (ret != 0)
			return (ret);
		i915_gem_retire_requests_ring(obj->ring);
	}

	return 0;
}

int
i915_gem_set_domain_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file)
{
	struct drm_i915_gem_set_domain *args = data;
	struct drm_i915_gem_object *obj;
	uint32_t read_domains = args->read_domains;
	uint32_t write_domain = args->write_domain;
	int ret;

	/* Only handle setting domains to types used by the CPU. */
	if (write_domain & I915_GEM_GPU_DOMAINS)
		return -EINVAL;

	if (read_domains & I915_GEM_GPU_DOMAINS)
		return -EINVAL;

	/* Having something in the write domain implies it's in the read
	 * domain, and only that read domain.  Enforce that in the request.
	 */
	if (write_domain != 0 && read_domains != write_domain)
		return -EINVAL;

	ret = i915_mutex_lock_interruptible(dev);
	if (ret)
		return ret;

	obj = to_intel_bo(drm_gem_object_lookup(dev, file, args->handle));
	if (&obj->base == NULL) {
		ret = -ENOENT;
		goto unlock;
	}

	if (read_domains & I915_GEM_DOMAIN_GTT) {
		ret = i915_gem_object_set_to_gtt_domain(obj, write_domain != 0);

		/* Silently promote "you're not bound, there was nothing to do"
		 * to success, since the client was just asking us to
		 * make sure everything was done.
		 */
		if (ret == -EINVAL)
			ret = 0;
	} else {
		ret = i915_gem_object_set_to_cpu_domain(obj, write_domain != 0);
	}

	drm_gem_object_unreference(&obj->base);
unlock:
	DRM_UNLOCK(dev);
	return ret;
}

/**
 * Called when user space has done writes to this buffer
 */
int
i915_gem_sw_finish_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file)
{
	struct drm_i915_gem_sw_finish *args = data;
	struct drm_i915_gem_object *obj;
	int ret = 0;

	ret = i915_mutex_lock_interruptible(dev);
	if (ret)
		return ret;

	obj = to_intel_bo(drm_gem_object_lookup(dev, file, args->handle));
	if (&obj->base == NULL) {
		ret = -ENOENT;
		goto unlock;
	}

	/* Pinned buffers may be scanout, so flush the cache */
	if (obj->pin_count)
		i915_gem_object_flush_cpu_write_domain(obj);

	drm_gem_object_unreference(&obj->base);
unlock:
	DRM_UNLOCK(dev);
	return ret;
}

/**
 * Maps the contents of an object, returning the address it is mapped
 * into.
 *
 * While the mapping holds a reference on the contents of the object, it doesn't
 * imply a ref on the object itself.
 */
int
i915_gem_mmap_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file)
{
	struct drm_i915_gem_mmap *args = data;
	struct drm_gem_object *obj;
	struct proc *p;
	vm_map_t map;
	vm_offset_t addr;
	vm_size_t size;
	int error, rv;

	obj = drm_gem_object_lookup(dev, file, args->handle);
	if (obj == NULL)
		return -ENOENT;

	error = 0;
	if (args->size == 0)
		goto out;
	p = curproc;
	map = &p->p_vmspace->vm_map;
	size = round_page(args->size);
	PROC_LOCK(p);
	if (map->size + size > lim_cur_proc(p, RLIMIT_VMEM)) {
		PROC_UNLOCK(p);
		error = -ENOMEM;
		goto out;
	}
	PROC_UNLOCK(p);

	addr = 0;
	vm_object_reference(obj->vm_obj);
	rv = vm_map_find(map, obj->vm_obj, args->offset, &addr, args->size, 0,
	    VMFS_OPTIMAL_SPACE, VM_PROT_READ | VM_PROT_WRITE,
	    VM_PROT_READ | VM_PROT_WRITE, MAP_INHERIT_SHARE);
	if (rv != KERN_SUCCESS) {
		vm_object_deallocate(obj->vm_obj);
		error = -vm_mmap_to_errno(rv);
	} else {
		args->addr_ptr = (uint64_t)addr;
	}
out:
	drm_gem_object_unreference(obj);
	return (error);
}

static int
i915_gem_pager_ctor(void *handle, vm_ooffset_t size, vm_prot_t prot,
    vm_ooffset_t foff, struct ucred *cred, u_short *color)
{

	*color = 0; /* XXXKIB */
	return (0);
}

/**
 * i915_gem_fault - fault a page into the GTT
 * vma: VMA in question
 * vmf: fault info
 *
 * The fault handler is set up by drm_gem_mmap() when a object is GTT mapped
 * from userspace.  The fault handler takes care of binding the object to
 * the GTT (if needed), allocating and programming a fence register (again,
 * only if needed based on whether the old reg is still valid or the object
 * is tiled) and inserting a new PTE into the faulting process.
 *
 * Note that the faulting process may involve evicting existing objects
 * from the GTT and/or fence registers to make room.  So performance may
 * suffer if the GTT working set is large or there are few fence registers
 * left.
 */

int i915_intr_pf;

static int
i915_gem_pager_fault(vm_object_t vm_obj, vm_ooffset_t offset, int prot,
    vm_page_t *mres)
{
	struct drm_gem_object *gem_obj;
	struct drm_i915_gem_object *obj;
	struct drm_device *dev;
	drm_i915_private_t *dev_priv;
	vm_page_t page, oldpage;
	int cause, ret;
	bool write;

	gem_obj = vm_obj->handle;
	obj = to_intel_bo(gem_obj);
	dev = obj->base.dev;
	dev_priv = dev->dev_private;
#if 0
	write = (prot & VM_PROT_WRITE) != 0;
#else
	write = true;
#endif
	vm_object_pip_add(vm_obj, 1);

	/*
	 * Remove the placeholder page inserted by vm_fault() from the
	 * object before dropping the object lock. If
	 * i915_gem_release_mmap() is active in parallel on this gem
	 * object, then it owns the drm device sx and might find the
	 * placeholder already. Then, since the page is busy,
	 * i915_gem_release_mmap() sleeps waiting for the busy state
	 * of the page cleared. We will be not able to acquire drm
	 * device lock until i915_gem_release_mmap() is able to make a
	 * progress.
	 */
	if (*mres != NULL) {
		oldpage = *mres;
		vm_page_lock(oldpage);
		vm_page_remove(oldpage);
		vm_page_unlock(oldpage);
		*mres = NULL;
	} else
		oldpage = NULL;
	VM_OBJECT_WUNLOCK(vm_obj);
retry:
	cause = ret = 0;
	page = NULL;

	if (i915_intr_pf) {
		ret = i915_mutex_lock_interruptible(dev);
		if (ret != 0) {
			cause = 10;
			goto out;
		}
	} else
		DRM_LOCK(dev);

	/*
	 * Since the object lock was dropped, other thread might have
	 * faulted on the same GTT address and instantiated the
	 * mapping for the page.  Recheck.
	 */
	VM_OBJECT_WLOCK(vm_obj);
	page = vm_page_lookup(vm_obj, OFF_TO_IDX(offset));
	if (page != NULL) {
		if (vm_page_busied(page)) {
			DRM_UNLOCK(dev);
			vm_page_lock(page);
			VM_OBJECT_WUNLOCK(vm_obj);
			vm_page_busy_sleep(page, "915pee");
			goto retry;
		}
		goto have_page;
	} else
		VM_OBJECT_WUNLOCK(vm_obj);

	/* Now bind it into the GTT if needed */
	if (!obj->map_and_fenceable) {
		ret = i915_gem_object_unbind(obj);
		if (ret != 0) {
			cause = 20;
			goto unlock;
		}
	}
	if (!obj->gtt_space) {
		ret = i915_gem_object_bind_to_gtt(obj, 0, true);
		if (ret != 0) {
			cause = 30;
			goto unlock;
		}

		ret = i915_gem_object_set_to_gtt_domain(obj, write);
		if (ret != 0) {
			cause = 40;
			goto unlock;
		}
	}

	if (!obj->has_global_gtt_mapping)
		i915_gem_gtt_bind_object(obj, obj->cache_level);

	ret = i915_gem_object_get_fence(obj);
	if (ret != 0) {
		cause = 50;
		goto unlock;
	}

	if (i915_gem_object_is_inactive(obj))
		list_move_tail(&obj->mm_list, &dev_priv->mm.inactive_list);

	obj->fault_mappable = true;
	VM_OBJECT_WLOCK(vm_obj);
	page = PHYS_TO_VM_PAGE(dev->agp->base + obj->gtt_offset + offset);
	KASSERT((page->flags & PG_FICTITIOUS) != 0,
	    ("physical address %#jx not fictitious",
	    (uintmax_t)(dev->agp->base + obj->gtt_offset + offset)));
	if (page == NULL) {
		VM_OBJECT_WUNLOCK(vm_obj);
		cause = 60;
		ret = -EFAULT;
		goto unlock;
	}
	KASSERT((page->flags & PG_FICTITIOUS) != 0,
	    ("not fictitious %p", page));
	KASSERT(page->wire_count == 1, ("wire_count not 1 %p", page));

	if (vm_page_busied(page)) {
		DRM_UNLOCK(dev);
		vm_page_lock(page);
		VM_OBJECT_WUNLOCK(vm_obj);
		vm_page_busy_sleep(page, "915pbs");
		goto retry;
	}
	if (vm_page_insert(page, vm_obj, OFF_TO_IDX(offset))) {
		DRM_UNLOCK(dev);
		VM_OBJECT_WUNLOCK(vm_obj);
		VM_WAIT;
		goto retry;
	}
	page->valid = VM_PAGE_BITS_ALL;
have_page:
	*mres = page;
	vm_page_xbusy(page);

	CTR4(KTR_DRM, "fault %p %jx %x phys %x", gem_obj, offset, prot,
	    page->phys_addr);
	DRM_UNLOCK(dev);
	if (oldpage != NULL) {
		vm_page_lock(oldpage);
		vm_page_free(oldpage);
		vm_page_unlock(oldpage);
	}
	vm_object_pip_wakeup(vm_obj);
	return (VM_PAGER_OK);

unlock:
	DRM_UNLOCK(dev);
out:
	KASSERT(ret != 0, ("i915_gem_pager_fault: wrong return"));
	CTR5(KTR_DRM, "fault_fail %p %jx %x err %d %d", gem_obj, offset, prot,
	    -ret, cause);
	if (ret == -EAGAIN || ret == -EIO || ret == -EINTR) {
		kern_yield(PRI_USER);
		goto retry;
	}
	VM_OBJECT_WLOCK(vm_obj);
	vm_object_pip_wakeup(vm_obj);
	return (VM_PAGER_ERROR);
}

static void
i915_gem_pager_dtor(void *handle)
{
	struct drm_gem_object *obj;
	struct drm_device *dev;

	obj = handle;
	dev = obj->dev;

	DRM_LOCK(dev);
	drm_gem_free_mmap_offset(obj);
	i915_gem_release_mmap(to_intel_bo(obj));
	drm_gem_object_unreference(obj);
	DRM_UNLOCK(dev);
}

struct cdev_pager_ops i915_gem_pager_ops = {
	.cdev_pg_fault	= i915_gem_pager_fault,
	.cdev_pg_ctor	= i915_gem_pager_ctor,
	.cdev_pg_dtor	= i915_gem_pager_dtor
};

/**
 * i915_gem_release_mmap - remove physical page mappings
 * @obj: obj in question
 *
 * Preserve the reservation of the mmapping with the DRM core code, but
 * relinquish ownership of the pages back to the system.
 *
 * It is vital that we remove the page mapping if we have mapped a tiled
 * object through the GTT and then lose the fence register due to
 * resource pressure. Similarly if the object has been moved out of the
 * aperture, than pages mapped into userspace must be revoked. Removing the
 * mapping will then trigger a page fault on the next user access, allowing
 * fixup by i915_gem_fault().
 */
void
i915_gem_release_mmap(struct drm_i915_gem_object *obj)
{
	vm_object_t devobj;
	vm_page_t page;
	int i, page_count;

	if (!obj->fault_mappable)
		return;

	CTR3(KTR_DRM, "release_mmap %p %x %x", obj, obj->gtt_offset,
	    OFF_TO_IDX(obj->base.size));
	devobj = cdev_pager_lookup(obj);
	if (devobj != NULL) {
		page_count = OFF_TO_IDX(obj->base.size);

		VM_OBJECT_WLOCK(devobj);
retry:
		for (i = 0; i < page_count; i++) {
			page = vm_page_lookup(devobj, i);
			if (page == NULL)
				continue;
			if (vm_page_sleep_if_busy(page, "915unm"))
				goto retry;
			cdev_pager_free_page(devobj, page);
		}
		VM_OBJECT_WUNLOCK(devobj);
		vm_object_deallocate(devobj);
	}

	obj->fault_mappable = false;
}

static uint32_t
i915_gem_get_gtt_size(struct drm_device *dev, uint32_t size, int tiling_mode)
{
	uint32_t gtt_size;

	if (INTEL_INFO(dev)->gen >= 4 ||
	    tiling_mode == I915_TILING_NONE)
		return size;

	/* Previous chips need a power-of-two fence region when tiling */
	if (INTEL_INFO(dev)->gen == 3)
		gtt_size = 1024*1024;
	else
		gtt_size = 512*1024;

	while (gtt_size < size)
		gtt_size <<= 1;

	return gtt_size;
}

/**
 * i915_gem_get_gtt_alignment - return required GTT alignment for an object
 * @obj: object to check
 *
 * Return the required GTT alignment for an object, taking into account
 * potential fence register mapping.
 */
static uint32_t
i915_gem_get_gtt_alignment(struct drm_device *dev,
			   uint32_t size,
			   int tiling_mode)
{
	/*
	 * Minimum alignment is 4k (GTT page size), but might be greater
	 * if a fence register is needed for the object.
	 */
	if (INTEL_INFO(dev)->gen >= 4 ||
	    tiling_mode == I915_TILING_NONE)
		return 4096;

	/*
	 * Previous chips need to be aligned to the size of the smallest
	 * fence register that can contain the object.
	 */
	return i915_gem_get_gtt_size(dev, size, tiling_mode);
}

/**
 * i915_gem_get_unfenced_gtt_alignment - return required GTT alignment for an
 *					 unfenced object
 * @dev: the device
 * @size: size of the object
 * @tiling_mode: tiling mode of the object
 *
 * Return the required GTT alignment for an object, only taking into account
 * unfenced tiled surface requirements.
 */
uint32_t
i915_gem_get_unfenced_gtt_alignment(struct drm_device *dev,
				    uint32_t size,
				    int tiling_mode)
{
	/*
	 * Minimum alignment is 4k (GTT page size) for sane hw.
	 */
	if (INTEL_INFO(dev)->gen >= 4 || IS_G33(dev) ||
	    tiling_mode == I915_TILING_NONE)
		return 4096;

	/* Previous hardware however needs to be aligned to a power-of-two
	 * tile height. The simplest method for determining this is to reuse
	 * the power-of-tile object size.
	 */
	return i915_gem_get_gtt_size(dev, size, tiling_mode);
}

int
i915_gem_mmap_gtt(struct drm_file *file,
		  struct drm_device *dev,
		  uint32_t handle,
		  uint64_t *offset)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj;
	int ret;

	ret = i915_mutex_lock_interruptible(dev);
	if (ret)
		return ret;

	obj = to_intel_bo(drm_gem_object_lookup(dev, file, handle));
	if (&obj->base == NULL) {
		ret = -ENOENT;
		goto unlock;
	}

	if (obj->base.size > dev_priv->mm.gtt_mappable_end) {
		ret = -E2BIG;
		goto out;
	}

	if (obj->madv != I915_MADV_WILLNEED) {
		DRM_ERROR("Attempting to mmap a purgeable buffer\n");
		ret = -EINVAL;
		goto out;
	}

	ret = drm_gem_create_mmap_offset(&obj->base);
	if (ret)
		goto out;

	*offset = DRM_GEM_MAPPING_OFF(obj->base.map_list.key) |
	    DRM_GEM_MAPPING_KEY;

out:
	drm_gem_object_unreference(&obj->base);
unlock:
	DRM_UNLOCK(dev);
	return ret;
}

/**
 * i915_gem_mmap_gtt_ioctl - prepare an object for GTT mmap'ing
 * @dev: DRM device
 * @data: GTT mapping ioctl data
 * @file: GEM object info
 *
 * Simply returns the fake offset to userspace so it can mmap it.
 * The mmap call will end up in drm_gem_mmap(), which will set things
 * up so we can get faults in the handler above.
 *
 * The fault handler will take care of binding the object into the GTT
 * (since it may have been evicted to make room for something), allocating
 * a fence register, and mapping the appropriate aperture address into
 * userspace.
 */
int
i915_gem_mmap_gtt_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file)
{
	struct drm_i915_gem_mmap_gtt *args = data;

	return i915_gem_mmap_gtt(file, dev, args->handle, &args->offset);
}

/* Immediately discard the backing storage */
static void
i915_gem_object_truncate(struct drm_i915_gem_object *obj)
{
	vm_object_t vm_obj;

	vm_obj = obj->base.vm_obj;
	VM_OBJECT_WLOCK(vm_obj);
	vm_object_page_remove(vm_obj, 0, 0, false);
	VM_OBJECT_WUNLOCK(vm_obj);
	drm_gem_free_mmap_offset(&obj->base);
	obj->madv = I915_MADV_PURGED_INTERNAL;
}

static inline int
i915_gem_object_is_purgeable(struct drm_i915_gem_object *obj)
{
	return obj->madv == I915_MADV_DONTNEED;
}

static void
i915_gem_object_put_pages_range_locked(struct drm_i915_gem_object *obj,
    vm_pindex_t si, vm_pindex_t ei)
{
	vm_object_t vm_obj;
	vm_page_t page;
	vm_pindex_t i;

	vm_obj = obj->base.vm_obj;
	VM_OBJECT_ASSERT_LOCKED(vm_obj);
	for (i = si,  page = vm_page_lookup(vm_obj, i); i < ei;
	    page = vm_page_next(page), i++) {
		KASSERT(page->pindex == i, ("pindex %jx %jx",
		    (uintmax_t)page->pindex, (uintmax_t)i));
		vm_page_lock(page);
		vm_page_unwire(page, PQ_INACTIVE);
		if (page->wire_count == 0)
			atomic_add_long(&i915_gem_wired_pages_cnt, -1);
		vm_page_unlock(page);
	}
}

#define	GEM_PARANOID_CHECK_GTT 0
#if GEM_PARANOID_CHECK_GTT
static void
i915_gem_assert_pages_not_mapped(struct drm_device *dev, vm_page_t *ma,
    int page_count)
{
	struct drm_i915_private *dev_priv;
	vm_paddr_t pa;
	unsigned long start, end;
	u_int i;
	int j;

	dev_priv = dev->dev_private;
	start = OFF_TO_IDX(dev_priv->mm.gtt_start);
	end = OFF_TO_IDX(dev_priv->mm.gtt_end);
	for (i = start; i < end; i++) {
		pa = intel_gtt_read_pte_paddr(i);
		for (j = 0; j < page_count; j++) {
			if (pa == VM_PAGE_TO_PHYS(ma[j])) {
				panic("Page %p in GTT pte index %d pte %x",
				    ma[i], i, intel_gtt_read_pte(i));
			}
		}
	}
}
#endif

static void
i915_gem_object_put_pages_range(struct drm_i915_gem_object *obj,
    off_t start, off_t end)
{
	vm_object_t vm_obj;

	vm_obj = obj->base.vm_obj;
	VM_OBJECT_WLOCK(vm_obj);
	i915_gem_object_put_pages_range_locked(obj,
	    OFF_TO_IDX(trunc_page(start)), OFF_TO_IDX(round_page(end)));
	VM_OBJECT_WUNLOCK(vm_obj);
}

static void
i915_gem_object_put_pages_gtt(struct drm_i915_gem_object *obj)
{
	vm_page_t page;
	int page_count, i;

	KASSERT(obj->madv != I915_MADV_PURGED_INTERNAL, ("Purged object"));

	if (obj->tiling_mode != I915_TILING_NONE)
		i915_gem_object_save_bit_17_swizzle(obj);
	if (obj->madv == I915_MADV_DONTNEED)
		obj->dirty = 0;
	page_count = obj->base.size / PAGE_SIZE;
	VM_OBJECT_WLOCK(obj->base.vm_obj);
#if GEM_PARANOID_CHECK_GTT
	i915_gem_assert_pages_not_mapped(obj->base.dev, obj->pages, page_count);
#endif
	for (i = 0; i < page_count; i++) {
		page = obj->pages[i];
		if (obj->dirty)
			vm_page_dirty(page);
		if (obj->madv == I915_MADV_WILLNEED)
			vm_page_reference(page);
		vm_page_lock(page);
		vm_page_unwire(obj->pages[i], PQ_ACTIVE);
		vm_page_unlock(page);
		atomic_add_long(&i915_gem_wired_pages_cnt, -1);
	}
	VM_OBJECT_WUNLOCK(obj->base.vm_obj);
	obj->dirty = 0;
	free(obj->pages, DRM_I915_GEM);
	obj->pages = NULL;
}

static int
i915_gpu_is_active(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	return (!list_empty(&dev_priv->mm.flushing_list) ||
	    !list_empty(&dev_priv->mm.active_list));
}

static void
i915_gem_lowmem(void *arg)
{
	struct drm_device *dev;
	struct drm_i915_private *dev_priv;
	struct drm_i915_gem_object *obj, *next;
	int cnt, cnt_fail, cnt_total;

	dev = arg;
	dev_priv = dev->dev_private;

	if (!sx_try_xlock(&dev->dev_struct_lock))
		return;

	CTR0(KTR_DRM, "gem_lowmem");

rescan:
	/* first scan for clean buffers */
	i915_gem_retire_requests(dev);

	cnt_total = cnt_fail = cnt = 0;

	list_for_each_entry_safe(obj, next, &dev_priv->mm.inactive_list,
	    mm_list) {
		if (i915_gem_object_is_purgeable(obj)) {
			if (i915_gem_object_unbind(obj) != 0)
				cnt_total++;
		} else
			cnt_total++;
	}

	/* second pass, evict/count anything still on the inactive list */
	list_for_each_entry_safe(obj, next, &dev_priv->mm.inactive_list,
	    mm_list) {
		if (i915_gem_object_unbind(obj) == 0)
			cnt++;
		else
			cnt_fail++;
	}

	if (cnt_fail > cnt_total / 100 && i915_gpu_is_active(dev)) {
		/*
		 * We are desperate for pages, so as a last resort, wait
		 * for the GPU to finish and discard whatever we can.
		 * This has a dramatic impact to reduce the number of
		 * OOM-killer events whilst running the GPU aggressively.
		 */
		if (i915_gpu_idle(dev) == 0)
			goto rescan;
	}
	DRM_UNLOCK(dev);
}

static int
i915_gem_object_get_pages_range(struct drm_i915_gem_object *obj,
    off_t start, off_t end)
{
	vm_object_t vm_obj;
	vm_page_t page;
	vm_pindex_t si, ei, i;
	bool need_swizzle, fresh;

	need_swizzle = i915_gem_object_needs_bit17_swizzle(obj) != 0;
	vm_obj = obj->base.vm_obj;
	si = OFF_TO_IDX(trunc_page(start));
	ei = OFF_TO_IDX(round_page(end));
	VM_OBJECT_WLOCK(vm_obj);
	for (i = si; i < ei; i++) {
		page = i915_gem_wire_page(vm_obj, i, &fresh);
		if (page == NULL)
			goto failed;
		if (need_swizzle && fresh)
			i915_gem_object_do_bit_17_swizzle_page(obj, page);
	}
	VM_OBJECT_WUNLOCK(vm_obj);
	return (0);
failed:
	i915_gem_object_put_pages_range_locked(obj, si, i);
	VM_OBJECT_WUNLOCK(vm_obj);
	return (-EIO);
}

static int
i915_gem_object_get_pages_gtt(struct drm_i915_gem_object *obj,
    int flags)
{
	vm_object_t vm_obj;
	vm_page_t page;
	vm_pindex_t i, page_count;
	int res;

	KASSERT(obj->pages == NULL, ("Obj already has pages"));

	page_count = OFF_TO_IDX(obj->base.size);
	obj->pages = malloc(page_count * sizeof(vm_page_t), DRM_I915_GEM,
	    M_WAITOK);
	res = i915_gem_object_get_pages_range(obj, 0, obj->base.size);
	if (res != 0) {
		free(obj->pages, DRM_I915_GEM);
		obj->pages = NULL;
		return (res);
	}
	vm_obj = obj->base.vm_obj;
	VM_OBJECT_WLOCK(vm_obj);
	for (i = 0, page = vm_page_lookup(vm_obj, 0); i < page_count;
	    i++, page = vm_page_next(page)) {
		KASSERT(page->pindex == i, ("pindex %jx %jx",
		    (uintmax_t)page->pindex, (uintmax_t)i));
		obj->pages[i] = page;
	}
	VM_OBJECT_WUNLOCK(vm_obj);
	return (0);
}

void
i915_gem_object_move_to_active(struct drm_i915_gem_object *obj,
			       struct intel_ring_buffer *ring, uint32_t seqno)
{
	struct drm_device *dev = obj->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_fence_reg *reg;

	KASSERT(ring != NULL, ("NULL ring"));
	obj->ring = ring;

	/* Add a reference if we're newly entering the active list. */
	if (!obj->active) {
		drm_gem_object_reference(&obj->base);
		obj->active = 1;
	}

	/* Move from whatever list we were on to the tail of execution. */
	list_move_tail(&obj->mm_list, &dev_priv->mm.active_list);
	list_move_tail(&obj->ring_list, &ring->active_list);

	obj->last_rendering_seqno = seqno;
	if (obj->fenced_gpu_access) {
		obj->last_fenced_seqno = seqno;

		/* Bump MRU to take account of the delayed flush */
		if (obj->fence_reg != I915_FENCE_REG_NONE) {
			reg = &dev_priv->fence_regs[obj->fence_reg];
			list_move_tail(&reg->lru_list,
				       &dev_priv->mm.fence_list);
		}
	}
}

static void
i915_gem_object_move_off_active(struct drm_i915_gem_object *obj)
{
	list_del_init(&obj->ring_list);
	obj->last_rendering_seqno = 0;
	obj->last_fenced_seqno = 0;
}

static void
i915_gem_object_move_to_flushing(struct drm_i915_gem_object *obj)
{
	struct drm_device *dev = obj->base.dev;
	drm_i915_private_t *dev_priv = dev->dev_private;

	KASSERT(obj->active, ("Object not active"));
	list_move_tail(&obj->mm_list, &dev_priv->mm.flushing_list);

	i915_gem_object_move_off_active(obj);
}

static void
i915_gem_object_move_to_inactive(struct drm_i915_gem_object *obj)
{
	struct drm_device *dev = obj->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	list_move_tail(&obj->mm_list, &dev_priv->mm.inactive_list);

	KASSERT(list_empty(&obj->gpu_write_list), ("On gpu_write_list"));
	KASSERT(obj->active, ("Object not active"));
	obj->ring = NULL;

	i915_gem_object_move_off_active(obj);
	obj->fenced_gpu_access = false;

	obj->active = 0;
	obj->pending_gpu_write = false;
	drm_gem_object_unreference(&obj->base);

#if 1
	KIB_NOTYET();
#else
	WARN_ON(i915_verify_lists(dev));
#endif
}

static u32
i915_gem_get_seqno(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	u32 seqno = dev_priv->next_seqno;

	/* reserve 0 for non-seqno */
	if (++dev_priv->next_seqno == 0)
		dev_priv->next_seqno = 1;

	return seqno;
}

u32
i915_gem_next_request_seqno(struct intel_ring_buffer *ring)
{
	if (ring->outstanding_lazy_request == 0)
		ring->outstanding_lazy_request = i915_gem_get_seqno(ring->dev);

	return ring->outstanding_lazy_request;
}

int
i915_add_request(struct intel_ring_buffer *ring,
		 struct drm_file *file,
		 struct drm_i915_gem_request *request)
{
	drm_i915_private_t *dev_priv = ring->dev->dev_private;
	struct drm_i915_file_private *file_priv;
	uint32_t seqno;
	u32 request_ring_position;
	int was_empty;
	int ret;

	KASSERT(request != NULL, ("NULL request in add"));
	DRM_LOCK_ASSERT(ring->dev);

	seqno = i915_gem_next_request_seqno(ring);
	request_ring_position = intel_ring_get_tail(ring);

	ret = ring->add_request(ring, &seqno);
	if (ret != 0)
	    return ret;

	CTR2(KTR_DRM, "request_add %s %d", ring->name, seqno);

	request->seqno = seqno;
	request->ring = ring;
	request->tail = request_ring_position;
	request->emitted_jiffies = ticks;
	was_empty = list_empty(&ring->request_list);
	list_add_tail(&request->list, &ring->request_list);

	if (file) {
		file_priv = file->driver_priv;

		mtx_lock(&file_priv->mm.lck);
		request->file_priv = file_priv;
		list_add_tail(&request->client_list,
			      &file_priv->mm.request_list);
		mtx_unlock(&file_priv->mm.lck);
	}

	ring->outstanding_lazy_request = 0;

	if (!dev_priv->mm.suspended) {
		if (i915_enable_hangcheck) {
			callout_schedule(&dev_priv->hangcheck_timer,
			    DRM_I915_HANGCHECK_PERIOD);
		}
		if (was_empty)
			taskqueue_enqueue_timeout(dev_priv->tq,
			    &dev_priv->mm.retire_task, hz);
	}

	return 0;
}

static inline void
i915_gem_request_remove_from_client(struct drm_i915_gem_request *request)
{
	struct drm_i915_file_private *file_priv = request->file_priv;

	if (!file_priv)
		return;

	DRM_LOCK_ASSERT(request->ring->dev);

	mtx_lock(&file_priv->mm.lck);
	if (request->file_priv) {
		list_del(&request->client_list);
		request->file_priv = NULL;
	}
	mtx_unlock(&file_priv->mm.lck);
}

static void i915_gem_reset_ring_lists(struct drm_i915_private *dev_priv,
				      struct intel_ring_buffer *ring)
{
	if (ring->dev != NULL)
		DRM_LOCK_ASSERT(ring->dev);

	while (!list_empty(&ring->request_list)) {
		struct drm_i915_gem_request *request;

		request = list_first_entry(&ring->request_list,
					   struct drm_i915_gem_request,
					   list);

		list_del(&request->list);
		i915_gem_request_remove_from_client(request);
		free(request, DRM_I915_GEM);
	}

	while (!list_empty(&ring->active_list)) {
		struct drm_i915_gem_object *obj;

		obj = list_first_entry(&ring->active_list,
				       struct drm_i915_gem_object,
				       ring_list);

		obj->base.write_domain = 0;
		list_del_init(&obj->gpu_write_list);
		i915_gem_object_move_to_inactive(obj);
	}
}

static void i915_gem_reset_fences(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int i;

	for (i = 0; i < dev_priv->num_fence_regs; i++) {
		struct drm_i915_fence_reg *reg = &dev_priv->fence_regs[i];

		i915_gem_write_fence(dev, i, NULL);

		if (reg->obj)
			i915_gem_object_fence_lost(reg->obj);

		reg->pin_count = 0;
		reg->obj = NULL;
		INIT_LIST_HEAD(&reg->lru_list);
	}

	INIT_LIST_HEAD(&dev_priv->mm.fence_list);
}

void i915_gem_reset(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj;
	struct intel_ring_buffer *ring;
	int i;

	for_each_ring(ring, dev_priv, i)
		i915_gem_reset_ring_lists(dev_priv, ring);

	/* Remove anything from the flushing lists. The GPU cache is likely
	 * to be lost on reset along with the data, so simply move the
	 * lost bo to the inactive list.
	 */
	while (!list_empty(&dev_priv->mm.flushing_list)) {
		obj = list_first_entry(&dev_priv->mm.flushing_list,
				      struct drm_i915_gem_object,
				      mm_list);

		obj->base.write_domain = 0;
		list_del_init(&obj->gpu_write_list);
		i915_gem_object_move_to_inactive(obj);
	}

	/* Move everything out of the GPU domains to ensure we do any
	 * necessary invalidation upon reuse.
	 */
	list_for_each_entry(obj, &dev_priv->mm.inactive_list, mm_list) {
		obj->base.read_domains &= ~I915_GEM_GPU_DOMAINS;
	}

	/* The fence registers are invalidated so clear them out */
	i915_gem_reset_fences(dev);
}

/**
 * This function clears the request list as sequence numbers are passed.
 */
void
i915_gem_retire_requests_ring(struct intel_ring_buffer *ring)
{
	uint32_t seqno;
	int i;

	if (list_empty(&ring->request_list))
		return;

	seqno = ring->get_seqno(ring);
	CTR2(KTR_DRM, "retire_request_ring %s %d", ring->name, seqno);

	for (i = 0; i < ARRAY_SIZE(ring->sync_seqno); i++)
		if (seqno >= ring->sync_seqno[i])
			ring->sync_seqno[i] = 0;

	while (!list_empty(&ring->request_list)) {
		struct drm_i915_gem_request *request;

		request = list_first_entry(&ring->request_list,
					   struct drm_i915_gem_request,
					   list);

		if (!i915_seqno_passed(seqno, request->seqno))
			break;

		CTR2(KTR_DRM, "retire_request_seqno_passed %s %d",
		    ring->name, seqno);
		ring->last_retired_head = request->tail;

		list_del(&request->list);
		i915_gem_request_remove_from_client(request);
		free(request, DRM_I915_GEM);
	}

	/* Move any buffers on the active list that are no longer referenced
	 * by the ringbuffer to the flushing/inactive lists as appropriate.
	 */
	while (!list_empty(&ring->active_list)) {
		struct drm_i915_gem_object *obj;

		obj = list_first_entry(&ring->active_list,
				      struct drm_i915_gem_object,
				      ring_list);

		if (!i915_seqno_passed(seqno, obj->last_rendering_seqno))
			break;

		if (obj->base.write_domain != 0)
			i915_gem_object_move_to_flushing(obj);
		else
			i915_gem_object_move_to_inactive(obj);
	}

	if (ring->trace_irq_seqno &&
	    i915_seqno_passed(seqno, ring->trace_irq_seqno)) {
		struct drm_i915_private *dev_priv = ring->dev->dev_private;
		mtx_lock(&dev_priv->irq_lock);
		ring->irq_put(ring);
		mtx_unlock(&dev_priv->irq_lock);
		ring->trace_irq_seqno = 0;
	}
}

void
i915_gem_retire_requests(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring;
	int i;

	for_each_ring(ring, dev_priv, i)
		i915_gem_retire_requests_ring(ring);
}

static void
i915_gem_process_flushing_list(struct intel_ring_buffer *ring,
    uint32_t flush_domains)
{
	struct drm_i915_gem_object *obj, *next;
	uint32_t old_write_domain;

	list_for_each_entry_safe(obj, next, &ring->gpu_write_list,
	    gpu_write_list) {
		if (obj->base.write_domain & flush_domains) {
			old_write_domain = obj->base.write_domain;
			obj->base.write_domain = 0;
			list_del_init(&obj->gpu_write_list);
			i915_gem_object_move_to_active(obj, ring,
			    i915_gem_next_request_seqno(ring));

	CTR3(KTR_DRM, "object_change_domain process_flush %p %x %x",
			    obj, obj->base.read_domains, old_write_domain);
		}
	}
}

int
i915_gem_flush_ring(struct intel_ring_buffer *ring, uint32_t invalidate_domains,
    uint32_t flush_domains)
{
	int ret;

	if (((invalidate_domains | flush_domains) & I915_GEM_GPU_DOMAINS) == 0)
		return 0;

	CTR3(KTR_DRM, "ring_flush %s %x %x", ring->name, invalidate_domains,
	    flush_domains);
	ret = ring->flush(ring, invalidate_domains, flush_domains);
	if (ret)
		return ret;

	if (flush_domains & I915_GEM_GPU_DOMAINS)
		i915_gem_process_flushing_list(ring, flush_domains);
	return 0;
}

static void
i915_gem_retire_task_handler(void *arg, int pending)
{
	drm_i915_private_t *dev_priv;
	struct drm_device *dev;
	struct intel_ring_buffer *ring;
	bool idle;
	int i;

	dev_priv = arg;
	dev = dev_priv->dev;

	/* Come back later if the device is busy... */
	if (!sx_try_xlock(&dev->dev_struct_lock)) {
		taskqueue_enqueue_timeout(dev_priv->tq,
		    &dev_priv->mm.retire_task, hz);
		return;
	}

	CTR0(KTR_DRM, "retire_task");

	i915_gem_retire_requests(dev);

	/* Send a periodic flush down the ring so we don't hold onto GEM
	 * objects indefinitely.
	 */
	idle = true;
	for_each_ring(ring, dev_priv, i) {
		struct intel_ring_buffer *ring = &dev_priv->rings[i];

		if (!list_empty(&ring->gpu_write_list)) {
			struct drm_i915_gem_request *request;
			int ret;

			ret = i915_gem_flush_ring(ring,
						  0, I915_GEM_GPU_DOMAINS);
			request = malloc(sizeof(*request), DRM_I915_GEM,
			    M_WAITOK | M_ZERO);
			if (ret || request == NULL ||
			    i915_add_request(ring, NULL, request))
				free(request, DRM_I915_GEM);
		}

		idle &= list_empty(&ring->request_list);
	}

	if (!dev_priv->mm.suspended && !idle)
		taskqueue_enqueue_timeout(dev_priv->tq,
		    &dev_priv->mm.retire_task, hz);

	DRM_UNLOCK(dev);
}

int
i915_gem_object_sync(struct drm_i915_gem_object *obj,
		     struct intel_ring_buffer *to)
{
	struct intel_ring_buffer *from = obj->ring;
	u32 seqno;
	int ret, idx;

	if (from == NULL || to == from)
		return 0;

	if (to == NULL || !i915_semaphore_is_enabled(obj->base.dev))
		return i915_gem_object_wait_rendering(obj);

	idx = intel_ring_sync_index(from, to);

	seqno = obj->last_rendering_seqno;
	if (seqno <= from->sync_seqno[idx])
		return 0;

	if (seqno == from->outstanding_lazy_request) {
		struct drm_i915_gem_request *request;

		request = malloc(sizeof(*request), DRM_I915_GEM,
		    M_WAITOK | M_ZERO);
		ret = i915_add_request(from, NULL, request);
		if (ret) {
			free(request, DRM_I915_GEM);
			return ret;
		}
		seqno = request->seqno;
	}


	ret = to->sync_to(to, from, seqno);
	if (!ret)
		from->sync_seqno[idx] = seqno;

	return ret;
}

static void i915_gem_object_finish_gtt(struct drm_i915_gem_object *obj)
{
	u32 old_write_domain, old_read_domains;

	/* Act a barrier for all accesses through the GTT */
	mb();

	/* Force a pagefault for domain tracking on next user access */
	i915_gem_release_mmap(obj);

	if ((obj->base.read_domains & I915_GEM_DOMAIN_GTT) == 0)
		return;

	old_read_domains = obj->base.read_domains;
	old_write_domain = obj->base.write_domain;

	obj->base.read_domains &= ~I915_GEM_DOMAIN_GTT;
	obj->base.write_domain &= ~I915_GEM_DOMAIN_GTT;

	CTR3(KTR_DRM, "object_change_domain finish gtt %p %x %x",
	    obj, old_read_domains, old_write_domain);
}

/**
 * Unbinds an object from the GTT aperture.
 */
int
i915_gem_object_unbind(struct drm_i915_gem_object *obj)
{
	drm_i915_private_t *dev_priv = obj->base.dev->dev_private;
	int ret = 0;

	if (obj->gtt_space == NULL)
		return 0;

	if (obj->pin_count) {
		DRM_ERROR("Attempting to unbind pinned buffer\n");
		return -EINVAL;
	}

	ret = i915_gem_object_finish_gpu(obj);
	if (ret == -ERESTARTSYS || ret == -EINTR)
		return ret;

	i915_gem_object_finish_gtt(obj);

	if (ret == 0)
		ret = i915_gem_object_set_to_cpu_domain(obj, 1);
	if (ret == -ERESTARTSYS || ret == -EINTR)
		return ret;
	if (ret != 0) {
		i915_gem_clflush_object(obj);
		obj->base.read_domains = obj->base.write_domain =
		    I915_GEM_DOMAIN_CPU;
	}

	/* release the fence reg _after_ flushing */
	ret = i915_gem_object_put_fence(obj);
	if (ret)
		return ret;

	if (obj->has_global_gtt_mapping)
		i915_gem_gtt_unbind_object(obj);
	if (obj->has_aliasing_ppgtt_mapping) {
		i915_ppgtt_unbind_object(dev_priv->mm.aliasing_ppgtt, obj);
		obj->has_aliasing_ppgtt_mapping = 0;
	}
	i915_gem_gtt_finish_object(obj);

	i915_gem_object_put_pages_gtt(obj);

	list_del_init(&obj->gtt_list);
	list_del_init(&obj->mm_list);
	obj->map_and_fenceable = true;

	drm_mm_put_block(obj->gtt_space);
	obj->gtt_space = NULL;
	obj->gtt_offset = 0;

	if (i915_gem_object_is_purgeable(obj))
		i915_gem_object_truncate(obj);
	CTR1(KTR_DRM, "object_unbind %p", obj);

	return ret;
}

static int
i915_ring_idle(struct intel_ring_buffer *ring)
{
	int ret;

	if (list_empty(&ring->gpu_write_list) && list_empty(&ring->active_list))
		return 0;

	if (!list_empty(&ring->gpu_write_list)) {
		ret = i915_gem_flush_ring(ring, I915_GEM_GPU_DOMAINS,
		    I915_GEM_GPU_DOMAINS);
		if (ret != 0)
			return ret;
	}

	return (i915_wait_request(ring, i915_gem_next_request_seqno(ring)));
}

int i915_gpu_idle(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring;
	int ret, i;

	/* Flush everything onto the inactive list. */
	for_each_ring(ring, dev_priv, i) {
		ret = i915_switch_context(ring, NULL, DEFAULT_CONTEXT_ID);
		if (ret)
			return ret;

		ret = i915_ring_idle(ring);
		if (ret)
			return ret;

		/* Is the device fubar? */
		if (!list_empty(&ring->gpu_write_list))
			return -EBUSY;
	}

	return 0;
}

static void sandybridge_write_fence_reg(struct drm_device *dev, int reg,
					struct drm_i915_gem_object *obj)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	uint64_t val;

	if (obj) {
		u32 size = obj->gtt_space->size;

		val = (uint64_t)((obj->gtt_offset + size - 4096) &
				 0xfffff000) << 32;
		val |= obj->gtt_offset & 0xfffff000;
		val |= (uint64_t)((obj->stride / 128) - 1) <<
			SANDYBRIDGE_FENCE_PITCH_SHIFT;

		if (obj->tiling_mode == I915_TILING_Y)
			val |= 1 << I965_FENCE_TILING_Y_SHIFT;
		val |= I965_FENCE_REG_VALID;
	} else
		val = 0;

	I915_WRITE64(FENCE_REG_SANDYBRIDGE_0 + reg * 8, val);
	POSTING_READ(FENCE_REG_SANDYBRIDGE_0 + reg * 8);
}

static void i965_write_fence_reg(struct drm_device *dev, int reg,
				 struct drm_i915_gem_object *obj)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	uint64_t val;

	if (obj) {
		u32 size = obj->gtt_space->size;

		val = (uint64_t)((obj->gtt_offset + size - 4096) &
				 0xfffff000) << 32;
		val |= obj->gtt_offset & 0xfffff000;
		val |= ((obj->stride / 128) - 1) << I965_FENCE_PITCH_SHIFT;
		if (obj->tiling_mode == I915_TILING_Y)
			val |= 1 << I965_FENCE_TILING_Y_SHIFT;
		val |= I965_FENCE_REG_VALID;
	} else
		val = 0;

	I915_WRITE64(FENCE_REG_965_0 + reg * 8, val);
	POSTING_READ(FENCE_REG_965_0 + reg * 8);
}

static void i915_write_fence_reg(struct drm_device *dev, int reg,
				 struct drm_i915_gem_object *obj)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	u32 val;

	if (obj) {
		u32 size = obj->gtt_space->size;
		int pitch_val;
		int tile_width;

		if ((obj->gtt_offset & ~I915_FENCE_START_MASK) ||
		     (size & -size) != size ||
		     (obj->gtt_offset & (size - 1)))
			printf(
		     "object 0x%08x [fenceable? %d] not 1M or pot-size (0x%08x) aligned\n",
		     obj->gtt_offset, obj->map_and_fenceable, size);

		if (obj->tiling_mode == I915_TILING_Y && HAS_128_BYTE_Y_TILING(dev))
			tile_width = 128;
		else
			tile_width = 512;

		/* Note: pitch better be a power of two tile widths */
		pitch_val = obj->stride / tile_width;
		pitch_val = ffs(pitch_val) - 1;

		val = obj->gtt_offset;
		if (obj->tiling_mode == I915_TILING_Y)
			val |= 1 << I830_FENCE_TILING_Y_SHIFT;
		val |= I915_FENCE_SIZE_BITS(size);
		val |= pitch_val << I830_FENCE_PITCH_SHIFT;
		val |= I830_FENCE_REG_VALID;
	} else
		val = 0;

	if (reg < 8)
		reg = FENCE_REG_830_0 + reg * 4;
	else
		reg = FENCE_REG_945_8 + (reg - 8) * 4;

	I915_WRITE(reg, val);
	POSTING_READ(reg);
}

static void i830_write_fence_reg(struct drm_device *dev, int reg,
				struct drm_i915_gem_object *obj)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	uint32_t val;

	if (obj) {
		u32 size = obj->gtt_space->size;
		uint32_t pitch_val;

		if ((obj->gtt_offset & ~I830_FENCE_START_MASK) ||
		     (size & -size) != size ||
		     (obj->gtt_offset & (size - 1)))
		    printf(
		     "object 0x%08x not 512K or pot-size 0x%08x aligned\n",
		     obj->gtt_offset, size);

		pitch_val = obj->stride / 128;
		pitch_val = ffs(pitch_val) - 1;

		val = obj->gtt_offset;
		if (obj->tiling_mode == I915_TILING_Y)
			val |= 1 << I830_FENCE_TILING_Y_SHIFT;
		val |= I830_FENCE_SIZE_BITS(size);
		val |= pitch_val << I830_FENCE_PITCH_SHIFT;
		val |= I830_FENCE_REG_VALID;
	} else
		val = 0;

	I915_WRITE(FENCE_REG_830_0 + reg * 4, val);
	POSTING_READ(FENCE_REG_830_0 + reg * 4);
}

static void i915_gem_write_fence(struct drm_device *dev, int reg,
				 struct drm_i915_gem_object *obj)
{
	switch (INTEL_INFO(dev)->gen) {
	case 7:
	case 6: sandybridge_write_fence_reg(dev, reg, obj); break;
	case 5:
	case 4: i965_write_fence_reg(dev, reg, obj); break;
	case 3: i915_write_fence_reg(dev, reg, obj); break;
	case 2: i830_write_fence_reg(dev, reg, obj); break;
	default: break;
	}
}

static inline int fence_number(struct drm_i915_private *dev_priv,
			       struct drm_i915_fence_reg *fence)
{
	return fence - dev_priv->fence_regs;
}

static void i915_gem_object_update_fence(struct drm_i915_gem_object *obj,
					 struct drm_i915_fence_reg *fence,
					 bool enable)
{
	struct drm_device *dev = obj->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int fence_reg = fence_number(dev_priv, fence);

	i915_gem_write_fence(dev, fence_reg, enable ? obj : NULL);

	if (enable) {
		obj->fence_reg = fence_reg;
		fence->obj = obj;
		list_move_tail(&fence->lru_list, &dev_priv->mm.fence_list);
	} else {
		obj->fence_reg = I915_FENCE_REG_NONE;
		fence->obj = NULL;
		list_del_init(&fence->lru_list);
	}
}

static int
i915_gem_object_flush_fence(struct drm_i915_gem_object *obj)
{
	int ret;

	if (obj->fenced_gpu_access) {
		if (obj->base.write_domain & I915_GEM_GPU_DOMAINS) {
			ret = i915_gem_flush_ring(obj->ring,
						  0, obj->base.write_domain);
			if (ret)
				return ret;
		}

		obj->fenced_gpu_access = false;
	}

	if (obj->last_fenced_seqno) {
		ret = i915_wait_request(obj->ring,
					obj->last_fenced_seqno);
		if (ret)
			return ret;

		obj->last_fenced_seqno = 0;
	}

	/* Ensure that all CPU reads are completed before installing a fence
	 * and all writes before removing the fence.
	 */
	if (obj->base.read_domains & I915_GEM_DOMAIN_GTT)
		mb();

	return 0;
}

int
i915_gem_object_put_fence(struct drm_i915_gem_object *obj)
{
	struct drm_i915_private *dev_priv = obj->base.dev->dev_private;
	int ret;

	ret = i915_gem_object_flush_fence(obj);
	if (ret)
		return ret;

	if (obj->fence_reg == I915_FENCE_REG_NONE)
		return 0;

	i915_gem_object_update_fence(obj,
				     &dev_priv->fence_regs[obj->fence_reg],
				     false);
	i915_gem_object_fence_lost(obj);

	return 0;
}

static struct drm_i915_fence_reg *
i915_find_fence_reg(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_fence_reg *reg, *avail;
	int i;

	/* First try to find a free reg */
	avail = NULL;
	for (i = dev_priv->fence_reg_start; i < dev_priv->num_fence_regs; i++) {
		reg = &dev_priv->fence_regs[i];
		if (!reg->obj)
			return reg;

		if (!reg->pin_count)
			avail = reg;
	}

	if (avail == NULL)
		return NULL;

	/* None available, try to steal one or wait for a user to finish */
	list_for_each_entry(reg, &dev_priv->mm.fence_list, lru_list) {
		if (reg->pin_count)
			continue;

		return reg;
	}

	return NULL;
}

/**
 * i915_gem_object_get_fence - set up fencing for an object
 * @obj: object to map through a fence reg
 *
 * When mapping objects through the GTT, userspace wants to be able to write
 * to them without having to worry about swizzling if the object is tiled.
 * This function walks the fence regs looking for a free one for @obj,
 * stealing one if it can't find any.
 *
 * It then sets up the reg based on the object's properties: address, pitch
 * and tiling format.
 *
 * For an untiled surface, this removes any existing fence.
 */
int
i915_gem_object_get_fence(struct drm_i915_gem_object *obj)
{
	struct drm_device *dev = obj->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	bool enable = obj->tiling_mode != I915_TILING_NONE;
	struct drm_i915_fence_reg *reg;
	int ret;

	/* Have we updated the tiling parameters upon the object and so
	 * will need to serialise the write to the associated fence register?
	 */
	if (obj->fence_dirty) {
		ret = i915_gem_object_flush_fence(obj);
		if (ret)
			return ret;
	}

	/* Just update our place in the LRU if our fence is getting reused. */
	if (obj->fence_reg != I915_FENCE_REG_NONE) {
		reg = &dev_priv->fence_regs[obj->fence_reg];
		if (!obj->fence_dirty) {
			list_move_tail(&reg->lru_list,
				       &dev_priv->mm.fence_list);
			return 0;
		}
	} else if (enable) {
		reg = i915_find_fence_reg(dev);
		if (reg == NULL)
			return -EDEADLK;

		if (reg->obj) {
			struct drm_i915_gem_object *old = reg->obj;

			ret = i915_gem_object_flush_fence(old);
			if (ret)
				return ret;

			i915_gem_object_fence_lost(old);
		}
	} else
		return 0;

	i915_gem_object_update_fence(obj, reg, enable);
	obj->fence_dirty = false;

	return 0;
}

/**
 * Finds free space in the GTT aperture and binds the object there.
 */
static int
i915_gem_object_bind_to_gtt(struct drm_i915_gem_object *obj,
			    unsigned alignment,
			    bool map_and_fenceable)
{
	struct drm_device *dev = obj->base.dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_mm_node *free_space;
	u32 size, fence_size, fence_alignment, unfenced_alignment;
	bool mappable, fenceable;
	int ret;

	if (obj->madv != I915_MADV_WILLNEED) {
		DRM_ERROR("Attempting to bind a purgeable object\n");
		return -EINVAL;
	}

	fence_size = i915_gem_get_gtt_size(dev,
					   obj->base.size,
					   obj->tiling_mode);
	fence_alignment = i915_gem_get_gtt_alignment(dev,
						     obj->base.size,
						     obj->tiling_mode);
	unfenced_alignment =
		i915_gem_get_unfenced_gtt_alignment(dev,
						    obj->base.size,
						    obj->tiling_mode);

	if (alignment == 0)
		alignment = map_and_fenceable ? fence_alignment :
						unfenced_alignment;
	if (map_and_fenceable && alignment & (fence_alignment - 1)) {
		DRM_ERROR("Invalid object alignment requested %u\n", alignment);
		return -EINVAL;
	}

	size = map_and_fenceable ? fence_size : obj->base.size;

	/* If the object is bigger than the entire aperture, reject it early
	 * before evicting everything in a vain attempt to find space.
	 */
	if (obj->base.size >
	    (map_and_fenceable ? dev_priv->mm.gtt_mappable_end : dev_priv->mm.gtt_total)) {
		DRM_ERROR("Attempting to bind an object larger than the aperture\n");
		return -E2BIG;
	}

 search_free:
	if (map_and_fenceable)
		free_space = drm_mm_search_free_in_range(
		    &dev_priv->mm.gtt_space, size, alignment, 0,
		    dev_priv->mm.gtt_mappable_end, 0);
	else
		free_space = drm_mm_search_free(&dev_priv->mm.gtt_space,
		    size, alignment, 0);
	if (free_space != NULL) {
		if (map_and_fenceable)
			obj->gtt_space = drm_mm_get_block_range_generic(
			    free_space, size, alignment, 0, 0,
			    dev_priv->mm.gtt_mappable_end, 1);
		else
			obj->gtt_space = drm_mm_get_block_generic(free_space,
			    size, alignment, 0, 1);
	}
	if (obj->gtt_space == NULL) {
		ret = i915_gem_evict_something(dev, size, alignment,
		    map_and_fenceable);
		if (ret != 0)
			return ret;
		goto search_free;
	}
	ret = i915_gem_object_get_pages_gtt(obj, 0);
	if (ret) {
		drm_mm_put_block(obj->gtt_space);
		obj->gtt_space = NULL;
		/*
		 * i915_gem_object_get_pages_gtt() cannot return
		 * ENOMEM, since we use vm_page_grab().
		 */
		return ret;
	}

	ret = i915_gem_gtt_prepare_object(obj);
	if (ret) {
		i915_gem_object_put_pages_gtt(obj);
		drm_mm_put_block(obj->gtt_space);
		obj->gtt_space = NULL;
		if (i915_gem_evict_everything(dev, false))
			return ret;
		goto search_free;
	}

	if (!dev_priv->mm.aliasing_ppgtt)
		i915_gem_gtt_bind_object(obj, obj->cache_level);

	list_add_tail(&obj->gtt_list, &dev_priv->mm.gtt_list);
	list_add_tail(&obj->mm_list, &dev_priv->mm.inactive_list);

	KASSERT((obj->base.read_domains & I915_GEM_GPU_DOMAINS) == 0,
	    ("Object in gpu read domain"));
	KASSERT((obj->base.write_domain & I915_GEM_GPU_DOMAINS) == 0,
	    ("Object in gpu write domain"));

	obj->gtt_offset = obj->gtt_space->start;

	fenceable =
		obj->gtt_space->size == fence_size &&
		(obj->gtt_space->start & (fence_alignment - 1)) == 0;

	mappable =
		obj->gtt_offset + obj->base.size <= dev_priv->mm.gtt_mappable_end;

	obj->map_and_fenceable = mappable && fenceable;

	CTR4(KTR_DRM, "object_bind %p %x %x %d", obj, obj->gtt_offset,
	    obj->base.size, map_and_fenceable);
	return 0;
}

void
i915_gem_clflush_object(struct drm_i915_gem_object *obj)
{
	/* If we don't have a page list set up, then we're not pinned
	 * to GPU, and we can ignore the cache flush because it'll happen
	 * again at bind time.
	 */
	if (obj->pages == NULL)
		return;

	/* If the GPU is snooping the contents of the CPU cache,
	 * we do not need to manually clear the CPU cache lines.  However,
	 * the caches are only snooped when the render cache is
	 * flushed/invalidated.  As we always have to emit invalidations
	 * and flushes when moving into and out of the RENDER domain, correct
	 * snooping behaviour occurs naturally as the result of our domain
	 * tracking.
	 */
	if (obj->cache_level != I915_CACHE_NONE)
		return;

	CTR1(KTR_DRM, "object_clflush %p", obj);

	drm_clflush_pages(obj->pages, obj->base.size / PAGE_SIZE);
}

/** Flushes the GTT write domain for the object if it's dirty. */
static void
i915_gem_object_flush_gtt_write_domain(struct drm_i915_gem_object *obj)
{
	uint32_t old_write_domain;

	if (obj->base.write_domain != I915_GEM_DOMAIN_GTT)
		return;

	/* No actual flushing is required for the GTT write domain.  Writes
	 * to it immediately go to main memory as far as we know, so there's
	 * no chipset flush.  It also doesn't land in render cache.
	 *
	 * However, we do have to enforce the order so that all writes through
	 * the GTT land before any writes to the device, such as updates to
	 * the GATT itself.
	 */
	wmb();

	old_write_domain = obj->base.write_domain;
	obj->base.write_domain = 0;

	CTR3(KTR_DRM, "object_change_domain flush gtt_write %p %x %x", obj,
	    obj->base.read_domains, old_write_domain);
}

/** Flushes the CPU write domain for the object if it's dirty. */
static void
i915_gem_object_flush_cpu_write_domain(struct drm_i915_gem_object *obj)
{
	uint32_t old_write_domain;

	if (obj->base.write_domain != I915_GEM_DOMAIN_CPU)
		return;

	i915_gem_clflush_object(obj);
	intel_gtt_chipset_flush();
	old_write_domain = obj->base.write_domain;
	obj->base.write_domain = 0;

	CTR3(KTR_DRM, "object_change_domain flush_cpu_write %p %x %x", obj,
	    obj->base.read_domains, old_write_domain);
}

static int
i915_gem_object_flush_gpu_write_domain(struct drm_i915_gem_object *obj)
{

	if ((obj->base.write_domain & I915_GEM_GPU_DOMAINS) == 0)
		return (0);
	return (i915_gem_flush_ring(obj->ring, 0, obj->base.write_domain));
}

/**
 * Moves a single object to the GTT read, and possibly write domain.
 *
 * This function returns when the move is complete, including waiting on
 * flushes to occur.
 */
int
i915_gem_object_set_to_gtt_domain(struct drm_i915_gem_object *obj, bool write)
{
	drm_i915_private_t *dev_priv = obj->base.dev->dev_private;
	uint32_t old_write_domain, old_read_domains;
	int ret;

	/* Not valid to be called on unbound objects. */
	if (obj->gtt_space == NULL)
		return -EINVAL;

	if (obj->base.write_domain == I915_GEM_DOMAIN_GTT)
		return 0;

	ret = i915_gem_object_flush_gpu_write_domain(obj);
	if (ret)
		return ret;

	if (obj->pending_gpu_write || write) {
		ret = i915_gem_object_wait_rendering(obj);
		if (ret)
			return (ret);
	}

	i915_gem_object_flush_cpu_write_domain(obj);

	old_write_domain = obj->base.write_domain;
	old_read_domains = obj->base.read_domains;

	/* It should now be out of any other write domains, and we can update
	 * the domain values for our changes.
	 */
	KASSERT((obj->base.write_domain & ~I915_GEM_DOMAIN_GTT) == 0,
	    ("In GTT write domain"));
	obj->base.read_domains |= I915_GEM_DOMAIN_GTT;
	if (write) {
		obj->base.read_domains = I915_GEM_DOMAIN_GTT;
		obj->base.write_domain = I915_GEM_DOMAIN_GTT;
		obj->dirty = 1;
	}

	CTR3(KTR_DRM, "object_change_domain set_to_gtt %p %x %x", obj,
	    old_read_domains, old_write_domain);

	/* And bump the LRU for this access */
	if (i915_gem_object_is_inactive(obj))
		list_move_tail(&obj->mm_list, &dev_priv->mm.inactive_list);

	return 0;
}

int i915_gem_object_set_cache_level(struct drm_i915_gem_object *obj,
				    enum i915_cache_level cache_level)
{
	struct drm_device *dev = obj->base.dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret;

	if (obj->cache_level == cache_level)
		return 0;

	if (obj->pin_count) {
		DRM_DEBUG("can not change the cache level of pinned objects\n");
		return -EBUSY;
	}

	if (obj->gtt_space) {
		ret = i915_gem_object_finish_gpu(obj);
		if (ret)
			return ret;

		i915_gem_object_finish_gtt(obj);

		/* Before SandyBridge, you could not use tiling or fence
		 * registers with snooped memory, so relinquish any fences
		 * currently pointing to our region in the aperture.
		 */
		if (INTEL_INFO(obj->base.dev)->gen < 6) {
			ret = i915_gem_object_put_fence(obj);
			if (ret)
				return ret;
		}

		if (obj->has_global_gtt_mapping)
			i915_gem_gtt_bind_object(obj, cache_level);
		if (obj->has_aliasing_ppgtt_mapping)
			i915_ppgtt_bind_object(dev_priv->mm.aliasing_ppgtt,
					       obj, cache_level);
	}

	if (cache_level == I915_CACHE_NONE) {
		u32 old_read_domains, old_write_domain;

		/* If we're coming from LLC cached, then we haven't
		 * actually been tracking whether the data is in the
		 * CPU cache or not, since we only allow one bit set
		 * in obj->write_domain and have been skipping the clflushes.
		 * Just set it to the CPU cache for now.
		 */
		KASSERT((obj->base.write_domain & ~I915_GEM_DOMAIN_CPU) == 0,
		    ("obj %p in CPU write domain", obj));
		KASSERT((obj->base.read_domains & ~I915_GEM_DOMAIN_CPU) == 0,
		    ("obj %p in CPU read domain", obj));

		old_read_domains = obj->base.read_domains;
		old_write_domain = obj->base.write_domain;

		obj->base.read_domains = I915_GEM_DOMAIN_CPU;
		obj->base.write_domain = I915_GEM_DOMAIN_CPU;

		CTR3(KTR_DRM, "object_change_domain set_cache_level %p %x %x",
		    obj, old_read_domains, old_write_domain);
	}

	obj->cache_level = cache_level;
	return 0;
}

static bool is_pin_display(struct drm_i915_gem_object *obj)
{
	/* There are 3 sources that pin objects:
	 *   1. The display engine (scanouts, sprites, cursors);
	 *   2. Reservations for execbuffer;
	 *   3. The user.
	 *
	 * We can ignore reservations as we hold the struct_mutex and
	 * are only called outside of the reservation path.  The user
	 * can only increment pin_count once, and so if after
	 * subtracting the potential reference by the user, any pin_count
	 * remains, it must be due to another use by the display engine.
	 */
	return obj->pin_count - !!obj->user_pin_count;
}

int
i915_gem_object_pin_to_display_plane(struct drm_i915_gem_object *obj,
				     u32 alignment,
				     struct intel_ring_buffer *pipelined)
{
	u32 old_read_domains, old_write_domain;
	int ret;

	ret = i915_gem_object_flush_gpu_write_domain(obj);
	if (ret)
		return ret;

	if (pipelined != obj->ring) {
		ret = i915_gem_object_sync(obj, pipelined);
		if (ret)
			return ret;
	}

	/* Mark the pin_display early so that we account for the
	 * display coherency whilst setting up the cache domains.
	 */
	obj->pin_display = true;

	/* The display engine is not coherent with the LLC cache on gen6.  As
	 * a result, we make sure that the pinning that is about to occur is
	 * done with uncached PTEs. This is lowest common denominator for all
	 * chipsets.
	 *
	 * However for gen6+, we could do better by using the GFDT bit instead
	 * of uncaching, which would allow us to flush all the LLC-cached data
	 * with that bit in the PTE to main memory with just one PIPE_CONTROL.
	 */
	ret = i915_gem_object_set_cache_level(obj, I915_CACHE_NONE);
	if (ret)
		goto err_unpin_display;

	/* As the user may map the buffer once pinned in the display plane
	 * (e.g. libkms for the bootup splash), we have to ensure that we
	 * always use map_and_fenceable for all scanout buffers.
	 */
	ret = i915_gem_object_pin(obj, alignment, true);
	if (ret)
		goto err_unpin_display;

	i915_gem_object_flush_cpu_write_domain(obj);

	old_write_domain = obj->base.write_domain;
	old_read_domains = obj->base.read_domains;

	KASSERT((obj->base.write_domain & ~I915_GEM_DOMAIN_GTT) == 0,
	    ("obj %p in GTT write domain", obj));
	obj->base.read_domains |= I915_GEM_DOMAIN_GTT;

	CTR3(KTR_DRM, "object_change_domain pin_to_display_plan %p %x %x",
	    obj, old_read_domains, obj->base.write_domain);

	return 0;

err_unpin_display:
	obj->pin_display = is_pin_display(obj);
	return ret;
}

void
i915_gem_object_unpin_from_display_plane(struct drm_i915_gem_object *obj)
{
	i915_gem_object_unpin(obj);
	obj->pin_display = is_pin_display(obj);
}

int
i915_gem_object_finish_gpu(struct drm_i915_gem_object *obj)
{
	int ret;

	if ((obj->base.read_domains & I915_GEM_GPU_DOMAINS) == 0)
		return 0;

	if (obj->base.write_domain & I915_GEM_GPU_DOMAINS) {
		ret = i915_gem_flush_ring(obj->ring, 0, obj->base.write_domain);
		if (ret)
			return ret;
	}

	ret = i915_gem_object_wait_rendering(obj);
	if (ret)
		return ret;

	/* Ensure that we invalidate the GPU's caches and TLBs. */
	obj->base.read_domains &= ~I915_GEM_GPU_DOMAINS;
	return 0;
}

/**
 * Moves a single object to the CPU read, and possibly write domain.
 *
 * This function returns when the move is complete, including waiting on
 * flushes to occur.
 */
int
i915_gem_object_set_to_cpu_domain(struct drm_i915_gem_object *obj, bool write)
{
	uint32_t old_write_domain, old_read_domains;
	int ret;

	if (obj->base.write_domain == I915_GEM_DOMAIN_CPU)
		return 0;

	ret = i915_gem_object_flush_gpu_write_domain(obj);
	if (ret)
		return ret;

	if (write || obj->pending_gpu_write) {
		ret = i915_gem_object_wait_rendering(obj);
		if (ret)
			return ret;
	}

	i915_gem_object_flush_gtt_write_domain(obj);

	old_write_domain = obj->base.write_domain;
	old_read_domains = obj->base.read_domains;

	/* Flush the CPU cache if it's still invalid. */
	if ((obj->base.read_domains & I915_GEM_DOMAIN_CPU) == 0) {
		i915_gem_clflush_object(obj);

		obj->base.read_domains |= I915_GEM_DOMAIN_CPU;
	}

	/* It should now be out of any other write domains, and we can update
	 * the domain values for our changes.
	 */
	KASSERT((obj->base.write_domain & ~I915_GEM_DOMAIN_CPU) == 0,
	    ("In cpu write domain"));

	/* If we're writing through the CPU, then the GPU read domains will
	 * need to be invalidated at next use.
	 */
	if (write) {
		obj->base.read_domains = I915_GEM_DOMAIN_CPU;
		obj->base.write_domain = I915_GEM_DOMAIN_CPU;
	}

	CTR3(KTR_DRM, "object_change_domain set_to_cpu %p %x %x", obj,
	    old_read_domains, old_write_domain);

	return 0;
}

/* Throttle our rendering by waiting until the ring has completed our requests
 * emitted over 20 msec ago.
 *
 * Note that if we were to use the current jiffies each time around the loop,
 * we wouldn't escape the function with any frames outstanding if the time to
 * render a frame was over 20ms.
 *
 * This should get us reasonable parallelism between CPU and GPU but also
 * relatively low latency when blocking on a particular request to finish.
 */
static int
i915_gem_ring_throttle(struct drm_device *dev, struct drm_file *file)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_file_private *file_priv = file->driver_priv;
	unsigned long recent_enough = ticks - (20 * hz / 1000);
	struct drm_i915_gem_request *request;
	struct intel_ring_buffer *ring = NULL;
	u32 seqno = 0;
	int ret;

	if (atomic_load_acq_int(&dev_priv->mm.wedged))
		return -EIO;

	mtx_lock(&file_priv->mm.lck);
	list_for_each_entry(request, &file_priv->mm.request_list, client_list) {
		if (time_after_eq(request->emitted_jiffies, recent_enough))
			break;
		ring = request->ring;
		seqno = request->seqno;
	}
	mtx_unlock(&file_priv->mm.lck);
	if (seqno == 0)
		return 0;

	ret = __wait_seqno(ring, seqno, true);
	if (ret == 0)
		taskqueue_enqueue_timeout(dev_priv->tq,
		    &dev_priv->mm.retire_task, 0);

	return ret;
}

int
i915_gem_object_pin(struct drm_i915_gem_object *obj,
		    uint32_t alignment,
		    bool map_and_fenceable)
{
	int ret;

	if (obj->pin_count == DRM_I915_GEM_OBJECT_MAX_PIN_COUNT)
		return -EBUSY;

	if (obj->gtt_space != NULL) {
		if ((alignment && obj->gtt_offset & (alignment - 1)) ||
		    (map_and_fenceable && !obj->map_and_fenceable)) {
			DRM_DEBUG("bo is already pinned with incorrect alignment:"
			     " offset=%x, req.alignment=%x, req.map_and_fenceable=%d,"
			     " obj->map_and_fenceable=%d\n",
			     obj->gtt_offset, alignment,
			     map_and_fenceable,
			     obj->map_and_fenceable);
			ret = i915_gem_object_unbind(obj);
			if (ret)
				return ret;
		}
	}

	if (obj->gtt_space == NULL) {
		ret = i915_gem_object_bind_to_gtt(obj, alignment,
						  map_and_fenceable);
		if (ret)
			return ret;
	}

	if (!obj->has_global_gtt_mapping && map_and_fenceable)
		i915_gem_gtt_bind_object(obj, obj->cache_level);

	obj->pin_count++;
	obj->pin_mappable |= map_and_fenceable;

	return 0;
}

void
i915_gem_object_unpin(struct drm_i915_gem_object *obj)
{

	KASSERT(obj->pin_count != 0, ("zero pin count"));
	KASSERT(obj->gtt_space != NULL, ("No gtt mapping"));

	if (--obj->pin_count == 0)
		obj->pin_mappable = false;
}

int
i915_gem_pin_ioctl(struct drm_device *dev, void *data,
		   struct drm_file *file)
{
	struct drm_i915_gem_pin *args = data;
	struct drm_i915_gem_object *obj;
	struct drm_gem_object *gobj;
	int ret;

	ret = i915_mutex_lock_interruptible(dev);
	if (ret)
		return ret;

	gobj = drm_gem_object_lookup(dev, file, args->handle);
	if (gobj == NULL) {
		ret = -ENOENT;
		goto unlock;
	}
	obj = to_intel_bo(gobj);

	if (obj->madv != I915_MADV_WILLNEED) {
		DRM_ERROR("Attempting to pin a purgeable buffer\n");
		ret = -EINVAL;
		goto out;
	}

	if (obj->pin_filp != NULL && obj->pin_filp != file) {
		DRM_ERROR("Already pinned in i915_gem_pin_ioctl(): %d\n",
			  args->handle);
		ret = -EINVAL;
		goto out;
	}

	obj->user_pin_count++;
	obj->pin_filp = file;
	if (obj->user_pin_count == 1) {
		ret = i915_gem_object_pin(obj, args->alignment, true);
		if (ret)
			goto out;
	}

	/* XXX - flush the CPU caches for pinned objects
	 * as the X server doesn't manage domains yet
	 */
	i915_gem_object_flush_cpu_write_domain(obj);
	args->offset = obj->gtt_offset;
out:
	drm_gem_object_unreference(&obj->base);
unlock:
	DRM_UNLOCK(dev);
	return ret;
}

int
i915_gem_unpin_ioctl(struct drm_device *dev, void *data,
		     struct drm_file *file)
{
	struct drm_i915_gem_pin *args = data;
	struct drm_i915_gem_object *obj;
	int ret;

	ret = i915_mutex_lock_interruptible(dev);
	if (ret)
		return ret;

	obj = to_intel_bo(drm_gem_object_lookup(dev, file, args->handle));
	if (&obj->base == NULL) {
		ret = -ENOENT;
		goto unlock;
	}

	if (obj->pin_filp != file) {
		DRM_ERROR("Not pinned by caller in i915_gem_pin_ioctl(): %d\n",
			  args->handle);
		ret = -EINVAL;
		goto out;
	}
	obj->user_pin_count--;
	if (obj->user_pin_count == 0) {
		obj->pin_filp = NULL;
		i915_gem_object_unpin(obj);
	}

out:
	drm_gem_object_unreference(&obj->base);
unlock:
	DRM_UNLOCK(dev);
	return ret;
}

int
i915_gem_busy_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file)
{
	struct drm_i915_gem_busy *args = data;
	struct drm_i915_gem_object *obj;
	int ret;

	ret = i915_mutex_lock_interruptible(dev);
	if (ret)
		return ret;

	obj = to_intel_bo(drm_gem_object_lookup(dev, file, args->handle));
	if (&obj->base == NULL) {
		ret = -ENOENT;
		goto unlock;
	}

	args->busy = obj->active;
	if (args->busy) {
		if (obj->base.write_domain & I915_GEM_GPU_DOMAINS) {
			ret = i915_gem_flush_ring(obj->ring,
			    0, obj->base.write_domain);
		} else {
			ret = i915_gem_check_olr(obj->ring,
						 obj->last_rendering_seqno);
		}

		i915_gem_retire_requests_ring(obj->ring);
		args->busy = obj->active;
	}

	drm_gem_object_unreference(&obj->base);
unlock:
	DRM_UNLOCK(dev);
	return ret;
}

int
i915_gem_throttle_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	return i915_gem_ring_throttle(dev, file_priv);
}

int
i915_gem_madvise_ioctl(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct drm_i915_gem_madvise *args = data;
	struct drm_i915_gem_object *obj;
	int ret;

	switch (args->madv) {
	case I915_MADV_DONTNEED:
	case I915_MADV_WILLNEED:
	    break;
	default:
	    return -EINVAL;
	}

	ret = i915_mutex_lock_interruptible(dev);
	if (ret)
		return ret;

	obj = to_intel_bo(drm_gem_object_lookup(dev, file_priv, args->handle));
	if (&obj->base == NULL) {
		ret = -ENOENT;
		goto unlock;
	}

	if (obj->pin_count) {
		ret = -EINVAL;
		goto out;
	}

	if (obj->madv != I915_MADV_PURGED_INTERNAL)
		obj->madv = args->madv;

	/* if the object is no longer attached, discard its backing storage */
	if (i915_gem_object_is_purgeable(obj) && obj->gtt_space == NULL)
		i915_gem_object_truncate(obj);

	args->retained = obj->madv != I915_MADV_PURGED_INTERNAL;

out:
	drm_gem_object_unreference(&obj->base);
unlock:
	DRM_UNLOCK(dev);
	return ret;
}

struct drm_i915_gem_object *i915_gem_alloc_object(struct drm_device *dev,
						  size_t size)
{
	struct drm_i915_private *dev_priv;
	struct drm_i915_gem_object *obj;

	dev_priv = dev->dev_private;

	obj = malloc(sizeof(*obj), DRM_I915_GEM, M_WAITOK | M_ZERO);

	if (drm_gem_object_init(dev, &obj->base, size) != 0) {
		free(obj, DRM_I915_GEM);
		return NULL;
	}

	obj->base.write_domain = I915_GEM_DOMAIN_CPU;
	obj->base.read_domains = I915_GEM_DOMAIN_CPU;

	if (HAS_LLC(dev)) {
		/* On some devices, we can have the GPU use the LLC (the CPU
		 * cache) for about a 10% performance improvement
		 * compared to uncached.  Graphics requests other than
		 * display scanout are coherent with the CPU in
		 * accessing this cache.  This means in this mode we
		 * don't need to clflush on the CPU side, and on the
		 * GPU side we only need to flush internal caches to
		 * get data visible to the CPU.
		 *
		 * However, we maintain the display planes as UC, and so
		 * need to rebind when first used as such.
		 */
		obj->cache_level = I915_CACHE_LLC;
	} else
		obj->cache_level = I915_CACHE_NONE;
	obj->base.driver_private = NULL;
	obj->fence_reg = I915_FENCE_REG_NONE;
	INIT_LIST_HEAD(&obj->mm_list);
	INIT_LIST_HEAD(&obj->gtt_list);
	INIT_LIST_HEAD(&obj->ring_list);
	INIT_LIST_HEAD(&obj->exec_list);
	INIT_LIST_HEAD(&obj->gpu_write_list);
	obj->madv = I915_MADV_WILLNEED;
	/* Avoid an unnecessary call to unbind on the first bind. */
	obj->map_and_fenceable = true;

	i915_gem_info_add_obj(dev_priv, size);

	return obj;
}

int i915_gem_init_object(struct drm_gem_object *obj)
{
	printf("i915_gem_init_object called\n");

	return 0;
}

void i915_gem_free_object(struct drm_gem_object *gem_obj)
{
	struct drm_i915_gem_object *obj = to_intel_bo(gem_obj);
	struct drm_device *dev = obj->base.dev;
	drm_i915_private_t *dev_priv = dev->dev_private;

	CTR1(KTR_DRM, "object_destroy_tail %p", obj);

	if (obj->phys_obj)
		i915_gem_detach_phys_object(dev, obj);

	obj->pin_count = 0;
	if (i915_gem_object_unbind(obj) == -ERESTARTSYS) {
		bool was_interruptible;

		was_interruptible = dev_priv->mm.interruptible;
		dev_priv->mm.interruptible = false;

		if (i915_gem_object_unbind(obj))
			printf("i915_gem_free_object: unbind\n");

		dev_priv->mm.interruptible = was_interruptible;
	}

	drm_gem_free_mmap_offset(&obj->base);
	drm_gem_object_release(&obj->base);
	i915_gem_info_remove_obj(dev_priv, obj->base.size);

	free(obj->bit_17, DRM_I915_GEM);
	free(obj, DRM_I915_GEM);
}

int
i915_gem_idle(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret;

	DRM_LOCK(dev);

	if (dev_priv->mm.suspended) {
		DRM_UNLOCK(dev);
		return 0;
	}

	ret = i915_gpu_idle(dev);
	if (ret) {
		DRM_UNLOCK(dev);
		return ret;
	}
	i915_gem_retire_requests(dev);

	/* Under UMS, be paranoid and evict. */
	if (!drm_core_check_feature(dev, DRIVER_MODESET)) {
		ret = i915_gem_evict_everything(dev, false);
		if (ret) {
			DRM_UNLOCK(dev);
			return ret;
		}
	}

	i915_gem_reset_fences(dev);

	/* Hack!  Don't let anybody do execbuf while we don't control the chip.
	 * We need to replace this with a semaphore, or something.
	 * And not confound mm.suspended!
	 */
	dev_priv->mm.suspended = 1;
	callout_stop(&dev_priv->hangcheck_timer);

	i915_kernel_lost_context(dev);
	i915_gem_cleanup_ringbuffer(dev);

	DRM_UNLOCK(dev);

	/* Cancel the retire work handler, which should be idle now. */
	taskqueue_cancel_timeout(dev_priv->tq, &dev_priv->mm.retire_task, NULL);

	return ret;
}

void i915_gem_init_swizzling(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	if (INTEL_INFO(dev)->gen < 5 ||
	    dev_priv->mm.bit_6_swizzle_x == I915_BIT_6_SWIZZLE_NONE)
		return;

	I915_WRITE(DISP_ARB_CTL, I915_READ(DISP_ARB_CTL) |
				 DISP_TILE_SURFACE_SWIZZLING);

	if (IS_GEN5(dev))
		return;

	I915_WRITE(TILECTL, I915_READ(TILECTL) | TILECTL_SWZCTL);
	if (IS_GEN6(dev))
		I915_WRITE(ARB_MODE, _MASKED_BIT_ENABLE(ARB_MODE_SWIZZLE_SNB));
	else
		I915_WRITE(ARB_MODE, _MASKED_BIT_ENABLE(ARB_MODE_SWIZZLE_IVB));
}

int
i915_gem_init_hw(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret;

	i915_gem_init_swizzling(dev);

	ret = intel_init_render_ring_buffer(dev);
	if (ret)
		return ret;

	if (HAS_BSD(dev)) {
		ret = intel_init_bsd_ring_buffer(dev);
		if (ret)
			goto cleanup_render_ring;
	}

	if (HAS_BLT(dev)) {
		ret = intel_init_blt_ring_buffer(dev);
		if (ret)
			goto cleanup_bsd_ring;
	}

	dev_priv->next_seqno = 1;

	/*
	 * XXX: There was some w/a described somewhere suggesting loading
	 * contexts before PPGTT.
	 */
	i915_gem_context_init(dev);
	i915_gem_init_ppgtt(dev);

	return 0;

cleanup_bsd_ring:
	intel_cleanup_ring_buffer(&dev_priv->rings[VCS]);
cleanup_render_ring:
	intel_cleanup_ring_buffer(&dev_priv->rings[RCS]);
	return ret;
}

static bool
intel_enable_ppgtt(struct drm_device *dev)
{
	if (i915_enable_ppgtt >= 0)
		return i915_enable_ppgtt;

	/* Disable ppgtt on SNB if VT-d is on. */
	if (INTEL_INFO(dev)->gen == 6 && intel_iommu_enabled)
		return false;

	return true;
}

int i915_gem_init(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	unsigned long gtt_size, mappable_size;
	int ret;

	gtt_size = dev_priv->mm.gtt.gtt_total_entries << PAGE_SHIFT;
	mappable_size = dev_priv->mm.gtt.gtt_mappable_entries << PAGE_SHIFT;

	DRM_LOCK(dev);
	if (intel_enable_ppgtt(dev) && HAS_ALIASING_PPGTT(dev)) {
		/* PPGTT pdes are stolen from global gtt ptes, so shrink the
		 * aperture accordingly when using aliasing ppgtt. */
		gtt_size -= I915_PPGTT_PD_ENTRIES*PAGE_SIZE;

		i915_gem_init_global_gtt(dev, 0, mappable_size, gtt_size);

		ret = i915_gem_init_aliasing_ppgtt(dev);
		if (ret) {
			DRM_UNLOCK(dev);
			return ret;
		}
	} else {
		/* Let GEM Manage all of the aperture.
		 *
		 * However, leave one page at the end still bound to the scratch
		 * page.  There are a number of places where the hardware
		 * apparently prefetches past the end of the object, and we've
		 * seen multiple hangs with the GPU head pointer stuck in a
		 * batchbuffer bound at the last page of the aperture.  One page
		 * should be enough to keep any prefetching inside of the
		 * aperture.
		 */
		i915_gem_init_global_gtt(dev, 0, mappable_size,
					 gtt_size);
	}

	ret = i915_gem_init_hw(dev);
	DRM_UNLOCK(dev);
	if (ret) {
		i915_gem_cleanup_aliasing_ppgtt(dev);
		return ret;
	}

	/* Allow hardware batchbuffers unless told otherwise, but not for KMS. */
	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		dev_priv->dri1.allow_batchbuffer = 1;
	return 0;
}

void
i915_gem_cleanup_ringbuffer(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring;
	int i;

	for_each_ring(ring, dev_priv, i)
		intel_cleanup_ring_buffer(ring);
}

int
i915_gem_entervt_ioctl(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret;

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		return 0;

	if (atomic_load_acq_int(&dev_priv->mm.wedged) != 0) {
		DRM_ERROR("Reenabling wedged hardware, good luck\n");
		atomic_store_rel_int(&dev_priv->mm.wedged, 0);
	}

	DRM_LOCK(dev);
	dev_priv->mm.suspended = 0;

	ret = i915_gem_init_hw(dev);
	if (ret != 0) {
		DRM_UNLOCK(dev);
		return ret;
	}

	KASSERT(list_empty(&dev_priv->mm.active_list), ("active list"));
	KASSERT(list_empty(&dev_priv->mm.flushing_list), ("flushing list"));
	KASSERT(list_empty(&dev_priv->mm.inactive_list), ("inactive list"));
	DRM_UNLOCK(dev);

	ret = drm_irq_install(dev);
	if (ret)
		goto cleanup_ringbuffer;

	return 0;

cleanup_ringbuffer:
	DRM_LOCK(dev);
	i915_gem_cleanup_ringbuffer(dev);
	dev_priv->mm.suspended = 1;
	DRM_UNLOCK(dev);

	return ret;
}

int
i915_gem_leavevt_ioctl(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	if (drm_core_check_feature(dev, DRIVER_MODESET))
		return 0;

	drm_irq_uninstall(dev);
	return i915_gem_idle(dev);
}

void
i915_gem_lastclose(struct drm_device *dev)
{
	int ret;

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		return;

	ret = i915_gem_idle(dev);
	if (ret)
		DRM_ERROR("failed to idle hardware: %d\n", ret);
}

static void
init_ring_lists(struct intel_ring_buffer *ring)
{
	INIT_LIST_HEAD(&ring->active_list);
	INIT_LIST_HEAD(&ring->request_list);
	INIT_LIST_HEAD(&ring->gpu_write_list);
}

void
i915_gem_load(struct drm_device *dev)
{
	int i;
	drm_i915_private_t *dev_priv = dev->dev_private;

	INIT_LIST_HEAD(&dev_priv->mm.active_list);
	INIT_LIST_HEAD(&dev_priv->mm.flushing_list);
	INIT_LIST_HEAD(&dev_priv->mm.inactive_list);
	INIT_LIST_HEAD(&dev_priv->mm.fence_list);
	INIT_LIST_HEAD(&dev_priv->mm.gtt_list);
	for (i = 0; i < I915_NUM_RINGS; i++)
		init_ring_lists(&dev_priv->rings[i]);
	for (i = 0; i < I915_MAX_NUM_FENCES; i++)
		INIT_LIST_HEAD(&dev_priv->fence_regs[i].lru_list);
	TIMEOUT_TASK_INIT(dev_priv->tq, &dev_priv->mm.retire_task, 0,
	    i915_gem_retire_task_handler, dev_priv);
	dev_priv->error_completion = 0;

	/* On GEN3 we really need to make sure the ARB C3 LP bit is set */
	if (IS_GEN3(dev)) {
		I915_WRITE(MI_ARB_STATE,
			   _MASKED_BIT_ENABLE(MI_ARB_C3_LP_WRITE_ENABLE));
	}

	dev_priv->relative_constants_mode = I915_EXEC_CONSTANTS_REL_GENERAL;

	/* Old X drivers will take 0-2 for front, back, depth buffers */
	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		dev_priv->fence_reg_start = 3;

	if (INTEL_INFO(dev)->gen >= 4 || IS_I945G(dev) || IS_I945GM(dev) || IS_G33(dev))
		dev_priv->num_fence_regs = 16;
	else
		dev_priv->num_fence_regs = 8;

	/* Initialize fence registers to zero */
	i915_gem_reset_fences(dev);

	i915_gem_detect_bit_6_swizzle(dev);
	dev_priv->mm.interruptible = true;

	dev_priv->mm.i915_lowmem = EVENTHANDLER_REGISTER(vm_lowmem,
	    i915_gem_lowmem, dev, EVENTHANDLER_PRI_ANY);
}

void
i915_gem_unload(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv;

	dev_priv = dev->dev_private;
	EVENTHANDLER_DEREGISTER(vm_lowmem, dev_priv->mm.i915_lowmem);
}

/*
 * Create a physically contiguous memory object for this object
 * e.g. for cursor + overlay regs
 */
static int i915_gem_init_phys_object(struct drm_device *dev,
				     int id, int size, int align)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_phys_object *phys_obj;
	int ret;

	if (dev_priv->mm.phys_objs[id - 1] || !size)
		return 0;

	phys_obj = malloc(sizeof(struct drm_i915_gem_phys_object),
	    DRM_I915_GEM, M_WAITOK | M_ZERO);

	phys_obj->id = id;

	phys_obj->handle = drm_pci_alloc(dev, size, align, BUS_SPACE_MAXADDR);
	if (!phys_obj->handle) {
		ret = -ENOMEM;
		goto kfree_obj;
	}
	pmap_change_attr((vm_offset_t)phys_obj->handle->vaddr,
	    size / PAGE_SIZE, PAT_WRITE_COMBINING);

	dev_priv->mm.phys_objs[id - 1] = phys_obj;

	return 0;
kfree_obj:
	free(phys_obj, DRM_I915_GEM);
	return ret;
}

static void i915_gem_free_phys_object(struct drm_device *dev, int id)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_phys_object *phys_obj;

	if (!dev_priv->mm.phys_objs[id - 1])
		return;

	phys_obj = dev_priv->mm.phys_objs[id - 1];
	if (phys_obj->cur_obj) {
		i915_gem_detach_phys_object(dev, phys_obj->cur_obj);
	}

	drm_pci_free(dev, phys_obj->handle);
	free(phys_obj, DRM_I915_GEM);
	dev_priv->mm.phys_objs[id - 1] = NULL;
}

void i915_gem_free_all_phys_object(struct drm_device *dev)
{
	int i;

	for (i = I915_GEM_PHYS_CURSOR_0; i <= I915_MAX_PHYS_OBJECT; i++)
		i915_gem_free_phys_object(dev, i);
}

void i915_gem_detach_phys_object(struct drm_device *dev,
				 struct drm_i915_gem_object *obj)
{
	vm_page_t page;
	struct sf_buf *sf;
	char *vaddr, *dst;
	int i, page_count;

	if (!obj->phys_obj)
		return;
	vaddr = obj->phys_obj->handle->vaddr;

	page_count = obj->base.size / PAGE_SIZE;
	VM_OBJECT_WLOCK(obj->base.vm_obj);
	for (i = 0; i < page_count; i++) {
		page = i915_gem_wire_page(obj->base.vm_obj, i, NULL);
		if (page == NULL)
			continue; /* XXX */

		VM_OBJECT_WUNLOCK(obj->base.vm_obj);
		sf = sf_buf_alloc(page, 0);
		if (sf != NULL) {
			dst = (char *)sf_buf_kva(sf);
			memcpy(dst, vaddr + IDX_TO_OFF(i), PAGE_SIZE);
			sf_buf_free(sf);
		}
		drm_clflush_pages(&page, 1);

		VM_OBJECT_WLOCK(obj->base.vm_obj);
		vm_page_reference(page);
		vm_page_lock(page);
		vm_page_dirty(page);
		vm_page_unwire(page, PQ_INACTIVE);
		vm_page_unlock(page);
		atomic_add_long(&i915_gem_wired_pages_cnt, -1);
	}
	VM_OBJECT_WUNLOCK(obj->base.vm_obj);
	intel_gtt_chipset_flush();

	obj->phys_obj->cur_obj = NULL;
	obj->phys_obj = NULL;
}

int
i915_gem_attach_phys_object(struct drm_device *dev,
			    struct drm_i915_gem_object *obj,
			    int id,
			    int align)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	vm_page_t page;
	struct sf_buf *sf;
	char *dst, *src;
	int ret = 0;
	int page_count;
	int i;

	if (id > I915_MAX_PHYS_OBJECT)
		return -EINVAL;

	if (obj->phys_obj) {
		if (obj->phys_obj->id == id)
			return 0;
		i915_gem_detach_phys_object(dev, obj);
	}

	/* create a new object */
	if (!dev_priv->mm.phys_objs[id - 1]) {
		ret = i915_gem_init_phys_object(dev, id,
						obj->base.size, align);
		if (ret) {
			DRM_ERROR("failed to init phys object %d size: %zu\n",
				  id, obj->base.size);
			return ret;
		}
	}

	/* bind to the object */
	obj->phys_obj = dev_priv->mm.phys_objs[id - 1];
	obj->phys_obj->cur_obj = obj;

	page_count = obj->base.size / PAGE_SIZE;

	VM_OBJECT_WLOCK(obj->base.vm_obj);
	for (i = 0; i < page_count; i++) {
		page = i915_gem_wire_page(obj->base.vm_obj, i, NULL);
		if (page == NULL) {
			ret = -EIO;
			break;
		}
		VM_OBJECT_WUNLOCK(obj->base.vm_obj);
		sf = sf_buf_alloc(page, 0);
		src = (char *)sf_buf_kva(sf);
		dst = (char *)obj->phys_obj->handle->vaddr + IDX_TO_OFF(i);
		memcpy(dst, src, PAGE_SIZE);
		sf_buf_free(sf);

		VM_OBJECT_WLOCK(obj->base.vm_obj);

		vm_page_reference(page);
		vm_page_lock(page);
		vm_page_unwire(page, PQ_INACTIVE);
		vm_page_unlock(page);
		atomic_add_long(&i915_gem_wired_pages_cnt, -1);
	}
	VM_OBJECT_WUNLOCK(obj->base.vm_obj);

	return ret;
}

static int
i915_gem_phys_pwrite(struct drm_device *dev,
		     struct drm_i915_gem_object *obj,
		     struct drm_i915_gem_pwrite *args,
		     struct drm_file *file_priv)
{
	void *vaddr = (char *)obj->phys_obj->handle->vaddr + args->offset;
	char __user *user_data = to_user_ptr(args->data_ptr);

	if (__copy_from_user_inatomic_nocache(vaddr, user_data, args->size)) {
		unsigned long unwritten;

		/* The physical object once assigned is fixed for the lifetime
		 * of the obj, so we can safely drop the lock and continue
		 * to access vaddr.
		 */
		DRM_UNLOCK(dev);
		unwritten = copy_from_user(vaddr, user_data, args->size);
		DRM_LOCK(dev);
		if (unwritten)
			return -EFAULT;
	}

	i915_gem_chipset_flush(dev);
	return 0;
}

void i915_gem_release(struct drm_device *dev, struct drm_file *file)
{
	struct drm_i915_file_private *file_priv = file->driver_priv;

	/* Clean up our request list when the client is going away, so that
	 * later retire_requests won't dereference our soon-to-be-gone
	 * file_priv.
	 */
	mtx_lock(&file_priv->mm.lck);
	while (!list_empty(&file_priv->mm.request_list)) {
		struct drm_i915_gem_request *request;

		request = list_first_entry(&file_priv->mm.request_list,
					   struct drm_i915_gem_request,
					   client_list);
		list_del(&request->client_list);
		request->file_priv = NULL;
	}
	mtx_unlock(&file_priv->mm.lck);
}

static vm_page_t
i915_gem_wire_page(vm_object_t object, vm_pindex_t pindex, bool *fresh)
{
	vm_page_t page;
	int rv;

	VM_OBJECT_ASSERT_WLOCKED(object);
	page = vm_page_grab(object, pindex, VM_ALLOC_NORMAL);
	if (page->valid != VM_PAGE_BITS_ALL) {
		if (vm_pager_has_page(object, pindex, NULL, NULL)) {
			rv = vm_pager_get_pages(object, &page, 1, 0);
			if (rv != VM_PAGER_OK) {
				vm_page_lock(page);
				vm_page_free(page);
				vm_page_unlock(page);
				return (NULL);
			}
			if (fresh != NULL)
				*fresh = true;
		} else {
			pmap_zero_page(page);
			page->valid = VM_PAGE_BITS_ALL;
			page->dirty = 0;
			if (fresh != NULL)
				*fresh = false;
		}
	} else if (fresh != NULL) {
		*fresh = false;
	}
	vm_page_lock(page);
	vm_page_wire(page);
	vm_page_unlock(page);
	vm_page_xunbusy(page);
	atomic_add_long(&i915_gem_wired_pages_cnt, 1);
	return (page);
}

#undef __user
#undef __force
#undef __iomem
#undef __must_check
#undef to_user_ptr
#undef offset_in_page
#undef page_to_phys
