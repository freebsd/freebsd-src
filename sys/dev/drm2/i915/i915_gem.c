/*-
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

static void i915_gem_object_flush_cpu_write_domain(
    struct drm_i915_gem_object *obj);
static uint32_t i915_gem_get_gtt_size(struct drm_device *dev, uint32_t size,
    int tiling_mode);
static uint32_t i915_gem_get_gtt_alignment(struct drm_device *dev,
    uint32_t size, int tiling_mode);
static int i915_gem_object_bind_to_gtt(struct drm_i915_gem_object *obj,
    unsigned alignment, bool map_and_fenceable);
static int i915_gem_object_get_pages_gtt(struct drm_i915_gem_object *obj,
    int flags);
static void i915_gem_object_put_pages_gtt(struct drm_i915_gem_object *obj);
static int i915_gem_object_set_to_cpu_domain(struct drm_i915_gem_object *obj,
    bool write);
static void i915_gem_object_set_to_full_cpu_read_domain(
    struct drm_i915_gem_object *obj);
static int i915_gem_object_set_cpu_read_domain_range(
    struct drm_i915_gem_object *obj, uint64_t offset, uint64_t size);
static void i915_gem_object_finish_gtt(struct drm_i915_gem_object *obj);
static void i915_gem_object_truncate(struct drm_i915_gem_object *obj);
static int i915_gem_object_is_purgeable(struct drm_i915_gem_object *obj);
static bool i915_gem_object_is_inactive(struct drm_i915_gem_object *obj);
static int i915_gem_object_needs_bit17_swizzle(struct drm_i915_gem_object *obj);
static vm_page_t i915_gem_wire_page(vm_object_t object, vm_pindex_t pindex);
static void i915_gem_process_flushing_list(struct intel_ring_buffer *ring,
    uint32_t flush_domains);
static void i915_gem_clear_fence_reg(struct drm_device *dev,
    struct drm_i915_fence_reg *reg);
static void i915_gem_reset_fences(struct drm_device *dev);
static void i915_gem_retire_task_handler(void *arg, int pending);
static int i915_gem_phys_pwrite(struct drm_device *dev,
    struct drm_i915_gem_object *obj, uint64_t data_ptr, uint64_t offset,
    uint64_t size, struct drm_file *file_priv);
static void i915_gem_lowmem(void *arg);

MALLOC_DEFINE(DRM_I915_GEM, "i915gem", "Allocations from i915 gem");
long i915_gem_wired_pages_cnt;

static void
i915_gem_info_add_obj(struct drm_i915_private *dev_priv, size_t size)
{

	dev_priv->mm.object_count++;
	dev_priv->mm.object_memory += size;
}

static void
i915_gem_info_remove_obj(struct drm_i915_private *dev_priv, size_t size)
{

	dev_priv->mm.object_count--;
	dev_priv->mm.object_memory -= size;
}

static int
i915_gem_wait_for_error(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv;
	int ret;

	dev_priv = dev->dev_private;
	if (!atomic_load_acq_int(&dev_priv->mm.wedged))
		return (0);

	mtx_lock(&dev_priv->error_completion_lock);
	while (dev_priv->error_completion == 0) {
		ret = -msleep(&dev_priv->error_completion,
		    &dev_priv->error_completion_lock, PCATCH, "915wco", 0);
		if (ret != 0) {
			mtx_unlock(&dev_priv->error_completion_lock);
			return (ret);
		}
	}
	mtx_unlock(&dev_priv->error_completion_lock);

	if (atomic_load_acq_int(&dev_priv->mm.wedged)) {
		mtx_lock(&dev_priv->error_completion_lock);
		dev_priv->error_completion++;
		mtx_unlock(&dev_priv->error_completion_lock);
	}
	return (0);
}

int
i915_mutex_lock_interruptible(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv;
	int ret;

	dev_priv = dev->dev_private;
	ret = i915_gem_wait_for_error(dev);
	if (ret != 0)
		return (ret);

	/*
	 * interruptible shall it be. might indeed be if dev_lock is
	 * changed to sx
	 */
	ret = sx_xlock_sig(&dev->dev_struct_lock);
	if (ret != 0)
		return (-ret);

	return (0);
}


static void
i915_gem_free_object_tail(struct drm_i915_gem_object *obj)
{
	struct drm_device *dev;
	drm_i915_private_t *dev_priv;
	int ret;

	dev = obj->base.dev;
	dev_priv = dev->dev_private;

	ret = i915_gem_object_unbind(obj);
	if (ret == -ERESTART) {
		list_move(&obj->mm_list, &dev_priv->mm.deferred_free_list);
		return;
	}

	CTR1(KTR_DRM, "object_destroy_tail %p", obj);
	drm_gem_free_mmap_offset(&obj->base);
	drm_gem_object_release(&obj->base);
	i915_gem_info_remove_obj(dev_priv, obj->base.size);

	free(obj->page_cpu_valid, DRM_I915_GEM);
	free(obj->bit_17, DRM_I915_GEM);
	free(obj, DRM_I915_GEM);
}

void
i915_gem_free_object(struct drm_gem_object *gem_obj)
{
	struct drm_i915_gem_object *obj;
	struct drm_device *dev;

	obj = to_intel_bo(gem_obj);
	dev = obj->base.dev;

	while (obj->pin_count > 0)
		i915_gem_object_unpin(obj);

	if (obj->phys_obj != NULL)
		i915_gem_detach_phys_object(dev, obj);

	i915_gem_free_object_tail(obj);
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
	drm_i915_private_t *dev_priv;
	int i;

	dev_priv = dev->dev_private;

	INIT_LIST_HEAD(&dev_priv->mm.active_list);
	INIT_LIST_HEAD(&dev_priv->mm.flushing_list);
	INIT_LIST_HEAD(&dev_priv->mm.inactive_list);
	INIT_LIST_HEAD(&dev_priv->mm.pinned_list);
	INIT_LIST_HEAD(&dev_priv->mm.fence_list);
	INIT_LIST_HEAD(&dev_priv->mm.deferred_free_list);
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
		u32 tmp = I915_READ(MI_ARB_STATE);
		if (!(tmp & MI_ARB_C3_LP_WRITE_ENABLE)) {
			/*
			 * arb state is a masked write, so set bit +
			 * bit in mask.
			 */
			tmp = MI_ARB_C3_LP_WRITE_ENABLE |
			    (MI_ARB_C3_LP_WRITE_ENABLE << MI_ARB_MASK_SHIFT);
			I915_WRITE(MI_ARB_STATE, tmp);
		}
	}

	dev_priv->relative_constants_mode = I915_EXEC_CONSTANTS_REL_GENERAL;

	/* Old X drivers will take 0-2 for front, back, depth buffers */
	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		dev_priv->fence_reg_start = 3;

	if (INTEL_INFO(dev)->gen >= 4 || IS_I945G(dev) || IS_I945GM(dev) ||
	    IS_G33(dev))
		dev_priv->num_fence_regs = 16;
	else
		dev_priv->num_fence_regs = 8;

	/* Initialize fence registers to zero */
	for (i = 0; i < dev_priv->num_fence_regs; i++) {
		i915_gem_clear_fence_reg(dev, &dev_priv->fence_regs[i]);
	}
	i915_gem_detect_bit_6_swizzle(dev);
	dev_priv->mm.interruptible = true;

	dev_priv->mm.i915_lowmem = EVENTHANDLER_REGISTER(vm_lowmem,
	    i915_gem_lowmem, dev, EVENTHANDLER_PRI_ANY);
}

int
i915_gem_do_init(struct drm_device *dev, unsigned long start,
    unsigned long mappable_end, unsigned long end)
{
	drm_i915_private_t *dev_priv;
	unsigned long mappable;
	int error;

	dev_priv = dev->dev_private;
	mappable = min(end, mappable_end) - start;

	drm_mm_init(&dev_priv->mm.gtt_space, start, end - start);

	dev_priv->mm.gtt_start = start;
	dev_priv->mm.gtt_mappable_end = mappable_end;
	dev_priv->mm.gtt_end = end;
	dev_priv->mm.gtt_total = end - start;
	dev_priv->mm.mappable_gtt_total = mappable;

	/* Take over this portion of the GTT */
	intel_gtt_clear_range(start / PAGE_SIZE, (end-start) / PAGE_SIZE);
	device_printf(dev->device,
	    "taking over the fictitious range 0x%lx-0x%lx\n",
	    dev->agp->base + start, dev->agp->base + start + mappable);
	error = -vm_phys_fictitious_reg_range(dev->agp->base + start,
	    dev->agp->base + start + mappable, VM_MEMATTR_WRITE_COMBINING);
	return (error);
}

int
i915_gem_init_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file)
{
	struct drm_i915_gem_init *args;
	drm_i915_private_t *dev_priv;

	dev_priv = dev->dev_private;
	args = data;

	if (args->gtt_start >= args->gtt_end ||
	    (args->gtt_end | args->gtt_start) & (PAGE_SIZE - 1))
		return (-EINVAL);

	if (mtx_initialized(&dev_priv->mm.gtt_space.unused_lock))
		return (-EBUSY);
	/*
	 * XXXKIB. The second-time initialization should be guarded
	 * against.
	 */
	return (i915_gem_do_init(dev, args->gtt_start, args->gtt_end,
	    args->gtt_end));
}

