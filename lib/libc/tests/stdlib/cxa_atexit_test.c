/*-
 * Copyright (c) 2025 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/wait.h>

#include <dlfcn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <atf-c.h>

#define	ARBITRARY_EXIT_CODE	42

static char *
get_shlib(const char *srcdir)
{
	char *shlib;

	shlib = NULL;
	if (asprintf(&shlib, "%s/libatexit.so", srcdir) < 0)
		atf_tc_fail("failed to construct path to libatexit.so");
	return (shlib);
}

static void
run_test(const atf_tc_t *tc, bool with_fatal_atexit, bool with_exit)
{
	pid_t p;
	void (*set_fatal_atexit)(bool);
	void (*set_exit_code)(int);
	void *hdl;
	char *shlib;

	shlib = get_shlib(atf_tc_get_config_var(tc, "srcdir"));

	hdl = dlopen(shlib, RTLD_LAZY);
	ATF_REQUIRE_MSG(hdl != NULL, "dlopen: %s", dlerror());

	free(shlib);

	if (with_fatal_atexit) {
		set_fatal_atexit = dlsym(hdl, "set_fatal_atexit");
		ATF_REQUIRE_MSG(set_fatal_atexit != NULL,
		    "set_fatal_atexit: %s", dlerror());
	}
	if (with_exit) {
		set_exit_code = dlsym(hdl, "set_exit_code");
		ATF_REQUIRE_MSG(set_exit_code != NULL, "set_exit_code: %s",
		    dlerror());
	}

	p = atf_utils_fork();
	if (p == 0) {
		/*
		 * Don't let the child clobber the results file; stderr/stdout
		 * have been replaced by atf_utils_fork() to capture it.  We're
		 * intentionally using exit() instead of _exit() here to run
		 * __cxa_finalize at exit, otherwise we'd just leave it be.
		 */
		closefrom(3);

		if (with_fatal_atexit)
			set_fatal_atexit(true);
		if (with_exit)
			set_exit_code(ARBITRARY_EXIT_CODE);

		dlclose(hdl);

		/*
		 * If the dtor was supposed to exit (most cases), then we should
		 * not have made it to this point.  If it's not supposed to
		 * exit, then we just exit with success here because we might
		 * be expecting either a clean exit or a signal on our way out
		 * as the final __cxa_finalize tries to run a callback in the
		 * unloaded DSO.
		 */
		if (with_exit)
			exit(1);
		exit(0);
	}

	dlclose(hdl);
	atf_utils_wait(p, with_exit ? ARBITRARY_EXIT_CODE : 0, "", "");
}

ATF_TC_WITHOUT_HEAD(simple_cxa_atexit);
ATF_TC_BODY(simple_cxa_atexit, tc)
{
	/*
	 * This test exits in a global object's dtor so that we check for our
	 * dtor being run at dlclose() time.  If it isn't, then the forked child
	 * will have a chance to exit(1) after dlclose() to raise a failure.
	 */
	run_test(tc, false, true);
}

ATF_TC_WITHOUT_HEAD(late_cxa_atexit);
ATF_TC_BODY(late_cxa_atexit, tc)
{
	/*
	 * This test creates another global object during a __cxa_atexit handler
	 * invocation.  It's been observed in the wild that we weren't executing
	 * it, then the DSO gets torn down and it was executed at application
	 * exit time instead.  In the best case scenario we would crash if
	 * something else hadn't been mapped there.
	 */
	run_test(tc, true, false);
}

ATF_TC_WITHOUT_HEAD(late_cxa_atexit_ran);
ATF_TC_BODY(late_cxa_atexit_ran, tc)
{
	/*
	 * This is a slight variation of the previous test where we trigger an
	 * exit() in our late-registered __cxa_atexit handler so that we can
	 * ensure it was ran *before* dlclose() finished and not through some
	 * weird chain of events afterwards.
	 */
	run_test(tc, true, true);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, simple_cxa_atexit);
	ATF_TP_ADD_TC(tp, late_cxa_atexit);
	ATF_TP_ADD_TC(tp, late_cxa_atexit_ran);
	return (atf_no_error());
}
