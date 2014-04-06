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

static int test_count = 1;
static int exit_status = EXIT_SUCCESS;

#define CHECK(x) \
	do { \
		if (x) { \
			printf("ok %d\n", test_count); \
		} else { \
			printf("not ok %d # %s\n", test_count, #x); \
			exit_status = EXIT_FAILURE; \
		} \
		++test_count; \
	} while (0)

int
main(void)
{
	char buf[64];
	char *sret;
	int iret;

	printf("1..27\n");

	/*
	 * strerror() failure tests.
	 */
	errno = 0;
	sret = strerror(INT_MAX);
	snprintf(buf, sizeof(buf), "Unknown error: %d", INT_MAX);
	CHECK(strcmp(sret, buf) == 0);
	CHECK(errno == EINVAL);

	/*
	 * strerror() success tests.
	 */
	errno = 0;
	sret = strerror(0);
	CHECK(strcmp(sret, "No error: 0") == 0);
	CHECK(errno == 0);

	errno = 0;
	sret = strerror(EPERM);
	CHECK(strcmp(sret, "Operation not permitted") == 0);
	CHECK(errno == 0);

	errno = 0;
	sret = strerror(EPFNOSUPPORT);
	CHECK(strcmp(sret, "Protocol family not supported") == 0);
	CHECK(errno == 0);

	errno = 0;
	sret = strerror(ELAST);
	CHECK(errno == 0);

	/*
	 * strerror_r() failure tests.
	 */
	memset(buf, '*', sizeof(buf));
	iret = strerror_r(-1, buf, sizeof(buf));
	CHECK(strcmp(buf, "Unknown error: -1") == 0);
	CHECK(iret == EINVAL);

	memset(buf, '*', sizeof(buf));
	/* One byte too short. */
	iret = strerror_r(EPERM, buf, strlen("Operation not permitted"));
	CHECK(strcmp(buf, "Operation not permitte") == 0);
	CHECK(iret == ERANGE);

	memset(buf, '*', sizeof(buf));
	/* One byte too short. */
	iret = strerror_r(-1, buf, strlen("Unknown error: -1"));
	CHECK(strcmp(buf, "Unknown error: -") == 0);
	CHECK(iret == EINVAL);

	memset(buf, '*', sizeof(buf));
	/* Two bytes too short. */
	iret = strerror_r(-2, buf, strlen("Unknown error: -2") - 1);
	CHECK(strcmp(buf, "Unknown error: ") == 0);
	CHECK(iret == EINVAL);

	memset(buf, '*', sizeof(buf));
	/* Three bytes too short. */
	iret = strerror_r(-2, buf, strlen("Unknown error: -2") - 2);
	CHECK(strcmp(buf, "Unknown error:") == 0);
	CHECK(iret == EINVAL);

	memset(buf, '*', sizeof(buf));
	/* One byte too short. */
	iret = strerror_r(12345, buf, strlen("Unknown error: 12345"));
	CHECK(strcmp(buf, "Unknown error: 1234") == 0);
	CHECK(iret == EINVAL);

	/*
	 * strerror_r() success tests.
	 */
	memset(buf, '*', sizeof(buf));
	iret = strerror_r(0, buf, sizeof(buf));
	CHECK(strcmp(buf, "No error: 0") == 0);
	CHECK(iret == 0);

	memset(buf, '*', sizeof(buf));
	iret = strerror_r(EDEADLK, buf, sizeof(buf));
	CHECK(strcmp(buf, "Resource deadlock avoided") == 0);
	CHECK(iret == 0);

	memset(buf, '*', sizeof(buf));
	iret = strerror_r(EPROCLIM, buf, sizeof(buf));
	CHECK(strcmp(buf, "Too many processes") == 0);
	CHECK(iret == 0);

	return exit_status;
}
