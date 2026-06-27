/*
 * Copyright (c) 2026 Faraz Vahedi <kfv@kfv.io>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdio.h>
#include <stdlib.h>

int
strfromf(char * __restrict s, size_t n, const char * __restrict fmt, float fp)
{

	return (snprintf(s, n, fmt, fp));
}
