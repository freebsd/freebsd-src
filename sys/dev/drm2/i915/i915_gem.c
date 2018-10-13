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
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
#include <dev/drm2/i915/i915_drm.h>
#include <dev/drm2/i915/i915_drv.h>
#include <dev/drm2/i915/intel_drv.h>

#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/sf_buf.h>

#include <vm/vm.h>
#include <vm/vm_pageout.h>

#include <machine/md_var.h>

static void i915_gem_object_flush_gtt_write_domain(struct drm_i915_gem_object *obj);
static void i915_gem_object_flush_cpu_write_domain(struct drm_i915_gem_object *obj);
static __must_check int i915_gem_object_bind_to_gtt(struct drm_i915_gem_object *obj,
						    unsigned alignment,
						    bool map_and_fenceable,
						    bool nonblocking);
static int i915_gem_phys_pwrite(struct drm_device *dev,
				struct drm_i915_gem_object *obj,
				struct drm_i915_gem_pwrite *args,
				struct drm_file *file);

static void i915_gem_write_fence(struct drm_device *dev, int reg,
				 struct drm_i915_gem_object *obj);
static void i915_gem_object_update_fence(struct drm_i915_gem_object *obj,
					 struct drm_i915_fence_reg *fence,
					 bool enable);

static void i915_gem_inactive_shrink(void *);
static long i915_gem_purge(struct drm_i915_private *dev_priv, long target);
static void i915_gem_shrink_all(struct drm_i915_private *dev_priv);
static void i915_gem_object_truncate(struct drm_i915_gem_object *obj);

static int i915_gem_object_get_pages_range(struct drm_i915_gem_object *obj,
    off_t start, off_t end);

static vm_page_t i915_gem_wire_page(vm_object_t object, vm_pindex_t pindex,
    bool *fresh);

MALLOC_DEFINE(DRM_I915_GEM, "i915gem", "Allocations from i915 gem");
long i915_gem_wired_pages_cnt;

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
	struct completion *x = &dev_priv->error_completion;
	int ret;

	if (!atomic_read(&dev_priv->mm.wedged))
		return 0;

	/*
	 * Only wait 10 seconds for the gpu reset to complete to avoid hanging
	 * userspace. If it takes that long something really bad is going on and
	 * we should simply try to bail out and fail as gracefully as possible.
	 */
	ret = wait_for_completion_interruptible_timeout(x, 10*HZ);
	if (ret == 0) {
		DRM_ERROR("Timed out waiting for the gpu reset to complete\n");
		return -EIO;
	} else if (ret < 0) {
		return ret;
	}

	if (atomic_read(&dev_priv->mm.wedged)) {
		/* GPU is hung, bump the completion count to account for
		 * the token we just consumed so that we never hit zero and
		 * end up waiting upon a subsequent completion event that
		 * will never happen.
		 */
		mtx_lock(&x->lock);
		x->done++;
		mtx_unlock(&x->lock);
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
	ret = sx_xlock_sig(&dev->dev_struct_lock);
	if (ret)
		return -EINTR;

	WARN_ON(i915_verify_lists(dev));
	return 0;
}

static inline bool
i915_gem_object_is_inactive(struct drm_i915_gem_object *obj)
{
	return obj->gtt_space && !obj->active;
}

