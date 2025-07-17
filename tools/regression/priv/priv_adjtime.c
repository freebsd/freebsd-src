/*-
 * Copyright (c) 2006 nCircle Network Security, Inc.
 * Copyright (c) 2007 Robert N. M. Watson
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
 */

/*
 * Confirm that privilege is required to invoke adjtime(); first query, then
 * try setting with and without privilege.  Hopefully this will not disturb
 * system time too much.
 */

#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <unistd.h>

#include "main.h"

static int		initialized;
static struct timeval	query_tv;

int
priv_adjtime_setup(int asroot, int injail, struct test *test)
{

	if (initialized)
		return (0);
	if (adjtime(NULL, &query_tv) < 0) {
		warn("priv_adjtime_setup: adjtime(NULL)");
		return (-1);
	}
	initialized = 1;
	return (0);
}

void
priv_adjtime_set(int asroot, int injail, struct test *test)
{
	int error;

	error = adjtime(&query_tv, NULL);
	if (asroot && injail)
		expect("priv_adjtime(asroot, injail)", error, -1, EPERM);
	if (asroot && !injail)
		expect("priv_adjtime(asroot, !injail)", error, 0, 0);
	if (!asroot && injail)
		expect("priv_adjtime(!asroot, injail)", error, -1, EPERM);
	if (!asroot && !injail)
		expect("priv_adjtime(!asroot, !injail)", error, -1, EPERM);
}

void
priv_adjtime_cleanup(int asroot, int injail, struct test *test)
{

}
