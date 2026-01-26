/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2026 The FreeBSD Foundation
 *
 * This software were developed by
 * Konstantin Belousov <kib@FreeBSD.org> under sponsorship from
 * the FreeBSD Foundation.
 */

#include <sys/types.h>
#include <sys/procdesc.h>
#include <errno.h>
#include <unistd.h>

pid_t
pdrfork_thread(int *fdp, int pdflags, int rfflags, void *stack_addr,
    int (*start_fn)(void *), void *arg)
{
	pid_t res;
	int ret;

	/* See comment in rfork_thread_gen.c. */
	if (stack_addr != NULL) {
		errno = EOPNOTSUPP;
		return (-1);
	}
	res = pdrfork(fdp, pdflags, rfflags);
	if (res == 0) {
		ret = start_fn(arg);
		_exit(ret);
	}
	return (res);
}