int
i915_gem_idle(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv;
	int ret;

	dev_priv = dev->dev_private;
	if (dev_priv->mm.suspended)
		return (0);

	ret = i915_gpu_idle(dev, true);
	if (ret != 0)
		return (ret);

	/* Under UMS, be paranoid and evict. */
	if (!drm_core_check_feature(dev, DRIVER_MODESET)) {
		ret = i915_gem_evict_inactive(dev, false);
		if (ret != 0)
			return ret;
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

	/* Cancel the retire work handler, which should be idle now. */
	taskqueue_cancel_timeout(dev_priv->tq, &dev_priv->mm.retire_task, NULL);
	return (ret);
}

void
i915_gem_init_swizzling(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv;

	dev_priv = dev->dev_private;

	if (INTEL_INFO(dev)->gen < 5 ||
	    dev_priv->mm.bit_6_swizzle_x == I915_BIT_6_SWIZZLE_NONE)
		return;

	I915_WRITE(DISP_ARB_CTL, I915_READ(DISP_ARB_CTL) |
				 DISP_TILE_SURFACE_SWIZZLING);

	if (IS_GEN5(dev))
		return;

	I915_WRITE(TILECTL, I915_READ(TILECTL) | TILECTL_SWZCTL);
	if (IS_GEN6(dev))
		I915_WRITE(ARB_MODE, ARB_MODE_ENABLE(ARB_MODE_SWIZZLE_SNB));
	else
		I915_WRITE(ARB_MODE, ARB_MODE_ENABLE(ARB_MODE_SWIZZLE_IVB));
}

void
i915_gem_init_ppgtt(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv;
	struct i915_hw_ppgtt *ppgtt;
	uint32_t pd_offset, pd_entry;
	vm_paddr_t pt_addr;
	struct intel_ring_buffer *ring;
	u_int first_pd_entry_in_global_pt, i;

	dev_priv = dev->dev_private;
	ppgtt = dev_priv->mm.aliasing_ppgtt;
	if (ppgtt == NULL)
		return;

	first_pd_entry_in_global_pt = 512 * 1024 - I915_PPGTT_PD_ENTRIES;
	for (i = 0; i < ppgtt->num_pd_entries; i++) {
		pt_addr = VM_PAGE_TO_PHYS(ppgtt->pt_pages[i]);
		pd_entry = GEN6_PDE_ADDR_ENCODE(pt_addr);
		pd_entry |= GEN6_PDE_VALID;
		intel_gtt_write(first_pd_entry_in_global_pt + i, pd_entry);
	}
	intel_gtt_read_pte(first_pd_entry_in_global_pt);

	pd_offset = ppgtt->pd_offset;
	pd_offset /= 64; /* in cachelines, */
	pd_offset <<= 16;

	if (INTEL_INFO(dev)->gen == 6) {
		uint32_t ecochk = I915_READ(GAM_ECOCHK);
		I915_WRITE(GAM_ECOCHK, ecochk | ECOCHK_SNB_BIT |
				       ECOCHK_PPGTT_CACHE64B);
		I915_WRITE(GFX_MODE, GFX_MODE_ENABLE(GFX_PPGTT_ENABLE));
	} else if (INTEL_INFO(dev)->gen >= 7) {
		I915_WRITE(GAM_ECOCHK, ECOCHK_PPGTT_CACHE64B);
		/* GFX_MODE is per-ring on gen7+ */
	}

	for (i = 0; i < I915_NUM_RINGS; i++) {
		ring = &dev_priv->rings[i];

		if (INTEL_INFO(dev)->gen >= 7)
			I915_WRITE(RING_MODE_GEN7(ring),
				   GFX_MODE_ENABLE(GFX_PPGTT_ENABLE));

		I915_WRITE(RING_PP_DIR_DCLV(ring), PP_DIR_DCLV_2G);
		I915_WRITE(RING_PP_DIR_BASE(ring), pd_offset);
	}
}

int
i915_gem_init_hw(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv;
	int ret;

	dev_priv = dev->dev_private;

	i915_gem_init_swizzling(dev);

	ret = intel_init_render_ring_buffer(dev);
	if (ret != 0)
		return (ret);

	if (HAS_BSD(dev)) {
		ret = intel_init_bsd_ring_buffer(dev);
		if (ret != 0)
			goto cleanup_render_ring;
	}

	if (HAS_BLT(dev)) {
		ret = intel_init_blt_ring_buffer(dev);
		if (ret != 0)
			goto cleanup_bsd_ring;
	}

	dev_priv->next_seqno = 1;
	i915_gem_init_ppgtt(dev);
	return (0);

cleanup_bsd_ring:
	intel_cleanup_ring_buffer(&dev_priv->rings[VCS]);
cleanup_render_ring:
	intel_cleanup_ring_buffer(&dev_priv->rings[RCS]);
	return (ret);
}

int
i915_gem_get_aperture_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file)
{
	struct drm_i915_private *dev_priv;
	struct drm_i915_gem_get_aperture *args;
	struct drm_i915_gem_object *obj;
	size_t pinned;

	dev_priv = dev->dev_private;
	args = data;

	if (!(dev->driver->driver_features & DRIVER_GEM))
		return (-ENODEV);

	pinned = 0;
	DRM_LOCK(dev);
	list_for_each_entry(obj, &dev_priv->mm.pinned_list, mm_list)
		pinned += obj->gtt_space->size;
	DRM_UNLOCK(dev);

	args->aper_size = dev_priv->mm.gtt_total;
	args->aper_available_size = args->aper_size - pinned;

	return (0);
}

int
i915_gem_object_pin(struct drm_i915_gem_object *obj, uint32_t alignment,
     bool map_and_fenceable)
{
	struct drm_device *dev;
	struct drm_i915_private *dev_priv;
	int ret;

	dev = obj->base.dev;
	dev_priv = dev->dev_private;

	KASSERT(obj->pin_count != DRM_I915_GEM_OBJECT_MAX_PIN_COUNT,
	    ("Max pin count"));

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
			if (ret != 0)
				return (ret);
		}
	}

	if (obj->gtt_space == NULL) {
		ret = i915_gem_object_bind_to_gtt(obj, alignment,
		    map_and_fenceable);
		if (ret)
			return (ret);
	}

	if (obj->pin_count++ == 0 && !obj->active)
		list_move_tail(&obj->mm_list, &dev_priv->mm.pinned_list);
	obj->pin_mappable |= map_and_fenceable;

#if 1
	KIB_NOTYET();
#else
	WARN_ON(i915_verify_lists(dev));
#endif
	return (0);
}

void
i915_gem_object_unpin(struct drm_i915_gem_object *obj)
{
	struct drm_device *dev;
	drm_i915_private_t *dev_priv;

	dev = obj->base.dev;
	dev_priv = dev->dev_private;

#if 1
	KIB_NOTYET();
#else
	WARN_ON(i915_verify_lists(dev));
#endif
	
	KASSERT(obj->pin_count != 0, ("zero pin count"));
	KASSERT(obj->gtt_space != NULL, ("No gtt mapping"));

	if (--obj->pin_count == 0) {
		if (!obj->active)
			list_move_tail(&obj->mm_list,
			    &dev_priv->mm.inactive_list);
		obj->pin_mappable = false;
	}
#if 1
	KIB_NOTYET();
#else
	WARN_ON(i915_verify_lists(dev));
#endif
}

int
i915_gem_pin_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file)
{
	struct drm_i915_gem_pin *args;
	struct drm_i915_gem_object *obj;
	struct drm_gem_object *gobj;
	int ret;

	args = data;

	ret = i915_mutex_lock_interruptible(dev);
	if (ret != 0)
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
		if (ret != 0)
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
	return (ret);
}

int
i915_gem_unpin_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file)
{
	struct drm_i915_gem_pin *args;
	struct drm_i915_gem_object *obj;
	int ret;

	args = data;
	ret = i915_mutex_lock_interruptible(dev);
	if (ret != 0)
		return (ret);

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
	return (ret);
}

int
i915_gem_busy_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file)
{
	struct drm_i915_gem_busy *args;
	struct drm_i915_gem_object *obj;
	struct drm_i915_gem_request *request;
	int ret;

	args = data;

	ret = i915_mutex_lock_interruptible(dev);
	if (ret != 0)
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
		} else if (obj->ring->outstanding_lazy_request ==
		    obj->last_rendering_seqno) {
			request = malloc(sizeof(*request), DRM_I915_GEM,
			    M_WAITOK | M_ZERO);
			ret = i915_add_request(obj->ring, NULL, request);
			if (ret != 0)
				free(request, DRM_I915_GEM);
		}

		i915_gem_retire_requests_ring(obj->ring);
		args->busy = obj->active;
	}

	drm_gem_object_unreference(&obj->base);
unlock:
	DRM_UNLOCK(dev);
	return (ret);
}

static int
i915_gem_ring_throttle(struct drm_device *dev, struct drm_file *file)
{
	struct drm_i915_private *dev_priv;
	struct drm_i915_file_private *file_priv;
	unsigned long recent_enough;
	struct drm_i915_gem_request *request;
	struct intel_ring_buffer *ring;
	u32 seqno;
	int ret;

	dev_priv = dev->dev_private;
	if (atomic_load_acq_int(&dev_priv->mm.wedged))
		return (-EIO);

	file_priv = file->driver_priv;
	recent_enough = ticks - (20 * hz / 1000);
	ring = NULL;
	seqno = 0;

	mtx_lock(&file_priv->mm.lck);
	list_for_each_entry(request, &file_priv->mm.request_list, client_list) {
		if (time_after_eq(request->emitted_jiffies, recent_enough))
			break;
		ring = request->ring;
		seqno = request->seqno;
	}
	mtx_unlock(&file_priv->mm.lck);
	if (seqno == 0)
		return (0);

	ret = 0;
	mtx_lock(&ring->irq_lock);
	if (!i915_seqno_passed(ring->get_seqno(ring), seqno)) {
		if (ring->irq_get(ring)) {
			while (ret == 0 &&
			    !(i915_seqno_passed(ring->get_seqno(ring), seqno) ||
			    atomic_load_acq_int(&dev_priv->mm.wedged)))
				ret = -msleep(ring, &ring->irq_lock, PCATCH,
				    "915thr", 0);
			ring->irq_put(ring);
			if (ret == 0 && atomic_load_acq_int(&dev_priv->mm.wedged))
				ret = -EIO;
		} else if (_intel_wait_for(dev,
		    i915_seqno_passed(ring->get_seqno(ring), seqno) ||
		    atomic_load_acq_int(&dev_priv->mm.wedged), 3000, 0, "915rtr")) {
			ret = -EBUSY;
		}
	}
	mtx_unlock(&ring->irq_lock);

	if (ret == 0)
		taskqueue_enqueue_timeout(dev_priv->tq,
		    &dev_priv->mm.retire_task, 0);

	return (ret);
}

int
i915_gem_throttle_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{

	return (i915_gem_ring_throttle(dev, file_priv));
}

int
i915_gem_madvise_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	struct drm_i915_gem_madvise *args;
	struct drm_i915_gem_object *obj;
	int ret;

	args = data;
	switch (args->madv) {
	case I915_MADV_DONTNEED:
	case I915_MADV_WILLNEED:
		break;
	default:
		return (-EINVAL);
	}

	ret = i915_mutex_lock_interruptible(dev);
	if (ret != 0)
		return (ret);

	obj = to_intel_bo(drm_gem_object_lookup(dev, file_priv, args->handle));
	if (&obj->base == NULL) {
		ret = -ENOENT;
		goto unlock;
	}

	if (obj->pin_count != 0) {
		ret = -EINVAL;
		goto out;
	}

	if (obj->madv != I915_MADV_PURGED_INTERNAL)
		obj->madv = args->madv;
	if (i915_gem_object_is_purgeable(obj) && obj->gtt_space == NULL)
		i915_gem_object_truncate(obj);
	args->retained = obj->madv != I915_MADV_PURGED_INTERNAL;

out:
	drm_gem_object_unreference(&obj->base);
unlock:
	DRM_UNLOCK(dev);
	return (ret);
}

void
i915_gem_cleanup_ringbuffer(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv;
	int i;

	dev_priv = dev->dev_private;
	for (i = 0; i < I915_NUM_RINGS; i++)
		intel_cleanup_ring_buffer(&dev_priv->rings[i]);
}

int
i915_gem_entervt_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv;
	int ret, i;

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		return (0);
	dev_priv = dev->dev_private;
	if (atomic_load_acq_int(&dev_priv->mm.wedged) != 0) {
		DRM_ERROR("Reenabling wedged hardware, good luck\n");
		atomic_store_rel_int(&dev_priv->mm.wedged, 0);
	}

	dev_priv->mm.suspended = 0;

	ret = i915_gem_init_hw(dev);
	if (ret != 0) {
		return (ret);
	}

	KASSERT(list_empty(&dev_priv->mm.active_list), ("active list"));
	KASSERT(list_empty(&dev_priv->mm.flushing_list), ("flushing list"));
	KASSERT(list_empty(&dev_priv->mm.inactive_list), ("inactive list"));
	for (i = 0; i < I915_NUM_RINGS; i++) {
		KASSERT(list_empty(&dev_priv->rings[i].active_list),
		    ("ring %d active list", i));
		KASSERT(list_empty(&dev_priv->rings[i].request_list),
		    ("ring %d request list", i));
	}

	DRM_UNLOCK(dev);
	ret = drm_irq_install(dev);
	DRM_LOCK(dev);
	if (ret)
		goto cleanup_ringbuffer;

	return (0);

