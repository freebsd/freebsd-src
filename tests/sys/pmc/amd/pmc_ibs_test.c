/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: Davi C.A <davi.chavesazevedo@amd.com>
 * Author: Osvaldo Janeri Filho <ojanerif@amd.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/cpuset.h>
#include <sys/event.h>
#include <sys/pmc.h>
#include <sys/pmclog.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <dev/hwpmc/hwpmc_ibs.h>

#include <atf-c.h>

#include <errno.h>
#include <fcntl.h>
#include <pmc.h>
#include <pmclog.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "amd_pmc_test_common.h"

#define	IBS_RATE		IBS_FETCH_MIN_RATE
#define	IBS_CAPS		(PMC_CAP_INTERRUPT | PMC_CAP_SYSTEM |	\
				    PMC_CAP_EDGE | PMC_CAP_QUALIFIER |	\
				    PMC_CAP_PRECISE)
#define	LOG_WAIT_SECONDS	5

static const struct pmc_classinfo *
ibs_classinfo(void)
{
	const struct pmc_cpuinfo *cpuinfo;

	ATF_REQUIRE_MSG(pmc_cpuinfo(&cpuinfo) == 0,
	    "pmc_cpuinfo() failed: %s", strerror(errno));
	return (amd_test_find_classinfo(cpuinfo, PMC_CLASS_IBS));
}

static void
skip_unless_ibs(void)
{
	u_int regs[4];

	amd_test_skip_unless_amd();
	do_cpuid(0x80000000, regs);
	if (regs[0] < CPUID_IBSID)
		atf_tc_skip("CPU lacks the IBS CPUID leaf");
	do_cpuid(CPUID_IBSID, regs);
	if ((regs[0] & (CPUID_IBSID_IBSFFV | CPUID_IBSID_FETCHSAM |
	    CPUID_IBSID_OPSAM)) != (CPUID_IBSID_IBSFFV |
	    CPUID_IBSID_FETCHSAM | CPUID_IBSID_OPSAM))
		atf_tc_skip("CPU lacks IBS Fetch or Op sampling");
	amd_test_skip_unless_hwpmc();
	if (ibs_classinfo() == NULL)
		atf_tc_skip("hwpmc does not expose PMC_CLASS_IBS");
}

static int
pin_cpu(void)
{
	cpuset_t mask;
	int cpu;

	ATF_REQUIRE(cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
	    sizeof(mask), &mask) == 0);
	cpu = CPU_FFS(&mask) - 1;
	ATF_REQUIRE(cpu >= 0);
	CPU_ZERO(&mask);
	CPU_SET(cpu, &mask);
	ATF_REQUIRE_MSG(cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
	    sizeof(mask), &mask) == 0, "cannot bind to cpu%d: %s", cpu,
	    strerror(errno));
	return (cpu);
}

static int
alloc_errno(const char *event, enum pmc_mode mode, int cpu,
    pmc_value_t rate)
{
	pmc_id_t id;

	id = PMC_ID_INVALID;
	errno = 0;
	if (pmc_allocate(event, mode, 0, cpu, &id, rate) == 0) {
		amd_test_release_pmc(id);
		return (0);
	}
	return (errno);
}

static pmc_id_t
alloc_attach_fetch(pid_t pid, pmc_value_t rate)
{
	pmc_id_t id;
	int error;

	id = PMC_ID_INVALID;
	ATF_REQUIRE_MSG(pmc_allocate("IBS-FETCH", PMC_MODE_TS, 0, PMC_CPU_ANY,
	    &id, rate) == 0, "pmc_allocate(TS IBS-FETCH) failed: %s",
	    strerror(errno));
	if (pmc_attach(id, pid) != 0) {
		error = errno;
		amd_test_release_pmc(id);
		atf_tc_fail("pmc_attach(IBS-FETCH, %d) failed: %s", pid,
		    strerror(error));
	}
	return (id);
}

ATF_TC(class_info);
ATF_TC_HEAD(class_info, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "pmc_cpuinfo() reports PMC_CLASS_IBS with two PMCs, width 0 and "
	    "the interrupt, system, edge, qualifier and precise capabilities");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(class_info, tc)
{
	const struct pmc_classinfo *ci;

	skip_unless_ibs();
	ci = ibs_classinfo();
	ATF_CHECK_EQ_MSG(ci->pm_num, 2, "IBS class has %u PMCs", ci->pm_num);
	ATF_CHECK_EQ_MSG(ci->pm_width, 0, "IBS class width is %u",
	    ci->pm_width);
	ATF_CHECK_MSG((ci->pm_caps & IBS_CAPS) == IBS_CAPS,
	    "IBS class caps %#x lack %#x", ci->pm_caps, IBS_CAPS);
}

