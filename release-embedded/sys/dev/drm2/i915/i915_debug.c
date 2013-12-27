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
 *    Keith Packard <keithp@keithp.com>
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <dev/drm2/drmP.h>
#include <dev/drm2/drm.h>
#include <dev/drm2/i915/i915_drm.h>
#include <dev/drm2/i915/i915_drv.h>
#include <dev/drm2/i915/intel_drv.h>
#include <dev/drm2/i915/intel_ringbuffer.h>

#include <sys/sysctl.h>

enum {
	ACTIVE_LIST,
	FLUSHING_LIST,
	INACTIVE_LIST,
	PINNED_LIST,
	DEFERRED_FREE_LIST,
};

static const char *
yesno(int v)
{
	return (v ? "yes" : "no");
}

static int
i915_capabilities(struct drm_device *dev, struct sbuf *m, void *data)
{
	const struct intel_device_info *info = INTEL_INFO(dev);

	sbuf_printf(m, "gen: %d\n", info->gen);
	if (HAS_PCH_SPLIT(dev))
		sbuf_printf(m, "pch: %d\n", INTEL_PCH_TYPE(dev));
#define B(x) sbuf_printf(m, #x ": %s\n", yesno(info->x))
	B(is_mobile);
	B(is_i85x);
	B(is_i915g);
	B(is_i945gm);
	B(is_g33);
	B(need_gfx_hws);
	B(is_g4x);
	B(is_pineview);
	B(has_fbc);
	B(has_pipe_cxsr);
	B(has_hotplug);
	B(cursor_needs_physical);
	B(has_overlay);
	B(overlay_needs_physical);
	B(supports_tv);
	B(has_bsd_ring);
	B(has_blt_ring);
	B(has_llc);
#undef B

	return (0);
}

static const char *
get_pin_flag(struct drm_i915_gem_object *obj)
{
	if (obj->user_pin_count > 0)
		return "P";
	else if (obj->pin_count > 0)
		return "p";
	else
		return " ";
}

static const char *
get_tiling_flag(struct drm_i915_gem_object *obj)
{
	switch (obj->tiling_mode) {
	default:
	case I915_TILING_NONE: return (" ");
	case I915_TILING_X: return ("X");
	case I915_TILING_Y: return ("Y");
	}
}

static const char *
cache_level_str(int type)
{
	switch (type) {
	case I915_CACHE_NONE: return " uncached";
	case I915_CACHE_LLC: return " snooped (LLC)";
	case I915_CACHE_LLC_MLC: return " snooped (LLC+MLC)";
	default: return ("");
	}
}

static void
describe_obj(struct sbuf *m, struct drm_i915_gem_object *obj)
{

	sbuf_printf(m, "%p: %s%s %8zdKiB %04x %04x %d %d%s%s%s",
		   &obj->base,
		   get_pin_flag(obj),
		   get_tiling_flag(obj),
		   obj->base.size / 1024,
		   obj->base.read_domains,
		   obj->base.write_domain,
		   obj->last_rendering_seqno,
		   obj->last_fenced_seqno,
		   cache_level_str(obj->cache_level),
		   obj->dirty ? " dirty" : "",
		   obj->madv == I915_MADV_DONTNEED ? " purgeable" : "");
	if (obj->base.name)
		sbuf_printf(m, " (name: %d)", obj->base.name);
	if (obj->fence_reg != I915_FENCE_REG_NONE)
		sbuf_printf(m, " (fence: %d)", obj->fence_reg);
	if (obj->gtt_space != NULL)
		sbuf_printf(m, " (gtt offset: %08x, size: %08x)",
			   obj->gtt_offset, (unsigned int)obj->gtt_space->size);
	if (obj->pin_mappable || obj->fault_mappable) {
		char s[3], *t = s;
		if (obj->pin_mappable)
			*t++ = 'p';
		if (obj->fault_mappable)
			*t++ = 'f';
		*t = '\0';
		sbuf_printf(m, " (%s mappable)", s);
	}
	if (obj->ring != NULL)
		sbuf_printf(m, " (%s)", obj->ring->name);
}

static int
i915_gem_object_list_info(struct drm_device *dev, struct sbuf *m, void *data)
{
	uintptr_t list = (uintptr_t)data;
	struct list_head *head;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj;
	size_t total_obj_size, total_gtt_size;
	int count;

	if (sx_xlock_sig(&dev->dev_struct_lock))
		return (EINTR);

	switch (list) {
	case ACTIVE_LIST:
		sbuf_printf(m, "Active:\n");
		head = &dev_priv->mm.active_list;
		break;
	case INACTIVE_LIST:
		sbuf_printf(m, "Inactive:\n");
		head = &dev_priv->mm.inactive_list;
		break;
	case PINNED_LIST:
		sbuf_printf(m, "Pinned:\n");
		head = &dev_priv->mm.pinned_list;
		break;
	case FLUSHING_LIST:
		sbuf_printf(m, "Flushing:\n");
		head = &dev_priv->mm.flushing_list;
		break;
	case DEFERRED_FREE_LIST:
		sbuf_printf(m, "Deferred free:\n");
		head = &dev_priv->mm.deferred_free_list;
		break;
	default:
		DRM_UNLOCK(dev);
		return (EINVAL);
	}

	total_obj_size = total_gtt_size = count = 0;
	list_for_each_entry(obj, head, mm_list) {
		sbuf_printf(m, "   ");
		describe_obj(m, obj);
		sbuf_printf(m, "\n");
		total_obj_size += obj->base.size;
		total_gtt_size += obj->gtt_space->size;
		count++;
	}
	DRM_UNLOCK(dev);

	sbuf_printf(m, "Total %d objects, %zu bytes, %zu GTT size\n",
		   count, total_obj_size, total_gtt_size);
	return (0);
}

#define count_objects(list, member) do { \
	list_for_each_entry(obj, list, member) { \
		size += obj->gtt_space->size; \
		++count; \
		if (obj->map_and_fenceable) { \
			mappable_size += obj->gtt_space->size; \
			++mappable_count; \
		} \
	} \
} while (0)

static int
i915_gem_object_info(struct drm_device *dev, struct sbuf *m, void *data)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 count, mappable_count;
	size_t size, mappable_size;
	struct drm_i915_gem_object *obj;

	if (sx_xlock_sig(&dev->dev_struct_lock))
		return (EINTR);
	sbuf_printf(m, "%u objects, %zu bytes\n",
		   dev_priv->mm.object_count,
		   dev_priv->mm.object_memory);

	size = count = mappable_size = mappable_count = 0;
	count_objects(&dev_priv->mm.gtt_list, gtt_list);
	sbuf_printf(m, "%u [%u] objects, %zu [%zu] bytes in gtt\n",
		   count, mappable_count, size, mappable_size);

	size = count = mappable_size = mappable_count = 0;
	count_objects(&dev_priv->mm.active_list, mm_list);
	count_objects(&dev_priv->mm.flushing_list, mm_list);
	sbuf_printf(m, "  %u [%u] active objects, %zu [%zu] bytes\n",
		   count, mappable_count, size, mappable_size);

	size = count = mappable_size = mappable_count = 0;
	count_objects(&dev_priv->mm.pinned_list, mm_list);
	sbuf_printf(m, "  %u [%u] pinned objects, %zu [%zu] bytes\n",
		   count, mappable_count, size, mappable_size);

	size = count = mappable_size = mappable_count = 0;
	count_objects(&dev_priv->mm.inactive_list, mm_list);
	sbuf_printf(m, "  %u [%u] inactive objects, %zu [%zu] bytes\n",
		   count, mappable_count, size, mappable_size);

	size = count = mappable_size = mappable_count = 0;
	count_objects(&dev_priv->mm.deferred_free_list, mm_list);
	sbuf_printf(m, "  %u [%u] freed objects, %zu [%zu] bytes\n",
		   count, mappable_count, size, mappable_size);

	size = count = mappable_size = mappable_count = 0;
	list_for_each_entry(obj, &dev_priv->mm.gtt_list, gtt_list) {
		if (obj->fault_mappable) {
			size += obj->gtt_space->size;
			++count;
		}
		if (obj->pin_mappable) {
			mappable_size += obj->gtt_space->size;
			++mappable_count;
		}
	}
	sbuf_printf(m, "%u pinned mappable objects, %zu bytes\n",
		   mappable_count, mappable_size);
	sbuf_printf(m, "%u fault mappable objects, %zu bytes\n",
		   count, size);

	sbuf_printf(m, "%zu [%zu] gtt total\n",
		   dev_priv->mm.gtt_total, dev_priv->mm.mappable_gtt_total);
	DRM_UNLOCK(dev);

	return (0);
}

