/* Copyright (c) 2026, Vsevolod Stakhov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *       * Redistributions of source code must retain the above copyright
 *         notice, this list of conditions and the following disclaimer.
 *       * Redistributions in binary form must reproduce the above copyright
 *         notice, this list of conditions and the following disclaimer in the
 *         documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ucl_internal.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void
test_strnstr (void)
{
	char *partial = malloc (3);
	char *exact = malloc (3);
	char *single = malloc (1);

	assert (partial != NULL && exact != NULL && single != NULL);
	memcpy (partial, "x:/", 3);
	memcpy (exact, "://", 3);
	single[0] = 'x';

	assert (ucl_strnstr (partial, "://", 3) == NULL);
	assert (ucl_strnstr (exact, "://", 3) == exact);
	assert (ucl_strnstr (single, "x", 1) == single);
	assert (ucl_strnstr (single, "x", 0) == NULL);
	assert (ucl_strnstr (single, "", 0) == single);

	free (partial);
	free (exact);
	free (single);
}

static void
test_strncasestr (void)
{
	char *partial = malloc (3);
	char *exact = malloc (3);
	char *single = malloc (1);

	assert (partial != NULL && exact != NULL && single != NULL);
	memcpy (partial, "xA/", 3);
	memcpy (exact, "aBc", 3);
	single[0] = 'X';

	assert (ucl_strncasestr (partial, "a//", 3) == NULL);
	assert (ucl_strncasestr (exact, "AbC", 3) == exact);
	assert (ucl_strncasestr (single, "x", 1) == single);
	assert (ucl_strncasestr (single, "x", 0) == NULL);
	assert (ucl_strncasestr (single, "", 0) == single);

	free (partial);
	free (exact);
	free (single);
}

int
main (void)
{
	test_strnstr ();
	test_strncasestr ();

	return 0;
}
