/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 */

/*
 * Every IBS NMI hwpmc declines must be claimed after the cpuctl(4) write.
 * machdep.panic_on_nmi is cleared so that an unfixed kernel fails instead
 * of panicking.
 */

#include <sys/param.h>
#include <sys/cpuctl.h>
#include <sys/cpuset.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>

#include <machine/specialreg.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

#define	FETCH_EN	(1ULL << 48)
#define	OP_EN		(1ULL << 17)

#define	TOGGLES		20000
#define	FETCH_MAXCNT	0x100ULL	/* complete within the spin window */
#define	OP_MAXCNT	0x1000ULL
#define	MAX_SPIN	200000		/* TSC ticks */

#define	STATE_FILE	"ibs_stray_state"

struct stray_state {
	int		panic_on_nmi;
	uint64_t	fetch;
	uint64_t	op;
};

static int
msr_rw(int fd, u_long cmd, uint32_t msr, uint64_t *val)
{
	cpuctl_msr_args_t args;

	args.msr = msr;
	args.data = *val;
	if (ioctl(fd, cmd, &args) != 0)
		return (errno);
	if (cmd == CPUCTL_RDMSR)
		*val = args.data;
	return (0);
}

static uint64_t
intr_ignored(void)
{
	uint64_t v;
	size_t len;

	len = sizeof(v);
	ATF_REQUIRE_MSG(sysctlbyname("kern.hwpmc.stats.intr_ignored", &v, &len,
	    NULL, 0) == 0, "kern.hwpmc.stats.intr_ignored: %s",
	    strerror(errno));
	return (v);
}

/* A kernel without the fix has no counter and claims nothing. */
static uint64_t
nmis_claimed(void)
{
	u_long v;
	size_t len;

	len = sizeof(v);
	if (sysctlbyname("machdep.ibs_ctl_write_nmis", &v, &len, NULL,
	    0) != 0) {
		ATF_REQUIRE_MSG(errno == ENOENT,
		    "machdep.ibs_ctl_write_nmis: %s", strerror(errno));
		return (0);
	}
	return (v);
}

static void
set_panic_on_nmi(int v)
{
	ATF_REQUIRE_MSG(sysctlbyname("machdep.panic_on_nmi", NULL, NULL, &v,
	    sizeof(v)) == 0, "machdep.panic_on_nmi: %s", strerror(errno));
}

static int
setup(void)
{
	cpuctl_cpuid_args_t ext, vendor;
	struct stray_state st;
	uint64_t v;
	size_t len;
	FILE *f;
	int error, fd;

	fd = open("/dev/cpuctl0", O_RDWR);
	if (fd < 0)
		atf_tc_skip("/dev/cpuctl0: %s", strerror(errno));
	vendor.level = 0;
	ext.level = 0x80000001;
	error = 0;
	if (ioctl(fd, CPUCTL_CPUID, &vendor) != 0 ||
	    ioctl(fd, CPUCTL_CPUID, &ext) != 0)
		error = errno;
	if (error != 0) {
		close(fd);
		atf_tc_fail("CPUCTL_CPUID: %s", strerror(error));
	}
	if (!(vendor.data[1] == 0x68747541 && vendor.data[3] == 0x69746e65 &&
	    vendor.data[2] == 0x444d4163) &&		/* AuthenticAMD */
	    !(vendor.data[1] == 0x6f677948 && vendor.data[3] == 0x6e65476e &&
	    vendor.data[2] == 0x656e6975)) {		/* HygonGenuine */
		close(fd);
		atf_tc_skip("not an AMD or Hygon CPU");
	}
	if ((ext.data[2] & AMDID2_IBS) == 0) {
		close(fd);
		atf_tc_skip("CPU does not implement IBS");
	}
	len = sizeof(v);
	if (sysctlbyname("kern.hwpmc.stats.intr_ignored", &v, &len,
	    NULL, 0) != 0) {
		close(fd);
		atf_tc_skip("hwpmc is not loaded (IBS NMIs stay masked)");
	}

	len = sizeof(st.panic_on_nmi);
	if (sysctlbyname("machdep.panic_on_nmi", &st.panic_on_nmi, &len,
	    NULL, 0) != 0)
		error = errno;
	st.fetch = st.op = 0;
	if (error == 0)
		error = msr_rw(fd, CPUCTL_RDMSR, MSR_AMD_IBS_FETCH_CTL,
		    &st.fetch);
	if (error == 0)
		error = msr_rw(fd, CPUCTL_RDMSR, MSR_AMD_IBS_OP_CTL, &st.op);
	if (error != 0) {
		close(fd);
		atf_tc_fail("reading the initial state: %s", strerror(error));
	}
	if ((st.fetch & FETCH_EN) != 0 || (st.op & OP_EN) != 0) {
		close(fd);
		atf_tc_skip("IBS sampling is already enabled on CPU 0 "
		    "(fetch 0x%jx, op 0x%jx); run in a reserved environment "
		    "with no other IBS users", (uintmax_t)st.fetch,
		    (uintmax_t)st.op);
	}

	f = fopen(STATE_FILE, "w");
	if (f == NULL || fwrite(&st, sizeof(st), 1, f) != 1 ||
	    fclose(f) != 0) {
		close(fd);
		atf_tc_fail("cannot save the state to %s", STATE_FILE);
	}
	return (fd);
}

