/*
 * Copyright (c) 2021 Dell Inc. or its subsidiaries. All Rights Reserved.
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

/*
 * Test behavior when a mapping of a shared shadow vm object is
 * invalidated by COW from another mapping.
 *
 * This is a regression test for an issue isolated by rlibby@FreeBSD.org
 * from an issue detected by stress2's collapse.sh by jeff@FreeBSD.org.
 * The issue became CVE-2021-29626.
 *
 * This file is written as an ATF test suite but may be compiled as a
 * standalone program with -DSTANDALONE (and optionally -DDEBUG).
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/procctl.h>
#include <sys/wait.h>
#include <machine/atomic.h>

#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef	STANDALONE
#define	ATF_REQUIRE(x)	do {		\
	if (!(x))			\
		errx(1, "%s", #x);	\
} while (0)
#else
#include <atf-c.h>
#endif

#ifdef	DEBUG
#define	dprintf(...)	printf(__VA_ARGS__)
#else
#define	dprintf(...)
#endif

#define	DEPTH	5

struct shared_state {
	void *p;
	size_t len;
	size_t modlen;
	bool collapse;
	bool block_xfer;
	bool okay;
	volatile bool exiting[DEPTH];
	volatile bool exit;
	volatile bool p3_did_write;
};

static long g_pagesize;

/*
 * Program flow.  There are three or four processes that are descendants
 * of the process running the test (P0), where arrows go from parents to
 * children, and thicker arrows indicate sharing a certain memory region
 * without COW semantics:
 *     P0 -> P1 -> P2 => P3
 *             \=> P4
 * The main idea is that P1 maps a memory region, and that region is
 * shared with P2/P3, but with COW semantics.  When P3 modifies the
 * memory, P2 ought to see that modification.  P4 optionally exists to
 * defeat a COW optimization.
 */

#define	child_err(...)	do {						\
	ss->exit = true;						\
	err(1, __VA_ARGS__);						\
} while (0)

#define	child_errx(...)	do {						\
	ss->exit = true;						\
	errx(1, __VA_ARGS__);						\
} while (0)

#define	SLEEP_TIME_US	1000

static void child(struct shared_state *ss, int depth);

static pid_t
child_fork(struct shared_state *ss, int depth)
{
	pid_t pid = fork();
	if (pid == -1)
		child_err("fork");
	else if (pid == 0)
		child(ss, depth);
	return pid;
}

static void
child_fault(struct shared_state *ss)
{
	size_t i;

	for (i = 0; i < ss->len; i += g_pagesize)
		(void)((volatile char *)ss->p)[i];
}

static void
child_write(struct shared_state *ss, int val, size_t len)
{
	size_t i;

	for (i = 0; i < len; i += g_pagesize)
		((int *)ss->p)[i / sizeof(int)] = val;
	atomic_thread_fence_rel();
}

static void
child_wait_p3_write(struct shared_state *ss)
{
	while (!ss->p3_did_write) {
		if (ss->exit)
			exit(1);
		usleep(SLEEP_TIME_US);
	}
	atomic_thread_fence_acq();
}

static void
child_verify(struct shared_state *ss, int depth, int newval, int oldval)
{
	size_t i;
	int expectval, foundval;

	for (i = 0; i < ss->len; i += g_pagesize) {
		expectval = i < ss->modlen ? newval : oldval;
		foundval = ((int *)ss->p)[i / sizeof(int)];
		if (foundval == expectval)
			continue;
		child_errx("P%d saw %d but expected %d, %d was the old value",
		    depth, foundval, expectval, oldval);
	}
}

static void
child(struct shared_state *ss, int depth)
{
	pid_t mypid, oldval, pid;

	if (depth < 1 || depth >= DEPTH)
		child_errx("Bad depth %d", depth);
	mypid = getpid();
	dprintf("P%d (pid %d) started\n", depth, mypid);
	switch (depth) {
	case 1:
		/* Shared memory undergoing test. */
		ss->p = mmap(NULL, ss->len, PROT_READ | PROT_WRITE,
		    MAP_SHARED | MAP_ANON, -1, 0);
		if (ss->p == MAP_FAILED)
			child_err("mmap");
		/* P1 stamps the shared memory. */
		child_write(ss, mypid, ss->len);
		if (ss->block_xfer) {
			/*
			 * P4 is forked so that its existence blocks a page COW
			 * path where the page is simply transferred between
			 * objects, rather than being copied.
			 */
			child_fork(ss, 4);
		}
		/*
		 * P1 specifies that modifications from its child processes not
		 * be shared with P1.  Child process reads can be serviced from
		 * pages in P1's object, but writes must be COW'd.
		 */
		if (minherit(ss->p, ss->len, INHERIT_COPY) != 0)
			child_err("minherit");
		/* Fork P2. */
		child_fork(ss, depth + 1);
		/* P1 and P4 wait for P3's writes before exiting. */
		child_wait_p3_write(ss);
		child_verify(ss, depth, mypid, mypid);
		if (!ss->collapse) {
			/* Hang around to prevent collapse. */
			while (!ss->exit)
				usleep(SLEEP_TIME_US);
		}
		/* Exit so the P2 -> P1/P4 shadow chain can collapse. */
		break;
	case 2:
		/*
		 * P2 now specifies that modifications from its child processes
		 * be shared.  P2 and P3 will share a shadow object.
		 */
		if (minherit(ss->p, ss->len, INHERIT_SHARE) != 0)
			child_err("minherit");
		/*
		 * P2 faults a page in P1's object before P1 exits and the
		 * shadow chain is collapsed.
		 */
		child_fault(ss);
		oldval = atomic_load_acq_int(ss->p);
		/* Fork P3. */
		pid = child_fork(ss, depth + 1);
		if (ss->collapse) {
			/* Wait for P1 and P4 to exit, triggering collapse. */
			while (!ss->exiting[1] ||
			    (ss->block_xfer && !ss->exiting[4]))
				usleep(SLEEP_TIME_US);
			/*
			 * This is racy, just guess at how long it may take
			 * them to finish exiting.
			 */
			usleep(100 * 1000);
		}
		/* P2 waits for P3's modification. */
		child_wait_p3_write(ss);
		child_verify(ss, depth, pid, oldval);
		ss->okay = true;
		ss->exit = true;
		break;
	case 3:
		/*
		 * P3 writes the memory.  A page is faulted into the shared
		 * P2/P3 shadow object.  P2's mapping of the page in P1's
		 * object must now be shot down, or else P2 will wrongly
		 * continue to have that page mapped.
		 */
		child_write(ss, mypid, ss->modlen);
		ss->p3_did_write = true;
		dprintf("P3 (pid %d) wrote its pid\n", mypid);
		break;
	case 4:
		/* Just hang around until P3 is done writing. */
		oldval = atomic_load_acq_int(ss->p);
		child_wait_p3_write(ss);
		child_verify(ss, depth, oldval, oldval);
		break;
	default:
		child_errx("Bad depth %d", depth);
	}

	dprintf("P%d (pid %d) exiting\n", depth, mypid);
	ss->exiting[depth] = true;
	exit(0);
}

