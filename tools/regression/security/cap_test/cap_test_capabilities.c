/*-
 * Copyright (c) 2009-2011 Robert N. M. Watson
 * Copyright (c) 2011 Jonathan Anderson
 * All rights reserved.
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
 * Test whether various operations on capabilities are properly masked for
 * various object types.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/capability.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cap_test.h"

#define	SYSCALL_FAIL(syscall, message) \
	FAIL("%s:\t%s (rights 0x%jx)", #syscall, message, rights)

/*
 * Ensure that, if the capability had enough rights for the system call to
 * pass, then it did. Otherwise, ensure that the errno is ENOTCAPABLE;
 * capability restrictions should kick in before any other error logic.
 */
#define	CHECK_RESULT(syscall, rights_needed, succeeded)	do {		\
	if ((rights & (rights_needed)) == (rights_needed)) {		\
		if (!(succeeded))					\
			SYSCALL_FAIL(syscall, "failed");		\
	} else {							\
		if (succeeded)						\
			FAILX("%s:\tsucceeded when it shouldn't have"	\
			    " (rights 0x%jx)", #syscall, rights);	\
		else if (errno != ENOTCAPABLE)				\
			SYSCALL_FAIL(syscall, "errno != ENOTCAPABLE");	\
	}								\
} while (0)

/*
 * As above, but for the special mmap() case: unmap after successful mmap().
 */
#define	CHECK_MMAP_RESULT(rights_needed)	do {			\
	if ((rights & (rights_needed)) == (rights_needed)) {		\
		if (p == MAP_FAILED)					\
			SYSCALL_FAIL(mmap, "failed");			\
		else							\
			(void)munmap(p, getpagesize());			\
	} else {							\
		if (p != MAP_FAILED) {					\
			FAILX("%s:\tsucceeded when it shouldn't have"	\
			    " (rights 0x%jx)", "mmap", rights);		\
			(void)munmap(p, getpagesize());			\
		} else if (errno != ENOTCAPABLE)			\
			SYSCALL_FAIL(syscall, "errno != ENOTCAPABLE");	\
	}								\
} while (0)

/*
 * Given a file descriptor, create a capability with specific rights and
 * make sure only those rights work. 
*/
static int
try_file_ops(int fd, cap_rights_t rights)
{
	struct stat sb;
	struct statfs sf;
	int fd_cap, fd_capcap;
	ssize_t ssize, ssize2;
	off_t off;
	void *p;
	char ch;
	int ret, is_nfs;
	int success = PASSED;

	REQUIRE(fstatfs(fd, &sf));
	is_nfs = (strncmp("nfs", sf.f_fstypename, sizeof(sf.f_fstypename))
	    == 0);

	REQUIRE(fd_cap = cap_new(fd, rights));
	REQUIRE(fd_capcap = cap_new(fd_cap, rights));
	CHECK(fd_capcap != fd_cap);

	ssize = read(fd_cap, &ch, sizeof(ch));
	CHECK_RESULT(read, CAP_READ | CAP_SEEK, ssize >= 0);

	ssize = pread(fd_cap, &ch, sizeof(ch), 0);
	ssize2 = pread(fd_cap, &ch, sizeof(ch), 0);
	CHECK_RESULT(pread, CAP_READ, ssize >= 0);
	CHECK(ssize == ssize2);

	ssize = write(fd_cap, &ch, sizeof(ch));
	CHECK_RESULT(write, CAP_WRITE | CAP_SEEK, ssize >= 0);

	ssize = pwrite(fd_cap, &ch, sizeof(ch), 0);
	CHECK_RESULT(pwrite, CAP_WRITE, ssize >= 0);

	off = lseek(fd_cap, 0, SEEK_SET);
	CHECK_RESULT(lseek, CAP_SEEK, off >= 0);

	/*
	 * Note: this is not expected to work over NFS.
	 */
	ret = fchflags(fd_cap, UF_NODUMP);
	CHECK_RESULT(fchflags, CAP_FCHFLAGS,
	    (ret == 0) || (is_nfs && (errno == EOPNOTSUPP)));

	ret = fstat(fd_cap, &sb);
	CHECK_RESULT(fstat, CAP_FSTAT, ret == 0);

	p = mmap(NULL, getpagesize(), PROT_READ, MAP_SHARED, fd_cap, 0);
	CHECK_MMAP_RESULT(CAP_MMAP | CAP_READ);

	p = mmap(NULL, getpagesize(), PROT_WRITE, MAP_SHARED, fd_cap, 0);
	CHECK_MMAP_RESULT(CAP_MMAP | CAP_WRITE);

	p = mmap(NULL, getpagesize(), PROT_EXEC, MAP_SHARED, fd_cap, 0);
	CHECK_MMAP_RESULT(CAP_MMAP | CAP_MAPEXEC);

	p = mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE, MAP_SHARED,
	    fd_cap, 0);
	CHECK_MMAP_RESULT(CAP_MMAP | CAP_READ | CAP_WRITE);

	p = mmap(NULL, getpagesize(), PROT_READ | PROT_EXEC, MAP_SHARED,
	    fd_cap, 0);
	CHECK_MMAP_RESULT(CAP_MMAP | CAP_READ | CAP_MAPEXEC);

	p = mmap(NULL, getpagesize(), PROT_EXEC | PROT_WRITE, MAP_SHARED,
	    fd_cap, 0);
	CHECK_MMAP_RESULT(CAP_MMAP | CAP_MAPEXEC | CAP_WRITE);

	p = mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE | PROT_EXEC,
	    MAP_SHARED, fd_cap, 0);
	CHECK_MMAP_RESULT(CAP_MMAP | CAP_READ | CAP_WRITE | CAP_MAPEXEC);

	ret = fsync(fd_cap);
	CHECK_RESULT(fsync, CAP_FSYNC, ret == 0);

	ret = fchown(fd_cap, -1, -1);
	CHECK_RESULT(fchown, CAP_FCHOWN, ret == 0);

	ret = fchmod(fd_cap, 0644);
	CHECK_RESULT(fchmod, CAP_FCHMOD, ret == 0);

	/* XXX flock */

	ret = ftruncate(fd_cap, 0);
	CHECK_RESULT(ftruncate, CAP_FTRUNCATE, ret == 0);

	ret = fstatfs(fd_cap, &sf);
	CHECK_RESULT(fstatfs, CAP_FSTATFS, ret == 0);

	ret = fpathconf(fd_cap, _PC_NAME_MAX);
	CHECK_RESULT(fpathconf, CAP_FPATHCONF, ret >= 0);

	ret = futimes(fd_cap, NULL);
	CHECK_RESULT(futimes, CAP_FUTIMES, ret == 0);

	/* XXX select / poll / kqueue */

	close (fd_cap);
	return (success);
}

