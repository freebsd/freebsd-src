/*-
 * Copyright (c) 2001 Wes Peters <wes@FreeBSD.org>
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
 * $FreeBSD$
 */

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tap.h>

int
main(void)
{
	char buf[64];
	char *sret;
	int iret;

	plan_tests(25);

	/*
	 * strerror() failure tests.
	 */
	errno = 0;
	sret = strerror(0);
	ok1(strcmp(sret, "Unknown error: 0") == 0);
	ok1(errno == EINVAL);

	errno = 0;
	sret = strerror(INT_MAX);
	snprintf(buf, sizeof(buf), "Unknown error: %d", INT_MAX);
	ok1(strcmp(sret, buf) == 0);
	ok1(errno == EINVAL);

	/*
	 * strerror() success tests.
	 */
	errno = 0;
	sret = strerror(EPERM);
	ok1(strcmp(sret, "Operation not permitted") == 0);
	ok1(errno == 0);

	errno = 0;
	sret = strerror(EPFNOSUPPORT);
	ok1(strcmp(sret, "Protocol family not supported") == 0);
	ok1(errno == 0);

	errno = 0;
	sret = strerror(ELAST);
	ok1(errno == 0);

	/*
	 * strerror_r() failure tests.
	 */
	memset(buf, '*', sizeof(buf));
	iret = strerror_r(0, buf, sizeof(buf));
	ok1(strcmp(buf, "Unknown error: 0") == 0);
	ok1(iret == EINVAL);

	memset(buf, '*', sizeof(buf));
	/* One byte too short. */
	iret = strerror_r(EPERM, buf, strlen("Operation not permitted"));
	ok1(strcmp(buf, "Operation not permitte") == 0);
	ok1(iret == ERANGE);

	memset(buf, '*', sizeof(buf));
	/* One byte too short. */
	iret = strerror_r(-1, buf, strlen("Unknown error: -1"));
	ok1(strcmp(buf, "Unknown error: -") == 0);
	ok1(iret == EINVAL);

	memset(buf, '*', sizeof(buf));
	/* Two bytes too short. */
	iret = strerror_r(-2, buf, strlen("Unknown error: -2") - 1);
	ok1(strcmp(buf, "Unknown error: ") == 0);
	ok1(iret == EINVAL);

	memset(buf, '*', sizeof(buf));
	/* Three bytes too short. */
	iret = strerror_r(-2, buf, strlen("Unknown error: -2") - 2);
	ok1(strcmp(buf, "Unknown error:") == 0);
	ok1(iret == EINVAL);

	memset(buf, '*', sizeof(buf));
	/* One byte too short. */
	iret = strerror_r(12345, buf, strlen("Unknown error: 12345"));
	ok1(strcmp(buf, "Unknown error: 1234") == 0);
	ok1(iret == EINVAL);

	/*
	 * strerror_r() success tests.
	 */
	memset(buf, '*', sizeof(buf));
	iret = strerror_r(EDEADLK, buf, sizeof(buf));
	ok1(strcmp(buf, "Resource deadlock avoided") == 0);
	ok1(iret == 0);

	memset(buf, '*', sizeof(buf));
	iret = strerror_r(EPROCLIM, buf, sizeof(buf));
	ok1(strcmp(buf, "Too many processes") == 0);
	ok1(iret == 0);

	return exit_status();
}
