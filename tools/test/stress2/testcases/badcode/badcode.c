/*
 *             COPYRIGHT (c) 1990 BY             *
 *  GEORGE J. CARRETTE, CONCORD, MASSACHUSETTS.  *
 *             ALL RIGHTS RESERVED               *

Permission to use, copy, modify, distribute and sell this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all copies
and that both that copyright notice and this permission notice appear
in supporting documentation, and that the name of the author
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
HE BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

This code is based on crashme.c

*/

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "stress.h"

static pid_t pid;
static int failsafe;

static int
tobemangled(void) {
	volatile int i, j;

	j = 2;
	for (i = 0; i < 100; i++)
		j = j + 3;
	j = j / 2;

	return (j);
}

static void
mangle(void) {	/* Change one byte in the code */
	int i;
	char *p = (void *)tobemangled;

	i = arc4random() % 50;
	p[i] = arc4random() & 0xff;
}

static void
hand(int i __unused) {	/* handler */
	_exit(1);
}

static void
ahand(int i __unused) {	/* alarm handler */
	if (pid != 0) {
		kill(pid, SIGKILL);
	}
	_exit(EXIT_SUCCESS);
}

int
setup(int nb __unused)
{
	return (0);
}

void
cleanup(void)
{
}

int
test(void)
{
	void *st;
	struct rlimit rl;

	if (failsafe)
		return (0);

	while (done_testing == 0) {
		signal(SIGALRM, ahand);
		alarm(3);
		if ((pid = fork()) == 0) {
			rl.rlim_max = rl.rlim_cur = 0;
			if (setrlimit(RLIMIT_CORE, &rl) == -1)
				warn("setrlimit");
			st = (void *)trunc_page((unsigned long)tobemangled);
			if (mprotect(st, PAGE_SIZE, PROT_WRITE | PROT_READ |
			    PROT_EXEC) == -1)
				err(1, "mprotect()");

			signal(SIGALRM, hand);
			signal(SIGILL,  hand);
			signal(SIGFPE,  hand);
			signal(SIGSEGV, hand);
			signal(SIGBUS,  hand);
			signal(SIGURG,  hand);
			signal(SIGSYS,  hand);
			signal(SIGTRAP, hand);

			mangle();
			failsafe = 1;
			(void)tobemangled();

			_exit(EXIT_SUCCESS);

		} else if (pid > 0) {
			if (waitpid(pid, NULL, 0) == -1)
				warn("waitpid(%d)", pid);
			alarm(0);
		} else
			err(1, "fork(), %s:%d",  __FILE__, __LINE__);
	}

	return (0);
}
