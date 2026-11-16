/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: Osvaldo Janeri Filho <ojanerif@amd.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/linker.h>
#include <sys/wait.h>

#include <atf-c.h>
#include <errno.h>
#include <memstat.h>
#include <pmc.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define	CYCLES	3

/* Live allocations and bytes of the kernel's "pmc" malloc(9) type. */
static void
pmc_malloc_stats(uint64_t *count, uint64_t *bytes)
{
	struct memory_type_list *mtl;
	struct memory_type *mtp;

	mtl = memstat_mtl_alloc();
	ATF_REQUIRE(mtl != NULL);
	ATF_REQUIRE_MSG(memstat_sysctl_malloc(mtl, 0) == 0,
	    "memstat_sysctl_malloc: %s",
	    memstat_strerror(memstat_mtl_geterror(mtl)));
	mtp = memstat_mtl_find(mtl, ALLOCATOR_MALLOC, "pmc");
	ATF_REQUIRE_MSG(mtp != NULL, "no \"pmc\" malloc type");
	*count = memstat_get_count(mtp);
	*bytes = memstat_get_bytes(mtp);
	memstat_mtl_free(mtl);
}

/*
 * Count with a process PMC and release it.  This runs in a child because
 * libpmc keeps the system call number of the first hwpmc it finds.
 */
static void
use_hwpmc(void)
{
	pmc_id_t id;
	pmc_value_t v;
	pid_t pid;
	int status;
	volatile int i;

	pid = fork();
	ATF_REQUIRE(pid >= 0);
	if (pid == 0) {
		if (pmc_init() != 0 ||
		    pmc_allocate("SOFT-CLOCK.HARD", PMC_MODE_TC, 0,
		    PMC_CPU_ANY, &id, 0) != 0 ||
		    pmc_attach(id, 0) != 0 || pmc_start(id) != 0)
			_exit(1);
		for (i = 0; i < 10000000; i++)
			;
		if (pmc_stop(id) != 0 || pmc_read(id, &v) != 0 ||
		    pmc_detach(id, 0) != 0 || pmc_release(id) != 0)
			_exit(2);
		_exit(0);
	}
	ATF_REQUIRE(waitpid(pid, &status, 0) == pid);
	ATF_REQUIRE_MSG(WIFEXITED(status) && WEXITSTATUS(status) == 0,
	    "using hwpmc failed in the child (status %#x)", status);
}

ATF_TC_WITH_CLEANUP(unload_frees_memory);
ATF_TC_HEAD(unload_frees_memory, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Unloading hwpmc.ko returns every allocation of the pmc "
	    "malloc(9) type");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(unload_frees_memory, tc)
{
	struct kld_file_stat st;
	uint64_t bytes, bytes0, count, count0;
	int fileid, i;

	fileid = kldfind("hwpmc.ko");
	if (fileid < 0)
		atf_tc_skip("hwpmc.ko is not loaded as a module");
	st.version = sizeof(st);
	ATF_REQUIRE(kldstat(fileid, &st) == 0);
	if (st.refs > 1)
		atf_tc_skip("hwpmc.ko is referenced by %d other files",
		    st.refs - 1);
	if (kldunload(fileid) != 0) {
		if (errno == EBUSY)
			atf_tc_skip("hwpmc.ko is in use");
		atf_tc_fail("kldunload(hwpmc.ko) failed: %s", strerror(errno));
	}
	pmc_malloc_stats(&count0, &bytes0);

	for (i = 0; i < CYCLES; i++) {
		fileid = kldload("hwpmc");
		ATF_REQUIRE_MSG(fileid >= 0, "kldload(hwpmc) failed: %s",
		    strerror(errno));
		use_hwpmc();
		ATF_REQUIRE_MSG(kldunload(fileid) == 0,
		    "kldunload(hwpmc.ko) failed: %s", strerror(errno));
		pmc_malloc_stats(&count, &bytes);
		ATF_CHECK_MSG(count == count0 && bytes == bytes0,
		    "cycle %d: pmc type at %ju allocations, %ju bytes after "
		    "unload; %ju, %ju before load", i, (uintmax_t)count,
		    (uintmax_t)bytes, (uintmax_t)count0, (uintmax_t)bytes0);
	}
}
ATF_TC_CLEANUP(unload_frees_memory, tc)
{
	if (kldfind("hwpmc.ko") < 0)
		(void)kldload("hwpmc");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, unload_frees_memory);

	return (atf_no_error());
}
