/*-
 * Copyright (c) 2006 Joseph Koshy
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: hash.m4 2191 2011-11-21 08:34:02Z jkoshy $
 */

#include <sys/types.h>

#include <ctype.h>
#include <libelf.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tet_api.h"

/*
 * Test the `elf_hash' API.
 */

/*
 * A motley collection of test strings.
 */
static struct htab {
	const char	*s;
	unsigned long	h;
} htab[] = {
#undef	H
#define	H(S,V) { .s = (S), .h = (V) }
	H("",			0),
	H("\377\377\377\377",	0x10FFEfL),
	H("\030\2265Q\023_;\312\214\212#f\001\220\224|",
				0xe07d55c),
	H("elf-hash",		0x293ee58),
	H(NULL,			0)
};

static void
to_printable_string(char *dst, const char *src)
{
	int c;
	char *s;
	
	s = dst;
	while (c = *src++) {
		if (isprint(c))
			*s++ = c;
		else
			s += sprintf(s, "\\%3.3o", (c & 0xFF));
	}
	*s = '\0';
}

void
tpCheckHash(void)
{
	unsigned long h;
	struct htab *ht;
	int result;
	char *tmp;

	tet_infoline("assertion: check elf_hash() against several constant "
	    "strings.");

	result = TET_PASS;
	for (ht = htab; ht->s; ht++) {
		if ((h = elf_hash(ht->s)) != ht->h) {
			if ((tmp = malloc(4 * strlen(ht->s) + 1)) != NULL) {
				to_printable_string(tmp, ht->s);
				tet_printf("fail: elf_hash(\"%s\") = 0x%x != "
				    "expected 0x%x.", tmp, h, ht->h);
				free(tmp);
			}
			result = TET_FAIL;
		}
	}
	tet_result(result);
}
