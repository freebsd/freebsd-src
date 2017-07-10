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
test_open_ordinary(const struct cheri_test *ctp __unused)
{
	char * __capability path;
	int error, fd;

	path = (char * __capability)"/dev/null";

	fd = open(path, O_RDONLY);
	if (fd < 0)
		cheritest_failure_err("open");

	error = close(fd);
	if (error != 0)
		cheritest_failure_err("close");

	cheritest_success();
}

void
test_open_offset(const struct cheri_test *ctp __unused)
{
	char * __capability path;
	int error, fd;

	path = (char * __capability)"xxxx/dev/null";
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
test_open_shortened(const struct cheri_test *ctp __unused)
{
	char * __capability path;
	int error, fd;

	path = (char * __capability)"/dev/null/xxxx";
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
test_open_bad_addr(const struct cheri_test *ctp __unused)
{
	char * __capability path;
	int fd;

	path = (char * __capability)90210;

	fd = open(path, O_RDONLY);
	if (fd > 0)
		cheritest_failure_errx("open succeeded");

	cheritest_success();
}

void
test_open_bad_addr_2(const struct cheri_test *ctp __unused)
{
	char * __capability path;
	int fd;

	path = (char * __capability)-90210;

	fd = open(path, O_RDONLY);
	if (fd > 0)
		cheritest_failure_errx("open succeeded");

	cheritest_success();
}

void
test_open_bad_len(const struct cheri_test *ctp __unused)
{
	char * __capability path;
	int fd;

	path = (char * __capability)"/dev/null";
	path = cheri_csetbounds(path, strlen(path));

	fd = open(path, O_RDONLY);
	if (fd > 0)
		cheritest_failure_errx("open succeeded");

	cheritest_success();
}

void
test_open_bad_len_2(const struct cheri_test *ctp __unused)
{
	char * __capability path;
	int fd;

	path = (char * __capability)"xxxx/dev/null";
	path = cheri_csetbounds(path, 3);
	path += 4;

	fd = open(path, O_RDONLY);
	if (fd > 0)
		cheritest_failure_errx("open succeeded");

	cheritest_success();
}

void
test_open_bad_tag(const struct cheri_test *ctp __unused)
{
	char * __capability path;
	int fd;


	path = (char * __capability)"/dev/null";
	path = cheri_cleartag(path);

	fd = open(path, O_RDONLY);
	if (fd > 0)
		cheritest_failure_errx("open succeeded");

	cheritest_success();
}

void
test_open_bad_perm(const struct cheri_test *ctp __unused)
{
	char * __capability path;
	int fd;

	path = (char * __capability)"/dev/null";
	path = cheri_andperm(path, ~CHERI_PERM_LOAD);

	fd = open(path, O_RDONLY);
	if (fd > 0)
		cheritest_failure_errx("open succeeded");

	cheritest_success();
}

void
test_open_sealed(const struct cheri_test *ctp __unused)
{
	char * __capability path;
	void * __capability sealer;
	int fd;

	if (sysarch(CHERI_GET_SEALCAP, &sealer) < 0)
		cheritest_failure_err("CHERI_GET_SEALCAP");

	path = (char * __capability)"/dev/null";
	path = cheri_seal(path, sealer);

	fd = open(path, O_RDONLY);
	if (fd > 0)
		cheritest_failure_errx("open succeeded");

	cheritest_success();
}
