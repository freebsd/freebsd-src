/*-
 * Copyright (c) 2006 Robert N. M. Watson
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test.h"

#define	TEST_PATH	"/tmp/posixshm_regression_test"

/*
 * Attempt a shm_open() that should fail with an expected error of 'error'.
 */
static void
shm_open_should_fail(const char *path, int flags, mode_t mode, int error)
{
	int fd;

	fd = shm_open(path, flags, mode);
	if (fd >= 0) {
		fail_err("shm_open() didn't fail");
		close(fd);
		return;
	}
	if (errno != error) {
		fail_errno("shm_open");
		return;
	}
	pass();
}

/*
 * Attempt a shm_unlink() that should fail with an expected error of 'error'.
 */
static void
shm_unlink_should_fail(const char *path, int error)
{

	if (shm_unlink(path) >= 0) {
		fail_err("shm_unlink() didn't fail");
		return;
	}
	if (errno != error) {
		fail_errno("shm_unlink");
		return;
	}
	pass();
}

/*
 * Open the test object and write '1' to the first byte.  Returns valid fd
 * on success and -1 on failure.
 */
static int
scribble_object(void)
{
	char *page;
	int fd;

	fd = shm_open(TEST_PATH, O_CREAT | O_EXCL | O_RDWR, 0777);
	if (fd < 0 && errno == EEXIST) {
		if (shm_unlink(TEST_PATH) < 0) {
			fail_errno("shm_unlink");
			return (-1);
		}
		fd = shm_open(TEST_PATH, O_CREAT | O_EXCL | O_RDWR, 0777);
	}
	if (fd < 0) {
		fail_errno("shm_open");
		return (-1);
	}
	if (ftruncate(fd, getpagesize()) < 0) {
		fail_errno("ftruncate");
		close(fd);
		shm_unlink(TEST_PATH);
		return (-1);
	}

	page = mmap(0, getpagesize(), PROT_READ | PROT_WRITE, MAP_SHARED, fd,
	    0);
	if (page == MAP_FAILED) {
		fail_errno("mmap");
		close(fd);
		shm_unlink(TEST_PATH);
		return (-1);
	}

	page[0] = '1';

	if (munmap(page, getpagesize()) < 0) {
		fail_errno("munmap");
		close(fd);
		shm_unlink(TEST_PATH);
		return (-1);
	}

	return (fd);
}

static void
remap_object(void)
{
	char *page;
	int fd;

	fd = scribble_object();
	if (fd < 0)
		return;

	page = mmap(0, getpagesize(), PROT_READ | PROT_WRITE, MAP_SHARED, fd,
	    0);
	if (page == MAP_FAILED) {		
		fail_errno("mmap(2)");
		close(fd);
		shm_unlink(TEST_PATH);
		return;
	}

	if (page[0] != '1') {		
		fail_err("missing data");
		close(fd);
		shm_unlink(TEST_PATH);
		return;
	}

	close(fd);
	if (munmap(page, getpagesize()) < 0) {
		fail_errno("munmap");
		shm_unlink(TEST_PATH);
		return;
	}

	if (shm_unlink(TEST_PATH) < 0) {
		fail_errno("shm_unlink");
		return;
	}

	pass();
}
TEST(remap_object, "remap object");

static void
reopen_object(void)
{
	char *page;
	int fd;

	fd = scribble_object();
	if (fd < 0)
		return;
	close(fd);

	fd = shm_open(TEST_PATH, O_RDONLY, 0777);
	if (fd < 0) {
		fail_errno("shm_open(2)");
		shm_unlink(TEST_PATH);
		return;
	}
	page = mmap(0, getpagesize(), PROT_READ, MAP_SHARED, fd, 0);
	if (page == MAP_FAILED) {
		fail_errno("mmap(2)");
		close(fd);
		shm_unlink(TEST_PATH);
		return;
	}

	if (page[0] != '1') {
		fail_err("missing data");
		munmap(page, getpagesize());
		close(fd);
		shm_unlink(TEST_PATH);
		return;
	}

	munmap(page, getpagesize());
	close(fd);
	shm_unlink(TEST_PATH);
	pass();
}
TEST(reopen_object, "reopen object");

static void
readonly_mmap_write(void)
{
	char *page;
	int fd;

	fd = shm_open(TEST_PATH, O_RDONLY | O_CREAT, 0777);
	if (fd < 0) {
		fail_errno("shm_open");
		return;
	}

	/* PROT_WRITE should fail with EACCES. */
	page = mmap(0, getpagesize(), PROT_READ | PROT_WRITE, MAP_SHARED, fd,
	    0);
	if (page != MAP_FAILED) {		
		fail_err("mmap(PROT_WRITE) succeeded");
		munmap(page, getpagesize());
		close(fd);
		shm_unlink(TEST_PATH);
		return;
	}
	if (errno != EACCES) {
		fail_errno("mmap");
		close(fd);
		shm_unlink(TEST_PATH);
		return;
	}

	close(fd);
	shm_unlink(TEST_PATH);
	pass();
}
TEST(readonly_mmap_write, "RDONLY object");

