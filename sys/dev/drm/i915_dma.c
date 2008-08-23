/* i915_dma.c -- DMA support for the I915 -*- linux-c -*-
 */
/*-
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "dev/drm/drmP.h"
#include "dev/drm/drm.h"
#include "dev/drm/i915_drm.h"
#include "dev/drm/i915_drv.h"

/* Really want an OS-independent resettable timer.  Would like to have
 * this loop run for (eg) 3 sec, but have the timer reset every time
 * the head pointer changes, so that EBUSY only happens if the ring
 * actually stalls for (eg) 3 seconds.
 */
int i915_wait_ring(struct drm_device * dev, int n, const char *caller)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_ring_buffer_t *ring = &(dev_priv->ring);
	u32 last_head = I915_READ(PRB0_HEAD) & HEAD_ADDR;
	int i;

	for (i = 0; i < 10000; i++) {
		ring->head = I915_READ(PRB0_HEAD) & HEAD_ADDR;
		ring->space = ring->head - (ring->tail + 8);
		if (ring->space < 0)
			ring->space += ring->Size;
		if (ring->space >= n)
			return 0;

		if (ring->head != last_head)
			i = 0;

		last_head = ring->head;
		DRM_UDELAY(1);
	}

	return -EBUSY;
}

void i915_kernel_lost_context(struct drm_device * dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_ring_buffer_t *ring = &(dev_priv->ring);

	ring->head = I915_READ(PRB0_HEAD) & HEAD_ADDR;
	ring->tail = I915_READ(PRB0_TAIL) & TAIL_ADDR;
	ring->space = ring->head - (ring->tail + 8);
	if (ring->space < 0)
		ring->space += ring->Size;
}

static int i915_dma_cleanup(struct drm_device * dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	/* Make sure interrupts are disabled here because the uninstall ioctl
	 * may not have been called from userspace and after dev_private
	 * is freed, it's too late.
	 */
	if (dev->irq)
		drm_irq_uninstall(dev);

	if (dev_priv->ring.virtual_start) {
		drm_core_ioremapfree(&dev_priv->ring.map, dev);
		dev_priv->ring.virtual_start = 0;
		dev_priv->ring.map.handle = 0;
		dev_priv->ring.map.size = 0;
	}

	if (dev_priv->status_page_dmah) {
		drm_pci_free(dev, dev_priv->status_page_dmah);
		dev_priv->status_page_dmah = NULL;
		/* Need to rewrite hardware status page */
		I915_WRITE(0x02080, 0x1ffff000);
	}

	if (dev_priv->status_gfx_addr) {
		dev_priv->status_gfx_addr = 0;
		drm_core_ioremapfree(&dev_priv->hws_map, dev);
		I915_WRITE(0x02080, 0x1ffff000);
	}

	return 0;
}

#if defined(I915_HAVE_BUFFER)
#define DRI2_SAREA_BLOCK_TYPE(b) ((b) >> 16)
#define DRI2_SAREA_BLOCK_SIZE(b) ((b) & 0xffff)
#define DRI2_SAREA_BLOCK_NEXT(p)				\
	((void *) ((unsigned char *) (p) +			\
		   DRI2_SAREA_BLOCK_SIZE(*(unsigned int *) p)))

#define DRI2_SAREA_BLOCK_END		0x0000
#define DRI2_SAREA_BLOCK_LOCK		0x0001
#define DRI2_SAREA_BLOCK_EVENT_BUFFER	0x0002

static int
setup_dri2_sarea(struct drm_device * dev,
		 struct drm_file *file_priv,
		 drm_i915_init_t * init)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int ret;
	unsigned int *p, *end, *next;

	mutex_lock(&dev->struct_mutex);
	dev_priv->sarea_bo =
		drm_lookup_buffer_object(file_priv,
					 init->sarea_handle, 1);
	mutex_unlock(&dev->struct_mutex);

	if (!dev_priv->sarea_bo) {
		DRM_ERROR("did not find sarea bo\n");
		return -EINVAL;
	}

	ret = drm_bo_kmap(dev_priv->sarea_bo, 0,
			  dev_priv->sarea_bo->num_pages,
			  &dev_priv->sarea_kmap);
	if (ret) {
		DRM_ERROR("could not map sarea bo\n");
		return ret;
	}

	p = dev_priv->sarea_kmap.virtual;
	end = (void *) p + (dev_priv->sarea_bo->num_pages << PAGE_SHIFT);
	while (p < end && DRI2_SAREA_BLOCK_TYPE(*p) != DRI2_SAREA_BLOCK_END) {
		switch (DRI2_SAREA_BLOCK_TYPE(*p)) {
		case DRI2_SAREA_BLOCK_LOCK:
			dev->lock.hw_lock = (void *) (p + 1);
			dev->sigdata.lock = dev->lock.hw_lock;
			break;
		}
		next = DRI2_SAREA_BLOCK_NEXT(p);
		if (next <= p || end < next) {
			DRM_ERROR("malformed dri2 sarea: next is %p should be within %p-%p\n",
				  next, p, end);
			return -EINVAL;
		}
		p = next;
	}

	return 0;
}
#endif

