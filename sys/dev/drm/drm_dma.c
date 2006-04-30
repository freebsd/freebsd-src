/* drm_dma.c -- DMA IOCTL and function support -*- linux-c -*-
 * Created: Fri Mar 19 14:30:16 1999 by faith@valinux.com
 */
/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "dev/drm/drmP.h"

int drm_dma_setup(drm_device_t *dev)
{

	dev->dma = malloc(sizeof(*dev->dma), M_DRM, M_NOWAIT | M_ZERO);
	if (dev->dma == NULL)
		return DRM_ERR(ENOMEM);

	DRM_SPININIT(dev->dma_lock, "drmdma");

	return 0;
}

void drm_dma_takedown(drm_device_t *dev)
{
	drm_device_dma_t  *dma = dev->dma;
	int		  i, j;

	if (dma == NULL)
		return;

				/* Clear dma buffers */
	for (i = 0; i <= DRM_MAX_ORDER; i++) {
		if (dma->bufs[i].seg_count) {
			DRM_DEBUG("order %d: buf_count = %d,"
				  " seg_count = %d\n",
				  i,
				  dma->bufs[i].buf_count,
				  dma->bufs[i].seg_count);
			for (j = 0; j < dma->bufs[i].seg_count; j++) {
				drm_pci_free(dev, dma->bufs[i].seglist[j]);
			}
			free(dma->bufs[i].seglist, M_DRM);
		}

	   	if (dma->bufs[i].buf_count) {
		   	for (j = 0; j < dma->bufs[i].buf_count; j++) {
				free(dma->bufs[i].buflist[j].dev_private,
				    M_DRM);
			}
		   	free(dma->bufs[i].buflist, M_DRM);
		}
	}

	free(dma->buflist, M_DRM);
	free(dma->pagelist, M_DRM);
	free(dev->dma, M_DRM);
	dev->dma = NULL;
	DRM_SPINUNINIT(dev->dma_lock);
}


void drm_free_buffer(drm_device_t *dev, drm_buf_t *buf)
{
	if (!buf) return;

	buf->pending  = 0;
	buf->filp     = NULL;
	buf->used     = 0;
}

void drm_reclaim_buffers(drm_device_t *dev, DRMFILE filp)
{
	drm_device_dma_t *dma = dev->dma;
	int		 i;

	if (!dma) return;
	for (i = 0; i < dma->buf_count; i++) {
		if (dma->buflist[i]->filp == filp) {
			switch (dma->buflist[i]->list) {
			case DRM_LIST_NONE:
				drm_free_buffer(dev, dma->buflist[i]);
				break;
			case DRM_LIST_WAIT:
				dma->buflist[i]->list = DRM_LIST_RECLAIM;
				break;
			default:
				/* Buffer already on hardware. */
				break;
			}
		}
	}
}

/* Call into the driver-specific DMA handler */
int drm_dma(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;

	if (dev->driver.dma_ioctl) {
		return dev->driver.dma_ioctl(kdev, cmd, data, flags, p, filp);
	} else {
		DRM_DEBUG("DMA ioctl on driver with no dma handler\n");
		return EINVAL;
	}
}
