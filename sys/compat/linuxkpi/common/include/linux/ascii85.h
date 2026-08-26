/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Foundation
 */

#ifndef	_LINUXKPI_LINUX_ASCII85_H_
#define	_LINUXKPI_LINUX_ASCII85_H_

#include <sys/param.h>

#define	ASCII85_BUFSZ 6

static inline long
ascii85_encode_len(long in_len)
{
	long out_len;

	out_len = howmany(in_len, 4);

	return (out_len);
}

static inline const char *
ascii85_encode(uint32_t in, char *out)
{
	int i;

	if (in == 0) {
		out[0] = 'z';
		out[1] = '\0';
		return (out);
	}

	for (i = ASCII85_BUFSZ - 2; i >= 0; i--) {
		out[i] = in % 85;
		out[i] += 33;

		in /= 85;
	}
	out[ASCII85_BUFSZ - 1] = '\0';

	return (out);
}

#endif /* _LINUXKPI_LINUX_ASCII85_H_ */
