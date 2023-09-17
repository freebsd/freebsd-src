/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 The FreeBSD Foundation
 *
 * This software was developed by Mark Johnston under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
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
 */

#include <sys/capsicum.h>
#include <stdio.h>
#include <stdlib.h>

#include <atf-c.h>
#include <sysdecode.h>

/*
 * Take a comma-separated list of capability rights and verify that all rights
 * are present in the specified table, and that all rights in the table are
 * present in the list.
 */
static void
check_sysdecode_cap_rights(FILE *fp, char **bufp, size_t *szp,
    cap_rights_t *rightsp, const char *tab[])
{
	const char *next, *tok;
	char *buf;
	int i;

	sysdecode_cap_rights(fp, rightsp);

	ATF_REQUIRE(fflush(fp) == 0);
	(*bufp)[*szp] = '\0';

	buf = strdup(*bufp);
	for (tok = buf; (next = strsep(&buf, ",")), tok != NULL; tok = next) {
		for (i = 0; tab[i] != NULL; i++) {
			if (strcmp(tok, tab[i]) == 0)
				break;
		}
		ATF_REQUIRE_MSG(tab[i] != NULL,
		    "did not find '%s' in table", tok);
	}
	free(buf);

	for (i = 0; tab[i] != NULL; i++) {
		buf = strdup(*bufp);
		for (tok = buf; (next = strsep(&buf, ",")), tok != NULL;
		    tok = next) {
			if (strcmp(tok, tab[i]) == 0)
				break;
		}
		free(buf);
		ATF_REQUIRE_MSG(tok != NULL,
		    "did not find '%s' in output stream", tab[i]);
	}

	ATF_REQUIRE(fseek(fp, 0, SEEK_SET) == 0);
}

/*
 * Regression tests for sysdecode_cap_rights(3).
 */
ATF_TC_WITHOUT_HEAD(cap_rights);
ATF_TC_BODY(cap_rights, tc)
{
	char *buf;
	FILE *fp;
	size_t sz;
	cap_rights_t rights;

	fp = open_memstream(&buf, &sz);
	ATF_REQUIRE(fp != NULL);

	/*
	 * libsysdecode emits a pseudo-right, CAP_NONE, when no rights are
	 * present.
	 */
	check_sysdecode_cap_rights(fp, &buf, &sz,
	    cap_rights_init(&rights),
	    (const char *[]){ "CAP_NONE", NULL, });

	check_sysdecode_cap_rights(fp, &buf, &sz,
	    cap_rights_init(&rights, CAP_READ, CAP_SEEK),
	    (const char *[]){ "CAP_PREAD", NULL, });

	check_sysdecode_cap_rights(fp, &buf, &sz,
	    cap_rights_init(&rights, CAP_READ, CAP_MMAP, CAP_SEEK_TELL),
	    (const char *[]){ "CAP_READ", "CAP_MMAP", "CAP_SEEK_TELL", NULL, });

	check_sysdecode_cap_rights(fp, &buf, &sz,
	    cap_rights_init(&rights, CAP_MMAP, CAP_READ, CAP_WRITE, CAP_SEEK),
	    (const char *[]){ "CAP_MMAP_RW", NULL, });

	check_sysdecode_cap_rights(fp, &buf, &sz,
	    cap_rights_init(&rights, CAP_READ, CAP_MMAP_X),
	    (const char *[]){ "CAP_MMAP_RX", NULL, });

	/* Aliases map back to the main definition. */
	check_sysdecode_cap_rights(fp, &buf, &sz,
	    cap_rights_init(&rights, CAP_RECV, CAP_SEND),
	    (const char *[]){ "CAP_READ", "CAP_WRITE", NULL, });

	/* This set straddles both indices. */
	check_sysdecode_cap_rights(fp, &buf, &sz,
	    cap_rights_init(&rights, CAP_READ, CAP_KQUEUE),
	    (const char *[]){ "CAP_READ", "CAP_KQUEUE", NULL, });

	/* Create a rights set with an unnamed flag. */
	cap_rights_init(&rights, CAP_SEEK);
	cap_rights_clear(&rights, CAP_SEEK_TELL);
	check_sysdecode_cap_rights(fp, &buf, &sz,
	    &rights,
	    (const char *[]){ "CAP_NONE", "unknown rights", NULL, });

	cap_rights_init(&rights, CAP_SEEK, CAP_KQUEUE_CHANGE);
	cap_rights_clear(&rights, CAP_SEEK_TELL);
	check_sysdecode_cap_rights(fp, &buf, &sz,
	    &rights,
	    (const char *[]){ "CAP_KQUEUE_CHANGE", "unknown rights", NULL, });

	ATF_REQUIRE(fclose(fp) == 0);
	free(buf);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, cap_rights);

	return (atf_no_error());
}