static int
i915_gem_gtt_info(struct drm_device *dev, struct sbuf *m, void* data)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj;
	size_t total_obj_size, total_gtt_size;
	int count;

	if (sx_xlock_sig(&dev->dev_struct_lock))
		return (EINTR);

	total_obj_size = total_gtt_size = count = 0;
	list_for_each_entry(obj, &dev_priv->mm.gtt_list, gtt_list) {
		sbuf_printf(m, "   ");
		describe_obj(m, obj);
		sbuf_printf(m, "\n");
		total_obj_size += obj->base.size;
		total_gtt_size += obj->gtt_space->size;
		count++;
	}

	DRM_UNLOCK(dev);

	sbuf_printf(m, "Total %d objects, %zu bytes, %zu GTT size\n",
		   count, total_obj_size, total_gtt_size);

	return (0);
}

static int
i915_gem_pageflip_info(struct drm_device *dev, struct sbuf *m, void *data)
{
	struct intel_crtc *crtc;
	struct drm_i915_gem_object *obj;
	struct intel_unpin_work *work;
	char pipe;
	char plane;

	if ((dev->driver->driver_features & DRIVER_MODESET) == 0)
		return (0);
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, base.head) {
		pipe = pipe_name(crtc->pipe);
		plane = plane_name(crtc->plane);

		mtx_lock(&dev->event_lock);
		work = crtc->unpin_work;
		if (work == NULL) {
			sbuf_printf(m, "No flip due on pipe %c (plane %c)\n",
				   pipe, plane);
		} else {
			if (!work->pending) {
				sbuf_printf(m, "Flip queued on pipe %c (plane %c)\n",
					   pipe, plane);
			} else {
				sbuf_printf(m, "Flip pending (waiting for vsync) on pipe %c (plane %c)\n",
					   pipe, plane);
			}
			if (work->enable_stall_check)
				sbuf_printf(m, "Stall check enabled, ");
			else
				sbuf_printf(m, "Stall check waiting for page flip ioctl, ");
			sbuf_printf(m, "%d prepares\n", work->pending);

			if (work->old_fb_obj) {
				obj = work->old_fb_obj;
				if (obj)
					sbuf_printf(m, "Old framebuffer gtt_offset 0x%08x\n", obj->gtt_offset);
			}
			if (work->pending_flip_obj) {
				obj = work->pending_flip_obj;
				if (obj)
					sbuf_printf(m, "New framebuffer gtt_offset 0x%08x\n", obj->gtt_offset);
			}
		}
		mtx_unlock(&dev->event_lock);
	}

	return (0);
}

static int
i915_gem_request_info(struct drm_device *dev, struct sbuf *m, void *data)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_request *gem_request;
	int count;

	if (sx_xlock_sig(&dev->dev_struct_lock))
		return (EINTR);

	count = 0;
	if (!list_empty(&dev_priv->rings[RCS].request_list)) {
		sbuf_printf(m, "Render requests:\n");
		list_for_each_entry(gem_request,
				    &dev_priv->rings[RCS].request_list,
				    list) {
			sbuf_printf(m, "    %d @ %d\n",
				   gem_request->seqno,
				   (int) (jiffies - gem_request->emitted_jiffies));
		}
		count++;
	}
	if (!list_empty(&dev_priv->rings[VCS].request_list)) {
		sbuf_printf(m, "BSD requests:\n");
		list_for_each_entry(gem_request,
				    &dev_priv->rings[VCS].request_list,
				    list) {
			sbuf_printf(m, "    %d @ %d\n",
				   gem_request->seqno,
				   (int) (jiffies - gem_request->emitted_jiffies));
		}
		count++;
	}
	if (!list_empty(&dev_priv->rings[BCS].request_list)) {
		sbuf_printf(m, "BLT requests:\n");
		list_for_each_entry(gem_request,
				    &dev_priv->rings[BCS].request_list,
				    list) {
			sbuf_printf(m, "    %d @ %d\n",
				   gem_request->seqno,
				   (int) (jiffies - gem_request->emitted_jiffies));
		}
		count++;
	}
	DRM_UNLOCK(dev);

	if (count == 0)
		sbuf_printf(m, "No requests\n");

	return 0;
}

static void
i915_ring_seqno_info(struct sbuf *m, struct intel_ring_buffer *ring)
{
	if (ring->get_seqno) {
		sbuf_printf(m, "Current sequence (%s): %d\n",
			   ring->name, ring->get_seqno(ring));
		sbuf_printf(m, "Waiter sequence (%s):  %d\n",
			   ring->name, ring->waiting_seqno);
		sbuf_printf(m, "IRQ sequence (%s):     %d\n",
			   ring->name, ring->irq_seqno);
	}
}

static int
i915_gem_seqno_info(struct drm_device *dev, struct sbuf *m, void *data)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int i;

	if (sx_xlock_sig(&dev->dev_struct_lock))
		return (EINTR);
	for (i = 0; i < I915_NUM_RINGS; i++)
		i915_ring_seqno_info(m, &dev_priv->rings[i]);
	DRM_UNLOCK(dev);
	return (0);
}


static int
i915_interrupt_info(struct drm_device *dev, struct sbuf *m, void *data)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int i, pipe;

	if (sx_xlock_sig(&dev->dev_struct_lock))
		return (EINTR);

	if (!HAS_PCH_SPLIT(dev)) {
		sbuf_printf(m, "Interrupt enable:    %08x\n",
			   I915_READ(IER));
		sbuf_printf(m, "Interrupt identity:  %08x\n",
			   I915_READ(IIR));
		sbuf_printf(m, "Interrupt mask:      %08x\n",
			   I915_READ(IMR));
		for_each_pipe(pipe)
			sbuf_printf(m, "Pipe %c stat:         %08x\n",
				   pipe_name(pipe),
				   I915_READ(PIPESTAT(pipe)));
	} else {
		sbuf_printf(m, "North Display Interrupt enable:		%08x\n",
			   I915_READ(DEIER));
		sbuf_printf(m, "North Display Interrupt identity:	%08x\n",
			   I915_READ(DEIIR));
		sbuf_printf(m, "North Display Interrupt mask:		%08x\n",
			   I915_READ(DEIMR));
		sbuf_printf(m, "South Display Interrupt enable:		%08x\n",
			   I915_READ(SDEIER));
		sbuf_printf(m, "South Display Interrupt identity:	%08x\n",
			   I915_READ(SDEIIR));
		sbuf_printf(m, "South Display Interrupt mask:		%08x\n",
			   I915_READ(SDEIMR));
		sbuf_printf(m, "Graphics Interrupt enable:		%08x\n",
			   I915_READ(GTIER));
		sbuf_printf(m, "Graphics Interrupt identity:		%08x\n",
			   I915_READ(GTIIR));
		sbuf_printf(m, "Graphics Interrupt mask:		%08x\n",
			   I915_READ(GTIMR));
	}
	sbuf_printf(m, "Interrupts received: %d\n",
		   atomic_read(&dev_priv->irq_received));
	for (i = 0; i < I915_NUM_RINGS; i++) {
		if (IS_GEN6(dev) || IS_GEN7(dev)) {
			sbuf_printf(m, "Graphics Interrupt mask (%s):	%08x\n",
				   dev_priv->rings[i].name,
				   I915_READ_IMR(&dev_priv->rings[i]));
		}
		i915_ring_seqno_info(m, &dev_priv->rings[i]);
	}
	DRM_UNLOCK(dev);

	return (0);
}