static int i915_initialize(struct drm_device * dev,
			   struct drm_file *file_priv,
			   drm_i915_init_t * init)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
#if defined(I915_HAVE_BUFFER)
	int ret;
#endif
	dev_priv->sarea = drm_getsarea(dev);
	if (!dev_priv->sarea) {
		DRM_ERROR("can not find sarea!\n");
		i915_dma_cleanup(dev);
		return -EINVAL;
	}

	if (init->mmio_offset != 0)
		dev_priv->mmio_map = drm_core_findmap(dev, init->mmio_offset);
	if (!dev_priv->mmio_map) {
		i915_dma_cleanup(dev);
		DRM_ERROR("can not find mmio map!\n");
		return -EINVAL;
	}

#ifdef I915_HAVE_BUFFER
	dev_priv->max_validate_buffers = I915_MAX_VALIDATE_BUFFERS;
#endif

	if (init->sarea_priv_offset)
		dev_priv->sarea_priv = (drm_i915_sarea_t *)
			((u8 *) dev_priv->sarea->handle +
			 init->sarea_priv_offset);
	else {
		/* No sarea_priv for you! */
		dev_priv->sarea_priv = NULL;
	}

	dev_priv->ring.Start = init->ring_start;
	dev_priv->ring.End = init->ring_end;
	dev_priv->ring.Size = init->ring_size;
	dev_priv->ring.tail_mask = dev_priv->ring.Size - 1;

	dev_priv->ring.map.offset = init->ring_start;
	dev_priv->ring.map.size = init->ring_size;
	dev_priv->ring.map.type = 0;
	dev_priv->ring.map.flags = 0;
	dev_priv->ring.map.mtrr = 0;

	drm_core_ioremap(&dev_priv->ring.map, dev);

	if (dev_priv->ring.map.handle == NULL) {
		i915_dma_cleanup(dev);
		DRM_ERROR("can not ioremap virtual address for"
			  " ring buffer\n");
		return -ENOMEM;
	}

	dev_priv->ring.virtual_start = dev_priv->ring.map.handle;

	dev_priv->cpp = init->cpp;

	if (dev_priv->sarea_priv)
		dev_priv->sarea_priv->pf_current_page = 0;

	/* We are using separate values as placeholders for mechanisms for
	 * private backbuffer/depthbuffer usage.
	 */
	dev_priv->use_mi_batchbuffer_start = 0;
	if (IS_I965G(dev)) /* 965 doesn't support older method */
		dev_priv->use_mi_batchbuffer_start = 1;

	/* Allow hardware batchbuffers unless told otherwise.
	 */
	dev_priv->allow_batchbuffer = 1;

	/* Enable vblank on pipe A for older X servers
	 */
	dev_priv->vblank_pipe = DRM_I915_VBLANK_PIPE_A;

	/* Program Hardware Status Page */
	if (!I915_NEED_GFX_HWS(dev)) {
		dev_priv->status_page_dmah =
			drm_pci_alloc(dev, PAGE_SIZE, PAGE_SIZE, 0xffffffff);

		if (!dev_priv->status_page_dmah) {
			i915_dma_cleanup(dev);
			DRM_ERROR("Can not allocate hardware status page\n");
			return -ENOMEM;
		}
		dev_priv->hw_status_page = dev_priv->status_page_dmah->vaddr;
		dev_priv->dma_status_page = dev_priv->status_page_dmah->busaddr;

		memset(dev_priv->hw_status_page, 0, PAGE_SIZE);

		I915_WRITE(0x02080, dev_priv->dma_status_page);
	}
	DRM_DEBUG("Enabled hardware status page\n");
#ifdef I915_HAVE_BUFFER
	mutex_init(&dev_priv->cmdbuf_mutex);
#endif
#if defined(I915_HAVE_BUFFER)
	if (init->func == I915_INIT_DMA2) {
		ret = setup_dri2_sarea(dev, file_priv, init);
		if (ret) {
			i915_dma_cleanup(dev);
			DRM_ERROR("could not set up dri2 sarea\n");
			return ret;
		}
	}
#endif

	return 0;
}

