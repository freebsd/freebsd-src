/* sis_drv.h -- Private header for sis driver -*- linux-c -*-
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All rights reserved.
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
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/linux/drm/kernel/sis.h,v 1.2 2001/12/19 21:25:59 dawes Exp $ */

#ifndef __SIS_H__
#define __SIS_H__

/* This remains constant for all DRM template files.
 * Name it sisdrv_##x as there's a conflict with sis_free/malloc in the kernel
 * that's used for fb devices 
 */
#define DRM(x) sisdrv_##x

/* General customization:
 */
#define __HAVE_AGP		1
#define __MUST_HAVE_AGP		0
#define __HAVE_MTRR		1
#define __HAVE_CTX_BITMAP	1

#define DRIVER_AUTHOR	 "SIS"
#define DRIVER_NAME	 "sis"
#define DRIVER_DESC	 "SIS 300/630/540"
#define DRIVER_DATE	 "20010503"
#define DRIVER_MAJOR	 1
#define DRIVER_MINOR	 0
#define DRIVER_PATCHLEVEL  0

#define DRIVER_IOCTLS \
        [DRM_IOCTL_NR(SIS_IOCTL_FB_ALLOC)]   = { sis_fb_alloc,	  1, 0 }, \
        [DRM_IOCTL_NR(SIS_IOCTL_FB_FREE)]    = { sis_fb_free,	  1, 0 }, \
        /* AGP Memory Management */					  \
        [DRM_IOCTL_NR(SIS_IOCTL_AGP_INIT)]   = { sisp_agp_init,	  1, 0 }, \
        [DRM_IOCTL_NR(SIS_IOCTL_AGP_ALLOC)]  = { sisp_agp_alloc,  1, 0 }, \
        [DRM_IOCTL_NR(SIS_IOCTL_AGP_FREE)]   = { sisp_agp_free,	  1, 0 }
#if 0 /* these don't appear to be defined */
	/* SIS Stereo */						 
	[DRM_IOCTL_NR(DRM_IOCTL_CONTROL)]    = { sis_control,	  1, 1 }, 
        [DRM_IOCTL_NR(SIS_IOCTL_FLIP)]       = { sis_flip,	  1, 1 }, 
        [DRM_IOCTL_NR(SIS_IOCTL_FLIP_INIT)]  = { sis_flip_init,	  1, 1 }, 
        [DRM_IOCTL_NR(SIS_IOCTL_FLIP_FINAL)] = { sis_flip_final,  1, 1 }
#endif

#define __HAVE_COUNTERS		5

/* Buffer customization:
 */
#define DRIVER_AGP_BUFFERS_MAP( dev )					\
	((drm_sis_private_t *)((dev)->dev_private))->buffers

extern int sis_init_context(int context);
extern int sis_final_context(int context);

#define DRIVER_CTX_CTOR sis_init_context
#define DRIVER_CTX_DTOR sis_final_context

#endif
