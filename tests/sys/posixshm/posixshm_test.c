/*-
 * Copyright (c) 2006 Robert N. M. Watson
 * All rights reserved.
 *
 * Copyright (c) 2021 The FreeBSD Foundation
 *
 * Portions of this software were developed by Ka Ho Ng
 * under sponsorship from the FreeBSD Foundation.
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
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

#define	TEST_PATH_LEN	256
static char test_path[TEST_PATH_LEN];
static char test_path2[TEST_PATH_LEN];
static unsigned int test_path_idx = 0;

static void
gen_a_test_path(char *path)
{
	snprintf(path, TEST_PATH_LEN, "/%s/tmp.XXXXXX%d",
	    getenv("TMPDIR") == NULL ? "/tmp" : getenv("TMPDIR"),
	    test_path_idx);

	test_path_idx++;

	ATF_REQUIRE_MSG(mkstemp(path) != -1,
	    "mkstemp failed; errno=%d", errno);
	ATF_REQUIRE_MSG(unlink(path) == 0,
	    "unlink failed; errno=%d", errno);
}

static void
gen_test_path(void)
{
	gen_a_test_path(test_path);
}

static void
gen_test_path2(void)
{
	gen_a_test_path(test_path2);
}

/*
 * Attempt a shm_open() that should fail with an expected error of 'error'.
 */
static void
shm_open_should_fail(const char *path, int flags, mode_t mode, int error)
{
	int fd;

	fd = shm_open(path, flags, mode);
	ATF_CHECK_MSG(fd == -1, "shm_open didn't fail");
	ATF_CHECK_MSG(error == errno,
	    "shm_open didn't fail with expected errno; errno=%d; expected "
	    "errno=%d", errno, error);
}

/*
 * Attempt a shm_unlink() that should fail with an expected error of 'error'.
 */
static void
shm_unlink_should_fail(const char *path, int error)
{

	ATF_CHECK_MSG(shm_unlink(path) == -1, "shm_unlink didn't fail");
	ATF_CHECK_MSG(error == errno,
	    "shm_unlink didn't fail with expected errno; errno=%d; expected "
	    "errno=%d", errno, error);
}

/*
 * Open the test object and write a value to the first byte.  Returns valid fd
 * on success and -1 on failure.
 */