#define TRY(fd, rights) \
do { \
	if (success == PASSED) \
		success = try_file_ops(fd, rights); \
	else \
		/* We've already failed, but try the test anyway. */ \
		try_file_ops(fd, rights); \
} while (0)

int
test_capabilities(void)
{
	int fd;
	int success = PASSED;

	fd = open("/tmp/cap_test", O_RDWR | O_CREAT, 0644);
	if (fd < 0)
		err(-1, "open");

	if (cap_enter() < 0)
		err(-1, "cap_enter");

	/* XXX: Really want to try all combinations. */
	TRY(fd, CAP_READ);
	TRY(fd, CAP_READ | CAP_SEEK);
	TRY(fd, CAP_WRITE);
	TRY(fd, CAP_WRITE | CAP_SEEK);
	TRY(fd, CAP_READ | CAP_WRITE);
	TRY(fd, CAP_READ | CAP_WRITE | CAP_SEEK);
	TRY(fd, CAP_SEEK);
	TRY(fd, CAP_FCHFLAGS);
	TRY(fd, CAP_IOCTL);
	TRY(fd, CAP_FSTAT);
	TRY(fd, CAP_MMAP);
	TRY(fd, CAP_MMAP | CAP_READ);
	TRY(fd, CAP_MMAP | CAP_WRITE);
	TRY(fd, CAP_MMAP | CAP_MAPEXEC);
	TRY(fd, CAP_MMAP | CAP_READ | CAP_WRITE);
	TRY(fd, CAP_MMAP | CAP_READ | CAP_MAPEXEC);
	TRY(fd, CAP_MMAP | CAP_MAPEXEC | CAP_WRITE);
	TRY(fd, CAP_MMAP | CAP_READ | CAP_WRITE | CAP_MAPEXEC);
	TRY(fd, CAP_FCNTL);
	TRY(fd, CAP_POST_KEVENT);
	TRY(fd, CAP_POLL_KEVENT);
	TRY(fd, CAP_FSYNC);
	TRY(fd, CAP_FCHOWN);
	TRY(fd, CAP_FCHMOD);
	TRY(fd, CAP_FTRUNCATE);
	TRY(fd, CAP_FLOCK);
	TRY(fd, CAP_FSTATFS);
	TRY(fd, CAP_FPATHCONF);
	TRY(fd, CAP_FUTIMES);
	TRY(fd, CAP_ACL_GET);
	TRY(fd, CAP_ACL_SET);
	TRY(fd, CAP_ACL_DELETE);
	TRY(fd, CAP_ACL_CHECK);
	TRY(fd, CAP_EXTATTR_GET);
	TRY(fd, CAP_EXTATTR_SET);
	TRY(fd, CAP_EXTATTR_DELETE);
	TRY(fd, CAP_EXTATTR_LIST);
	TRY(fd, CAP_MAC_GET);
	TRY(fd, CAP_MAC_SET);

	/*
	 * Socket-specific.
	 */
	TRY(fd, CAP_GETPEERNAME);
	TRY(fd, CAP_GETSOCKNAME);
	TRY(fd, CAP_ACCEPT);

	return (success);
}
