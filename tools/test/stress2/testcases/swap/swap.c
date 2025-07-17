/*-
 * Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
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
 */

#include <sys/types.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "stress.h"

#if defined(__LP64__)
#define MINLEFT (1792LL * 1024 * 1024)
#else
#define MINLEFT (1024LL * 1024 * 1024)
#endif

static int64_t size;

int
setup(int nb)
{
	struct rlimit rlp;
	int64_t  mem, swapinfo;
	int mi, mx, pct;
	char *cp;

	if (nb == 0) {
		mem = usermem();
		swapinfo = swap();

		pct = 0;
		if (op->hog == 0) {
			mi = 80;
			mx = 100;
		}

		if (op->hog == 1) {
			mi = 100;
			mx = 110;
		}

		if (op->hog == 2) {
			mi = 110;
			mx = 120;
		}

		if (op->hog >= 3) {
			mi = 120;
			mx = 130;
		}
		if ((cp = getenv("MAXSWAPPCT")) != NULL && *cp != '\0') {
			mx = atoi(cp);
			mi = mx - 10;
		}
		pct = random_int(mi, mx);

		if (swapinfo == 0) {
			pct = random_int(30, 50);
			if (mem <= MINLEFT) {
				putval(0);
				_exit(1);
			}
			mem -= MINLEFT;
			size = mem / 100 * pct;
		} else {
			size = mem / 100 * pct;
			if (size > mem + swapinfo / 4) {
				size = mem + swapinfo / 4;
				pct = size * 100 / mem;
			}
		}

		size = size / op->incarnations;

		if (getrlimit(RLIMIT_DATA, &rlp) < 0)
			err(1,"getrlimit");
		rlp.rlim_cur -= 1024 * 1024;

		if (size > rlp.rlim_cur)
			size = rlp.rlim_cur;
		putval(size);

		if (op->verbose > 1 && nb == 0)
			printf("setup: pid %d, %d%%. Total %dMb, %d thread(s).\n",
			    getpid(), pct, (int)(size / 1024 / 1024 *
			    op->incarnations), op->incarnations);
	} else
		size = getval();

	if (size == 0)
		exit(1);

	return (0);
}

void
cleanup(void)
{
}

int
test(void)
{
	time_t start;
	int64_t i, oldsize;
	int page;
	char *c;

	if (size == 0)
		return (0);
	oldsize = size;
	c = malloc(size);
	while (c == NULL && done_testing == 0) {
		size -=  1024 * 1024;
		c = malloc(size);
	}
	if (op->verbose > 1 && size != oldsize)
		printf("Malloc size changed from %d Mb to %d Mb\n",
		    (int)(oldsize / 1024 / 1024), (int)(size / 1024 / 102));
	page = getpagesize();
	start = time(NULL);	/* Livelock workaround */
	while (done_testing == 0 &&
			(time(NULL) - start) < op->run_time) {
		i = 0;
		while (i < size && done_testing == 0) {
			c[i] = 0;
			i += page;
		}
		if (arc4random() % 100 < 10)
			usleep(10000);
	}
	free((void *)c);

	return (0);
}
