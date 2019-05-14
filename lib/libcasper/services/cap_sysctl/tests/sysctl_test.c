/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2013, 2018 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
 *
 * Portions of this software were developed by Mark Johnston
 * under sponsorship from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/capsicum.h>
#include <sys/sysctl.h>
#include <sys/nv.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libcasper.h>
#include <casper/cap_sysctl.h>

#include <atf-c.h>

/*
 * We need some sysctls to perform the tests on.
 * We remember their values and restore them afer the test is done.
 */
#define	SYSCTL0_PARENT	"kern"
#define	SYSCTL0_NAME	"kern.sync_on_panic"
#define	SYSCTL0_FILE	"./sysctl0"
#define	SYSCTL1_PARENT	"debug"
#define	SYSCTL1_NAME	"debug.minidump"
#define	SYSCTL1_FILE	"./sysctl1"

#define	SYSCTL0_READ0		0x0001
#define	SYSCTL0_READ1		0x0002
#define	SYSCTL0_READ2		0x0004
#define	SYSCTL0_WRITE		0x0008
#define	SYSCTL0_READ_WRITE	0x0010
#define	SYSCTL1_READ0		0x0020
#define	SYSCTL1_READ1		0x0040
#define	SYSCTL1_READ2		0x0080
#define	SYSCTL1_WRITE		0x0100
#define	SYSCTL1_READ_WRITE	0x0200

static void
save_int_sysctl(const char *name, const char *file)
{
	ssize_t n;
	size_t sz;
	int error, fd, val;

	sz = sizeof(val);
	error = sysctlbyname(name, &val, &sz, NULL, 0);
	ATF_REQUIRE_MSG(error == 0,
	    "sysctlbyname(%s): %s", name, strerror(errno));

	fd = open(file, O_CREAT | O_WRONLY, 0600);
	ATF_REQUIRE_MSG(fd >= 0, "open(%s): %s", file, strerror(errno));
	n = write(fd, &val, sz);
	ATF_REQUIRE(n >= 0 && (size_t)n == sz);
	error = close(fd);
	ATF_REQUIRE(error == 0);
}

static void
restore_int_sysctl(const char *name, const char *file)
{
	ssize_t n;
	size_t sz;
	int error, fd, val;

	fd = open(file, O_RDONLY);
	ATF_REQUIRE(fd >= 0);
	sz = sizeof(val);
	n = read(fd, &val, sz);
	ATF_REQUIRE(n >= 0 && (size_t)n == sz);
	error = unlink(file);
	ATF_REQUIRE(error == 0);
	error = close(fd);
	ATF_REQUIRE(error == 0);

	error = sysctlbyname(name, NULL, NULL, &val, sz);
	ATF_REQUIRE_MSG(error == 0,
	    "sysctlbyname(%s): %s", name, strerror(errno));
}

static cap_channel_t *
initcap(void)
{
	cap_channel_t *capcas, *capsysctl;

	save_int_sysctl(SYSCTL0_NAME, SYSCTL0_FILE);
	save_int_sysctl(SYSCTL1_NAME, SYSCTL1_FILE);

	capcas = cap_init();
	ATF_REQUIRE(capcas != NULL);

	capsysctl = cap_service_open(capcas, "system.sysctl");
	ATF_REQUIRE(capsysctl != NULL);

	cap_close(capcas);

	return (capsysctl);
}

static void
cleanup(void)
{

	restore_int_sysctl(SYSCTL0_NAME, SYSCTL0_FILE);
	restore_int_sysctl(SYSCTL1_NAME, SYSCTL1_FILE);
}

