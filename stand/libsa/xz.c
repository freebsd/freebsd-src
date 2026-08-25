/*
 * Copyright (c) 2026 Netflix, Inc. Written by Warner Losh
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "stand.h"
#include "xz.h"
#include "xz_malloc.h"

void *
xz_malloc(unsigned long size)
{
	return (malloc(size));
}

void
xz_free(void *addr)
{
	free(addr);
}