static void
toggle(const char *unit, uint32_t msr, uint64_t enable)
{
	cpuset_t set;
	uint64_t claimed, deadline, ignored, val;
	int fd, i;

	CPU_ZERO(&set);
	CPU_SET(0, &set);
	ATF_REQUIRE(cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
	    sizeof(set), &set) == 0);
	fd = setup();
	set_panic_on_nmi(0);

	srandom((unsigned)__builtin_ia32_rdtsc());
	ignored = intr_ignored();
	claimed = nmis_claimed();
	for (i = 0; i < TOGGLES; i++) {
		val = enable;
		ATF_REQUIRE_EQ(msr_rw(fd, CPUCTL_WRMSR, msr, &val), 0);
		/* Let the disabling write land anywhere in the period. */
		deadline = __builtin_ia32_rdtsc() +
		    (uint64_t)(random() % (MAX_SPIN + 1));
		while (__builtin_ia32_rdtsc() < deadline)
			;
		val = 0;
		ATF_REQUIRE_EQ(msr_rw(fd, CPUCTL_WRMSR, msr, &val), 0);
	}
	close(fd);

	ignored = intr_ignored() - ignored;
	claimed = nmis_claimed() - claimed;
	ATF_CHECK_MSG(ignored == claimed,
	    "%ju IBS %s NMIs declined by hwpmc, %ju claimed after the cpuctl "
	    "write, after %d toggles", (uintmax_t)ignored, unit,
	    (uintmax_t)claimed, TOGGLES);
}

static void
restore_msr(int fd, const char *unit, uint32_t msr, uint64_t saved, int *bad)
{
	uint64_t val;
	int i;

	for (i = 0; i < 3; i++) {
		val = saved;
		if (msr_rw(fd, CPUCTL_WRMSR, msr, &val) == 0 &&
		    msr_rw(fd, CPUCTL_RDMSR, msr, &val) == 0 && val == saved)
			return;
	}
	warnx("cannot restore the IBS %s control of CPU 0", unit);
	(*bad)++;
}

static void
cleanup(void)
{
	struct stray_state st;
	int bad, fd, i, v;
	size_t len;
	FILE *f;

	f = fopen(STATE_FILE, "r");
	if (f == NULL)
		return;		/* skipped before anything was changed */
	if (fread(&st, sizeof(st), 1, f) != 1) {
		fclose(f);
		errx(EXIT_FAILURE, "cannot read the saved state");
	}
	fclose(f);

	bad = 0;
	fd = open("/dev/cpuctl0", O_RDWR);
	if (fd < 0) {
		warn("/dev/cpuctl0");
		bad++;
	} else {
		restore_msr(fd, "fetch", MSR_AMD_IBS_FETCH_CTL, st.fetch, &bad);
		restore_msr(fd, "op", MSR_AMD_IBS_OP_CTL, st.op, &bad);
		close(fd);
	}
	for (i = 0; i < 3; i++) {
		len = sizeof(v);
		if (sysctlbyname("machdep.panic_on_nmi", NULL, NULL,
		    &st.panic_on_nmi, sizeof(st.panic_on_nmi)) == 0 &&
		    sysctlbyname("machdep.panic_on_nmi", &v, &len, NULL,
		    0) == 0 && v == st.panic_on_nmi)
			break;
	}
	if (i == 3) {
		warnx("cannot restore machdep.panic_on_nmi to %d",
		    st.panic_on_nmi);
		bad++;
	}
	if (bad != 0)
		errx(EXIT_FAILURE, "state not restored");
}

ATF_TC_WITH_CLEANUP(raw_op_toggle_claimed);
ATF_TC_HEAD(raw_op_toggle_claimed, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Toggling IBS op through cpuctl leaves no unclaimed NMI");
}

ATF_TC_BODY(raw_op_toggle_claimed, tc)
{
	toggle("op", MSR_AMD_IBS_OP_CTL, OP_EN | OP_MAXCNT);
}

ATF_TC_CLEANUP(raw_op_toggle_claimed, tc)
{
	cleanup();
}

ATF_TC_WITH_CLEANUP(raw_fetch_toggle_claimed);
ATF_TC_HEAD(raw_fetch_toggle_claimed, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Toggling IBS fetch through cpuctl leaves no unclaimed NMI");
}

ATF_TC_BODY(raw_fetch_toggle_claimed, tc)
{
	toggle("fetch", MSR_AMD_IBS_FETCH_CTL, FETCH_EN | FETCH_MAXCNT);
}

ATF_TC_CLEANUP(raw_fetch_toggle_claimed, tc)
{
	cleanup();
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, raw_op_toggle_claimed);
	ATF_TP_ADD_TC(tp, raw_fetch_toggle_claimed);

	return (atf_no_error());
}