static int i915_dma_resume(struct drm_device * dev)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;

	DRM_DEBUG("\n");

	if (!dev_priv->sarea) {
		DRM_ERROR("can not find sarea!\n");
		return -EINVAL;
	}

	if (!dev_priv->mmio_map) {
		DRM_ERROR("can not find mmio map!\n");
		return -EINVAL;
	}

	if (dev_priv->ring.map.handle == NULL) {
		DRM_ERROR("can not ioremap virtual address for"
			  " ring buffer\n");
		return -ENOMEM;
	}

	/* Program Hardware Status Page */
	if (!dev_priv->hw_status_page) {
		DRM_ERROR("Can not find hardware status page\n");
		return -EINVAL;
	}
	DRM_DEBUG("hw status page @ %p\n", dev_priv->hw_status_page);

	if (dev_priv->status_gfx_addr != 0)
		I915_WRITE(0x02080, dev_priv->status_gfx_addr);
	else
		I915_WRITE(0x02080, dev_priv->dma_status_page);
	DRM_DEBUG("Enabled hardware status page\n");

	return 0;
}

static int i915_dma_init(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	drm_i915_init_t *init = data;
	int retcode = 0;

	switch (init->func) {
	case I915_INIT_DMA:
	case I915_INIT_DMA2:
		retcode = i915_initialize(dev, file_priv, init);
		break;
	case I915_CLEANUP_DMA:
		retcode = i915_dma_cleanup(dev);
		break;
	case I915_RESUME_DMA:
		retcode = i915_dma_resume(dev);
		break;
	default:
		retcode = -EINVAL;
		break;
	}

	return retcode;
}

/* Implement basically the same security restrictions as hardware does
 * for MI_BATCH_NON_SECURE.  These can be made stricter at any time.
 *
 * Most of the calculations below involve calculating the size of a
 * particular instruction.  It's important to get the size right as
 * that tells us where the next instruction to check is.  Any illegal
 * instruction detected will be given a size of zero, which is a
 * signal to abort the rest of the buffer.
 */
static int do_validate_cmd(int cmd)
{
	switch (((cmd >> 29) & 0x7)) {
	case 0x0:
		switch ((cmd >> 23) & 0x3f) {
		case 0x0:
			return 1;	/* MI_NOOP */
		case 0x4:
			return 1;	/* MI_FLUSH */
		default:
			return 0;	/* disallow everything else */
		}
		break;
	case 0x1:
		return 0;	/* reserved */
	case 0x2:
		return (cmd & 0xff) + 2;	/* 2d commands */
	case 0x3:
		if (((cmd >> 24) & 0x1f) <= 0x18)
			return 1;

		switch ((cmd >> 24) & 0x1f) {
		case 0x1c:
			return 1;
		case 0x1d:
			switch ((cmd >> 16) & 0xff) {
			case 0x3:
				return (cmd & 0x1f) + 2;
			case 0x4:
				return (cmd & 0xf) + 2;
			default:
				return (cmd & 0xffff) + 2;
			}
		case 0x1e:
			if (cmd & (1 << 23))
				return (cmd & 0xffff) + 1;
			else
				return 1;
		case 0x1f:
			if ((cmd & (1 << 23)) == 0)	/* inline vertices */
				return (cmd & 0x1ffff) + 2;
			else if (cmd & (1 << 17))	/* indirect random */
				if ((cmd & 0xffff) == 0)
					return 0;	/* unknown length, too hard */
				else
					return (((cmd & 0xffff) + 1) / 2) + 1;
			else
				return 2;	/* indirect sequential */
		default:
			return 0;
		}
	default:
		return 0;
	}

	return 0;
}

static int validate_cmd(int cmd)
{
	int ret = do_validate_cmd(cmd);

/*	printk("validate_cmd( %x ): %d\n", cmd, ret); */

	return ret;
}

static int i915_emit_cmds(struct drm_device *dev, int __user *buffer,
			  int dwords)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int i;
	RING_LOCALS;

	if ((dwords+1) * sizeof(int) >= dev_priv->ring.Size - 8)
		return -EINVAL;

	BEGIN_LP_RING((dwords+1)&~1);

	for (i = 0; i < dwords;) {
		int cmd, sz;

		cmd = buffer[i];
		if ((sz = validate_cmd(cmd)) == 0 || i + sz > dwords)
			return -EINVAL;

		OUT_RING(cmd);

		while (++i, --sz) {
			OUT_RING(cmd);
			cmd = buffer[i];
		}
	}

	if (dwords & 1)
		OUT_RING(0);

	ADVANCE_LP_RING();

	return 0;
}

