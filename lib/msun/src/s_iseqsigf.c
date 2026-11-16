/*
 * Copyright (c) 2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <fenv.h>
#include <math.h>

int
__iseqsigf(float x, float y)
{
	if (isunordered(x, y)) {
		feraiseexcept(FE_INVALID);
		return (0);
	}
	return (x == y);
}
