/*-
 *
 * Copyright (C) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 */

#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <atf-c.h>

#define	EXIT_NOPREPARE		1
#define	EXIT_CALLEDPARENT	2
#define	EXIT_NOCHILD		3
#define	EXIT_BADORDER		4

static int child;
static int forked;
static int parent;

/*
 * We'll disable prefork unless we're specifically running the preinit test to
 * be sure that we don't mess up any other tests' results.
 */
static bool prefork_enabled;

static void
prefork(void)
{
	if (prefork_enabled)
		forked++;
}

static void
registrar(void)
{
	pthread_atfork(prefork, NULL, NULL);
}

static __attribute__((section(".preinit_array"), used))
void (*preinitfn)(void) = &registrar;

/*
 * preinit_atfork() just enables the prepare handler that we registered in a
 * .preinit_array entry and checks that forking actually invoked that callback.
 * We don't bother testing all three callbacks here because the implementation
 * doesn't really lend itself to the kind of error where we only have a partial
 * set of callbacks registered.
 */
ATF_TC(preinit_atfork);
ATF_TC_HEAD(preinit_atfork, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks that atfork callbacks may be registered in .preinit_array functions");
}
ATF_TC_BODY(preinit_atfork, tc)
{
	pid_t p;

	(void)signal(SIGCHLD, SIG_IGN);
	prefork_enabled = true;
	p = fork();

	ATF_REQUIRE(p >= 0);
	if (p == 0)
		_exit(0);

	prefork_enabled = false;

	ATF_REQUIRE(forked != 0);
}

static void
basic_prepare(void)
{
	ATF_REQUIRE(parent == 0);
	forked++;
}

static void
basic_parent(void)
{
	ATF_REQUIRE(forked != 0);
	parent++;
}

static void
basic_child(void)
{
	if (!forked)
		_exit(EXIT_NOPREPARE);
	if (parent != 0)
		_exit(EXIT_CALLEDPARENT);
	child++;
}

/*
 * In the basic test, we'll register just once and set some globals to confirm
 * that the prepare/parent callbacks were executed as expected.  The child will
 * use its exit status to communicate to us if the callback was not executed
 * properly since we cannot assert there.  This is a subset of the
 * multi-callback test, but separated out so that it's more obvious from running
 * the atfork_test if pthread_atfork() is completely broken or just
 * out-of-order.
 */
ATF_TC(basic_atfork);
ATF_TC_HEAD(basic_atfork, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks invocation of all three atfork callbacks");
}
ATF_TC_BODY(basic_atfork, tc)
{
	pid_t p, wpid;
	int status;

	pthread_atfork(basic_prepare, basic_parent, basic_child);

	p = fork();

	ATF_REQUIRE(p >= 0);
	if (p == 0)
		_exit(child != 0 ? 0 : EXIT_NOCHILD);

	/*
	 * The child can't use any of our standard atf-c(3) macros, so we have
	 * to rely on the exit status to convey any shenanigans.
	 */
	while ((wpid = waitpid(p, &status, 0)) != p) {
		ATF_REQUIRE_ERRNO(EINTR, wpid == -1);
		if (wpid == -1)
			continue;
	}

	ATF_REQUIRE_MSG(WIFEXITED(status),
	    "child did not exit cleanly, status %x", status);

	status = WEXITSTATUS(status);
	ATF_REQUIRE_MSG(status == 0, "atfork in child %s",
	   status == EXIT_NOPREPARE ? "did not see `prepare` execute" :
	   (status == EXIT_CALLEDPARENT ? "observed `parent` executing" :
	   (status == EXIT_NOCHILD ? "did not see `child` execute" :
	    "mystery")));

	ATF_REQUIRE(forked != 0);
	ATF_REQUIRE(parent != 0);
	ATF_REQUIRE(child == 0);
}

static void
multi_assert(bool cond, bool can_assert)
{
	if (can_assert)
		ATF_REQUIRE((cond));
	else if (!(cond))
		_exit(EXIT_BADORDER);
}

static void
multi_bump(int *var, int bit, bool can_assert)
{
	int mask, val;

	mask = (1 << (bit - 1));
	val = *var;

	/*
	 * Every bit below this one must be set, and none of the upper bits
	 * should be set.
	 */
	multi_assert((val & mask) == 0, can_assert);
	if (bit == 1)
		multi_assert(val == 0, can_assert);
	else
		multi_assert((val & ~mask) == (mask - 1), can_assert);

	*var |= mask;
}

static void
multi_prepare1(void)
{
	/*
	 * The bits are flipped for prepare because it's supposed to be called
	 * in the reverse order of registration.
	 */
	multi_bump(&forked, 2, true);
}
static void
multi_prepare2(void)
{
	multi_bump(&forked, 1, true);
}

static void
multi_parent1(void)
{
	multi_bump(&parent, 1, true);
}
static void
multi_parent2(void)
{
	multi_bump(&parent, 2, true);
}

static void
multi_child1(void)
{
	multi_bump(&child, 1, false);
}
static void
multi_child2(void)
{
	multi_bump(&child, 2, false);
}

/*
 * The multi-atfork test works much like the basic one, but it registers
 * multiple times and enforces an order.  The child still does just as strict
 * of tests as the parent and continues to communicate the results of those
 * tests back via its exit status.
 */
ATF_TC(multi_atfork);
ATF_TC_HEAD(multi_atfork, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks that multiple callbacks are called in the documented order");
}
ATF_TC_BODY(multi_atfork, tc)
{
	pid_t p, wpid;
	int status;

	pthread_atfork(multi_prepare1, multi_parent1, multi_child1);
	pthread_atfork(multi_prepare2, multi_parent2, multi_child2);

	p = fork();

	ATF_REQUIRE(p >= 0);
	if (p == 0)
		_exit(child != 0 ? 0 : EXIT_NOCHILD);

	/*
	 * The child can't use any of our standard atf-c(3) macros, so we have
	 * to rely on the exit status to convey any shenanigans.
	 */
	while ((wpid = waitpid(p, &status, 0)) != p) {
		ATF_REQUIRE_ERRNO(EINTR, wpid == -1);
		if (wpid == -1)
			continue;
	}

	ATF_REQUIRE_MSG(WIFEXITED(status),
	    "child did not exit cleanly, status %x", status);

	status = WEXITSTATUS(status);
	ATF_REQUIRE_MSG(status == 0, "atfork in child %s",
	   status == EXIT_BADORDER ? "called in wrong order" :
	   (status == EXIT_NOCHILD ? "did not see `child` execute" :
	    "mystery"));

	ATF_REQUIRE(forked != 0);
	ATF_REQUIRE(parent != 0);
	ATF_REQUIRE(child == 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, preinit_atfork);
	ATF_TP_ADD_TC(tp, basic_atfork);
	ATF_TP_ADD_TC(tp, multi_atfork);
	return (atf_no_error());
}
