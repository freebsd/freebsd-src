/* drm_context.h -- IOCTLs for generic contexts -*- linux-c -*-
 * Created: Fri Nov 24 18:31:37 2000 by gareth@valinux.com
 *
 * Copyright 1999, 2000 Precision Insight, Inc., Cedar Park, Texas.
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
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *
 * $FreeBSD$
 */

#include "dev/drm/drmP.h"

#if !__HAVE_CTX_BITMAP
#error "__HAVE_CTX_BITMAP must be defined"
#endif

/* ================================================================
 * Context bitmap support
 */

void DRM(ctxbitmap_free)( drm_device_t *dev, int ctx_handle )
{
	if ( ctx_handle < 0 ) goto failed;
	if ( !dev->ctx_bitmap ) goto failed;

	if ( ctx_handle < DRM_MAX_CTXBITMAP ) {
		DRM_LOCK;
		clear_bit( ctx_handle, dev->ctx_bitmap );
		dev->context_sareas[ctx_handle] = NULL;
		DRM_UNLOCK;
		return;
	}
failed:
       	DRM_ERROR( "Attempt to free invalid context handle: %d\n",
		   ctx_handle );
       	return;
}

int DRM(ctxbitmap_next)( drm_device_t *dev )
{
	int bit;

	if(!dev->ctx_bitmap) return -1;

	DRM_LOCK;
	bit = find_first_zero_bit( dev->ctx_bitmap, DRM_MAX_CTXBITMAP );
	if ( bit < DRM_MAX_CTXBITMAP ) {
		set_bit( bit, dev->ctx_bitmap );
	   	DRM_DEBUG( "drm_ctxbitmap_next bit : %d\n", bit );
		if((bit+1) > dev->max_context) {
			dev->max_context = (bit+1);
			if(dev->context_sareas) {
				drm_local_map_t **ctx_sareas;

				ctx_sareas = DRM(realloc)(dev->context_sareas,
						(dev->max_context - 1) * 
						sizeof(*dev->context_sareas),
						dev->max_context * 
						sizeof(*dev->context_sareas),
						DRM_MEM_MAPS);
				if(!ctx_sareas) {
					clear_bit(bit, dev->ctx_bitmap);
					DRM_UNLOCK;
					return -1;
				}
				dev->context_sareas = ctx_sareas;
				dev->context_sareas[bit] = NULL;
			} else {
				/* max_context == 1 at this point */
				dev->context_sareas = DRM(alloc)(
						dev->max_context * 
						sizeof(*dev->context_sareas),
						DRM_MEM_MAPS);
				if(!dev->context_sareas) {
					clear_bit(bit, dev->ctx_bitmap);
					DRM_UNLOCK;
					return -1;
				}
				dev->context_sareas[bit] = NULL;
			}
		}
		DRM_UNLOCK;
		return bit;
	}
	DRM_UNLOCK;
	return -1;
}

int DRM(ctxbitmap_init)( drm_device_t *dev )
{
	int i;
   	int temp;

	DRM_LOCK;
	dev->ctx_bitmap = (atomic_t *) DRM(alloc)( PAGE_SIZE,
							DRM_MEM_CTXBITMAP );
	if ( dev->ctx_bitmap == NULL ) {
		DRM_UNLOCK;
		return DRM_ERR(ENOMEM);
	}
	memset( (void *)dev->ctx_bitmap, 0, PAGE_SIZE );
	dev->context_sareas = NULL;
	dev->max_context = -1;
	DRM_UNLOCK;

	for ( i = 0 ; i < DRM_RESERVED_CONTEXTS ; i++ ) {
		temp = DRM(ctxbitmap_next)( dev );
	   	DRM_DEBUG( "drm_ctxbitmap_init : %d\n", temp );
	}

	return 0;
}

void DRM(ctxbitmap_cleanup)( drm_device_t *dev )
{
	DRM_LOCK;
	if( dev->context_sareas ) DRM(free)( dev->context_sareas,
					     sizeof(*dev->context_sareas) * 
					     dev->max_context,
					     DRM_MEM_MAPS );
	DRM(free)( (void *)dev->ctx_bitmap, PAGE_SIZE, DRM_MEM_CTXBITMAP );
	DRM_UNLOCK;
}

/* ================================================================
 * Per Context SAREA Support
 */

int DRM(getsareactx)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_ctx_priv_map_t request;
	drm_local_map_t *map;

	DRM_COPY_FROM_USER_IOCTL( request, (drm_ctx_priv_map_t *)data, 
			   sizeof(request) );

	DRM_LOCK;
	if (dev->max_context < 0 || request.ctx_id >= (unsigned) dev->max_context) {
		DRM_UNLOCK;
		return DRM_ERR(EINVAL);
	}

	map = dev->context_sareas[request.ctx_id];
	DRM_UNLOCK;

	request.handle = map->handle;

	DRM_COPY_TO_USER_IOCTL( (drm_ctx_priv_map_t *)data, request, sizeof(request) );

	return 0;
}