ATF_TC(class_and_event_names);
ATF_TC_HEAD(class_and_event_names, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "libpmc names the class IBS and lists the FETCH and OP events");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(class_and_event_names, tc)
{
	const char **names;
	const char *name;
	bool fetch, op;
	int i, n;

	skip_unless_ibs();
	name = pmc_name_of_class(PMC_CLASS_IBS);
	ATF_REQUIRE(name != NULL);
	ATF_CHECK_STREQ(name, "IBS");

	ATF_REQUIRE_MSG(pmc_event_names_of_class(PMC_CLASS_IBS, &names,
	    &n) == 0, "pmc_event_names_of_class() failed: %s",
	    strerror(errno));
	fetch = op = false;
	for (i = 0; i < n; i++) {
		fetch |= strcasecmp(names[i], "FETCH") == 0;
		op |= strcasecmp(names[i], "OP") == 0;
	}
	free(names);
	ATF_CHECK_MSG(fetch && op, "IBS events: FETCH %s, OP %s",
	    fetch ? "listed" : "missing", op ? "listed" : "missing");
}

ATF_TC(pmcinfo_rows);
ATF_TC_HEAD(pmcinfo_rows, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "pmc_pmcinfo() has exactly one IBS-FETCH and one IBS-OP row");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(pmcinfo_rows, tc)
{
	size_t fetch, op;
	int cpu;

	skip_unless_ibs();
	cpu = pin_cpu();
	fetch = amd_test_count_pmc_rows_with_prefix(cpu, "IBS-FETCH");
	op = amd_test_count_pmc_rows_with_prefix(cpu, "IBS-OP");
	ATF_CHECK_MSG(fetch == 1 && op == 1,
	    "cpu%d has %zu IBS-FETCH and %zu IBS-OP rows", cpu, fetch, op);
}

ATF_TC(alloc_system_sampling);
ATF_TC_HEAD(alloc_system_sampling, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "IBS-FETCH and IBS-OP allocate as system-sampling PMCs; IBS-FETCH "
	    "reports the IBS capabilities and width 0");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(alloc_system_sampling, tc)
{
	pmc_id_t fetch, op;
	uint32_t caps, width;
	int cpu;

	skip_unless_ibs();
	cpu = pin_cpu();
	fetch = op = PMC_ID_INVALID;
	ATF_REQUIRE_MSG(pmc_allocate("IBS-FETCH", PMC_MODE_SS, 0, cpu, &fetch,
	    IBS_RATE) == 0, "pmc_allocate(IBS-FETCH) failed: %s",
	    strerror(errno));
	ATF_CHECK_MSG(pmc_allocate("IBS-OP", PMC_MODE_SS, 0, cpu, &op,
	    IBS_RATE) == 0, "pmc_allocate(IBS-OP) failed: %s",
	    strerror(errno));

	caps = 0;
	width = UINT32_MAX;
	ATF_CHECK(pmc_capabilities(fetch, &caps) == 0);
	ATF_CHECK(pmc_width(fetch, &width) == 0);
	ATF_CHECK_MSG((caps & IBS_CAPS) == IBS_CAPS,
	    "IBS-FETCH caps %#x lack %#x", caps, IBS_CAPS);
	ATF_CHECK_EQ_MSG(width, 0, "IBS-FETCH width is %u", width);

	amd_test_release_pmc(op);
	amd_test_release_pmc(fetch);
}

ATF_TC(alloc_refusals);
ATF_TC_HEAD(alloc_refusals, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "IBS allocation fails with EINVAL in counting mode, below the "
	    "minimum sample rate and with an unknown qualifier");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(alloc_refusals, tc)
{
	int cpu, error;

	skip_unless_ibs();
	cpu = pin_cpu();

	error = alloc_errno("IBS-FETCH", PMC_MODE_SC, cpu, IBS_RATE);
	ATF_CHECK_MSG(error == EINVAL, "counting mode: errno %d (%s)", error,
	    strerror(error));
	error = alloc_errno("IBS-OP", PMC_MODE_SS, cpu, IBS_OP_MIN_RATE - 1);
	ATF_CHECK_MSG(error == EINVAL, "rate %d: errno %d (%s)",
	    IBS_OP_MIN_RATE - 1, error, strerror(error));
	error = alloc_errno("IBS-OP,no_such_qualifier", PMC_MODE_SS, cpu,
	    IBS_RATE);
	ATF_CHECK_MSG(error == EINVAL, "unknown qualifier: errno %d (%s)",
	    error, strerror(error));
}

