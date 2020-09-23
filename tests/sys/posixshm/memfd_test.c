/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <atf-c.h>
#include <errno.h>
#include <unistd.h>

ATF_TC_WITHOUT_HEAD(basic);
ATF_TC_BODY(basic, tc)
{
	struct stat sb;
	int fd;
	char buf[8];

	ATF_REQUIRE((fd = memfd_create("...", 0)) != -1);

	/* write(2) should grow us out automatically. */
	ATF_REQUIRE(write(fd, buf, sizeof(buf)) == sizeof(buf));
	ATF_REQUIRE(fstat(fd, &sb) == 0);
	ATF_REQUIRE(sb.st_size == sizeof(buf));

	/* ftruncate(2) must succeed without seals */
	ATF_REQUIRE(ftruncate(fd, 2 * (sizeof(buf) - 1)) == 0);

	/* write(2) again must not be limited by ftruncate(2) size. */
	ATF_REQUIRE(write(fd, buf, sizeof(buf)) == sizeof(buf));

	/* Sanity check. */
	ATF_REQUIRE(fstat(fd, &sb) == 0);
	ATF_REQUIRE(sb.st_size == 2 * sizeof(buf));

	close(fd);
}

ATF_TC_WITHOUT_HEAD(cloexec);
ATF_TC_BODY(cloexec, tc)
{
	int fd_nocl, fd_cl;

	ATF_REQUIRE((fd_nocl = memfd_create("...", 0)) != -1);
	ATF_REQUIRE((fd_cl = memfd_create("...", MFD_CLOEXEC)) != -1);

	ATF_REQUIRE((fcntl(fd_nocl, F_GETFD) & FD_CLOEXEC) == 0);
	ATF_REQUIRE((fcntl(fd_cl, F_GETFD) & FD_CLOEXEC) != 0);

	close(fd_nocl);
	close(fd_cl);
}

ATF_TC_WITHOUT_HEAD(disallowed_sealing);
ATF_TC_BODY(disallowed_sealing, tc)
{
	int fd;

	ATF_REQUIRE((fd = memfd_create("...", 0)) != -1);
	ATF_REQUIRE(fcntl(fd, F_GET_SEALS) == F_SEAL_SEAL);
	ATF_REQUIRE(fcntl(fd, F_ADD_SEALS, F_SEAL_WRITE) == -1);
	ATF_REQUIRE(errno == EPERM);

	close(fd);
}

#define	BUF_SIZE	1024

ATF_TC_WITHOUT_HEAD(write_seal);
ATF_TC_BODY(write_seal, tc)
{
	int fd;
	char *addr, buf[BUF_SIZE];

	ATF_REQUIRE((fd = memfd_create("...", MFD_ALLOW_SEALING)) != -1);
	ATF_REQUIRE(ftruncate(fd, BUF_SIZE) == 0);

	/* Write once, then we'll seal it and try again */
	ATF_REQUIRE(write(fd, buf, BUF_SIZE) == BUF_SIZE);
	ATF_REQUIRE(lseek(fd, 0, SEEK_SET) == 0);

	addr = mmap(0, BUF_SIZE, (PROT_READ | PROT_WRITE), MAP_PRIVATE, fd, 0);
	ATF_REQUIRE(addr != MAP_FAILED);
	ATF_REQUIRE(munmap(addr, BUF_SIZE) == 0);

	ATF_REQUIRE(fcntl(fd, F_ADD_SEALS, F_SEAL_WRITE) == 0);

	ATF_REQUIRE(write(fd, buf, BUF_SIZE) == -1);
	ATF_REQUIRE(errno == EPERM);

	ATF_REQUIRE(mmap(0, BUF_SIZE, (PROT_READ | PROT_WRITE), MAP_SHARED,
	    fd, 0) == MAP_FAILED);
	ATF_REQUIRE(errno == EACCES);

	close(fd);
}

