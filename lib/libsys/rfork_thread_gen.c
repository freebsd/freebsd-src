/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2026 The FreeBSD Foundation
 *
 * This software were developed by
 * Konstantin Belousov <kib@FreeBSD.org> under sponsorship from
 * the FreeBSD Foundation.
 */

#include <errno.h>
#include <unistd.h>

pid_t
rfork_thread(int flags, void *stack_addr, int (*start_fn)(void *), void *arg)
{
	pid_t res;
	int ret;

	/*
	 * Generic implementation cannot switch stacks.  Only
	 * architecture-specific code knows how to do it.  Require
	 * that caller knows that, and refuse to do operate if the
	 * stack was supplied.
	 *
	 * Note that implementations that do switch stack, would fault
	 * immediately if the passed stack is NULL.  They do not need to
	 * specifically check for the NULL stack value.
	 */
	if (stack_addr != NULL) {
		errno = EOPNOTSUPP;
		return (-1);
	}
	res = rfork(flags);
	if (res == 0) {
		ret = start_fn(arg);
		_exit(ret);
	}
	return (res);
}