static void
open_after_unlink(void)
{
	int fd;

	fd = shm_open(TEST_PATH, O_RDONLY | O_CREAT, 0777);
	if (fd < 0) {
		fail_errno("shm_open(1)");
		return;
	}
	close(fd);

	if (shm_unlink(TEST_PATH) < 0) {
		fail_errno("shm_unlink");
		return;
	}

	shm_open_should_fail(TEST_PATH, O_RDONLY, 0777, ENOENT);
}
TEST(open_after_unlink, "open after unlink");

static void
open_invalid_path(void)
{

	shm_open_should_fail("blah", O_RDONLY, 0777, EINVAL);
}
TEST(open_invalid_path, "open invalid path");

static void
open_write_only(void)
{

	shm_open_should_fail(TEST_PATH, O_WRONLY, 0777, EINVAL);
}
TEST(open_write_only, "open with O_WRONLY");

static void
open_extra_flags(void)
{

	shm_open_should_fail(TEST_PATH, O_RDONLY | O_DIRECT, 0777, EINVAL);
}
TEST(open_extra_flags, "open with extra flags");

static void
open_anon(void)
{
	int fd;

	fd = shm_open(SHM_ANON, O_RDWR, 0777);
	if (fd < 0) {
		fail_errno("shm_open");
		return;
	}
	close(fd);
	pass();
}
TEST(open_anon, "open anonymous object");

static void
open_anon_readonly(void)
{

	shm_open_should_fail(SHM_ANON, O_RDONLY, 0777, EINVAL);
}
TEST(open_anon_readonly, "open SHM_ANON with O_RDONLY");

static void
open_bad_path_pointer(void)
{

	shm_open_should_fail((char *)1024, O_RDONLY, 0777, EFAULT);
}
TEST(open_bad_path_pointer, "open bad path pointer");

static void
open_path_too_long(void)
{
	char *page;

	page = malloc(MAXPATHLEN + 1);
	memset(page, 'a', MAXPATHLEN);
	page[MAXPATHLEN] = '\0';
	shm_open_should_fail(page, O_RDONLY, 0777, ENAMETOOLONG);
	free(page);
}
TEST(open_path_too_long, "open pathname too long");

static void
open_nonexisting_object(void)
{

	shm_open_should_fail("/notreallythere", O_RDONLY, 0777, ENOENT);
}
TEST(open_nonexisting_object, "open nonexistent object");

static void
exclusive_create_existing_object(void)
{
	int fd;

	fd = shm_open("/tmp/notreallythere", O_RDONLY | O_CREAT, 0777);
	if (fd < 0) {
		fail_errno("shm_open(O_CREAT)");
		return;
	}
	close(fd);

	shm_open_should_fail("/tmp/notreallythere", O_RDONLY | O_CREAT | O_EXCL,
	    0777, EEXIST);

	shm_unlink("/tmp/notreallythere");
}
TEST(exclusive_create_existing_object, "O_EXCL of existing object");

static void
trunc_resets_object(void)
{
	struct stat sb;
	int fd;

	/* Create object and set size to 1024. */
	fd = shm_open(TEST_PATH, O_RDWR | O_CREAT, 0777);
	if (fd < 0) {
		fail_errno("shm_open(1)");
		return;
	}
	if (ftruncate(fd, 1024) < 0) {
		fail_errno("ftruncate");
		close(fd);
		return;
	}
	if (fstat(fd, &sb) < 0) {		
		fail_errno("fstat(1)");
		close(fd);
		return;
	}
	if (sb.st_size != 1024) {
		fail_err("size %d != 1024", (int)sb.st_size);
		close(fd);
		return;
	}
	close(fd);

	/* Open with O_TRUNC which should reset size to 0. */
	fd = shm_open(TEST_PATH, O_RDWR | O_TRUNC, 0777);
	if (fd < 0) {
		fail_errno("shm_open(2)");
		return;
	}
	if (fstat(fd, &sb) < 0) {
		fail_errno("fstat(2)");
		close(fd);
		return;
	}
	if (sb.st_size != 0) {
		fail_err("size after O_TRUNC %d != 0", (int)sb.st_size);
		close(fd);
		return;
	}
	close(fd);
	if (shm_unlink(TEST_PATH) < 0) {
		fail_errno("shm_unlink");
		return;
	}
	pass();
}
TEST(trunc_resets_object, "O_TRUNC resets size");

static void
unlink_bad_path_pointer(void)
{

	shm_unlink_should_fail((char *)1024, EFAULT);
}
TEST(unlink_bad_path_pointer, "unlink bad path pointer");