ATF_TC_WITHOUT_HEAD(mmap_write_seal);
ATF_TC_BODY(mmap_write_seal, tc)
{
	int fd;
	char *addr, *paddr, *raddr;

	ATF_REQUIRE((fd = memfd_create("...", MFD_ALLOW_SEALING)) != -1);
	ATF_REQUIRE(ftruncate(fd, BUF_SIZE) == 0);

	/* Map it, both shared and privately */
	addr = mmap(0, BUF_SIZE, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, 0);
	ATF_REQUIRE(addr != MAP_FAILED);
	paddr = mmap(0, BUF_SIZE, (PROT_READ | PROT_WRITE), MAP_PRIVATE, fd, 0);
	ATF_REQUIRE(paddr != MAP_FAILED);
	raddr = mmap(0, BUF_SIZE, PROT_READ, MAP_SHARED, fd, 0);
	ATF_REQUIRE(raddr != MAP_FAILED);

	/* Now try to seal it before unmapping */
	ATF_REQUIRE(fcntl(fd, F_ADD_SEALS, F_SEAL_WRITE) == -1);
	ATF_REQUIRE(errno == EBUSY);

	ATF_REQUIRE(munmap(addr, BUF_SIZE) == 0);

	/*
	 * This should fail, because raddr still exists and it was spawned from
	 * a r/w fd.
	 */
	ATF_REQUIRE(fcntl(fd, F_ADD_SEALS, F_SEAL_WRITE) == -1);
	ATF_REQUIRE(errno == EBUSY);

	ATF_REQUIRE(munmap(raddr, BUF_SIZE) == 0);
	/* This one should succeed; only the private mapping remains. */
	ATF_REQUIRE(fcntl(fd, F_ADD_SEALS, F_SEAL_WRITE) == 0);

	ATF_REQUIRE(munmap(paddr, BUF_SIZE) == 0);
	ATF_REQUIRE(mmap(0, BUF_SIZE, (PROT_READ | PROT_WRITE), MAP_SHARED,
	    fd, 0) == MAP_FAILED);
	ATF_REQUIRE(errno == EACCES);

	/* Make sure we can still map privately r/w or shared r/o. */
	paddr = mmap(0, BUF_SIZE, (PROT_READ | PROT_WRITE), MAP_PRIVATE, fd, 0);
	ATF_REQUIRE(paddr != MAP_FAILED);
	raddr = mmap(0, BUF_SIZE, PROT_READ, MAP_SHARED, fd, 0);
	ATF_REQUIRE(raddr != MAP_FAILED);
	ATF_REQUIRE(munmap(raddr, BUF_SIZE) == 0);
	ATF_REQUIRE(munmap(paddr, BUF_SIZE) == 0);

	close(fd);
}

static int
memfd_truncate_test(int initial_size, int dest_size, int seals)
{
	int err, fd;

	ATF_REQUIRE((fd = memfd_create("...", MFD_ALLOW_SEALING)) != -1);
	ATF_REQUIRE(ftruncate(fd, initial_size) == 0);

	ATF_REQUIRE(fcntl(fd, F_ADD_SEALS, seals) == 0);

	err = ftruncate(fd, dest_size);
	if (err != 0)
		err = errno;
	close(fd);
	return (err);
}

ATF_TC_WITHOUT_HEAD(truncate_seals);
ATF_TC_BODY(truncate_seals, tc)
{

	ATF_REQUIRE(memfd_truncate_test(4, 8, F_SEAL_GROW) == EPERM);
	ATF_REQUIRE(memfd_truncate_test(8, 4, F_SEAL_SHRINK) == EPERM);
	ATF_REQUIRE(memfd_truncate_test(8, 4, F_SEAL_GROW) == 0);
	ATF_REQUIRE(memfd_truncate_test(4, 8, F_SEAL_SHRINK) == 0);

	ATF_REQUIRE(memfd_truncate_test(4, 8, F_SEAL_GROW | F_SEAL_SHRINK) ==
	    EPERM);
	ATF_REQUIRE(memfd_truncate_test(8, 4, F_SEAL_GROW | F_SEAL_SHRINK) ==
	    EPERM);
	ATF_REQUIRE(memfd_truncate_test(4, 4, F_SEAL_GROW | F_SEAL_SHRINK) ==
	    0);
}

