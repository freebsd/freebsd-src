/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Mark Johnston <markj@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
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

#include <sys/cdefs.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

static void
test_wired_copy_on_write(void *addr, size_t len)
{
	int status, val;
	pid_t pid;

	pid = fork();
	if (pid == -1)
		atf_tc_fail("fork() failed: %s", strerror(errno));
	if (pid == 0) {
		if (mlock(addr, len) != 0)
			_exit(1);
		if (ptrace(PT_TRACE_ME, 0, NULL, 0) != 0)
			_exit(2);
		if (raise(SIGSTOP) != 0)
			_exit(3);
		if (munlock(addr, len) != 0)
			_exit(4);
		_exit(0);
	}

	ATF_REQUIRE(waitpid(pid, &status, 0) == pid);
	ATF_REQUIRE_MSG(!WIFEXITED(status),
	    "child exited with status %d", WEXITSTATUS(status));
	ATF_REQUIRE(WIFSTOPPED(status));
	ATF_REQUIRE(WSTOPSIG(status) == SIGSTOP);

	errno = 0;
	val = ptrace(PT_READ_D, pid, addr, 0);
	ATF_REQUIRE(errno == 0);
	ATF_REQUIRE(ptrace(PT_WRITE_D, pid, addr, val) == 0);
	ATF_REQUIRE(ptrace(PT_CONTINUE, pid, (caddr_t)1, 0) == 0);
	ATF_REQUIRE(waitpid(pid, &status, 0) == pid);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE_MSG(WEXITSTATUS(status) == 0,
	    "child exited with status %d", WSTOPSIG(status));
}

/*
 * Use ptrace(2) to trigger a copy-on-write fault of anonymous memory.
 */
ATF_TC_WITHOUT_HEAD(mlock__copy_on_write_anon);
ATF_TC_BODY(mlock__copy_on_write_anon, tc)
{
	char *addr;
	int len;

	len = getpagesize();
	addr = mmap(NULL, len, PROT_READ, MAP_ANON, -1, 0);
	ATF_REQUIRE(addr != MAP_FAILED);

	test_wired_copy_on_write(addr, len);
}

/*
 * Use ptrace(2) to trigger a copy-on-write fault of a read-only text page.
 */
ATF_TC_WITHOUT_HEAD(mlock__copy_on_write_vnode);
ATF_TC_BODY(mlock__copy_on_write_vnode, tc)
{
	void *addr;
	int len;

	len = getpagesize();
	addr = (void *)((uintptr_t)test_wired_copy_on_write & ~(len - 1));

	test_wired_copy_on_write(addr, len);
}

/*
 * Try truncating and then resizing an mlock()ed mapping.
 */
ATF_TC_WITHOUT_HEAD(mlock__truncate_and_resize);
ATF_TC_BODY(mlock__truncate_and_resize, tc)
{
	char filename[16];
	char *addr;
	int fd, i, len;

	snprintf(filename, sizeof(filename), "tmp.XXXXXX");
	fd = mkstemp(filename);
	ATF_REQUIRE(fd >= 0);
	ATF_REQUIRE(unlink(filename) == 0);

	len = getpagesize();
	ATF_REQUIRE(ftruncate(fd, len) == 0);

	addr = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	ATF_REQUIRE(addr != MAP_FAILED);
	ATF_REQUIRE(mlock(addr, len) == 0);
	memset(addr, 1, len);
	ATF_REQUIRE(ftruncate(fd, 0) == 0);
	ATF_REQUIRE(ftruncate(fd, len) == 0);
	for (i = 0; i < len; i++)
		ATF_REQUIRE(addr[i] == 0);
	ATF_REQUIRE(munlock(addr, len) == 0);
}

/*
 * Make sure that we can munlock() a truncated mapping.
 */
ATF_TC_WITHOUT_HEAD(mlock__truncate_and_unlock);
ATF_TC_BODY(mlock__truncate_and_unlock, tc)
{
	char filename[16];
	void *addr;
	int fd, len;

	snprintf(filename, sizeof(filename), "tmp.XXXXXX");
	fd = mkstemp(filename);
	ATF_REQUIRE(fd >= 0);
	ATF_REQUIRE(unlink(filename) == 0);

	len = getpagesize();
	ATF_REQUIRE(ftruncate(fd, len) == 0);

	addr = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	ATF_REQUIRE(addr != MAP_FAILED);
	ATF_REQUIRE(mlock(addr, len) == 0);
	ATF_REQUIRE(ftruncate(fd, 0) == 0);
	ATF_REQUIRE(munlock(addr, len) == 0);
}

/*
 * Exercise a corner case involving an interaction between mlock() and superpage
 * creation: a truncation of the object backing a mapping results in the
 * truncated region being unmapped by the pmap, but does not affect the logical
 * mapping.  In particular, the truncated region remains mlock()ed.  If the
 * mapping is later extended, a page fault in the formerly truncated region can
 * result in superpage creation via a call to pmap_enter(psind = 1).
 */
