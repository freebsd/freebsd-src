/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Ka Ho Ng under sponsorship from
 * the FreeBSD Foundation.
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

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <atf-c.h>
#include <fcntl.h>
#include <malloc.h>

static off_t file_max_blocks = 32;
static const char byte_to_fill = 0x5f;

static int
fill(int fd, off_t offset, off_t len)
{
	int error;
	size_t blen;
	char *buf;
	struct stat statbuf;
	blksize_t blocksize;

	if (fstat(fd, &statbuf) == -1)
		return (1);
	blocksize = statbuf.st_blksize;
	error = 0;
	buf = malloc(blocksize);
	if (buf == NULL)
		return (1);

	while (len > 0) {
		blen = len < (off_t)blocksize ? len : blocksize;
		memset(buf, byte_to_fill, blen);
		if (pwrite(fd, buf, blen, offset) != (ssize_t)blen) {
			error = 1;
			break;
		}
		len -= blen;
		offset += blen;
	}

	free(buf);
	return (error);
}

static blksize_t
fd_get_blksize(void)
{
	struct statfs statfsbuf;

	if (statfs(".", &statfsbuf) == -1)
		return (-1);
	return statfsbuf.f_iosize;
}

static int
check_content_dealloc(int fd, off_t hole_start, off_t hole_len, off_t file_sz)
{
	int error;
	size_t blen;
	off_t offset, resid;
	struct stat statbuf;
	char *buf, *sblk;
	blksize_t blocksize;

	blocksize = fd_get_blksize();
	if (blocksize == -1)
		return (1);
	error = 0;
	buf = malloc(blocksize * 2);
	if (buf == NULL)
		return (1);
	sblk = buf + blocksize;

	memset(sblk, 0, blocksize);

	if ((uint64_t)hole_start + hole_len > (uint64_t)file_sz)
		hole_len = file_sz - hole_start;

	/*
	 * Check hole is zeroed.
	 */
	offset = hole_start;
	resid = hole_len;
	while (resid > 0) {
		blen = resid < (off_t)blocksize ? resid : blocksize;
		if (pread(fd, buf, blen, offset) != (ssize_t)blen) {
			error = 1;
			break;
		}
		if (memcmp(buf, sblk, blen) != 0) {
			error = 1;
			break;
		}
		resid -= blen;
		offset += blen;
	}

	memset(sblk, byte_to_fill, blocksize);

	/*
	 * Check file region before hole is zeroed.
	 */
	offset = 0;
	resid = hole_start;
	while (resid > 0) {
		blen = resid < (off_t)blocksize ? resid : blocksize;
		if (pread(fd, buf, blen, offset) != (ssize_t)blen) {
			error = 1;
			break;
		}
		if (memcmp(buf, sblk, blen) != 0) {
			error = 1;
			break;
		}
		resid -= blen;
		offset += blen;
	}

	/*
	 * Check file region after hole is zeroed.
	 */
	offset = hole_start + hole_len;
	resid = file_sz - offset;
	while (resid > 0) {
		blen = resid < (off_t)blocksize ? resid : blocksize;
		if (pread(fd, buf, blen, offset) != (ssize_t)blen) {
			error = 1;
			break;
		}
		if (memcmp(buf, sblk, blen) != 0) {
			error = 1;
			break;
		}
		resid -= blen;
		offset += blen;
	}

	/*
	 * Check file size matches with expected file size.
	 */
	if (fstat(fd, &statbuf) == -1)
		error = -1;
	if (statbuf.st_size != file_sz)
		error = -1;

	free(buf);
	return (error);
}

/*
 * Check aligned deallocation
 */
ATF_TC_WITHOUT_HEAD(aligned_dealloc);
ATF_TC_BODY(aligned_dealloc, tc)
{
	struct spacectl_range range;
	off_t offset, length;
	blksize_t blocksize;
	int fd;

	ATF_REQUIRE((blocksize = fd_get_blksize()) != -1);
	range.r_offset = offset = blocksize;
	range.r_len = length = (file_max_blocks - 1) * blocksize -
	    range.r_offset;

	ATF_REQUIRE((fd = open("sys_fspacectl_testfile",
			 O_CREAT | O_RDWR | O_TRUNC, 0600)) != -1);
	ATF_REQUIRE(fill(fd, 0, file_max_blocks * blocksize) == 0);
	ATF_CHECK(fspacectl(fd, SPACECTL_DEALLOC, &range, 0, &range) == 0);
	ATF_CHECK(check_content_dealloc(fd, offset, length,
		      file_max_blocks * blocksize) == 0);
	ATF_REQUIRE(close(fd) == 0);
}

/*
 * Check unaligned deallocation
 */
ATF_TC_WITHOUT_HEAD(unaligned_dealloc);
ATF_TC_BODY(unaligned_dealloc, tc)
{
	struct spacectl_range range;
	off_t offset, length;
	blksize_t blocksize;
	int fd;

	ATF_REQUIRE((blocksize = fd_get_blksize()) != -1);
	range.r_offset = offset = blocksize / 2;
	range.r_len = length = (file_max_blocks - 1) * blocksize +
	    blocksize / 2 - offset;

	ATF_REQUIRE((fd = open("sys_fspacectl_testfile",
			 O_CREAT | O_RDWR | O_TRUNC, 0600)) != -1);
	ATF_REQUIRE(fill(fd, 0, file_max_blocks * blocksize) == 0);
	ATF_CHECK(fspacectl(fd, SPACECTL_DEALLOC, &range, 0, &range) == 0);
	ATF_CHECK(check_content_dealloc(fd, offset, length,
		      file_max_blocks * blocksize) == 0);
	ATF_REQUIRE(close(fd) == 0);
}

