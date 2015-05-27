/*-
 * Copyright (c) 2009	Simon L. Nielsen <simon@FreeBSD.org>,
 * 			Bjoern A. Zeeb <bz@FreeBSD.org>
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

static const struct {
	void	*addr;
	int	ok[2];	/* Depending on security.bsd.map_at_zero {0, !=0}. */
} tests[] = {
	{ (void *)0,			{ 0, 1 } }, /* Test sysctl. */
	{ (void *)1,			{ 0, 0 } },
	{ (void *)(PAGE_SIZE - 1),	{ 0, 0 } },
	{ (void *)PAGE_SIZE,		{ 1, 1 } },
	{ (void *)-1,			{ 0, 0 } },
	{ (void *)(-PAGE_SIZE),		{ 0, 0 } },
	{ (void *)(-1 - PAGE_SIZE),	{ 0, 0 } },
	{ (void *)(-1 - PAGE_SIZE - 1),	{ 0, 0 } },
	{ (void *)(0x1000 * PAGE_SIZE),	{ 1, 1 } },
};

#define	MAP_AT_ZERO	"security.bsd.map_at_zero"

int
main(void)
{
	void *p;
	size_t len;
	int i, error, mib[3], map_at_zero;

	error = 0;

	/* Get the current sysctl value of security.bsd.map_at_zero. */
	len = sizeof(mib) / sizeof(*mib);
	if (sysctlnametomib(MAP_AT_ZERO, mib, &len) == -1) {
		printf("1..0 # SKIP: sysctlnametomib(\"%s\") failed: %s\n",
		    MAP_AT_ZERO, strerror(errno));
		return (0);
	}

	len = sizeof(map_at_zero);
	if (sysctl(mib, 3, &map_at_zero, &len, NULL, 0) == -1) {
		printf("1..0 # SKIP: sysctl for %s failed: %s\n", MAP_AT_ZERO,
		    strerror(errno));
		return (0);
	}

	/* Normalize to 0 or 1 for array access. */
	map_at_zero = !!map_at_zero;

	printf("1..%zu\n", nitems(tests));
	for (i = 0; i < (int)nitems(tests); i++) {
		p = mmap((void *)tests[i].addr, PAGE_SIZE,
		    PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_FIXED,
		    -1, 0);
		if (p == MAP_FAILED) {
			if (tests[i].ok[map_at_zero] != 0)
				error++;
			printf("%sok %d # mmap(%p, ...) failed\n",
			    tests[i].ok[map_at_zero] == 0 ? "" : "not ",
			    i + 1,
			    tests[i].addr);
		} else {
			if (tests[i].ok[map_at_zero] != 1)
				error++;
			printf("%sok %d # mmap(%p, ...) succeeded: p=%p\n",
			    tests[i].ok[map_at_zero] == 1 ? "" : "not ",
			    i + 1,
			    tests[i].addr, p);
		}
	}

	return (error != 0);
}