static int i915_emit_box(struct drm_device * dev,
			 struct drm_clip_rect __user * boxes,
			 int i, int DR1, int DR4)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_clip_rect box;
	RING_LOCALS;

	box = boxes[i];
	if (box.y2 <= box.y1 || box.x2 <= box.x1 || box.y2 <= 0 || box.x2 <= 0) {
		DRM_ERROR("Bad box %d,%d..%d,%d\n",
			  box.x1, box.y1, box.x2, box.y2);
		return -EINVAL;
	}

	if (IS_I965G(dev)) {
		BEGIN_LP_RING(4);
		OUT_RING(GFX_OP_DRAWRECT_INFO_I965);
		OUT_RING((box.x1 & 0xffff) | (box.y1 << 16));
		OUT_RING(((box.x2 - 1) & 0xffff) | ((box.y2 - 1) << 16));
		OUT_RING(DR4);
		ADVANCE_LP_RING();
	} else {
		BEGIN_LP_RING(6);
		OUT_RING(GFX_OP_DRAWRECT_INFO);
		OUT_RING(DR1);
		OUT_RING((box.x1 & 0xffff) | (box.y1 << 16));
		OUT_RING(((box.x2 - 1) & 0xffff) | ((box.y2 - 1) << 16));
		OUT_RING(DR4);
		OUT_RING(0);
		ADVANCE_LP_RING();
	}

	return 0;
}

/* XXX: Emitting the counter should really be moved to part of the IRQ
 * emit. For now, do it in both places:
 */

void i915_emit_breadcrumb(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	RING_LOCALS;

	if (++dev_priv->counter > BREADCRUMB_MASK) {
		 dev_priv->counter = 1;
		 DRM_DEBUG("Breadcrumb counter wrapped around\n");
	}

	if (dev_priv->sarea_priv)
		dev_priv->sarea_priv->last_enqueue = dev_priv->counter;

	BEGIN_LP_RING(4);
	OUT_RING(MI_STORE_DWORD_INDEX);
	OUT_RING(20);
	OUT_RING(dev_priv->counter);
	OUT_RING(0);
	ADVANCE_LP_RING();
}


int i915_emit_mi_flush(struct drm_device *dev, uint32_t flush)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	uint32_t flush_cmd = MI_FLUSH;
	RING_LOCALS;

	flush_cmd |= flush;

	i915_kernel_lost_context(dev);

	BEGIN_LP_RING(4);
	OUT_RING(flush_cmd);
	OUT_RING(0);
	OUT_RING(0);
	OUT_RING(0);
	ADVANCE_LP_RING();

	return 0;
}


static int i915_dispatch_cmdbuffer(struct drm_device * dev,
				   drm_i915_cmdbuffer_t * cmd)
{
#ifdef I915_HAVE_FENCE
	drm_i915_private_t *dev_priv = dev->dev_private;
#endif
	int nbox = cmd->num_cliprects;
	int i = 0, count, ret;

	if (cmd->sz & 0x3) {
		DRM_ERROR("alignment\n");
		return -EINVAL;
	}

	i915_kernel_lost_context(dev);

	count = nbox ? nbox : 1;

	for (i = 0; i < count; i++) {
		if (i < nbox) {
			ret = i915_emit_box(dev, cmd->cliprects, i,
					    cmd->DR1, cmd->DR4);
			if (ret)
				return ret;
		}

		ret = i915_emit_cmds(dev, (int __user *)cmd->buf, cmd->sz / 4);
		if (ret)
			return ret;
	}

	i915_emit_breadcrumb(dev);
#ifdef I915_HAVE_FENCE
	if (unlikely((dev_priv->counter & 0xFF) == 0))
		drm_fence_flush_old(dev, 0, dev_priv->counter);
#endif
	return 0;
}

int i915_dispatch_batchbuffer(struct drm_device * dev,
			      drm_i915_batchbuffer_t * batch)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_clip_rect __user *boxes = batch->cliprects;
	int nbox = batch->num_cliprects;
	int i = 0, count;
	RING_LOCALS;

	if ((batch->start | batch->used) & 0x7) {
		DRM_ERROR("alignment\n");
		return -EINVAL;
	}

	i915_kernel_lost_context(dev);

	count = nbox ? nbox : 1;

	for (i = 0; i < count; i++) {
		if (i < nbox) {
			int ret = i915_emit_box(dev, boxes, i,
						batch->DR1, batch->DR4);
			if (ret)
				return ret;
		}

		if (dev_priv->use_mi_batchbuffer_start) {
			BEGIN_LP_RING(2);
			if (IS_I965G(dev)) {
				OUT_RING(MI_BATCH_BUFFER_START | (2 << 6) | MI_BATCH_NON_SECURE_I965);
				OUT_RING(batch->start);
			} else {
				OUT_RING(MI_BATCH_BUFFER_START | (2 << 6));
				OUT_RING(batch->start | MI_BATCH_NON_SECURE);
			}
			ADVANCE_LP_RING();

		} else {
			BEGIN_LP_RING(4);
			OUT_RING(MI_BATCH_BUFFER);
			OUT_RING(batch->start | MI_BATCH_NON_SECURE);
			OUT_RING(batch->start + batch->used - 4);
			OUT_RING(0);
			ADVANCE_LP_RING();
		}
	}

	i915_emit_breadcrumb(dev);