static int
i915_gem_fence_regs_info(struct drm_device *dev, struct sbuf *m, void *data)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int i;

	if (sx_xlock_sig(&dev->dev_struct_lock))
		return (EINTR);

	sbuf_printf(m, "Reserved fences = %d\n", dev_priv->fence_reg_start);
	sbuf_printf(m, "Total fences = %d\n", dev_priv->num_fence_regs);
	for (i = 0; i < dev_priv->num_fence_regs; i++) {
		struct drm_i915_gem_object *obj = dev_priv->fence_regs[i].obj;

		sbuf_printf(m, "Fenced object[%2d] = ", i);
		if (obj == NULL)
			sbuf_printf(m, "unused");
		else
			describe_obj(m, obj);
		sbuf_printf(m, "\n");
	}

	DRM_UNLOCK(dev);
	return (0);
}

static int
i915_hws_info(struct drm_device *dev, struct sbuf *m, void *data)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring;
	const volatile u32 *hws;
	int i;

	ring = &dev_priv->rings[(uintptr_t)data];
	hws = (volatile u32 *)ring->status_page.page_addr;
	if (hws == NULL)
		return (0);

	for (i = 0; i < 4096 / sizeof(u32) / 4; i += 4) {
		sbuf_printf(m, "0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
			   i * 4,
			   hws[i], hws[i + 1], hws[i + 2], hws[i + 3]);
	}
	return (0);
}

static int
i915_ringbuffer_data(struct drm_device *dev, struct sbuf *m, void *data)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring;

	if (sx_xlock_sig(&dev->dev_struct_lock))
		return (EINTR);
	ring = &dev_priv->rings[(uintptr_t)data];
	if (!ring->obj) {
		sbuf_printf(m, "No ringbuffer setup\n");
	} else {
		u8 *virt = ring->virtual_start;
		uint32_t off;

		for (off = 0; off < ring->size; off += 4) {
			uint32_t *ptr = (uint32_t *)(virt + off);
			sbuf_printf(m, "%08x :  %08x\n", off, *ptr);
		}
	}
	DRM_UNLOCK(dev);
	return (0);
}

static int
i915_ringbuffer_info(struct drm_device *dev, struct sbuf *m, void *data)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring;

	ring = &dev_priv->rings[(uintptr_t)data];
	if (ring->size == 0)
		return (0);

	if (sx_xlock_sig(&dev->dev_struct_lock))
		return (EINTR);

	sbuf_printf(m, "Ring %s:\n", ring->name);
	sbuf_printf(m, "  Head :    %08x\n", I915_READ_HEAD(ring) & HEAD_ADDR);
	sbuf_printf(m, "  Tail :    %08x\n", I915_READ_TAIL(ring) & TAIL_ADDR);
	sbuf_printf(m, "  Size :    %08x\n", ring->size);
	sbuf_printf(m, "  Active :  %08x\n", intel_ring_get_active_head(ring));
	sbuf_printf(m, "  NOPID :   %08x\n", I915_READ_NOPID(ring));
	if (IS_GEN6(dev) || IS_GEN7(dev)) {
		sbuf_printf(m, "  Sync 0 :   %08x\n", I915_READ_SYNC_0(ring));
		sbuf_printf(m, "  Sync 1 :   %08x\n", I915_READ_SYNC_1(ring));
	}
	sbuf_printf(m, "  Control : %08x\n", I915_READ_CTL(ring));
	sbuf_printf(m, "  Start :   %08x\n", I915_READ_START(ring));

	DRM_UNLOCK(dev);

	return (0);
}

static const char *
ring_str(int ring)
{
	switch (ring) {
	case RCS: return (" render");
	case VCS: return (" bsd");
	case BCS: return (" blt");
	default: return ("");
	}
}

static const char *
pin_flag(int pinned)
{
	if (pinned > 0)
		return (" P");
	else if (pinned < 0)
		return (" p");
	else
		return ("");
}

static const char *tiling_flag(int tiling)
{
	switch (tiling) {
	default:
	case I915_TILING_NONE: return "";
	case I915_TILING_X: return " X";
	case I915_TILING_Y: return " Y";
	}
}

static const char *dirty_flag(int dirty)
{
	return dirty ? " dirty" : "";
}

static const char *purgeable_flag(int purgeable)
{
	return purgeable ? " purgeable" : "";
}

static void print_error_buffers(struct sbuf *m, const char *name,
    struct drm_i915_error_buffer *err, int count)
{

	sbuf_printf(m, "%s [%d]:\n", name, count);

	while (count--) {
		sbuf_printf(m, "  %08x %8u %04x %04x %08x%s%s%s%s%s%s%s",
			   err->gtt_offset,
			   err->size,
			   err->read_domains,
			   err->write_domain,
			   err->seqno,
			   pin_flag(err->pinned),
			   tiling_flag(err->tiling),
			   dirty_flag(err->dirty),
			   purgeable_flag(err->purgeable),
			   err->ring != -1 ? " " : "",
			   ring_str(err->ring),
			   cache_level_str(err->cache_level));

		if (err->name)
			sbuf_printf(m, " (name: %d)", err->name);
		if (err->fence_reg != I915_FENCE_REG_NONE)
			sbuf_printf(m, " (fence: %d)", err->fence_reg);

		sbuf_printf(m, "\n");
		err++;
	}
}

static void
i915_ring_error_state(struct sbuf *m, struct drm_device *dev,
    struct drm_i915_error_state *error, unsigned ring)
{

	sbuf_printf(m, "%s command stream:\n", ring_str(ring));
	sbuf_printf(m, "  HEAD: 0x%08x\n", error->head[ring]);
	sbuf_printf(m, "  TAIL: 0x%08x\n", error->tail[ring]);
	sbuf_printf(m, "  ACTHD: 0x%08x\n", error->acthd[ring]);
	sbuf_printf(m, "  IPEIR: 0x%08x\n", error->ipeir[ring]);
	sbuf_printf(m, "  IPEHR: 0x%08x\n", error->ipehr[ring]);
	sbuf_printf(m, "  INSTDONE: 0x%08x\n", error->instdone[ring]);
	if (ring == RCS && INTEL_INFO(dev)->gen >= 4) {
		sbuf_printf(m, "  INSTDONE1: 0x%08x\n", error->instdone1);
		sbuf_printf(m, "  BBADDR: 0x%08jx\n", (uintmax_t)error->bbaddr);
	}
	if (INTEL_INFO(dev)->gen >= 4)
		sbuf_printf(m, "  INSTPS: 0x%08x\n", error->instps[ring]);
	sbuf_printf(m, "  INSTPM: 0x%08x\n", error->instpm[ring]);
	if (INTEL_INFO(dev)->gen >= 6) {
		sbuf_printf(m, "  FADDR: 0x%08x\n", error->faddr[ring]);
		sbuf_printf(m, "  FAULT_REG: 0x%08x\n", error->fault_reg[ring]);
		sbuf_printf(m, "  SYNC_0: 0x%08x\n",
			   error->semaphore_mboxes[ring][0]);
		sbuf_printf(m, "  SYNC_1: 0x%08x\n",
			   error->semaphore_mboxes[ring][1]);
	}
	sbuf_printf(m, "  seqno: 0x%08x\n", error->seqno[ring]);
	sbuf_printf(m, "  ring->head: 0x%08x\n", error->cpu_ring_head[ring]);
	sbuf_printf(m, "  ring->tail: 0x%08x\n", error->cpu_ring_tail[ring]);
}

