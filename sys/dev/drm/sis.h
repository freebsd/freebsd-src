/* sis_drv.h -- Private header for sis driver -*- linux-c -*- */
/*-
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
 * $FreeBSD$
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/linux/drm/kernel/sis.h,v 1.3 2002/10/30 12:52:38 alanh Exp $ */

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

#define DRIVER_AUTHOR		"SIS"
#define DRIVER_NAME		"sis"
#define DRIVER_DESC		"SIS 300/630/540"
#define DRIVER_DATE		"20030826"
#define DRIVER_MAJOR		1
#define DRIVER_MINOR		1
#define DRIVER_PATCHLEVEL	0

#define DRIVER_IOCTLS \
	[DRM_IOCTL_NR(DRM_IOCTL_SIS_FB_ALLOC)]  = { sis_fb_alloc,	1, 0 }, \
	[DRM_IOCTL_NR(DRM_IOCTL_SIS_FB_FREE)]   = { sis_fb_free,	1, 0 }, \
	[DRM_IOCTL_NR(DRM_IOCTL_SIS_AGP_INIT)]  = { sis_ioctl_agp_init,	1, 1 }, \
	[DRM_IOCTL_NR(DRM_IOCTL_SIS_AGP_ALLOC)] = { sis_ioctl_agp_alloc, 1, 0 }, \
	[DRM_IOCTL_NR(DRM_IOCTL_SIS_AGP_FREE)]	= { sis_ioctl_agp_free,	1, 0 }, \
	[DRM_IOCTL_NR(DRM_IOCTL_SIS_FB_INIT)]	= { sis_fb_init,	1, 1 }

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
