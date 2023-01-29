/*-
 * Copyright (c) 2022 Dmitry Chagin <dchagin@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/stdint.h>
#include <sys/sysctl.h>

#include <errno.h>
#include <sched.h>

#include <atf-c.h>

static uint32_t maxcpuid;
static uint32_t maxcpus;
static uint32_t cpus;

static uint32_t
support_getcpus(void)
{
	uint32_t val;
	size_t sz = sizeof(val);

	ATF_REQUIRE(sysctlbyname("kern.smp.cpus", &val, &sz, NULL, 0) == 0);
	return (val);
}

static uint32_t
support_getmaxcpus(void)
{
	uint32_t val;
	size_t sz = sizeof(val);

	ATF_REQUIRE(sysctlbyname("kern.smp.maxcpus", &val, &sz, NULL, 0) == 0);
	return (val);
}

static uint32_t
support_getmaxcpuid(void)
{
	cpuset_t *set;
	int setsize, rv;
	uint32_t i, id;

	for (i = 1; i < maxcpus; i++) {
		setsize = CPU_ALLOC_SIZE(i);
		set = CPU_ALLOC(i);
		ATF_REQUIRE(set != NULL);
		CPU_ZERO_S(setsize, set);
		rv = cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID,
		    -1, setsize, set);
		if (rv == 0) {
			id = __BIT_FLS(i, set) - 1;
			CPU_FREE(set);
			break;
		}
		CPU_FREE(set);
	}
	ATF_REQUIRE(rv == 0);
	return (id);
}

ATF_TC_WITHOUT_HEAD(test_setinvalidcpu);
ATF_TC_BODY(test_setinvalidcpu, tc)
{
	size_t cpusetsize;
	cpuset_t *set;

	cpusetsize = CPU_ALLOC_SIZE(maxcpuid + 1);
	set = CPU_ALLOC(maxcpuid + 1);
	ATF_REQUIRE(set != NULL);
	CPU_ZERO_S(cpusetsize, set);
	CPU_SET_S(maxcpuid + 1, cpusetsize, set);
	CPU_SET_S(maxcpuid - 1, cpusetsize, set);
	ATF_REQUIRE(sched_setaffinity(0, cpusetsize, set) == 0);
	CPU_FREE(set);

	cpusetsize = CPU_ALLOC_SIZE(maxcpus + 1);
	set = CPU_ALLOC(maxcpus + 1);
	ATF_REQUIRE(set != NULL);
	CPU_ZERO_S(cpusetsize, set);
	CPU_SET_S(maxcpuid + 1, cpusetsize, set);
	CPU_SET_S(maxcpuid - 1, cpusetsize, set);
	ATF_REQUIRE(cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID,
	    -1, cpusetsize, set) == -1);
	ATF_REQUIRE_EQ(errno, EINVAL);
	CPU_FREE(set);
}

ATF_TC_WITHOUT_HEAD(test_setvalidcpu);
ATF_TC_BODY(test_setvalidcpu, tc)
{
	size_t cpusetsize;
	cpuset_t *set;
	int cpu;

	ATF_REQUIRE(maxcpuid < maxcpus);
	cpu = maxcpuid > 1 ? maxcpuid - 1 : 0;

	cpusetsize = CPU_ALLOC_SIZE(maxcpus + 1);
	set = CPU_ALLOC(maxcpus + 1);
	ATF_REQUIRE(set != NULL);
	CPU_ZERO_S(cpusetsize, set);
	CPU_SET_S(cpu, cpusetsize, set);
	ATF_REQUIRE(cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID,
	    -1, cpusetsize, set) == 0);
	ATF_REQUIRE_EQ(cpu, sched_getcpu());
	CPU_FREE(set);
}

ATF_TC_WITHOUT_HEAD(test_setzeroset1);
ATF_TC_BODY(test_setzeroset1, tc)
{
	size_t cpusetsize;
	cpuset_t *set;

	cpusetsize = CPU_ALLOC_SIZE(maxcpuid + 1);
	set = CPU_ALLOC(maxcpuid + 1);
	ATF_REQUIRE(set != NULL);
	CPU_ZERO_S(cpusetsize, set);
	ATF_REQUIRE(sched_setaffinity(0, cpusetsize, set) == -1);
	ATF_REQUIRE_EQ(errno, EINVAL);
	CPU_FREE(set);
}

ATF_TC_WITHOUT_HEAD(test_setzeroset2);
ATF_TC_BODY(test_setzeroset2, tc)
{
	size_t cpusetsize;
	cpuset_t *set;

	cpusetsize = CPU_ALLOC_SIZE(maxcpuid + 1);
	set = CPU_ALLOC(maxcpuid + 1);
	ATF_REQUIRE(set != NULL);
	CPU_ZERO_S(cpusetsize, set);
	ATF_REQUIRE(cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID,
	    -1, cpusetsize, set) == -1);
	ATF_REQUIRE_EQ(errno, EDEADLK);
	CPU_FREE(set);
}

ATF_TC_WITHOUT_HEAD(test_setmaxsetsize);
ATF_TC_BODY(test_setmaxsetsize, tc)
{
	size_t cpusetsize;
	cpuset_t *set;

	cpusetsize = CPU_ALLOC_SIZE(maxcpus * 2);
	set = CPU_ALLOC(maxcpus * 2);
	ATF_REQUIRE(set != NULL);
	CPU_ZERO_S(cpusetsize, set);
	ATF_REQUIRE(CPU_COUNT_S(cpusetsize, set) == 0);
	CPU_SET_S(0, cpusetsize, set);
	ATF_REQUIRE(CPU_COUNT_S(cpusetsize, set) == 1);
	ATF_REQUIRE(cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID,
	    -1, cpusetsize, set) == 0);

	CPU_ZERO_S(cpusetsize, set);
	CPU_SET_S(maxcpuid, cpusetsize, set);
	ATF_REQUIRE(cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID,
	    -1, cpusetsize, set) == 0);

	CPU_ZERO_S(cpusetsize, set);
	CPU_SET_S(maxcpuid + 1, cpusetsize, set);
	ATF_REQUIRE(cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID,
	    -1, cpusetsize, set) == -1);
	ATF_REQUIRE_EQ(errno, EINVAL);
	CPU_FREE(set);
}

ATF_TC_WITHOUT_HEAD(test_setminsetsize);
ATF_TC_BODY(test_setminsetsize, tc)
{
	size_t cpusetsize = 1;
	int8_t set;

	if (cpus <= 8)
		return;

	set = 1;
	ATF_REQUIRE(cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID,
	    -1, cpusetsize, (const cpuset_t *)&set) == 0);
	set = 0;
	ATF_REQUIRE(cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID,
	    -1, cpusetsize, (const cpuset_t *)&set) == -1);
	ATF_REQUIRE_EQ(errno, EDEADLK);
}

ATF_TC_WITHOUT_HEAD(test_getminsetsize);
ATF_TC_BODY(test_getminsetsize, tc)
{
	size_t cpusetsize = 1;
	int8_t set = 0;

	if (cpus < 9)
		return;
	ATF_REQUIRE(cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID,
	    -1, cpusetsize, (cpuset_t *)&set) == -1);
	ATF_REQUIRE_EQ(errno, ERANGE);
}

ATF_TC_WITHOUT_HEAD(test_getsetsize);
ATF_TC_BODY(test_getsetsize, tc)
{
	size_t cpusetsize;
	cpuset_t *set;

	cpusetsize = CPU_ALLOC_SIZE(maxcpuid + 1);
	set = CPU_ALLOC(maxcpuid + 1);
	ATF_REQUIRE(set != NULL);
	CPU_ZERO_S(cpusetsize, set);
	ATF_REQUIRE(cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID,
	    -1, cpusetsize, set) == 0);
	CPU_FREE(set);
}

ATF_TC_WITHOUT_HEAD(test_holes);
ATF_TC_BODY(test_holes, tc)
{
	cpuset_t *set;
	int cpusetsize;

	cpusetsize = CPU_ALLOC_SIZE(maxcpus * 2);
	set = CPU_ALLOC(maxcpus * 2);
	ATF_REQUIRE(set != NULL);
	CPU_ZERO_S(cpusetsize, set);
	ATF_REQUIRE(CPU_COUNT_S(cpusetsize, set) == 0);
	CPU_SET_S(maxcpuid, cpusetsize, set);
	ATF_REQUIRE(CPU_COUNT_S(cpusetsize, set) == 1);
	ATF_REQUIRE(cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID,
	    -1, cpusetsize, set) == 0);

	CPU_ZERO_S(cpusetsize, set);
	ATF_REQUIRE(CPU_COUNT_S(cpusetsize, set) == 0);
	CPU_SET_S(maxcpuid + 1, cpusetsize, set);
	ATF_REQUIRE(CPU_COUNT_S(cpusetsize, set) == 1);
	ATF_REQUIRE(cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID,
	    -1, cpusetsize, set) == -1);
	ATF_REQUIRE_EQ(errno, EINVAL);

	ATF_REQUIRE(CPU_COUNT_S(cpusetsize, set) == 1);
	ATF_REQUIRE(cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID,
	    -1, cpusetsize, set) == 0);
	ATF_REQUIRE(CPU_ISSET_S(maxcpuid + 1, cpusetsize, set) == false);
	ATF_REQUIRE(CPU_ISSET_S(maxcpuid, cpusetsize, set) == true);
	ATF_REQUIRE_EQ(maxcpuid, (uint32_t)sched_getcpu());
}

ATF_TP_ADD_TCS(tp)
{

	cpus = support_getcpus();
	maxcpus = support_getmaxcpus();
	maxcpuid = support_getmaxcpuid();

	ATF_TP_ADD_TC(tp, test_setinvalidcpu);
	ATF_TP_ADD_TC(tp, test_setvalidcpu);
	ATF_TP_ADD_TC(tp, test_setzeroset1);
	ATF_TP_ADD_TC(tp, test_setzeroset2);

	ATF_TP_ADD_TC(tp, test_setminsetsize);
	ATF_TP_ADD_TC(tp, test_setmaxsetsize);

	ATF_TP_ADD_TC(tp, test_getminsetsize);
	ATF_TP_ADD_TC(tp, test_getsetsize);

	ATF_TP_ADD_TC(tp, test_holes);

	return (atf_no_error());
}
