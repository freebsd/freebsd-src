/*-
 * Copyright (c) 2017 Edward Tomasz Napierala
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
#include <sys/sysctl.h>
#include <sys/time.h>

#include <cheri/cheri.h>
#include <cheri/cheric.h>

#include <machine/sysarch.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "cheritest.h"

void
test_cheriabi_open_ordinary(const struct cheri_test *ctp __unused)
{
	char path[] = "/dev/null";
	int error, fd;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		cheritest_failure_err("open");

	error = close(fd);
	if (error != 0)
		cheritest_failure_err("close");

	cheritest_success();
}

void
test_cheriabi_open_offset(const struct cheri_test *ctp __unused)
{
	char pathbuf[] = "xxxx/dev/null";;
	char *path;
	int error, fd;

	path = pathbuf;
	path += 4;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		cheritest_failure_err("open");

	error = close(fd);
	if (error != 0)
		cheritest_failure_err("close");

	cheritest_success();
}

void
test_cheriabi_open_shortened(const struct cheri_test *ctp __unused)
{
	char path[] = "/dev/null/xxxx";
	int error, fd;

	path[9] = '\0';

	fd = open(path, O_RDONLY);
	if (fd < 0)
		cheritest_failure_err("open");

	error = close(fd);
	if (error != 0)
		cheritest_failure_err("close");

	cheritest_success();
}

void
test_cheriabi_open_bad_addr(const struct cheri_test *ctp __unused)
{
	char *path;
	int fd;

	path = (char *)(intptr_t)90210;

	fd = open(path, O_RDONLY);
	if (fd > 0)
		cheritest_failure_errx("open succeeded");

	if (errno != EFAULT)
		cheritest_failure_err("EFAULT expected");

	cheritest_success();
}

void
test_cheriabi_open_bad_addr_2(const struct cheri_test *ctp __unused)
{
	char *path;
	int fd;

	path = (char *)(intptr_t)-90210;

	fd = open(path, O_RDONLY);
	if (fd > 0)
		cheritest_failure_errx("open succeeded");

	if (errno != EFAULT)
		cheritest_failure_err("EFAULT expected");

	cheritest_success();
}

void
test_cheriabi_open_bad_len(const struct cheri_test *ctp __unused)
{
	char pathbuf[] = "/dev/null";
	char *path;
	int fd;

	path = cheri_csetbounds(pathbuf, strlen(path));

	fd = open(path, O_RDONLY);
	if (fd > 0)
		cheritest_failure_errx("open succeeded");

	if (errno != EFAULT)
		cheritest_failure_err("EFAULT expected");

	cheritest_success();
}

void
test_cheriabi_open_bad_len_2(const struct cheri_test *ctp __unused)
{
	char pathbuf[] = "xxxx/dev/null";;
	char *path;
	int fd;

	path = cheri_csetbounds(pathbuf, 3);
	path += 4;

	fd = open(path, O_RDONLY);
	if (fd > 0)
		cheritest_failure_errx("open succeeded");

	if (errno != EFAULT)
		cheritest_failure_err("EFAULT expected");

	cheritest_success();
}

void
test_cheriabi_open_bad_tag(const struct cheri_test *ctp __unused)
{
	char pathbuf[] = "/dev/null";
	char *path;
	int fd;

	path = cheri_cleartag(pathbuf);

	fd = open(path, O_RDONLY);
	if (fd > 0)
		cheritest_failure_errx("open succeeded");

	if (errno != EFAULT)
		cheritest_failure_err("EFAULT expected");

	cheritest_success();
}

void
test_cheriabi_open_bad_perm(const struct cheri_test *ctp __unused)
{
	char pathbuf[] = "/dev/null";
	char *path;
	int fd;

	path = cheri_andperm(pathbuf, ~CHERI_PERM_LOAD);

	fd = open(path, O_RDONLY);
	if (fd > 0)
		cheritest_failure_errx("open succeeded");

	if (errno != EFAULT)
		cheritest_failure_err("EFAULT expected");

	cheritest_success();
}

void
test_cheriabi_open_sealed(const struct cheri_test *ctp __unused)
{
	char *path;
	void * sealer;
	int fd;

	if (sysarch(CHERI_GET_SEALCAP, &sealer) < 0)
		cheritest_failure_err("CHERI_GET_SEALCAP");

	/* Allocate enough space that it's sealable for 128-bit */
	path = calloc(1, 1<<12);
	if (path == NULL)
		cheritest_failure_err("calloc");
	strcpy(path, "/dev/null");
	path = cheri_seal(path, sealer);

	fd = open(path, O_RDONLY);
	free(path);
	if (fd > 0)
		cheritest_failure_errx("open succeeded");

	if (errno != EFAULT)
		cheritest_failure_err("EFAULT expected");

	cheritest_success();
}
