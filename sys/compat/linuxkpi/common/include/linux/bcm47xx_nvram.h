/*
 * Copyright (c) 2025 Bjoern A. Zeeb
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef	_LINUXKPI_LINUX_BCM47XX_NVRAM_H
#define	_LINUXKPI_LINUX_BCM47XX_NVRAM_H

static inline char *
bcm47xx_nvram_get_contents(size_t *x __unused)
{
	return (NULL);
};

static inline void
bcm47xx_nvram_release_contents(const char *x __unused)
{
};

#endif	/* _LINUXKPI_LINUX_BCM47XX_NVRAM_H */
