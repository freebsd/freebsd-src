/* gamma.c -- 3dlabs GMX 2000 driver -*- linux-c -*-
 * Created: Mon Jan  4 08:58:31 1999 by gareth@valinux.com
 *
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
 * Authors:
 *    Gareth Hughes <gareth@valinux.com>
 *
 * $FreeBSD$
 */

#ifndef __GAMMA_H__
#define __GAMMA_H__

/* This remains constant for all DRM template files.
 */
#define DRM(x) gamma_##x

/* General customization:
 */
#define __HAVE_MTRR			1

/* DMA customization:
 */
#define __HAVE_DMA			1
#define __HAVE_OLD_DMA			1
#define __HAVE_PCI_DMA			1

#define __HAVE_MULTIPLE_DMA_QUEUES	1
#define __HAVE_DMA_WAITQUEUE		1

#define __HAVE_DMA_WAITLIST		1
#define __HAVE_DMA_FREELIST		1

#define __HAVE_DMA_FLUSH		1
#define __HAVE_DMA_SCHEDULE		1

#define __HAVE_DMA_READY		1
#define DRIVER_DMA_READY() do {						\
	gamma_dma_ready(dev);						\
} while (0)

#define __HAVE_DMA_QUIESCENT		1
#define DRIVER_DMA_QUIESCENT() do {					\
	/* FIXME ! */ 							\
	gamma_dma_quiescent_dual(dev);					\
	return 0;							\
} while (0)

#define __HAVE_DMA_IRQ			1
#define __HAVE_DMA_IRQ_BH		1
#define DRIVER_PREINSTALL() do {					\
	drm_gamma_private_t *dev_priv =					\
				(drm_gamma_private_t *)dev->dev_private;\
	GAMMA_WRITE( GAMMA_GCOMMANDMODE,	0x00000000 );		\
	GAMMA_WRITE( GAMMA_GDMACONTROL,		0x00000000 );		\
} while (0)

#define DRIVER_POSTINSTALL() do {					\
	drm_gamma_private_t *dev_priv =					\
				(drm_gamma_private_t *)dev->dev_private;\
	GAMMA_WRITE( GAMMA_GINTENABLE,		0x00002001 );		\
	GAMMA_WRITE( GAMMA_COMMANDINTENABLE,	0x00000008 );		\
	GAMMA_WRITE( GAMMA_GDELAYTIMER,		0x00039090 );		\
} while (0)

#define DRIVER_UNINSTALL() do {						\
	drm_gamma_private_t *dev_priv =					\
				(drm_gamma_private_t *)dev->dev_private;\
	GAMMA_WRITE( GAMMA_GDELAYTIMER,		0x00000000 );		\
	GAMMA_WRITE( GAMMA_COMMANDINTENABLE,	0x00000000 );		\
	GAMMA_WRITE( GAMMA_GINTENABLE,		0x00000000 );		\
} while (0)

#endif /* __GAMMA_H__ */
