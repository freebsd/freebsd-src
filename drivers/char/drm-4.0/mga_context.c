/* mga_context.c -- IOCTLs for mga contexts -*- linux-c -*-
 * Created: Mon Dec 13 09:51:35 1999 by faith@precisioninsight.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 * 
 * Author: Rickard E. (Rik) Faith <faith@valinux.com>
 *	   Jeff Hartmann <jhartmann@valinux.com>
 *
 */

#define __NO_VERSION__
#include "drmP.h"
#include "mga_drv.h"

static int mga_alloc_queue(drm_device_t *dev)
{
   	return drm_ctxbitmap_next(dev);
}

int mga_context_switch(drm_device_t *dev, int old, int new)
{
        char        buf[64];

        atomic_inc(&dev->total_ctx);

        if (test_and_set_bit(0, &dev->context_flag)) {
                DRM_ERROR("Reentering -- FIXME\n");
                return -EBUSY;
        }

#if DRM_DMA_HISTOGRAM
        dev->ctx_start = get_cycles();
#endif
        
        DRM_DEBUG("Context switch from %d to %d\n", old, new);

        if (new == dev->last_context) {
                clear_bit(0, &dev->context_flag);
                return 0;
        }
        
        if (drm_flags & DRM_FLAG_NOCTX) {
                mga_context_switch_complete(dev, new);
        } else {
                sprintf(buf, "C %d %d\n", old, new);
                drm_write_string(dev, buf);
        }
        
        return 0;
}

int mga_context_switch_complete(drm_device_t *dev, int new)
{
        dev->last_context = new;  /* PRE/POST: This is the _only_ writer. */
        dev->last_switch  = jiffies;
        
        if (!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
                DRM_ERROR("Lock isn't held after context switch\n");
        }

				/* If a context switch is ever initiated
                                   when the kernel holds the lock, release
                                   that lock here. */
#if DRM_DMA_HISTOGRAM
        atomic_inc(&dev->histo.ctx[drm_histogram_slot(get_cycles()
                                                      - dev->ctx_start)]);
                   
#endif
        clear_bit(0, &dev->context_flag);
        wake_up(&dev->context_wait);
        
        return 0;
}

int mga_resctx(struct inode *inode, struct file *filp, unsigned int cmd,
	       unsigned long arg)
{
	drm_ctx_res_t	res;
	drm_ctx_t	ctx;
	int		i;

	if (copy_from_user(&res, (drm_ctx_res_t *)arg, sizeof(res)))
		return -EFAULT;
	if (res.count >= DRM_RESERVED_CONTEXTS) {
		memset(&ctx, 0, sizeof(ctx));
		for (i = 0; i < DRM_RESERVED_CONTEXTS; i++) {
			ctx.handle = i;
			if (copy_to_user(&res.contexts[i],
					 &i,
					 sizeof(i)))
				return -EFAULT;
		}
	}
	res.count = DRM_RESERVED_CONTEXTS;
	if (copy_to_user((drm_ctx_res_t *)arg, &res, sizeof(res)))
		return -EFAULT;
	return 0;
}

int mga_addctx(struct inode *inode, struct file *filp, unsigned int cmd,
	       unsigned long arg)
{
	drm_file_t	*priv	= filp->private_data;
	drm_device_t	*dev	= priv->dev;
	drm_ctx_t	ctx;

	if (copy_from_user(&ctx, (drm_ctx_t *)arg, sizeof(ctx)))
		return -EFAULT;
	if ((ctx.handle = mga_alloc_queue(dev)) == DRM_KERNEL_CONTEXT) {
				/* Skip kernel's context and get a new one. */
		ctx.handle = mga_alloc_queue(dev);
	}
        if (ctx.handle == -1) {
		return -ENOMEM;
	}
	DRM_DEBUG("%d\n", ctx.handle);
	if (copy_to_user((drm_ctx_t *)arg, &ctx, sizeof(ctx)))
		return -EFAULT;
	return 0;
}

int mga_modctx(struct inode *inode, struct file *filp, unsigned int cmd,
	unsigned long arg)
{
   	/* This does nothing for the mga */
	return 0;
}

int mga_getctx(struct inode *inode, struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	drm_ctx_t ctx;

	if (copy_from_user(&ctx, (drm_ctx_t*)arg, sizeof(ctx)))
		return -EFAULT;
	/* This is 0, because we don't hanlde any context flags */
	ctx.flags = 0;
	if (copy_to_user((drm_ctx_t*)arg, &ctx, sizeof(ctx)))
		return -EFAULT;
	return 0;
}

int mga_switchctx(struct inode *inode, struct file *filp, unsigned int cmd,
		   unsigned long arg)
{
	drm_file_t	*priv	= filp->private_data;
	drm_device_t	*dev	= priv->dev;
	drm_ctx_t	ctx;

	if (copy_from_user(&ctx, (drm_ctx_t *)arg, sizeof(ctx)))
		return -EFAULT;
	DRM_DEBUG("%d\n", ctx.handle);
	return mga_context_switch(dev, dev->last_context, ctx.handle);
}

int mga_newctx(struct inode *inode, struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	drm_file_t	*priv	= filp->private_data;
	drm_device_t	*dev	= priv->dev;
	drm_ctx_t	ctx;

	if (copy_from_user(&ctx, (drm_ctx_t *)arg, sizeof(ctx)))
		return -EFAULT;
	DRM_DEBUG("%d\n", ctx.handle);
	mga_context_switch_complete(dev, ctx.handle);

	return 0;
}

int mga_rmctx(struct inode *inode, struct file *filp, unsigned int cmd,
	      unsigned long arg)
{
	drm_file_t	*priv	= filp->private_data;
	drm_device_t	*dev	= priv->dev;
	drm_ctx_t	ctx;

	if (copy_from_user(&ctx, (drm_ctx_t *)arg, sizeof(ctx)))
		return -EFAULT;
	DRM_DEBUG("%d\n", ctx.handle);
	if(ctx.handle == DRM_KERNEL_CONTEXT+1) priv->remove_auth_on_close = 1;

      	if(ctx.handle != DRM_KERNEL_CONTEXT) {
	   	drm_ctxbitmap_free(dev, ctx.handle);
	}
	
	return 0;
}