cleanup_ringbuffer:
	i915_gem_cleanup_ringbuffer(dev);
	dev_priv->mm.suspended = 1;

	return (ret);
}

int
i915_gem_leavevt_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file_priv)
{

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		return 0;

	drm_irq_uninstall(dev);
	return (i915_gem_idle(dev));
}

int
i915_gem_create(struct drm_file *file, struct drm_device *dev, uint64_t size,
    uint32_t *handle_p)
{
	struct drm_i915_gem_object *obj;
	uint32_t handle;
	int ret;

	size = roundup(size, PAGE_SIZE);
	if (size == 0)
		return (-EINVAL);

	obj = i915_gem_alloc_object(dev, size);
	if (obj == NULL)
		return (-ENOMEM);

	handle = 0;
	ret = drm_gem_handle_create(file, &obj->base, &handle);
	if (ret != 0) {
		drm_gem_object_release(&obj->base);
		i915_gem_info_remove_obj(dev->dev_private, obj->base.size);
		free(obj, DRM_I915_GEM);
		return (-ret);
	}

	/* drop reference from allocate - handle holds it now */
	drm_gem_object_unreference(&obj->base);
	CTR2(KTR_DRM, "object_create %p %x", obj, size);
	*handle_p = handle;
	return (0);
}

int
i915_gem_dumb_create(struct drm_file *file, struct drm_device *dev,
    struct drm_mode_create_dumb *args)
{

	/* have to work out size/pitch and return them */
	args->pitch = roundup2(args->width * ((args->bpp + 7) / 8), 64);
	args->size = args->pitch * args->height;
	return (i915_gem_create(file, dev, args->size, &args->handle));
}

int
i915_gem_dumb_destroy(struct drm_file *file, struct drm_device *dev,
    uint32_t handle)
{

	return (drm_gem_handle_delete(file, handle));
}

int
i915_gem_create_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file)
{
	struct drm_i915_gem_create *args = data;

	return (i915_gem_create(file, dev, args->size, &args->handle));
}

static int
i915_gem_swap_io(struct drm_device *dev, struct drm_i915_gem_object *obj,
    uint64_t data_ptr, uint64_t size, uint64_t offset, enum uio_rw rw,
    struct drm_file *file)
{
	vm_object_t vm_obj;
	vm_page_t m;
	struct sf_buf *sf;
	vm_offset_t mkva;
	vm_pindex_t obj_pi;
	int cnt, do_bit17_swizzling, length, obj_po, ret, swizzled_po;

	if (obj->gtt_offset != 0 && rw == UIO_READ)
		do_bit17_swizzling = i915_gem_object_needs_bit17_swizzle(obj);
	else
		do_bit17_swizzling = 0;

	obj->dirty = 1;
	vm_obj = obj->base.vm_obj;
	ret = 0;

	VM_OBJECT_WLOCK(vm_obj);
	vm_object_pip_add(vm_obj, 1);
	while (size > 0) {
		obj_pi = OFF_TO_IDX(offset);
		obj_po = offset & PAGE_MASK;

		m = i915_gem_wire_page(vm_obj, obj_pi);
		VM_OBJECT_WUNLOCK(vm_obj);

		sched_pin();
		sf = sf_buf_alloc(m, SFB_CPUPRIVATE);
		mkva = sf_buf_kva(sf);
		length = min(size, PAGE_SIZE - obj_po);
		while (length > 0) {
			if (do_bit17_swizzling &&
			    (VM_PAGE_TO_PHYS(m) & (1 << 17)) != 0) {
				cnt = roundup2(obj_po + 1, 64);
				cnt = min(cnt - obj_po, length);
				swizzled_po = obj_po ^ 64;
			} else {
				cnt = length;
				swizzled_po = obj_po;
			}
			if (rw == UIO_READ)
				ret = -copyout_nofault(
				    (char *)mkva + swizzled_po,
				    (void *)(uintptr_t)data_ptr, cnt);
			else
				ret = -copyin_nofault(
				    (void *)(uintptr_t)data_ptr,
				    (char *)mkva + swizzled_po, cnt);
			if (ret != 0)
				break;
			data_ptr += cnt;
			size -= cnt;
			length -= cnt;
			offset += cnt;
			obj_po += cnt;
		}
		sf_buf_free(sf);
		sched_unpin();
		VM_OBJECT_WLOCK(vm_obj);
		if (rw == UIO_WRITE)
			vm_page_dirty(m);
		vm_page_reference(m);
		vm_page_lock(m);
		vm_page_unwire(m, PQ_ACTIVE);
		vm_page_unlock(m);
		atomic_add_long(&i915_gem_wired_pages_cnt, -1);

		if (ret != 0)
			break;
	}
	vm_object_pip_wakeup(vm_obj);
	VM_OBJECT_WUNLOCK(vm_obj);

	return (ret);
}

static int
i915_gem_gtt_write(struct drm_device *dev, struct drm_i915_gem_object *obj,
    uint64_t data_ptr, uint64_t size, uint64_t offset, struct drm_file *file)
{
	vm_offset_t mkva;
	vm_pindex_t obj_pi;
	int obj_po, ret;

	obj_pi = OFF_TO_IDX(offset);
	obj_po = offset & PAGE_MASK;

	mkva = (vm_offset_t)pmap_mapdev_attr(dev->agp->base + obj->gtt_offset +
	    IDX_TO_OFF(obj_pi), size, PAT_WRITE_COMBINING);
	ret = -copyin_nofault((void *)(uintptr_t)data_ptr, (char *)mkva +
	    obj_po, size);
	pmap_unmapdev(mkva, size);
	return (ret);
}

static int
i915_gem_obj_io(struct drm_device *dev, uint32_t handle, uint64_t data_ptr,
    uint64_t size, uint64_t offset, enum uio_rw rw, struct drm_file *file)
{
	struct drm_i915_gem_object *obj;
	vm_page_t *ma;
	vm_offset_t start, end;
	int npages, ret;

	if (size == 0)
		return (0);
	start = trunc_page(data_ptr);
	end = round_page(data_ptr + size);
	npages = howmany(end - start, PAGE_SIZE);
	ma = malloc(npages * sizeof(vm_page_t), DRM_I915_GEM, M_WAITOK |
	    M_ZERO);
	npages = vm_fault_quick_hold_pages(&curproc->p_vmspace->vm_map,
	    (vm_offset_t)data_ptr, size,
	    (rw == UIO_READ ? VM_PROT_WRITE : 0 ) | VM_PROT_READ, ma, npages);
	if (npages == -1) {
		ret = -EFAULT;
		goto free_ma;
	}

	ret = i915_mutex_lock_interruptible(dev);
	if (ret != 0)
		goto unlocked;

	obj = to_intel_bo(drm_gem_object_lookup(dev, file, handle));
	if (&obj->base == NULL) {
		ret = -ENOENT;
		goto unlock;
	}
	if (offset > obj->base.size || size > obj->base.size - offset) {
		ret = -EINVAL;
		goto out;
	}

	if (rw == UIO_READ) {
		CTR3(KTR_DRM, "object_pread %p %jx %jx", obj, offset, size);
		ret = i915_gem_object_set_cpu_read_domain_range(obj,
		    offset, size);
		if (ret != 0)
			goto out;
		ret = i915_gem_swap_io(dev, obj, data_ptr, size, offset,
		    UIO_READ, file);
	} else {
		if (obj->phys_obj) {
			CTR3(KTR_DRM, "object_phys_write %p %jx %jx", obj,
			    offset, size);
			ret = i915_gem_phys_pwrite(dev, obj, data_ptr, offset,
			    size, file);
		} else if (obj->gtt_space &&
		    obj->base.write_domain != I915_GEM_DOMAIN_CPU) {
			CTR3(KTR_DRM, "object_gtt_write %p %jx %jx", obj,
			    offset, size);
			ret = i915_gem_object_pin(obj, 0, true);
			if (ret != 0)
				goto out;
			ret = i915_gem_object_set_to_gtt_domain(obj, true);
			if (ret != 0)
				goto out_unpin;
			ret = i915_gem_object_put_fence(obj);
			if (ret != 0)
				goto out_unpin;
			ret = i915_gem_gtt_write(dev, obj, data_ptr, size,
			    offset, file);
out_unpin:
			i915_gem_object_unpin(obj);
		} else {
			CTR3(KTR_DRM, "object_pwrite %p %jx %jx", obj,
			    offset, size);
			ret = i915_gem_object_set_to_cpu_domain(obj, true);
			if (ret != 0)
				goto out;
			ret = i915_gem_swap_io(dev, obj, data_ptr, size, offset,
			    UIO_WRITE, file);
		}
	}
out:
	drm_gem_object_unreference(&obj->base);
unlock:
	DRM_UNLOCK(dev);
unlocked:
	vm_page_unhold_pages(ma, npages);
free_ma:
	free(ma, DRM_I915_GEM);
	return (ret);
}

int
i915_gem_pread_ioctl(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct drm_i915_gem_pread *args;

	args = data;
	return (i915_gem_obj_io(dev, args->handle, args->data_ptr, args->size,
	    args->offset, UIO_READ, file));
}

int
i915_gem_pwrite_ioctl(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct drm_i915_gem_pwrite *args;

	args = data;
	return (i915_gem_obj_io(dev, args->handle, args->data_ptr, args->size,
	    args->offset, UIO_WRITE, file));
}

int
i915_gem_set_domain_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file)
{
	struct drm_i915_gem_set_domain *args;
	struct drm_i915_gem_object *obj;
	uint32_t read_domains;
	uint32_t write_domain;
	int ret;

	if ((dev->driver->driver_features & DRIVER_GEM) == 0)
		return (-ENODEV);

	args = data;
	read_domains = args->read_domains;
	write_domain = args->write_domain;

	if ((write_domain & I915_GEM_GPU_DOMAINS) != 0 ||
	    (read_domains & I915_GEM_GPU_DOMAINS) != 0 ||
	    (write_domain != 0 && read_domains != write_domain))
		return (-EINVAL);

	ret = i915_mutex_lock_interruptible(dev);
	if (ret != 0)
		return (ret);

	obj = to_intel_bo(drm_gem_object_lookup(dev, file, args->handle));
	if (&obj->base == NULL) {
		ret = -ENOENT;
		goto unlock;
	}

	if ((read_domains & I915_GEM_DOMAIN_GTT) != 0) {
		ret = i915_gem_object_set_to_gtt_domain(obj, write_domain != 0);
		if (ret == -EINVAL)
			ret = 0;
	} else
		ret = i915_gem_object_set_to_cpu_domain(obj, write_domain != 0);

	drm_gem_object_unreference(&obj->base);
unlock:
	DRM_UNLOCK(dev);
	return (ret);
}

