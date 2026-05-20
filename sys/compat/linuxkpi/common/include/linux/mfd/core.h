/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Foundation
 */

#ifndef	_LINUXKPI_LINUX_MFD_CORE_H_
#define	_LINUXKPI_LINUX_MFD_CORE_H_

#include <linux/platform_device.h>

/*
 * <linux/ioport.h> is not included by Linux, but we need it here to get the
 * definition of `struct resource`.
 *
 * At least the amdgpu DRM driver (amdgpu_isp.c at the time of this writing)
 * needs the structure without including this header: it relies on an implicit
 * include of <linux/ioport.h> from <linux/pci.h>, which we can't have due to
 * conflict with the FreeBSD native `struct resource`.
 */
#include <linux/ioport.h>

#include <linux/kernel.h> /* pr_debug */

struct resource;
struct mfd_cell {
	const char		*name;
	void			*platform_data;
	size_t			 pdata_size;
	int			 num_resources;
	const struct resource	*resources;
};

static inline int
mfd_add_hotplug_devices(struct device *parent,
    const struct mfd_cell *cells, int n_devs)
{
	pr_debug("%s: TODO\n", __func__);

	return (0);
}

static inline void
mfd_remove_devices(struct device *parent)
{
	pr_debug("%s: TODO\n", __func__);
}

#endif /* _LINUXKPI_LINUX_MFD_CORE_H_ */