ATF_TC(fetch_attach_detach);
ATF_TC_HEAD(fetch_attach_detach, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "A process-mode IBS-FETCH PMC attaches to its owner, detaches and "
	    "is released");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(fetch_attach_detach, tc)
{
	pmc_id_t id;

	skip_unless_ibs();
	(void)pin_cpu();
	id = alloc_attach_fetch(getpid(), IBS_RATE);
	ATF_CHECK_MSG(pmc_detach(id, getpid()) == 0,
	    "pmc_detach() failed: %s", strerror(errno));
	amd_test_release_pmc(id);
}

ATF_TC(fetch_self_count);
ATF_TC_HEAD(fetch_self_count, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "A running IBS-FETCH PMC attached to its owner reads a non-zero "
	    "count while the owner makes system calls");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(fetch_self_count, tc)
{
	pmc_id_t id;
	pmc_value_t v;
	int i, round;

	skip_unless_ibs();
	(void)pin_cpu();
	id = alloc_attach_fetch(getpid(), IBS_RATE);
	ATF_REQUIRE_MSG(pmc_start(id) == 0, "pmc_start() failed: %s",
	    strerror(errno));

	/* The count restarts after each sample, so one read may see 0. */
	v = 0;
	for (round = 0; round < 100 && v == 0; round++) {
		for (i = 0; i < 10000; i++)
			(void)getppid();
		ATF_REQUIRE(pmc_read(id, &v) == 0);
	}
	ATF_CHECK(pmc_stop(id) == 0);
	ATF_CHECK(pmc_detach(id, getpid()) == 0);
	amd_test_release_pmc(id);
	ATF_CHECK_MSG(v != 0, "IBS-FETCH count stayed 0");
}