static unsigned int
checkcaps(cap_channel_t *capsysctl)
{
	unsigned int result;
	size_t len0, len1, oldsize;
	int error, mib0[2], mib1[2], oldvalue, newvalue;

	result = 0;

	len0 = nitems(mib0);
	ATF_REQUIRE(sysctlnametomib(SYSCTL0_NAME, mib0, &len0) == 0);
	len1 = nitems(mib1);
	ATF_REQUIRE(sysctlnametomib(SYSCTL1_NAME, mib1, &len1) == 0);

	oldsize = sizeof(oldvalue);
	if (cap_sysctlbyname(capsysctl, SYSCTL0_NAME, &oldvalue, &oldsize,
	    NULL, 0) == 0) {
		if (oldsize == sizeof(oldvalue))
			result |= SYSCTL0_READ0;
	}
	error = cap_sysctl(capsysctl, mib0, len0, &oldvalue, &oldsize, NULL, 0);
	if ((result & SYSCTL0_READ0) != 0)
		ATF_REQUIRE(error == 0);
	else
		ATF_REQUIRE_ERRNO(ENOTCAPABLE, error != 0);

	newvalue = 123;
	if (cap_sysctlbyname(capsysctl, SYSCTL0_NAME, NULL, NULL, &newvalue,
	    sizeof(newvalue)) == 0) {
		result |= SYSCTL0_WRITE;
	}

	if ((result & SYSCTL0_WRITE) != 0) {
		oldsize = sizeof(oldvalue);
		if (cap_sysctlbyname(capsysctl, SYSCTL0_NAME, &oldvalue,
		    &oldsize, NULL, 0) == 0) {
			if (oldsize == sizeof(oldvalue) && oldvalue == 123)
				result |= SYSCTL0_READ1;
		}
	}
	newvalue = 123;
	error = cap_sysctl(capsysctl, mib0, len0, NULL, NULL,
	    &newvalue, sizeof(newvalue));
	if ((result & SYSCTL0_WRITE) != 0)
		ATF_REQUIRE(error == 0);
	else
		ATF_REQUIRE_ERRNO(ENOTCAPABLE, error != 0);

	oldsize = sizeof(oldvalue);
	newvalue = 4567;
	if (cap_sysctlbyname(capsysctl, SYSCTL0_NAME, &oldvalue, &oldsize,
	    &newvalue, sizeof(newvalue)) == 0) {
		if (oldsize == sizeof(oldvalue) && oldvalue == 123)
			result |= SYSCTL0_READ_WRITE;
	}

	if ((result & SYSCTL0_READ_WRITE) != 0) {
		oldsize = sizeof(oldvalue);
		if (cap_sysctlbyname(capsysctl, SYSCTL0_NAME, &oldvalue,
		    &oldsize, NULL, 0) == 0) {
			if (oldsize == sizeof(oldvalue) && oldvalue == 4567)
				result |= SYSCTL0_READ2;
		}
	}

	oldsize = sizeof(oldvalue);
	if (cap_sysctlbyname(capsysctl, SYSCTL1_NAME, &oldvalue, &oldsize,
	    NULL, 0) == 0) {
		if (oldsize == sizeof(oldvalue))
			result |= SYSCTL1_READ0;
	}
	error = cap_sysctl(capsysctl, mib1, len1, &oldvalue, &oldsize, NULL, 0);
	if ((result & SYSCTL1_READ0) != 0)
		ATF_REQUIRE(error == 0);
	else
		ATF_REQUIRE_ERRNO(ENOTCAPABLE, error != 0);

	newvalue = 506;
	if (cap_sysctlbyname(capsysctl, SYSCTL1_NAME, NULL, NULL, &newvalue,
	    sizeof(newvalue)) == 0) {
		result |= SYSCTL1_WRITE;
	}

	if ((result & SYSCTL1_WRITE) != 0) {
		newvalue = 506;
		ATF_REQUIRE(cap_sysctl(capsysctl, mib1, len1, NULL, NULL,
		    &newvalue, sizeof(newvalue)) == 0);

		oldsize = sizeof(oldvalue);
		if (cap_sysctlbyname(capsysctl, SYSCTL1_NAME, &oldvalue,
		    &oldsize, NULL, 0) == 0) {
			if (oldsize == sizeof(oldvalue) && oldvalue == 506)
				result |= SYSCTL1_READ1;
		}
	}
	newvalue = 506;
	error = cap_sysctl(capsysctl, mib1, len1, NULL, NULL,
	    &newvalue, sizeof(newvalue));
	if ((result & SYSCTL1_WRITE) != 0)
		ATF_REQUIRE(error == 0);
	else
		ATF_REQUIRE_ERRNO(ENOTCAPABLE, error != 0);

	oldsize = sizeof(oldvalue);
	newvalue = 7008;
	if (cap_sysctlbyname(capsysctl, SYSCTL1_NAME, &oldvalue, &oldsize,
	    &newvalue, sizeof(newvalue)) == 0) {
		if (oldsize == sizeof(oldvalue) && oldvalue == 506)
			result |= SYSCTL1_READ_WRITE;
	}

	if ((result & SYSCTL1_READ_WRITE) != 0) {
		oldsize = sizeof(oldvalue);
		if (cap_sysctlbyname(capsysctl, SYSCTL1_NAME, &oldvalue,
		    &oldsize, NULL, 0) == 0) {
			if (oldsize == sizeof(oldvalue) && oldvalue == 7008)
				result |= SYSCTL1_READ2;
		}
	}

	return (result);
}

