/*-
 * Copyright (c) 2007 Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/fcntl.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libutil.h>

/*
 * Test that flopen() can create a file.
 */
const char *
test_flopen_create(void)
{
	const char *fn = "test_flopen_create";
	const char *result = NULL;
	int fd;

	unlink(fn);
	fd = flopen(fn, O_RDWR|O_CREAT, 0640);
	if (fd < 0) {
		result = strerror(errno);
	} else {
		close(fd);
	}
	unlink(fn);
	return (result);
}

/*
 * Test that flopen() can open an existing file.
 */
const char *
test_flopen_open(void)
{
	const char *fn = "test_flopen_open";
	const char *result = NULL;
	int fd;

	fd = open(fn, O_RDWR|O_CREAT, 0640);
	if (fd < 0) {
		result = strerror(errno);
	} else {
		close(fd);
		fd = flopen(fn, O_RDWR);
		if (fd < 0) {
			result = strerror(errno);
		} else {
			close(fd);
		}
	}
	unlink(fn);
	return (result);
}

#if FLOPEN_CAN_LOCK_AGAINST_SELF
/*
 * Test that flopen() can lock against itself
 */
const char *
test_flopen_lock_self(void)
{
	const char *fn = "test_flopen_lock";
	const char *result = NULL;
	int fd1, fd2;

	unlink(fn);
	fd1 = flopen(fn, O_RDWR|O_CREAT, 0640);
	if (fd1 < 0) {
		result = strerror(errno);
	} else {
		fd2 = flopen(fn, O_RDWR|O_NONBLOCK);
		if (fd2 >= 0) {
			result = "second open succeeded";
			close(fd2);
		}
		close(fd1);
	}
	unlink(fn);
	return (result);
}
#endif

/*
 * Test that flopen() can lock against other processes
 */
const char *
test_flopen_lock_other(void)
{
	const char *fn = "test_flopen_lock";
	const char *result = NULL;
	volatile int fd1, fd2;

	unlink(fn);
	fd1 = flopen(fn, O_RDWR|O_CREAT, 0640);
	if (fd1 < 0) {
		result = strerror(errno);
	} else {
		fd2 = -42;
		if (vfork() == 0) {
			fd2 = flopen(fn, O_RDWR|O_NONBLOCK);
			close(fd2);
			_exit(0);
		}
		if (fd2 == -42)
			result = "vfork() doesn't work as expected";
		if (fd2 >= 0)
			result = "second open succeeded";
		close(fd1);
	}
	unlink(fn);
	return (result);
}

static struct test {
	const char *name;
	const char *(*func)(void);
} t[] = {
	{ "flopen_create", test_flopen_create },
	{ "flopen_open", test_flopen_open },
#if FLOPEN_CAN_LOCK_AGAINST_SELF
	{ "flopen_lock_self", test_flopen_lock_self },
#endif
	{ "flopen_lock_other", test_flopen_lock_other },
};

int
main(void)
{
	const char *result;
	int i, nt;

	nt = sizeof(t) / sizeof(*t);
	printf("1..%d\n", nt);
	for (i = 0; i < nt; ++i) {
		if ((result = t[i].func()) != NULL)
			printf("not ok %d - %s # %s\n", i + 1,
			    t[i].name, result);
		else
			printf("ok %d - %s\n", i + 1,
			    t[i].name);
	}
	exit(0);
}