ATF_TC_WITHOUT_HEAD(get_seals);
ATF_TC_BODY(get_seals, tc)
{
	int fd;
	int seals;

	ATF_REQUIRE((fd = memfd_create("...", MFD_ALLOW_SEALING)) != -1);
	ATF_REQUIRE(fcntl(fd, F_GET_SEALS) == 0);

	ATF_REQUIRE(fcntl(fd, F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_GROW) == 0);
	seals = fcntl(fd, F_GET_SEALS);
	ATF_REQUIRE(seals == (F_SEAL_WRITE | F_SEAL_GROW));

	close(fd);
}

ATF_TC_WITHOUT_HEAD(dup_seals);
ATF_TC_BODY(dup_seals, tc)
{
	char buf[8];
	int fd, fdx;
	int seals;

	ATF_REQUIRE((fd = memfd_create("...", MFD_ALLOW_SEALING)) != -1);
	ATF_REQUIRE((fdx = dup(fd)) != -1);
	ATF_REQUIRE(fcntl(fd, F_GET_SEALS) == 0);

	ATF_REQUIRE(fcntl(fd, F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_GROW) == 0);
	seals = fcntl(fd, F_GET_SEALS);
	ATF_REQUIRE(seals == (F_SEAL_WRITE | F_SEAL_GROW));

	seals = fcntl(fdx, F_GET_SEALS);
	ATF_REQUIRE(seals == (F_SEAL_WRITE | F_SEAL_GROW));

	/* Make sure the seal's actually being applied at the inode level */
	ATF_REQUIRE(write(fdx, buf, sizeof(buf)) == -1);
	ATF_REQUIRE(errno == EPERM);

	ATF_REQUIRE(mmap(0, BUF_SIZE, (PROT_READ | PROT_WRITE), MAP_SHARED,
	    fdx, 0) == MAP_FAILED);
	ATF_REQUIRE(errno == EACCES);

	close(fd);
	close(fdx);
}

ATF_TC_WITHOUT_HEAD(immutable_seals);
ATF_TC_BODY(immutable_seals, tc)
{
	int fd;

	ATF_REQUIRE((fd = memfd_create("...", MFD_ALLOW_SEALING)) != -1);

	ATF_REQUIRE(fcntl(fd, F_ADD_SEALS, F_SEAL_SEAL) == 0);
	ATF_REQUIRE(fcntl(fd, F_ADD_SEALS, F_SEAL_GROW) == -1);
	ATF_REQUIRE_MSG(errno == EPERM,
	    "Added unique grow seal after restricting seals");

	close(fd);

	/*
	 * Also check that adding a seal that already exists really doesn't
	 * do anything once we're sealed.
	 */
	ATF_REQUIRE((fd = memfd_create("...", MFD_ALLOW_SEALING)) != -1);

	ATF_REQUIRE(fcntl(fd, F_ADD_SEALS, F_SEAL_GROW | F_SEAL_SEAL) == 0);
	ATF_REQUIRE(fcntl(fd, F_ADD_SEALS, F_SEAL_GROW) == -1);
	ATF_REQUIRE_MSG(errno == EPERM,
	    "Added duplicate grow seal after restricting seals");
	close(fd);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, basic);
	ATF_TP_ADD_TC(tp, cloexec);
	ATF_TP_ADD_TC(tp, disallowed_sealing);
	ATF_TP_ADD_TC(tp, write_seal);
	ATF_TP_ADD_TC(tp, mmap_write_seal);
	ATF_TP_ADD_TC(tp, truncate_seals);
	ATF_TP_ADD_TC(tp, get_seals);
	ATF_TP_ADD_TC(tp, dup_seals);
	ATF_TP_ADD_TC(tp, immutable_seals);
	return (atf_no_error());
}