ATF_TC_WITH_CLEANUP(cap_sysctl__operation);
ATF_TC_HEAD(cap_sysctl__operation, tc)
{
}
ATF_TC_BODY(cap_sysctl__operation, tc)
{
	cap_channel_t *capsysctl, *ocapsysctl;
	void *limit;

	ocapsysctl = initcap();

	/*
	 * Allow:
	 * SYSCTL0_PARENT/RDWR/RECURSIVE
	 * SYSCTL1_PARENT/RDWR/RECURSIVE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, "foo.bar",
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, "foo.bar",
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == (SYSCTL0_READ0 | SYSCTL0_READ1 |
	    SYSCTL0_READ2 | SYSCTL0_WRITE | SYSCTL0_READ_WRITE |
	    SYSCTL1_READ0 | SYSCTL1_READ1 | SYSCTL1_READ2 | SYSCTL1_WRITE |
	    SYSCTL1_READ_WRITE));

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);

	ATF_REQUIRE(checkcaps(capsysctl) == (SYSCTL0_READ0 | SYSCTL0_READ1 |
	    SYSCTL0_READ2 | SYSCTL0_WRITE | SYSCTL0_READ_WRITE |
	    SYSCTL1_READ0 | SYSCTL1_READ1 | SYSCTL1_READ2 | SYSCTL1_WRITE |
	    SYSCTL1_READ_WRITE));

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);

	ATF_REQUIRE(checkcaps(capsysctl) == (SYSCTL0_READ0 | SYSCTL1_WRITE));

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_READ);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);

	ATF_REQUIRE(checkcaps(capsysctl) == (SYSCTL0_READ0 | SYSCTL1_WRITE));

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);

	ATF_REQUIRE(checkcaps(capsysctl) == SYSCTL0_READ0);

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_NAME/RDWR/RECURSIVE
	 * SYSCTL1_NAME/RDWR/RECURSIVE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_RDWR);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_READ);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == (SYSCTL0_READ0 | SYSCTL0_READ1 |
	    SYSCTL0_READ2 | SYSCTL0_WRITE | SYSCTL0_READ_WRITE |
	    SYSCTL1_READ0 | SYSCTL1_READ1 | SYSCTL1_READ2 | SYSCTL1_WRITE |
	    SYSCTL1_READ_WRITE));

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_PARENT/RDWR
	 * SYSCTL1_PARENT/RDWR
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_RDWR);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == 0);

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_NAME/RDWR
	 * SYSCTL1_NAME/RDWR
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_RDWR);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == (SYSCTL0_READ0 | SYSCTL0_READ1 |
	    SYSCTL0_READ2 | SYSCTL0_WRITE | SYSCTL0_READ_WRITE |
	    SYSCTL1_READ0 | SYSCTL1_READ1 | SYSCTL1_READ2 | SYSCTL1_WRITE |
	    SYSCTL1_READ_WRITE));

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_PARENT/RDWR
	 * SYSCTL1_PARENT/RDWR/RECURSIVE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_RDWR);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == (SYSCTL1_READ0 | SYSCTL1_READ1 |
	    SYSCTL1_READ2 | SYSCTL1_WRITE | SYSCTL1_READ_WRITE));

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_NAME/RDWR
	 * SYSCTL1_NAME/RDWR/RECURSIVE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_RDWR);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == (SYSCTL0_READ0 | SYSCTL0_READ1 |
	    SYSCTL0_READ2 | SYSCTL0_WRITE | SYSCTL0_READ_WRITE |
	    SYSCTL1_READ0 | SYSCTL1_READ1 | SYSCTL1_READ2 | SYSCTL1_WRITE |
	    SYSCTL1_READ_WRITE));

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_PARENT/READ/RECURSIVE
	 * SYSCTL1_PARENT/READ/RECURSIVE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_RDWR);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_WRITE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == (SYSCTL0_READ0 | SYSCTL1_READ0));

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_NAME/READ/RECURSIVE
	 * SYSCTL1_NAME/READ/RECURSIVE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_RDWR);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_WRITE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == (SYSCTL0_READ0 | SYSCTL1_READ0));

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_PARENT/READ
	 * SYSCTL1_PARENT/READ
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_READ);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_RDWR);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_WRITE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == 0);

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_NAME/READ
	 * SYSCTL1_NAME/READ
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_READ);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_RDWR);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_WRITE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == (SYSCTL0_READ0 | SYSCTL1_READ0));

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_PARENT/READ
	 * SYSCTL1_PARENT/READ/RECURSIVE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_READ);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == SYSCTL1_READ0);

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_NAME/READ
	 * SYSCTL1_NAME/READ/RECURSIVE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_READ);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == (SYSCTL0_READ0 | SYSCTL1_READ0));

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_PARENT/WRITE/RECURSIVE
	 * SYSCTL1_PARENT/WRITE/RECURSIVE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_RDWR);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_READ);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == (SYSCTL0_WRITE | SYSCTL1_WRITE));

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_NAME/WRITE/RECURSIVE
	 * SYSCTL1_NAME/WRITE/RECURSIVE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_RDWR);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_READ);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == (SYSCTL0_WRITE | SYSCTL1_WRITE));

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_PARENT/WRITE
	 * SYSCTL1_PARENT/WRITE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_WRITE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_RDWR);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_READ);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == 0);

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_NAME/WRITE
	 * SYSCTL1_NAME/WRITE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_WRITE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_RDWR);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_READ);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == (SYSCTL0_WRITE | SYSCTL1_WRITE));

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_PARENT/WRITE
	 * SYSCTL1_PARENT/WRITE/RECURSIVE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_WRITE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == SYSCTL1_WRITE);

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_NAME/WRITE
	 * SYSCTL1_NAME/WRITE/RECURSIVE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_WRITE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == (SYSCTL0_WRITE | SYSCTL1_WRITE));

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_PARENT/READ/RECURSIVE
	 * SYSCTL1_PARENT/WRITE/RECURSIVE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);

	ATF_REQUIRE(checkcaps(capsysctl) == (SYSCTL0_READ0 | SYSCTL1_WRITE));

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_NAME/READ/RECURSIVE
	 * SYSCTL1_NAME/WRITE/RECURSIVE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);

	ATF_REQUIRE(checkcaps(capsysctl) == (SYSCTL0_READ0 | SYSCTL1_WRITE));

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_PARENT/READ
	 * SYSCTL1_PARENT/WRITE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_READ);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);

	ATF_REQUIRE(checkcaps(capsysctl) == 0);

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_NAME/READ
	 * SYSCTL1_NAME/WRITE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_READ);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);

	ATF_REQUIRE(checkcaps(capsysctl) == (SYSCTL0_READ0 | SYSCTL1_WRITE));

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_PARENT/READ
	 * SYSCTL1_PARENT/WRITE/RECURSIVE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_READ);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);

	ATF_REQUIRE(checkcaps(capsysctl) == SYSCTL1_WRITE);

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_NAME/READ
	 * SYSCTL1_NAME/WRITE/RECURSIVE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_READ);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);

	ATF_REQUIRE(checkcaps(capsysctl) == (SYSCTL0_READ0 | SYSCTL1_WRITE));

	cap_close(capsysctl);
}
ATF_TC_CLEANUP(cap_sysctl__operation, tc)
{
	cleanup();
}

ATF_TC_WITH_CLEANUP(cap_sysctl__names);
ATF_TC_HEAD(cap_sysctl__names, tc)
{
}
ATF_TC_BODY(cap_sysctl__names, tc)
{
	cap_channel_t *capsysctl, *ocapsysctl;
	void *limit;

	ocapsysctl = initcap();

	/*
	 * Allow:
	 * SYSCTL0_PARENT/READ/RECURSIVE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_READ);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == SYSCTL0_READ0);

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL1_NAME/READ/RECURSIVE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_READ | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_READ);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == SYSCTL1_READ0);

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_PARENT/WRITE/RECURSIVE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_WRITE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == SYSCTL0_WRITE);

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL1_NAME/WRITE/RECURSIVE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_WRITE | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_WRITE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == SYSCTL1_WRITE);

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_PARENT/RDWR/RECURSIVE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_READ);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == (SYSCTL0_READ0 | SYSCTL0_READ1 |
	    SYSCTL0_READ2 | SYSCTL0_WRITE | SYSCTL0_READ_WRITE));

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL1_NAME/RDWR/RECURSIVE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME,
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_READ);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == (SYSCTL1_READ0 | SYSCTL1_READ1 |
	    SYSCTL1_READ2 | SYSCTL1_WRITE | SYSCTL1_READ_WRITE));

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_PARENT/READ
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_READ);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == 0);

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL1_NAME/READ
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_READ);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_READ);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == SYSCTL1_READ0);

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_PARENT/WRITE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_WRITE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == 0);

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL1_NAME/WRITE
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_WRITE);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_WRITE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == SYSCTL1_WRITE);

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL0_PARENT/RDWR
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_PARENT, CAP_SYSCTL_RDWR);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_PARENT, CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == 0);

	cap_close(capsysctl);

	/*
	 * Allow:
	 * SYSCTL1_NAME/RDWR
	 */

	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_RDWR);
	(void)cap_sysctl_limit_name(limit, SYSCTL1_NAME, CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);
	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, SYSCTL0_NAME, CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == -1 && errno == ENOTCAPABLE);

	ATF_REQUIRE(checkcaps(capsysctl) == (SYSCTL1_READ0 | SYSCTL1_READ1 |
	    SYSCTL1_READ2 | SYSCTL1_WRITE | SYSCTL1_READ_WRITE));

	cap_close(capsysctl);
}
ATF_TC_CLEANUP(cap_sysctl__names, tc)
{
	cleanup();
}