#ifdef I915_HAVE_FENCE
	if (unlikely((dev_priv->counter & 0xFF) == 0))
		drm_fence_flush_old(dev, 0, dev_priv->counter);
#endif
	return 0;
}

static void i915_do_dispatch_flip(struct drm_device * dev, int plane, int sync)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	u32 num_pages, current_page, next_page, dspbase;
	int shift = 2 * plane, x, y;
	RING_LOCALS;

	/* Calculate display base offset */
	num_pages = dev_priv->sarea_priv->third_handle ? 3 : 2;
	current_page = (dev_priv->sarea_priv->pf_current_page >> shift) & 0x3;
	next_page = (current_page + 1) % num_pages;

	switch (next_page) {
	default:
	case 0:
		dspbase = dev_priv->sarea_priv->front_offset;
		break;
	case 1:
		dspbase = dev_priv->sarea_priv->back_offset;
		break;
	case 2:
		dspbase = dev_priv->sarea_priv->third_offset;
		break;
	}

	if (plane == 0) {
		x = dev_priv->sarea_priv->planeA_x;
		y = dev_priv->sarea_priv->planeA_y;
	} else {
		x = dev_priv->sarea_priv->planeB_x;
		y = dev_priv->sarea_priv->planeB_y;
	}

	dspbase += (y * dev_priv->sarea_priv->pitch + x) * dev_priv->cpp;

	DRM_DEBUG("plane=%d current_page=%d dspbase=0x%x\n", plane, current_page,
		  dspbase);

	BEGIN_LP_RING(4);
	OUT_RING(sync ? 0 :
		 (MI_WAIT_FOR_EVENT | (plane ? MI_WAIT_FOR_PLANE_B_FLIP :
				       MI_WAIT_FOR_PLANE_A_FLIP)));
	OUT_RING(CMD_OP_DISPLAYBUFFER_INFO | (sync ? 0 : ASYNC_FLIP) |
		 (plane ? DISPLAY_PLANE_B : DISPLAY_PLANE_A));
	OUT_RING(dev_priv->sarea_priv->pitch * dev_priv->cpp);
	OUT_RING(dspbase);
	ADVANCE_LP_RING();

	dev_priv->sarea_priv->pf_current_page &= ~(0x3 << shift);
	dev_priv->sarea_priv->pf_current_page |= next_page << shift;
}

void i915_dispatch_flip(struct drm_device * dev, int planes, int sync)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int i;

	DRM_DEBUG("planes=0x%x pfCurrentPage=%d\n",
		  planes, dev_priv->sarea_priv->pf_current_page);

	i915_emit_mi_flush(dev, MI_READ_FLUSH | MI_EXE_FLUSH);

	for (i = 0; i < 2; i++)
		if (planes & (1 << i))
			i915_do_dispatch_flip(dev, i, sync);

	i915_emit_breadcrumb(dev);
#ifdef I915_HAVE_FENCE
	if (unlikely(!sync && ((dev_priv->counter & 0xFF) == 0)))
		drm_fence_flush_old(dev, 0, dev_priv->counter);
#endif
}

int i915_quiescent(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	i915_kernel_lost_context(dev);
	return i915_wait_ring(dev, dev_priv->ring.Size - 8, __FUNCTION__);
}

static int i915_flush_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *file_priv)
{

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	return i915_quiescent(dev);
}

static int i915_batchbuffer(struct drm_device *dev, void *data,
			    struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	drm_i915_sarea_t *sarea_priv = (drm_i915_sarea_t *)
	    dev_priv->sarea_priv;
	drm_i915_batchbuffer_t *batch = data;
	size_t cliplen;
	int ret;

	if (!dev_priv->allow_batchbuffer) {
		DRM_ERROR("Batchbuffer ioctl disabled\n");
		return -EINVAL;
	}

	DRM_DEBUG("i915 batchbuffer, start %x used %d cliprects %d\n",
		  batch->start, batch->used, batch->num_cliprects);

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	DRM_UNLOCK();
	cliplen = batch->num_cliprects * sizeof(drm_clip_rect_t);
	if (batch->num_cliprects && DRM_VERIFYAREA_READ(batch->cliprects,
	    cliplen)) {
		DRM_LOCK();
		return -EFAULT;
	}
	if (batch->num_cliprects) {
		ret = vslock(batch->cliprects, cliplen);
		if (ret) {
			DRM_ERROR("Fault wiring cliprects\n");
			DRM_LOCK();
			return -EFAULT;
		}
	}
	DRM_LOCK();
	ret = i915_dispatch_batchbuffer(dev, batch);

	sarea_priv->last_dispatch = READ_BREADCRUMB(dev_priv);
	DRM_UNLOCK();
	if (batch->num_cliprects)
	    vsunlock(batch->cliprects, cliplen);
	DRM_LOCK();
	return ret;
}

