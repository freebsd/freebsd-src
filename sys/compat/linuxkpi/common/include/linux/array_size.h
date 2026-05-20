/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * All rights reserved.
 */

#ifndef	_LINUXKPI_LINUX_ARRAY_SIZE_H_
#define	_LINUXKPI_LINUX_ARRAY_SIZE_H_

#include <linux/compiler.h>

#define	ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))

#endif	/* _LINUXKPI_LINUX_ARRAY_SIZE_H_ */
