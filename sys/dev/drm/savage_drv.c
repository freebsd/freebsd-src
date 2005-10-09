/* savage_drv.c -- Savage DRI driver
 */
/*-
 * Copyright 2005 Eric Anholt
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
 * ERIC ANHOLT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <anholt@FreeBSD.org>
 *
 * $FreeBSD$
 */

#include "drmP.h"
#include "drm.h"
#include "savage_drm.h"
#include "savage_drv.h"
#include "drm_pciids.h"

/* drv_PCI_IDs comes from drm_pciids.h, generated from drm_pciids.txt. */
static drm_pci_id_list_t savage_pciidlist[] = {
	savage_PCI_IDS
};

extern drm_ioctl_desc_t savage_ioctls[];
extern int savage_max_ioctl;

static void savage_configure(drm_device_t *dev)
{
	dev->dev_priv_size = sizeof(drm_savage_buf_priv_t);
	dev->preinit = savage_preinit;
	dev->postcleanup = savage_postcleanup;
	dev->reclaim_buffers = savage_reclaim_buffers;
	dev->dma_ioctl = savage_bci_buffers;

	dev->driver_ioctls = savage_ioctls;
	dev->max_driver_ioctl = savage_max_ioctl;

	dev->driver_name = DRIVER_NAME;
	dev->driver_desc = DRIVER_DESC;
	dev->driver_date = DRIVER_DATE;
	dev->driver_major = DRIVER_MAJOR;
	dev->driver_minor = DRIVER_MINOR;
	dev->driver_patchlevel = DRIVER_PATCHLEVEL;

	dev->use_agp = 1;
	dev->use_mtrr = 1;
	dev->use_pci_dma = 1;
	dev->use_dma = 1;
}

#ifdef __FreeBSD__
static int
savage_probe(device_t dev)
{
	return drm_probe(dev, savage_pciidlist);
}

static int
savage_attach(device_t nbdev)
{
	drm_device_t *dev = device_get_softc(nbdev);

	bzero(dev, sizeof(drm_device_t));
	savage_configure(dev);
	return drm_attach(nbdev, savage_pciidlist);
}

static device_method_t savage_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		savage_probe),
	DEVMETHOD(device_attach,	savage_attach),
	DEVMETHOD(device_detach,	drm_detach),

	{ 0, 0 }
};

static driver_t savage_driver = {
	"drm",
	savage_methods,
	sizeof(drm_device_t)
};

extern devclass_t drm_devclass;
DRIVER_MODULE(savage, pci, savage_driver, drm_devclass, 0, 0);
MODULE_DEPEND(savage, drm, 1, 1, 1);

#elif defined(__NetBSD__) || defined(__OpenBSD__)
CFDRIVER_DECL(savage, DV_TTY, NULL);
#endif