static int i915_error_state(struct drm_device *dev, struct sbuf *m,
    void *unused)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_error_state *error;
	int i, j, page, offset, elt;

	mtx_lock(&dev_priv->error_lock);
	if (!dev_priv->first_error) {
		sbuf_printf(m, "no error state collected\n");
		goto out;
	}

	error = dev_priv->first_error;

	sbuf_printf(m, "Time: %jd s %jd us\n", (intmax_t)error->time.tv_sec,
	    (intmax_t)error->time.tv_usec);
	sbuf_printf(m, "PCI ID: 0x%04x\n", dev->pci_device);
	sbuf_printf(m, "EIR: 0x%08x\n", error->eir);
	sbuf_printf(m, "PGTBL_ER: 0x%08x\n", error->pgtbl_er);

	for (i = 0; i < dev_priv->num_fence_regs; i++)
		sbuf_printf(m, "  fence[%d] = %08jx\n", i,
		    (uintmax_t)error->fence[i]);

	if (INTEL_INFO(dev)->gen >= 6) {
		sbuf_printf(m, "ERROR: 0x%08x\n", error->error);
		sbuf_printf(m, "DONE_REG: 0x%08x\n", error->done_reg);
	}

	i915_ring_error_state(m, dev, error, RCS);
	if (HAS_BLT(dev))
		i915_ring_error_state(m, dev, error, BCS);
	if (HAS_BSD(dev))
		i915_ring_error_state(m, dev, error, VCS);

	if (error->active_bo)
		print_error_buffers(m, "Active",
				    error->active_bo,
				    error->active_bo_count);

	if (error->pinned_bo)
		print_error_buffers(m, "Pinned",
				    error->pinned_bo,
				    error->pinned_bo_count);

	for (i = 0; i < DRM_ARRAY_SIZE(error->ring); i++) {
		struct drm_i915_error_object *obj;
 
		if ((obj = error->ring[i].batchbuffer)) {
			sbuf_printf(m, "%s --- gtt_offset = 0x%08x\n",
				   dev_priv->rings[i].name,
				   obj->gtt_offset);
			offset = 0;
			for (page = 0; page < obj->page_count; page++) {
				for (elt = 0; elt < PAGE_SIZE/4; elt++) {
					sbuf_printf(m, "%08x :  %08x\n",
					    offset, obj->pages[page][elt]);
					offset += 4;
				}
			}
		}

		if (error->ring[i].num_requests) {
			sbuf_printf(m, "%s --- %d requests\n",
				   dev_priv->rings[i].name,
				   error->ring[i].num_requests);
			for (j = 0; j < error->ring[i].num_requests; j++) {
				sbuf_printf(m, "  seqno 0x%08x, emitted %ld, tail 0x%08x\n",
					   error->ring[i].requests[j].seqno,
					   error->ring[i].requests[j].jiffies,
					   error->ring[i].requests[j].tail);
			}
		}

		if ((obj = error->ring[i].ringbuffer)) {
			sbuf_printf(m, "%s --- ringbuffer = 0x%08x\n",
				   dev_priv->rings[i].name,
				   obj->gtt_offset);
			offset = 0;
			for (page = 0; page < obj->page_count; page++) {
				for (elt = 0; elt < PAGE_SIZE/4; elt++) {
					sbuf_printf(m, "%08x :  %08x\n",
						   offset,
						   obj->pages[page][elt]);
					offset += 4;
				}
			}
		}
	}

	if (error->overlay)
		intel_overlay_print_error_state(m, error->overlay);

	if (error->display)
		intel_display_print_error_state(m, dev, error->display);

out:
	mtx_unlock(&dev_priv->error_lock);

	return (0);
}

static int
i915_rstdby_delays(struct drm_device *dev, struct sbuf *m, void *unused)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	u16 crstanddelay;

	if (sx_xlock_sig(&dev->dev_struct_lock))
		return (EINTR);
	crstanddelay = I915_READ16(CRSTANDVID);
	DRM_UNLOCK(dev);

	sbuf_printf(m, "w/ctx: %d, w/o ctx: %d\n",
	    (crstanddelay >> 8) & 0x3f, (crstanddelay & 0x3f));

	return 0;
}

static int
i915_cur_delayinfo(struct drm_device *dev, struct sbuf *m, void *unused)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	if (IS_GEN5(dev)) {
		u16 rgvswctl = I915_READ16(MEMSWCTL);
		u16 rgvstat = I915_READ16(MEMSTAT_ILK);

		sbuf_printf(m, "Requested P-state: %d\n", (rgvswctl >> 8) & 0xf);
		sbuf_printf(m, "Requested VID: %d\n", rgvswctl & 0x3f);
		sbuf_printf(m, "Current VID: %d\n", (rgvstat & MEMSTAT_VID_MASK) >>
			   MEMSTAT_VID_SHIFT);
		sbuf_printf(m, "Current P-state: %d\n",
			   (rgvstat & MEMSTAT_PSTATE_MASK) >> MEMSTAT_PSTATE_SHIFT);
	} else if (IS_GEN6(dev)) {
		u32 gt_perf_status = I915_READ(GEN6_GT_PERF_STATUS);
		u32 rp_state_limits = I915_READ(GEN6_RP_STATE_LIMITS);
		u32 rp_state_cap = I915_READ(GEN6_RP_STATE_CAP);
		u32 rpstat;
		u32 rpupei, rpcurup, rpprevup;
		u32 rpdownei, rpcurdown, rpprevdown;
		int max_freq;

		/* RPSTAT1 is in the GT power well */
		if (sx_xlock_sig(&dev->dev_struct_lock))
			return (EINTR);
		gen6_gt_force_wake_get(dev_priv);

		rpstat = I915_READ(GEN6_RPSTAT1);
		rpupei = I915_READ(GEN6_RP_CUR_UP_EI);
		rpcurup = I915_READ(GEN6_RP_CUR_UP);
		rpprevup = I915_READ(GEN6_RP_PREV_UP);
		rpdownei = I915_READ(GEN6_RP_CUR_DOWN_EI);
		rpcurdown = I915_READ(GEN6_RP_CUR_DOWN);
		rpprevdown = I915_READ(GEN6_RP_PREV_DOWN);

		gen6_gt_force_wake_put(dev_priv);
		DRM_UNLOCK(dev);

		sbuf_printf(m, "GT_PERF_STATUS: 0x%08x\n", gt_perf_status);
		sbuf_printf(m, "RPSTAT1: 0x%08x\n", rpstat);
		sbuf_printf(m, "Render p-state ratio: %d\n",
			   (gt_perf_status & 0xff00) >> 8);
		sbuf_printf(m, "Render p-state VID: %d\n",
			   gt_perf_status & 0xff);
		sbuf_printf(m, "Render p-state limit: %d\n",
			   rp_state_limits & 0xff);
		sbuf_printf(m, "CAGF: %dMHz\n", ((rpstat & GEN6_CAGF_MASK) >>
						GEN6_CAGF_SHIFT) * 50);
		sbuf_printf(m, "RP CUR UP EI: %dus\n", rpupei &
			   GEN6_CURICONT_MASK);
		sbuf_printf(m, "RP CUR UP: %dus\n", rpcurup &
			   GEN6_CURBSYTAVG_MASK);
		sbuf_printf(m, "RP PREV UP: %dus\n", rpprevup &
			   GEN6_CURBSYTAVG_MASK);
		sbuf_printf(m, "RP CUR DOWN EI: %dus\n", rpdownei &
			   GEN6_CURIAVG_MASK);
		sbuf_printf(m, "RP CUR DOWN: %dus\n", rpcurdown &
			   GEN6_CURBSYTAVG_MASK);
		sbuf_printf(m, "RP PREV DOWN: %dus\n", rpprevdown &
			   GEN6_CURBSYTAVG_MASK);

		max_freq = (rp_state_cap & 0xff0000) >> 16;
		sbuf_printf(m, "Lowest (RPN) frequency: %dMHz\n",
			   max_freq * 50);

		max_freq = (rp_state_cap & 0xff00) >> 8;
		sbuf_printf(m, "Nominal (RP1) frequency: %dMHz\n",
			   max_freq * 50);

		max_freq = rp_state_cap & 0xff;
		sbuf_printf(m, "Max non-overclocked (RP0) frequency: %dMHz\n",
			   max_freq * 50);
	} else {
		sbuf_printf(m, "no P-state info available\n");
	}

	return 0;
}