ATF_TC_WITH_CLEANUP(cap_sysctl__no_limits);
ATF_TC_HEAD(cap_sysctl__no_limits, tc)
{
}
ATF_TC_BODY(cap_sysctl__no_limits, tc)
{
	cap_channel_t *capsysctl;

	capsysctl = initcap();

	ATF_REQUIRE_EQ(checkcaps(capsysctl), (SYSCTL0_READ0 | SYSCTL0_READ1 |
	    SYSCTL0_READ2 | SYSCTL0_WRITE | SYSCTL0_READ_WRITE |
	    SYSCTL1_READ0 | SYSCTL1_READ1 | SYSCTL1_READ2 | SYSCTL1_WRITE |
	    SYSCTL1_READ_WRITE));
}
ATF_TC_CLEANUP(cap_sysctl__no_limits, tc)
{
	cleanup();
}

ATF_TC_WITH_CLEANUP(cap_sysctl__recursive_limits);
ATF_TC_HEAD(cap_sysctl__recursive_limits, tc)
{
}
ATF_TC_BODY(cap_sysctl__recursive_limits, tc)
{
	cap_channel_t *capsysctl, *ocapsysctl;
	void *limit;
	size_t len;
	int mib[2], val = 420;

	len = nitems(mib);
	ATF_REQUIRE(sysctlnametomib(SYSCTL0_NAME, mib, &len) == 0);

	ocapsysctl = initcap();

	/*
	 * Make sure that we match entire components.
	 */
	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, "ker",
	    CAP_SYSCTL_RDWR | CAP_SYSCTL_RECURSIVE);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);

	ATF_REQUIRE_ERRNO(ENOTCAPABLE, cap_sysctlbyname(capsysctl, SYSCTL0_NAME,
	    NULL, NULL, &val, sizeof(val)));
	ATF_REQUIRE_ERRNO(ENOTCAPABLE, cap_sysctl(capsysctl, mib, len,
	    NULL, NULL, &val, sizeof(val)));

	cap_close(capsysctl);

	/*
	 * Verify that we check for CAP_SYSCTL_RECURSIVE.
	 */
	capsysctl = cap_clone(ocapsysctl);
	ATF_REQUIRE(capsysctl != NULL);

	limit = cap_sysctl_limit_init(capsysctl);
	(void)cap_sysctl_limit_name(limit, "kern", CAP_SYSCTL_RDWR);
	ATF_REQUIRE(cap_sysctl_limit(limit) == 0);

	ATF_REQUIRE_ERRNO(ENOTCAPABLE, cap_sysctlbyname(capsysctl, SYSCTL0_NAME,
	    NULL, NULL, &val, sizeof(val)));
	ATF_REQUIRE_ERRNO(ENOTCAPABLE, cap_sysctl(capsysctl, mib, len,
	    NULL, NULL, &val, sizeof(val)));

	cap_close(capsysctl);
}
ATF_TC_CLEANUP(cap_sysctl__recursive_limits, tc)
{
	cleanup();
}

