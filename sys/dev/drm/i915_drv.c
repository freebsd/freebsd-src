/* i915_drv.c -- ATI Radeon driver -*- linux-c -*-
 * Created: Wed Feb 14 17:10:04 2001 by gareth@valinux.com
 */
/*-
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
 *    Gareth Hughes <gareth@valinux.com>
 *
 * $FreeBSD$
 */

#include "drmP.h"
#include "drm.h"
#include "i915_drm.h"
#include "i915_drv.h"
#include "drm_pciids.h"

/* drv_PCI_IDs comes from drm_pciids.h, generated from drm_pciids.txt. */
static drm_pci_id_list_t i915_pciidlist[] = {
	i915_PCI_IDS
};

extern drm_ioctl_desc_t i915_ioctls[];
extern int i915_max_ioctl;

static void i915_configure(drm_device_t *dev)
{
	dev->dev_priv_size = 1;	/* No dev_priv */
	dev->prerelease = i915_driver_prerelease;
	dev->pretakedown = i915_driver_pretakedown;
	dev->irq_preinstall = i915_driver_irq_preinstall;
	dev->irq_postinstall = i915_driver_irq_postinstall;
	dev->irq_uninstall = i915_driver_irq_uninstall;
	dev->irq_handler = i915_driver_irq_handler;

	dev->driver_ioctls = i915_ioctls;
	dev->max_driver_ioctl = i915_max_ioctl;

	dev->driver_name = DRIVER_NAME;
	dev->driver_desc = DRIVER_DESC;
	dev->driver_date = DRIVER_DATE;
	dev->driver_major = DRIVER_MAJOR;
	dev->driver_minor = DRIVER_MINOR;
	dev->driver_patchlevel = DRIVER_PATCHLEVEL;

	dev->use_agp = 1;
	dev->require_agp = 1;
	dev->use_mtrr = 1;
	dev->use_irq = 1;
}

#ifdef __FreeBSD__
static int
i915_probe(device_t dev)
{
	return drm_probe(dev, i915_pciidlist);
}

static int
i915_attach(device_t nbdev)
{
	drm_device_t *dev = device_get_softc(nbdev);

	bzero(dev, sizeof(drm_device_t));
	i915_configure(dev);
	return drm_attach(nbdev, i915_pciidlist);
}

static device_method_t i915_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		i915_probe),
	DEVMETHOD(device_attach,	i915_attach),
	DEVMETHOD(device_detach,	drm_detach),

	{ 0, 0 }
};

static driver_t i915_driver = {
	"drmsub",
	i915_methods,
	sizeof(drm_device_t)
};

extern devclass_t drm_devclass;
DRIVER_MODULE(i915, pci, i915_driver, drm_devclass, 0, 0);
MODULE_DEPEND(i915, drm, 1, 1, 1);

#elif defined(__NetBSD__) || defined(__OpenBSD__)
CFDRIVER_DECL(i915, DV_TTY, NULL);
#endif
