/* mga_drv.c -- Matrox G200/G400 driver -*- linux-c -*-
 * Created: Mon Dec 13 01:56:22 1999 by jhartmann@precisioninsight.com
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
 *
 * $FreeBSD$
 */

#include "dev/drm/drmP.h"
#include "dev/drm/drm.h"
#include "dev/drm/mga_drm.h"
#include "dev/drm/mga_drv.h"
#include "dev/drm/drm_pciids.h"

/* drv_PCI_IDs comes from drm_pciids.h, generated from drm_pciids.txt. */
static drm_pci_id_list_t mga_pciidlist[] = {
	mga_PCI_IDS
};

extern drm_ioctl_desc_t mga_ioctls[];
extern int mga_max_ioctl;

static void mga_configure(drm_device_t *dev)
{
	dev->dev_priv_size = sizeof(drm_mga_buf_priv_t);
	/* XXX dev->prerelease = mga_driver_prerelease; */
	dev->pretakedown = mga_driver_pretakedown;
	dev->vblank_wait = mga_driver_vblank_wait;
	dev->irq_preinstall = mga_driver_irq_preinstall;
	dev->irq_postinstall = mga_driver_irq_postinstall;
	dev->irq_uninstall = mga_driver_irq_uninstall;
	dev->irq_handler = mga_driver_irq_handler;
	dev->dma_ioctl = mga_dma_buffers;
	dev->dma_quiescent = mga_driver_dma_quiescent;

	dev->driver_ioctls = mga_ioctls;
	dev->max_driver_ioctl = mga_max_ioctl;

	dev->driver_name = DRIVER_NAME;
	dev->driver_desc = DRIVER_DESC;
	dev->driver_date = DRIVER_DATE;
	dev->driver_major = DRIVER_MAJOR;
	dev->driver_minor = DRIVER_MINOR;
	dev->driver_patchlevel = DRIVER_PATCHLEVEL;

	dev->use_agp = 1;
	dev->require_agp = 1;
	dev->use_mtrr = 1;
	dev->use_dma = 1;
	dev->use_irq = 1;
	dev->use_vbl_irq = 1;
}

#ifdef __FreeBSD__
static int
mga_probe(device_t dev)
{
	return drm_probe(dev, mga_pciidlist);
}

static int
mga_attach(device_t nbdev)
{
	drm_device_t *dev = device_get_softc(nbdev);

	bzero(dev, sizeof(drm_device_t));
	mga_configure(dev);
	return drm_attach(nbdev, mga_pciidlist);
}

static device_method_t mga_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		mga_probe),
	DEVMETHOD(device_attach,	mga_attach),
	DEVMETHOD(device_detach,	drm_detach),

	{ 0, 0 }
};

static driver_t mga_driver = {
	"drm",
	mga_methods,
	sizeof(drm_device_t)
};

extern devclass_t drm_devclass;
DRIVER_MODULE(mga, pci, mga_driver, drm_devclass, 0, 0);
MODULE_DEPEND(mga, drm, 1, 1, 1);

#elif defined(__NetBSD__) || defined(__OpenBSD__)
CFDRIVER_DECL(mga, DV_TTY, NULL);
#endif