int
i915_gem_sw_finish_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file)
{
	struct drm_i915_gem_sw_finish *args;
	struct drm_i915_gem_object *obj;
	int ret;

	args = data;
	ret = 0;
	if ((dev->driver->driver_features & DRIVER_GEM) == 0)
		return (ENODEV);
	ret = i915_mutex_lock_interruptible(dev);
	if (ret != 0)
		return (ret);
	obj = to_intel_bo(drm_gem_object_lookup(dev, file, args->handle));
	if (&obj->base == NULL) {
		ret = -ENOENT;
		goto unlock;
	}
	if (obj->pin_count != 0)
		i915_gem_object_flush_cpu_write_domain(obj);
	drm_gem_object_unreference(&obj->base);
unlock:
	DRM_UNLOCK(dev);
	return (ret);
}

int
i915_gem_mmap_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file)
{
	struct drm_i915_gem_mmap *args;
	struct drm_gem_object *obj;
	struct proc *p;
	vm_map_t map;
	vm_offset_t addr;
	vm_size_t size;
	int error, rv;

	args = data;

	if ((dev->driver->driver_features & DRIVER_GEM) == 0)
		return (-ENODEV);

	obj = drm_gem_object_lookup(dev, file, args->handle);
	if (obj == NULL)
		return (-ENOENT);
	error = 0;
	if (args->size == 0)
		goto out;
	p = curproc;
	map = &p->p_vmspace->vm_map;
	size = round_page(args->size);
	PROC_LOCK(p);
	if (map->size + size > lim_cur(p, RLIMIT_VMEM)) {
		PROC_UNLOCK(p);
		error = ENOMEM;
		goto out;
	}
	PROC_UNLOCK(p);

	addr = 0;
	vm_object_reference(obj->vm_obj);
	DRM_UNLOCK(dev);
	rv = vm_map_find(map, obj->vm_obj, args->offset, &addr, args->size, 0,
	    VMFS_OPTIMAL_SPACE, VM_PROT_READ | VM_PROT_WRITE,
	    VM_PROT_READ | VM_PROT_WRITE, MAP_INHERIT_SHARE);
	if (rv != KERN_SUCCESS) {
		vm_object_deallocate(obj->vm_obj);
		error = -vm_mmap_to_errno(rv);
	} else {
		args->addr_ptr = (uint64_t)addr;
	}
	DRM_LOCK(dev);
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

int i915_intr_pf;

static int
i915_gem_pager_fault(vm_object_t vm_obj, vm_ooffset_t offset, int prot,
    vm_page_t *mres)
{
	struct drm_gem_object *gem_obj;
	struct drm_i915_gem_object *obj;
	struct drm_device *dev;
	drm_i915_private_t *dev_priv;
	vm_page_t m, oldm;
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
		oldm = *mres;
		vm_page_lock(oldm);
		vm_page_remove(oldm);
		vm_page_unlock(oldm);
		*mres = NULL;
	} else
		oldm = NULL;
	VM_OBJECT_WUNLOCK(vm_obj);
retry:
	cause = ret = 0;
	m = NULL;

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
	m = vm_page_lookup(vm_obj, OFF_TO_IDX(offset));
	if (m != NULL) {
		if (vm_page_busied(m)) {
			DRM_UNLOCK(dev);
			vm_page_lock(m);
			VM_OBJECT_WUNLOCK(vm_obj);
			vm_page_busy_sleep(m, "915pee");
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

	if (obj->tiling_mode == I915_TILING_NONE)
		ret = i915_gem_object_put_fence(obj);
	else
		ret = i915_gem_object_get_fence(obj, NULL);
	if (ret != 0) {
		cause = 50;
		goto unlock;
	}

	if (i915_gem_object_is_inactive(obj))
		list_move_tail(&obj->mm_list, &dev_priv->mm.inactive_list);

	obj->fault_mappable = true;
	VM_OBJECT_WLOCK(vm_obj);
	m = PHYS_TO_VM_PAGE(dev->agp->base + obj->gtt_offset + offset);
	KASSERT((m->flags & PG_FICTITIOUS) != 0,
	    ("physical address %#jx not fictitious",
	    (uintmax_t)(dev->agp->base + obj->gtt_offset + offset)));
	if (m == NULL) {
		VM_OBJECT_WUNLOCK(vm_obj);
		cause = 60;
		ret = -EFAULT;
		goto unlock;
	}
	KASSERT((m->flags & PG_FICTITIOUS) != 0,
	    ("not fictitious %p", m));
	KASSERT(m->wire_count == 1, ("wire_count not 1 %p", m));

	if (vm_page_busied(m)) {
		DRM_UNLOCK(dev);
		vm_page_lock(m);
		VM_OBJECT_WUNLOCK(vm_obj);
		vm_page_busy_sleep(m, "915pbs");
		goto retry;
	}
	if (vm_page_insert(m, vm_obj, OFF_TO_IDX(offset))) {
		DRM_UNLOCK(dev);
		VM_OBJECT_WUNLOCK(vm_obj);
		VM_WAIT;
		goto retry;
	}
	m->valid = VM_PAGE_BITS_ALL;
have_page:
	*mres = m;
	vm_page_xbusy(m);

	CTR4(KTR_DRM, "fault %p %jx %x phys %x", gem_obj, offset, prot,
	    m->phys_addr);
	DRM_UNLOCK(dev);
	if (oldm != NULL) {
		vm_page_lock(oldm);
		vm_page_free(oldm);
		vm_page_unlock(oldm);
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

int
i915_gem_mmap_gtt(struct drm_file *file, struct drm_device *dev,
    uint32_t handle, uint64_t *offset)
{
	struct drm_i915_private *dev_priv;
	struct drm_i915_gem_object *obj;
	int ret;

	if (!(dev->driver->driver_features & DRIVER_GEM))
		return (-ENODEV);

	dev_priv = dev->dev_private;

	ret = i915_mutex_lock_interruptible(dev);
	if (ret != 0)
		return (ret);

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
	if (ret != 0)
		goto out;

	*offset = DRM_GEM_MAPPING_OFF(obj->base.map_list.key) |
	    DRM_GEM_MAPPING_KEY;
out:
	drm_gem_object_unreference(&obj->base);
unlock:
	DRM_UNLOCK(dev);
	return (ret);
}

int
i915_gem_mmap_gtt_ioctl(struct drm_device *dev, void *data,
    struct drm_file *file)
{
	struct drm_i915_private *dev_priv;
	struct drm_i915_gem_mmap_gtt *args;

	dev_priv = dev->dev_private;
	args = data;

	return (i915_gem_mmap_gtt(file, dev, args->handle, &args->offset));
}

struct drm_i915_gem_object *
i915_gem_alloc_object(struct drm_device *dev, size_t size)
{
	struct drm_i915_private *dev_priv;
	struct drm_i915_gem_object *obj;

	dev_priv = dev->dev_private;

	obj = malloc(sizeof(*obj), DRM_I915_GEM, M_WAITOK | M_ZERO);

	if (drm_gem_object_init(dev, &obj->base, size) != 0) {
		free(obj, DRM_I915_GEM);
		return (NULL);
	}

	obj->base.write_domain = I915_GEM_DOMAIN_CPU;
	obj->base.read_domains = I915_GEM_DOMAIN_CPU;

	if (HAS_LLC(dev))
		obj->cache_level = I915_CACHE_LLC;
	else
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

	return (obj);
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

static void
i915_gem_object_flush_gtt_write_domain(struct drm_i915_gem_object *obj)
{
	uint32_t old_write_domain;

	if (obj->base.write_domain != I915_GEM_DOMAIN_GTT)
		return;

	wmb();

	old_write_domain = obj->base.write_domain;
	obj->base.write_domain = 0;

	CTR3(KTR_DRM, "object_change_domain flush gtt_write %p %x %x", obj,
	    obj->base.read_domains, old_write_domain);
}

int
i915_gem_object_set_to_gtt_domain(struct drm_i915_gem_object *obj, bool write)
{
	uint32_t old_write_domain, old_read_domains;
	int ret;

	if (obj->gtt_space == NULL)
		return (-EINVAL);

	if (obj->base.write_domain == I915_GEM_DOMAIN_GTT)
		return 0;

	ret = i915_gem_object_flush_gpu_write_domain(obj);
	if (ret != 0)
		return (ret);

	if (obj->pending_gpu_write || write) {
		ret = i915_gem_object_wait_rendering(obj);
		if (ret != 0)
			return (ret);
	}

	i915_gem_object_flush_cpu_write_domain(obj);

	old_write_domain = obj->base.write_domain;
	old_read_domains = obj->base.read_domains;

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
	return (0);
}

int
i915_gem_object_set_cache_level(struct drm_i915_gem_object *obj,
    enum i915_cache_level cache_level)
{
	struct drm_device *dev;
	drm_i915_private_t *dev_priv;
	int ret;

	if (obj->cache_level == cache_level)
		return 0;

	if (obj->pin_count) {
		DRM_DEBUG("can not change the cache level of pinned objects\n");
		return (-EBUSY);
	}

	dev = obj->base.dev;
	dev_priv = dev->dev_private;
	if (obj->gtt_space) {
		ret = i915_gem_object_finish_gpu(obj);
		if (ret != 0)
			return (ret);

		i915_gem_object_finish_gtt(obj);

		/* Before SandyBridge, you could not use tiling or fence
		 * registers with snooped memory, so relinquish any fences
		 * currently pointing to our region in the aperture.
		 */
		if (INTEL_INFO(obj->base.dev)->gen < 6) {
			ret = i915_gem_object_put_fence(obj);
			if (ret != 0)
				return (ret);
		}

		i915_gem_gtt_rebind_object(obj, cache_level);
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
	return (0);
}

int
i915_gem_object_pin_to_display_plane(struct drm_i915_gem_object *obj,
    u32 alignment, struct intel_ring_buffer *pipelined)
{
	u32 old_read_domains, old_write_domain;
	int ret;

	ret = i915_gem_object_flush_gpu_write_domain(obj);
	if (ret != 0)
		return (ret);

	if (pipelined != obj->ring) {
		ret = i915_gem_object_wait_rendering(obj);
		if (ret == -ERESTART || ret == -EINTR)
			return (ret);
	}

	ret = i915_gem_object_set_cache_level(obj, I915_CACHE_NONE);
	if (ret != 0)
		return (ret);

	ret = i915_gem_object_pin(obj, alignment, true);
	if (ret != 0)
		return (ret);

	i915_gem_object_flush_cpu_write_domain(obj);

	old_write_domain = obj->base.write_domain;
	old_read_domains = obj->base.read_domains;

	KASSERT((obj->base.write_domain & ~I915_GEM_DOMAIN_GTT) == 0,
	    ("obj %p in GTT write domain", obj));
	obj->base.read_domains |= I915_GEM_DOMAIN_GTT;

	CTR3(KTR_DRM, "object_change_domain pin_to_display_plan %p %x %x",
	    obj, old_read_domains, obj->base.write_domain);
	return (0);
}

int
i915_gem_object_finish_gpu(struct drm_i915_gem_object *obj)
{
	int ret;

	if ((obj->base.read_domains & I915_GEM_GPU_DOMAINS) == 0)
		return (0);

	if (obj->base.write_domain & I915_GEM_GPU_DOMAINS) {
		ret = i915_gem_flush_ring(obj->ring, 0, obj->base.write_domain);
		if (ret != 0)
			return (ret);
	}

	ret = i915_gem_object_wait_rendering(obj);
	if (ret != 0)
		return (ret);

	obj->base.read_domains &= ~I915_GEM_GPU_DOMAINS;

	return (0);
}

static int
i915_gem_object_set_to_cpu_domain(struct drm_i915_gem_object *obj, bool write)
{
	uint32_t old_write_domain, old_read_domains;
	int ret;

	if (obj->base.write_domain == I915_GEM_DOMAIN_CPU)
		return 0;

	ret = i915_gem_object_flush_gpu_write_domain(obj);
	if (ret != 0)
		return (ret);

	ret = i915_gem_object_wait_rendering(obj);
	if (ret != 0)
		return (ret);

	i915_gem_object_flush_gtt_write_domain(obj);
	i915_gem_object_set_to_full_cpu_read_domain(obj);

	old_write_domain = obj->base.write_domain;
	old_read_domains = obj->base.read_domains;

	if ((obj->base.read_domains & I915_GEM_DOMAIN_CPU) == 0) {
		i915_gem_clflush_object(obj);
		obj->base.read_domains |= I915_GEM_DOMAIN_CPU;
	}

	KASSERT((obj->base.write_domain & ~I915_GEM_DOMAIN_CPU) == 0,
	    ("In cpu write domain"));

	if (write) {
		obj->base.read_domains = I915_GEM_DOMAIN_CPU;
		obj->base.write_domain = I915_GEM_DOMAIN_CPU;
	}

	CTR3(KTR_DRM, "object_change_domain set_to_cpu %p %x %x", obj,
	    old_read_domains, old_write_domain);
	return (0);
}

static void
i915_gem_object_set_to_full_cpu_read_domain(struct drm_i915_gem_object *obj)
{
	int i;

	if (obj->page_cpu_valid == NULL)
		return;

	if ((obj->base.read_domains & I915_GEM_DOMAIN_CPU) != 0) {
		for (i = 0; i <= (obj->base.size - 1) / PAGE_SIZE; i++) {
			if (obj->page_cpu_valid[i] != 0)
				continue;
			drm_clflush_pages(obj->pages + i, 1);
		}
	}

	free(obj->page_cpu_valid, DRM_I915_GEM);
	obj->page_cpu_valid = NULL;
}

static int
i915_gem_object_set_cpu_read_domain_range(struct drm_i915_gem_object *obj,
    uint64_t offset, uint64_t size)
{
	uint32_t old_read_domains;
	int i, ret;

	if (offset == 0 && size == obj->base.size)
		return (i915_gem_object_set_to_cpu_domain(obj, 0));

	ret = i915_gem_object_flush_gpu_write_domain(obj);
	if (ret != 0)
		return (ret);
	ret = i915_gem_object_wait_rendering(obj);
	if (ret != 0)
		return (ret);

	i915_gem_object_flush_gtt_write_domain(obj);

	if (obj->page_cpu_valid == NULL &&
	    (obj->base.read_domains & I915_GEM_DOMAIN_CPU) != 0)
		return (0);

	if (obj->page_cpu_valid == NULL) {
		obj->page_cpu_valid = malloc(obj->base.size / PAGE_SIZE,
		    DRM_I915_GEM, M_WAITOK | M_ZERO);
	} else if ((obj->base.read_domains & I915_GEM_DOMAIN_CPU) == 0)
		memset(obj->page_cpu_valid, 0, obj->base.size / PAGE_SIZE);

	for (i = offset / PAGE_SIZE; i <= (offset + size - 1) / PAGE_SIZE;
	     i++) {
		if (obj->page_cpu_valid[i])
			continue;
		drm_clflush_pages(obj->pages + i, 1);
		obj->page_cpu_valid[i] = 1;
	}

	KASSERT((obj->base.write_domain & ~I915_GEM_DOMAIN_CPU) == 0,
	    ("In gpu write domain"));

	old_read_domains = obj->base.read_domains;
	obj->base.read_domains |= I915_GEM_DOMAIN_CPU;

	CTR3(KTR_DRM, "object_change_domain set_cpu_read %p %x %x", obj,
	    old_read_domains, obj->base.write_domain);
	return (0);
}

static uint32_t
i915_gem_get_gtt_size(struct drm_device *dev, uint32_t size, int tiling_mode)
{
	uint32_t gtt_size;

	if (INTEL_INFO(dev)->gen >= 4 ||
	    tiling_mode == I915_TILING_NONE)
		return (size);

	/* Previous chips need a power-of-two fence region when tiling */
	if (INTEL_INFO(dev)->gen == 3)
		gtt_size = 1024*1024;
	else
		gtt_size = 512*1024;

	while (gtt_size < size)
		gtt_size <<= 1;

	return (gtt_size);
}

/**
 * i915_gem_get_gtt_alignment - return required GTT alignment for an object
 * @obj: object to check
 *
 * Return the required GTT alignment for an object, taking into account
 * potential fence register mapping.
 */
static uint32_t
i915_gem_get_gtt_alignment(struct drm_device *dev, uint32_t size,
     int tiling_mode)
{

	/*
	 * Minimum alignment is 4k (GTT page size), but might be greater
	 * if a fence register is needed for the object.
	 */
	if (INTEL_INFO(dev)->gen >= 4 ||
	    tiling_mode == I915_TILING_NONE)
		return (4096);

	/*
	 * Previous chips need to be aligned to the size of the smallest
	 * fence register that can contain the object.
	 */
	return (i915_gem_get_gtt_size(dev, size, tiling_mode));
}

uint32_t
i915_gem_get_unfenced_gtt_alignment(struct drm_device *dev, uint32_t size,
    int tiling_mode)
{

	if (tiling_mode == I915_TILING_NONE)
		return (4096);

	/*
	 * Minimum alignment is 4k (GTT page size) for sane hw.
	 */
	if (INTEL_INFO(dev)->gen >= 4 || IS_G33(dev))
		return (4096);

	/*
	 * Previous hardware however needs to be aligned to a power-of-two
	 * tile height. The simplest method for determining this is to reuse
	 * the power-of-tile object size.
         */
	return (i915_gem_get_gtt_size(dev, size, tiling_mode));
}

static int
i915_gem_object_bind_to_gtt(struct drm_i915_gem_object *obj,
    unsigned alignment, bool map_and_fenceable)
{
	struct drm_device *dev;
	struct drm_i915_private *dev_priv;
	struct drm_mm_node *free_space;
	uint32_t size, fence_size, fence_alignment, unfenced_alignment;
	bool mappable, fenceable;
	int ret;

	dev = obj->base.dev;
	dev_priv = dev->dev_private;

	if (obj->madv != I915_MADV_WILLNEED) {
		DRM_ERROR("Attempting to bind a purgeable object\n");
		return (-EINVAL);
	}

	fence_size = i915_gem_get_gtt_size(dev, obj->base.size,
	    obj->tiling_mode);
	fence_alignment = i915_gem_get_gtt_alignment(dev, obj->base.size,
	    obj->tiling_mode);
	unfenced_alignment = i915_gem_get_unfenced_gtt_alignment(dev,
	    obj->base.size, obj->tiling_mode);
	if (alignment == 0)
		alignment = map_and_fenceable ? fence_alignment :
		    unfenced_alignment;
	if (map_and_fenceable && (alignment & (fence_alignment - 1)) != 0) {
		DRM_ERROR("Invalid object alignment requested %u\n", alignment);
		return (-EINVAL);
	}

	size = map_and_fenceable ? fence_size : obj->base.size;

	/* If the object is bigger than the entire aperture, reject it early
	 * before evicting everything in a vain attempt to find space.
	 */
	if (obj->base.size > (map_and_fenceable ?
	    dev_priv->mm.gtt_mappable_end : dev_priv->mm.gtt_total)) {
		DRM_ERROR(
"Attempting to bind an object larger than the aperture\n");
		return (-E2BIG);
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
			    free_space, size, alignment, 0,
			    dev_priv->mm.gtt_mappable_end, 1);
		else
			obj->gtt_space = drm_mm_get_block_generic(free_space,
			    size, alignment, 1);
	}
	if (obj->gtt_space == NULL) {
		ret = i915_gem_evict_something(dev, size, alignment,
		    map_and_fenceable);
		if (ret != 0)
			return (ret);
		goto search_free;
	}
	ret = i915_gem_object_get_pages_gtt(obj, 0);
	if (ret != 0) {
		drm_mm_put_block(obj->gtt_space);
		obj->gtt_space = NULL;
		/*
		 * i915_gem_object_get_pages_gtt() cannot return
		 * ENOMEM, since we use vm_page_grab().
		 */
		return (ret);
	}

	ret = i915_gem_gtt_bind_object(obj);
	if (ret != 0) {
		i915_gem_object_put_pages_gtt(obj);
		drm_mm_put_block(obj->gtt_space);
		obj->gtt_space = NULL;
		if (i915_gem_evict_everything(dev, false))
			return (ret);
		goto search_free;
	}

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
	return (0);
}

static void
i915_gem_object_finish_gtt(struct drm_i915_gem_object *obj)
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

int
i915_gem_object_unbind(struct drm_i915_gem_object *obj)
{
	drm_i915_private_t *dev_priv;
	int ret;

	dev_priv = obj->base.dev->dev_private;
	ret = 0;
	if (obj->gtt_space == NULL)
		return (0);
	if (obj->pin_count != 0) {
		DRM_ERROR("Attempting to unbind pinned buffer\n");
		return (-EINVAL);
	}

	ret = i915_gem_object_finish_gpu(obj);
	if (ret == -ERESTART || ret == -EINTR)
		return (ret);

	i915_gem_object_finish_gtt(obj);

	if (ret == 0)
		ret = i915_gem_object_set_to_cpu_domain(obj, 1);
	if (ret == -ERESTART || ret == -EINTR)
		return (ret);
	if (ret != 0) {
		i915_gem_clflush_object(obj);
		obj->base.read_domains = obj->base.write_domain =
		    I915_GEM_DOMAIN_CPU;
	}

	ret = i915_gem_object_put_fence(obj);
	if (ret == -ERESTART)
		return (ret);

	i915_gem_gtt_unbind_object(obj);
	if (obj->has_aliasing_ppgtt_mapping) {
		i915_ppgtt_unbind_object(dev_priv->mm.aliasing_ppgtt, obj);
		obj->has_aliasing_ppgtt_mapping = 0;
	}
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

	return (ret);
}

static int
i915_gem_object_get_pages_gtt(struct drm_i915_gem_object *obj,
    int flags)
{
	struct drm_device *dev;
	vm_object_t vm_obj;
	vm_page_t m;
	int page_count, i, j;

	dev = obj->base.dev;
	KASSERT(obj->pages == NULL, ("Obj already has pages"));
	page_count = obj->base.size / PAGE_SIZE;
	obj->pages = malloc(page_count * sizeof(vm_page_t), DRM_I915_GEM,
	    M_WAITOK);
	vm_obj = obj->base.vm_obj;
	VM_OBJECT_WLOCK(vm_obj);
	for (i = 0; i < page_count; i++) {
		if ((obj->pages[i] = i915_gem_wire_page(vm_obj, i)) == NULL)
			goto failed;
	}
	VM_OBJECT_WUNLOCK(vm_obj);
	if (i915_gem_object_needs_bit17_swizzle(obj))
		i915_gem_object_do_bit_17_swizzle(obj);
	return (0);

failed:
	for (j = 0; j < i; j++) {
		m = obj->pages[j];
		vm_page_lock(m);
		vm_page_unwire(m, PQ_INACTIVE);
		vm_page_unlock(m);
		atomic_add_long(&i915_gem_wired_pages_cnt, -1);
	}
	VM_OBJECT_WUNLOCK(vm_obj);
	free(obj->pages, DRM_I915_GEM);
	obj->pages = NULL;
	return (-EIO);
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
	vm_page_t m;
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
		m = obj->pages[i];
		if (obj->dirty)
			vm_page_dirty(m);
		if (obj->madv == I915_MADV_WILLNEED)
			vm_page_reference(m);
		vm_page_lock(m);
		vm_page_unwire(obj->pages[i], PQ_ACTIVE);
		vm_page_unlock(m);
		atomic_add_long(&i915_gem_wired_pages_cnt, -1);
	}
	VM_OBJECT_WUNLOCK(obj->base.vm_obj);
	obj->dirty = 0;
	free(obj->pages, DRM_I915_GEM);
	obj->pages = NULL;
}

void
i915_gem_release_mmap(struct drm_i915_gem_object *obj)
{
	vm_object_t devobj;
	vm_page_t m;
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
			m = vm_page_lookup(devobj, i);
			if (m == NULL)
				continue;
			if (vm_page_sleep_if_busy(m, "915unm"))
				goto retry;
			cdev_pager_free_page(devobj, m);
		}
		VM_OBJECT_WUNLOCK(devobj);
		vm_object_deallocate(devobj);
	}

	obj->fault_mappable = false;
}

int
i915_gem_object_wait_rendering(struct drm_i915_gem_object *obj)
{
	int ret;

	KASSERT((obj->base.write_domain & I915_GEM_GPU_DOMAINS) == 0,
	    ("In GPU write domain"));

	CTR5(KTR_DRM, "object_wait_rendering %p %s %x %d %d", obj,
	    obj->ring != NULL ? obj->ring->name : "none", obj->gtt_offset,
	    obj->active, obj->last_rendering_seqno);
	if (obj->active) {
		ret = i915_wait_request(obj->ring, obj->last_rendering_seqno,
		    true);
		if (ret != 0)
			return (ret);
	}
	return (0);
}

void
i915_gem_object_move_to_active(struct drm_i915_gem_object *obj,
    struct intel_ring_buffer *ring, uint32_t seqno)
{
	struct drm_device *dev = obj->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_fence_reg *reg;

	obj->ring = ring;
	KASSERT(ring != NULL, ("NULL ring"));

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
		obj->last_fenced_ring = ring;

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

	if (obj->pin_count != 0)
		list_move_tail(&obj->mm_list, &dev_priv->mm.pinned_list);
	else
		list_move_tail(&obj->mm_list, &dev_priv->mm.inactive_list);

	KASSERT(list_empty(&obj->gpu_write_list), ("On gpu_write_list"));
	KASSERT(obj->active, ("Object not active"));
	obj->ring = NULL;
	obj->last_fenced_ring = NULL;

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

static void
i915_gem_object_truncate(struct drm_i915_gem_object *obj)
{
	vm_object_t vm_obj;

	vm_obj = obj->base.vm_obj;
	VM_OBJECT_WLOCK(vm_obj);
	vm_object_page_remove(vm_obj, 0, 0, false);
	VM_OBJECT_WUNLOCK(vm_obj);
	obj->madv = I915_MADV_PURGED_INTERNAL;
}

static inline int
i915_gem_object_is_purgeable(struct drm_i915_gem_object *obj)
{

	return (obj->madv == I915_MADV_DONTNEED);
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

static int
i915_gem_object_needs_bit17_swizzle(struct drm_i915_gem_object *obj)
{
	drm_i915_private_t *dev_priv;

	dev_priv = obj->base.dev->dev_private;
	return (dev_priv->mm.bit_6_swizzle_x == I915_BIT_6_SWIZZLE_9_10_17 &&
	    obj->tiling_mode != I915_TILING_NONE);
}

static vm_page_t
i915_gem_wire_page(vm_object_t object, vm_pindex_t pindex)
{
	vm_page_t m;
	int rv;

	VM_OBJECT_ASSERT_WLOCKED(object);
	m = vm_page_grab(object, pindex, VM_ALLOC_NORMAL);
	if (m->valid != VM_PAGE_BITS_ALL) {
		if (vm_pager_has_page(object, pindex, NULL, NULL)) {
			rv = vm_pager_get_pages(object, &m, 1, 0);
			m = vm_page_lookup(object, pindex);
			if (m == NULL)
				return (NULL);
			if (rv != VM_PAGER_OK) {
				vm_page_lock(m);
				vm_page_free(m);
				vm_page_unlock(m);
				return (NULL);
			}
		} else {
			pmap_zero_page(m);
			m->valid = VM_PAGE_BITS_ALL;
			m->dirty = 0;
		}
	}
	vm_page_lock(m);
	vm_page_wire(m);
	vm_page_unlock(m);
	vm_page_xunbusy(m);
	atomic_add_long(&i915_gem_wired_pages_cnt, 1);
	return (m);
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

static int
i915_ring_idle(struct intel_ring_buffer *ring, bool do_retire)
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

	return (i915_wait_request(ring, i915_gem_next_request_seqno(ring),
	    do_retire));
}

int
i915_gpu_idle(struct drm_device *dev, bool do_retire)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret, i;

	/* Flush everything onto the inactive list. */
	for (i = 0; i < I915_NUM_RINGS; i++) {
		ret = i915_ring_idle(&dev_priv->rings[i], do_retire);
		if (ret)
			return ret;
	}

	return 0;
}

int
i915_wait_request(struct intel_ring_buffer *ring, uint32_t seqno, bool do_retire)
{
	drm_i915_private_t *dev_priv;
	struct drm_i915_gem_request *request;
	uint32_t ier;
	int flags, ret;
	bool recovery_complete;

	KASSERT(seqno != 0, ("Zero seqno"));

	dev_priv = ring->dev->dev_private;
	ret = 0;

	if (atomic_load_acq_int(&dev_priv->mm.wedged) != 0) {
		/* Give the error handler a chance to run. */
		mtx_lock(&dev_priv->error_completion_lock);
		recovery_complete = (&dev_priv->error_completion) > 0;
		mtx_unlock(&dev_priv->error_completion_lock);
		return (recovery_complete ? -EIO : -EAGAIN);
	}

	if (seqno == ring->outstanding_lazy_request) {
		request = malloc(sizeof(*request), DRM_I915_GEM,
		    M_WAITOK | M_ZERO);
		if (request == NULL)
			return (-ENOMEM);

		ret = i915_add_request(ring, NULL, request);
		if (ret != 0) {
			free(request, DRM_I915_GEM);
			return (ret);
		}

		seqno = request->seqno;
	}

	if (!i915_seqno_passed(ring->get_seqno(ring), seqno)) {
		if (HAS_PCH_SPLIT(ring->dev))
			ier = I915_READ(DEIER) | I915_READ(GTIER);
		else
			ier = I915_READ(IER);
		if (!ier) {
			DRM_ERROR("something (likely vbetool) disabled "
				  "interrupts, re-enabling\n");
			ring->dev->driver->irq_preinstall(ring->dev);
			ring->dev->driver->irq_postinstall(ring->dev);
		}

		CTR2(KTR_DRM, "request_wait_begin %s %d", ring->name, seqno);

		ring->waiting_seqno = seqno;
		mtx_lock(&ring->irq_lock);
		if (ring->irq_get(ring)) {
			flags = dev_priv->mm.interruptible ? PCATCH : 0;
			while (!i915_seqno_passed(ring->get_seqno(ring), seqno)
			    && !atomic_load_acq_int(&dev_priv->mm.wedged) &&
			    ret == 0) {
				ret = -msleep(ring, &ring->irq_lock, flags,
				    "915gwr", 0);
			}
			ring->irq_put(ring);
			mtx_unlock(&ring->irq_lock);
		} else {
			mtx_unlock(&ring->irq_lock);
			if (_intel_wait_for(ring->dev,
			    i915_seqno_passed(ring->get_seqno(ring), seqno) ||
			    atomic_load_acq_int(&dev_priv->mm.wedged), 3000,
			    0, "i915wrq") != 0)
				ret = -EBUSY;
		}
		ring->waiting_seqno = 0;

		CTR3(KTR_DRM, "request_wait_end %s %d %d", ring->name, seqno,
		    ret);
	}
	if (atomic_load_acq_int(&dev_priv->mm.wedged))
		ret = -EAGAIN;

	/* Directly dispatch request retiring.  While we have the work queue
	 * to handle this, the waiter on a request often wants an associated
	 * buffer to have made it to the inactive list, and we would need
	 * a separate wait queue to handle that.
	 */
	if (ret == 0 && do_retire)
		i915_gem_retire_requests_ring(ring);

	return (ret);
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
i915_add_request(struct intel_ring_buffer *ring, struct drm_file *file,
     struct drm_i915_gem_request *request)
{
	drm_i915_private_t *dev_priv;
	struct drm_i915_file_private *file_priv;
	uint32_t seqno;
	u32 request_ring_position;
	int was_empty;
	int ret;

	KASSERT(request != NULL, ("NULL request in add"));
	DRM_LOCK_ASSERT(ring->dev);
	dev_priv = ring->dev->dev_private;

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

	if (file != NULL) {
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
	return (0);
}

static inline void
i915_gem_request_remove_from_client(struct drm_i915_gem_request *request)
{
	struct drm_i915_file_private *file_priv = request->file_priv;

	if (!file_priv)
		return;

	DRM_LOCK_ASSERT(request->ring->dev);

	mtx_lock(&file_priv->mm.lck);
	if (request->file_priv != NULL) {
		list_del(&request->client_list);
		request->file_priv = NULL;
	}
	mtx_unlock(&file_priv->mm.lck);
}

void
i915_gem_release(struct drm_device *dev, struct drm_file *file)
{
	struct drm_i915_file_private *file_priv;
	struct drm_i915_gem_request *request;

	file_priv = file->driver_priv;

	/* Clean up our request list when the client is going away, so that
	 * later retire_requests won't dereference our soon-to-be-gone
	 * file_priv.
	 */
	mtx_lock(&file_priv->mm.lck);
	while (!list_empty(&file_priv->mm.request_list)) {
		request = list_first_entry(&file_priv->mm.request_list,
					   struct drm_i915_gem_request,
					   client_list);
		list_del(&request->client_list);
		request->file_priv = NULL;
	}
	mtx_unlock(&file_priv->mm.lck);
}

static void
i915_gem_reset_ring_lists(struct drm_i915_private *dev_priv,
    struct intel_ring_buffer *ring)
{

	if (ring->dev != NULL)
		DRM_LOCK_ASSERT(ring->dev);

	while (!list_empty(&ring->request_list)) {
		struct drm_i915_gem_request *request;

		request = list_first_entry(&ring->request_list,
		    struct drm_i915_gem_request, list);

		list_del(&request->list);
		i915_gem_request_remove_from_client(request);
		free(request, DRM_I915_GEM);
	}

	while (!list_empty(&ring->active_list)) {
		struct drm_i915_gem_object *obj;

		obj = list_first_entry(&ring->active_list,
		    struct drm_i915_gem_object, ring_list);

		obj->base.write_domain = 0;
		list_del_init(&obj->gpu_write_list);
		i915_gem_object_move_to_inactive(obj);
	}
}

static void
i915_gem_reset_fences(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int i;

	for (i = 0; i < dev_priv->num_fence_regs; i++) {
		struct drm_i915_fence_reg *reg = &dev_priv->fence_regs[i];
		struct drm_i915_gem_object *obj = reg->obj;

		if (!obj)
			continue;

		if (obj->tiling_mode)
			i915_gem_release_mmap(obj);

		reg->obj->fence_reg = I915_FENCE_REG_NONE;
		reg->obj->fenced_gpu_access = false;
		reg->obj->last_fenced_seqno = 0;
		reg->obj->last_fenced_ring = NULL;
		i915_gem_clear_fence_reg(dev, reg);
	}
}

void
i915_gem_reset(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj;
	int i;

	for (i = 0; i < I915_NUM_RINGS; i++)
		i915_gem_reset_ring_lists(dev_priv, &dev_priv->rings[i]);

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

	for (i = 0; i < DRM_ARRAY_SIZE(ring->sync_seqno); i++)
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
		mtx_lock(&ring->irq_lock);
		ring->irq_put(ring);
		mtx_unlock(&ring->irq_lock);
		ring->trace_irq_seqno = 0;
	}
}

void
i915_gem_retire_requests(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj, *next;
	int i;

	if (!list_empty(&dev_priv->mm.deferred_free_list)) {
		list_for_each_entry_safe(obj, next,
		    &dev_priv->mm.deferred_free_list, mm_list)
			i915_gem_free_object_tail(obj);
	}

	for (i = 0; i < I915_NUM_RINGS; i++)
		i915_gem_retire_requests_ring(&dev_priv->rings[i]);
}

static int
sandybridge_write_fence_reg(struct drm_i915_gem_object *obj,
    struct intel_ring_buffer *pipelined)
{
	struct drm_device *dev = obj->base.dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	u32 size = obj->gtt_space->size;
	int regnum = obj->fence_reg;
	uint64_t val;

	val = (uint64_t)((obj->gtt_offset + size - 4096) &
			 0xfffff000) << 32;
	val |= obj->gtt_offset & 0xfffff000;
	val |= (uint64_t)((obj->stride / 128) - 1) <<
		SANDYBRIDGE_FENCE_PITCH_SHIFT;

	if (obj->tiling_mode == I915_TILING_Y)
		val |= 1 << I965_FENCE_TILING_Y_SHIFT;
	val |= I965_FENCE_REG_VALID;

	if (pipelined) {
		int ret = intel_ring_begin(pipelined, 6);
		if (ret)
			return ret;

		intel_ring_emit(pipelined, MI_NOOP);
		intel_ring_emit(pipelined, MI_LOAD_REGISTER_IMM(2));
		intel_ring_emit(pipelined, FENCE_REG_SANDYBRIDGE_0 + regnum*8);
		intel_ring_emit(pipelined, (u32)val);
		intel_ring_emit(pipelined, FENCE_REG_SANDYBRIDGE_0 + regnum*8 + 4);
		intel_ring_emit(pipelined, (u32)(val >> 32));
		intel_ring_advance(pipelined);
	} else
		I915_WRITE64(FENCE_REG_SANDYBRIDGE_0 + regnum * 8, val);

	return 0;
}

static int
i965_write_fence_reg(struct drm_i915_gem_object *obj,
    struct intel_ring_buffer *pipelined)
{
	struct drm_device *dev = obj->base.dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	u32 size = obj->gtt_space->size;
	int regnum = obj->fence_reg;
	uint64_t val;

	val = (uint64_t)((obj->gtt_offset + size - 4096) &
		    0xfffff000) << 32;
	val |= obj->gtt_offset & 0xfffff000;
	val |= ((obj->stride / 128) - 1) << I965_FENCE_PITCH_SHIFT;
	if (obj->tiling_mode == I915_TILING_Y)
		val |= 1 << I965_FENCE_TILING_Y_SHIFT;
	val |= I965_FENCE_REG_VALID;

	if (pipelined) {
		int ret = intel_ring_begin(pipelined, 6);
		if (ret)
			return ret;

		intel_ring_emit(pipelined, MI_NOOP);
		intel_ring_emit(pipelined, MI_LOAD_REGISTER_IMM(2));
		intel_ring_emit(pipelined, FENCE_REG_965_0 + regnum*8);
		intel_ring_emit(pipelined, (u32)val);
		intel_ring_emit(pipelined, FENCE_REG_965_0 + regnum*8 + 4);
		intel_ring_emit(pipelined, (u32)(val >> 32));
		intel_ring_advance(pipelined);
	} else
		I915_WRITE64(FENCE_REG_965_0 + regnum * 8, val);

	return 0;
}

static int
i915_write_fence_reg(struct drm_i915_gem_object *obj,
    struct intel_ring_buffer *pipelined)
{
	struct drm_device *dev = obj->base.dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	u32 size = obj->gtt_space->size;
	u32 fence_reg, val, pitch_val;
	int tile_width;

	if ((obj->gtt_offset & ~I915_FENCE_START_MASK) ||
	    (size & -size) != size || (obj->gtt_offset & (size - 1))) {
		printf(
"object 0x%08x [fenceable? %d] not 1M or pot-size (0x%08x) aligned\n",
		 obj->gtt_offset, obj->map_and_fenceable, size);
		return -EINVAL;
	}

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

	fence_reg = obj->fence_reg;
	if (fence_reg < 8)
		fence_reg = FENCE_REG_830_0 + fence_reg * 4;
	else
		fence_reg = FENCE_REG_945_8 + (fence_reg - 8) * 4;

	if (pipelined) {
		int ret = intel_ring_begin(pipelined, 4);
		if (ret)
			return ret;

		intel_ring_emit(pipelined, MI_NOOP);
		intel_ring_emit(pipelined, MI_LOAD_REGISTER_IMM(1));
		intel_ring_emit(pipelined, fence_reg);
		intel_ring_emit(pipelined, val);
		intel_ring_advance(pipelined);
	} else
		I915_WRITE(fence_reg, val);

	return 0;
}

static int
i830_write_fence_reg(struct drm_i915_gem_object *obj,
    struct intel_ring_buffer *pipelined)
{
	struct drm_device *dev = obj->base.dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	u32 size = obj->gtt_space->size;
	int regnum = obj->fence_reg;
	uint32_t val;
	uint32_t pitch_val;

	if ((obj->gtt_offset & ~I830_FENCE_START_MASK) ||
	    (size & -size) != size || (obj->gtt_offset & (size - 1))) {
		printf(
"object 0x%08x not 512K or pot-size 0x%08x aligned\n",
		    obj->gtt_offset, size);
		return -EINVAL;
	}

	pitch_val = obj->stride / 128;
	pitch_val = ffs(pitch_val) - 1;

	val = obj->gtt_offset;
	if (obj->tiling_mode == I915_TILING_Y)
		val |= 1 << I830_FENCE_TILING_Y_SHIFT;
	val |= I830_FENCE_SIZE_BITS(size);
	val |= pitch_val << I830_FENCE_PITCH_SHIFT;
	val |= I830_FENCE_REG_VALID;

	if (pipelined) {
		int ret = intel_ring_begin(pipelined, 4);
		if (ret)
			return ret;

		intel_ring_emit(pipelined, MI_NOOP);
		intel_ring_emit(pipelined, MI_LOAD_REGISTER_IMM(1));
		intel_ring_emit(pipelined, FENCE_REG_830_0 + regnum*4);
		intel_ring_emit(pipelined, val);
		intel_ring_advance(pipelined);
	} else
		I915_WRITE(FENCE_REG_830_0 + regnum * 4, val);

	return 0;
}

static bool ring_passed_seqno(struct intel_ring_buffer *ring, u32 seqno)
{
	return i915_seqno_passed(ring->get_seqno(ring), seqno);
}

static int
i915_gem_object_flush_fence(struct drm_i915_gem_object *obj,
    struct intel_ring_buffer *pipelined)
{
	int ret;

	if (obj->fenced_gpu_access) {
		if (obj->base.write_domain & I915_GEM_GPU_DOMAINS) {
			ret = i915_gem_flush_ring(obj->last_fenced_ring, 0,
			    obj->base.write_domain);
			if (ret)
				return ret;
		}

		obj->fenced_gpu_access = false;
	}

	if (obj->last_fenced_seqno && pipelined != obj->last_fenced_ring) {
		if (!ring_passed_seqno(obj->last_fenced_ring,
				       obj->last_fenced_seqno)) {
			ret = i915_wait_request(obj->last_fenced_ring,
						obj->last_fenced_seqno,
						true);
			if (ret)
				return ret;
		}

		obj->last_fenced_seqno = 0;
		obj->last_fenced_ring = NULL;
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
	int ret;

	if (obj->tiling_mode)
		i915_gem_release_mmap(obj);

	ret = i915_gem_object_flush_fence(obj, NULL);
	if (ret)
		return ret;

	if (obj->fence_reg != I915_FENCE_REG_NONE) {
		struct drm_i915_private *dev_priv = obj->base.dev->dev_private;

		if (dev_priv->fence_regs[obj->fence_reg].pin_count != 0)
			printf("%s: pin_count %d\n", __func__,
			    dev_priv->fence_regs[obj->fence_reg].pin_count);
		i915_gem_clear_fence_reg(obj->base.dev,
					 &dev_priv->fence_regs[obj->fence_reg]);

		obj->fence_reg = I915_FENCE_REG_NONE;
	}

	return 0;
}

static struct drm_i915_fence_reg *
i915_find_fence_reg(struct drm_device *dev, struct intel_ring_buffer *pipelined)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_fence_reg *reg, *first, *avail;
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
	avail = first = NULL;
	list_for_each_entry(reg, &dev_priv->mm.fence_list, lru_list) {
		if (reg->pin_count)
			continue;

		if (first == NULL)
			first = reg;

		if (!pipelined ||
		    !reg->obj->last_fenced_ring ||
		    reg->obj->last_fenced_ring == pipelined) {
			avail = reg;
			break;
		}
	}

	if (avail == NULL)
		avail = first;

	return avail;
}

int
i915_gem_object_get_fence(struct drm_i915_gem_object *obj,
    struct intel_ring_buffer *pipelined)
{
	struct drm_device *dev = obj->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_fence_reg *reg;
	int ret;

	pipelined = NULL;
	ret = 0;

	if (obj->fence_reg != I915_FENCE_REG_NONE) {
		reg = &dev_priv->fence_regs[obj->fence_reg];
		list_move_tail(&reg->lru_list, &dev_priv->mm.fence_list);

		if (obj->tiling_changed) {
			ret = i915_gem_object_flush_fence(obj, pipelined);
			if (ret)
				return ret;

			if (!obj->fenced_gpu_access && !obj->last_fenced_seqno)
				pipelined = NULL;

			if (pipelined) {
				reg->setup_seqno =
					i915_gem_next_request_seqno(pipelined);
				obj->last_fenced_seqno = reg->setup_seqno;
				obj->last_fenced_ring = pipelined;
			}

			goto update;
		}

		if (!pipelined) {
			if (reg->setup_seqno) {
				if (!ring_passed_seqno(obj->last_fenced_ring,
				    reg->setup_seqno)) {
					ret = i915_wait_request(
					    obj->last_fenced_ring,
					    reg->setup_seqno,
					    true);
					if (ret)
						return ret;
				}

				reg->setup_seqno = 0;
			}
		} else if (obj->last_fenced_ring &&
			   obj->last_fenced_ring != pipelined) {
			ret = i915_gem_object_flush_fence(obj, pipelined);
			if (ret)
				return ret;
		}

		if (!obj->fenced_gpu_access && !obj->last_fenced_seqno)
			pipelined = NULL;
		KASSERT(pipelined || reg->setup_seqno == 0, ("!pipelined"));

		if (obj->tiling_changed) {
			if (pipelined) {
				reg->setup_seqno =
					i915_gem_next_request_seqno(pipelined);
				obj->last_fenced_seqno = reg->setup_seqno;
				obj->last_fenced_ring = pipelined;
			}
			goto update;
		}

		return 0;
	}

	reg = i915_find_fence_reg(dev, pipelined);
	if (reg == NULL)
		return -EDEADLK;

	ret = i915_gem_object_flush_fence(obj, pipelined);
	if (ret)
		return ret;

	if (reg->obj) {
		struct drm_i915_gem_object *old = reg->obj;

		drm_gem_object_reference(&old->base);

		if (old->tiling_mode)
			i915_gem_release_mmap(old);

		ret = i915_gem_object_flush_fence(old, pipelined);
		if (ret) {
			drm_gem_object_unreference(&old->base);
			return ret;
		}

		if (old->last_fenced_seqno == 0 && obj->last_fenced_seqno == 0)
			pipelined = NULL;

		old->fence_reg = I915_FENCE_REG_NONE;
		old->last_fenced_ring = pipelined;
		old->last_fenced_seqno =
			pipelined ? i915_gem_next_request_seqno(pipelined) : 0;

		drm_gem_object_unreference(&old->base);
	} else if (obj->last_fenced_seqno == 0)
		pipelined = NULL;

	reg->obj = obj;
	list_move_tail(&reg->lru_list, &dev_priv->mm.fence_list);
	obj->fence_reg = reg - dev_priv->fence_regs;
	obj->last_fenced_ring = pipelined;

	reg->setup_seqno =
		pipelined ? i915_gem_next_request_seqno(pipelined) : 0;
	obj->last_fenced_seqno = reg->setup_seqno;

update:
	obj->tiling_changed = false;
	switch (INTEL_INFO(dev)->gen) {
	case 7:
	case 6:
		ret = sandybridge_write_fence_reg(obj, pipelined);
		break;
	case 5:
	case 4:
		ret = i965_write_fence_reg(obj, pipelined);
		break;
	case 3:
		ret = i915_write_fence_reg(obj, pipelined);
		break;
	case 2:
		ret = i830_write_fence_reg(obj, pipelined);
		break;
	}

	return ret;
}

static void
i915_gem_clear_fence_reg(struct drm_device *dev, struct drm_i915_fence_reg *reg)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	uint32_t fence_reg = reg - dev_priv->fence_regs;

	switch (INTEL_INFO(dev)->gen) {
	case 7:
	case 6:
		I915_WRITE64(FENCE_REG_SANDYBRIDGE_0 + fence_reg*8, 0);
		break;
	case 5:
	case 4:
		I915_WRITE64(FENCE_REG_965_0 + fence_reg*8, 0);
		break;
	case 3:
		if (fence_reg >= 8)
			fence_reg = FENCE_REG_945_8 + (fence_reg - 8) * 4;
		else
	case 2:
			fence_reg = FENCE_REG_830_0 + fence_reg * 4;

		I915_WRITE(fence_reg, 0);
		break;
	}

	list_del_init(&reg->lru_list);
	reg->obj = NULL;
	reg->setup_seqno = 0;
	reg->pin_count = 0;
}

int
i915_gem_init_object(struct drm_gem_object *obj)
{

	printf("i915_gem_init_object called\n");
	return (0);
}

static bool
i915_gem_object_is_inactive(struct drm_i915_gem_object *obj)
{

	return (obj->gtt_space && !obj->active && obj->pin_count == 0);
}

static void
i915_gem_retire_task_handler(void *arg, int pending)
{
	drm_i915_private_t *dev_priv;
	struct drm_device *dev;
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
	for (i = 0; i < I915_NUM_RINGS; i++) {
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

void
i915_gem_lastclose(struct drm_device *dev)
{
	int ret;

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		return;

	ret = i915_gem_idle(dev);
	if (ret != 0)
		DRM_ERROR("failed to idle hardware: %d\n", ret);
}

static int
i915_gem_init_phys_object(struct drm_device *dev, int id, int size, int align)
{
	drm_i915_private_t *dev_priv;
	struct drm_i915_gem_phys_object *phys_obj;
	int ret;

	dev_priv = dev->dev_private;
	if (dev_priv->mm.phys_objs[id - 1] != NULL || size == 0)
		return (0);

	phys_obj = malloc(sizeof(struct drm_i915_gem_phys_object), DRM_I915_GEM,
	    M_WAITOK | M_ZERO);

	phys_obj->id = id;

	phys_obj->handle = drm_pci_alloc(dev, size, align, ~0);
	if (phys_obj->handle == NULL) {
		ret = -ENOMEM;
		goto free_obj;
	}
	pmap_change_attr((vm_offset_t)phys_obj->handle->vaddr,
	    size / PAGE_SIZE, PAT_WRITE_COMBINING);

	dev_priv->mm.phys_objs[id - 1] = phys_obj;

	return (0);

free_obj:
	free(phys_obj, DRM_I915_GEM);
	return (ret);
}

static void
i915_gem_free_phys_object(struct drm_device *dev, int id)
{
	drm_i915_private_t *dev_priv;
	struct drm_i915_gem_phys_object *phys_obj;

	dev_priv = dev->dev_private;
	if (dev_priv->mm.phys_objs[id - 1] == NULL)
		return;

	phys_obj = dev_priv->mm.phys_objs[id - 1];
	if (phys_obj->cur_obj != NULL)
		i915_gem_detach_phys_object(dev, phys_obj->cur_obj);

	drm_pci_free(dev, phys_obj->handle);
	free(phys_obj, DRM_I915_GEM);
	dev_priv->mm.phys_objs[id - 1] = NULL;
}

void
i915_gem_free_all_phys_object(struct drm_device *dev)
{
	int i;

	for (i = I915_GEM_PHYS_CURSOR_0; i <= I915_MAX_PHYS_OBJECT; i++)
		i915_gem_free_phys_object(dev, i);
}

void
i915_gem_detach_phys_object(struct drm_device *dev,
    struct drm_i915_gem_object *obj)
{
	vm_page_t m;
	struct sf_buf *sf;
	char *vaddr, *dst;
	int i, page_count;

	if (obj->phys_obj == NULL)
		return;
	vaddr = obj->phys_obj->handle->vaddr;

	page_count = obj->base.size / PAGE_SIZE;
	VM_OBJECT_WLOCK(obj->base.vm_obj);
	for (i = 0; i < page_count; i++) {
		m = i915_gem_wire_page(obj->base.vm_obj, i);
		if (m == NULL)
			continue; /* XXX */

		VM_OBJECT_WUNLOCK(obj->base.vm_obj);
		sf = sf_buf_alloc(m, 0);
		if (sf != NULL) {
			dst = (char *)sf_buf_kva(sf);
			memcpy(dst, vaddr + IDX_TO_OFF(i), PAGE_SIZE);
			sf_buf_free(sf);
		}
		drm_clflush_pages(&m, 1);

		VM_OBJECT_WLOCK(obj->base.vm_obj);
		vm_page_reference(m);
		vm_page_lock(m);
		vm_page_dirty(m);
		vm_page_unwire(m, PQ_INACTIVE);
		vm_page_unlock(m);
		atomic_add_long(&i915_gem_wired_pages_cnt, -1);
	}
	VM_OBJECT_WUNLOCK(obj->base.vm_obj);
	intel_gtt_chipset_flush();

	obj->phys_obj->cur_obj = NULL;
	obj->phys_obj = NULL;
}

int
i915_gem_attach_phys_object(struct drm_device *dev,
    struct drm_i915_gem_object *obj, int id, int align)
{
	drm_i915_private_t *dev_priv;
	vm_page_t m;
	struct sf_buf *sf;
	char *dst, *src;
	int i, page_count, ret;

	if (id > I915_MAX_PHYS_OBJECT)
		return (-EINVAL);

	if (obj->phys_obj != NULL) {
		if (obj->phys_obj->id == id)
			return (0);
		i915_gem_detach_phys_object(dev, obj);
	}

	dev_priv = dev->dev_private;
	if (dev_priv->mm.phys_objs[id - 1] == NULL) {
		ret = i915_gem_init_phys_object(dev, id, obj->base.size, align);
		if (ret != 0) {
			DRM_ERROR("failed to init phys object %d size: %zu\n",
				  id, obj->base.size);
			return (ret);
		}
	}

	/* bind to the object */
	obj->phys_obj = dev_priv->mm.phys_objs[id - 1];
	obj->phys_obj->cur_obj = obj;

	page_count = obj->base.size / PAGE_SIZE;

	VM_OBJECT_WLOCK(obj->base.vm_obj);
	ret = 0;
	for (i = 0; i < page_count; i++) {
		m = i915_gem_wire_page(obj->base.vm_obj, i);
		if (m == NULL) {
			ret = -EIO;
			break;
		}
		VM_OBJECT_WUNLOCK(obj->base.vm_obj);
		sf = sf_buf_alloc(m, 0);
		src = (char *)sf_buf_kva(sf);
		dst = (char *)obj->phys_obj->handle->vaddr + IDX_TO_OFF(i);
		memcpy(dst, src, PAGE_SIZE);
		sf_buf_free(sf);

		VM_OBJECT_WLOCK(obj->base.vm_obj);

		vm_page_reference(m);
		vm_page_lock(m);
		vm_page_unwire(m, PQ_INACTIVE);
		vm_page_unlock(m);
		atomic_add_long(&i915_gem_wired_pages_cnt, -1);
	}
	VM_OBJECT_WUNLOCK(obj->base.vm_obj);

	return (0);
}

static int
i915_gem_phys_pwrite(struct drm_device *dev, struct drm_i915_gem_object *obj,
    uint64_t data_ptr, uint64_t offset, uint64_t size,
    struct drm_file *file_priv)
{
	char *user_data, *vaddr;
	int ret;

	vaddr = (char *)obj->phys_obj->handle->vaddr + offset;
	user_data = (char *)(uintptr_t)data_ptr;

	if (copyin_nofault(user_data, vaddr, size) != 0) {
		/* The physical object once assigned is fixed for the lifetime
		 * of the obj, so we can safely drop the lock and continue
		 * to access vaddr.
		 */
		DRM_UNLOCK(dev);
		ret = -copyin(user_data, vaddr, size);
		DRM_LOCK(dev);
		if (ret != 0)
			return (ret);
	}

	intel_gtt_chipset_flush();
	return (0);
}

static int
i915_gpu_is_active(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv;

	dev_priv = dev->dev_private;
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
		if (i915_gpu_idle(dev, true) == 0)
			goto rescan;
	}
	DRM_UNLOCK(dev);
}

void
i915_gem_unload(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv;

	dev_priv = dev->dev_private;
	EVENTHANDLER_DEREGISTER(vm_lowmem, dev_priv->mm.i915_lowmem);
}
