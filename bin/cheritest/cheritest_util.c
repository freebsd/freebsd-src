/*-
 * Copyright (c) 2014 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#include <sys/cdefs.h>

#if !__has_feature(capabilities)
#error "This code requires a CHERI-aware compiler"
#endif

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/ucontext.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "cheritest.h"

static void
vcheritest_failure_errx(const char *msg, va_list ap)
{

	ccsp->ccs_testresult = TESTRESULT_FAILURE;
	vsnprintf(ccsp->ccs_testresult_str, sizeof(ccsp->ccs_testresult_str),
	    msg, ap);
}

static void
vcheritest_failure_err(const char *msg, va_list ap)
{
	size_t buflen;
	int len;

	ccsp->ccs_testresult = TESTRESULT_FAILURE;
	buflen = sizeof(ccsp->ccs_testresult_str);
	len = vsnprintf(ccsp->ccs_testresult_str, buflen, msg, ap);
	if (len < 0)
		return;
	if ((size_t)len >= buflen)	/* No room for further strings. */
		return;
	snprintf(ccsp->ccs_testresult_str + len, buflen - len, ": %s",
	    strerror(errno));
}

void
cheritest_failure_errx(const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	vcheritest_failure_errx(msg, ap);
	va_end(ap);
	exit(EX_SOFTWARE);
}

void
cheritest_failure_err(const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	vcheritest_failure_err(msg, ap);
	va_end(ap);
	exit(EX_SOFTWARE);
}

void
cheritest_success(void)
{

	ccsp->ccs_testresult = TESTRESULT_SUCCESS;
	exit(0);
}
