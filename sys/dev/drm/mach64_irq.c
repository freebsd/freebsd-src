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
	struct drm_device *dev = arg;
	drm_mach64_private_t *dev_priv = dev->dev_private;
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

		atomic_inc(&dev_priv->vbl_received);
		drm_handle_vblank(dev, 0);
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

u32 mach64_get_vblank_counter(struct drm_device * dev, int crtc)
{
	const drm_mach64_private_t *const dev_priv = dev->dev_private;

	if (crtc != 0)
		return 0;

	return atomic_read(&dev_priv->vbl_received);
}

int mach64_enable_vblank(struct drm_device * dev, int crtc)
{
	drm_mach64_private_t *dev_priv = dev->dev_private;
	u32 status = MACH64_READ(MACH64_CRTC_INT_CNTL);

	if (crtc != 0) {
		DRM_ERROR("tried to enable vblank on non-existent crtc %d\n",
			  crtc);
		return -EINVAL;
	}

	DRM_DEBUG("before enable vblank CRTC_INT_CTNL: 0x%08x\n", status);

	/* Turn on VBLANK interrupt */
	MACH64_WRITE(MACH64_CRTC_INT_CNTL, MACH64_READ(MACH64_CRTC_INT_CNTL)
		     | MACH64_CRTC_VBLANK_INT_EN);

	return 0;
}

void mach64_disable_vblank(struct drm_device * dev, int crtc)
{
	if (crtc != 0) {
		DRM_ERROR("tried to disable vblank on non-existent crtc %d\n",
			  crtc);
		return;
	}

	/*
	 * FIXME: implement proper interrupt disable by using the vblank
	 * counter register (if available).
	 */
}

static void mach64_disable_vblank_local(struct drm_device * dev, int crtc)
{
	drm_mach64_private_t *dev_priv = dev->dev_private;
	u32 status = MACH64_READ(MACH64_CRTC_INT_CNTL);

	if (crtc != 0) {
		DRM_ERROR("tried to disable vblank on non-existent crtc %d\n",
			  crtc);
		return;
	}

	DRM_DEBUG("before disable vblank CRTC_INT_CTNL: 0x%08x\n", status);

	/* Disable and clear VBLANK interrupt */
	MACH64_WRITE(MACH64_CRTC_INT_CNTL, (status & ~MACH64_CRTC_VBLANK_INT_EN)
		     | MACH64_CRTC_VBLANK_INT);
}

void mach64_driver_irq_preinstall(struct drm_device * dev)
{
	drm_mach64_private_t *dev_priv = dev->dev_private;

	u32 status = MACH64_READ(MACH64_CRTC_INT_CNTL);

	DRM_DEBUG("before install CRTC_INT_CTNL: 0x%08x\n", status);

	mach64_disable_vblank_local(dev, 0);
}

int mach64_driver_irq_postinstall(struct drm_device * dev)
{
	return drm_vblank_init(dev, 1);
}

void mach64_driver_irq_uninstall(struct drm_device * dev)
{
	drm_mach64_private_t *dev_priv = dev->dev_private;
	if (!dev_priv)
		return;

	mach64_disable_vblank_local(dev, 0);

	DRM_DEBUG("after uninstall CRTC_INT_CTNL: 0x%08x\n",
		  MACH64_READ(MACH64_CRTC_INT_CNTL));
}
