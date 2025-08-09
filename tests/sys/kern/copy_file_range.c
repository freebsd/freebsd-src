/*
 * Copyright (c) 2025 Mark Johnston <markj@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/mman.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <atf-c.h>
#include <sha256.h>

/*
 * Create a file with random data and size between 1B and 32MB.  Return a file
 * descriptor for the file.
 */
static int
genfile(void)
{
	char buf[256], file[NAME_MAX];
	size_t sz;
	int fd;

	sz = (random() % (32 * 1024 * 1024ul)) + 1;

	snprintf(file, sizeof(file), "testfile.XXXXXX");
	fd = mkstemp(file);
	ATF_REQUIRE(fd != -1);

	while (sz > 0) {
		ssize_t n;
		int error;

		error = getentropy(buf, sizeof(buf));
		ATF_REQUIRE(error == 0);
		n = write(fd, buf, sizeof(buf) < sz ? sizeof(buf) : sz);
		ATF_REQUIRE(n > 0);

		sz -= n;
	}

	ATF_REQUIRE(lseek(fd, 0, SEEK_SET) == 0);
	return (fd);
}

/*
 * Return true if the file data in the two file descriptors is the same,
 * false otherwise.
 */
static bool
cmpfile(int fd1, int fd2)
{
	struct stat st1, st2;
	void *addr1, *addr2;
	size_t sz;
	int res;

	ATF_REQUIRE(fstat(fd1, &st1) == 0);
	ATF_REQUIRE(fstat(fd2, &st2) == 0);
	if (st1.st_size != st2.st_size)
		return (false);

	sz = st1.st_size;
	addr1 = mmap(NULL, sz, PROT_READ, MAP_PRIVATE, fd1, 0);
	ATF_REQUIRE(addr1 != MAP_FAILED);
	addr2 = mmap(NULL, sz, PROT_READ, MAP_PRIVATE, fd2, 0);
	ATF_REQUIRE(addr2 != MAP_FAILED);

	res = memcmp(addr1, addr2, sz);

	ATF_REQUIRE(munmap(addr1, sz) == 0);
	ATF_REQUIRE(munmap(addr2, sz) == 0);

	return (res == 0);
}

/*
 * Exercise a few error paths in the copy_file_range() syscall.
 */
ATF_TC_WITHOUT_HEAD(copy_file_range_invalid);
ATF_TC_BODY(copy_file_range_invalid, tc)
{
	off_t off1, off2;
	int fd1, fd2;

	fd1 = genfile();
	fd2 = genfile();

	/* Can't copy a file to itself without explicit offsets. */
	ATF_REQUIRE_ERRNO(EINVAL,
	    copy_file_range(fd1, NULL, fd1, NULL, SSIZE_MAX, 0) == -1);

	/* When copying a file to itself, ranges cannot overlap. */
	off1 = off2 = 0;
	ATF_REQUIRE_ERRNO(EINVAL,
	    copy_file_range(fd1, &off1, fd1, &off2, 1, 0) == -1);

	/* Negative offsets are not allowed. */
	off1 = -1;
	off2 = 0;
	ATF_REQUIRE_ERRNO(EINVAL,
	    copy_file_range(fd1, &off1, fd2, &off2, 42, 0) == -1);
	ATF_REQUIRE_ERRNO(EINVAL,
	    copy_file_range(fd2, &off2, fd1, &off1, 42, 0) == -1);
}

/*
 * Make sure that copy_file_range() updates the file offsets passed to it.
 */