int
i915_gem_init_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file)
{
	struct drm_i915_gem_init *args = data;

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		return -ENODEV;

	if (args->gtt_start >= args->gtt_end ||
	    (args->gtt_end | args->gtt_start) & (PAGE_SIZE - 1))
		return -EINVAL;

	/* GEM with user mode setting was never supported on ilk and later. */
	if (INTEL_INFO(dev)->gen >= 5)
		return -ENODEV;

	/*
	 * XXXKIB. The second-time initialization should be guarded
	 * against.
	 */
	DRM_LOCK(dev);
	i915_gem_init_global_gtt(dev, args->gtt_start,
				 args->gtt_end, args->gtt_end);
	DRM_UNLOCK(dev);

	return 0;
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
	list_for_each_entry(obj, &dev_priv->mm.bound_list, gtt_list)
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
		start = round_down(start, 128);
		end = round_up(end, 128);

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
	ssize_t remain;
	off_t offset;
	int shmem_page_offset, page_length, ret = 0;
	int obj_do_bit17_swizzling, page_do_bit17_swizzling;
	int hit_slowpath = 0;
	int prefaulted = 0;
	int needs_clflush = 0;

	user_data = to_user_ptr(args->data_ptr);
	remain = args->size;

	obj_do_bit17_swizzling = i915_gem_object_needs_bit17_swizzle(obj);

	if (!(obj->base.read_domains & I915_GEM_DOMAIN_CPU)) {
		/* If we're not in the cpu read domain, set ourself into the gtt
		 * read domain and manually flush cachelines (if required). This
		 * optimizes for the case when the gpu will dirty the data
		 * anyway again before the next pread happens. */
		if (obj->cache_level == I915_CACHE_NONE)
			needs_clflush = 1;
		if (obj->gtt_space) {
			ret = i915_gem_object_set_to_gtt_domain(obj, false);
			if (ret)
				return ret;
		}
	}

	ret = i915_gem_object_get_pages(obj);
	if (ret)
		return ret;

	i915_gem_object_pin_pages(obj);

	offset = args->offset;

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

		hit_slowpath = 1;
		DRM_UNLOCK(dev);

		if (!prefaulted) {
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

	if (hit_slowpath) {
		/* Fixup: Kill any reinstated backing storage pages */
		if (obj->madv == __I915_MADV_PURGED)
			i915_gem_object_truncate(obj);
	}

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

#ifdef FREEBSD_WIP
	/* prime objects have no backing filp to GEM pread/pwrite
	 * pages from.
	 */
	if (!obj->base.filp) {
		ret = -EINVAL;
		goto out;
	}
#endif /* FREEBSD_WIP */

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
fast_user_write(vm_paddr_t mapping_addr,
		off_t page_base, int page_offset,
		char __user *user_data,
		int length)
{
	void __iomem *vaddr_atomic;
	void *vaddr;
	unsigned long unwritten;

	vaddr_atomic = pmap_mapdev_attr(mapping_addr + page_base,
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
	drm_i915_private_t *dev_priv = dev->dev_private;
	ssize_t remain;
	off_t offset, page_base;
	char __user *user_data;
	int page_offset, page_length, ret;

	ret = i915_gem_object_pin(obj, 0, true, true);
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
		if (fast_user_write(dev_priv->mm.gtt_base_addr, page_base,
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
	ssize_t remain;
	off_t offset;
	char __user *user_data;
	int shmem_page_offset, page_length, ret = 0;
	int obj_do_bit17_swizzling, page_do_bit17_swizzling;
	int hit_slowpath = 0;
	int needs_clflush_after = 0;
	int needs_clflush_before = 0;

	user_data = to_user_ptr(args->data_ptr);
	remain = args->size;

	obj_do_bit17_swizzling = i915_gem_object_needs_bit17_swizzle(obj);

	if (obj->base.write_domain != I915_GEM_DOMAIN_CPU) {
		/* If we're not in the cpu write domain, set ourself into the gtt
		 * write domain and manually flush cachelines (if required). This
		 * optimizes for the case when the gpu will use the data
		 * right away and we therefore have to clflush anyway. */
		if (obj->cache_level == I915_CACHE_NONE)
			needs_clflush_after = 1;
		if (obj->gtt_space) {
			ret = i915_gem_object_set_to_gtt_domain(obj, true);
			if (ret)
				return ret;
		}
	}
	/* Same trick applies for invalidate partially written cachelines before
	 * writing.  */
	if (!(obj->base.read_domains & I915_GEM_DOMAIN_CPU)
	    && obj->cache_level == I915_CACHE_NONE)
		needs_clflush_before = 1;

	ret = i915_gem_object_get_pages(obj);
	if (ret)
		return ret;

	i915_gem_object_pin_pages(obj);

	offset = args->offset;
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

	if (hit_slowpath) {
		/* Fixup: Kill any reinstated backing storage pages */
		if (obj->madv == __I915_MADV_PURGED)
			i915_gem_object_truncate(obj);
		/* and flush dirty cachelines in case the object isn't in the cpu write
		 * domain anymore. */
		if (obj->base.write_domain != I915_GEM_DOMAIN_CPU) {
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

	ret = fault_in_multipages_readable(to_user_ptr(args->data_ptr),
					   args->size);
	if (ret)
		return -EFAULT;

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

#ifdef FREEBSD_WIP
	/* prime objects have no backing filp to GEM pread/pwrite
	 * pages from.
	 */
	if (!obj->base.filp) {
		ret = -EINVAL;
		goto out;
	}
#endif /* FREEBSD_WIP */

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

	if (obj->cache_level == I915_CACHE_NONE &&
	    obj->tiling_mode == I915_TILING_NONE &&
	    obj->base.write_domain != I915_GEM_DOMAIN_CPU) {
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

int
i915_gem_check_wedge(struct drm_i915_private *dev_priv,
		     bool interruptible)
{
	if (atomic_read(&dev_priv->mm.wedged)) {
		struct completion *x = &dev_priv->error_completion;
		bool recovery_complete;

		/* Give the error handler a chance to run. */
		mtx_lock(&x->lock);
		recovery_complete = x->done > 0;
		mtx_unlock(&x->lock);

		/* Non-interruptible callers can't handle -EAGAIN, hence return
		 * -EIO unconditionally for these. */
		if (!interruptible)
			return -EIO;

		/* Recovery complete, but still wedged means reset failure. */
		if (recovery_complete)
			return -EIO;

		return -EAGAIN;
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
	if (seqno == ring->outstanding_lazy_request)
		ret = i915_add_request(ring, NULL, NULL);

	return ret;
}

/**
 * __wait_seqno - wait until execution of seqno has finished
 * @ring: the ring expected to report seqno
 * @seqno: duh!
 * @interruptible: do an interruptible wait (normally yes)
 * @timeout: in - how long to wait (NULL forever); out - how much time remaining
 *
 * Returns 0 if the seqno was found within the alloted time. Else returns the
 * errno with remaining time filled in timeout argument.
 */
static int __wait_seqno(struct intel_ring_buffer *ring, u32 seqno,
			bool interruptible, struct timespec *timeout)
{
	drm_i915_private_t *dev_priv = ring->dev->dev_private;
	struct timespec before, now, wait_time={1,0};
	sbintime_t timeout_sbt;
	long end;
	bool wait_forever = true;
	int ret, flags;

	if (i915_seqno_passed(ring->get_seqno(ring, true), seqno))
		return 0;

	CTR2(KTR_DRM, "request_wait_begin %s %d", ring->name, seqno);

	if (timeout != NULL) {
		wait_time = *timeout;
		wait_forever = false;
	}

	timeout_sbt = tstosbt(wait_time);

	if (WARN_ON(!ring->irq_get(ring)))
		return -ENODEV;

	/* Record current time in case interrupted by signal, or wedged * */
	getrawmonotonic(&before);

#define EXIT_COND \
	(i915_seqno_passed(ring->get_seqno(ring, false), seqno) || \
	atomic_read(&dev_priv->mm.wedged))
	flags = interruptible ? PCATCH : 0;
	mtx_lock(&dev_priv->irq_lock);
	do {
		if (EXIT_COND) {
			end = 1;
		} else {
			ret = -msleep_sbt(&ring->irq_queue, &dev_priv->irq_lock, flags,
			    "915gwr", timeout_sbt, 0, 0);

			/*
			 * NOTE Linux<->FreeBSD: Convert msleep_sbt() return
			 * value to something close to wait_event*_timeout()
			 * functions used on Linux.
			 *
			 * >0 -> condition is true (end = time remaining)
			 * =0 -> sleep timed out
			 * <0 -> error (interrupted)
			 *
			 * We fake the remaining time by returning 1. We
			 * compute a proper value later.
			 */
			if (EXIT_COND)
				/* We fake a remaining time of 1 tick. */
				end = 1;
			else if (ret == -EINTR || ret == -ERESTART)
				/* Interrupted. */
				end = -ERESTARTSYS;
			else
				/* Timeout. */
				end = 0;
		}

		ret = i915_gem_check_wedge(dev_priv, interruptible);
		if (ret)
			end = ret;
	} while (end == 0 && wait_forever);
	mtx_unlock(&dev_priv->irq_lock);

	getrawmonotonic(&now);

	ring->irq_put(ring);
	CTR3(KTR_DRM, "request_wait_end %s %d %d", ring->name, seqno, end);
#undef EXIT_COND

	if (timeout) {
		timespecsub(&now, &before, &now);
		timespecsub(timeout, &now, timeout);
	}

	switch (end) {
	case -EIO:
	case -EAGAIN: /* Wedged */
	case -ERESTARTSYS: /* Signal */
	case -ETIMEDOUT: /* Timeout */
		return (int)end;
	case 0: /* Timeout */
		return -ETIMEDOUT;
	default: /* Completed */
		WARN_ON(end < 0); /* We're not aware of other errors */
		return 0;
	}
}

/**
 * Waits for a sequence number to be signaled, and cleans up the
 * request and object lists appropriately for that event.
 */
int
i915_wait_seqno(struct intel_ring_buffer *ring, uint32_t seqno)
{
	struct drm_device *dev = ring->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	bool interruptible = dev_priv->mm.interruptible;
	int ret;

	DRM_LOCK_ASSERT(dev);
	BUG_ON(seqno == 0);

	ret = i915_gem_check_wedge(dev_priv, interruptible);
	if (ret)
		return ret;

	ret = i915_gem_check_olr(ring, seqno);
	if (ret)
		return ret;

	return __wait_seqno(ring, seqno, interruptible, NULL);
}

/**
 * Ensures that all rendering to the object has completed and the object is
 * safe to unbind from the GTT or access from the CPU.
 */
static __must_check int
i915_gem_object_wait_rendering(struct drm_i915_gem_object *obj,
			       bool readonly)
{
	struct intel_ring_buffer *ring = obj->ring;
	u32 seqno;
	int ret;

	seqno = readonly ? obj->last_write_seqno : obj->last_read_seqno;
	if (seqno == 0)
		return 0;

	ret = i915_wait_seqno(ring, seqno);
	if (ret)
		return ret;

	i915_gem_retire_requests_ring(ring);

	/* Manually manage the write flush as we may have not yet
	 * retired the buffer.
	 */
	if (obj->last_write_seqno &&
	    i915_seqno_passed(seqno, obj->last_write_seqno)) {
		obj->last_write_seqno = 0;
		obj->base.write_domain &= ~I915_GEM_GPU_DOMAINS;
	}

	return 0;
}

/* A nonblocking variant of the above wait. This is a highly dangerous routine
 * as the object state may change during this call.
 */
static __must_check int
i915_gem_object_wait_rendering__nonblocking(struct drm_i915_gem_object *obj,
					    bool readonly)
{
	struct drm_device *dev = obj->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring = obj->ring;
	u32 seqno;
	int ret;

	DRM_LOCK_ASSERT(dev);
	BUG_ON(!dev_priv->mm.interruptible);

	seqno = readonly ? obj->last_write_seqno : obj->last_read_seqno;
	if (seqno == 0)
		return 0;

	ret = i915_gem_check_wedge(dev_priv, true);
	if (ret)
		return ret;

	ret = i915_gem_check_olr(ring, seqno);
	if (ret)
		return ret;

	DRM_UNLOCK(dev);
	ret = __wait_seqno(ring, seqno, true, NULL);
	DRM_LOCK(dev);

	i915_gem_retire_requests_ring(ring);

	/* Manually manage the write flush as we may have not yet
	 * retired the buffer.
	 */
	if (ret == 0 &&
	    obj->last_write_seqno &&
	    i915_seqno_passed(seqno, obj->last_write_seqno)) {
		obj->last_write_seqno = 0;
		obj->base.write_domain &= ~I915_GEM_GPU_DOMAINS;
	}

	return ret;
}

/**
 * Called when user space prepares to use an object with the CPU, either
 * through the mmap ioctl's mapping or a GTT mapping.
 */
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

	/* Try to flush the object off the GPU without holding the lock.
	 * We will repeat the flush holding the lock in the normal manner
	 * to catch cases where we are gazumped.
	 */
	ret = i915_gem_object_wait_rendering__nonblocking(obj, !write_domain);
	if (ret)
		goto unref;

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

unref:
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

#ifdef FREEBSD_WIP
	/* prime objects have no backing filp to GEM mmap
	 * pages from.
	 */
	if (!obj->filp) {
		drm_gem_object_unreference_unlocked(obj);
		return -EINVAL;
	}
#endif /* FREEBSD_WIP */

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
	drm_gem_object_unreference_unlocked(obj);
	return (error);
}

static int
i915_gem_pager_ctor(void *handle, vm_ooffset_t size, vm_prot_t prot,
    vm_ooffset_t foff, struct ucred *cred, u_short *color)
{

	/*
	 * NOTE Linux<->FreeBSD: drm_gem_mmap_single() takes care of
	 * calling drm_gem_object_reference(). That's why we don't
	 * do this here. i915_gem_pager_dtor(), below, will call
	 * drm_gem_object_unreference().
	 *
	 * On Linux, drm_gem_vm_open() references the object because
	 * it's called the mapping is copied. drm_gem_vm_open() is not
	 * called when the mapping is created. So the possible sequences
	 * are:
	 *     1. drm_gem_mmap():     ref++
	 *     2. drm_gem_vm_close(): ref--
	 *
	 *     1. drm_gem_mmap():     ref++
	 *     2. drm_gem_vm_open():  ref++ (for the copied vma)
	 *     3. drm_gem_vm_close(): ref-- (for the copied vma)
	 *     4. drm_gem_vm_close(): ref-- (for the initial vma)
	 *
	 * On FreeBSD, i915_gem_pager_ctor() is called once during the
	 * creation of the mapping. No callback is called when the
	 * mapping is shared during a fork(). i915_gem_pager_dtor() is
	 * called when the last reference to the mapping is dropped. So
	 * the only sequence is:
	 *     1. drm_gem_mmap_single(): ref++
	 *     2. i915_gem_pager_ctor(): <noop>
	 *     3. i915_gem_pager_dtor(): ref--
	 */

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
i915_gem_pager_populate(vm_object_t vm_obj, vm_pindex_t pidx, int fault_type,
    vm_prot_t max_prot, vm_pindex_t *first, vm_pindex_t *last)
{
	struct drm_gem_object *gem_obj = vm_obj->handle;
	struct drm_i915_gem_object *obj = to_intel_bo(gem_obj);
	struct drm_device *dev = obj->base.dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	vm_page_t page;
	int ret = 0;
	bool write = (max_prot & VM_PROT_WRITE) != 0;
	bool pinned;

	VM_OBJECT_WUNLOCK(vm_obj);
retry:
	ret = 0;
	pinned = 0;
	page = NULL;

	if (i915_intr_pf) {
		ret = i915_mutex_lock_interruptible(dev);
		if (ret != 0)
			goto out;
	} else
		DRM_LOCK(dev);

	/*
	 * Since the object lock was dropped, other thread might have
	 * faulted on the same GTT address and instantiated the
	 * mapping for the page.  Recheck.
	 */
	VM_OBJECT_WLOCK(vm_obj);
	page = vm_page_lookup(vm_obj, pidx);
	if (page != NULL) {
		if (vm_page_busied(page)) {
			DRM_UNLOCK(dev);
			vm_page_lock(page);
			VM_OBJECT_WUNLOCK(vm_obj);
			vm_page_busy_sleep(page, "915pee", false);
			goto retry;
		}
		goto have_page;
	} else
		VM_OBJECT_WUNLOCK(vm_obj);

	/* Now bind it into the GTT if needed */
	ret = i915_gem_object_pin(obj, 0, true, false);
	if (ret)
		goto unlock;
	pinned = 1;

	ret = i915_gem_object_set_to_gtt_domain(obj, write);
	if (ret)
		goto unpin;

	ret = i915_gem_object_get_fence(obj);
	if (ret)
		goto unpin;

	obj->fault_mappable = true;

	page = PHYS_TO_VM_PAGE(dev_priv->mm.gtt_base_addr + obj->gtt_offset +
	    IDX_TO_OFF(pidx));
	if (page == NULL) {
		ret = -EFAULT;
		goto unpin;
	}
	KASSERT((page->flags & PG_FICTITIOUS) != 0,
	    ("physical address %#jx not fictitious, page %p",
	    (uintmax_t)(dev_priv->mm.gtt_base_addr + obj->gtt_offset +
	    IDX_TO_OFF(pidx)), page));
	KASSERT(page->wire_count == 1, ("wire_count not 1 %p", page));

	VM_OBJECT_WLOCK(vm_obj);
	if (vm_page_busied(page)) {
		i915_gem_object_unpin(obj);
		DRM_UNLOCK(dev);
		vm_page_lock(page);
		VM_OBJECT_WUNLOCK(vm_obj);
		vm_page_busy_sleep(page, "915pbs", false);
		goto retry;
	}
	if (vm_page_insert(page, vm_obj, pidx)) {
		i915_gem_object_unpin(obj);
		DRM_UNLOCK(dev);
		VM_OBJECT_WUNLOCK(vm_obj);
		vm_wait(vm_obj);
		goto retry;
	}
	page->valid = VM_PAGE_BITS_ALL;
have_page:
	vm_page_xbusy(page);

	CTR4(KTR_DRM, "fault %p %jx %x phys %x", gem_obj, pidx, fault_type,
	    page->phys_addr);
	if (pinned) {
		/*
		 * We may have not pinned the object if the page was
		 * found by the call to vm_page_lookup().
		 */
		i915_gem_object_unpin(obj);
	}
	DRM_UNLOCK(dev);
	*first = *last = pidx;
	return (VM_PAGER_OK);

unpin:
	i915_gem_object_unpin(obj);
unlock:
	DRM_UNLOCK(dev);
out:
	KASSERT(ret != 0, ("i915_gem_pager_fault: wrong return"));
	CTR4(KTR_DRM, "fault_fail %p %jx %x err %d", gem_obj, pidx, fault_type,
	    -ret);
	if (ret == -ERESTARTSYS) {
		/*
		 * NOTE Linux<->FreeBSD: Convert Linux' -ERESTARTSYS to
		 * the more common -EINTR, so the page fault is retried.
		 */
		ret = -EINTR;
	}
	if (ret == -EAGAIN || ret == -EIO || ret == -EINTR) {
		kern_yield(PRI_USER);
		goto retry;
	}
	VM_OBJECT_WLOCK(vm_obj);
	return (VM_PAGER_ERROR);
}

static void
i915_gem_pager_dtor(void *handle)
{
	struct drm_gem_object *obj = handle;
	struct drm_device *dev = obj->dev;

	DRM_LOCK(dev);
	drm_gem_object_unreference(obj);
	DRM_UNLOCK(dev);
}

struct cdev_pager_ops i915_gem_pager_ops = {
	.cdev_pg_populate	= i915_gem_pager_populate,
	.cdev_pg_ctor		= i915_gem_pager_ctor,
	.cdev_pg_dtor		= i915_gem_pager_dtor,
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

static int i915_gem_object_create_mmap_offset(struct drm_i915_gem_object *obj)
{
	struct drm_i915_private *dev_priv = obj->base.dev->dev_private;
	int ret;

	if (obj->base.on_map)
		return 0;

	dev_priv->mm.shrinker_no_lock_stealing = true;

	ret = drm_gem_create_mmap_offset(&obj->base);
	if (ret != -ENOSPC)
		goto out;

	/* Badly fragmented mmap space? The only way we can recover
	 * space is by destroying unwanted objects. We can't randomly release
	 * mmap_offsets as userspace expects them to be persistent for the
	 * lifetime of the objects. The closest we can is to release the
	 * offsets on purgeable objects by truncating it and marking it purged,
	 * which prevents userspace from ever using that object again.
	 */
	i915_gem_purge(dev_priv, obj->base.size >> PAGE_SHIFT);
	ret = drm_gem_create_mmap_offset(&obj->base);
	if (ret != -ENOSPC)
		goto out;

	i915_gem_shrink_all(dev_priv);
	ret = drm_gem_create_mmap_offset(&obj->base);
out:
	dev_priv->mm.shrinker_no_lock_stealing = false;

	return ret;
}

static void i915_gem_object_free_mmap_offset(struct drm_i915_gem_object *obj)
{
	if (!obj->base.on_map)
		return;

	drm_gem_free_mmap_offset(&obj->base);
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

	ret = i915_gem_object_create_mmap_offset(obj);
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
	i915_gem_object_free_mmap_offset(obj);

	obj->madv = __I915_MADV_PURGED;
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
		if (vm_page_unwire(page, PQ_INACTIVE))
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
i915_gem_object_put_pages_gtt(struct drm_i915_gem_object *obj)
{
	int page_count = obj->base.size / PAGE_SIZE;
	int ret, i;

	BUG_ON(obj->madv == __I915_MADV_PURGED);

	ret = i915_gem_object_set_to_cpu_domain(obj, true);
	if (ret) {
		/* In the event of a disaster, abandon all caches and
		 * hope for the best.
		 */
		WARN_ON(ret != -EIO);
		i915_gem_clflush_object(obj);
		obj->base.read_domains = obj->base.write_domain = I915_GEM_DOMAIN_CPU;
	}

	if (i915_gem_object_needs_bit17_swizzle(obj))
		i915_gem_object_save_bit_17_swizzle(obj);

	if (obj->madv == I915_MADV_DONTNEED)
		obj->dirty = 0;

	VM_OBJECT_WLOCK(obj->base.vm_obj);
#if GEM_PARANOID_CHECK_GTT
	i915_gem_assert_pages_not_mapped(obj->base.dev, obj->pages, page_count);
#endif
	for (i = 0; i < page_count; i++) {
		vm_page_t page = obj->pages[i];

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
i915_gem_object_put_pages(struct drm_i915_gem_object *obj)
{
	const struct drm_i915_gem_object_ops *ops = obj->ops;

	if (obj->pages == NULL)
		return 0;

	BUG_ON(obj->gtt_space);

	if (obj->pages_pin_count)
		return -EBUSY;

	/* ->put_pages might need to allocate memory for the bit17 swizzle
	 * array, hence protect them from being reaped by removing them from gtt
	 * lists early. */
	list_del(&obj->gtt_list);

	ops->put_pages(obj);
	obj->pages = NULL;

	if (i915_gem_object_is_purgeable(obj))
		i915_gem_object_truncate(obj);

	return 0;
}

static long
__i915_gem_shrink(struct drm_i915_private *dev_priv, long target,
		  bool purgeable_only)
{
	struct drm_i915_gem_object *obj, *next;
	long count = 0;

	list_for_each_entry_safe(obj, next,
				 &dev_priv->mm.unbound_list,
				 gtt_list) {
		if ((i915_gem_object_is_purgeable(obj) || !purgeable_only) &&
		    i915_gem_object_put_pages(obj) == 0) {
			count += obj->base.size >> PAGE_SHIFT;
			if (target != -1 && count >= target)
				return count;
		}
	}

	list_for_each_entry_safe(obj, next,
				 &dev_priv->mm.inactive_list,
				 mm_list) {
		if ((i915_gem_object_is_purgeable(obj) || !purgeable_only) &&
		    i915_gem_object_unbind(obj) == 0 &&
		    i915_gem_object_put_pages(obj) == 0) {
			count += obj->base.size >> PAGE_SHIFT;
			if (target != -1 && count >= target)
				return count;
		}
	}

	return count;
}

static long
i915_gem_purge(struct drm_i915_private *dev_priv, long target)
{
	return __i915_gem_shrink(dev_priv, target, true);
}

static void
i915_gem_shrink_all(struct drm_i915_private *dev_priv)
{
	struct drm_i915_gem_object *obj, *next;

	i915_gem_evict_everything(dev_priv->dev);

	list_for_each_entry_safe(obj, next, &dev_priv->mm.unbound_list, gtt_list)
		i915_gem_object_put_pages(obj);
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
i915_gem_object_get_pages_gtt(struct drm_i915_gem_object *obj)
{
	vm_object_t vm_obj;
	vm_page_t page;
	vm_pindex_t i, page_count;
	int res;

	/* Assert that the object is not currently in any GPU domain. As it
	 * wasn't in the GTT, there shouldn't be any way it could have been in
	 * a GPU cache
	 */
	BUG_ON(obj->base.read_domains & I915_GEM_GPU_DOMAINS);
	BUG_ON(obj->base.write_domain & I915_GEM_GPU_DOMAINS);
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

/* Ensure that the associated pages are gathered from the backing storage
 * and pinned into our object. i915_gem_object_get_pages() may be called
 * multiple times before they are released by a single call to
 * i915_gem_object_put_pages() - once the pages are no longer referenced
 * either as a result of memory pressure (reaping pages under the shrinker)
 * or as the object is itself released.
 */
int
i915_gem_object_get_pages(struct drm_i915_gem_object *obj)
{
	struct drm_i915_private *dev_priv = obj->base.dev->dev_private;
	const struct drm_i915_gem_object_ops *ops = obj->ops;
	int ret;

	if (obj->pages)
		return 0;

	BUG_ON(obj->pages_pin_count);

	ret = ops->get_pages(obj);
	if (ret)
		return ret;

	list_add_tail(&obj->gtt_list, &dev_priv->mm.unbound_list);
	return 0;
}

void
i915_gem_object_move_to_active(struct drm_i915_gem_object *obj,
			       struct intel_ring_buffer *ring)
{
	struct drm_device *dev = obj->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 seqno = intel_ring_get_seqno(ring);

	BUG_ON(ring == NULL);
	obj->ring = ring;

	/* Add a reference if we're newly entering the active list. */
	if (!obj->active) {
		drm_gem_object_reference(&obj->base);
		obj->active = 1;
	}

	/* Move from whatever list we were on to the tail of execution. */
	list_move_tail(&obj->mm_list, &dev_priv->mm.active_list);
	list_move_tail(&obj->ring_list, &ring->active_list);

	obj->last_read_seqno = seqno;

	if (obj->fenced_gpu_access) {
		obj->last_fenced_seqno = seqno;

		/* Bump MRU to take account of the delayed flush */
		if (obj->fence_reg != I915_FENCE_REG_NONE) {
			struct drm_i915_fence_reg *reg;

			reg = &dev_priv->fence_regs[obj->fence_reg];
			list_move_tail(&reg->lru_list,
				       &dev_priv->mm.fence_list);
		}
	}
}

static void
i915_gem_object_move_to_inactive(struct drm_i915_gem_object *obj)
{
	struct drm_device *dev = obj->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	BUG_ON(obj->base.write_domain & ~I915_GEM_GPU_DOMAINS);
	BUG_ON(!obj->active);

	list_move_tail(&obj->mm_list, &dev_priv->mm.inactive_list);

	list_del_init(&obj->ring_list);
	obj->ring = NULL;

	obj->last_read_seqno = 0;
	obj->last_write_seqno = 0;
	obj->base.write_domain = 0;

	obj->last_fenced_seqno = 0;
	obj->fenced_gpu_access = false;

	obj->active = 0;
	drm_gem_object_unreference(&obj->base);

	WARN_ON(i915_verify_lists(dev));
}

static int
i915_gem_handle_seqno_wrap(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring;
	int ret, i, j;

	/* The hardware uses various monotonic 32-bit counters, if we
	 * detect that they will wraparound we need to idle the GPU
	 * and reset those counters.
	 */
	ret = 0;
	for_each_ring(ring, dev_priv, i) {
		for (j = 0; j < ARRAY_SIZE(ring->sync_seqno); j++)
			ret |= ring->sync_seqno[j] != 0;
	}
	if (ret == 0)
		return ret;

	ret = i915_gpu_idle(dev);
	if (ret)
		return ret;

	i915_gem_retire_requests(dev);
	for_each_ring(ring, dev_priv, i) {
		for (j = 0; j < ARRAY_SIZE(ring->sync_seqno); j++)
			ring->sync_seqno[j] = 0;
	}

	return 0;
}

int
i915_gem_get_seqno(struct drm_device *dev, u32 *seqno)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	/* reserve 0 for non-seqno */
	if (dev_priv->next_seqno == 0) {
		int ret = i915_gem_handle_seqno_wrap(dev);
		if (ret)
			return ret;

		dev_priv->next_seqno = 1;
	}

	*seqno = dev_priv->next_seqno++;
	return 0;
}

int
i915_add_request(struct intel_ring_buffer *ring,
		 struct drm_file *file,
		 u32 *out_seqno)
{
	drm_i915_private_t *dev_priv = ring->dev->dev_private;
	struct drm_i915_gem_request *request;
	u32 request_ring_position;
	int was_empty;
	int ret;

	/*
	 * Emit any outstanding flushes - execbuf can fail to emit the flush
	 * after having emitted the batchbuffer command. Hence we need to fix
	 * things up similar to emitting the lazy request. The difference here
	 * is that the flush _must_ happen before the next request, no matter
	 * what.
	 */
	ret = intel_ring_flush_all_caches(ring);
	if (ret)
		return ret;

	request = malloc(sizeof(*request), DRM_I915_GEM, M_NOWAIT);
	if (request == NULL)
		return -ENOMEM;


	/* Record the position of the start of the request so that
	 * should we detect the updated seqno part-way through the
	 * GPU processing the request, we never over-estimate the
	 * position of the head.
	 */
	request_ring_position = intel_ring_get_tail(ring);

	ret = ring->add_request(ring);
	if (ret) {
		free(request, DRM_I915_GEM);
		return ret;
	}

	request->seqno = intel_ring_get_seqno(ring);
	request->ring = ring;
	request->tail = request_ring_position;
	request->emitted_jiffies = jiffies;
	was_empty = list_empty(&ring->request_list);
	list_add_tail(&request->list, &ring->request_list);
	request->file_priv = NULL;

	if (file) {
		struct drm_i915_file_private *file_priv = file->driver_priv;

		mtx_lock(&file_priv->mm.lock);
		request->file_priv = file_priv;
		list_add_tail(&request->client_list,
			      &file_priv->mm.request_list);
		mtx_unlock(&file_priv->mm.lock);
	}

	CTR2(KTR_DRM, "request_add %s %d", ring->name, request->seqno);
	ring->outstanding_lazy_request = 0;

	if (!dev_priv->mm.suspended) {
		if (i915_enable_hangcheck) {
			callout_schedule(&dev_priv->hangcheck_timer,
			    DRM_I915_HANGCHECK_PERIOD);
		}
		if (was_empty) {
			taskqueue_enqueue_timeout(dev_priv->wq,
			    &dev_priv->mm.retire_work, hz);
			intel_mark_busy(dev_priv->dev);
		}
	}

	if (out_seqno)
		*out_seqno = request->seqno;
	return 0;
}

static inline void
i915_gem_request_remove_from_client(struct drm_i915_gem_request *request)
{
	struct drm_i915_file_private *file_priv = request->file_priv;

	if (!file_priv)
		return;

	mtx_lock(&file_priv->mm.lock);
	if (request->file_priv) {
		list_del(&request->client_list);
		request->file_priv = NULL;
	}
	mtx_unlock(&file_priv->mm.lock);
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

	/* Move everything out of the GPU domains to ensure we do any
	 * necessary invalidation upon reuse.
	 */
	list_for_each_entry(obj,
			    &dev_priv->mm.inactive_list,
			    mm_list)
	{
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

	if (list_empty(&ring->request_list))
		return;

	WARN_ON(i915_verify_lists(ring->dev));

	seqno = ring->get_seqno(ring, true);
	CTR2(KTR_DRM, "retire_request_ring %s %d", ring->name, seqno);

	while (!list_empty(&ring->request_list)) {
		struct drm_i915_gem_request *request;

		request = list_first_entry(&ring->request_list,
					   struct drm_i915_gem_request,
					   list);

		if (!i915_seqno_passed(seqno, request->seqno))
			break;

		CTR2(KTR_DRM, "retire_request_seqno_passed %s %d",
		    ring->name, seqno);
		/* We know the GPU must have read the request to have
		 * sent us the seqno + interrupt, so use the position
		 * of tail of the request to update the last known position
		 * of the GPU head.
		 */
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

		if (!i915_seqno_passed(seqno, obj->last_read_seqno))
			break;

		i915_gem_object_move_to_inactive(obj);
	}

	if (unlikely(ring->trace_irq_seqno &&
		     i915_seqno_passed(seqno, ring->trace_irq_seqno))) {
		ring->irq_put(ring);
		ring->trace_irq_seqno = 0;
	}

	WARN_ON(i915_verify_lists(ring->dev));
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
i915_gem_retire_work_handler(void *arg, int pending)
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
		taskqueue_enqueue_timeout(dev_priv->wq,
		    &dev_priv->mm.retire_work, hz);
		return;
	}

	CTR0(KTR_DRM, "retire_task");

	i915_gem_retire_requests(dev);

	/* Send a periodic flush down the ring so we don't hold onto GEM
	 * objects indefinitely.
	 */
	idle = true;
	for_each_ring(ring, dev_priv, i) {
		if (ring->gpu_caches_dirty)
			i915_add_request(ring, NULL, NULL);

		idle &= list_empty(&ring->request_list);
	}

	if (!dev_priv->mm.suspended && !idle)
		taskqueue_enqueue_timeout(dev_priv->wq,
		    &dev_priv->mm.retire_work, hz);
	if (idle)
		intel_mark_idle(dev);

	DRM_UNLOCK(dev);
}

/**
 * Ensures that an object will eventually get non-busy by flushing any required
 * write domains, emitting any outstanding lazy request and retiring and
 * completed requests.
 */
static int
i915_gem_object_flush_active(struct drm_i915_gem_object *obj)
{
	int ret;

	if (obj->active) {
		ret = i915_gem_check_olr(obj->ring, obj->last_read_seqno);
		if (ret)
			return ret;

		i915_gem_retire_requests_ring(obj->ring);
	}

	return 0;
}

/**
 * i915_gem_wait_ioctl - implements DRM_IOCTL_I915_GEM_WAIT
 * @DRM_IOCTL_ARGS: standard ioctl arguments
 *
 * Returns 0 if successful, else an error is returned with the remaining time in
 * the timeout parameter.
 *  -ETIME: object is still busy after timeout
 *  -ERESTARTSYS: signal interrupted the wait
 *  -ENONENT: object doesn't exist
 * Also possible, but rare:
 *  -EAGAIN: GPU wedged
 *  -ENOMEM: damn
 *  -ENODEV: Internal IRQ fail
 *  -E?: The add request failed
 *
 * The wait ioctl with a timeout of 0 reimplements the busy ioctl. With any
 * non-zero timeout parameter the wait ioctl will wait for the given number of
 * nanoseconds on an object becoming unbusy. Since the wait itself does so
 * without holding struct_mutex the object may become re-busied before this
 * function completes. A similar but shorter * race condition exists in the busy
 * ioctl
 */
int
i915_gem_wait_ioctl(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct drm_i915_gem_wait *args = data;
	struct drm_i915_gem_object *obj;
	struct intel_ring_buffer *ring = NULL;
	struct timespec timeout_stack, *timeout = NULL;
	u32 seqno = 0;
	int ret = 0;

	if (args->timeout_ns >= 0) {
		timeout_stack.tv_sec = args->timeout_ns / 1000000;
		timeout_stack.tv_nsec = args->timeout_ns % 1000000;
		timeout = &timeout_stack;
	}

	ret = i915_mutex_lock_interruptible(dev);
	if (ret)
		return ret;

	obj = to_intel_bo(drm_gem_object_lookup(dev, file, args->bo_handle));
	if (&obj->base == NULL) {
		DRM_UNLOCK(dev);
		return -ENOENT;
	}

	/* Need to make sure the object gets inactive eventually. */
	ret = i915_gem_object_flush_active(obj);
	if (ret)
		goto out;

	if (obj->active) {
		seqno = obj->last_read_seqno;
		ring = obj->ring;
	}

	if (seqno == 0)
		 goto out;

	/* Do this after OLR check to make sure we make forward progress polling
	 * on this IOCTL with a 0 timeout (like busy ioctl)
	 */
	if (!args->timeout_ns) {
		ret = -ETIMEDOUT;
		goto out;
	}

	drm_gem_object_unreference(&obj->base);
	DRM_UNLOCK(dev);

	ret = __wait_seqno(ring, seqno, true, timeout);
	if (timeout) {
		args->timeout_ns = timeout->tv_sec * 1000000 + timeout->tv_nsec;
	}
	return ret;

out:
	drm_gem_object_unreference(&obj->base);
	DRM_UNLOCK(dev);
	return ret;
}

/**
 * i915_gem_object_sync - sync an object to a ring.
 *
 * @obj: object which may be in use on another ring.
 * @to: ring we wish to use the object on. May be NULL.
 *
 * This code is meant to abstract object synchronization with the GPU.
 * Calling with NULL implies synchronizing the object with the CPU
 * rather than a particular GPU ring.
 *
 * Returns 0 if successful, else propagates up the lower layer error.
 */
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
		return i915_gem_object_wait_rendering(obj, false);

	idx = intel_ring_sync_index(from, to);

	seqno = obj->last_read_seqno;
	if (seqno <= from->sync_seqno[idx])
		return 0;

	ret = i915_gem_check_olr(obj->ring, seqno);
	if (ret)
		return ret;

	ret = to->sync_to(to, from, seqno);
	if (!ret)
		/* We use last_read_seqno because sync_to()
		 * might have just caused seqno wrap under
		 * the radar.
		 */
		from->sync_seqno[idx] = obj->last_read_seqno;

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

	if (obj->pin_count)
		return -EBUSY;

	BUG_ON(obj->pages == NULL);

	ret = i915_gem_object_finish_gpu(obj);
	if (ret)
		return ret;
	/* Continue on if we fail due to EIO, the GPU is hung so we
	 * should be safe and we need to cleanup or else we might
	 * cause memory corruption through use-after-free.
	 */

	i915_gem_object_finish_gtt(obj);

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

	list_del(&obj->mm_list);
	list_move_tail(&obj->gtt_list, &dev_priv->mm.unbound_list);
	/* Avoid an unnecessary call to unbind on rebind. */
	obj->map_and_fenceable = true;

	drm_mm_put_block(obj->gtt_space);
	obj->gtt_space = NULL;
	obj->gtt_offset = 0;

	return 0;
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

		ret = intel_ring_idle(ring);
		if (ret)
			return ret;
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

		WARN((obj->gtt_offset & ~I915_FENCE_START_MASK) ||
		     (size & -size) != size ||
		     (obj->gtt_offset & (size - 1)),
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

		WARN((obj->gtt_offset & ~I830_FENCE_START_MASK) ||
		     (size & -size) != size ||
		     (obj->gtt_offset & (size - 1)),
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

static void i915_gem_write_fence__ipi(void *data)
{
	wbinvd();
}

static void i915_gem_object_update_fence(struct drm_i915_gem_object *obj,
					 struct drm_i915_fence_reg *fence,
					 bool enable)
{
	struct drm_device *dev = obj->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int fence_reg = fence_number(dev_priv, fence);

	/* In order to fully serialize access to the fenced region and
	 * the update to the fence register we need to take extreme
	 * measures on SNB+. In theory, the write to the fence register
	 * flushes all memory transactions before, and coupled with the
	 * mb() placed around the register write we serialise all memory
	 * operations with respect to the changes in the tiler. Yet, on
	 * SNB+ we need to take a step further and emit an explicit wbinvd()
	 * on each processor in order to manually flush all memory
	 * transactions before updating the fence register.
	 */
	if (HAS_LLC(obj->base.dev))
		on_each_cpu(i915_gem_write_fence__ipi, NULL, 1);
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
	if (obj->last_fenced_seqno) {
		int ret = i915_wait_seqno(obj->ring, obj->last_fenced_seqno);
		if (ret)
			return ret;

		obj->last_fenced_seqno = 0;
	}

	/* Ensure that all CPU reads are completed before installing a fence
	 * and all writes before removing the fence.
	 */
	if (obj->base.read_domains & I915_GEM_DOMAIN_GTT)
		mb();

	obj->fenced_gpu_access = false;
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

static bool i915_gem_valid_gtt_space(struct drm_device *dev,
				     struct drm_mm_node *gtt_space,
				     unsigned long cache_level)
{
	struct drm_mm_node *other;

	/* On non-LLC machines we have to be careful when putting differing
	 * types of snoopable memory together to avoid the prefetcher
	 * crossing memory domains and dying.
	 */
	if (HAS_LLC(dev))
		return true;

	if (gtt_space == NULL)
		return true;

	if (list_empty(&gtt_space->node_list))
		return true;

	other = list_entry(gtt_space->node_list.prev, struct drm_mm_node, node_list);
	if (other->allocated && !other->hole_follows && other->color != cache_level)
		return false;

	other = list_entry(gtt_space->node_list.next, struct drm_mm_node, node_list);
	if (other->allocated && !gtt_space->hole_follows && other->color != cache_level)
		return false;

	return true;
}

static void i915_gem_verify_gtt(struct drm_device *dev)
{
#if WATCH_GTT
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj;
	int err = 0;

	list_for_each_entry(obj, &dev_priv->mm.gtt_list, gtt_list) {
		if (obj->gtt_space == NULL) {
			DRM_ERROR("object found on GTT list with no space reserved\n");
			err++;
			continue;
		}

		if (obj->cache_level != obj->gtt_space->color) {
			DRM_ERROR("object reserved space [%08lx, %08lx] with wrong color, cache_level=%x, color=%lx\n",
			       obj->gtt_space->start,
			       obj->gtt_space->start + obj->gtt_space->size,
			       obj->cache_level,
			       obj->gtt_space->color);
			err++;
			continue;
		}

		if (!i915_gem_valid_gtt_space(dev,
					      obj->gtt_space,
					      obj->cache_level)) {
			DRM_ERROR("invalid GTT space found at [%08lx, %08lx] - color=%x\n",
			       obj->gtt_space->start,
			       obj->gtt_space->start + obj->gtt_space->size,
			       obj->cache_level);
			err++;
			continue;
		}
	}

	WARN_ON(err);
#endif
}

/**
 * Finds free space in the GTT aperture and binds the object there.
 */
static int
i915_gem_object_bind_to_gtt(struct drm_i915_gem_object *obj,
			    unsigned alignment,
			    bool map_and_fenceable,
			    bool nonblocking)
{
	struct drm_device *dev = obj->base.dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_mm_node *node;
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

	ret = i915_gem_object_get_pages(obj);
	if (ret)
		return ret;

	i915_gem_object_pin_pages(obj);

	node = malloc(sizeof(*node), DRM_MEM_MM, M_NOWAIT | M_ZERO);
	if (node == NULL) {
		i915_gem_object_unpin_pages(obj);
		return -ENOMEM;
	}

 search_free:
	if (map_and_fenceable)
		ret = drm_mm_insert_node_in_range_generic(&dev_priv->mm.gtt_space, node,
							  size, alignment, obj->cache_level,
							  0, dev_priv->mm.gtt_mappable_end);
	else
		ret = drm_mm_insert_node_generic(&dev_priv->mm.gtt_space, node,
						 size, alignment, obj->cache_level);
	if (ret) {
		ret = i915_gem_evict_something(dev, size, alignment,
					       obj->cache_level,
					       map_and_fenceable,
					       nonblocking);
		if (ret == 0)
			goto search_free;

		i915_gem_object_unpin_pages(obj);
		free(node, DRM_MEM_MM);
		return ret;
	}
	if (WARN_ON(!i915_gem_valid_gtt_space(dev, node, obj->cache_level))) {
		i915_gem_object_unpin_pages(obj);
		drm_mm_put_block(node);
		return -EINVAL;
	}

	ret = i915_gem_gtt_prepare_object(obj);
	if (ret) {
		i915_gem_object_unpin_pages(obj);
		drm_mm_put_block(node);
		return ret;
	}

	list_move_tail(&obj->gtt_list, &dev_priv->mm.bound_list);
	list_add_tail(&obj->mm_list, &dev_priv->mm.inactive_list);

	obj->gtt_space = node;
	obj->gtt_offset = node->start;

	fenceable =
		node->size == fence_size &&
		(node->start & (fence_alignment - 1)) == 0;

	mappable =
		obj->gtt_offset + obj->base.size <= dev_priv->mm.gtt_mappable_end;

	obj->map_and_fenceable = mappable && fenceable;

	i915_gem_object_unpin_pages(obj);
	CTR4(KTR_DRM, "object_bind %p %x %x %d", obj, obj->gtt_offset,
	    obj->base.size, map_and_fenceable);
	i915_gem_verify_gtt(dev);
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
	i915_gem_chipset_flush(obj->base.dev);
	old_write_domain = obj->base.write_domain;
	obj->base.write_domain = 0;

	CTR3(KTR_DRM, "object_change_domain flush_cpu_write %p %x %x", obj,
	    obj->base.read_domains, old_write_domain);
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

	ret = i915_gem_object_wait_rendering(obj, !write);
	if (ret)
		return ret;

	i915_gem_object_flush_cpu_write_domain(obj);

	old_write_domain = obj->base.write_domain;
	old_read_domains = obj->base.read_domains;

	/* It should now be out of any other write domains, and we can update
	 * the domain values for our changes.
	 */
	BUG_ON((obj->base.write_domain & ~I915_GEM_DOMAIN_GTT) != 0);
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

	if (!i915_gem_valid_gtt_space(dev, obj->gtt_space, cache_level)) {
		ret = i915_gem_object_unbind(obj);
		if (ret)
			return ret;
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
		if (INTEL_INFO(dev)->gen < 6) {
			ret = i915_gem_object_put_fence(obj);
			if (ret)
				return ret;
		}

		if (obj->has_global_gtt_mapping)
			i915_gem_gtt_bind_object(obj, cache_level);
		if (obj->has_aliasing_ppgtt_mapping)
			i915_ppgtt_bind_object(dev_priv->mm.aliasing_ppgtt,
					       obj, cache_level);

		obj->gtt_space->color = cache_level;
	}

	if (cache_level == I915_CACHE_NONE) {
		u32 old_read_domains, old_write_domain;

		/* If we're coming from LLC cached, then we haven't
		 * actually been tracking whether the data is in the
		 * CPU cache or not, since we only allow one bit set
		 * in obj->write_domain and have been skipping the clflushes.
		 * Just set it to the CPU cache for now.
		 */
		WARN_ON(obj->base.write_domain & ~I915_GEM_DOMAIN_CPU);
		WARN_ON(obj->base.read_domains & ~I915_GEM_DOMAIN_CPU);

		old_read_domains = obj->base.read_domains;
		old_write_domain = obj->base.write_domain;

		obj->base.read_domains = I915_GEM_DOMAIN_CPU;
		obj->base.write_domain = I915_GEM_DOMAIN_CPU;

		CTR3(KTR_DRM, "object_change_domain set_cache_level %p %x %x",
		    obj, old_read_domains, old_write_domain);
	}

	obj->cache_level = cache_level;
	i915_gem_verify_gtt(dev);
	return 0;
}

int i915_gem_get_caching_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file)
{
	struct drm_i915_gem_caching *args = data;
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

	args->caching = obj->cache_level != I915_CACHE_NONE;

	drm_gem_object_unreference(&obj->base);
unlock:
	DRM_UNLOCK(dev);
	return ret;
}

int i915_gem_set_caching_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file)
{
	struct drm_i915_gem_caching *args = data;
	struct drm_i915_gem_object *obj;
	enum i915_cache_level level;
	int ret;

	switch (args->caching) {
	case I915_CACHING_NONE:
		level = I915_CACHE_NONE;
		break;
	case I915_CACHING_CACHED:
		level = I915_CACHE_LLC;
		break;
	default:
		return -EINVAL;
	}

	ret = i915_mutex_lock_interruptible(dev);
	if (ret)
		return ret;

	obj = to_intel_bo(drm_gem_object_lookup(dev, file, args->handle));
	if (&obj->base == NULL) {
		ret = -ENOENT;
		goto unlock;
	}

	ret = i915_gem_object_set_cache_level(obj, level);

	drm_gem_object_unreference(&obj->base);
unlock:
	DRM_UNLOCK(dev);
	return ret;
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

/*
 * Prepare buffer for display plane (scanout, cursors, etc).
 * Can be called from an uninterruptible phase (modesetting) and allows
 * any flushes to be pipelined (for pageflips).
 */
int
i915_gem_object_pin_to_display_plane(struct drm_i915_gem_object *obj,
				     u32 alignment,
				     struct intel_ring_buffer *pipelined)
{
	u32 old_read_domains, old_write_domain;
	int ret;

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
	ret = i915_gem_object_pin(obj, alignment, true, false);
	if (ret)
		goto err_unpin_display;

	i915_gem_object_flush_cpu_write_domain(obj);

	old_write_domain = obj->base.write_domain;
	old_read_domains = obj->base.read_domains;

	/* It should now be out of any other write domains, and we can update
	 * the domain values for our changes.
	 */
	obj->base.write_domain = 0;
	obj->base.read_domains |= I915_GEM_DOMAIN_GTT;

	CTR3(KTR_DRM, "object_change_domain pin_to_display_plan %p %x %x",
	    obj, old_read_domains, old_write_domain);

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

	ret = i915_gem_object_wait_rendering(obj, false);
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

	ret = i915_gem_object_wait_rendering(obj, !write);
	if (ret)
		return ret;

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
	BUG_ON((obj->base.write_domain & ~I915_GEM_DOMAIN_CPU) != 0);

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
	unsigned long recent_enough = jiffies - msecs_to_jiffies(20);
	struct drm_i915_gem_request *request;
	struct intel_ring_buffer *ring = NULL;
	u32 seqno = 0;
	int ret;

	if (atomic_read(&dev_priv->mm.wedged))
		return -EIO;

	mtx_lock(&file_priv->mm.lock);
	list_for_each_entry(request, &file_priv->mm.request_list, client_list) {
		if (time_after_eq(request->emitted_jiffies, recent_enough))
			break;

		ring = request->ring;
		seqno = request->seqno;
	}
	mtx_unlock(&file_priv->mm.lock);

	if (seqno == 0)
		return 0;

	ret = __wait_seqno(ring, seqno, true, NULL);
	if (ret == 0)
		taskqueue_enqueue_timeout(dev_priv->wq,
		    &dev_priv->mm.retire_work, 0);

	return ret;
}

int
i915_gem_object_pin(struct drm_i915_gem_object *obj,
		    uint32_t alignment,
		    bool map_and_fenceable,
		    bool nonblocking)
{
	int ret;

	if (WARN_ON(obj->pin_count == DRM_I915_GEM_OBJECT_MAX_PIN_COUNT))
		return -EBUSY;

	if (obj->gtt_space != NULL) {
		if ((alignment && obj->gtt_offset & (alignment - 1)) ||
		    (map_and_fenceable && !obj->map_and_fenceable)) {
			WARN(obj->pin_count,
			     "bo is already pinned with incorrect alignment:"
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
		struct drm_i915_private *dev_priv = obj->base.dev->dev_private;

		ret = i915_gem_object_bind_to_gtt(obj, alignment,
						  map_and_fenceable,
						  nonblocking);
		if (ret)
			return ret;

		if (!dev_priv->mm.aliasing_ppgtt)
			i915_gem_gtt_bind_object(obj, obj->cache_level);
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
	BUG_ON(obj->pin_count == 0);
	BUG_ON(obj->gtt_space == NULL);

	if (--obj->pin_count == 0)
		obj->pin_mappable = false;
}

int
i915_gem_pin_ioctl(struct drm_device *dev, void *data,
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

	if (obj->user_pin_count == 0) {
		ret = i915_gem_object_pin(obj, args->alignment, true, false);
		if (ret)
			goto out;
	}

	obj->user_pin_count++;
	obj->pin_filp = file;

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

	/* Count all active objects as busy, even if they are currently not used
	 * by the gpu. Users of this interface expect objects to eventually
	 * become non-busy without any further actions, therefore emit any
	 * necessary flushes here.
	 */
	ret = i915_gem_object_flush_active(obj);

	args->busy = obj->active;
	if (obj->ring) {
		BUILD_BUG_ON(I915_NUM_RINGS > 16);
		args->busy |= intel_ring_flag(obj->ring) << 16;
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

	if (obj->madv != __I915_MADV_PURGED)
		obj->madv = args->madv;

	/* if the object is no longer attached, discard its backing storage */
	if (i915_gem_object_is_purgeable(obj) && obj->pages == NULL)
		i915_gem_object_truncate(obj);

	args->retained = obj->madv != __I915_MADV_PURGED;

out:
	drm_gem_object_unreference(&obj->base);
unlock:
	DRM_UNLOCK(dev);
	return ret;
}

void i915_gem_object_init(struct drm_i915_gem_object *obj,
			  const struct drm_i915_gem_object_ops *ops)
{
	INIT_LIST_HEAD(&obj->mm_list);
	INIT_LIST_HEAD(&obj->gtt_list);
	INIT_LIST_HEAD(&obj->ring_list);
	INIT_LIST_HEAD(&obj->exec_list);

	obj->ops = ops;

	obj->fence_reg = I915_FENCE_REG_NONE;
	obj->madv = I915_MADV_WILLNEED;
	/* Avoid an unnecessary call to unbind on the first bind. */
	obj->map_and_fenceable = true;

	i915_gem_info_add_obj(obj->base.dev->dev_private, obj->base.size);
}

static const struct drm_i915_gem_object_ops i915_gem_object_ops = {
	.get_pages = i915_gem_object_get_pages_gtt,
	.put_pages = i915_gem_object_put_pages_gtt,
};

struct drm_i915_gem_object *i915_gem_alloc_object(struct drm_device *dev,
						  size_t size)
{
	struct drm_i915_gem_object *obj;

	obj = malloc(sizeof(*obj), DRM_I915_GEM, M_WAITOK | M_ZERO);
	if (obj == NULL)
		return NULL;

	if (drm_gem_object_init(dev, &obj->base, size) != 0) {
		free(obj, DRM_I915_GEM);
		return NULL;
	}

#ifdef FREEBSD_WIP
	mask = GFP_HIGHUSER | __GFP_RECLAIMABLE;
	if (IS_CRESTLINE(dev) || IS_BROADWATER(dev)) {
		/* 965gm cannot relocate objects above 4GiB. */
		mask &= ~__GFP_HIGHMEM;
		mask |= __GFP_DMA32;
	}

	mapping = obj->base.filp->f_path.dentry->d_inode->i_mapping;
	mapping_set_gfp_mask(mapping, mask);
#endif /* FREEBSD_WIP */

	i915_gem_object_init(obj, &i915_gem_object_ops);

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
	if (WARN_ON(i915_gem_object_unbind(obj) == -ERESTARTSYS)) {
		bool was_interruptible;

		was_interruptible = dev_priv->mm.interruptible;
		dev_priv->mm.interruptible = false;

		WARN_ON(i915_gem_object_unbind(obj));

		dev_priv->mm.interruptible = was_interruptible;
	}

	obj->pages_pin_count = 0;
	i915_gem_object_put_pages(obj);
	i915_gem_object_free_mmap_offset(obj);

	BUG_ON(obj->pages);

#ifdef FREEBSD_WIP
	if (obj->base.import_attach)
		drm_prime_gem_destroy(&obj->base, NULL);
#endif /* FREEBSD_WIP */

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
	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		i915_gem_evict_everything(dev);

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
	taskqueue_cancel_timeout(dev_priv->wq, &dev_priv->mm.retire_work, NULL);

	return 0;
}

void i915_gem_l3_remap(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	u32 misccpctl;
	int i;

	if (!HAS_L3_GPU_CACHE(dev))
		return;

	if (!dev_priv->l3_parity.remap_info)
		return;

	misccpctl = I915_READ(GEN7_MISCCPCTL);
	I915_WRITE(GEN7_MISCCPCTL, misccpctl & ~GEN7_DOP_CLOCK_GATE_ENABLE);
	POSTING_READ(GEN7_MISCCPCTL);

	for (i = 0; i < GEN7_L3LOG_SIZE; i += 4) {
		u32 remap = I915_READ(GEN7_L3LOG_BASE + i);
		if (remap && remap != dev_priv->l3_parity.remap_info[i/4])
			DRM_DEBUG("0x%x was already programmed to %x\n",
				  GEN7_L3LOG_BASE + i, remap);
		if (remap && !dev_priv->l3_parity.remap_info[i/4])
			DRM_DEBUG_DRIVER("Clearing remapped register\n");
		I915_WRITE(GEN7_L3LOG_BASE + i, dev_priv->l3_parity.remap_info[i/4]);
	}

	/* Make sure all the writes land before disabling dop clock gating */
	POSTING_READ(GEN7_L3LOG_BASE);

	I915_WRITE(GEN7_MISCCPCTL, misccpctl);
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

static bool
intel_enable_blt(struct drm_device *dev)
{
	if (!HAS_BLT(dev))
		return false;

	/* The blitter was dysfunctional on early prototypes */
	if (IS_GEN6(dev) && pci_get_revid(dev->dev) < 8) {
		DRM_INFO("BLT not supported on this pre-production hardware;"
			 " graphics performance will be degraded.\n");
		return false;
	}

	return true;
}

int
i915_gem_init_hw(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret;

#ifdef FREEBSD_WIP
	if (INTEL_INFO(dev)->gen < 6 && !intel_enable_gtt())
		return -EIO;
#endif /* FREEBSD_WIP */

	if (IS_HASWELL(dev) && (I915_READ(0x120010) == 1))
		I915_WRITE(0x9008, I915_READ(0x9008) | 0xf0000);

	i915_gem_l3_remap(dev);

	i915_gem_init_swizzling(dev);

	ret = intel_init_render_ring_buffer(dev);
	if (ret)
		return ret;

	if (HAS_BSD(dev)) {
		ret = intel_init_bsd_ring_buffer(dev);
		if (ret)
			goto cleanup_render_ring;
	}

	if (intel_enable_blt(dev)) {
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
	intel_cleanup_ring_buffer(&dev_priv->ring[VCS]);
cleanup_render_ring:
	intel_cleanup_ring_buffer(&dev_priv->ring[RCS]);
	return ret;
}

static bool
intel_enable_ppgtt(struct drm_device *dev)
{
	if (i915_enable_ppgtt >= 0)
		return i915_enable_ppgtt;

#ifdef CONFIG_INTEL_IOMMU
	/* Disable ppgtt on SNB if VT-d is on. */
	if (INTEL_INFO(dev)->gen == 6 && intel_iommu_gfx_mapped)
		return false;
#endif

	return true;
}

int i915_gem_init(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	unsigned long gtt_size, mappable_size;
	int ret;

	gtt_size = dev_priv->mm.gtt->gtt_total_entries << PAGE_SHIFT;
	mappable_size = dev_priv->mm.gtt->gtt_mappable_entries << PAGE_SHIFT;

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

	if (atomic_read(&dev_priv->mm.wedged)) {
		DRM_ERROR("Reenabling wedged hardware, good luck\n");
		atomic_set(&dev_priv->mm.wedged, 0);
	}

	DRM_LOCK(dev);
	dev_priv->mm.suspended = 0;

	ret = i915_gem_init_hw(dev);
	if (ret != 0) {
		DRM_UNLOCK(dev);
		return ret;
	}

	BUG_ON(!list_empty(&dev_priv->mm.active_list));
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
}

void
i915_gem_load(struct drm_device *dev)
{
	int i;
	drm_i915_private_t *dev_priv = dev->dev_private;

	INIT_LIST_HEAD(&dev_priv->mm.active_list);
	INIT_LIST_HEAD(&dev_priv->mm.inactive_list);
	INIT_LIST_HEAD(&dev_priv->mm.unbound_list);
	INIT_LIST_HEAD(&dev_priv->mm.bound_list);
	INIT_LIST_HEAD(&dev_priv->mm.fence_list);
	for (i = 0; i < I915_NUM_RINGS; i++)
		init_ring_lists(&dev_priv->ring[i]);
	for (i = 0; i < I915_MAX_NUM_FENCES; i++)
		INIT_LIST_HEAD(&dev_priv->fence_regs[i].lru_list);
	TIMEOUT_TASK_INIT(dev_priv->wq, &dev_priv->mm.retire_work, 0,
	    i915_gem_retire_work_handler, dev_priv);
	init_completion(&dev_priv->error_completion);

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
	DRM_INIT_WAITQUEUE(&dev_priv->pending_flip_queue);

	dev_priv->mm.interruptible = true;

	dev_priv->mm.inactive_shrinker = EVENTHANDLER_REGISTER(vm_lowmem,
	    i915_gem_inactive_shrink, dev, EVENTHANDLER_PRI_ANY);
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
	if (!phys_obj)
		return -ENOMEM;

	phys_obj->id = id;

	phys_obj->handle = drm_pci_alloc(dev, size, align, BUS_SPACE_MAXADDR);
	if (!phys_obj->handle) {
		ret = -ENOMEM;
		goto kfree_obj;
	}
#ifdef CONFIG_X86
	pmap_change_attr((vm_offset_t)phys_obj->handle->vaddr,
	    size / PAGE_SIZE, PAT_WRITE_COMBINING);
#endif

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

#ifdef FREEBSD_WIP
#ifdef CONFIG_X86
	set_memory_wb((unsigned long)phys_obj->handle->vaddr, phys_obj->handle->size / PAGE_SIZE);
#endif
#endif /* FREEBSD_WIP */

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
	struct sf_buf *sf;
	char *vaddr;
	char *dst;
	int i;
	int page_count;

	if (!obj->phys_obj)
		return;
	vaddr = obj->phys_obj->handle->vaddr;

	page_count = obj->base.size / PAGE_SIZE;
	VM_OBJECT_WLOCK(obj->base.vm_obj);
	for (i = 0; i < page_count; i++) {
		vm_page_t page = i915_gem_wire_page(obj->base.vm_obj, i, NULL);
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
	i915_gem_chipset_flush(dev);

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
		vm_page_t page = i915_gem_wire_page(obj->base.vm_obj, i, NULL);
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
	mtx_lock(&file_priv->mm.lock);
	while (!list_empty(&file_priv->mm.request_list)) {
		struct drm_i915_gem_request *request;

		request = list_first_entry(&file_priv->mm.request_list,
					   struct drm_i915_gem_request,
					   client_list);
		list_del(&request->client_list);
		request->file_priv = NULL;
	}
	mtx_unlock(&file_priv->mm.lock);
}

static void
i915_gem_inactive_shrink(void *arg)
{
	struct drm_device *dev = arg;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int pass1, pass2;

	if (!sx_try_xlock(&dev->dev_struct_lock)) {
		return;
	}

	CTR0(KTR_DRM, "gem_lowmem");

	pass1 = i915_gem_purge(dev_priv, -1);
	pass2 = __i915_gem_shrink(dev_priv, -1, false);

	if (pass2 <= pass1 / 100)
		i915_gem_shrink_all(dev_priv);

	DRM_UNLOCK(dev);
}

static vm_page_t
i915_gem_wire_page(vm_object_t object, vm_pindex_t pindex, bool *fresh)
{
	vm_page_t page;
	int rv;

	VM_OBJECT_ASSERT_WLOCKED(object);
	page = vm_page_grab(object, pindex, VM_ALLOC_NORMAL | VM_ALLOC_NOBUSY |
	    VM_ALLOC_WIRED);
	if (page->valid != VM_PAGE_BITS_ALL) {
		vm_page_xbusy(page);
		if (vm_pager_has_page(object, pindex, NULL, NULL)) {
			rv = vm_pager_get_pages(object, &page, 1, NULL, NULL);
			if (rv != VM_PAGER_OK) {
				vm_page_lock(page);
				vm_page_unwire(page, PQ_NONE);
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
		vm_page_xunbusy(page);
	} else if (fresh != NULL)
		*fresh = false;
	atomic_add_long(&i915_gem_wired_pages_cnt, 1);
	return (page);
}