ATF_TC_WITH_CLEANUP(cap_sysctl__just_size);
ATF_TC_HEAD(cap_sysctl__just_size, tc)
{
}
ATF_TC_BODY(cap_sysctl__just_size, tc)
{
	cap_channel_t *capsysctl;
	size_t len;
	int mib0[2];

	capsysctl = initcap();

	len = nitems(mib0);
	ATF_REQUIRE(sysctlnametomib(SYSCTL0_NAME, mib0, &len) == 0);

	ATF_REQUIRE(cap_sysctlbyname(capsysctl, SYSCTL0_NAME,
	    NULL, &len, NULL, 0) == 0);
	ATF_REQUIRE(len == sizeof(int));
	ATF_REQUIRE(cap_sysctl(capsysctl, mib0, nitems(mib0),
	    NULL, &len, NULL, 0) == 0);
	ATF_REQUIRE(len == sizeof(int));

	cap_close(capsysctl);
}
ATF_TC_CLEANUP(cap_sysctl__just_size, tc)
{
	cleanup();
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, cap_sysctl__operation);
	ATF_TP_ADD_TC(tp, cap_sysctl__names);
	ATF_TP_ADD_TC(tp, cap_sysctl__no_limits);
	ATF_TP_ADD_TC(tp, cap_sysctl__recursive_limits);
	ATF_TP_ADD_TC(tp, cap_sysctl__just_size);

	return (atf_no_error());
}
