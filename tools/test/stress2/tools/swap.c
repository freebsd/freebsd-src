/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Peter Holm <pho@FreeBSD.org>
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
 */

#include <sys/types.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <vm/vm_param.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define INCARNATIONS 32

static unsigned long size, original;
static int runtime, utime;

void
usage(void)
{
	fprintf(stderr, "Usage: %s [-d delay] [-p pct] [-t runtime] "
	    "[-v]\n",
	    getprogname());
	exit(1);
}

static unsigned long
usermem(void)
{
	unsigned long mem;
	size_t nlen = sizeof(mem);

	if (sysctlbyname("hw.usermem", &mem, &nlen, NULL, 0) == -1)
		err(1, "sysctlbyname() %s:%d", __FILE__, __LINE__);

#if defined(DEBUG)
	printf("Total free user memory %lu Mb\n",
		mem / 1024 / 1024);
#endif

	return (mem);
}
static int64_t
swap(void)
{
	struct xswdev xsw;
	size_t mibsize, size;
	int mib[16], n;
	int64_t sz;

	mibsize = sizeof mib / sizeof mib[0];
	sz = 0;

	if (sysctlnametomib("vm.swap_info", mib, &mibsize) == -1)
		err(1, "sysctlnametomib()");

	for (n = 0; ; ++n) {
		mib[mibsize] = n;
		size = sizeof xsw;
		if (sysctl(mib, mibsize + 1, &xsw, &size, NULL, 0) == -1)
			break;
		if (xsw.xsw_version != XSWDEV_VERSION)
			errx(1, "xswdev version mismatch");
		sz = sz + xsw.xsw_nblks - xsw.xsw_used;
	}
	if (errno != ENOENT)
		err(1, "sysctl()");

#if defined(DEBUG)
	printf("Total free swap space %jd Mb\n",
		sz * getpagesize() / 1024 / 1024);
#endif

	return (sz * getpagesize());
}

static void
setup(void)
{
	struct rlimit rlp;

	size = size / INCARNATIONS;
	original = size;
	if (size == 0)
		errx(1, "Argument too small");

	if (getrlimit(RLIMIT_DATA, &rlp) < 0)
		err(1,"getrlimit");
	rlp.rlim_cur -= 1024 * 1024;

	if (size > (unsigned long)rlp.rlim_cur)
		size = rlp.rlim_cur;

#if 0
	printf("setup: pid %d. Total %luMb\n",
		getpid(), size / 1024 / 1024 * INCARNATIONS);
#endif

	if (size == 0)
		errx(1, "Argument too small");

	return;
}

static int
test(void)
{
	volatile char *c;
	int page;
	unsigned long i, j;
	time_t start;

	c = malloc(size);
	while (c == NULL) {
		size -=  1024 * 1024;
		c = malloc(size);
	}
	if (size != original)
		printf("Malloc size changed from %ld Mb to %ld Mb\n",
		    original / 1024 / 1024, size / 1024 / 1024);
	page = getpagesize();
	start = time(NULL);
	while ((time(NULL) - start) < runtime) {
		i = j = 0;
		while (i < size) {
			c[i] = 1;
			i += page;
			if ((time(NULL) - start) >= runtime)
				break;
			usleep(utime);
		}
	}
	free((void *)c);

	_exit(0);
}

int
main(int argc, char **argv)
{
	int64_t s;
	pid_t pids[INCARNATIONS];
	unsigned long u;
	int ch, i, pct, verbose;

	s = swap();
	u = usermem();

	runtime = 120;	/* 2 minutes */
	utime = 1000;	/* 0.001 sec */
	verbose = 0;
	if (s == 0)
		pct = 80;
	else
		pct = 101;
	while ((ch = getopt(argc, argv, "d:p:t:v")) != -1) {
		switch(ch) {
		case 'd':	/* delay in usec */
			if (sscanf(optarg, "%d", &utime) != 1)
				usage();
			break;
		case 'p':	/* % of usermem */
			if (sscanf(optarg, "%d", &pct) != 1)
				usage();
			break;
		case 't':	/* runtime in sec*/
			if (sscanf(optarg, "%d", &runtime) != 1)
				usage();
			break;
		case 'v':	/* verbose*/
			verbose = 1;
			break;
		default:
			usage();
		}
	}

	if (s == 0 &&  pct > 80)
		errx(1, "pct range with no swap is 0-80");
	size = u / 100 * pct;
	if (verbose == 1)
		fprintf(stderr, "pct = %d, sleep = %d usec, size = %lumb\n",
		    pct, utime, size / 1024 / 1024);
	setup();

	for (i = 0; i < INCARNATIONS; i++)
		if ((pids[i] = fork()) == 0)
			test();

	for (i = 0; i < INCARNATIONS; i++)
		if (waitpid(pids[i], NULL, 0) != pids[i])
			err(1, "paitpid(%d)", pids[i]);

	return (0);
}