static int i915_cmdbuffer(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	drm_i915_sarea_t *sarea_priv = (drm_i915_sarea_t *)
	    dev_priv->sarea_priv;
	drm_i915_cmdbuffer_t *cmdbuf = data;
	size_t cliplen;
	int ret;

	DRM_DEBUG("i915 cmdbuffer, buf %p sz %d cliprects %d\n",
		  cmdbuf->buf, cmdbuf->sz, cmdbuf->num_cliprects);

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	DRM_UNLOCK();
	cliplen = cmdbuf->num_cliprects * sizeof(drm_clip_rect_t);
	if (cmdbuf->num_cliprects &&
	    DRM_VERIFYAREA_READ(cmdbuf->cliprects, cliplen)) {
		DRM_ERROR("Fault accessing cliprects\n");
		DRM_LOCK();
		return -EFAULT;
	}
	if (cmdbuf->num_cliprects) {
		ret = vslock(cmdbuf->cliprects, cliplen);
		if (ret) {
			DRM_ERROR("Fault wiring cliprects\n");
			DRM_LOCK();
			return -EFAULT;
		}
		ret = vslock(cmdbuf->buf, cmdbuf->sz);
		if (ret) {
			vsunlock(cmdbuf->cliprects, cliplen);
			DRM_ERROR("Fault wiring cmds\n");
			DRM_LOCK();
			return -EFAULT;
		}
	}
	DRM_LOCK();
	ret = i915_dispatch_cmdbuffer(dev, cmdbuf);
	if (ret == 0)
		sarea_priv->last_dispatch = READ_BREADCRUMB(dev_priv);
	else
		DRM_ERROR("i915_dispatch_cmdbuffer failed\n");
	DRM_UNLOCK();
	if (cmdbuf->num_cliprects) {
		vsunlock(cmdbuf->buf, cmdbuf->sz);
		vsunlock(cmdbuf->cliprects, cliplen);
	}
	DRM_LOCK();
	return (ret);
}

#if defined(DRM_DEBUG_CODE)
#define DRM_DEBUG_RELOCATION	(drm_debug != 0)
#else
#define DRM_DEBUG_RELOCATION	0
#endif

static int i915_do_cleanup_pageflip(struct drm_device * dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	int i, planes, num_pages = dev_priv->sarea_priv->third_handle ? 3 : 2;

	DRM_DEBUG("\n");

	for (i = 0, planes = 0; i < 2; i++)
		if (dev_priv->sarea_priv->pf_current_page & (0x3 << (2 * i))) {
			dev_priv->sarea_priv->pf_current_page =
				(dev_priv->sarea_priv->pf_current_page &
				 ~(0x3 << (2 * i))) | ((num_pages - 1) << (2 * i));

			planes |= 1 << i;
		}

	if (planes)
		i915_dispatch_flip(dev, planes, 0);

	return 0;
}

static int i915_flip_bufs(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_i915_flip_t *param = data;

	DRM_DEBUG("\n");

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	/* This is really planes */
	if (param->pipes & ~0x3) {
		DRM_ERROR("Invalid planes 0x%x, only <= 0x3 is valid\n",
			  param->pipes);
		return -EINVAL;
	}

	i915_dispatch_flip(dev, param->pipes, 0);

	return 0;
}


static int i915_getparam(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_getparam_t *param = data;
	int value;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return -EINVAL;
	}

	switch (param->param) {
	case I915_PARAM_IRQ_ACTIVE:
		value = dev->irq ? 1 : 0;
		break;
	case I915_PARAM_ALLOW_BATCHBUFFER:
		value = dev_priv->allow_batchbuffer ? 1 : 0;
		break;
	case I915_PARAM_LAST_DISPATCH:
		value = READ_BREADCRUMB(dev_priv);
		break;
	case I915_PARAM_CHIPSET_ID:
		value = dev->pci_device;
		break;
	default:
		DRM_ERROR("Unknown parameter %d\n", param->param);
		return -EINVAL;
	}

	if (DRM_COPY_TO_USER(param->value, &value, sizeof(int))) {
		DRM_ERROR("DRM_COPY_TO_USER failed\n");
		return -EFAULT;
	}

	return 0;
}

