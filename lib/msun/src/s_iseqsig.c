/*
 * Copyright (c) 2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <fenv.h>
#include <float.h>
#include <math.h>

int
__iseqsig(double x, double y)
{
	if (isunordered(x, y)) {
		feraiseexcept(FE_INVALID);
		return (0);
	}
	return (x == y);
}

#if (LDBL_MANT_DIG == 53)
__weak_reference(__iseqsig, __iseqsigl);
#endif
