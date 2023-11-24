/*-
 * Copyright (c) 2023 Dmitry Chagin <dchagin@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/systm.h>
#include <vm/vm_param.h>

#include <atf-c.h>
#include <dlfcn.h>

static int jumpstack0(void) __noinline;
static int jumpstack1(void) __noinline;

static int (*socheckstack)(void) = NULL;

static int
checkstack(void)
{
	void *fh;

	if (socheckstack == NULL) {
		fh = dlopen("libsoxstack.so", RTLD_LAZY);
		ATF_REQUIRE(fh != NULL);
		socheckstack = dlsym(fh, "checkstack");
		ATF_REQUIRE(socheckstack != NULL);
	}
	return (socheckstack());
}

static int
jumpstack0(void)
{
	char stack[SGROWSIZ];

	explicit_bzero(stack, sizeof(stack));
	return (checkstack());
}

static int
jumpstack1(void)
{
	char stack[SGROWSIZ * 2];

	explicit_bzero(stack, sizeof(stack));
	return (checkstack());
}

ATF_TC_WITHOUT_HEAD(dlopen_test);
ATF_TC_BODY(dlopen_test, tc)
{

	ATF_REQUIRE(jumpstack0() == 0);
	ATF_REQUIRE(jumpstack1() == 0);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, dlopen_test);

	return (atf_no_error());
}