ATF_TC(mlock__superpage_fault);
ATF_TC_HEAD(mlock__superpage_fault, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(mlock__superpage_fault, tc)
{
	struct rlimit rlim;
	void *addr1, *addr2;
	size_t len, pagesizes[MAXPAGESIZES];
	int count, error, shmfd;
	char vec;

	count = getpagesizes(pagesizes, MAXPAGESIZES);
	ATF_REQUIRE_MSG(count >= 1,
	    "failed to get page sizes: %s", strerror(errno));
	if (count == 1)
		atf_tc_skip("system does not support multiple page sizes");
	len = pagesizes[1];

	error = getrlimit(RLIMIT_MEMLOCK, &rlim);
	ATF_REQUIRE_MSG(error == 0, "getrlimit: %s", strerror(errno));
	rlim.rlim_cur += len;
	rlim.rlim_max += len;
	error = setrlimit(RLIMIT_MEMLOCK, &rlim);
	ATF_REQUIRE_MSG(error == 0, "setrlimit: %s", strerror(errno));

	shmfd = shm_open(SHM_ANON, O_RDWR | O_CREAT, 0600);
	ATF_REQUIRE_MSG(shmfd >= 0, "shm_open: %s", strerror(errno));
	error = ftruncate(shmfd, len);
	ATF_REQUIRE_MSG(error == 0, "ftruncate: %s", strerror(errno));

	addr1 = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_SHARED | MAP_ALIGNED_SUPER, shmfd, 0);
	ATF_REQUIRE_MSG(addr1 != MAP_FAILED, "mmap: %s", strerror(errno));
	ATF_REQUIRE_MSG(((uintptr_t)addr1 & (len - 1)) == 0,
	    "addr %p is misaligned", addr1);
	addr2 = mmap(NULL, len, PROT_READ,
	    MAP_SHARED | MAP_ALIGNED_SUPER, shmfd, 0);
	ATF_REQUIRE_MSG(addr2 != MAP_FAILED, "mmap: %s", strerror(errno));
	ATF_REQUIRE_MSG(((uintptr_t)addr2 & (len - 1)) == 0,
	    "addr %p is misaligned", addr2);

	memset(addr1, 0x42, len);
	error = mincore(addr1, pagesizes[0], &vec);
	ATF_REQUIRE_MSG(error == 0, "mincore: %s", strerror(errno));
	if ((vec & MINCORE_SUPER) == 0)
		atf_tc_skip("initial superpage promotion failed");

	error = mlock(addr2, len);
	ATF_REQUIRE_MSG(error == 0, "mlock: %s", strerror(errno));
	error = mincore(addr2, pagesizes[0], &vec);
	ATF_REQUIRE_MSG(error == 0, "mincore: %s", strerror(errno));
	ATF_REQUIRE((vec & MINCORE_SUPER) != 0);

	/*
	 * Free a page back to the superpage reservation, demoting both
	 * mappings.
	 */
	error = ftruncate(shmfd, len - pagesizes[0]);
	ATF_REQUIRE_MSG(error == 0, "ftruncate: %s", strerror(errno));

	/*
	 * Extend the mapping back to its original size.
	 */
	error = ftruncate(shmfd, len);
	ATF_REQUIRE_MSG(error == 0, "ftruncate: %s", strerror(errno));

	/*
	 * Trigger re-promotion.
	 */
	error = mincore(addr1, pagesizes[0], &vec);
	ATF_REQUIRE_MSG(error == 0, "mincore: %s", strerror(errno));
	ATF_REQUIRE((vec & MINCORE_SUPER) == 0);
	memset((char *)addr1 + len - pagesizes[0], 0x43, pagesizes[0]);
	error = mincore(addr1, pagesizes[0], &vec);
	ATF_REQUIRE_MSG(error == 0, "mincore: %s", strerror(errno));
	ATF_REQUIRE((vec & MINCORE_SUPER) != 0);

	/*
	 * Trigger a read fault, which should install a superpage mapping
	 * without promotion.
	 */
	error = mincore(addr2, pagesizes[0], &vec);
	ATF_REQUIRE_MSG(error == 0, "mincore: %s", strerror(errno));
	ATF_REQUIRE((vec & MINCORE_SUPER) == 0);
	(void)atomic_load(
	    (_Atomic int *)(void *)((char *)addr2 + len - pagesizes[0]));
	error = mincore(addr2, pagesizes[0], &vec);
	ATF_REQUIRE_MSG(error == 0, "mincore: %s", strerror(errno));
	ATF_REQUIRE((vec & MINCORE_SUPER) != 0);

	/*
	 * Trigger demotion of the wired mapping.
	 */
	error = munlock(addr2, pagesizes[0]);
	ATF_REQUIRE_MSG(error == 0, "munlock: %s", strerror(errno));

	ATF_REQUIRE(close(shmfd) == 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, mlock__copy_on_write_anon);
	ATF_TP_ADD_TC(tp, mlock__copy_on_write_vnode);
	ATF_TP_ADD_TC(tp, mlock__truncate_and_resize);
	ATF_TP_ADD_TC(tp, mlock__truncate_and_unlock);
	ATF_TP_ADD_TC(tp, mlock__superpage_fault);

	return (atf_no_error());
}
