/* drm_dma.c -- DMA IOCTL and function support -*- linux-c -*-
 * Created: Fri Mar 19 14:30:16 1999 by faith@valinux.com
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

#ifndef __HAVE_DMA_WAITQUEUE
#define __HAVE_DMA_WAITQUEUE	0
#endif
#ifndef __HAVE_DMA_RECLAIM
#define __HAVE_DMA_RECLAIM	0
#endif
#ifndef __HAVE_SHARED_IRQ
#define __HAVE_SHARED_IRQ	0
#endif

#if __HAVE_DMA

int DRM(dma_setup)( drm_device_t *dev )
{

	dev->dma = DRM(calloc)(1, sizeof(*dev->dma), DRM_MEM_DRIVER);
	if (dev->dma == NULL)
		return DRM_ERR(ENOMEM);

	DRM_SPININIT(dev->dma_lock, "drmdma");

	return 0;
}

void DRM(dma_takedown)(drm_device_t *dev)
{
	drm_device_dma_t  *dma = dev->dma;
	int		  i, j;

	if (dma == NULL)
		return;

				/* Clear dma buffers */
	for (i = 0; i <= DRM_MAX_ORDER; i++) {
#if __HAVE_PCI_DMA
		if (dma->bufs[i].seg_count) {
			DRM_DEBUG("order %d: buf_count = %d,"
				  " seg_count = %d\n",
				  i,
				  dma->bufs[i].buf_count,
				  dma->bufs[i].seg_count);
			for (j = 0; j < dma->bufs[i].seg_count; j++) {
				if (dma->bufs[i].seglist[j] != NULL)
					DRM(pci_free)(dev, dma->bufs[i].buf_size,
					    (void *)dma->bufs[i].seglist[j],
					    dma->bufs[i].seglist_bus[j]);
			}
			DRM(free)(dma->bufs[i].seglist,
				  dma->bufs[i].seg_count
				  * sizeof(*dma->bufs[0].seglist),
				  DRM_MEM_SEGS);
			DRM(free)(dma->bufs[i].seglist_bus,
				  dma->bufs[i].seg_count
				  * sizeof(*dma->bufs[0].seglist_bus),
				  DRM_MEM_SEGS);
		}
#endif /* __HAVE_PCI_DMA */

	   	if (dma->bufs[i].buf_count) {
		   	for (j = 0; j < dma->bufs[i].buf_count; j++) {
				DRM(free)(dma->bufs[i].buflist[j].dev_private,
					dma->bufs[i].buflist[j].dev_priv_size,
					DRM_MEM_BUFS);
			}
		   	DRM(free)(dma->bufs[i].buflist,
				  dma->bufs[i].buf_count *
				  sizeof(*dma->bufs[0].buflist),
				  DRM_MEM_BUFS);
		}
	}

	DRM(free)(dma->buflist, dma->buf_count * sizeof(*dma->buflist),
	    DRM_MEM_BUFS);
	DRM(free)(dma->pagelist, dma->page_count * sizeof(*dma->pagelist),
	    DRM_MEM_PAGES);
	DRM(free)(dev->dma, sizeof(*dev->dma), DRM_MEM_DRIVER);
	dev->dma = NULL;
	DRM_SPINUNINIT(dev->dma_lock);
}


void DRM(free_buffer)(drm_device_t *dev, drm_buf_t *buf)
{
	if (!buf) return;

	buf->pending  = 0;
	buf->filp     = NULL;
	buf->used     = 0;
}

#if !__HAVE_DMA_RECLAIM
void DRM(reclaim_buffers)(drm_device_t *dev, DRMFILE filp)
{
	drm_device_dma_t *dma = dev->dma;
	int		 i;

	if (!dma) return;
	for (i = 0; i < dma->buf_count; i++) {
		if (dma->buflist[i]->filp == filp) {
			switch (dma->buflist[i]->list) {
			case DRM_LIST_NONE:
				DRM(free_buffer)(dev, dma->buflist[i]);
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
#endif

#if !__HAVE_IRQ
/* This stub DRM_IOCTL_CONTROL handler is for the drivers that used to require
 * IRQs for DMA but no longer do.  It maintains compatibility with the X Servers
 * that try to use the control ioctl by simply returning success.
 */
int DRM(control)( DRM_IOCTL_ARGS )
{
	drm_control_t ctl;

	DRM_COPY_FROM_USER_IOCTL( ctl, (drm_control_t *) data, sizeof(ctl) );

	switch ( ctl.func ) {
	case DRM_INST_HANDLER:
	case DRM_UNINST_HANDLER:
		return 0;
	default:
		return DRM_ERR(EINVAL);
	}
}
#endif

#endif /* __HAVE_DMA */
