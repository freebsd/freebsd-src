/*-
 *
 * Copyright (C) 2023 NetApp, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 */

#include <dlfcn.h>

#include <atf-c.h>

int get_value(void);
void set_value(int);

#define	APP_VALUE	5
#define	LIB_VALUE	20

ATF_TC_WITHOUT_HEAD(deepbind_simple);
ATF_TC_BODY(deepbind_simple, tc)
{
	void *hdl;
	void (*proxy_set_value)(int);
	int (*proxy_get_value)(void);
	int app_value, lib_value;

	set_value(APP_VALUE);

	/*
	 * libdeep has a dependency on libval2.so, which is a rebuild of
	 * libval.so that provides get_value() and set_value() for both us and
	 * the lib.  The lib's get_value() and set_value() should bind to the
	 * versions in libval2 instead of libval with RTLD_DEEPBIND.
	 */
	hdl = dlopen("$ORIGIN/libdeep.so", RTLD_LAZY | RTLD_DEEPBIND);
	ATF_REQUIRE(hdl != NULL);

	proxy_set_value = dlsym(hdl, "proxy_set_value");
	ATF_REQUIRE(proxy_set_value != NULL);

	proxy_get_value = dlsym(hdl, "proxy_get_value");
	ATF_REQUIRE(proxy_get_value != NULL);

	(*proxy_set_value)(LIB_VALUE);

	lib_value = (*proxy_get_value)();
	app_value = get_value();

	/*
	 * In the initial implementation or if libdeep.so is *not* linked
	 * against its own libval2, then these both return the later set
	 * LIB_VALUE (20) as they bind to the symbol provided by libval and
	 * use its .bss val.
	 */
	ATF_REQUIRE_INTEQ(lib_value, LIB_VALUE);
	ATF_REQUIRE_INTEQ(app_value, APP_VALUE);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, deepbind_simple);

	return atf_no_error();
}