/* Wait for a sample of pid, or for CLOSELOG if closing. */
static int
wait_log(void *parser, int kq, pmc_id_t id, pid_t pid, bool closing,
    bool *sampled, bool *dropped)
{
	struct pmclog_ev ev;
	struct kevent ke;
	struct timespec deadline, left, now;
	const struct pmclog_ev_callchain *cc;
	const uint8_t *hdr;

	ATF_REQUIRE(clock_gettime(CLOCK_MONOTONIC, &deadline) == 0);
	deadline.tv_sec += LOG_WAIT_SECONDS;
	for (;;) {
		if (!closing && pmc_flush_logfile() != 0)
			return (errno);
		memset(&ev, 0, sizeof(ev));
		if (pmclog_read(parser, &ev) == 0) {
			if (ev.pl_type == PMCLOG_TYPE_DROPNOTIFY)
				*dropped = true;
			if (ev.pl_type != PMCLOG_TYPE_CALLCHAIN)
				continue;
			cc = &ev.pl_u.pl_cc;
			hdr = (const uint8_t *)cc->pl_pc;
			if (cc->pl_pmcid != id || cc->pl_pid != (uint32_t)pid ||
			    (cc->pl_cpuflags & PMC_CC_F_MULTIPART) == 0 ||
			    cc->pl_npc == 0 ||
			    hdr[0] != PMC_CC_MULTIPART_IBS_FETCH)
				return (EPROTO);
			*sampled = true;
			if (!closing)
				return (0);
			continue;
		}
		if (ev.pl_state != PMCLOG_EOF)
			return (EPROTO);
		if (ev.pl_type == PMCLOG_TYPE_CLOSELOG)
			return (closing ? 0 : EPROTO);
		ATF_REQUIRE(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
		if (timespeccmp(&now, &deadline, >=))
			return (ETIMEDOUT);
		timespecsub(&deadline, &now, &left);
		if (kevent(kq, NULL, 0, &ke, 1, &left) < 0 && errno != EINTR)
			return (errno);
	}
}

ATF_TC(fetch_child_samples);
ATF_TC_HEAD(fetch_child_samples, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "An IBS-FETCH PMC attached to a child writes IBS-FETCH samples of "
	    "that child to the owner's log, without drops");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "30");
}
ATF_TC_BODY(fetch_child_samples, tc)
{
	struct kevent ke;
	char path[] = "ibs-fetch.XXXXXX";
	volatile uint64_t x;
	uint64_t i;
	pmc_id_t id;
	pid_t child;
	const char *what;
	void *parser;
	bool configured, dropped, sampled;
	int error, fd, kq, reader, status;

	skip_unless_ibs();
	(void)pin_cpu();

	fd = mkstemp(path);
	ATF_REQUIRE(fd >= 0);
	reader = open(path, O_RDONLY);
	ATF_REQUIRE(reader >= 0);
	ATF_REQUIRE(unlink(path) == 0);
	parser = pmclog_open(reader);
	ATF_REQUIRE(parser != NULL);
	kq = kqueue();
	ATF_REQUIRE(kq >= 0);
	EV_SET(&ke, reader, EVFILT_VNODE, EV_ADD | EV_CLEAR,
	    NOTE_WRITE | NOTE_EXTEND, 0, NULL);
	ATF_REQUIRE(kevent(kq, &ke, 1, NULL, 0, NULL) == 0);

	child = fork();
	ATF_REQUIRE(child >= 0);
	if (child == 0) {
		if (raise(SIGSTOP) != 0)
			_exit(1);
		x = 1;
		for (i = 0; i < 500000000; i++)
			x = x * 1664525U + 1013904223U;
		_exit(0);
	}
	/* From here on, kill and reap the child before any failure. */
	what = NULL;
	configured = false;
	id = PMC_ID_INVALID;
	if (waitpid(child, &status, WUNTRACED) != child || !WIFSTOPPED(status))
		what = "stopping the child";
	else if (pmc_configure_logfile(fd) != 0)
		what = "pmc_configure_logfile";
	else {
		configured = true;
		if (pmc_allocate("IBS-FETCH", PMC_MODE_TS, 0, PMC_CPU_ANY, &id,
		    IBS_FETCH_MAX_RATE) != 0)
			what = "pmc_allocate";
		else if (pmc_attach(id, child) != 0)
			what = "pmc_attach";
		else if (pmc_start(id) != 0)
			what = "pmc_start";
		else if (kill(child, SIGCONT) != 0)
			what = "kill(SIGCONT)";
	}
	error = what != NULL ? errno : 0;

	sampled = dropped = false;
	if (error == 0)
		error = wait_log(parser, kq, id, child, false, &sampled,
		    &dropped);

	(void)kill(child, SIGKILL);
	(void)waitpid(child, &status, 0);
	if (id != PMC_ID_INVALID) {
		(void)pmc_stop(id);
		amd_test_release_pmc(id);
	}
	if (configured) {
		ATF_CHECK(pmc_flush_logfile() == 0);
		if (pmc_close_logfile() != 0)
			ATF_CHECK_MSG(false, "pmc_close_logfile() failed: %s",
			    strerror(errno));
		else if (error == 0)
			error = wait_log(parser, kq, id, child, true,
			    &sampled, &dropped);
		ATF_CHECK(pmc_configure_logfile(-1) == 0);
	}
	pmclog_close(parser);
	close(kq);
	close(reader);
	close(fd);

	ATF_REQUIRE_MSG(what == NULL, "%s failed: %s", what, strerror(error));
	ATF_REQUIRE_MSG(error == 0, "reading the log failed: %s",
	    strerror(error));
	ATF_CHECK_MSG(sampled && !dropped, "IBS-FETCH log: sampled %d, "
	    "dropped %d", sampled, dropped);
}

ATF_TC(getmsr_rejected);
ATF_TC_HEAD(getmsr_rejected, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "pmc_get_msr() fails with ENOSYS or EINVAL for an IBS-FETCH PMC");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(getmsr_rejected, tc)
{
	pmc_id_t id;
	uint32_t msr;
	int error, rc;

	skip_unless_ibs();
	(void)pin_cpu();
	id = alloc_attach_fetch(getpid(), IBS_RATE);
	errno = 0;
	rc = pmc_get_msr(id, &msr);
	error = errno;
	ATF_CHECK(pmc_detach(id, getpid()) == 0);
	amd_test_release_pmc(id);
	/* IBS has no GETMSR method; the sampling-mode check may come first. */
	ATF_CHECK_MSG(rc == -1 && (error == ENOSYS || error == EINVAL),
	    "pmc_get_msr() returned %d, errno %d (%s)", rc, error,
	    strerror(error));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, class_info);
	ATF_TP_ADD_TC(tp, class_and_event_names);
	ATF_TP_ADD_TC(tp, pmcinfo_rows);
	ATF_TP_ADD_TC(tp, alloc_system_sampling);
	ATF_TP_ADD_TC(tp, alloc_refusals);
	ATF_TP_ADD_TC(tp, fetch_attach_detach);
	ATF_TP_ADD_TC(tp, fetch_self_count);
	ATF_TP_ADD_TC(tp, fetch_child_samples);
	ATF_TP_ADD_TC(tp, getmsr_rejected);

	return (atf_no_error());
}
