/*
 * Copyright (c) 2022-2026 Bjoern A. Zeeb
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef	_LINUXKPI_LINUX_HEX_H_
#define	_LINUXKPI_LINUX_HEX_H_

#include <linux/types.h>
#include <linux/errno.h>

static inline int
_h2b(const char c)
{

	if (c >= '0' && c <= '9')
		return (c - '0');
	if (c >= 'a' && c <= 'f')
		return (10 + c - 'a');
	if (c >= 'A' && c <= 'F')
		return (10 + c - 'A');
	return (-EINVAL);
}

static inline int
hex2bin(uint8_t *bindst, const char *hexsrc, size_t binlen)
{
	int hi4, lo4;

	while (binlen > 0) {
		hi4 = _h2b(*hexsrc++);
		lo4 = _h2b(*hexsrc++);
		if (hi4 < 0 || lo4 < 0)
			return (-EINVAL);

		*bindst++ = (hi4 << 4) | lo4;
		binlen--;
	}

	return (0);
}

#endif	/* _LINUXKPI_LINUX_HEX_H_ */
