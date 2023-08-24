/*-
 * Copyright (c) 2023 Dmitry Chagin <dchagin@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * PR:	272585
 * Test provided by John F. Carr
 */

#include <sys/systm.h>
#include <sys/mman.h>
#include <vm/vm_param.h>

#include <atf-c.h>
#include <signal.h>
#include <unistd.h>

static void
sigsegv_handler(int sig __unused)
{

	atf_tc_fail("Invalid stack protection mode after grows");
}

ATF_TC_WITHOUT_HEAD(mprotect_exec_test);
ATF_TC_BODY(mprotect_exec_test, tc)
{
	long pagesize;
	char *addr, *guard;
	size_t alloc_size;

	signal(SIGSEGV, sigsegv_handler);

	pagesize = sysconf(_SC_PAGESIZE);
	ATF_REQUIRE(pagesize > 0);

	alloc_size = SGROWSIZ * 2;
	addr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE,
	    MAP_STACK | MAP_PRIVATE | MAP_ANON, -1, 0);
	ATF_REQUIRE(addr != MAP_FAILED);

	/*
	 * Change prot of the last page in the mmaped stack area.
	 */
	guard = addr + alloc_size - SGROWSIZ;
	ATF_REQUIRE(mprotect(guard, pagesize, PROT_NONE) == 0);

	((volatile char *)guard)[-1];
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, mprotect_exec_test);

	return (atf_no_error());
}
