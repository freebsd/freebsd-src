/*
 * Copyright (c) 2013-2018, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "errcode.h"
#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *duplicate_str(const char *s)
{
	char *dup;

	if (!s)
		return NULL;

	dup = malloc(strlen(s)+1);
	if (!dup)
		return NULL;
	return strcpy(dup, s);
}

int str_to_uint64(const char *str, uint64_t *val, int base)
{
	char *endptr;
	uint64_t x;

	if (!str || !val)
		return -err_internal;

	errno = 0;
	x = strtoull(str, &endptr, base);

	if (errno == EINVAL)
		return -err_parse_int;

	if (errno == ERANGE)
		return -err_parse_int_too_big;

	if (str == endptr || *endptr != '\0')
		return -err_parse_int;

	*val = x;

	return 0;
}

int str_to_uint32(const char *str, uint32_t *val, int base)
{
	uint64_t x;
	int errcode;

	if (!str || !val)
		return -err_internal;

	errcode = str_to_uint64(str, &x, base);
	if (errcode < 0)
		return errcode;

	if (UINT32_MAX < x)
		return -err_parse_int_too_big;

	*val = (uint32_t) x;
	return 0;
}

int str_to_uint16(const char *str, uint16_t *val, int base)
{
	uint64_t x;
	int errcode;

	if (!str || !val)
		return -err_internal;

	errcode = str_to_uint64(str, &x, base);
	if (errcode < 0)
		return errcode;

	if (UINT16_MAX < x)
		return -err_parse_int_too_big;

	*val = (uint16_t) x;
	return 0;
}

int str_to_uint8(const char *str, uint8_t *val, int base)
{
	uint64_t x;
	int errcode;

	if (!str || !val)
		return -err_internal;

	errcode = str_to_uint64(str, &x, base);
	if (errcode < 0)
		return errcode;

	if (UINT8_MAX < x)
		return -err_parse_int_too_big;

	*val = (uint8_t) x;
	return 0;
}

int do_bug_on(int cond, const char *condstr, const char *file, int line)
{
	if (cond)
		fprintf(stderr, "%s:%d: internal error: %s\n", file, line,
			condstr);
	return cond;
}
struct label *l_alloc(void)
{
	return calloc(1, sizeof(struct label));
}

void l_free(struct label *l)
{
	if (!l)
		return;

	l_free(l->next);
	free(l->name);
	free(l);
}

int l_append(struct label *l, const char *name, uint64_t addr)
{
	int errcode;

	if (bug_on(!l))
		return -err_internal;

	if (bug_on(!name))
		return -err_internal;

	/* skip to the last label.  */
	while (l->next) {
		l = l->next;

		/* ignore the first label, which has no name. */
		if (strcmp(l->name, name) == 0)
			return -err_label_not_unique;
	}

	/* append a new label.  */
	l->next = l_alloc();
	if (!l->next)
		return -err_no_mem;

	/* save the name.  */
	l->next->name = duplicate_str(name);
	if (!l->next->name) {
		errcode = -err_no_mem;
		goto error;
	}

	/* save the address.  */
	l->next->addr = addr;

	return 0;
error:
	free(l->next->name);
	free(l->next);
	l->next = NULL;
	return errcode;
}

int l_lookup(const struct label *l, uint64_t *addr,
		    const char *name)
{
	if (bug_on(!l))
		return -err_internal;

	if (bug_on(!addr))
		return -err_internal;

	if (bug_on(!name))
		return -err_internal;


	*addr = 0;
	while (l->next) {
		l = l->next;
		if (strcmp(l->name, name) == 0) {
			*addr = l->addr;
			return 0;
		}
	}
	return -err_no_label;
}

struct label *l_find(struct label *l, const char *name)
{
	if (bug_on(!l))
		return NULL;

	if (bug_on(!name))
		return NULL;


	while (l->next) {
		l = l->next;

		if (bug_on(!l->name))
			continue;

		if (strcmp(l->name, name) == 0)
			return l;
	}
	return NULL;
}
