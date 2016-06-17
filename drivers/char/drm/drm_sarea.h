/* sarea.h -- SAREA definitions -*- linux-c -*-
 *
 * Copyright 2002 Tungsten Graphics, Inc., Cedar Park, Texas.
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
 * TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Michel Dänzer <michel@daenzer.net>
 */

#ifndef _DRM_SAREA_H_
#define _DRM_SAREA_H_

#define SAREA_MAX_DRAWABLES 		256

typedef struct _drm_sarea_drawable_t {
    unsigned int	stamp;
    unsigned int	flags;
} drm_sarea_drawable_t;

typedef struct _dri_sarea_frame_t {
    unsigned int        x;
    unsigned int        y;
    unsigned int        width;
    unsigned int        height;
    unsigned int        fullscreen;
} drm_sarea_frame_t;

typedef struct _drm_sarea_t {
    /* first thing is always the drm locking structure */
    drm_hw_lock_t		lock;
		/* NOT_DONE: Use readers/writer lock for drawable_lock */
    drm_hw_lock_t		drawable_lock;
    drm_sarea_drawable_t	drawableTable[SAREA_MAX_DRAWABLES];
    drm_sarea_frame_t		frame;
    drm_context_t		dummy_context;
} drm_sarea_t;

#endif	/* _DRM_SAREA_H_ */