static int i915_setparam(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_setparam_t *param = data;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return -EINVAL;
	}

	switch (param->param) {
	case I915_SETPARAM_USE_MI_BATCHBUFFER_START:
		if (!IS_I965G(dev))
			dev_priv->use_mi_batchbuffer_start = param->value;
		break;
	case I915_SETPARAM_TEX_LRU_LOG_GRANULARITY:
		dev_priv->tex_lru_log_granularity = param->value;
		break;
	case I915_SETPARAM_ALLOW_BATCHBUFFER:
		dev_priv->allow_batchbuffer = param->value;
		break;
	default:
		DRM_ERROR("unknown parameter %d\n", param->param);
		return -EINVAL;
	}

	return 0;
}

drm_i915_mmio_entry_t mmio_table[] = {
	[MMIO_REGS_PS_DEPTH_COUNT] = {
		I915_MMIO_MAY_READ|I915_MMIO_MAY_WRITE,
		0x2350,
		8
	}
};

static int mmio_table_size = sizeof(mmio_table)/sizeof(drm_i915_mmio_entry_t);

static int i915_mmio(struct drm_device *dev, void *data,
		     struct drm_file *file_priv)
{
	uint32_t buf[8];
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_mmio_entry_t *e;
	drm_i915_mmio_t *mmio = data;
	void __iomem *base;
	int i;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return -EINVAL;
	}

	if (mmio->reg >= mmio_table_size)
		return -EINVAL;

	e = &mmio_table[mmio->reg];
	base = (u8 *) dev_priv->mmio_map->handle + e->offset;

	switch (mmio->read_write) {
	case I915_MMIO_READ:
		if (!(e->flag & I915_MMIO_MAY_READ))
			return -EINVAL;
		for (i = 0; i < e->size / 4; i++)
			buf[i] = I915_READ(e->offset + i * 4);
		if (DRM_COPY_TO_USER(mmio->data, buf, e->size)) {
			DRM_ERROR("DRM_COPY_TO_USER failed\n");
			return -EFAULT;
		}
		break;
		
	case I915_MMIO_WRITE:
		if (!(e->flag & I915_MMIO_MAY_WRITE))
			return -EINVAL;
		if (DRM_COPY_FROM_USER(buf, mmio->data, e->size)) {
			DRM_ERROR("DRM_COPY_TO_USER failed\n");
			return -EFAULT;
		}
		for (i = 0; i < e->size / 4; i++)
			I915_WRITE(e->offset + i * 4, buf[i]);
		break;
	}
	return 0;
}

static int i915_set_status_page(struct drm_device *dev, void *data,
				struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_hws_addr_t *hws = data;

	if (!I915_NEED_GFX_HWS(dev))
		return -EINVAL;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return -EINVAL;
	}
	DRM_DEBUG("set status page addr 0x%08x\n", (u32)hws->addr);

	dev_priv->status_gfx_addr = hws->addr & (0x1ffff<<12);

	dev_priv->hws_map.offset = dev->agp->base + hws->addr;
	dev_priv->hws_map.size = 4*1024;
	dev_priv->hws_map.type = 0;
	dev_priv->hws_map.flags = 0;
	dev_priv->hws_map.mtrr = 0;

	drm_core_ioremap(&dev_priv->hws_map, dev);
	if (dev_priv->hws_map.handle == NULL) {
		i915_dma_cleanup(dev);
		dev_priv->status_gfx_addr = 0;
		DRM_ERROR("can not ioremap virtual address for"
				" G33 hw status page\n");
		return -ENOMEM;
	}
	dev_priv->hw_status_page = dev_priv->hws_map.handle;

	memset(dev_priv->hw_status_page, 0, PAGE_SIZE);
	I915_WRITE(HWS_PGA, dev_priv->status_gfx_addr);
	DRM_DEBUG("load hws 0x2080 with gfx mem 0x%x\n",
			dev_priv->status_gfx_addr);
	DRM_DEBUG("load hws at %p\n", dev_priv->hw_status_page);
	return 0;
}

int i915_driver_load(struct drm_device *dev, unsigned long flags)
{
	struct drm_i915_private *dev_priv;
	unsigned long base, size;
	int ret = 0, mmio_bar = IS_I9XX(dev) ? 0 : 1;

	/* i915 has 4 more counters */
	dev->counters += 4;
	dev->types[6] = _DRM_STAT_IRQ;
	dev->types[7] = _DRM_STAT_PRIMARY;
	dev->types[8] = _DRM_STAT_SECONDARY;
	dev->types[9] = _DRM_STAT_DMA;

	dev_priv = drm_alloc(sizeof(drm_i915_private_t), DRM_MEM_DRIVER);
	if (dev_priv == NULL)
		return -ENOMEM;

	memset(dev_priv, 0, sizeof(drm_i915_private_t));

	dev->dev_private = (void *)dev_priv;

	/* Add register map (needed for suspend/resume) */
	base = drm_get_resource_start(dev, mmio_bar);
	size = drm_get_resource_len(dev, mmio_bar);

	ret = drm_addmap(dev, base, size, _DRM_REGISTERS,
		_DRM_KERNEL | _DRM_DRIVER, &dev_priv->mmio_map);

	DRM_SPININIT(&dev_priv->swaps_lock, "swap");
	DRM_SPININIT(&dev_priv->user_irq_lock, "userirq");

#ifdef __linux__
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
	intel_init_chipset_flush_compat(dev);
#endif
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,25)
	intel_opregion_init(dev);