static void
unlink_path_too_long(void)
{
	char *page;

	page = malloc(MAXPATHLEN + 1);
	memset(page, 'a', MAXPATHLEN);
	page[MAXPATHLEN] = '\0';
	shm_unlink_should_fail(page, ENAMETOOLONG);
	free(page);
}
TEST(unlink_path_too_long, "unlink pathname too long");

static void
test_object_resize(void)
{
	pid_t pid;
	struct stat sb;
	char *page;
	int fd, status;

	/* Start off with a size of a single page. */
	fd = shm_open(SHM_ANON, O_CREAT | O_RDWR, 0777);
	if (fd < 0) {
		fail_errno("shm_open");
		return;
	}
	if (ftruncate(fd, getpagesize()) < 0) {
		fail_errno("ftruncate(1)");
		close(fd);
		return;
	}
	if (fstat(fd, &sb) < 0) {
		fail_errno("fstat(1)");
		close(fd);
		return;
	}
	if (sb.st_size != getpagesize()) {
		fail_err("first resize failed");
		close(fd);
		return;
	}

	/* Write a '1' to the first byte. */
	page = mmap(0, getpagesize(), PROT_READ | PROT_WRITE, MAP_SHARED, fd,
	    0);
	if (page == MAP_FAILED) {
		fail_errno("mmap(1)");
		close(fd);
		return;
	}

	page[0] = '1';

	if (munmap(page, getpagesize()) < 0) {
		fail_errno("munmap(1)");
		close(fd);
		return;
	}

	/* Grow the object to 2 pages. */
	if (ftruncate(fd, getpagesize() * 2) < 0) {
		fail_errno("ftruncate(2)");
		close(fd);
		return;
	}
	if (fstat(fd, &sb) < 0) {
		fail_errno("fstat(2)");
		close(fd);
		return;
	}
	if (sb.st_size != getpagesize() * 2) {
		fail_err("second resize failed");
		close(fd);
		return;
	}

	/* Check for '1' at the first byte. */
	page = mmap(0, getpagesize() * 2, PROT_READ | PROT_WRITE, MAP_SHARED,
	    fd, 0);
	if (page == MAP_FAILED) {
		fail_errno("mmap(2)");
		close(fd);
		return;
	}

	if (page[0] != '1') {
		fail_err("missing data at 0");
		close(fd);
		return;
	}

	/* Write a '2' at the start of the second page. */
	page[getpagesize()] = '2';

	/* Shrink the object back to 1 page. */
	if (ftruncate(fd, getpagesize()) < 0) {
		fail_errno("ftruncate(3)");
		close(fd);
		return;
	}
	if (fstat(fd, &sb) < 0) {
		fail_errno("fstat(3)");
		close(fd);
		return;
	}
	if (sb.st_size != getpagesize()) {
		fail_err("third resize failed");
		close(fd);
		return;
	}

	/*
	 * Fork a child process to make sure the second page is no
	 * longer valid.
	 */
	pid = fork();
	if (pid < 0) {
		fail_errno("fork");
		close(fd);
		return;
	}

	if (pid == 0) {
		struct rlimit lim;
		char c;

		/* Don't generate a core dump. */
		getrlimit(RLIMIT_CORE, &lim);
		lim.rlim_cur = 0;
		setrlimit(RLIMIT_CORE, &lim);

		/*
		 * The previous ftruncate(2) shrunk the backing object
		 * so that this address is no longer valid, so reading
		 * from it should trigger a SIGSEGV.
		 */
		c = page[getpagesize()];
		fprintf(stderr, "child: page 1: '%c'\n", c);
		exit(0);
	}
	if (wait(&status) < 0) {
		fail_errno("wait");
		close(fd);
		return;
	}
	if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGSEGV) {
		fail_err("child terminated with status %x", status);
		close(fd);
		return;
	}

	/* Grow the object back to 2 pages. */
	if (ftruncate(fd, getpagesize() * 2) < 0) {
		fail_errno("ftruncate(4)");
		close(fd);
		return;
	}
	if (fstat(fd, &sb) < 0) {
		fail_errno("fstat(4)");
		close(fd);
		return;
	}
	if (sb.st_size != getpagesize() * 2) {
		fail_err("second resize failed");
		close(fd);
		return;
	}

	/*
	 * Note that the mapping at 'page' for the second page is
	 * still valid, and now that the shm object has been grown
	 * back up to 2 pages, there is now memory backing this page
	 * so the read will work.  However, the data should be zero
	 * rather than '2' as the old data was thrown away when the
	 * object was shrunk and the new pages when an object are
	 * grown are zero-filled.
	 */
	if (page[getpagesize()] != 0) {
		fail_err("invalid data at %d", getpagesize());
		close(fd);
		return;
	}

	close(fd);
	pass();
}
TEST(test_object_resize, "object resize");

int
main(int argc, char *argv[])
{

	run_tests();
	return (0);
}
