/*-
 *
 * Copyright (C) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 */

#include <dlfcn.h>

#include <atf-c.h>

ATF_TC_WITHOUT_HEAD(dlopen_basic);
ATF_TC_BODY(dlopen_basic, tc)
{
	void *hdl, *sym;

	hdl = dlopen("libthr.so", RTLD_NOW);
	ATF_REQUIRE(hdl != NULL);

	sym = dlsym(hdl, "pthread_create");
	ATF_REQUIRE(sym != NULL);

	dlclose(hdl);

	sym = dlsym(hdl, "pthread_create");
	ATF_REQUIRE(sym == NULL);
}

ATF_TC_WITHOUT_HEAD(dlopen_recursing);
ATF_TC_BODY(dlopen_recursing, tc)
{
	void *hdl;

	/*
	 * If this doesn't crash, we're OK; a regression at one point caused
	 * some infinite recursion here.
	 */
	hdl = dlopen("libthr.so", RTLD_NOW | RTLD_GLOBAL);
	ATF_REQUIRE(hdl != NULL);

	dlclose(hdl);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, dlopen_basic);
	ATF_TP_ADD_TC(tp, dlopen_recursing);

	return atf_no_error();
}
