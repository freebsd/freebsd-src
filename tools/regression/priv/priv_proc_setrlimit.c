/*-
 * Copyright (c) 2006 nCircle Network Security, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert N. M. Watson for the TrustedBSD
 * Project under contract to nCircle Network Security, Inc.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR, NCIRCLE NETWORK SECURITY,
 * INC., OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Test that raising current resource limits above hard resource limits
 * requires privilege.  There is one privilege check, but two conditions:
 *
 * - To raise the current above the maximum.
 *
 * - To raise the maximum.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <err.h>
#include <errno.h>
#include <unistd.h>

#include "main.h"

void
priv_proc_setrlimit(void)
{
	struct rlimit rl, rl_lower, rl_raise_max, rl_raise_cur;
	int error;

	assert_root();

	/*
	 * To make sure that there is room to raise the resource limit, we
	 * must first lower it.  Otherwise, if the resource limit is already
	 * at the global maximum, that complicates matters.  In principle, we
	 * can bump into privilege failures during setup, but there's not
	 * much we can do about that.  Keep this prototypical setting around
	 * as the target to restore to later.
	 */
	if (getrlimit(RLIMIT_DATA, &rl) < 0)
		err(-1, "getrlimit(RLIMIT_DATA)");

	/*
	 * What to lower to before trying to raise.
	 */
	rl_lower = rl;
	rl_lower.rlim_cur -= 10;
	rl_lower.rlim_max = rl_lower.rlim_cur;

	/*
	 * Raise the maximum.
	 */
	rl_raise_max = rl;
	rl_raise_max.rlim_max += 10;

	/*
	 * Raise the current above the maximum.
	 */
	rl_raise_cur = rl;
	rl_raise_cur.rlim_cur += 10;

	/*
	 * Test raising the maximum with privilege.
	 */
	if (setrlimit(RLIMIT_DATA, &rl_lower) < 0)
		err(-1, "setrlimit(RLIMIT_DATA, lower) as root");

	if (setrlimit(RLIMIT_DATA, &rl_raise_max) < 0)
		err(-1, "setrlimit(RLIMIT_DATA, raise_max) as root");

	/*
	 * Test raising the current above the maximum with privilege.
	 */
	if (setrlimit(RLIMIT_DATA, &rl_lower) < 0)
		err(-1, "setrlimit(RLIMIT_DATA, lower) as root");

	if (setrlimit(RLIMIT_DATA, &rl_raise_cur) < 0)
		err(-1, "setrlimit(RLIMIT_DATA, raise_cur) as root");

	/*
	 * Test raising the maximum without privilege.
	 */
	if (setrlimit(RLIMIT_DATA, &rl_lower) < 0)
		err(-1, "setrlimit(RLIMIT_DATA, lower) as root");

	set_euid(UID_OTHER);
	error = setrlimit(RLIMIT_DATA, &rl_raise_max);
	if (error == 0)
		errx(-1,
		    "setrlimit(RLIMIT_DATA, raise_max) succeeded as !root");
	if (errno != EPERM)
		err(-1, "setrlimit(RLIMIT_DATA, raise_max) wrong errno %d "
		    "as !root", errno);

	/*
	 * Test raising the current above the maximum without privilege.
	 */
	set_euid(UID_ROOT);
	if (setrlimit(RLIMIT_DATA, &rl_lower) < 0)
		err(-1, "setrlimit(RLIMIT_DATA, lower) as root");
	set_euid(UID_OTHER);

	error = setrlimit(RLIMIT_DATA, &rl_raise_cur);
	if (error == 0)
		errx(-1,
		    "setrlimit(RLIMIT_DATA, raise_cur) succeeded as !root");
	if (errno != EPERM)
		err(-1, "setrlimit(RLIMIT_DATA, raise_cur) wrong errno %d "
		    "as !root", errno);
}
