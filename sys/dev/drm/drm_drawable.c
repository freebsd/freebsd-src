/*-
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
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/** @file drm_drawable.c
 * This file implements ioctls to store information along with DRM drawables,
 * such as the current set of cliprects for vblank-synced buffer swaps.
 */

#include "dev/drm/drmP.h"

struct bsd_drm_drawable_info {
	struct drm_drawable_info info;
	int handle;
	RB_ENTRY(bsd_drm_drawable_info) tree;
};

static int
drm_drawable_compare(struct bsd_drm_drawable_info *a,
    struct bsd_drm_drawable_info *b)
{
	if (a->handle > b->handle)
		return 1;
	if (a->handle < b->handle)
		return -1;
	return 0;
}

RB_GENERATE_STATIC(drawable_tree, bsd_drm_drawable_info, tree,
    drm_drawable_compare);

struct drm_drawable_info *
drm_get_drawable_info(struct drm_device *dev, int handle)
{
	struct bsd_drm_drawable_info find, *result;

	find.handle = handle;
	result = RB_FIND(drawable_tree, &dev->drw_head, &find);

	return &result->info;
}

int drm_adddraw(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_draw *draw = data;
	struct bsd_drm_drawable_info *info;

	info = drm_calloc(1, sizeof(struct bsd_drm_drawable_info),
	    DRM_MEM_DRAWABLE);
	if (info == NULL)
		return ENOMEM;

	info->handle = alloc_unr(dev->drw_unrhdr);
	DRM_SPINLOCK(&dev->drw_lock);
	RB_INSERT(drawable_tree, &dev->drw_head, info);
	draw->handle = info->handle;
	DRM_SPINUNLOCK(&dev->drw_lock);

	DRM_DEBUG("%d\n", draw->handle);

	return 0;
}

int drm_rmdraw(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_draw *draw = (struct drm_draw *)data;
	struct drm_drawable_info *info;

	DRM_SPINLOCK(&dev->drw_lock);
	info = drm_get_drawable_info(dev, draw->handle);
	if (info != NULL) {
		RB_REMOVE(drawable_tree, &dev->drw_head,
		    (struct bsd_drm_drawable_info *)info);
		DRM_SPINUNLOCK(&dev->drw_lock);
		free_unr(dev->drw_unrhdr, draw->handle);
		drm_free(info, sizeof(struct bsd_drm_drawable_info),
		    DRM_MEM_DRAWABLE);
		return 0;
	} else {
		DRM_SPINUNLOCK(&dev->drw_lock);
		return EINVAL;
	}
}

int drm_update_draw(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct drm_drawable_info *info;
	struct drm_update_draw *update = (struct drm_update_draw *)data;
	int ret;

	info = drm_get_drawable_info(dev, update->handle);
	if (info == NULL)
		return EINVAL;

	switch (update->type) {
	case DRM_DRAWABLE_CLIPRECTS:
		DRM_SPINLOCK(&dev->drw_lock);
		if (update->num != info->num_rects) {
			drm_free(info->rects,
			    sizeof(*info->rects) * info->num_rects,
			    DRM_MEM_DRAWABLE);
			info->rects = NULL;
			info->num_rects = 0;
		}
		if (update->num == 0) {
			DRM_SPINUNLOCK(&dev->drw_lock);
			return 0;
		}
		if (info->rects == NULL) {
			info->rects = drm_alloc(sizeof(*info->rects) *
			    update->num, DRM_MEM_DRAWABLE);
			if (info->rects == NULL) {
				DRM_SPINUNLOCK(&dev->drw_lock);
				return ENOMEM;
			}
			info->num_rects = update->num;
		}
		/* For some reason the pointer arg is unsigned long long. */
		ret = copyin((void *)(intptr_t)update->data, info->rects,
		    sizeof(*info->rects) * info->num_rects);
		DRM_SPINUNLOCK(&dev->drw_lock);
		return ret;
	default:
		return EINVAL;
	}
}

void drm_drawable_free_all(struct drm_device *dev)
{
	struct bsd_drm_drawable_info *info, *next;

	DRM_SPINLOCK(&dev->drw_lock);
	for (info = RB_MIN(drawable_tree, &dev->drw_head);
	    info != NULL ; info = next) {
		next = RB_NEXT(drawable_tree, &dev->drw_head, info);
		RB_REMOVE(drawable_tree, &dev->drw_head,
		    (struct bsd_drm_drawable_info *)info);
		DRM_SPINUNLOCK(&dev->drw_lock);
		free_unr(dev->drw_unrhdr, info->handle);
		drm_free(info, sizeof(struct bsd_drm_drawable_info),
		    DRM_MEM_DRAWABLE);
		DRM_SPINLOCK(&dev->drw_lock);
	}
	DRM_SPINUNLOCK(&dev->drw_lock);
}
