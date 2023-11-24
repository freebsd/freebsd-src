/*
 * Copyright (c) 2021 Dell Inc. or its subsidiaries. All Rights Reserved.
 * Copyright (c) 2022 The FreeBSD Foundation
 *
 * Portions of this software were developed by Mark Johnston under sponsorship
 * from the FreeBSD Foundation.
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
 * invalidated by COW from another mapping.  In particular, when
 * minherit(INHERT_SHARE) is applied to a COW mapping, a subsequently
 * forked child process will share the parent's shadow object.  Thus,
 * pages already mapped into one sharing process may be written from
 * another, triggering a copy into the shadow object.  The VM system
 * expects that a fully shadowed page is unmapped, but at one point the
 * use of a shared shadow object could break this invariant.
 *
 * This is a regression test for an issue isolated by rlibby@FreeBSD.org
 * from an issue detected by stress2's collapse.sh by jeff@FreeBSD.org.
 * The issue became CVE-2021-29626.
 *
 * This file is written as an ATF test suite but may be compiled as a
 * standalone program with -DSTANDALONE (and optionally -DDEBUG).
 */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/procctl.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

#include <machine/atomic.h>

#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef STANDALONE
#define	ATF_REQUIRE(x)	do {		\
	if (!(x))			\
		errx(1, "%s", #x);	\
} while (0)
#else
#include <atf-c.h>
#endif

#ifdef DEBUG
#define	dprintf(...)	printf(__VA_ARGS__)
#else
#define	dprintf(...)
#endif

#define	DEPTH	5

#define	FLAG_COLLAPSE		0x1
#define	FLAG_BLOCK_XFER		0x2
#define	FLAG_FULLMOD		0x4
#define FLAG_MASK		(FLAG_COLLAPSE | FLAG_BLOCK_XFER | FLAG_FULLMOD)

struct shared_state {
	void *p;
	size_t len;
	size_t modlen;
	size_t pagesize;
	bool collapse;
	bool block_xfer;
	bool lazy_cow;
	bool okay;
	volatile bool exiting[DEPTH];
	volatile bool exit;
	volatile bool p3_did_write;
};

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

	for (i = 0; i < ss->len; i += ss->pagesize)
		(void)((volatile char *)ss->p)[i];
}

static void
child_write(struct shared_state *ss, int val, size_t len)
{
	size_t i;

	for (i = 0; i < len; i += ss->pagesize)
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

	for (i = 0; i < ss->len; i += ss->pagesize) {
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
		if (!ss->lazy_cow) {
			if (mlock(ss->p, ss->len) == -1)
				child_err("mlock");
			if (mprotect(ss->p, ss->len, PROT_READ) == -1)
				child_err("mprotect");
		}
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
		 * shadow chain is collapsed.  This may be redundant if the
		 * (read-only) mappings were copied by fork(), but it doesn't
		 * hurt.
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
		 * Use mlock()+mprotect() to trigger the COW.  This
		 * exercises a different COW handler than the one used
		 * for lazy faults.
		 */
		if (!ss->lazy_cow) {
			if (mlock(ss->p, ss->len) == -1)
				child_err("mlock");
			if (mprotect(ss->p, ss->len, PROT_READ | PROT_WRITE) ==
			    -1)
				child_err("mprotect");
		}

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
do_one_shared_shadow_inval(bool lazy_cow, size_t pagesize, size_t len,
    unsigned int flags)
{
	struct shared_state *ss;
	pid_t pid;
	int status;

	pid = getpid();

	dprintf("P0 (pid %d) %s(collapse=%d, block_xfer=%d, full_mod=%d)\n",
	    pid, __func__, (int)collapse, (int)block_xfer, (int)full_mod);

	ATF_REQUIRE(procctl(P_PID, pid, PROC_REAP_ACQUIRE, NULL) == 0);

	/* Shared memory for coordination. */
	ss = mmap(NULL, sizeof(*ss), PROT_READ | PROT_WRITE,
	    MAP_SHARED | MAP_ANON, -1, 0);
	ATF_REQUIRE(ss != MAP_FAILED);

	ss->len = len;
	ss->modlen = (flags & FLAG_FULLMOD) ? ss->len : ss->len / 2;
	ss->pagesize = pagesize;
	ss->collapse = (flags & FLAG_COLLAPSE) != 0;
	ss->block_xfer = (flags & FLAG_BLOCK_XFER) != 0;
	ss->lazy_cow = lazy_cow;

	pid = fork();
	ATF_REQUIRE(pid != -1);
	if (pid == 0)
		child(ss, 1);

	/* Wait for all descendants to exit. */
	do {
		pid = wait(&status);
		ATF_REQUIRE(WIFEXITED(status));
	} while (pid != -1 || errno != ECHILD);

	atomic_thread_fence_acq();
	ATF_REQUIRE(ss->okay);

	ATF_REQUIRE(munmap(ss, sizeof(*ss)) == 0);
	ATF_REQUIRE(procctl(P_PID, getpid(), PROC_REAP_RELEASE, NULL) == 0);
}

static void
do_shared_shadow_inval(bool lazy_cow)
{
	size_t largepagesize, pagesize, pagesizes[MAXPAGESIZES], sysctllen;

	sysctllen = sizeof(pagesizes);
	ATF_REQUIRE(sysctlbyname("hw.pagesizes", pagesizes, &sysctllen, NULL,
	    0) == 0);
	ATF_REQUIRE(sysctllen >= sizeof(size_t));

	pagesize = pagesizes[0];
	largepagesize = MAXPAGESIZES >= 2 &&
	    sysctllen >= 2 * sizeof(size_t) && pagesizes[1] != 0 ?
	    pagesizes[1] : 2 * 1024 * 1024;

	for (unsigned int i = 0; i <= FLAG_MASK; i++) {
		do_one_shared_shadow_inval(lazy_cow, pagesize,
		    pagesize, i);
		do_one_shared_shadow_inval(lazy_cow, pagesize,
		    2 * pagesize, i);
		do_one_shared_shadow_inval(lazy_cow, pagesize,
		    largepagesize - pagesize, i);
		do_one_shared_shadow_inval(lazy_cow, pagesize,
		    largepagesize, i);
		do_one_shared_shadow_inval(lazy_cow, pagesize,
		    largepagesize + pagesize, i);
	}
}

static void
do_shared_shadow_inval_eager(void)
{
	struct rlimit rl;

	rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;
	ATF_REQUIRE(setrlimit(RLIMIT_MEMLOCK, &rl) == 0);

	do_shared_shadow_inval(false);
}

static void
do_shared_shadow_inval_lazy(void)
{
	do_shared_shadow_inval(true);
}

#ifdef STANDALONE
int
main(void)
{
	do_shared_shadow_inval_lazy();
	do_shared_shadow_inval_eager();
	printf("pass\n");
}
#else
ATF_TC_WITHOUT_HEAD(shared_shadow_inval__lazy_cow);
ATF_TC_BODY(shared_shadow_inval__lazy_cow, tc)
{
	do_shared_shadow_inval_lazy();
}

ATF_TC(shared_shadow_inval__eager_cow);
ATF_TC_HEAD(shared_shadow_inval__eager_cow, tc)
{
	/* Needed to raise the mlock() limit. */
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(shared_shadow_inval__eager_cow, tc)
{
	do_shared_shadow_inval_eager();
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, shared_shadow_inval__lazy_cow);
	ATF_TP_ADD_TC(tp, shared_shadow_inval__eager_cow);
	return (atf_no_error());
}
#endif /* !STANDALONE */
