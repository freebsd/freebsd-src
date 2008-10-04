/* mach64_drv.c -- ATI Rage 128 driver -*- linux-c -*-
 * Created: Mon Dec 13 09:47:27 1999 by faith@precisioninsight.com
 */
/*-
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");


#include <sys/types.h>

#include "dev/drm/drmP.h"
#include "dev/drm/drm.h"
#include "dev/drm/mach64_drm.h"
#include "dev/drm/mach64_drv.h"
#include "dev/drm/drm_pciids.h"

/* drv_PCI_IDs comes from drm_pciids.h, generated from drm_pciids.txt. */
static drm_pci_id_list_t mach64_pciidlist[] = {
	mach64_PCI_IDS
};

static void mach64_configure(struct drm_device *dev)
{
	dev->driver->driver_features =
	    DRIVER_USE_AGP | DRIVER_USE_MTRR | DRIVER_PCI_DMA |
	    DRIVER_HAVE_DMA | DRIVER_HAVE_IRQ;

	dev->driver->buf_priv_size	= 1; /* No dev_priv */
	dev->driver->lastclose		= mach64_driver_lastclose;
	dev->driver->get_vblank_counter	= mach64_get_vblank_counter;
	dev->driver->enable_vblank	= mach64_enable_vblank;
	dev->driver->disable_vblank	= mach64_disable_vblank;
	dev->driver->irq_preinstall	= mach64_driver_irq_preinstall;
	dev->driver->irq_postinstall	= mach64_driver_irq_postinstall;
	dev->driver->irq_uninstall	= mach64_driver_irq_uninstall;
	dev->driver->irq_handler	= mach64_driver_irq_handler;
	dev->driver->dma_ioctl		= mach64_dma_buffers;

	dev->driver->ioctls		= mach64_ioctls;
	dev->driver->max_ioctl		= mach64_max_ioctl;

	dev->driver->name		= DRIVER_NAME;
	dev->driver->desc		= DRIVER_DESC;
	dev->driver->date		= DRIVER_DATE;
	dev->driver->major		= DRIVER_MAJOR;
	dev->driver->minor		= DRIVER_MINOR;
	dev->driver->patchlevel		= DRIVER_PATCHLEVEL;
}

static int
mach64_probe(device_t dev)
{
	return drm_probe(dev, mach64_pciidlist);
}

static int
mach64_attach(device_t nbdev)
{
	struct drm_device *dev = device_get_softc(nbdev);

	bzero(dev, sizeof(struct drm_device));

	dev->driver = malloc(sizeof(struct drm_driver_info), M_DRM,
	    M_WAITOK | M_ZERO);

	mach64_configure(dev);

	return drm_attach(nbdev, mach64_pciidlist);
}

static int
mach64_detach(device_t nbdev)
{
	struct drm_device *dev = device_get_softc(nbdev);
	int ret;

	ret = drm_detach(nbdev);

	free(dev->driver, M_DRM);

	return ret;
}

static device_method_t mach64_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		mach64_probe),
	DEVMETHOD(device_attach,	mach64_attach),
	DEVMETHOD(device_detach,	mach64_detach),

	{ 0, 0 }
};

static driver_t mach64_driver = {
	"drm",
	mach64_methods,
	sizeof(struct drm_device)
};

extern devclass_t drm_devclass;
#if __FreeBSD_version >= 700010
DRIVER_MODULE(mach64, vgapci, mach64_driver, drm_devclass, 0, 0);
#else
DRIVER_MODULE(mach64, pci, mach64_driver, drm_devclass, 0, 0);
#endif
MODULE_DEPEND(mach64, drm, 1, 1, 1);
