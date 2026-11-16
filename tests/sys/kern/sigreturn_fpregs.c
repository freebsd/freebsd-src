/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Piotr Kubaj <pkubaj@FreeBSD.org>
 */

/*
 * Stress test for the lazy reload of the floating point registers after
 * sigreturn(2) on powerpc64.
 *
 * sigreturn turns the FPU off for the thread; the next FP instruction takes
 * the FP unavailable trap, which reloads the registers from the PCB.  The
 * checker loads f14-f31 with a pattern, sends itself a signal and stores the
 * registers again, so that store takes the trap.  Between the two it pings a
 * helper thread pinned to its own CPU, so the hardware registers hold the
 * helper's values when the reload runs; more helpers on the same CPU are
 * woken from another CPU so that ULE preempts the checker at random points
 * of the sequence (only a remote wakeup preempts immediately).  Any mismatch
 * means the kernel handed the checker another thread's registers.  Before
 * 1f1043d550ea ("powerpc: keep FP, VMX and VSX ownership changes atomic
 * with preemption") a POWER9 saw about one mismatch per 3000 round trips.
 */

#include <sys/param.h>
#include <sys/cpuset.h>
#include <sys/thr.h>

#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <atf-c.h>

/* sigreturn_fpregs_ppc64.S */
void	fp_roundtrip(const double *in, double *out, long tid, long sig,
	    long wfd, long rfd);
void	fp_load_all(const double *pat);

#define	DURATION	10	/* seconds */
#define	NPOLL		4	/* 3 woken by the waker, #3 woken by the checker */
#define	NREGS		18	/* f14-f31 */

static double pollpat[NPOLL][32];
static int pin_in[NPOLL][2], pin_out[NPOLL][2];
static int checker_cpu, waker_cpu;

static void
xfer(int fd, int wr, char *c)
{
	ssize_t n;

	do
		n = wr ? write(fd, c, 1) : read(fd, c, 1);
	while (n < 0 && errno == EINTR);
	if (n != 1)
		err(1, wr ? "write" : "read");
}

static void
pin_thread(int cpu)
{
	cpuset_t cs;

	CPU_ZERO(&cs);
	CPU_SET(cpu, &cs);
	if (cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1, sizeof(cs),
	    &cs) != 0)
		err(1, "cpuset_setaffinity(%d)", cpu);
}

static void
handler(int sig __unused)
{
}

static void *
polluter(void *arg)
{
	int i = (int)(intptr_t)arg;
	char c;

	pin_thread(checker_cpu);
	if (i == NPOLL - 1)	/* the checker's helper: distinctive FPSCR */
		__asm__ volatile("mtfsb1 31");
	for (;;) {
		xfer(pin_in[i][0], 0, &c);
		fp_load_all(pollpat[i]);
		xfer(pin_out[i][1], 1, &c);
	}
	return (NULL);
}

static void *
waker(void *arg __unused)
{
	uint64_t until;
	char c = 'x';
	int i;

	pin_thread(waker_cpu);
	for (;;) {
		for (i = 0; i < NPOLL - 1; i++) {
			/*
			 * Random 0-20 us delay so the preemption samples the
			 * checker's loop at uniformly random phases.
			 */
			until = __builtin_ppc_get_timebase() + random() % 10240;
			while (__builtin_ppc_get_timebase() < until)
				;
			xfer(pin_in[i][1], 1, &c);
			xfer(pin_out[i][0], 0, &c);
		}
	}
	return (NULL);
}

/* Pick two distinct CPUs this process may run on. */
static int
pick_cpus(void)
{
	cpuset_t cs;
	int i, n;

	CPU_ZERO(&cs);
	ATF_REQUIRE_EQ(0, cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID,
	    -1, sizeof(cs), &cs));
	n = 0;
	for (i = 0; i < CPU_SETSIZE; i++) {
		if (!CPU_ISSET(i, &cs))
			continue;
		if (n == 0)
			checker_cpu = i;
		else if (n == 1)
			waker_cpu = i;
		n++;
	}
	return (n);
}

ATF_TC(sigreturn_fpregs);
ATF_TC_HEAD(sigreturn_fpregs, tc)
{
	atf_tc_set_md_var(tc, "descr", "FP registers reloaded after sigreturn "
	    "are the thread's own under preemption");
	atf_tc_set_md_var(tc, "timeout", "60");
}
ATF_TC_BODY(sigreturn_fpregs, tc)
{
	double in[NREGS], out[NREGS + 1];
	struct sigaction sa;
	sigset_t set;
	pthread_t pt;
	uint64_t iters, bad, v, first_exp, first_got;
	time_t end;
	long tid;
	int first_reg, k, p, r;

	if (pick_cpus() < 2)
		atf_tc_skip("needs two CPUs");
	pin_thread(checker_cpu);

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handler;
	ATF_REQUIRE_EQ(0, sigaction(SIGUSR1, &sa, NULL));

	/* The helpers never see the signal. */
	sigemptyset(&set);
	sigaddset(&set, SIGUSR1);
	ATF_REQUIRE_EQ(0, pthread_sigmask(SIG_BLOCK, &set, NULL));
	for (p = 0; p < NPOLL; p++) {
		for (r = 0; r < 32; r++) {
			v = 0xbad0000000000000ULL | ((uint64_t)(p + 1) << 32) |
			    r;
			memcpy(&pollpat[p][r], &v, sizeof(v));
		}
		ATF_REQUIRE_EQ(0, pipe(pin_in[p]));
		ATF_REQUIRE_EQ(0, pipe(pin_out[p]));
		ATF_REQUIRE_EQ(0, pthread_create(&pt, NULL, polluter,
		    (void *)(intptr_t)p));
	}
	ATF_REQUIRE_EQ(0, pthread_create(&pt, NULL, waker, NULL));
	ATF_REQUIRE_EQ(0, pthread_sigmask(SIG_UNBLOCK, &set, NULL));

	for (r = 0; r < NREGS; r++) {
		v = 0xc0de000000000000ULL | (uint64_t)(r + 14);
		memcpy(&in[r], &v, sizeof(v));
	}
	thr_self(&tid);

	iters = bad = 0;
	first_reg = -1;
	first_exp = first_got = 0;
	end = time(NULL) + DURATION;
	while (time(NULL) < end) {
		for (k = 0; k < 2000; k++) {
			memset(out, 0, sizeof(out));
			fp_roundtrip(in, out, tid, SIGUSR1, pin_in[NPOLL - 1][1],
			    pin_out[NPOLL - 1][0]);
			iters++;
			if (memcmp(in, out, sizeof(in)) == 0)
				continue;
			bad++;
			if (first_reg >= 0)
				continue;
			for (r = 0; r < NREGS; r++) {
				memcpy(&first_exp, &in[r], sizeof(first_exp));
				memcpy(&first_got, &out[r], sizeof(first_got));
				if (first_exp != first_got) {
					first_reg = r + 14;
					break;
				}
			}
		}
	}
	if (bad != 0)
		atf_tc_fail("%ju of %ju sigreturn round trips came back with "
		    "another thread's FP registers (first: f%d expected %#jx "
		    "got %#jx)", (uintmax_t)bad, (uintmax_t)iters, first_reg,
		    (uintmax_t)first_exp, (uintmax_t)first_got);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, sigreturn_fpregs);

	return (atf_no_error());
}
