/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Getz Mikalsen <getz@FreeBSD.org>
*/

#include <string.h>

#undef strcat	/* _FORTIFY_SOURCE */

char *
strcat(char * __restrict s, const char * __restrict append)
{
	char *save = s;

	/* call into SIMD optimized functions */
	stpcpy(s + strlen(s), append);

	return(save);
}