static void
do_shared_shadow_inval(bool collapse, bool block_xfer, bool full_mod)
{
	struct shared_state *ss;
	pid_t pid;

	pid = getpid();

	dprintf("P0 (pid %d) %s(collapse=%d, block_xfer=%d, full_mod=%d)\n",
	    pid, __func__, (int)collapse, (int)block_xfer, (int)full_mod);

	g_pagesize = sysconf(_SC_PAGESIZE);
	ATF_REQUIRE(g_pagesize > 0);

	ATF_REQUIRE(procctl(P_PID, pid, PROC_REAP_ACQUIRE, NULL) == 0);

	/* Shared memory for coordination. */
	ss = mmap(NULL, sizeof(*ss), PROT_READ | PROT_WRITE,
	    MAP_SHARED | MAP_ANON, -1, 0);
	ATF_REQUIRE(ss != MAP_FAILED);

	ss->len = 2 * 1024 * 1024 + g_pagesize; /* 2 MB + page size */
	ss->modlen = full_mod ? ss->len : ss->len / 2;
	ss->collapse = collapse;
	ss->block_xfer = block_xfer;

	pid = fork();
	ATF_REQUIRE(pid != -1);
	if (pid == 0)
		child(ss, 1);

	/* Wait for all descendants to exit. */
	do {
		pid = wait(NULL);
	} while (pid != -1 || errno != ECHILD);

	atomic_thread_fence_acq();
	ATF_REQUIRE(ss->okay);

	ATF_REQUIRE(munmap(ss, sizeof(*ss)) == 0);
	ATF_REQUIRE(procctl(P_PID, getpid(), PROC_REAP_RELEASE, NULL) == 0);
}

#ifdef STANDALONE
int
main(void)
{

	do_shared_shadow_inval(false, false, false);
	do_shared_shadow_inval(false, false, true);
	do_shared_shadow_inval(false, true, false);
	do_shared_shadow_inval(false, true, true);
	do_shared_shadow_inval(true, false, false);
	do_shared_shadow_inval(true, false, true);
	do_shared_shadow_inval(true, true, false);
	do_shared_shadow_inval(true, true, true);
	printf("pass\n");
}
#else

#define SHARED_SHADOW_INVAL_TC(suffix, collapse, block_xfer, full_mod)	\
ATF_TC_WITHOUT_HEAD(shared_shadow_inval__##suffix);			\
ATF_TC_BODY(shared_shadow_inval__##suffix, tc)				\
{									\
	do_shared_shadow_inval(collapse, block_xfer, full_mod);		\
}

SHARED_SHADOW_INVAL_TC(nocollapse_noblockxfer_nofullmod, false, false, false);
SHARED_SHADOW_INVAL_TC(nocollapse_noblockxfer_fullmod, false, false, true);
SHARED_SHADOW_INVAL_TC(nocollapse_blockxfer_nofullmod, false, true, false);
SHARED_SHADOW_INVAL_TC(nocollapse_blockxfer_fullmod, false, true, true);
SHARED_SHADOW_INVAL_TC(collapse_noblockxfer_nofullmod, true, false, false);
SHARED_SHADOW_INVAL_TC(collapse_noblockxfer_fullmod, true, false, true);
SHARED_SHADOW_INVAL_TC(collapse_blockxfer_nofullmod, true, true, false);
SHARED_SHADOW_INVAL_TC(collapse_blockxfer_fullmod, true, true, true);

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp,
	    shared_shadow_inval__nocollapse_noblockxfer_nofullmod);
	ATF_TP_ADD_TC(tp, shared_shadow_inval__nocollapse_noblockxfer_fullmod);
	ATF_TP_ADD_TC(tp, shared_shadow_inval__nocollapse_blockxfer_nofullmod);
	ATF_TP_ADD_TC(tp, shared_shadow_inval__nocollapse_blockxfer_fullmod);
	ATF_TP_ADD_TC(tp, shared_shadow_inval__collapse_noblockxfer_nofullmod);
	ATF_TP_ADD_TC(tp, shared_shadow_inval__collapse_noblockxfer_fullmod);
	ATF_TP_ADD_TC(tp, shared_shadow_inval__collapse_blockxfer_nofullmod);
	ATF_TP_ADD_TC(tp, shared_shadow_inval__collapse_blockxfer_fullmod);

	return atf_no_error();
}
#endif