static int
i915_delayfreq_table(struct drm_device *dev, struct sbuf *m, void *unused)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	u32 delayfreq;
	int i;

	if (sx_xlock_sig(&dev->dev_struct_lock))
		return (EINTR);
	for (i = 0; i < 16; i++) {
		delayfreq = I915_READ(PXVFREQ_BASE + i * 4);
		sbuf_printf(m, "P%02dVIDFREQ: 0x%08x (VID: %d)\n", i, delayfreq,
			   (delayfreq & PXVFREQ_PX_MASK) >> PXVFREQ_PX_SHIFT);
	}
	DRM_UNLOCK(dev);
	return (0);
}

static inline int
MAP_TO_MV(int map)
{
	return 1250 - (map * 25);
}

static int
i915_inttoext_table(struct drm_device *dev, struct sbuf *m, void *unused)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	u32 inttoext;
	int i;

	if (sx_xlock_sig(&dev->dev_struct_lock))
		return (EINTR);
	for (i = 1; i <= 32; i++) {
		inttoext = I915_READ(INTTOEXT_BASE_ILK + i * 4);
		sbuf_printf(m, "INTTOEXT%02d: 0x%08x\n", i, inttoext);
	}
	DRM_UNLOCK(dev);

	return (0);
}

static int
ironlake_drpc_info(struct drm_device *dev, struct sbuf *m)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	u32 rgvmodectl;
	u32 rstdbyctl;
	u16 crstandvid;

	if (sx_xlock_sig(&dev->dev_struct_lock))
		return (EINTR);
	rgvmodectl = I915_READ(MEMMODECTL);
	rstdbyctl = I915_READ(RSTDBYCTL);
	crstandvid = I915_READ16(CRSTANDVID);
	DRM_UNLOCK(dev);

	sbuf_printf(m, "HD boost: %s\n", (rgvmodectl & MEMMODE_BOOST_EN) ?
		   "yes" : "no");
	sbuf_printf(m, "Boost freq: %d\n",
		   (rgvmodectl & MEMMODE_BOOST_FREQ_MASK) >>
		   MEMMODE_BOOST_FREQ_SHIFT);
	sbuf_printf(m, "HW control enabled: %s\n",
		   rgvmodectl & MEMMODE_HWIDLE_EN ? "yes" : "no");
	sbuf_printf(m, "SW control enabled: %s\n",
		   rgvmodectl & MEMMODE_SWMODE_EN ? "yes" : "no");
	sbuf_printf(m, "Gated voltage change: %s\n",
		   rgvmodectl & MEMMODE_RCLK_GATE ? "yes" : "no");
	sbuf_printf(m, "Starting frequency: P%d\n",
		   (rgvmodectl & MEMMODE_FSTART_MASK) >> MEMMODE_FSTART_SHIFT);
	sbuf_printf(m, "Max P-state: P%d\n",
		   (rgvmodectl & MEMMODE_FMAX_MASK) >> MEMMODE_FMAX_SHIFT);
	sbuf_printf(m, "Min P-state: P%d\n", (rgvmodectl & MEMMODE_FMIN_MASK));
	sbuf_printf(m, "RS1 VID: %d\n", (crstandvid & 0x3f));
	sbuf_printf(m, "RS2 VID: %d\n", ((crstandvid >> 8) & 0x3f));
	sbuf_printf(m, "Render standby enabled: %s\n",
		   (rstdbyctl & RCX_SW_EXIT) ? "no" : "yes");
	sbuf_printf(m, "Current RS state: ");
	switch (rstdbyctl & RSX_STATUS_MASK) {
	case RSX_STATUS_ON:
		sbuf_printf(m, "on\n");
		break;
	case RSX_STATUS_RC1:
		sbuf_printf(m, "RC1\n");
		break;
	case RSX_STATUS_RC1E:
		sbuf_printf(m, "RC1E\n");
		break;
	case RSX_STATUS_RS1:
		sbuf_printf(m, "RS1\n");
		break;
	case RSX_STATUS_RS2:
		sbuf_printf(m, "RS2 (RC6)\n");
		break;
	case RSX_STATUS_RS3:
		sbuf_printf(m, "RC3 (RC6+)\n");
		break;
	default:
		sbuf_printf(m, "unknown\n");
		break;
	}

	return 0;
}

static int
gen6_drpc_info(struct drm_device *dev, struct sbuf *m)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	u32 rpmodectl1, gt_core_status, rcctl1;
	unsigned forcewake_count;
	int count=0;

	if (sx_xlock_sig(&dev->dev_struct_lock))
		return (EINTR);

	mtx_lock(&dev_priv->gt_lock);
	forcewake_count = dev_priv->forcewake_count;
	mtx_unlock(&dev_priv->gt_lock);

	if (forcewake_count) {
		sbuf_printf(m, "RC information inaccurate because userspace "
			      "holds a reference \n");
	} else {
		/* NB: we cannot use forcewake, else we read the wrong values */
		while (count++ < 50 && (I915_READ_NOTRACE(FORCEWAKE_ACK) & 1))
			DRM_UDELAY(10);
		sbuf_printf(m, "RC information accurate: %s\n", yesno(count < 51));
	}

	gt_core_status = DRM_READ32(dev_priv->mmio_map, GEN6_GT_CORE_STATUS);
	trace_i915_reg_rw(false, GEN6_GT_CORE_STATUS, gt_core_status, 4);

	rpmodectl1 = I915_READ(GEN6_RP_CONTROL);
	rcctl1 = I915_READ(GEN6_RC_CONTROL);
	DRM_UNLOCK(dev);

	sbuf_printf(m, "Video Turbo Mode: %s\n",
		   yesno(rpmodectl1 & GEN6_RP_MEDIA_TURBO));
	sbuf_printf(m, "HW control enabled: %s\n",
		   yesno(rpmodectl1 & GEN6_RP_ENABLE));
	sbuf_printf(m, "SW control enabled: %s\n",
		   yesno((rpmodectl1 & GEN6_RP_MEDIA_MODE_MASK) ==
			  GEN6_RP_MEDIA_SW_MODE));
	sbuf_printf(m, "RC1e Enabled: %s\n",
		   yesno(rcctl1 & GEN6_RC_CTL_RC1e_ENABLE));
	sbuf_printf(m, "RC6 Enabled: %s\n",
		   yesno(rcctl1 & GEN6_RC_CTL_RC6_ENABLE));
	sbuf_printf(m, "Deep RC6 Enabled: %s\n",
		   yesno(rcctl1 & GEN6_RC_CTL_RC6p_ENABLE));
	sbuf_printf(m, "Deepest RC6 Enabled: %s\n",
		   yesno(rcctl1 & GEN6_RC_CTL_RC6pp_ENABLE));
	sbuf_printf(m, "Current RC state: ");
	switch (gt_core_status & GEN6_RCn_MASK) {
	case GEN6_RC0:
		if (gt_core_status & GEN6_CORE_CPD_STATE_MASK)
			sbuf_printf(m, "Core Power Down\n");
		else
			sbuf_printf(m, "on\n");
		break;
	case GEN6_RC3:
		sbuf_printf(m, "RC3\n");
		break;
	case GEN6_RC6:
		sbuf_printf(m, "RC6\n");
		break;
	case GEN6_RC7:
		sbuf_printf(m, "RC7\n");
		break;
	default:
		sbuf_printf(m, "Unknown\n");
		break;
	}

	sbuf_printf(m, "Core Power Down: %s\n",
		   yesno(gt_core_status & GEN6_CORE_CPD_STATE_MASK));
	return 0;
}