#endif
#endif

	return ret;
}

int i915_driver_unload(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	if (dev_priv->mmio_map)
		drm_rmmap(dev, dev_priv->mmio_map);

	DRM_SPINUNINIT(&dev_priv->swaps_lock);
	DRM_SPINUNINIT(&dev_priv->user_irq_lock);

#ifdef __linux__
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,25)
	intel_opregion_free(dev);
#endif
#endif

	drm_free(dev->dev_private, sizeof(drm_i915_private_t),
		 DRM_MEM_DRIVER);
	dev->dev_private = NULL;

#ifdef __linux__
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
	intel_fini_chipset_flush_compat(dev);
#endif
#endif
	return 0;
}

void i915_driver_lastclose(struct drm_device * dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	/* agp off can use this to get called before dev_priv */
	if (!dev_priv)
		return;

#ifdef I915_HAVE_BUFFER
	if (dev_priv->val_bufs) {
		vfree(dev_priv->val_bufs);
		dev_priv->val_bufs = NULL;
	}
#endif

	if (drm_getsarea(dev) && dev_priv->sarea_priv) {
		i915_do_cleanup_pageflip(dev);
		dev_priv->sarea_priv = NULL;
		dev_priv->sarea = NULL;
	}
	if (dev_priv->agp_heap)
		i915_mem_takedown(&(dev_priv->agp_heap));
#if defined(I915_HAVE_BUFFER)
	if (dev_priv->sarea_kmap.virtual) {
		drm_bo_kunmap(&dev_priv->sarea_kmap);
		dev_priv->sarea_kmap.virtual = NULL;
		dev->lock.hw_lock = NULL;
		dev->sigdata.lock = NULL;
	}

	if (dev_priv->sarea_bo) {
		mutex_lock(&dev->struct_mutex);
		drm_bo_usage_deref_locked(&dev_priv->sarea_bo);
		mutex_unlock(&dev->struct_mutex);
		dev_priv->sarea_bo = NULL;
	}
#endif
	i915_dma_cleanup(dev);
}

void i915_driver_preclose(struct drm_device * dev, struct drm_file *file_priv)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	i915_mem_release(dev, file_priv, dev_priv->agp_heap);
}

struct drm_ioctl_desc i915_ioctls[] = {
	DRM_IOCTL_DEF(DRM_I915_INIT, i915_dma_init, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_I915_FLUSH, i915_flush_ioctl, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_FLIP, i915_flip_bufs, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_BATCHBUFFER, i915_batchbuffer, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_IRQ_EMIT, i915_irq_emit, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_IRQ_WAIT, i915_irq_wait, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_GETPARAM, i915_getparam, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_SETPARAM, i915_setparam, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_I915_ALLOC, i915_mem_alloc, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_FREE, i915_mem_free, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_INIT_HEAP, i915_mem_init_heap, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_I915_CMDBUFFER, i915_cmdbuffer, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_DESTROY_HEAP,  i915_mem_destroy_heap, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY ),
	DRM_IOCTL_DEF(DRM_I915_SET_VBLANK_PIPE,  i915_vblank_pipe_set, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY ),
	DRM_IOCTL_DEF(DRM_I915_GET_VBLANK_PIPE,  i915_vblank_pipe_get, DRM_AUTH ),
	DRM_IOCTL_DEF(DRM_I915_VBLANK_SWAP, i915_vblank_swap, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_MMIO, i915_mmio, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_HWS_ADDR, i915_set_status_page, DRM_AUTH),
#ifdef I915_HAVE_BUFFER
	DRM_IOCTL_DEF(DRM_I915_EXECBUFFER, i915_execbuffer, DRM_AUTH),
#endif
};

int i915_max_ioctl = DRM_ARRAY_SIZE(i915_ioctls);

/**
 * Determine if the device really is AGP or not.
 *
 * All Intel graphics chipsets are treated as AGP, even if they are really
 * PCI-e.
 *
 * \param dev   The device to be tested.
 *
 * \returns
 * A value of 1 is always retured to indictate every i9x5 is AGP.
 */
int i915_driver_device_is_agp(struct drm_device * dev)
{
	return 1;
}

int i915_driver_firstopen(struct drm_device *dev)
{
#ifdef I915_HAVE_BUFFER
	drm_bo_driver_init(dev);
#endif
	return 0;
}
