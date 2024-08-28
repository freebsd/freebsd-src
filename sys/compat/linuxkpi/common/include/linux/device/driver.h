/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Bjoern A. Zeeb
 * Copyright (c) 2024 The FreeBSD Foundation
 *
 * Portions of this software were developed by Björn Zeeb
 * under sponsorship from the FreeBSD Foundation.
 */

#ifndef	LINUXKPI_LINUX_DEVICE_DRIVER_H
#define	LINUXKPI_LINUX_DEVICE_DRIVER_H

#include <sys/cdefs.h>
#include <linux/module.h>

#define	module_driver(_drv, _regf, _unregf)					\
static inline int								\
__CONCAT(__CONCAT(_, _drv), _init)(void)					\
{										\
	return (_regf(&(_drv)));		 				\
}										\
										\
static inline void								\
__CONCAT(__CONCAT(_, _drv), _exit)(void)					\
{										\
	_unregf(&(_drv));							\
}										\
										\
module_init(__CONCAT(__CONCAT(_, _drv), _init));				\
module_exit(__CONCAT(__CONCAT(_, _drv), _exit))

#endif	/* LINUXKPI_LINUX_DEVICE_DRIVER_H */
