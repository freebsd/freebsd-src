/*
 * Copyright (c) 2025 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdint.h>
#include <stdlib.h>

size_t
memalignment(const void *p)
{
	uintptr_t align;

	if (p == NULL)
		return (0);

	align = (uintptr_t)p;
	align &= -align;

#if UINTPTR_MAX > SIZE_MAX
	/* if alignment overflows size_t, return maximum possible */
	if (align > SIZE_MAX)
		align = SIZE_MAX - SIZE_MAX/2;
#endif

	return (align);
}
