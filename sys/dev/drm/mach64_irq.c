/* mach64_irq.c -- IRQ handling for ATI Mach64 -*- linux-c -*-
 * Created: Tue Feb 25, 2003 by Leif Delgass, based on radeon_irq.c/r128_irq.c
 */
/*-
 * Copyright (C) The Weather Channel, Inc.  2002.
 * Copyright 2003 Leif Delgass
 * All Rights Reserved.
 *
 * The Weather Channel (TM) funded Tungsten Graphics to develop the
 * initial release of the Radeon 8500 driver under the XFree86 license.
 * This notice must be preserved.
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
 * THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Keith Whitwell <keith@tungstengraphics.com>
 *    Eric Anholt <anholt@FreeBSD.org>
 *    Leif Delgass <ldelgass@retinalburn.net>
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "dev/drm/drmP.h"
#include "dev/drm/drm.h"
#include "dev/drm/mach64_drm.h"
#include "dev/drm/mach64_drv.h"

irqreturn_t mach64_driver_irq_handler(DRM_IRQ_ARGS)
{
	drm_device_t *dev = (drm_device_t *) arg;
	drm_mach64_private_t *dev_priv =
	    (drm_mach64_private_t *) dev->dev_private;
	int status;

	status = MACH64_READ(MACH64_CRTC_INT_CNTL);

	/* VBLANK interrupt */
	if (status & MACH64_CRTC_VBLANK_INT) {
		/* Mask off all interrupt ack bits before setting the ack bit, since
		 * there may be other handlers outside the DRM.
		 *
		 * NOTE: On mach64, you need to keep the enable bits set when doing
		 * the ack, despite what the docs say about not acking and enabling
		 * in a single write.
		 */
		MACH64_WRITE(MACH64_CRTC_INT_CNTL,
			     (status & ~MACH64_CRTC_INT_ACKS)
			     | MACH64_CRTC_VBLANK_INT);

		atomic_inc(&dev->vbl_received);
		DRM_WAKEUP(&dev->vbl_queue);
		drm_vbl_send_signals(dev);
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

int mach64_driver_vblank_wait(drm_device_t * dev, unsigned int *sequence)
{
	unsigned int cur_vblank;
	int ret = 0;

	/* Assume that the user has missed the current sequence number
	 * by about a day rather than she wants to wait for years
	 * using vertical blanks...
	 */
	DRM_WAIT_ON(ret, dev->vbl_queue, 3 * DRM_HZ,
		    (((cur_vblank = atomic_read(&dev->vbl_received))
		      - *sequence) <= (1 << 23)));

	*sequence = cur_vblank;

	return ret;
}

/* drm_dma.h hooks
*/
void mach64_driver_irq_preinstall(drm_device_t * dev)
{
	drm_mach64_private_t *dev_priv =
	    (drm_mach64_private_t *) dev->dev_private;

	u32 status = MACH64_READ(MACH64_CRTC_INT_CNTL);

	DRM_DEBUG("before install CRTC_INT_CTNL: 0x%08x\n", status);

	/* Disable and clear VBLANK interrupt */
	MACH64_WRITE(MACH64_CRTC_INT_CNTL, (status & ~MACH64_CRTC_VBLANK_INT_EN)
		     | MACH64_CRTC_VBLANK_INT);
}

void mach64_driver_irq_postinstall(drm_device_t * dev)
{
	drm_mach64_private_t *dev_priv =
	    (drm_mach64_private_t *) dev->dev_private;

	/* Turn on VBLANK interrupt */
	MACH64_WRITE(MACH64_CRTC_INT_CNTL, MACH64_READ(MACH64_CRTC_INT_CNTL)
		     | MACH64_CRTC_VBLANK_INT_EN);

	DRM_DEBUG("after install CRTC_INT_CTNL: 0x%08x\n",
		  MACH64_READ(MACH64_CRTC_INT_CNTL));

}

void mach64_driver_irq_uninstall(drm_device_t * dev)
{
	drm_mach64_private_t *dev_priv =
	    (drm_mach64_private_t *) dev->dev_private;
	if (!dev_priv)
		return;

	/* Disable and clear VBLANK interrupt */
	MACH64_WRITE(MACH64_CRTC_INT_CNTL,
		     (MACH64_READ(MACH64_CRTC_INT_CNTL) &
		      ~MACH64_CRTC_VBLANK_INT_EN)
		     | MACH64_CRTC_VBLANK_INT);

	DRM_DEBUG("after uninstall CRTC_INT_CTNL: 0x%08x\n",
		  MACH64_READ(MACH64_CRTC_INT_CNTL));
}