/*
 * Check aligned deallocation from certain offset to OFF_MAX
 */
ATF_TC_WITHOUT_HEAD(aligned_dealloc_offmax);
ATF_TC_BODY(aligned_dealloc_offmax, tc)
{
	struct spacectl_range range;
	off_t offset, length;
	blksize_t blocksize;
	int fd;

	ATF_REQUIRE((blocksize = fd_get_blksize()) != -1);
	range.r_offset = offset = blocksize;
	range.r_len = length = OFF_MAX - offset;

	ATF_REQUIRE((fd = open("sys_fspacectl_testfile",
			 O_CREAT | O_RDWR | O_TRUNC, 0600)) != -1);
	ATF_REQUIRE(fill(fd, 0, file_max_blocks * blocksize) == 0);
	ATF_CHECK(fspacectl(fd, SPACECTL_DEALLOC, &range, 0, &range) == 0);
	ATF_CHECK(check_content_dealloc(fd, offset, length,
		      file_max_blocks * blocksize) == 0);
	ATF_REQUIRE(close(fd) == 0);
}

/*
 * Check unaligned deallocation from certain offset to OFF_MAX
 */
ATF_TC_WITHOUT_HEAD(unaligned_dealloc_offmax);
ATF_TC_BODY(unaligned_dealloc_offmax, tc)
{
	struct spacectl_range range;
	off_t offset, length;
	blksize_t blocksize;
	int fd;

	ATF_REQUIRE((blocksize = fd_get_blksize()) != -1);
	range.r_offset = offset = blocksize / 2;
	range.r_len = length = OFF_MAX - offset;

	ATF_REQUIRE((fd = open("sys_fspacectl_testfile",
			 O_CREAT | O_RDWR | O_TRUNC, 0600)) != -1);
	ATF_REQUIRE(fill(fd, 0, file_max_blocks * blocksize) == 0);
	ATF_CHECK(fspacectl(fd, SPACECTL_DEALLOC, &range, 0, &range) == 0);
	ATF_CHECK(check_content_dealloc(fd, offset, length,
		      file_max_blocks * blocksize) == 0);
	ATF_REQUIRE(close(fd) == 0);
}

/*
 * Check aligned deallocation around EOF
 */
ATF_TC_WITHOUT_HEAD(aligned_dealloc_eof);
ATF_TC_BODY(aligned_dealloc_eof, tc)
{
	struct spacectl_range range;
	off_t offset, length;
	blksize_t blocksize;
	int fd;

	ATF_REQUIRE((blocksize = fd_get_blksize()) != -1);
	range.r_offset = offset = blocksize;
	range.r_len = length = (file_max_blocks + 1) * blocksize -
	    range.r_offset;

	ATF_REQUIRE((fd = open("sys_fspacectl_testfile",
			 O_CREAT | O_RDWR | O_TRUNC, 0600)) != -1);
	ATF_REQUIRE(fill(fd, 0, file_max_blocks * blocksize) == 0);
	ATF_CHECK(fspacectl(fd, SPACECTL_DEALLOC, &range, 0, &range) == 0);
	ATF_CHECK(check_content_dealloc(fd, offset, length,
		      file_max_blocks * blocksize) == 0);
	ATF_REQUIRE(close(fd) == 0);
}

/*
 * Check unaligned deallocation around EOF
 */
ATF_TC_WITHOUT_HEAD(unaligned_dealloc_eof);
ATF_TC_BODY(unaligned_dealloc_eof, tc)
{
	struct spacectl_range range;
	off_t offset, length;
	blksize_t blocksize;
	int fd;

	ATF_REQUIRE((blocksize = fd_get_blksize()) != -1);
	range.r_offset = offset = blocksize / 2;
	range.r_len = length = file_max_blocks * blocksize + blocksize / 2 -
	    range.r_offset;

	ATF_REQUIRE((fd = open("sys_fspacectl_testfile",
			 O_CREAT | O_RDWR | O_TRUNC, 0600)) != -1);
	ATF_REQUIRE(fill(fd, 0, file_max_blocks * blocksize) == 0);
	ATF_CHECK(fspacectl(fd, SPACECTL_DEALLOC, &range, 0, &range) == 0);
	ATF_CHECK(check_content_dealloc(fd, offset, length,
		      file_max_blocks * blocksize) == 0);
	ATF_REQUIRE(close(fd) == 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, aligned_dealloc);
	ATF_TP_ADD_TC(tp, unaligned_dealloc);
	ATF_TP_ADD_TC(tp, aligned_dealloc_eof);
	ATF_TP_ADD_TC(tp, unaligned_dealloc_eof);
	ATF_TP_ADD_TC(tp, aligned_dealloc_offmax);
	ATF_TP_ADD_TC(tp, unaligned_dealloc_offmax);

	return atf_no_error();
}
