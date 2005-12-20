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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "dev/drm/drmP.h"
#include "dev/drm/drm.h"
#include "dev/drm/savage_drm.h"
#include "dev/drm/savage_drv.h"
#include "dev/drm/drm_pciids.h"

/* drv_PCI_IDs comes from drm_pciids.h, generated from drm_pciids.txt. */
static drm_pci_id_list_t savage_pciidlist[] = {
	savage_PCI_IDS
};

static void savage_configure(drm_device_t *dev)
{
	dev->driver.buf_priv_size	= sizeof(drm_savage_buf_priv_t);
	dev->driver.load		= savage_driver_load;
	dev->driver.firstopen		= savage_driver_firstopen;
	dev->driver.lastclose		= savage_driver_lastclose;
	dev->driver.unload		= savage_driver_unload;
	dev->driver.reclaim_buffers_locked = savage_reclaim_buffers;
	dev->driver.dma_ioctl		= savage_bci_buffers;

	dev->driver.ioctls		= savage_ioctls;
	dev->driver.max_ioctl		= savage_max_ioctl;

	dev->driver.name		= DRIVER_NAME;
	dev->driver.desc		= DRIVER_DESC;
	dev->driver.date		= DRIVER_DATE;
	dev->driver.major		= DRIVER_MAJOR;
	dev->driver.minor		= DRIVER_MINOR;
	dev->driver.patchlevel		= DRIVER_PATCHLEVEL;

	dev->driver.use_agp		= 1;
	dev->driver.use_mtrr		= 1;
	dev->driver.use_pci_dma		= 1;
	dev->driver.use_dma		= 1;
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
#if __FreeBSD_version >= 700010
DRIVER_MODULE(savage, vgapci, savage_driver, drm_devclass, 0, 0);
#else
DRIVER_MODULE(savage, pci, savage_driver, drm_devclass, 0, 0);
#endif
MODULE_DEPEND(savage, drm, 1, 1, 1);

#elif defined(__NetBSD__) || defined(__OpenBSD__)
CFDRIVER_DECL(savage, DV_TTY, NULL);
#endif