int DRM(setsareactx)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_ctx_priv_map_t request;
	drm_local_map_t *map = NULL;
	drm_map_list_entry_t *list;

	DRM_COPY_FROM_USER_IOCTL( request, (drm_ctx_priv_map_t *)data,
			   sizeof(request) );

	DRM_LOCK;
	TAILQ_FOREACH(list, dev->maplist, link) {
		map=list->map;
		if(map->handle == request.handle) {
			if (dev->max_context < 0)
				goto bad;
			if (request.ctx_id >= (unsigned) dev->max_context)
				goto bad;
			dev->context_sareas[request.ctx_id] = map;
			DRM_UNLOCK;
			return 0;
		}
	}

bad:
	DRM_UNLOCK;
	return DRM_ERR(EINVAL);
}

/* ================================================================
 * The actual DRM context handling routines
 */

int DRM(context_switch)( drm_device_t *dev, int old, int new )
{
        if ( test_and_set_bit( 0, &dev->context_flag ) ) {
                DRM_ERROR( "Reentering -- FIXME\n" );
                return DRM_ERR(EBUSY);
        }

        DRM_DEBUG( "Context switch from %d to %d\n", old, new );

        if ( new == dev->last_context ) {
                clear_bit( 0, &dev->context_flag );
                return 0;
        }

        return 0;
}

int DRM(context_switch_complete)( drm_device_t *dev, int new )
{
        dev->last_context = new;  /* PRE/POST: This is the _only_ writer. */

        if ( !_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock) ) {
                DRM_ERROR( "Lock isn't held after context switch\n" );
        }

				/* If a context switch is ever initiated
                                   when the kernel holds the lock, release
                                   that lock here. */
        clear_bit( 0, &dev->context_flag );

        return 0;
}

int DRM(resctx)( DRM_IOCTL_ARGS )
{
	drm_ctx_res_t res;
	drm_ctx_t ctx;
	int i;

	DRM_COPY_FROM_USER_IOCTL( res, (drm_ctx_res_t *)data, sizeof(res) );

	if ( res.count >= DRM_RESERVED_CONTEXTS ) {
		memset( &ctx, 0, sizeof(ctx) );
		for ( i = 0 ; i < DRM_RESERVED_CONTEXTS ; i++ ) {
			ctx.handle = i;
			if ( DRM_COPY_TO_USER( &res.contexts[i],
					   &i, sizeof(i) ) )
				return DRM_ERR(EFAULT);
		}
	}
	res.count = DRM_RESERVED_CONTEXTS;

	DRM_COPY_TO_USER_IOCTL( (drm_ctx_res_t *)data, res, sizeof(res) );

	return 0;
}

int DRM(addctx)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_ctx_t ctx;

	DRM_COPY_FROM_USER_IOCTL( ctx, (drm_ctx_t *)data, sizeof(ctx) );

	ctx.handle = DRM(ctxbitmap_next)( dev );
	if ( ctx.handle == DRM_KERNEL_CONTEXT ) {
				/* Skip kernel's context and get a new one. */
		ctx.handle = DRM(ctxbitmap_next)( dev );
	}
	DRM_DEBUG( "%d\n", ctx.handle );
	if ( ctx.handle == -1 ) {
		DRM_DEBUG( "Not enough free contexts.\n" );
				/* Should this return -EBUSY instead? */
		return DRM_ERR(ENOMEM);
	}

#ifdef DRIVER_CTX_CTOR
	if ( ctx.handle != DRM_KERNEL_CONTEXT )
		DRIVER_CTX_CTOR(ctx.handle); /* XXX: also pass dev ? */
#endif

	DRM_COPY_TO_USER_IOCTL( (drm_ctx_t *)data, ctx, sizeof(ctx) );

	return 0;
}

int DRM(modctx)( DRM_IOCTL_ARGS )
{
	/* This does nothing */
	return 0;
}

int DRM(getctx)( DRM_IOCTL_ARGS )
{
	drm_ctx_t ctx;

	DRM_COPY_FROM_USER_IOCTL( ctx, (drm_ctx_t *)data, sizeof(ctx) );

	/* This is 0, because we don't handle any context flags */
	ctx.flags = 0;

	DRM_COPY_TO_USER_IOCTL( (drm_ctx_t *)data, ctx, sizeof(ctx) );

	return 0;
}

int DRM(switchctx)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_ctx_t ctx;

	DRM_COPY_FROM_USER_IOCTL( ctx, (drm_ctx_t *)data, sizeof(ctx) );

	DRM_DEBUG( "%d\n", ctx.handle );
	return DRM(context_switch)( dev, dev->last_context, ctx.handle );
}

int DRM(newctx)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_ctx_t ctx;

	DRM_COPY_FROM_USER_IOCTL( ctx, (drm_ctx_t *)data, sizeof(ctx) );

	DRM_DEBUG( "%d\n", ctx.handle );
	DRM(context_switch_complete)( dev, ctx.handle );

	return 0;
}

int DRM(rmctx)( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_ctx_t ctx;

	DRM_COPY_FROM_USER_IOCTL( ctx, (drm_ctx_t *)data, sizeof(ctx) );

	DRM_DEBUG( "%d\n", ctx.handle );
	if ( ctx.handle != DRM_KERNEL_CONTEXT ) {
#ifdef DRIVER_CTX_DTOR
		DRIVER_CTX_DTOR(ctx.handle); /* XXX: also pass dev ? */
#endif
		DRM(ctxbitmap_free)( dev, ctx.handle );
	}

	return 0;
}
