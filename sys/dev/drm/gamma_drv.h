/* gamma_drv.h -- Private header for 3dlabs GMX 2000 driver -*- linux-c -*-
 * Created: Mon Jan  4 10:05:05 1999 by faith@precisioninsight.com
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
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *
 * $FreeBSD$
 */

#ifndef _GAMMA_DRV_H_
#define _GAMMA_DRV_H_


typedef struct drm_gamma_private {
	drm_map_t *buffers;
	drm_map_t *mmio0;
	drm_map_t *mmio1;
	drm_map_t *mmio2;
	drm_map_t *mmio3;
} drm_gamma_private_t;

#define LOCK_TEST_WITH_RETURN( dev )					\
do {									\
	if ( !_DRM_LOCK_IS_HELD( dev->lock.hw_lock->lock ) ||		\
	     dev->lock.pid != DRM_OS_CURRENTPID ) {				\
		DRM_ERROR( "%s called without lock held\n",		\
			   __FUNCTION__ );				\
		return DRM_OS_ERR(EINVAL);						\
	}								\
} while (0)


extern void gamma_dma_ready(drm_device_t *dev);
extern void gamma_dma_quiescent_single(drm_device_t *dev);
extern void gamma_dma_quiescent_dual(drm_device_t *dev);

				/* gamma_dma.c */
extern int  gamma_dma_schedule(drm_device_t *dev, int locked);
extern int  gamma_dma( DRM_OS_IOCTL );
extern int  gamma_find_devices(void);
extern int  gamma_found(void);


#define GAMMA_OFF(reg)						   \
	((reg < 0x1000)						   \
	 ? reg							   \
	 : ((reg < 0x10000)					   \
	    ? (reg - 0x1000)					   \
	    : ((reg < 0x11000)					   \
	       ? (reg - 0x10000)				   \
	       : (reg - 0x11000))))

#define GAMMA_BASE(reg)	 ((unsigned long)				     \
			  ((reg < 0x1000)    ? dev_priv->mmio0->handle :     \
			   ((reg < 0x10000)  ? dev_priv->mmio1->handle :     \
			    ((reg < 0x11000) ? dev_priv->mmio2->handle :     \
					       dev_priv->mmio3->handle))))
	
#define GAMMA_ADDR(reg)	 (GAMMA_BASE(reg) + GAMMA_OFF(reg))
#define GAMMA_DEREF(reg) *(__volatile__ int *)GAMMA_ADDR(reg)
#define GAMMA_READ(reg)	 GAMMA_DEREF(reg)
#define GAMMA_WRITE(reg,val) do { GAMMA_DEREF(reg) = val; } while (0)

#define GAMMA_BROADCASTMASK    0x9378
#define GAMMA_COMMANDINTENABLE 0x0c48
#define GAMMA_DMAADDRESS       0x0028
#define GAMMA_DMACOUNT	       0x0030
#define GAMMA_FILTERMODE       0x8c00
#define GAMMA_GCOMMANDINTFLAGS 0x0c50
#define GAMMA_GCOMMANDMODE     0x0c40
#define GAMMA_GCOMMANDSTATUS   0x0c60
#define GAMMA_GDELAYTIMER      0x0c38
#define GAMMA_GDMACONTROL      0x0060
#define GAMMA_GINTENABLE       0x0808
#define GAMMA_GINTFLAGS	       0x0810
#define GAMMA_INFIFOSPACE      0x0018
#define GAMMA_OUTFIFOWORDS     0x0020
#define GAMMA_OUTPUTFIFO       0x2000
#define GAMMA_SYNC	       0x8c40
#define GAMMA_SYNC_TAG	       0x0188

#endif