ATF_TC_WITHOUT_HEAD(copy_file_range_offset);
ATF_TC_BODY(copy_file_range_offset, tc)
{
	struct stat sb;
	off_t off1, off2;
	ssize_t n;
	int fd1, fd2;

	off1 = off2 = 0;

	fd1 = genfile();
	fd2 = open("copy", O_RDWR | O_CREAT, 0644);
	ATF_REQUIRE(fd2 != -1);

	ATF_REQUIRE(fstat(fd1, &sb) == 0);

	ATF_REQUIRE(lseek(fd1, 0, SEEK_CUR) == 0);
	ATF_REQUIRE(lseek(fd2, 0, SEEK_CUR) == 0);

	do {
		off_t ooff1, ooff2;

		ooff1 = off1;
		ooff2 = off2;
		n = copy_file_range(fd1, &off1, fd2, &off2, sb.st_size, 0);
		ATF_REQUIRE(n >= 0);
		ATF_REQUIRE_EQ(off1, ooff1 + n);
		ATF_REQUIRE_EQ(off2, ooff2 + n);
	} while (n != 0);

	/* Offsets should have been adjusted by copy_file_range(). */
	ATF_REQUIRE_EQ(off1, sb.st_size);
	ATF_REQUIRE_EQ(off2, sb.st_size);
	/* Seek offsets should have been left alone. */
	ATF_REQUIRE(lseek(fd1, 0, SEEK_CUR) == 0);
	ATF_REQUIRE(lseek(fd2, 0, SEEK_CUR) == 0);
	/* Make sure the file contents are the same. */
	ATF_REQUIRE_MSG(cmpfile(fd1, fd2), "file contents differ");

	ATF_REQUIRE(close(fd1) == 0);
	ATF_REQUIRE(close(fd2) == 0);
}

/*
 * Make sure that copying to a larger file doesn't cause it to be truncated.
 */
ATF_TC_WITHOUT_HEAD(copy_file_range_truncate);
ATF_TC_BODY(copy_file_range_truncate, tc)
{
	struct stat sb, sb1, sb2;
	char digest1[65], digest2[65];
	off_t off;
	ssize_t n;
	int fd1, fd2;

	fd1 = genfile();
	fd2 = genfile();

	ATF_REQUIRE(fstat(fd1, &sb1) == 0);
	ATF_REQUIRE(fstat(fd2, &sb2) == 0);

	/* fd1 refers to the smaller file. */
	if (sb1.st_size > sb2.st_size) {
		int tmp;

		tmp = fd1;
		fd1 = fd2;
		fd2 = tmp;
		ATF_REQUIRE(fstat(fd1, &sb1) == 0);
		ATF_REQUIRE(fstat(fd2, &sb2) == 0);
	}

	/*
	 * Compute a hash of the bytes in the larger file which lie beyond the
	 * length of the smaller file.
	 */
	SHA256_FdChunk(fd2, digest1, sb1.st_size, sb2.st_size - sb1.st_size);
	ATF_REQUIRE(lseek(fd2, 0, SEEK_SET) == 0);

	do {
		n = copy_file_range(fd1, NULL, fd2, NULL, SSIZE_MAX, 0);
		ATF_REQUIRE(n >= 0);
	} while (n != 0);

	/* Validate file offsets after the copy. */
	off = lseek(fd1, 0, SEEK_CUR);
	ATF_REQUIRE(off == sb1.st_size);
	off = lseek(fd2, 0, SEEK_CUR);
	ATF_REQUIRE(off == sb1.st_size);

	/* The larger file's size should remain the same. */
	ATF_REQUIRE(fstat(fd2, &sb) == 0);
	ATF_REQUIRE(sb.st_size == sb2.st_size);

	/* The bytes beyond the end of the copy should be unchanged. */
	SHA256_FdChunk(fd2, digest2, sb1.st_size, sb2.st_size - sb1.st_size);
	ATF_REQUIRE_MSG(strcmp(digest1, digest2) == 0,
	    "trailing file contents differ after copy_file_range()");

	/*
	 * Verify that the copy actually replicated bytes from the smaller file.
	 */
	ATF_REQUIRE(ftruncate(fd2, sb1.st_size) == 0);
	ATF_REQUIRE(cmpfile(fd1, fd2));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, copy_file_range_invalid);
	ATF_TP_ADD_TC(tp, copy_file_range_offset);
	ATF_TP_ADD_TC(tp, copy_file_range_truncate);

	return (atf_no_error());
}
