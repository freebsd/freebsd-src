/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020-2022 Bjoern A. Zeeb
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_LINUXKPI_LINUX_PLATFORM_DEVICE_H
#define	_LINUXKPI_LINUX_PLATFORM_DEVICE_H

#include <linux/kernel.h>
#include <linux/device.h>

struct platform_device {
	struct device			dev;
};

struct platform_driver {
	int				(*remove)(struct platform_device *);
	struct device_driver		driver;
};

#define	dev_is_platform(dev)	(false)
#define	to_platform_device(dev)	(NULL)

static __inline int
platform_driver_register(struct platform_driver *pdrv)
{

	pr_debug("%s: TODO\n", __func__);
	return (-ENXIO);
}

static __inline void *
dev_get_platdata(struct device *dev)
{

	pr_debug("%s: TODO\n", __func__);
	return (NULL);
}

static __inline int
platform_driver_probe(struct platform_driver *pdrv,
    int(*pd_probe_f)(struct platform_device *))
{

	pr_debug("%s: TODO\n", __func__);
	return (-ENODEV);
}

static __inline void
platform_driver_unregister(struct platform_driver *pdrv)
{

	pr_debug("%s: TODO\n", __func__);
	return;
}

static __inline void
platform_device_unregister(struct platform_device *pdev)
{

	pr_debug("%s: TODO\n", __func__);
	return;
}

#endif	/* _LINUXKPI_LINUX_PLATFORM_DEVICE_H */