static int i915_drpc_info(struct drm_device *dev, struct sbuf *m, void *unused)
{

	if (IS_GEN6(dev) || IS_GEN7(dev))
		return (gen6_drpc_info(dev, m));
	else
		return (ironlake_drpc_info(dev, m));
}
static int
i915_fbc_status(struct drm_device *dev, struct sbuf *m, void *unused)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	if (!I915_HAS_FBC(dev)) {
		sbuf_printf(m, "FBC unsupported on this chipset");
		return 0;
	}

	if (intel_fbc_enabled(dev)) {
		sbuf_printf(m, "FBC enabled");
	} else {
		sbuf_printf(m, "FBC disabled: ");
		switch (dev_priv->no_fbc_reason) {
		case FBC_NO_OUTPUT:
			sbuf_printf(m, "no outputs");
			break;
		case FBC_STOLEN_TOO_SMALL:
			sbuf_printf(m, "not enough stolen memory");
			break;
		case FBC_UNSUPPORTED_MODE:
			sbuf_printf(m, "mode not supported");
			break;
		case FBC_MODE_TOO_LARGE:
			sbuf_printf(m, "mode too large");
			break;
		case FBC_BAD_PLANE:
			sbuf_printf(m, "FBC unsupported on plane");
			break;
		case FBC_NOT_TILED:
			sbuf_printf(m, "scanout buffer not tiled");
			break;
		case FBC_MULTIPLE_PIPES:
			sbuf_printf(m, "multiple pipes are enabled");
			break;
		default:
			sbuf_printf(m, "unknown reason");
		}
	}
	return 0;
}

static int
i915_sr_status(struct drm_device *dev, struct sbuf *m, void *unused)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	bool sr_enabled = false;

	if (HAS_PCH_SPLIT(dev))
		sr_enabled = I915_READ(WM1_LP_ILK) & WM1_LP_SR_EN;
	else if (IS_CRESTLINE(dev) || IS_I945G(dev) || IS_I945GM(dev))
		sr_enabled = I915_READ(FW_BLC_SELF) & FW_BLC_SELF_EN;
	else if (IS_I915GM(dev))
		sr_enabled = I915_READ(INSTPM) & INSTPM_SELF_EN;
	else if (IS_PINEVIEW(dev))
		sr_enabled = I915_READ(DSPFW3) & PINEVIEW_SELF_REFRESH_EN;

	sbuf_printf(m, "self-refresh: %s",
		   sr_enabled ? "enabled" : "disabled");

	return (0);
}

static int i915_ring_freq_table(struct drm_device *dev, struct sbuf *m,
    void *unused)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int gpu_freq, ia_freq;

	if (!(IS_GEN6(dev) || IS_GEN7(dev))) {
		sbuf_printf(m, "unsupported on this chipset");
		return (0);
	}

	if (sx_xlock_sig(&dev->dev_struct_lock))
		return (EINTR);

	sbuf_printf(m, "GPU freq (MHz)\tEffective CPU freq (MHz)\n");

	for (gpu_freq = dev_priv->min_delay; gpu_freq <= dev_priv->max_delay;
	     gpu_freq++) {
		I915_WRITE(GEN6_PCODE_DATA, gpu_freq);
		I915_WRITE(GEN6_PCODE_MAILBOX, GEN6_PCODE_READY |
			   GEN6_PCODE_READ_MIN_FREQ_TABLE);
		if (_intel_wait_for(dev,
		    (I915_READ(GEN6_PCODE_MAILBOX) & GEN6_PCODE_READY) == 0,
		    10, 1, "915frq")) {
			DRM_ERROR("pcode read of freq table timed out\n");
			continue;
		}
		ia_freq = I915_READ(GEN6_PCODE_DATA);
		sbuf_printf(m, "%d\t\t%d\n", gpu_freq * 50, ia_freq * 100);
	}

	DRM_UNLOCK(dev);

	return (0);
}

static int
i915_emon_status(struct drm_device *dev, struct sbuf *m, void *unused)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	unsigned long temp, chipset, gfx;

	if (!IS_GEN5(dev)) {
		sbuf_printf(m, "Not supported\n");
		return (0);
	}

	if (sx_xlock_sig(&dev->dev_struct_lock))
		return (EINTR);
	temp = i915_mch_val(dev_priv);
	chipset = i915_chipset_val(dev_priv);
	gfx = i915_gfx_val(dev_priv);
	DRM_UNLOCK(dev);

	sbuf_printf(m, "GMCH temp: %ld\n", temp);
	sbuf_printf(m, "Chipset power: %ld\n", chipset);
	sbuf_printf(m, "GFX power: %ld\n", gfx);
	sbuf_printf(m, "Total power: %ld\n", chipset + gfx);

	return (0);
}

static int
i915_gfxec(struct drm_device *dev, struct sbuf *m, void *unused)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	if (sx_xlock_sig(&dev->dev_struct_lock))
		return (EINTR);
	sbuf_printf(m, "GFXEC: %ld\n", (unsigned long)I915_READ(0x112f4));
	DRM_UNLOCK(dev);

	return (0);
}

#if 0
static int
i915_opregion(struct drm_device *dev, struct sbuf *m, void *unused)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_opregion *opregion = &dev_priv->opregion;

	if (sx_xlock_sig(&dev->dev_struct_lock))
		return (EINTR);
	if (opregion->header)
		seq_write(m, opregion->header, OPREGION_SIZE);
	DRM_UNLOCK(dev);

	return 0;
}
#endif

static int
i915_gem_framebuffer_info(struct drm_device *dev, struct sbuf *m, void *data)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_fbdev *ifbdev;
	struct intel_framebuffer *fb;

	if (sx_xlock_sig(&dev->dev_struct_lock))
		return (EINTR);

	ifbdev = dev_priv->fbdev;
	if (ifbdev == NULL) {
		DRM_UNLOCK(dev);
		return (0);
	}
	fb = to_intel_framebuffer(ifbdev->helper.fb);

	sbuf_printf(m, "fbcon size: %d x %d, depth %d, %d bpp, obj ",
		   fb->base.width,
		   fb->base.height,
		   fb->base.depth,
		   fb->base.bits_per_pixel);
	describe_obj(m, fb->obj);
	sbuf_printf(m, "\n");

	list_for_each_entry(fb, &dev->mode_config.fb_list, base.head) {
		if (&fb->base == ifbdev->helper.fb)
			continue;

		sbuf_printf(m, "user size: %d x %d, depth %d, %d bpp, obj ",
			   fb->base.width,
			   fb->base.height,
			   fb->base.depth,
			   fb->base.bits_per_pixel);
		describe_obj(m, fb->obj);
		sbuf_printf(m, "\n");
	}

	DRM_UNLOCK(dev);

	return (0);
}

static int
i915_context_status(struct drm_device *dev, struct sbuf *m, void *data)
{
	drm_i915_private_t *dev_priv;
	int ret;

	if ((dev->driver->driver_features & DRIVER_MODESET) == 0)
		return (0);

	dev_priv = dev->dev_private;
	ret = sx_xlock_sig(&dev->mode_config.mutex);
	if (ret != 0)
		return (EINTR);

	if (dev_priv->pwrctx != NULL) {
		sbuf_printf(m, "power context ");
		describe_obj(m, dev_priv->pwrctx);
		sbuf_printf(m, "\n");
	}

	if (dev_priv->renderctx != NULL) {
		sbuf_printf(m, "render context ");
		describe_obj(m, dev_priv->renderctx);
		sbuf_printf(m, "\n");
	}

	sx_xunlock(&dev->mode_config.mutex);

	return (0);
}

