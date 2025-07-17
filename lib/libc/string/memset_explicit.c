/*-
 * SPDF-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Robert Clausecker <fuz@FreeBSD.org>
 */

#include <string.h>
#include <ssp/ssp.h>

__attribute__((weak)) void __memset_explicit_hook(void *, int, size_t);

__attribute__((weak)) void
__memset_explicit_hook(void *buf, int ch, size_t len)
{
	(void)buf;
	(void)ch;
	(void)len;
}

void *
__ssp_real(memset_explicit)(void *buf, int ch, size_t len)
{
	memset(buf, ch, len);
	__memset_explicit_hook(buf, ch, len);

	return (buf);
}
