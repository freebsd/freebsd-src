/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Jean-Sébastien Pédron
 */

#ifndef	_LINUXKPI_LINUX_WORDPART_H_
#define	_LINUXKPI_LINUX_WORDPART_H_

#define	lower_32_bits(n)		((u32)(n))
#define	upper_32_bits(n)		((u32)(((n) >> 16) >> 16))

#endif	/* _LINUXKPI_LINUX_WORDPART_H_ */