static int
i915_gen6_forcewake_count_info(struct drm_device *dev, struct sbuf *m,
    void *data)
{
	struct drm_i915_private *dev_priv;
	unsigned forcewake_count;

	dev_priv = dev->dev_private;
	mtx_lock(&dev_priv->gt_lock);
	forcewake_count = dev_priv->forcewake_count;
	mtx_unlock(&dev_priv->gt_lock);

	sbuf_printf(m, "forcewake count = %u\n", forcewake_count);

	return (0);
}

static const char *
swizzle_string(unsigned swizzle)
{

	switch(swizzle) {
	case I915_BIT_6_SWIZZLE_NONE:
		return "none";
	case I915_BIT_6_SWIZZLE_9:
		return "bit9";
	case I915_BIT_6_SWIZZLE_9_10:
		return "bit9/bit10";
	case I915_BIT_6_SWIZZLE_9_11:
		return "bit9/bit11";
	case I915_BIT_6_SWIZZLE_9_10_11:
		return "bit9/bit10/bit11";
	case I915_BIT_6_SWIZZLE_9_17:
		return "bit9/bit17";
	case I915_BIT_6_SWIZZLE_9_10_17:
		return "bit9/bit10/bit17";
	case I915_BIT_6_SWIZZLE_UNKNOWN:
		return "unknown";
	}

	return "bug";
}

static int
i915_swizzle_info(struct drm_device *dev, struct sbuf *m, void *data)
{
	struct drm_i915_private *dev_priv;
	int ret;

	dev_priv = dev->dev_private;
	ret = sx_xlock_sig(&dev->dev_struct_lock);
	if (ret != 0)
		return (EINTR);

	sbuf_printf(m, "bit6 swizzle for X-tiling = %s\n",
		   swizzle_string(dev_priv->mm.bit_6_swizzle_x));
	sbuf_printf(m, "bit6 swizzle for Y-tiling = %s\n",
		   swizzle_string(dev_priv->mm.bit_6_swizzle_y));

	if (IS_GEN3(dev) || IS_GEN4(dev)) {
		sbuf_printf(m, "DDC = 0x%08x\n",
			   I915_READ(DCC));
		sbuf_printf(m, "C0DRB3 = 0x%04x\n",
			   I915_READ16(C0DRB3));
		sbuf_printf(m, "C1DRB3 = 0x%04x\n",
			   I915_READ16(C1DRB3));
	} else if (IS_GEN6(dev) || IS_GEN7(dev)) {
		sbuf_printf(m, "MAD_DIMM_C0 = 0x%08x\n",
			   I915_READ(MAD_DIMM_C0));
		sbuf_printf(m, "MAD_DIMM_C1 = 0x%08x\n",
			   I915_READ(MAD_DIMM_C1));
		sbuf_printf(m, "MAD_DIMM_C2 = 0x%08x\n",
			   I915_READ(MAD_DIMM_C2));
		sbuf_printf(m, "TILECTL = 0x%08x\n",
			   I915_READ(TILECTL));
		sbuf_printf(m, "ARB_MODE = 0x%08x\n",
			   I915_READ(ARB_MODE));
		sbuf_printf(m, "DISP_ARB_CTL = 0x%08x\n",
			   I915_READ(DISP_ARB_CTL));
 	}
	DRM_UNLOCK(dev);

	return (0);
}

static int
i915_ppgtt_info(struct drm_device *dev, struct sbuf *m, void *data)
{
	struct drm_i915_private *dev_priv;
	struct intel_ring_buffer *ring;
	int i, ret;

	dev_priv = dev->dev_private;

	ret = sx_xlock_sig(&dev->dev_struct_lock);
	if (ret != 0)
		return (EINTR);
	if (INTEL_INFO(dev)->gen == 6)
		sbuf_printf(m, "GFX_MODE: 0x%08x\n", I915_READ(GFX_MODE));

	for (i = 0; i < I915_NUM_RINGS; i++) {
		ring = &dev_priv->rings[i];

		sbuf_printf(m, "%s\n", ring->name);
		if (INTEL_INFO(dev)->gen == 7)
			sbuf_printf(m, "GFX_MODE: 0x%08x\n", I915_READ(RING_MODE_GEN7(ring)));
		sbuf_printf(m, "PP_DIR_BASE: 0x%08x\n", I915_READ(RING_PP_DIR_BASE(ring)));
		sbuf_printf(m, "PP_DIR_BASE_READ: 0x%08x\n", I915_READ(RING_PP_DIR_BASE_READ(ring)));
		sbuf_printf(m, "PP_DIR_DCLV: 0x%08x\n", I915_READ(RING_PP_DIR_DCLV(ring)));
	}
	if (dev_priv->mm.aliasing_ppgtt) {
		struct i915_hw_ppgtt *ppgtt = dev_priv->mm.aliasing_ppgtt;

		sbuf_printf(m, "aliasing PPGTT:\n");
		sbuf_printf(m, "pd gtt offset: 0x%08x\n", ppgtt->pd_offset);
	}
	sbuf_printf(m, "ECOCHK: 0x%08x\n", I915_READ(GAM_ECOCHK));
	DRM_UNLOCK(dev);

	return (0);
}

static int
i915_debug_set_wedged(SYSCTL_HANDLER_ARGS)
{
	struct drm_device *dev;
	drm_i915_private_t *dev_priv;
	int error, wedged;

	dev = arg1;
	dev_priv = dev->dev_private;
	if (dev_priv == NULL)
		return (EBUSY);
	wedged = dev_priv->mm.wedged;
	error = sysctl_handle_int(oidp, &wedged, 0, req);
	if (error || !req->newptr)
		return (error);
	DRM_INFO("Manually setting wedged to %d\n", wedged);
	i915_handle_error(dev, wedged);
	return (error);
}

static int
i915_max_freq(SYSCTL_HANDLER_ARGS)
{
	struct drm_device *dev;
	drm_i915_private_t *dev_priv;
	int error, max_freq;

	dev = arg1;
	dev_priv = dev->dev_private;
	if (dev_priv == NULL)
		return (EBUSY);
	max_freq = dev_priv->max_delay * 50;
	error = sysctl_handle_int(oidp, &max_freq, 0, req);
	if (error || !req->newptr)
		return (error);
	DRM_DEBUG("Manually setting max freq to %d\n", max_freq);
	/*
	 * Turbo will still be enabled, but won't go above the set value.
	 */
	dev_priv->max_delay = max_freq / 50;
	gen6_set_rps(dev, max_freq / 50);
	return (error);
}

static int
i915_cache_sharing(SYSCTL_HANDLER_ARGS)
{
	struct drm_device *dev;
	drm_i915_private_t *dev_priv;
	int error, snpcr, cache_sharing;

	dev = arg1;
	dev_priv = dev->dev_private;
	if (dev_priv == NULL)
		return (EBUSY);
	DRM_LOCK(dev);
	snpcr = I915_READ(GEN6_MBCUNIT_SNPCR);
	DRM_UNLOCK(dev);
	cache_sharing = (snpcr & GEN6_MBC_SNPCR_MASK) >> GEN6_MBC_SNPCR_SHIFT;
	error = sysctl_handle_int(oidp, &cache_sharing, 0, req);
	if (error || !req->newptr)
		return (error);
	if (cache_sharing < 0 || cache_sharing > 3)
		return (EINVAL);
	DRM_DEBUG("Manually setting uncore sharing to %d\n", cache_sharing);

	DRM_LOCK(dev);
	/* Update the cache sharing policy here as well */
	snpcr = I915_READ(GEN6_MBCUNIT_SNPCR);
	snpcr &= ~GEN6_MBC_SNPCR_MASK;
	snpcr |= (cache_sharing << GEN6_MBC_SNPCR_SHIFT);
	I915_WRITE(GEN6_MBCUNIT_SNPCR, snpcr);
	DRM_UNLOCK(dev);
	return (0);
}

