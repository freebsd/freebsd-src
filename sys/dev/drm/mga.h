/* mga.h -- Matrox G200/G400 DRM template customization -*- linux-c -*-
 * Created: Thu Jan 11 21:29:32 2001 by gareth@valinux.com */
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

#ifndef __MGA_H__
#define __MGA_H__

/* This remains constant for all DRM template files.
 */
#define DRM(x) mga_##x

/* General customization:
 */
#define __HAVE_AGP		1
#define __MUST_HAVE_AGP		1
#define __HAVE_MTRR		1
#define __HAVE_CTX_BITMAP	1

#define DRIVER_AUTHOR		"Gareth Hughes, VA Linux Systems Inc."

#define DRIVER_NAME		"mga"
#define DRIVER_DESC		"Matrox G200/G400"
#define DRIVER_DATE		"20021029"

#define DRIVER_MAJOR		3
#define DRIVER_MINOR		1
#define DRIVER_PATCHLEVEL	0

#define DRIVER_IOCTLS							   \
	[DRM_IOCTL_NR(DRM_IOCTL_DMA)]	      = { mga_dma_buffers, 1, 0 }, \
	[DRM_IOCTL_NR(DRM_IOCTL_MGA_INIT)]    = { mga_dma_init,    1, 1 }, \
	[DRM_IOCTL_NR(DRM_IOCTL_MGA_FLUSH)]   = { mga_dma_flush,   1, 0 }, \
	[DRM_IOCTL_NR(DRM_IOCTL_MGA_RESET)]   = { mga_dma_reset,   1, 0 }, \
	[DRM_IOCTL_NR(DRM_IOCTL_MGA_SWAP)]    = { mga_dma_swap,    1, 0 }, \
	[DRM_IOCTL_NR(DRM_IOCTL_MGA_CLEAR)]   = { mga_dma_clear,   1, 0 }, \
	[DRM_IOCTL_NR(DRM_IOCTL_MGA_VERTEX)]  = { mga_dma_vertex,  1, 0 }, \
	[DRM_IOCTL_NR(DRM_IOCTL_MGA_INDICES)] = { mga_dma_indices, 1, 0 }, \
	[DRM_IOCTL_NR(DRM_IOCTL_MGA_ILOAD)]   = { mga_dma_iload,   1, 0 }, \
	[DRM_IOCTL_NR(DRM_IOCTL_MGA_BLIT)]    = { mga_dma_blit,    1, 0 }, \
	[DRM_IOCTL_NR(DRM_IOCTL_MGA_GETPARAM)]= { mga_getparam,    1, 0 },

#define __HAVE_COUNTERS         3
#define __HAVE_COUNTER6         _DRM_STAT_IRQ
#define __HAVE_COUNTER7         _DRM_STAT_PRIMARY
#define __HAVE_COUNTER8         _DRM_STAT_SECONDARY

/* Driver customization:
 */
#define DRIVER_PRETAKEDOWN( dev ) do {					\
	mga_do_cleanup_dma( dev );					\
} while (0)

/* DMA customization:
 */
#define __HAVE_DMA		1
#define __HAVE_IRQ		1
#define __HAVE_VBL_IRQ		1
#define __HAVE_SHARED_IRQ       1

#define __HAVE_DMA_QUIESCENT	1
#define DRIVER_DMA_QUIESCENT() do {					\
	drm_mga_private_t *dev_priv = dev->dev_private;			\
	return mga_do_wait_for_idle( dev_priv );			\
} while (0)

/* Buffer customization:
 */
#define DRIVER_BUF_PRIV_T	drm_mga_buf_priv_t

#define DRIVER_AGP_BUFFERS_MAP( dev )					\
	((drm_mga_private_t *)((dev)->dev_private))->buffers

#endif