static int
scribble_object(const char *path, char value)
{
	char *page;
	int fd, pagesize;

	ATF_REQUIRE(0 < (pagesize = getpagesize()));

	fd = shm_open(path, O_CREAT|O_EXCL|O_RDWR, 0777);
	if (fd < 0 && errno == EEXIST) {
		if (shm_unlink(test_path) < 0)
			atf_tc_fail("shm_unlink");
		fd = shm_open(test_path, O_CREAT | O_EXCL | O_RDWR, 0777);
	}
	if (fd < 0)
		atf_tc_fail("shm_open failed; errno=%d", errno);
	if (ftruncate(fd, pagesize) < 0)
		atf_tc_fail("ftruncate failed; errno=%d", errno);

	page = mmap(0, pagesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (page == MAP_FAILED)
		atf_tc_fail("mmap failed; errno=%d", errno);

	page[0] = value;
	ATF_REQUIRE_MSG(munmap(page, pagesize) == 0, "munmap failed; errno=%d",
	    errno);

	return (fd);
}

/*
 * Fail the test case if the 'path' does not refer to an shm whose first byte
 * is equal to expected_value
 */
static void
verify_object(const char *path, char expected_value)
{
	int fd;
	int pagesize;
	char *page;

	ATF_REQUIRE(0 < (pagesize = getpagesize()));

	fd = shm_open(path, O_RDONLY, 0777);
	if (fd < 0)
		atf_tc_fail("shm_open failed in verify_object; errno=%d, path=%s",
		    errno, path);

	page = mmap(0, pagesize, PROT_READ, MAP_SHARED, fd, 0);
	if (page == MAP_FAILED)
		atf_tc_fail("mmap(1)");
	if (page[0] != expected_value)
		atf_tc_fail("Renamed object has incorrect value; has"
		    "%d (0x%x, '%c'), expected %d (0x%x, '%c')\n",
		    page[0], page[0], isprint(page[0]) ? page[0] : ' ',
		    expected_value, expected_value,
		    isprint(expected_value) ? expected_value : ' ');
	ATF_REQUIRE_MSG(munmap(page, pagesize) == 0, "munmap failed; errno=%d",
	    errno);
	close(fd);
}

static off_t shm_max_pages = 32;
static const char byte_to_fill = 0x5f;

static int
shm_fill(int fd, off_t offset, off_t len)
{
	int error;
	size_t blen, page_size;
	char *buf;

	error = 0;
	page_size = getpagesize();
	buf = malloc(page_size);
	if (buf == NULL)
		return (1);

	while (len > 0) {
		blen = len < (off_t)page_size ? (size_t)len : page_size;
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

static int
check_content_dealloc(int fd, off_t hole_start, off_t hole_len, off_t shm_sz)
{
	int error;
	size_t blen, page_size;
	off_t offset, resid;
	struct stat statbuf;
	char *buf, *sblk;

	error = 0;
	page_size = getpagesize();
	buf = malloc(page_size * 2);
	if (buf == NULL)
		return (1);
	sblk = buf + page_size;

	memset(sblk, 0, page_size);

	if ((uint64_t)hole_start + hole_len > (uint64_t)shm_sz)
		hole_len = shm_sz - hole_start;

	/*
	 * Check hole is zeroed.
	 */
	offset = hole_start;
	resid = hole_len;
	while (resid > 0) {
		blen = resid < (off_t)page_size ? (size_t)resid : page_size;
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

	memset(sblk, byte_to_fill, page_size);

	/*
	 * Check file region before hole is zeroed.
	 */
	offset = 0;
	resid = hole_start;
	while (resid > 0) {
		blen = resid < (off_t)page_size ? (size_t)resid : page_size;
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
	resid = shm_sz - offset;
	while (resid > 0) {
		blen = resid < (off_t)page_size ? (size_t)resid : page_size;
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
	if (statbuf.st_size != shm_sz)
		error = -1;

	free(buf);
	return (error);
}

ATF_TC_WITHOUT_HEAD(remap_object);
ATF_TC_BODY(remap_object, tc)
{
	char *page;
	int fd, pagesize;

	ATF_REQUIRE(0 < (pagesize = getpagesize()));

	gen_test_path();
	fd = scribble_object(test_path, '1');

	page = mmap(0, pagesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (page == MAP_FAILED)
		atf_tc_fail("mmap(2) failed; errno=%d", errno);

	if (page[0] != '1')
		atf_tc_fail("missing data ('%c' != '1')", page[0]);

	close(fd);
	ATF_REQUIRE_MSG(munmap(page, pagesize) == 0, "munmap failed; errno=%d",
	    errno);

	ATF_REQUIRE_MSG(shm_unlink(test_path) != -1,
	    "shm_unlink failed; errno=%d", errno);
}

ATF_TC_WITHOUT_HEAD(rename_from_anon);
ATF_TC_BODY(rename_from_anon, tc)
{
	int rc;

	gen_test_path();
	rc = shm_rename(SHM_ANON, test_path, 0);
	if (rc != -1)
		atf_tc_fail("shm_rename from SHM_ANON succeeded unexpectedly");
}

ATF_TC_WITHOUT_HEAD(rename_bad_path_pointer);
ATF_TC_BODY(rename_bad_path_pointer, tc)
{
	const char *bad_path;
	int rc;

	bad_path = (const char *)0x1;

	gen_test_path();
	rc = shm_rename(test_path, bad_path, 0);
	if (rc != -1)
		atf_tc_fail("shm_rename of nonexisting shm succeeded unexpectedly");

	rc = shm_rename(bad_path, test_path, 0);
	if (rc != -1)
		atf_tc_fail("shm_rename of nonexisting shm succeeded unexpectedly");
}

ATF_TC_WITHOUT_HEAD(rename_from_nonexisting);
ATF_TC_BODY(rename_from_nonexisting, tc)
{
	int rc;

	gen_test_path();
	gen_test_path2();
	rc = shm_rename(test_path, test_path2, 0);
	if (rc != -1)
		atf_tc_fail("shm_rename of nonexisting shm succeeded unexpectedly");

	if (errno != ENOENT)
		atf_tc_fail("Expected ENOENT to rename of nonexistent shm; got %d",
		    errno);
}

ATF_TC_WITHOUT_HEAD(rename_to_anon);
ATF_TC_BODY(rename_to_anon, tc)
{
	int rc;

	gen_test_path();
	rc = shm_rename(test_path, SHM_ANON, 0);
	if (rc != -1)
		atf_tc_fail("shm_rename to SHM_ANON succeeded unexpectedly");
}

ATF_TC_WITHOUT_HEAD(rename_to_replace);
ATF_TC_BODY(rename_to_replace, tc)
{
	char expected_value;
	int fd;
	int fd2;

	// Some contents we can verify later
	expected_value = 'g';

	gen_test_path();
	fd = scribble_object(test_path, expected_value);
	close(fd);

	// Give the other some different value so we can detect success
	gen_test_path2();
	fd2 = scribble_object(test_path2, 'h');
	close(fd2);

	ATF_REQUIRE_MSG(shm_rename(test_path, test_path2, 0) == 0,
	    "shm_rename failed; errno=%d", errno);

	// Read back renamed; verify contents
	verify_object(test_path2, expected_value);
}

ATF_TC_WITHOUT_HEAD(rename_to_noreplace);
ATF_TC_BODY(rename_to_noreplace, tc)
{
	char expected_value_from;
	char expected_value_to;
	int fd_from;
	int fd_to;
	int rc;

	// Some contents we can verify later
	expected_value_from = 'g';
	gen_test_path();
	fd_from = scribble_object(test_path, expected_value_from);
	close(fd_from);

	// Give the other some different value so we can detect success
	expected_value_to = 'h';
	gen_test_path2();
	fd_to = scribble_object(test_path2, expected_value_to);
	close(fd_to);

	rc = shm_rename(test_path, test_path2, SHM_RENAME_NOREPLACE);
	ATF_REQUIRE_MSG((rc == -1) && (errno == EEXIST),
	    "shm_rename didn't fail as expected; errno: %d; return: %d", errno,
	    rc);

	// Read back renamed; verify contents
	verify_object(test_path2, expected_value_to);
}

ATF_TC_WITHOUT_HEAD(rename_to_exchange);
ATF_TC_BODY(rename_to_exchange, tc)
{
	char expected_value_from;
	char expected_value_to;
	int fd_from;
	int fd_to;

	// Some contents we can verify later
	expected_value_from = 'g';
	gen_test_path();
	fd_from = scribble_object(test_path, expected_value_from);
	close(fd_from);

	// Give the other some different value so we can detect success
	expected_value_to = 'h';
	gen_test_path2();
	fd_to = scribble_object(test_path2, expected_value_to);
	close(fd_to);

	ATF_REQUIRE_MSG(shm_rename(test_path, test_path2,
	    SHM_RENAME_EXCHANGE) == 0,
	    "shm_rename failed; errno=%d", errno);

	// Read back renamed; verify contents
	verify_object(test_path, expected_value_to);
	verify_object(test_path2, expected_value_from);
}

ATF_TC_WITHOUT_HEAD(rename_to_exchange_nonexisting);
ATF_TC_BODY(rename_to_exchange_nonexisting, tc)
{
	char expected_value_from;
	int fd_from;

	// Some contents we can verify later
	expected_value_from = 'g';
	gen_test_path();
	fd_from = scribble_object(test_path, expected_value_from);
	close(fd_from);

	gen_test_path2();

	ATF_REQUIRE_MSG(shm_rename(test_path, test_path2,
	    SHM_RENAME_EXCHANGE) == 0,
	    "shm_rename failed; errno=%d", errno);

	// Read back renamed; verify contents
	verify_object(test_path2, expected_value_from);
}

ATF_TC_WITHOUT_HEAD(rename_to_self);
ATF_TC_BODY(rename_to_self, tc)
{
	int fd;
	char expected_value;

	expected_value = 't';

	gen_test_path();
	fd = scribble_object(test_path, expected_value);
	close(fd);

	ATF_REQUIRE_MSG(shm_rename(test_path, test_path, 0) == 0,
	    "shm_rename failed; errno=%d", errno);

	verify_object(test_path, expected_value);
}
	
ATF_TC_WITHOUT_HEAD(rename_bad_flag);
ATF_TC_BODY(rename_bad_flag, tc)
{
	int fd;
	int rc;

	/* Make sure we don't fail out due to ENOENT */
	gen_test_path();
	gen_test_path2();
	fd = scribble_object(test_path, 'd');
	close(fd);
	fd = scribble_object(test_path2, 'd');
	close(fd);

	/*
	 * Note: if we end up with enough flags that we use all the bits,
	 * then remove this test completely.
	 */
	rc = shm_rename(test_path, test_path2, INT_MIN);
	ATF_REQUIRE_MSG((rc == -1) && (errno == EINVAL),
	    "shm_rename should have failed with EINVAL; got: return=%d, "
	    "errno=%d", rc, errno);
}

ATF_TC_WITHOUT_HEAD(reopen_object);
ATF_TC_BODY(reopen_object, tc)
{
	char *page;
	int fd, pagesize;

	ATF_REQUIRE(0 < (pagesize = getpagesize()));

	gen_test_path();
	fd = scribble_object(test_path, '1');
	close(fd);

	fd = shm_open(test_path, O_RDONLY, 0777);
	if (fd < 0)
		atf_tc_fail("shm_open(2) failed; errno=%d", errno);

	page = mmap(0, pagesize, PROT_READ, MAP_SHARED, fd, 0);
	if (page == MAP_FAILED)
		atf_tc_fail("mmap(2) failed; errno=%d", errno);

	if (page[0] != '1')
		atf_tc_fail("missing data ('%c' != '1')", page[0]);

	ATF_REQUIRE_MSG(munmap(page, pagesize) == 0, "munmap failed; errno=%d",
	    errno);
	close(fd);
	ATF_REQUIRE_MSG(shm_unlink(test_path) != -1,
	    "shm_unlink failed; errno=%d", errno);
}

ATF_TC_WITHOUT_HEAD(readonly_mmap_write);
ATF_TC_BODY(readonly_mmap_write, tc)
{
	char *page;
	int fd, pagesize;

	ATF_REQUIRE(0 < (pagesize = getpagesize()));

	gen_test_path();

	fd = shm_open(test_path, O_RDONLY | O_CREAT, 0777);
	ATF_REQUIRE_MSG(fd >= 0, "shm_open failed; errno=%d", errno);

	/* PROT_WRITE should fail with EACCES. */
	page = mmap(0, pagesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (page != MAP_FAILED)
		atf_tc_fail("mmap(PROT_WRITE) succeeded unexpectedly");

	if (errno != EACCES)
		atf_tc_fail("mmap(PROT_WRITE) didn't fail with EACCES; "
		    "errno=%d", errno);

	close(fd);
	ATF_REQUIRE_MSG(shm_unlink(test_path) != -1,
	    "shm_unlink failed; errno=%d", errno);
}

ATF_TC_WITHOUT_HEAD(open_after_link);
ATF_TC_BODY(open_after_link, tc)
{
	int fd;

	gen_test_path();

	fd = shm_open(test_path, O_RDONLY | O_CREAT, 0777);
	ATF_REQUIRE_MSG(fd >= 0, "shm_open(1) failed; errno=%d", errno);
	close(fd);

	ATF_REQUIRE_MSG(shm_unlink(test_path) != -1, "shm_unlink failed: %d",
	    errno);

	shm_open_should_fail(test_path, O_RDONLY, 0777, ENOENT);
}

ATF_TC_WITHOUT_HEAD(open_invalid_path);
ATF_TC_BODY(open_invalid_path, tc)
{

	shm_open_should_fail("blah", O_RDONLY, 0777, EINVAL);
}

ATF_TC_WITHOUT_HEAD(open_write_only);
ATF_TC_BODY(open_write_only, tc)
{

	gen_test_path();

	shm_open_should_fail(test_path, O_WRONLY, 0777, EINVAL);
}

ATF_TC_WITHOUT_HEAD(open_extra_flags);
ATF_TC_BODY(open_extra_flags, tc)
{

	gen_test_path();

	shm_open_should_fail(test_path, O_RDONLY | O_DIRECT, 0777, EINVAL);
}

ATF_TC_WITHOUT_HEAD(open_anon);
ATF_TC_BODY(open_anon, tc)
{
	int fd;

	fd = shm_open(SHM_ANON, O_RDWR, 0777);
	ATF_REQUIRE_MSG(fd >= 0, "shm_open failed; errno=%d", errno);
	close(fd);
}

ATF_TC_WITHOUT_HEAD(open_anon_readonly);
ATF_TC_BODY(open_anon_readonly, tc)
{

	shm_open_should_fail(SHM_ANON, O_RDONLY, 0777, EINVAL);
}

ATF_TC_WITHOUT_HEAD(open_bad_path_pointer);
ATF_TC_BODY(open_bad_path_pointer, tc)
{

	shm_open_should_fail((char *)1024, O_RDONLY, 0777, EFAULT);
}

ATF_TC_WITHOUT_HEAD(open_path_too_long);
ATF_TC_BODY(open_path_too_long, tc)
{
	char *page;

	page = malloc(MAXPATHLEN + 1);
	memset(page, 'a', MAXPATHLEN);
	page[MAXPATHLEN] = '\0';
	shm_open_should_fail(page, O_RDONLY, 0777, ENAMETOOLONG);
	free(page);
}

ATF_TC_WITHOUT_HEAD(open_nonexisting_object);
ATF_TC_BODY(open_nonexisting_object, tc)
{

	shm_open_should_fail("/notreallythere", O_RDONLY, 0777, ENOENT);
}

ATF_TC_WITHOUT_HEAD(open_create_existing_object);
ATF_TC_BODY(open_create_existing_object, tc)
{
	int fd;

	gen_test_path();

	fd = shm_open(test_path, O_RDONLY|O_CREAT, 0777);
	ATF_REQUIRE_MSG(fd >= 0, "shm_open failed; errno=%d", errno);
	close(fd);

	shm_open_should_fail(test_path, O_RDONLY|O_CREAT|O_EXCL,
	    0777, EEXIST);

	ATF_REQUIRE_MSG(shm_unlink(test_path) != -1,
	    "shm_unlink failed; errno=%d", errno);
}

ATF_TC_WITHOUT_HEAD(trunc_resets_object);
ATF_TC_BODY(trunc_resets_object, tc)
{
	struct stat sb;
	int fd;

	gen_test_path();

	/* Create object and set size to 1024. */
	fd = shm_open(test_path, O_RDWR | O_CREAT, 0777);
	ATF_REQUIRE_MSG(fd >= 0, "shm_open(1) failed; errno=%d", errno);
	ATF_REQUIRE_MSG(ftruncate(fd, 1024) != -1,
	    "ftruncate failed; errno=%d", errno);
	ATF_REQUIRE_MSG(fstat(fd, &sb) != -1,
	    "fstat(1) failed; errno=%d", errno);
	ATF_REQUIRE_MSG(sb.st_size == 1024, "size %d != 1024", (int)sb.st_size);
	close(fd);

	/* Open with O_TRUNC which should reset size to 0. */
	fd = shm_open(test_path, O_RDWR | O_TRUNC, 0777);
	ATF_REQUIRE_MSG(fd >= 0, "shm_open(2) failed; errno=%d", errno);
	ATF_REQUIRE_MSG(fstat(fd, &sb) != -1,
	    "fstat(2) failed; errno=%d", errno);
	ATF_REQUIRE_MSG(sb.st_size == 0,
	    "size was not 0 after truncation: %d", (int)sb.st_size);
	close(fd);
	ATF_REQUIRE_MSG(shm_unlink(test_path) != -1,
	    "shm_unlink failed; errno=%d", errno);
}

ATF_TC_WITHOUT_HEAD(unlink_bad_path_pointer);
ATF_TC_BODY(unlink_bad_path_pointer, tc)
{

	shm_unlink_should_fail((char *)1024, EFAULT);
}

ATF_TC_WITHOUT_HEAD(unlink_path_too_long);
ATF_TC_BODY(unlink_path_too_long, tc)
{
	char *page;

	page = malloc(MAXPATHLEN + 1);
	memset(page, 'a', MAXPATHLEN);
	page[MAXPATHLEN] = '\0';
	shm_unlink_should_fail(page, ENAMETOOLONG);
	free(page);
}

ATF_TC_WITHOUT_HEAD(object_resize);
ATF_TC_BODY(object_resize, tc)
{
	pid_t pid;
	struct stat sb;
	char *page;
	int fd, pagesize, status;

	ATF_REQUIRE(0 < (pagesize = getpagesize()));

	/* Start off with a size of a single page. */
	fd = shm_open(SHM_ANON, O_CREAT|O_RDWR, 0777);
	if (fd < 0)
		atf_tc_fail("shm_open failed; errno=%d", errno);

	if (ftruncate(fd, pagesize) < 0)
		atf_tc_fail("ftruncate(1) failed; errno=%d", errno);

	if (fstat(fd, &sb) < 0)
		atf_tc_fail("fstat(1) failed; errno=%d", errno);

	if (sb.st_size != pagesize)
		atf_tc_fail("first resize failed (%d != %d)",
		    (int)sb.st_size, pagesize);

	/* Write a '1' to the first byte. */
	page = mmap(0, pagesize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (page == MAP_FAILED)
		atf_tc_fail("mmap(1)");

	page[0] = '1';

	ATF_REQUIRE_MSG(munmap(page, pagesize) == 0, "munmap failed; errno=%d",
	    errno);

	/* Grow the object to 2 pages. */
	if (ftruncate(fd, pagesize * 2) < 0)
		atf_tc_fail("ftruncate(2) failed; errno=%d", errno);

	if (fstat(fd, &sb) < 0)
		atf_tc_fail("fstat(2) failed; errno=%d", errno);

	if (sb.st_size != pagesize * 2)
		atf_tc_fail("second resize failed (%d != %d)",
		    (int)sb.st_size, pagesize * 2);

	/* Check for '1' at the first byte. */
	page = mmap(0, pagesize * 2, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (page == MAP_FAILED)
		atf_tc_fail("mmap(2) failed; errno=%d", errno);

	if (page[0] != '1')
		atf_tc_fail("'%c' != '1'", page[0]);

	/* Write a '2' at the start of the second page. */
	page[pagesize] = '2';

	/* Shrink the object back to 1 page. */
	if (ftruncate(fd, pagesize) < 0)
		atf_tc_fail("ftruncate(3) failed; errno=%d", errno);

	if (fstat(fd, &sb) < 0)
		atf_tc_fail("fstat(3) failed; errno=%d", errno);

	if (sb.st_size != pagesize)
		atf_tc_fail("third resize failed (%d != %d)",
		    (int)sb.st_size, pagesize);

	/*
	 * Fork a child process to make sure the second page is no
	 * longer valid.
	 */
	pid = fork();
	if (pid == -1)
		atf_tc_fail("fork failed; errno=%d", errno);

	if (pid == 0) {
		struct rlimit lim;
		char c;

		/* Don't generate a core dump. */
		ATF_REQUIRE(getrlimit(RLIMIT_CORE, &lim) == 0);
		lim.rlim_cur = 0;
		ATF_REQUIRE(setrlimit(RLIMIT_CORE, &lim) == 0);

		/*
		 * The previous ftruncate(2) shrunk the backing object
		 * so that this address is no longer valid, so reading
		 * from it should trigger a SIGBUS.
		 */
		c = page[pagesize];
		fprintf(stderr, "child: page 1: '%c'\n", c);
		exit(0);
	}

	if (wait(&status) < 0)
		atf_tc_fail("wait failed; errno=%d", errno);

	if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGBUS)
		atf_tc_fail("child terminated with status %x", status);

	/* Grow the object back to 2 pages. */
	if (ftruncate(fd, pagesize * 2) < 0)
		atf_tc_fail("ftruncate(2) failed; errno=%d", errno);

	if (fstat(fd, &sb) < 0)
		atf_tc_fail("fstat(2) failed; errno=%d", errno);

	if (sb.st_size != pagesize * 2)
		atf_tc_fail("fourth resize failed (%d != %d)",
		    (int)sb.st_size, pagesize);

	/*
	 * Note that the mapping at 'page' for the second page is
	 * still valid, and now that the shm object has been grown
	 * back up to 2 pages, there is now memory backing this page
	 * so the read will work.  However, the data should be zero
	 * rather than '2' as the old data was thrown away when the
	 * object was shrunk and the new pages when an object are
	 * grown are zero-filled.
	 */
	if (page[pagesize] != 0)
		atf_tc_fail("invalid data at %d: %x != 0",
		    pagesize, (int)page[pagesize]);

	close(fd);
}

/* Signal handler which does nothing. */
static void
ignoreit(int sig __unused)
{
	;
}

ATF_TC_WITHOUT_HEAD(shm_functionality_across_fork);
ATF_TC_BODY(shm_functionality_across_fork, tc)
{
	char *cp, c;
	int error, desc, rv;
	long scval;
	sigset_t ss;
	struct sigaction sa;
	void *region;
	size_t i, psize;

#ifndef _POSIX_SHARED_MEMORY_OBJECTS
	printf("_POSIX_SHARED_MEMORY_OBJECTS is undefined\n");
#else
	printf("_POSIX_SHARED_MEMORY_OBJECTS is defined as %ld\n", 
	       (long)_POSIX_SHARED_MEMORY_OBJECTS - 0);
	if (_POSIX_SHARED_MEMORY_OBJECTS - 0 == -1)
		printf("***Indicates this feature may be unsupported!\n");
#endif
	errno = 0;
	scval = sysconf(_SC_SHARED_MEMORY_OBJECTS);
	if (scval == -1 && errno != 0) {
		atf_tc_fail("sysconf(_SC_SHARED_MEMORY_OBJECTS) failed; "
		    "errno=%d", errno);
	} else {
		printf("sysconf(_SC_SHARED_MEMORY_OBJECTS) returns %ld\n",
		       scval);
		if (scval == -1)
			printf("***Indicates this feature is unsupported!\n");
	}

	errno = 0;
	scval = sysconf(_SC_PAGESIZE);
	if (scval == -1 && errno != 0) {
		atf_tc_fail("sysconf(_SC_PAGESIZE) failed; errno=%d", errno);
	} else if (scval <= 0) {
		fprintf(stderr, "bogus return from sysconf(_SC_PAGESIZE): %ld",
		    scval);
		psize = 4096;
	} else {
		printf("sysconf(_SC_PAGESIZE) returns %ld\n", scval);
		psize = scval;
	}

	gen_test_path();
	desc = shm_open(test_path, O_EXCL | O_CREAT | O_RDWR, 0600);

	ATF_REQUIRE_MSG(desc >= 0, "shm_open failed; errno=%d", errno);
	ATF_REQUIRE_MSG(shm_unlink(test_path) == 0,
	    "shm_unlink failed; errno=%d", errno);
	ATF_REQUIRE_MSG(ftruncate(desc, (off_t)psize) != -1,
	    "ftruncate failed; errno=%d", errno);

	region = mmap(NULL, psize, PROT_READ | PROT_WRITE, MAP_SHARED, desc, 0);
	ATF_REQUIRE_MSG(region != MAP_FAILED, "mmap failed; errno=%d", errno);
	memset(region, '\377', psize);

	sa.sa_flags = 0;
	sa.sa_handler = ignoreit;
	sigemptyset(&sa.sa_mask);
	ATF_REQUIRE_MSG(sigaction(SIGUSR1, &sa, (struct sigaction *)0) == 0,
	    "sigaction failed; errno=%d", errno);

	sigemptyset(&ss);
	sigaddset(&ss, SIGUSR1);
	ATF_REQUIRE_MSG(sigprocmask(SIG_BLOCK, &ss, (sigset_t *)0) == 0,
	    "sigprocmask failed; errno=%d", errno);

	rv = fork();
	ATF_REQUIRE_MSG(rv != -1, "fork failed; errno=%d", errno);
	if (rv == 0) {
		sigemptyset(&ss);
		sigsuspend(&ss);

		for (cp = region; cp < (char *)region + psize; cp++) {
			if (*cp != '\151')
				_exit(1);
		}
		if (lseek(desc, 0, SEEK_SET) == -1)
			_exit(1);
		for (i = 0; i < psize; i++) {
			error = read(desc, &c, 1);
			if (c != '\151')
				_exit(1);
		}
		_exit(0);
	} else {
		int status;

		memset(region, '\151', psize - 2);
		error = pwrite(desc, region, 2, psize - 2);
		if (error != 2) {
			if (error >= 0)
				atf_tc_fail("short write; %d bytes written",
				    error);
			else
				atf_tc_fail("shmfd write");
		}
		kill(rv, SIGUSR1);
		waitpid(rv, &status, 0);

		if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
			printf("Functionality test successful\n");
		} else if (WIFEXITED(status)) {
			atf_tc_fail("Child process exited with status %d",
			    WEXITSTATUS(status));
		} else {
			atf_tc_fail("Child process terminated with %s",
			    strsignal(WTERMSIG(status)));
		}
	}

	ATF_REQUIRE_MSG(munmap(region, psize) == 0, "munmap failed; errno=%d",
	    errno);
	shm_unlink(test_path);
}

ATF_TC_WITHOUT_HEAD(cloexec);
ATF_TC_BODY(cloexec, tc)
{
	int fd;

	gen_test_path();

	/* shm_open(2) is required to set FD_CLOEXEC */
	fd = shm_open(SHM_ANON, O_RDWR, 0777);
	ATF_REQUIRE_MSG(fd >= 0, "shm_open failed; errno=%d", errno);
	ATF_REQUIRE((fcntl(fd, F_GETFD) & FD_CLOEXEC) != 0);
	close(fd);

	/* Also make sure that named shm is correct */
	fd = shm_open(test_path, O_CREAT | O_RDWR, 0600);
	ATF_REQUIRE_MSG(fd >= 0, "shm_open failed; errno=%d", errno);
	ATF_REQUIRE((fcntl(fd, F_GETFD) & FD_CLOEXEC) != 0);
	close(fd);
}

ATF_TC_WITHOUT_HEAD(mode);
ATF_TC_BODY(mode, tc)
{
	struct stat st;
	int fd;
	mode_t restore_mask;

	gen_test_path();

	/* Remove inhibitions from umask */
	restore_mask = umask(0);
	fd = shm_open(test_path, O_CREAT | O_RDWR, 0600);
	ATF_REQUIRE_MSG(fd >= 0, "shm_open failed; errno=%d", errno);
	ATF_REQUIRE(fstat(fd, &st) == 0);
	ATF_REQUIRE((st.st_mode & ACCESSPERMS) == 0600);
	close(fd);
	ATF_REQUIRE(shm_unlink(test_path) == 0);

	fd = shm_open(test_path, O_CREAT | O_RDWR, 0660);
	ATF_REQUIRE_MSG(fd >= 0, "shm_open failed; errno=%d", errno);
	ATF_REQUIRE(fstat(fd, &st) == 0);
	ATF_REQUIRE((st.st_mode & ACCESSPERMS) == 0660);
	close(fd);
	ATF_REQUIRE(shm_unlink(test_path) == 0);

	fd = shm_open(test_path, O_CREAT | O_RDWR, 0666);
	ATF_REQUIRE_MSG(fd >= 0, "shm_open failed; errno=%d", errno);
	ATF_REQUIRE(fstat(fd, &st) == 0);
	ATF_REQUIRE((st.st_mode & ACCESSPERMS) == 0666);
	close(fd);
	ATF_REQUIRE(shm_unlink(test_path) == 0);

	umask(restore_mask);
}

ATF_TC_WITHOUT_HEAD(fallocate);
ATF_TC_BODY(fallocate, tc)
{
	struct stat st;
	int error, fd, sz;

	/*
	 * Primitive test case for posix_fallocate with shmd.  Effectively
	 * expected to work like a smarter ftruncate that will grow the region
	 * as needed in a race-free way.
	 */
	fd = shm_open(SHM_ANON, O_RDWR, 0666);
	ATF_REQUIRE_MSG(fd >= 0, "shm_open failed; errno=%d", errno);
	/* Set the initial size. */
	sz = 32;
	ATF_REQUIRE(ftruncate(fd, sz) == 0);

	/* Now grow it. */
	error = 0;
	sz *= 2;
	ATF_REQUIRE_MSG((error = posix_fallocate(fd, 0, sz)) == 0,
	    "posix_fallocate failed; error=%d", error);
	ATF_REQUIRE(fstat(fd, &st) == 0);
	ATF_REQUIRE(st.st_size == sz);
	/* Attempt to shrink it; should succeed, but not change the size. */
	ATF_REQUIRE_MSG((error = posix_fallocate(fd, 0, sz / 2)) == 0,
	    "posix_fallocate failed; error=%d", error);
	ATF_REQUIRE(fstat(fd, &st) == 0);
	ATF_REQUIRE(st.st_size == sz);
	/* Grow it using an offset of sz and len of sz. */
	ATF_REQUIRE_MSG((error = posix_fallocate(fd, sz, sz)) == 0,
	    "posix_fallocate failed; error=%d", error);
	ATF_REQUIRE(fstat(fd, &st) == 0);
	ATF_REQUIRE(st.st_size == sz * 2);

	close(fd);
}

ATF_TC_WITHOUT_HEAD(fspacectl);
ATF_TC_BODY(fspacectl, tc)
{
	struct spacectl_range range;
	off_t offset, length, shm_sz;
	size_t page_size;
	int fd, error;

	page_size = getpagesize();
	shm_sz = shm_max_pages * page_size;

	fd = shm_open("/testtest", O_RDWR | O_CREAT, 0666);
	ATF_REQUIRE_MSG(fd >= 0, "shm_open failed; errno:%d", errno);
	ATF_REQUIRE_MSG((error = posix_fallocate(fd, 0, shm_sz)) == 0,
	    "posix_fallocate failed; error=%d", error);

	/* Aligned fspacectl(fd, SPACECTL_DEALLOC, ...) */
	ATF_REQUIRE(shm_fill(fd, 0, shm_sz) == 0);
	range.r_offset = offset = page_size;
	range.r_len = length = ((shm_max_pages - 1) * page_size) -
	    range.r_offset;
	ATF_CHECK_MSG(fspacectl(fd, SPACECTL_DEALLOC, &range, 0, &range) == 0,
	    "Aligned fspacectl failed; errno=%d", errno);
	ATF_CHECK_MSG(check_content_dealloc(fd, offset, length, shm_sz) == 0,
	    "Aligned fspacectl content checking failed");

	/* Unaligned fspacectl(fd, SPACECTL_DEALLOC, ...) */
	ATF_REQUIRE(shm_fill(fd, 0, shm_sz) == 0);
	range.r_offset = offset = page_size / 2;
	range.r_len = length = (shm_max_pages - 1) * page_size +
	    (page_size / 2) - offset;
	ATF_CHECK_MSG(fspacectl(fd, SPACECTL_DEALLOC, &range, 0, &range) == 0,
	    "Unaligned fspacectl failed; errno=%d", errno);
	ATF_CHECK_MSG(check_content_dealloc(fd, offset, length, shm_sz) == 0,
	    "Unaligned fspacectl content checking failed");

	/* Aligned fspacectl(fd, SPACECTL_DEALLOC, ...) to OFF_MAX */
	ATF_REQUIRE(shm_fill(fd, 0, shm_sz) == 0);
	range.r_offset = offset = page_size;
	range.r_len = length = OFF_MAX - offset;
	ATF_CHECK_MSG(fspacectl(fd, SPACECTL_DEALLOC, &range, 0, &range) == 0,
	    "Aligned fspacectl to OFF_MAX failed; errno=%d", errno);
	ATF_CHECK_MSG(check_content_dealloc(fd, offset, length, shm_sz) == 0,
	    "Aligned fspacectl to OFF_MAX content checking failed");

	/* Unaligned fspacectl(fd, SPACECTL_DEALLOC, ...) to OFF_MAX */
	ATF_REQUIRE(shm_fill(fd, 0, shm_sz) == 0);
	range.r_offset = offset = page_size / 2;
	range.r_len = length = OFF_MAX - offset;
	ATF_CHECK_MSG(fspacectl(fd, SPACECTL_DEALLOC, &range, 0, &range) == 0,
	    "Unaligned fspacectl to OFF_MAX failed; errno=%d", errno);
	ATF_CHECK_MSG(check_content_dealloc(fd, offset, length, shm_sz) == 0,
	    "Unaligned fspacectl to OFF_MAX content checking failed");

	/* Aligned fspacectl(fd, SPACECTL_DEALLOC, ...) past shm_sz */
	ATF_REQUIRE(shm_fill(fd, 0, shm_sz) == 0);
	range.r_offset = offset = page_size;
	range.r_len = length = (shm_max_pages + 1) * page_size - offset;
	ATF_CHECK_MSG(fspacectl(fd, SPACECTL_DEALLOC, &range, 0, &range) == 0,
	    "Aligned fspacectl past shm_sz failed; errno=%d", errno);
	ATF_CHECK_MSG(check_content_dealloc(fd, offset, length, shm_sz) == 0,
	    "Aligned fspacectl past shm_sz content checking failed");

	/* Unaligned fspacectl(fd, SPACECTL_DEALLOC, ...) past shm_sz */
	ATF_REQUIRE(shm_fill(fd, 0, shm_sz) == 0);
	range.r_offset = offset = page_size / 2;
	range.r_len = length = (shm_max_pages + 1) * page_size - offset;
	ATF_CHECK_MSG(fspacectl(fd, SPACECTL_DEALLOC, &range, 0, &range) == 0,
	    "Unaligned fspacectl past shm_sz failed; errno=%d", errno);
	ATF_CHECK_MSG(check_content_dealloc(fd, offset, length, shm_sz) == 0,
	    "Unaligned fspacectl past shm_sz content checking failed");

	ATF_REQUIRE(close(fd) == 0);
}

ATF_TC_WITHOUT_HEAD(accounting);
ATF_TC_BODY(accounting, tc)
{
	struct spacectl_range range;
	struct stat st;
	off_t shm_sz, len;
	size_t page_size;
	int fd, error;

	page_size = getpagesize();
	shm_sz = shm_max_pages * page_size;

	fd = shm_open("/testtest1", O_RDWR | O_CREAT, 0666);
	ATF_REQUIRE_MSG(fd >= 0, "shm_open failed; errno:%d", errno);
	ATF_REQUIRE_MSG((error = posix_fallocate(fd, 0, shm_sz)) == 0,
	    "posix_fallocate failed; error=%d", error);

	ATF_REQUIRE(shm_fill(fd, 0, shm_sz) == 0);
	ATF_REQUIRE(fstat(fd, &st) == 0);
	ATF_REQUIRE(st.st_blksize * st.st_blocks == (blkcnt_t)shm_sz);

	range.r_offset = page_size;
	range.r_len = len = (shm_max_pages - 1) * page_size -
	    range.r_offset;
	ATF_CHECK_MSG(fspacectl(fd, SPACECTL_DEALLOC, &range, 0, &range) == 0,
	    "SPACECTL_DEALLOC failed; errno=%d", errno);
	ATF_REQUIRE(fstat(fd, &st) == 0);
	ATF_REQUIRE(st.st_blksize * st.st_blocks == (blkcnt_t)(shm_sz - len));

	ATF_REQUIRE(close(fd) == 0);
}

static int
shm_open_large(int psind, int policy, size_t sz)
{
	int error, fd;

	fd = shm_create_largepage(SHM_ANON, O_CREAT | O_RDWR, psind, policy, 0);
	if (fd < 0 && errno == ENOTTY)
		atf_tc_skip("no large page support");
	ATF_REQUIRE_MSG(fd >= 0, "shm_create_largepage failed; errno=%d", errno);

	error = ftruncate(fd, sz);
	if (error != 0 && errno == ENOMEM)
		/*
		 * The test system might not have enough memory to accommodate
		 * the request.
		 */
		atf_tc_skip("failed to allocate %zu-byte superpage", sz);
	ATF_REQUIRE_MSG(error == 0, "ftruncate failed; errno=%d", errno);

	return (fd);
}

static int
pagesizes(size_t ps[MAXPAGESIZES])
{
	int pscnt;

	pscnt = getpagesizes(ps, MAXPAGESIZES);
	ATF_REQUIRE_MSG(pscnt != -1, "getpagesizes failed; errno=%d", errno);
	ATF_REQUIRE_MSG(ps[0] != 0, "psind 0 is %zu", ps[0]);
	ATF_REQUIRE_MSG(pscnt <= MAXPAGESIZES, "invalid pscnt %d", pscnt);
	if (pscnt == 1)
		atf_tc_skip("no large page support");
	return (pscnt);
}

ATF_TC_WITHOUT_HEAD(largepage_basic);
ATF_TC_BODY(largepage_basic, tc)
{
	char *zeroes;
	char *addr, *vec;
	size_t ps[MAXPAGESIZES];
	int error, fd, pscnt;

	pscnt = pagesizes(ps);
	zeroes = calloc(1, ps[0]);
	ATF_REQUIRE(zeroes != NULL);
	for (int i = 1; i < pscnt; i++) {
		fd = shm_open_large(i, SHM_LARGEPAGE_ALLOC_DEFAULT, ps[i]);

		addr = mmap(NULL, ps[i], PROT_READ | PROT_WRITE, MAP_SHARED, fd,
		    0);
		ATF_REQUIRE_MSG(addr != MAP_FAILED,
		    "mmap(%zu bytes) failed; errno=%d", ps[i], errno);
		ATF_REQUIRE_MSG(((uintptr_t)addr & (ps[i] - 1)) == 0,
		    "mmap(%zu bytes) returned unaligned mapping; addr=%p",
		    ps[i], addr);

		/* Force a page fault. */
		*(volatile char *)addr = 0;

		vec = malloc(ps[i] / ps[0]);
		ATF_REQUIRE(vec != NULL);
		error = mincore(addr, ps[i], vec);
		ATF_REQUIRE_MSG(error == 0, "mincore failed; errno=%d", errno);

		/* Verify that all pages in the run are mapped. */
		for (size_t p = 0; p < ps[i] / ps[0]; p++) {
			ATF_REQUIRE_MSG((vec[p] & MINCORE_INCORE) != 0,
			    "page %zu is not mapped", p);
			ATF_REQUIRE_MSG((vec[p] & MINCORE_SUPER) ==
			    MINCORE_PSIND(i),
			    "page %zu is not in a %zu-byte superpage",
			    p, ps[i]);
		}

		/* Validate zeroing. */
		for (size_t p = 0; p < ps[i] / ps[0]; p++) {
			ATF_REQUIRE_MSG(memcmp(addr + p * ps[0], zeroes,
			    ps[0]) == 0, "page %zu miscompare", p);
		}

		free(vec);
		ATF_REQUIRE(munmap(addr, ps[i]) == 0);
		ATF_REQUIRE(close(fd) == 0);
	}

	free(zeroes);
}

extern int __sys_shm_open2(const char *, int, mode_t, int, const char *);

ATF_TC_WITHOUT_HEAD(largepage_config);
ATF_TC_BODY(largepage_config, tc)
{
	struct shm_largepage_conf lpc;
	char *addr, *buf;
	size_t ps[MAXPAGESIZES + 1]; /* silence warnings if MAXPAGESIZES == 1 */
	int error, fd;

	(void)pagesizes(ps);

	fd = shm_open(SHM_ANON, O_CREAT | O_RDWR, 0);
	ATF_REQUIRE_MSG(fd >= 0, "shm_open failed; error=%d", errno);

	/*
	 * Configure a large page policy for an object created without
	 * SHM_LARGEPAGE.
	 */
	lpc.psind = 1;
	lpc.alloc_policy = SHM_LARGEPAGE_ALLOC_DEFAULT;
	error = ioctl(fd, FIOSSHMLPGCNF, &lpc);
	ATF_REQUIRE(error != 0);
	ATF_REQUIRE_MSG(errno == ENOTTY, "ioctl(FIOSSHMLPGCNF) returned %d",
	    errno);
	ATF_REQUIRE(close(fd) == 0);

	/*
	 * Create a largepage object and try to use it without actually
	 * configuring anything.
	 */
	fd = __sys_shm_open2(SHM_ANON, O_CREAT | O_RDWR, 0, SHM_LARGEPAGE,
	    NULL);
	if (fd < 0 && errno == ENOTTY)
		atf_tc_skip("no large page support");
	ATF_REQUIRE_MSG(fd >= 0, "shm_open2 failed; error=%d", errno);

	error = ftruncate(fd, ps[1]);
	ATF_REQUIRE(error != 0);
	ATF_REQUIRE_MSG(errno == EINVAL, "ftruncate returned %d", errno);

	addr = mmap(NULL, ps[1], PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	ATF_REQUIRE(addr == MAP_FAILED);
	ATF_REQUIRE_MSG(errno == EINVAL, "mmap returned %d", errno);
	addr = mmap(NULL, 0, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	ATF_REQUIRE(addr == MAP_FAILED);
	ATF_REQUIRE_MSG(errno == EINVAL, "mmap returned %d", errno);

	buf = calloc(1, ps[0]);
	ATF_REQUIRE(buf != NULL);
	ATF_REQUIRE(write(fd, buf, ps[0]) == -1);
	ATF_REQUIRE_MSG(errno == EINVAL, "write returned %d", errno);
	free(buf);
	buf = calloc(1, ps[1]);
	ATF_REQUIRE(buf != NULL);
	ATF_REQUIRE(write(fd, buf, ps[1]) == -1);
	ATF_REQUIRE_MSG(errno == EINVAL, "write returned %d", errno);
	free(buf);

	error = posix_fallocate(fd, 0, ps[0]);
	ATF_REQUIRE_MSG(error == EINVAL, "posix_fallocate returned %d", error);

	ATF_REQUIRE(close(fd) == 0);
}

ATF_TC_WITHOUT_HEAD(largepage_mmap);
ATF_TC_BODY(largepage_mmap, tc)
{
	char *addr, *addr1, *vec;
	size_t ps[MAXPAGESIZES];
	int fd, pscnt;

	pscnt = pagesizes(ps);
	for (int i = 1; i < pscnt; i++) {
		fd = shm_open_large(i, SHM_LARGEPAGE_ALLOC_DEFAULT, ps[i]);

		/* For mincore(). */
		vec = malloc(ps[i]);
		ATF_REQUIRE(vec != NULL);

		/*
		 * Wrong mapping size.
		 */
		addr = mmap(NULL, ps[i - 1], PROT_READ | PROT_WRITE, MAP_SHARED,
		    fd, 0);
		ATF_REQUIRE_MSG(addr == MAP_FAILED,
		    "mmap(%zu bytes) succeeded", ps[i - 1]);
		ATF_REQUIRE_MSG(errno == EINVAL,
		    "mmap(%zu bytes) failed; error=%d", ps[i - 1], errno);

		/*
		 * Fixed mappings.
		 */
		addr = mmap(NULL, ps[i], PROT_READ | PROT_WRITE, MAP_SHARED, fd,
		    0);
		ATF_REQUIRE_MSG(addr != MAP_FAILED,
		    "mmap(%zu bytes) failed; errno=%d", ps[i], errno);
		ATF_REQUIRE_MSG(((uintptr_t)addr & (ps[i] - 1)) == 0,
		    "mmap(%zu bytes) returned unaligned mapping; addr=%p",
		    ps[i], addr);

		/* Try mapping a small page with anonymous memory. */
		addr1 = mmap(addr, ps[i - 1], PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
		ATF_REQUIRE_MSG(addr1 == MAP_FAILED,
		    "anon mmap(%zu bytes) succeeded", ps[i - 1]);
		ATF_REQUIRE_MSG(errno == EINVAL, "mmap returned %d", errno);

		/* Check MAP_EXCL when creating a second largepage mapping. */
		addr1 = mmap(addr, ps[i], PROT_READ | PROT_WRITE,
		    MAP_SHARED | MAP_FIXED | MAP_EXCL, fd, 0);
		ATF_REQUIRE_MSG(addr1 == MAP_FAILED,
		    "mmap(%zu bytes) succeeded", ps[i]);
		/* XXX wrong errno */
		ATF_REQUIRE_MSG(errno == ENOSPC, "mmap returned %d", errno);

		/* Overwrite a largepage mapping with a lagepage mapping. */
		addr1 = mmap(addr, ps[i], PROT_READ | PROT_WRITE,
		    MAP_SHARED | MAP_FIXED, fd, 0);
		ATF_REQUIRE_MSG(addr1 != MAP_FAILED,
		    "mmap(%zu bytes) failed; errno=%d", ps[i], errno);
		ATF_REQUIRE_MSG(addr == addr1,
		    "mmap(%zu bytes) moved from %p to %p", ps[i], addr, addr1);

		ATF_REQUIRE(munmap(addr, ps[i] == 0));

		/* Clobber an anonymous mapping with a superpage. */
		addr1 = mmap(NULL, ps[i], PROT_READ | PROT_WRITE,
		    MAP_ANON | MAP_PRIVATE | MAP_ALIGNED(ffsl(ps[i]) - 1), -1,
		    0);
		ATF_REQUIRE_MSG(addr1 != MAP_FAILED,
		    "mmap failed; error=%d", errno);
		*(volatile char *)addr1 = '\0';
		addr = mmap(addr1, ps[i], PROT_READ | PROT_WRITE,
		    MAP_SHARED | MAP_FIXED, fd, 0);
		ATF_REQUIRE_MSG(addr != MAP_FAILED,
		    "mmap failed; error=%d", errno);
		ATF_REQUIRE_MSG(addr == addr1,
		    "mmap disobeyed MAP_FIXED, %p %p", addr, addr1);
		*(volatile char *)addr = 0; /* fault */
		ATF_REQUIRE(mincore(addr, ps[i], vec) == 0);
		for (size_t p = 0; p < ps[i] / ps[0]; p++) {
			ATF_REQUIRE_MSG((vec[p] & MINCORE_INCORE) != 0,
			    "page %zu is not resident", p);
			ATF_REQUIRE_MSG((vec[p] & MINCORE_SUPER) ==
			    MINCORE_PSIND(i),
			    "page %zu is not resident", p);
		}

		/*
		 * Copy-on-write mappings are not permitted.
		 */
		addr = mmap(NULL, ps[i], PROT_READ | PROT_WRITE, MAP_PRIVATE,
		    fd, 0);
		ATF_REQUIRE_MSG(addr == MAP_FAILED,
		    "mmap(%zu bytes) succeeded", ps[i]);

		ATF_REQUIRE(close(fd) == 0);
	}
}

ATF_TC_WITHOUT_HEAD(largepage_munmap);
ATF_TC_BODY(largepage_munmap, tc)
{
	char *addr;
	size_t ps[MAXPAGESIZES], ps1;
	int fd, pscnt;

	pscnt = pagesizes(ps);
	for (int i = 1; i < pscnt; i++) {
		fd = shm_open_large(i, SHM_LARGEPAGE_ALLOC_DEFAULT, ps[i]);
		ps1 = ps[i - 1];

		addr = mmap(NULL, ps[i], PROT_READ | PROT_WRITE, MAP_SHARED, fd,
		    0);
		ATF_REQUIRE_MSG(addr != MAP_FAILED,
		    "mmap(%zu bytes) failed; errno=%d", ps[i], errno);

		/* Try several unaligned munmap() requests. */
		ATF_REQUIRE(munmap(addr, ps1) != 0);
		ATF_REQUIRE_MSG(errno == EINVAL,
		    "unexpected error %d from munmap", errno);
		ATF_REQUIRE(munmap(addr, ps[i] - ps1));
		ATF_REQUIRE_MSG(errno == EINVAL,
		    "unexpected error %d from munmap", errno);
		ATF_REQUIRE(munmap(addr + ps1, ps1) != 0);
		ATF_REQUIRE_MSG(errno == EINVAL,
		    "unexpected error %d from munmap", errno);
		ATF_REQUIRE(munmap(addr, 0));
		ATF_REQUIRE_MSG(errno == EINVAL,
		    "unexpected error %d from munmap", errno);

		ATF_REQUIRE(munmap(addr, ps[i]) == 0);
		ATF_REQUIRE(close(fd) == 0);
	}
}

static void
largepage_madvise(char *addr, size_t sz, int advice, int error)
{
	if (error == 0) {
		ATF_REQUIRE_MSG(madvise(addr, sz, advice) == 0,
		    "madvise(%zu, %d) failed; error=%d", sz, advice, errno);
	} else {
		ATF_REQUIRE_MSG(madvise(addr, sz, advice) != 0,
		    "madvise(%zu, %d) succeeded", sz, advice);
		ATF_REQUIRE_MSG(errno == error,
		    "unexpected error %d from madvise(%zu, %d)",
		    errno, sz, advice);
	}
}

ATF_TC_WITHOUT_HEAD(largepage_madvise);
ATF_TC_BODY(largepage_madvise, tc)
{
	char *addr;
	size_t ps[MAXPAGESIZES];
	int fd, pscnt;

	pscnt = pagesizes(ps);
	for (int i = 1; i < pscnt; i++) {
		fd = shm_open_large(i, SHM_LARGEPAGE_ALLOC_DEFAULT, ps[i]);
		addr = mmap(NULL, ps[i], PROT_READ | PROT_WRITE, MAP_SHARED, fd,
		    0);
		ATF_REQUIRE_MSG(addr != MAP_FAILED,
		    "mmap(%zu bytes) failed; error=%d", ps[i], errno);

		memset(addr, 0, ps[i]);

		/* Advice that requires clipping. */
		largepage_madvise(addr, ps[0], MADV_NORMAL, EINVAL);
		largepage_madvise(addr, ps[i], MADV_NORMAL, 0);
		largepage_madvise(addr, ps[0], MADV_RANDOM, EINVAL);
		largepage_madvise(addr, ps[i], MADV_RANDOM, 0);
		largepage_madvise(addr, ps[0], MADV_SEQUENTIAL, EINVAL);
		largepage_madvise(addr, ps[i], MADV_SEQUENTIAL, 0);
		largepage_madvise(addr, ps[0], MADV_NOSYNC, EINVAL);
		largepage_madvise(addr, ps[i], MADV_NOSYNC, 0);
		largepage_madvise(addr, ps[0], MADV_AUTOSYNC, EINVAL);
		largepage_madvise(addr, ps[i], MADV_AUTOSYNC, 0);
		largepage_madvise(addr, ps[0], MADV_CORE, EINVAL);
		largepage_madvise(addr, ps[i], MADV_CORE, 0);
		largepage_madvise(addr, ps[0], MADV_NOCORE, EINVAL);
		largepage_madvise(addr, ps[i], MADV_NOCORE, 0);

		/* Advice that does not result in clipping. */
		largepage_madvise(addr, ps[0], MADV_DONTNEED, 0);
		largepage_madvise(addr, ps[i], MADV_DONTNEED, 0);
		largepage_madvise(addr, ps[0], MADV_WILLNEED, 0);
		largepage_madvise(addr, ps[i], MADV_WILLNEED, 0);
		largepage_madvise(addr, ps[0], MADV_FREE, 0);
		largepage_madvise(addr, ps[i], MADV_FREE, 0);

		ATF_REQUIRE(munmap(addr, ps[i]) == 0);
		ATF_REQUIRE(close(fd) == 0);
	}
}

ATF_TC(largepage_mlock);
ATF_TC_HEAD(largepage_mlock, tc)
{
	/* Needed to set rlimit. */
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(largepage_mlock, tc)
{
	struct rlimit rl;
	char *addr;
	size_t ps[MAXPAGESIZES], sz;
	u_long max_wired, wired;
	int fd, error, pscnt;

	rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;
	ATF_REQUIRE_MSG(setrlimit(RLIMIT_MEMLOCK, &rl) == 0,
	    "setrlimit failed; error=%d", errno);

	sz = sizeof(max_wired);
	error = sysctlbyname("vm.max_user_wired", &max_wired, &sz, NULL, 0);
	ATF_REQUIRE_MSG(error == 0,
	    "sysctlbyname(vm.max_user_wired) failed; error=%d", errno);

	sz = sizeof(wired);
	error = sysctlbyname("vm.stats.vm.v_user_wire_count", &wired, &sz, NULL,
	    0);
	ATF_REQUIRE_MSG(error == 0,
	    "sysctlbyname(vm.stats.vm.v_user_wire_count) failed; error=%d",
	    errno);

	pscnt = pagesizes(ps);
	for (int i = 1; i < pscnt; i++) {
		if (ps[i] / ps[0] > max_wired - wired) {
			/* Cannot wire past the limit. */
			atf_tc_skip("test would exceed wiring limit");
		}

		fd = shm_open_large(i, SHM_LARGEPAGE_ALLOC_DEFAULT, ps[i]);
		addr = mmap(NULL, ps[i], PROT_READ | PROT_WRITE, MAP_SHARED, fd,
		    0);
		ATF_REQUIRE_MSG(addr != MAP_FAILED,
		    "mmap(%zu bytes) failed; error=%d", ps[i], errno);

		ATF_REQUIRE(mlock(addr, ps[0]) != 0);
		ATF_REQUIRE_MSG(errno == EINVAL,
		    "unexpected error %d from mlock(%zu bytes)", errno, ps[i]);
		ATF_REQUIRE(mlock(addr, ps[i] - ps[0]) != 0);
		ATF_REQUIRE_MSG(errno == EINVAL,
		    "unexpected error %d from mlock(%zu bytes)", errno, ps[i]);

		ATF_REQUIRE_MSG(mlock(addr, ps[i]) == 0,
		    "mlock failed; error=%d", errno);

		ATF_REQUIRE(munmap(addr, ps[i]) == 0);

		ATF_REQUIRE(mlockall(MCL_FUTURE) == 0);
		addr = mmap(NULL, ps[i], PROT_READ | PROT_WRITE, MAP_SHARED, fd,
		    0);
		ATF_REQUIRE_MSG(addr != MAP_FAILED,
		    "mmap(%zu bytes) failed; error=%d", ps[i], errno);

		ATF_REQUIRE(munmap(addr, ps[i]) == 0);
		ATF_REQUIRE(close(fd) == 0);
	}
}

ATF_TC_WITHOUT_HEAD(largepage_msync);
ATF_TC_BODY(largepage_msync, tc)
{
	char *addr;
	size_t ps[MAXPAGESIZES];
	int fd, pscnt;

	pscnt = pagesizes(ps);
	for (int i = 1; i < pscnt; i++) {
		fd = shm_open_large(i, SHM_LARGEPAGE_ALLOC_DEFAULT, ps[i]);
		addr = mmap(NULL, ps[i], PROT_READ | PROT_WRITE, MAP_SHARED, fd,
		    0);
		ATF_REQUIRE_MSG(addr != MAP_FAILED,
		    "mmap(%zu bytes) failed; error=%d", ps[i], errno);

		memset(addr, 0, ps[i]);

		/*
		 * "Sync" requests are no-ops for SHM objects, so small
		 * PAGE_SIZE-sized requests succeed.
		 */
		ATF_REQUIRE_MSG(msync(addr, ps[0], MS_ASYNC) == 0,
		    "msync(MS_ASYNC) failed; error=%d", errno);
		ATF_REQUIRE_MSG(msync(addr, ps[i], MS_ASYNC) == 0,
		    "msync(MS_ASYNC) failed; error=%d", errno);
		ATF_REQUIRE_MSG(msync(addr, ps[0], MS_SYNC) == 0,
		    "msync(MS_SYNC) failed; error=%d", errno);
		ATF_REQUIRE_MSG(msync(addr, ps[i], MS_SYNC) == 0,
		    "msync(MS_SYNC) failed; error=%d", errno);

		ATF_REQUIRE_MSG(msync(addr, ps[0], MS_INVALIDATE) != 0,
		    "msync(MS_INVALIDATE) succeeded");
		/* XXX wrong errno */
		ATF_REQUIRE_MSG(errno == EBUSY,
		    "unexpected error %d from msync(MS_INVALIDATE)", errno);
		ATF_REQUIRE_MSG(msync(addr, ps[i], MS_INVALIDATE) == 0,
		    "msync(MS_INVALIDATE) failed; error=%d", errno);
		memset(addr, 0, ps[i]);

		ATF_REQUIRE(munmap(addr, ps[i]) == 0);
		ATF_REQUIRE(close(fd) == 0);
	}
}

static void
largepage_protect(char *addr, size_t sz, int prot, int error)
{
	if (error == 0) {
		ATF_REQUIRE_MSG(mprotect(addr, sz, prot) == 0,
		    "mprotect(%zu, %x) failed; error=%d", sz, prot, errno);
	} else {
		ATF_REQUIRE_MSG(mprotect(addr, sz, prot) != 0,
		    "mprotect(%zu, %x) succeeded", sz, prot);
		ATF_REQUIRE_MSG(errno == error,
		    "unexpected error %d from mprotect(%zu, %x)",
		    errno, sz, prot);
	}
}

ATF_TC_WITHOUT_HEAD(largepage_mprotect);
ATF_TC_BODY(largepage_mprotect, tc)
{
	char *addr, *addr1;
	size_t ps[MAXPAGESIZES];
	int fd, pscnt;

	pscnt = pagesizes(ps);
	for (int i = 1; i < pscnt; i++) {
		/*
		 * Reserve a contiguous region in the address space to avoid
		 * spurious failures in the face of ASLR.
		 */
		addr = mmap(NULL, ps[i] * 2, PROT_NONE,
		    MAP_ANON | MAP_ALIGNED(ffsl(ps[i]) - 1), -1, 0);
		ATF_REQUIRE_MSG(addr != MAP_FAILED,
		    "mmap(%zu bytes) failed; error=%d", ps[i], errno);
		ATF_REQUIRE(munmap(addr, ps[i] * 2) == 0);

		fd = shm_open_large(i, SHM_LARGEPAGE_ALLOC_DEFAULT, ps[i]);
		addr = mmap(addr, ps[i], PROT_READ | PROT_WRITE,
		    MAP_SHARED | MAP_FIXED, fd, 0);
		ATF_REQUIRE_MSG(addr != MAP_FAILED,
		    "mmap(%zu bytes) failed; error=%d", ps[i], errno);

		/*
		 * These should be no-ops from the pmap perspective since the
		 * page is not yet entered into the pmap.
		 */
		largepage_protect(addr, ps[0], PROT_READ, EINVAL);
		largepage_protect(addr, ps[i], PROT_READ, 0);
		largepage_protect(addr, ps[0], PROT_NONE, EINVAL);
		largepage_protect(addr, ps[i], PROT_NONE, 0);
		largepage_protect(addr, ps[0],
		    PROT_READ | PROT_WRITE | PROT_EXEC, EINVAL);
		largepage_protect(addr, ps[i],
		    PROT_READ | PROT_WRITE | PROT_EXEC, 0);

		/* Trigger creation of a mapping and try again. */
		*(volatile char *)addr = 0;
		largepage_protect(addr, ps[0], PROT_READ, EINVAL);
		largepage_protect(addr, ps[i], PROT_READ, 0);
		largepage_protect(addr, ps[0], PROT_NONE, EINVAL);
		largepage_protect(addr, ps[i], PROT_NONE, 0);
		largepage_protect(addr, ps[0],
		    PROT_READ | PROT_WRITE | PROT_EXEC, EINVAL);
		largepage_protect(addr, ps[i],
		    PROT_READ | PROT_WRITE | PROT_EXEC, 0);

		memset(addr, 0, ps[i]);

		/* Map two contiguous large pages and merge map entries. */
		addr1 = mmap(addr + ps[i], ps[i], PROT_READ | PROT_WRITE,
		    MAP_SHARED | MAP_FIXED | MAP_EXCL, fd, 0);
		ATF_REQUIRE_MSG(addr1 != MAP_FAILED,
		    "mmap(%zu bytes) failed; error=%d", ps[i], errno);

		largepage_protect(addr1 - ps[0], ps[0] * 2,
		    PROT_READ | PROT_WRITE, EINVAL);
		largepage_protect(addr, ps[i] * 2, PROT_READ | PROT_WRITE, 0);

		memset(addr, 0, ps[i] * 2);

		ATF_REQUIRE(munmap(addr, ps[i]) == 0);
		ATF_REQUIRE(munmap(addr1, ps[i]) == 0);
		ATF_REQUIRE(close(fd) == 0);
	}
}

ATF_TC_WITHOUT_HEAD(largepage_minherit);
ATF_TC_BODY(largepage_minherit, tc)
{
	char *addr;
	size_t ps[MAXPAGESIZES];
	pid_t child;
	int fd, pscnt, status;

	pscnt = pagesizes(ps);
	for (int i = 1; i < pscnt; i++) {
		fd = shm_open_large(i, SHM_LARGEPAGE_ALLOC_DEFAULT, ps[i]);
		addr = mmap(NULL, ps[i], PROT_READ | PROT_WRITE, MAP_SHARED, fd,
		    0);
		ATF_REQUIRE_MSG(addr != MAP_FAILED,
		    "mmap(%zu bytes) failed; error=%d", ps[i], errno);

		ATF_REQUIRE(minherit(addr, ps[0], INHERIT_SHARE) != 0);

		ATF_REQUIRE_MSG(minherit(addr, ps[i], INHERIT_SHARE) == 0,
		    "minherit(%zu bytes) failed; error=%d", ps[i], errno);
		child = fork();
		ATF_REQUIRE_MSG(child != -1, "fork failed; error=%d", errno);
		if (child == 0) {
			char v;

			*(volatile char *)addr = 0;
			if (mincore(addr, ps[0], &v) != 0)
				_exit(1);
			if ((v & MINCORE_SUPER) == 0)
				_exit(2);
			_exit(0);
		}
		ATF_REQUIRE_MSG(waitpid(child, &status, 0) == child,
		    "waitpid failed; error=%d", errno);
		ATF_REQUIRE_MSG(WIFEXITED(status),
		    "child was killed by signal %d", WTERMSIG(status));
		ATF_REQUIRE_MSG(WEXITSTATUS(status) == 0,
		    "child exited with status %d", WEXITSTATUS(status));

		ATF_REQUIRE_MSG(minherit(addr, ps[i], INHERIT_NONE) == 0,
		    "minherit(%zu bytes) failed; error=%d", ps[i], errno);
		child = fork();
		ATF_REQUIRE_MSG(child != -1, "fork failed; error=%d", errno);
		if (child == 0) {
			char v;

			if (mincore(addr, ps[0], &v) == 0)
				_exit(1);
			_exit(0);
		}
		ATF_REQUIRE_MSG(waitpid(child, &status, 0) == child,
		    "waitpid failed; error=%d", errno);
		ATF_REQUIRE_MSG(WIFEXITED(status),
		    "child was killed by signal %d", WTERMSIG(status));
		ATF_REQUIRE_MSG(WEXITSTATUS(status) == 0,
		    "child exited with status %d", WEXITSTATUS(status));

		/* Copy-on-write is not supported for static large pages. */
		ATF_REQUIRE_MSG(minherit(addr, ps[i], INHERIT_COPY) != 0,
		    "minherit(%zu bytes) succeeded", ps[i]);

		ATF_REQUIRE_MSG(minherit(addr, ps[i], INHERIT_ZERO) == 0,
		    "minherit(%zu bytes) failed; error=%d", ps[i], errno);
		child = fork();
		ATF_REQUIRE_MSG(child != -1, "fork failed; error=%d", errno);
		if (child == 0) {
			char v;

			*(volatile char *)addr = 0;
			if (mincore(addr, ps[0], &v) != 0)
				_exit(1);
			if ((v & MINCORE_SUPER) != 0)
				_exit(2);
			_exit(0);
		}
		ATF_REQUIRE_MSG(waitpid(child, &status, 0) == child,
		    "waitpid failed; error=%d", errno);
		ATF_REQUIRE_MSG(WIFEXITED(status),
		    "child was killed by signal %d", WTERMSIG(status));
		ATF_REQUIRE_MSG(WEXITSTATUS(status) == 0,
		    "child exited with status %d", WEXITSTATUS(status));

		ATF_REQUIRE(munmap(addr, ps[i]) == 0);
		ATF_REQUIRE(close(fd) == 0);
	}
}

ATF_TC_WITHOUT_HEAD(largepage_pipe);
ATF_TC_BODY(largepage_pipe, tc)
{
	size_t ps[MAXPAGESIZES];
	char *addr;
	ssize_t len;
	int fd, pfd[2], pscnt, status;
	pid_t child;

	pscnt = pagesizes(ps);

	for (int i = 1; i < pscnt; i++) {
		fd = shm_open_large(i, SHM_LARGEPAGE_ALLOC_DEFAULT, ps[i]);
		addr = mmap(NULL, ps[i], PROT_READ | PROT_WRITE, MAP_SHARED, fd,
		    0);
		ATF_REQUIRE_MSG(addr != MAP_FAILED,
		    "mmap(%zu bytes) failed; error=%d", ps[i], errno);

		/* Trigger creation of a mapping. */
		*(volatile char *)addr = '\0';

		ATF_REQUIRE(pipe(pfd) == 0);
		child = fork();
		ATF_REQUIRE_MSG(child != -1, "fork() failed; error=%d", errno);
		if (child == 0) {
			char buf[BUFSIZ];
			ssize_t resid;

			(void)close(pfd[0]);
			for (resid = (size_t)ps[i]; resid > 0; resid -= len) {
				len = read(pfd[1], buf, sizeof(buf));
				if (len < 0)
					_exit(1);
			}
			_exit(0);
		}
		ATF_REQUIRE(close(pfd[1]) == 0);
		len = write(pfd[0], addr, ps[i]);
		ATF_REQUIRE_MSG(len >= 0, "write() failed; error=%d", errno);
		ATF_REQUIRE_MSG(len == (ssize_t)ps[i],
		    "short write; len=%zd", len);
		ATF_REQUIRE(close(pfd[0]) == 0);

		ATF_REQUIRE_MSG(waitpid(child, &status, 0) == child,
		    "waitpid() failed; error=%d", errno);
		ATF_REQUIRE_MSG(WIFEXITED(status),
		    "child was killed by signal %d", WTERMSIG(status));
		ATF_REQUIRE_MSG(WEXITSTATUS(status) == 0,
		    "child exited with status %d", WEXITSTATUS(status));

		ATF_REQUIRE(munmap(addr, ps[i]) == 0);
		ATF_REQUIRE(close(fd) == 0);
	}
}

ATF_TC_WITHOUT_HEAD(largepage_reopen);
ATF_TC_BODY(largepage_reopen, tc)
{
	char *addr, *vec;
	size_t ps[MAXPAGESIZES];
	int fd, psind;

	(void)pagesizes(ps);
	psind = 1;

	gen_test_path();
	fd = shm_create_largepage(test_path, O_CREAT | O_RDWR, psind,
	    SHM_LARGEPAGE_ALLOC_DEFAULT, 0600);
	if (fd < 0 && errno == ENOTTY)
		atf_tc_skip("no large page support");
	ATF_REQUIRE_MSG(fd >= 0, "shm_create_largepage failed; error=%d", errno);

	ATF_REQUIRE_MSG(ftruncate(fd, ps[psind]) == 0,
	    "ftruncate failed; error=%d", errno);

	ATF_REQUIRE_MSG(close(fd) == 0, "close failed; error=%d", errno);

	fd = shm_open(test_path, O_RDWR, 0);
	ATF_REQUIRE_MSG(fd >= 0, "shm_open failed; error=%d", errno);

	addr = mmap(NULL, ps[psind], PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	ATF_REQUIRE_MSG(addr != MAP_FAILED, "mmap failed; error=%d", errno);

	/* Trigger a fault and mapping creation. */
	*(volatile char *)addr = 0;

	vec = malloc(ps[psind] / ps[0]);
	ATF_REQUIRE(vec != NULL);
	ATF_REQUIRE_MSG(mincore(addr, ps[psind], vec) == 0,
	    "mincore failed; error=%d", errno);
	ATF_REQUIRE_MSG((vec[0] & MINCORE_SUPER) == MINCORE_PSIND(psind),
	    "page not mapped into a %zu-byte superpage", ps[psind]);

	ATF_REQUIRE_MSG(shm_unlink(test_path) == 0,
	    "shm_unlink failed; errno=%d", errno);
	ATF_REQUIRE_MSG(close(fd) == 0,
	    "close failed; errno=%d", errno);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, remap_object);
	ATF_TP_ADD_TC(tp, rename_from_anon);
	ATF_TP_ADD_TC(tp, rename_bad_path_pointer);
	ATF_TP_ADD_TC(tp, rename_from_nonexisting);
	ATF_TP_ADD_TC(tp, rename_to_anon);
	ATF_TP_ADD_TC(tp, rename_to_replace);
	ATF_TP_ADD_TC(tp, rename_to_noreplace);
	ATF_TP_ADD_TC(tp, rename_to_exchange);
	ATF_TP_ADD_TC(tp, rename_to_exchange_nonexisting);
	ATF_TP_ADD_TC(tp, rename_to_self);
	ATF_TP_ADD_TC(tp, rename_bad_flag);
	ATF_TP_ADD_TC(tp, reopen_object);
	ATF_TP_ADD_TC(tp, readonly_mmap_write);
	ATF_TP_ADD_TC(tp, open_after_link);
	ATF_TP_ADD_TC(tp, open_invalid_path);
	ATF_TP_ADD_TC(tp, open_write_only);
	ATF_TP_ADD_TC(tp, open_extra_flags);
	ATF_TP_ADD_TC(tp, open_anon);
	ATF_TP_ADD_TC(tp, open_anon_readonly);
	ATF_TP_ADD_TC(tp, open_bad_path_pointer);
	ATF_TP_ADD_TC(tp, open_path_too_long);
	ATF_TP_ADD_TC(tp, open_nonexisting_object);
	ATF_TP_ADD_TC(tp, open_create_existing_object);
	ATF_TP_ADD_TC(tp, shm_functionality_across_fork);
	ATF_TP_ADD_TC(tp, trunc_resets_object);
	ATF_TP_ADD_TC(tp, unlink_bad_path_pointer);
	ATF_TP_ADD_TC(tp, unlink_path_too_long);
	ATF_TP_ADD_TC(tp, object_resize);
	ATF_TP_ADD_TC(tp, cloexec);
	ATF_TP_ADD_TC(tp, mode);
	ATF_TP_ADD_TC(tp, fallocate);
	ATF_TP_ADD_TC(tp, fspacectl);
	ATF_TP_ADD_TC(tp, accounting);
	ATF_TP_ADD_TC(tp, largepage_basic);
	ATF_TP_ADD_TC(tp, largepage_config);
	ATF_TP_ADD_TC(tp, largepage_mmap);
	ATF_TP_ADD_TC(tp, largepage_munmap);
	ATF_TP_ADD_TC(tp, largepage_madvise);
	ATF_TP_ADD_TC(tp, largepage_mlock);
	ATF_TP_ADD_TC(tp, largepage_msync);
	ATF_TP_ADD_TC(tp, largepage_mprotect);
	ATF_TP_ADD_TC(tp, largepage_minherit);
	ATF_TP_ADD_TC(tp, largepage_pipe);
	ATF_TP_ADD_TC(tp, largepage_reopen);

	return (atf_no_error());
}
