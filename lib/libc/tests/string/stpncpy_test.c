/*-
 * Copyright (c) 2009 David Schultz <das@FreeBSD.org>
 * Copyright (c) 2023 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Robert Clausecker
 * <fuz@FreeBSD.org> under sponsorship from the FreeBSD Foundation.
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
 */

#include <sys/param.h>
#include <sys/mman.h>
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <atf-c.h>

static char *(*stpncpy_fn)(char *restrict, const char *restrict, size_t);

static char *
makebuf(size_t len, int guard_at_end)
{
	char *buf;
	size_t alloc_size, page_size;

	page_size = getpagesize();
	alloc_size = roundup2(len, page_size) + page_size;

	buf = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);
	assert(buf);
	if (guard_at_end) {
		assert(munmap(buf + alloc_size - page_size, page_size) == 0);
		return (buf + alloc_size - page_size - len);
	} else {
		assert(munmap(buf, page_size) == 0);
		return (buf + page_size);
	}
}

static void
test_stpncpy(const char *s)
{
	char *src, *dst;
	size_t size, len, bufsize, x;
	int i, j;

	size = strlen(s) + 1;
	for (i = 0; i <= 1; i++) {
		for (j = 0; j <= 1; j++) {
			for (bufsize = 0; bufsize <= size + 10; bufsize++) {
				src = makebuf(size, i);
				memcpy(src, s, size);
				dst = makebuf(bufsize, j);
				memset(dst, 'X', bufsize);
				len = (bufsize < size) ? bufsize : size - 1;
				assert(stpncpy_fn(dst, src, bufsize) == dst+len);
				assert(memcmp(src, dst, len) == 0);
				for (x = len; x < bufsize; x++)
					assert(dst[x] == '\0');
			}
		}
	}
}

static void
test_sentinel(char *dest, char *src, size_t destlen, size_t srclen)
{
	size_t i;
	const char *res, *wantres;
	const char *fail = NULL;

	for (i = 0; i < srclen; i++)
		/* src will never include (){} */
		src[i] = '0' + i;
	src[srclen] = '\0';

	/* source sentinels: not to be copied */
	src[-1] = '(';
	src[srclen+1] = ')';

	memset(dest, 0xee, destlen);

	/* destination sentinels: not to be touched */
	dest[-1] = '{';
	dest[destlen] = '}';

	wantres = dest + (srclen > destlen ? destlen : srclen);
	res = stpncpy_fn(dest, src, destlen);

	if (dest[-1] != '{')
		fail = "start sentinel overwritten";
	else if (dest[destlen] != '}')
		fail = "end sentinel overwritten";
	else if (strncmp(src, dest, destlen) != 0)
		fail = "string not copied correctly";
	else if (res != wantres)
		fail = "incorrect return value";
	else for (i = srclen; i < destlen; i++)
		if (dest[i] != '\0') {
			fail = "incomplete NUL padding";
			break;
		}

	if (fail)
		atf_tc_fail_nonfatal("%s\n"
		    "stpncpy(%p \"%s\", %p \"%s\", %zu) = %p (want %p)\n",
		    fail, dest, dest, src, src, destlen, res, wantres);
}

ATF_TC_WITHOUT_HEAD(null);
ATF_TC_BODY(null, tc)
{
	ATF_CHECK_EQ(stpncpy_fn(NULL, NULL, 0), NULL);
}

ATF_TC_WITHOUT_HEAD(bounds);
ATF_TC_BODY(bounds, tc)
{
	size_t i;
	char buf[64+1];

	for (i = 0; i < sizeof(buf) - 1; i++) {
		buf[i] = ' ' + i;
		buf[i+1] = '\0';
		test_stpncpy(buf);
	}
}

ATF_TC_WITHOUT_HEAD(alignments);
ATF_TC_BODY(alignments, tc)
{
	size_t srcalign, destalign, srclen, destlen;
	char src[15+3+64]; /* 15 offsets + 64 max length + NUL + sentinels */
	char dest[15+2+64]; /* 15 offsets + 64 max length + sentinels */

	for (srcalign = 0; srcalign < 16; srcalign++)
		for (destalign = 0; destalign < 16; destalign++)
			for (srclen = 0; srclen < 64; srclen++)
				for (destlen = 0; destlen < 64; destlen++)
					test_sentinel(dest+destalign+1,
					    src+srcalign+1, destlen, srclen);
}

ATF_TP_ADD_TCS(tp)
{
	void *dl_handle;

	dl_handle = dlopen(NULL, RTLD_LAZY);
	stpncpy_fn = dlsym(dl_handle, "test_stpncpy");
	if (stpncpy_fn == NULL)
		stpncpy_fn = stpncpy;

	ATF_TP_ADD_TC(tp, null);
	ATF_TP_ADD_TC(tp, bounds);
	ATF_TP_ADD_TC(tp, alignments);

	return (atf_no_error());
}
