/* r128.h -- ATI Rage 128 DRM template customization -*- linux-c -*-
 * Created: Wed Feb 14 16:07:10 2001 by gareth@valinux.com */
/*-
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
 *    Gareth Hughes <gareth@valinux.com>
 *
 * $FreeBSD$
 */

#ifndef __R128_H__
#define __R128_H__

/* This remains constant for all DRM template files.
 */
#define DRM(x) r128_##x

/* General customization:
 */
#define __HAVE_AGP		1
#define __MUST_HAVE_AGP		0
#define __HAVE_MTRR		1
#define __HAVE_CTX_BITMAP	1
#define __HAVE_SG		1
#define __HAVE_PCI_DMA		1

#define DRIVER_AUTHOR		"Gareth Hughes, VA Linux Systems Inc."

#define DRIVER_NAME		"r128"
#define DRIVER_DESC		"ATI Rage 128"
#define DRIVER_DATE		"20030725"

#define DRIVER_MAJOR		2
#define DRIVER_MINOR		5
#define DRIVER_PATCHLEVEL	0

/* Interface history:
 *
 * ??  - ??
 * 2.4 - Add support for ycbcr textures (no new ioctls)
 * 2.5 - Add FLIP ioctl, disable FULLSCREEN.
 */
#define DRIVER_IOCTLS							    \
   [DRM_IOCTL_NR(DRM_IOCTL_DMA)]             = { r128_cce_buffers,  1, 0 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_INIT)]       = { r128_cce_init,     1, 1 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_CCE_START)]  = { r128_cce_start,    1, 1 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_CCE_STOP)]   = { r128_cce_stop,     1, 1 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_CCE_RESET)]  = { r128_cce_reset,    1, 1 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_CCE_IDLE)]   = { r128_cce_idle,     1, 0 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_RESET)]      = { r128_engine_reset, 1, 0 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_FULLSCREEN)] = { r128_fullscreen,   1, 0 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_SWAP)]       = { r128_cce_swap,     1, 0 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_FLIP)]       = { r128_cce_flip,     1, 0 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_CLEAR)]      = { r128_cce_clear,    1, 0 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_VERTEX)]     = { r128_cce_vertex,   1, 0 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_INDICES)]    = { r128_cce_indices,  1, 0 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_BLIT)]       = { r128_cce_blit,     1, 0 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_DEPTH)]      = { r128_cce_depth,    1, 0 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_STIPPLE)]    = { r128_cce_stipple,  1, 0 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_INDIRECT)]   = { r128_cce_indirect, 1, 1 }, \
   [DRM_IOCTL_NR(DRM_IOCTL_R128_GETPARAM)]   = { r128_getparam, 1, 0 },

/* Driver customization:
 */
#define DRIVER_PRERELEASE() do {					\
	if ( dev->dev_private ) {					\
		drm_r128_private_t *dev_priv = dev->dev_private;	\
		if ( dev_priv->page_flipping ) {			\
			r128_do_cleanup_pageflip( dev );		\
		}							\
	}								\
} while (0)

#define DRIVER_PRETAKEDOWN(dev) do {					\
	r128_do_cleanup_cce( dev );					\
} while (0)

/* DMA customization:
 */
#define __HAVE_DMA		1
#define __HAVE_IRQ		1
#define __HAVE_VBL_IRQ		1
#define __HAVE_SHARED_IRQ       1

#if 0
/* GH: Remove this for now... */
#define __HAVE_DMA_QUIESCENT	1
#define DRIVER_DMA_QUIESCENT() do {					\
	drm_r128_private_t *dev_priv = dev->dev_private;		\
	return r128_do_cce_idle( dev_priv );				\
} while (0)
#endif

/* Buffer customization:
 */
#define DRIVER_BUF_PRIV_T	drm_r128_buf_priv_t

#define DRIVER_AGP_BUFFERS_MAP( dev )					\
	((drm_r128_private_t *)((dev)->dev_private))->buffers

#endif
