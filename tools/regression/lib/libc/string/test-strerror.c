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

int
main(void)
{
	char buf[64];
	char *sret;
	int iret;

	/*
	 * strerror() failure tests.
	 */
	errno = 0;
	sret = strerror(0);
	assert(strcmp(sret, "Unknown error: 0") == 0);
	assert(errno == EINVAL);

	errno = 0;
	sret = strerror(INT_MAX);
	snprintf(buf, sizeof(buf), "Unknown error: %d", INT_MAX);
	assert(strcmp(sret, buf) == 0);
	assert(errno == EINVAL);

	/*
	 * strerror() success tests.
	 */
	errno = 0;
	sret = strerror(EPERM);
	assert(strcmp(sret, "Operation not permitted") == 0);
	assert(errno == 0);

	errno = 0;
	sret = strerror(EPFNOSUPPORT);
	assert(strcmp(sret, "Protocol family not supported") == 0);
	assert(errno == 0);

	errno = 0;
	sret = strerror(ELAST);
	assert(errno == 0);

	printf("PASS strerror()\n");

	/*
	 * strerror_r() failure tests.
	 */
	memset(buf, '*', sizeof(buf));
	iret = strerror_r(0, buf, sizeof(buf));
	assert(strcmp(buf, "Unknown error: 0") == 0);
	assert(iret == EINVAL);

	memset(buf, '*', sizeof(buf));
	/* One byte too short. */
	iret = strerror_r(EPERM, buf, strlen("Operation not permitted"));
	assert(strcmp(buf, "Operation not permitte") == 0);
	assert(iret == ERANGE);

	memset(buf, '*', sizeof(buf));
	/* One byte too short. */
	iret = strerror_r(-1, buf, strlen("Unknown error: -1"));
	assert(strcmp(buf, "Unknown error: -") == 0);
	assert(iret == EINVAL);

	memset(buf, '*', sizeof(buf));
	/* Two bytes too short. */
	iret = strerror_r(-2, buf, strlen("Unknown error: -2") - 1);
	assert(strcmp(buf, "Unknown error: ") == 0);
	assert(iret == EINVAL);

	memset(buf, '*', sizeof(buf));
	/* Three bytes too short. */
	iret = strerror_r(-2, buf, strlen("Unknown error: -2") - 2);
	assert(strcmp(buf, "Unknown error:") == 0);
	assert(iret == EINVAL);

	memset(buf, '*', sizeof(buf));
	/* One byte too short. */
	iret = strerror_r(12345, buf, strlen("Unknown error: 12345"));
	assert(strcmp(buf, "Unknown error: 1234") == 0);
	assert(iret == EINVAL);

	/*
	 * strerror_r() success tests.
	 */
	memset(buf, '*', sizeof(buf));
	iret = strerror_r(EDEADLK, buf, sizeof(buf));
	assert(strcmp(buf, "Resource deadlock avoided") == 0);
	assert(iret == 0);

	memset(buf, '*', sizeof(buf));
	iret = strerror_r(EPROCLIM, buf, sizeof(buf));
	assert(strcmp(buf, "Too many processes") == 0);
	assert(iret == 0);

	printf("PASS strerror_r()\n");

	exit(0);
}
