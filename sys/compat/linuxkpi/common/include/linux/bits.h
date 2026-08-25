/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016 Kip Macy
 * Copyright (c) 2018 Johannes Lundberg
 */

#ifndef _LINUXKPI_LINUX_BITS_H_
#define	_LINUXKPI_LINUX_BITS_H_

#define	GENMASK(h, l)		(((~0UL) >> (BITS_PER_LONG - (h) - 1)) & ((~0UL) << (l)))
#define	GENMASK_ULL(h, l)	(((~0ULL) >> (BITS_PER_LONG_LONG - (h) - 1)) & ((~0ULL) << (l)))

#endif /* _LINUXKPI_LINUX_BITS_H_ */
