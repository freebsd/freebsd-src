/*
 * Copyright (c) 2026 Faraz Vahedi <kfv@kfv.io>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/cdefs.h>
#include <stdlib.h>

void	__free(void *);

void
__free_aligned_sized(void *ptr, size_t alignment, size_t size)
{

	(void)alignment;
	(void)size;
	__free(ptr);
}

__weak_reference(__free_aligned_sized, free_aligned_sized);