static struct i915_info_sysctl_list {
	const char *name;
	int (*ptr)(struct drm_device *dev, struct sbuf *m, void *data);
	int flags;
	void *data;
} i915_info_sysctl_list[] = {
	{"i915_capabilities", i915_capabilities, 0},
	{"i915_gem_objects", i915_gem_object_info, 0},
	{"i915_gem_gtt", i915_gem_gtt_info, 0},
	{"i915_gem_active", i915_gem_object_list_info, 0, (void *)ACTIVE_LIST},
	{"i915_gem_flushing", i915_gem_object_list_info, 0,
	    (void *)FLUSHING_LIST},
	{"i915_gem_inactive", i915_gem_object_list_info, 0,
	    (void *)INACTIVE_LIST},
	{"i915_gem_pinned", i915_gem_object_list_info, 0,
	    (void *)PINNED_LIST},
	{"i915_gem_deferred_free", i915_gem_object_list_info, 0,
	    (void *)DEFERRED_FREE_LIST},
	{"i915_gem_pageflip", i915_gem_pageflip_info, 0},
	{"i915_gem_request", i915_gem_request_info, 0},
	{"i915_gem_seqno", i915_gem_seqno_info, 0},
	{"i915_gem_fence_regs", i915_gem_fence_regs_info, 0},
	{"i915_gem_interrupt", i915_interrupt_info, 0},
	{"i915_gem_hws", i915_hws_info, 0, (void *)RCS},
	{"i915_gem_hws_blt", i915_hws_info, 0, (void *)BCS},
	{"i915_gem_hws_bsd", i915_hws_info, 0, (void *)VCS},
	{"i915_ringbuffer_data", i915_ringbuffer_data, 0, (void *)RCS},
	{"i915_ringbuffer_info", i915_ringbuffer_info, 0, (void *)RCS},
	{"i915_bsd_ringbuffer_data", i915_ringbuffer_data, 0, (void *)VCS},
	{"i915_bsd_ringbuffer_info", i915_ringbuffer_info, 0, (void *)VCS},
	{"i915_blt_ringbuffer_data", i915_ringbuffer_data, 0, (void *)BCS},
	{"i915_blt_ringbuffer_info", i915_ringbuffer_info, 0, (void *)BCS},
	{"i915_error_state", i915_error_state, 0},
	{"i915_rstdby_delays", i915_rstdby_delays, 0},
	{"i915_cur_delayinfo", i915_cur_delayinfo, 0},
	{"i915_delayfreq_table", i915_delayfreq_table, 0},
	{"i915_inttoext_table", i915_inttoext_table, 0},
	{"i915_drpc_info", i915_drpc_info, 0},
	{"i915_emon_status", i915_emon_status, 0},
	{"i915_ring_freq_table", i915_ring_freq_table, 0},
	{"i915_gfxec", i915_gfxec, 0},
	{"i915_fbc_status", i915_fbc_status, 0},
	{"i915_sr_status", i915_sr_status, 0},
#if 0
	{"i915_opregion", i915_opregion, 0},
#endif
	{"i915_gem_framebuffer", i915_gem_framebuffer_info, 0},
	{"i915_context_status", i915_context_status, 0},
	{"i915_gen6_forcewake_count_info", i915_gen6_forcewake_count_info, 0},
	{"i915_swizzle_info", i915_swizzle_info, 0},
	{"i915_ppgtt_info", i915_ppgtt_info, 0},
};

struct i915_info_sysctl_thunk {
	struct drm_device *dev;
	int idx;
	void *arg;
};

static int
i915_info_sysctl_handler(SYSCTL_HANDLER_ARGS)
{
	struct sbuf m;
	struct i915_info_sysctl_thunk *thunk;
	struct drm_device *dev;
	drm_i915_private_t *dev_priv;
	int error;

	thunk = arg1;
	dev = thunk->dev;
	dev_priv = dev->dev_private;
	if (dev_priv == NULL)
		return (EBUSY);
	error = sysctl_wire_old_buffer(req, 0);
	if (error != 0)
		return (error);
	sbuf_new_for_sysctl(&m, NULL, 128, req);
	error = i915_info_sysctl_list[thunk->idx].ptr(dev, &m,
	    thunk->arg);
	if (error == 0)
		error = sbuf_finish(&m);
	sbuf_delete(&m);
	return (error);
}

extern int i915_gem_sync_exec_requests;
extern int i915_fix_mi_batchbuffer_end;
extern int i915_intr_pf;
extern long i915_gem_wired_pages_cnt;

int
i915_sysctl_init(struct drm_device *dev, struct sysctl_ctx_list *ctx,
    struct sysctl_oid *top)
{
	struct sysctl_oid *oid, *info;
	struct i915_info_sysctl_thunk *thunks;
	int i, error;

	thunks = malloc(sizeof(*thunks) * DRM_ARRAY_SIZE(i915_info_sysctl_list),
	    DRM_MEM_DRIVER, M_WAITOK | M_ZERO);
	for (i = 0; i < DRM_ARRAY_SIZE(i915_info_sysctl_list); i++) {
		thunks[i].dev = dev;
		thunks[i].idx = i;
		thunks[i].arg = i915_info_sysctl_list[i].data;
	}
	dev->sysctl_private = thunks;
	info = SYSCTL_ADD_NODE(ctx, SYSCTL_CHILDREN(top), OID_AUTO, "info",
	    CTLFLAG_RW, NULL, NULL);
	if (info == NULL)
		return (ENOMEM);
	for (i = 0; i < DRM_ARRAY_SIZE(i915_info_sysctl_list); i++) {
		oid = SYSCTL_ADD_OID(ctx, SYSCTL_CHILDREN(info), OID_AUTO,
		    i915_info_sysctl_list[i].name, CTLTYPE_STRING | CTLFLAG_RD,
		    &thunks[i], 0, i915_info_sysctl_handler, "A", NULL);
		if (oid == NULL)
			return (ENOMEM);
	}
	oid = SYSCTL_ADD_LONG(ctx, SYSCTL_CHILDREN(info), OID_AUTO,
	    "i915_gem_wired_pages", CTLFLAG_RD, &i915_gem_wired_pages_cnt,
	    NULL);
	oid = SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(top), OID_AUTO, "wedged",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, dev, 0,
	    i915_debug_set_wedged, "I", NULL);
	if (oid == NULL)
		return (ENOMEM);
	oid = SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(top), OID_AUTO, "max_freq",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, dev, 0, i915_max_freq,
	    "I", NULL);
	if (oid == NULL)
		return (ENOMEM);
	oid = SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(top), OID_AUTO,
	    "cache_sharing", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, dev,
	    0, i915_cache_sharing, "I", NULL);
	if (oid == NULL)
		return (ENOMEM);
	oid = SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(top), OID_AUTO, "sync_exec",
	    CTLFLAG_RW, &i915_gem_sync_exec_requests, 0, NULL);
	if (oid == NULL)
		return (ENOMEM);
	oid = SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(top), OID_AUTO, "fix_mi",
	    CTLFLAG_RW, &i915_fix_mi_batchbuffer_end, 0, NULL);
	if (oid == NULL)
		return (ENOMEM);
	oid = SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(top), OID_AUTO, "intr_pf",
	    CTLFLAG_RW, &i915_intr_pf, 0, NULL);
	if (oid == NULL)
		return (ENOMEM);

	error = drm_add_busid_modesetting(dev, ctx, top);
	if (error != 0)
		return (error);

	return (0);
}

void
i915_sysctl_cleanup(struct drm_device *dev)
{

	free(dev->sysctl_private, DRM_MEM_DRIVER);
}
