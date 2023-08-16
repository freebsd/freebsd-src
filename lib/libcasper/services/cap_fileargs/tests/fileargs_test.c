/*-
 * Copyright (c) 2021 Mariusz Zaborski <oshogbo@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#include <atf-c.h>

#include <libcasper.h>
#include <casper/cap_fileargs.h>

#include "freebsd_test_suite/macros.h"

#define MAX_FILES		200

static char *files[MAX_FILES];
static int fds[MAX_FILES];

#define	TEST_FILE	"/etc/passwd"

static void
check_capsicum(void)
{
	ATF_REQUIRE_FEATURE("security_capabilities");
	ATF_REQUIRE_FEATURE("security_capability_mode");
}

static void
prepare_files(size_t num, bool create)
{
	const char template[] = "testsfiles.XXXXXXXX";
	size_t i;

	for (i = 0; i < num; i++) {
		files[i] = calloc(1, sizeof(template));
		ATF_REQUIRE(files[i] != NULL);
		strncpy(files[i], template, sizeof(template) - 1);

		if (create) {
			fds[i] = mkstemp(files[i]);
			ATF_REQUIRE(fds[i] >= 0);
		} else {
			fds[i] = -1;
			ATF_REQUIRE(mktemp(files[i]) != NULL);
		}
	}
}

static void
clear_files(void)
{
	size_t i;


	for (i = 0; files[i] != NULL; i++) {
		unlink(files[i]);
		free(files[i]);
		if (fds[i] != -1)
			close(fds[i]);
	}
}

static int
test_file_open(fileargs_t *fa, const char *file, int *fdp)
{
	int fd;

	fd = fileargs_open(fa, file);
	if (fd < 0)
		return (errno);

	if (fdp != NULL) {
		*fdp = fd;
	}

	return (0);
}

static int
test_file_fopen(fileargs_t *fa, const char *file, const char *mode,
    FILE **retfile)
{
	FILE *pfile;

	pfile = fileargs_fopen(fa, file, mode);
	if (pfile == NULL)
		return (errno);

	if (retfile != NULL) {
		*retfile = pfile;
	}

	return (0);
}

static int
test_file_lstat(fileargs_t *fa, const char *file)
{
	struct stat fasb, origsb;
	bool equals;

	if (fileargs_lstat(fa, file, &fasb) < 0)
		return (errno);

	ATF_REQUIRE(lstat(file, &origsb) == 0);

	equals = true;
	equals &= (origsb.st_dev == fasb.st_dev);
	equals &= (origsb.st_ino == fasb.st_ino);
	equals &= (origsb.st_nlink == fasb.st_nlink);
	equals &= (origsb.st_flags == fasb.st_flags);
	equals &= (memcmp(&origsb.st_ctim, &fasb.st_ctim,
	    sizeof(fasb.st_ctim)) == 0);
	equals &= (memcmp(&origsb.st_birthtim, &fasb.st_birthtim,
	    sizeof(fasb.st_birthtim)) == 0);
	if (!equals) {
		return (EINVAL);
	}

	return (0);
}

static int
test_file_realpath_static(fileargs_t *fa, const char *file)
{
	char fapath[PATH_MAX], origpath[PATH_MAX];

	if (fileargs_realpath(fa, file, fapath) == NULL)
		return (errno);

	ATF_REQUIRE(realpath(file, origpath) != NULL);

	if (strcmp(fapath, origpath) != 0)
		return (EINVAL);

	return (0);
}

static int
test_file_realpath_alloc(fileargs_t *fa, const char *file)
{
	char *fapath, *origpath;
	int serrno;

	fapath = fileargs_realpath(fa, file, NULL);
	if (fapath == NULL)
		return (errno);

	origpath = realpath(file, NULL);
	ATF_REQUIRE(origpath != NULL);

	serrno = 0;
	if (strcmp(fapath, origpath) != 0)
		serrno = EINVAL;

	free(fapath);
	free(origpath);

	return (serrno);
}

static int
test_file_realpath(fileargs_t *fa, const char *file)
{
	int serrno;

	serrno = test_file_realpath_static(fa, file);
	if (serrno != 0)
		return serrno;

	return (test_file_realpath_alloc(fa, file));
}

static int
test_file_mode(int fd, int mode)
{
	int flags;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		return (errno);

	if ((flags & O_ACCMODE) != mode)
		return (errno);

	return (0);
}

static bool
test_file_cap(int fd, cap_rights_t *rights)
{
	cap_rights_t fdrights;

	ATF_REQUIRE(cap_rights_get(fd, &fdrights) == 0);

	return (cap_rights_contains(&fdrights, rights));
}

static int
test_file_write(int fd)
{
	char buf;

	buf = 't';
	if (write(fd, &buf, sizeof(buf)) != sizeof(buf)) {
		return (errno);
	}

	return (0);
}

static int
test_file_read(int fd)
{
	char buf;

	if (read(fd, &buf, sizeof(buf)) < 0) {
		return (errno);
	}

	return (0);
}

static int
test_file_fwrite(FILE *pfile)
{
	char buf;

	buf = 't';
	if (fwrite(&buf, sizeof(buf), 1, pfile) != sizeof(buf))
		return (errno);

	return (0);
}

static int
test_file_fread(FILE *pfile)
{
	char buf;
	int ret, serrno;

	errno = 0;
	ret = fread(&buf, sizeof(buf), 1, pfile);
	serrno = errno;
	if (ret < 0) {
		return (serrno);
	} else if (ret == 0 && feof(pfile) == 0) {
		return (serrno != 0 ? serrno : EINVAL);
	}

	return (0);
}

ATF_TC_WITH_CLEANUP(fileargs__open_read);
ATF_TC_HEAD(fileargs__open_read, tc) {}
ATF_TC_BODY(fileargs__open_read, tc)
{
	cap_rights_t rights, norights;
	fileargs_t *fa;
	size_t i;
	int fd;

	check_capsicum();

	prepare_files(MAX_FILES, true);

	cap_rights_init(&rights, CAP_READ | CAP_FCNTL);
	cap_rights_init(&norights, CAP_WRITE);
	fa = fileargs_init(MAX_FILES, files, O_RDONLY, 0, &rights,
	    FA_OPEN);
	ATF_REQUIRE(fa != NULL);

	for (i = 0; i < MAX_FILES; i++) {
		/* ALLOWED */
		/* We open file twice to check if we can. */
		ATF_REQUIRE(test_file_open(fa, files[i], &fd) == 0);
		ATF_REQUIRE(close(fd) == 0);

		ATF_REQUIRE(test_file_open(fa, files[i], &fd) == 0);
		ATF_REQUIRE(test_file_mode(fd, O_RDONLY) == 0);
		ATF_REQUIRE(test_file_cap(fd, &rights) == true);
		ATF_REQUIRE(test_file_read(fd) == 0);

		/* DISALLOWED */
		ATF_REQUIRE(test_file_lstat(fa, files[i]) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_open(fa, TEST_FILE, NULL) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_cap(fd, &norights) == false);
		ATF_REQUIRE(test_file_write(fd) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_realpath(fa, files[i]) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_realpath(fa, TEST_FILE) == ENOTCAPABLE);

		/* CLOSE */
		ATF_REQUIRE(close(fd) == 0);
	}
}
ATF_TC_CLEANUP(fileargs__open_read, tc)
{
	clear_files();
}

ATF_TC_WITH_CLEANUP(fileargs__open_write);
ATF_TC_HEAD(fileargs__open_write, tc) {}
ATF_TC_BODY(fileargs__open_write, tc)
{
	cap_rights_t rights, norights;
	fileargs_t *fa;
	size_t i;
	int fd;

	check_capsicum();

	prepare_files(MAX_FILES, true);

	cap_rights_init(&rights, CAP_WRITE | CAP_FCNTL);
	cap_rights_init(&norights, CAP_READ);
	fa = fileargs_init(MAX_FILES, files, O_WRONLY, 0, &rights,
	    FA_OPEN);
	ATF_REQUIRE(fa != NULL);

	for (i = 0; i < MAX_FILES; i++) {
		/* ALLOWED */
		/* We open file twice to check if we can. */
		ATF_REQUIRE(test_file_open(fa, files[i], &fd) == 0);
		ATF_REQUIRE(close(fd) == 0);

		ATF_REQUIRE(test_file_open(fa, files[i], &fd) == 0);
		ATF_REQUIRE(test_file_mode(fd, O_WRONLY) == 0);
		ATF_REQUIRE(test_file_cap(fd, &rights) == true);
		ATF_REQUIRE(test_file_write(fd) == 0);

		/* DISALLOWED */
		ATF_REQUIRE(test_file_lstat(fa, files[i]) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_open(fa, TEST_FILE, NULL) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_cap(fd, &norights) == false);
		ATF_REQUIRE(test_file_read(fd) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_realpath(fa, files[i]) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_realpath(fa, TEST_FILE) == ENOTCAPABLE);

		/* CLOSE */
		ATF_REQUIRE(close(fd) == 0);
	}
}
ATF_TC_CLEANUP(fileargs__open_write, tc)
{
	clear_files();
}

ATF_TC_WITH_CLEANUP(fileargs__open_create);
ATF_TC_HEAD(fileargs__open_create, tc) {}
ATF_TC_BODY(fileargs__open_create, tc)
{
	cap_rights_t rights, norights;
	fileargs_t *fa;
	size_t i;
	int fd;

	check_capsicum();

	prepare_files(MAX_FILES, false);

	cap_rights_init(&rights, CAP_WRITE | CAP_FCNTL | CAP_READ);
	cap_rights_init(&norights, CAP_FCHMOD);
	fa = fileargs_init(MAX_FILES, files, O_RDWR | O_CREAT, 666,
	    &rights, FA_OPEN);
	ATF_REQUIRE(fa != NULL);

	for (i = 0; i < MAX_FILES; i++) {
		/* ALLOWED */
		ATF_REQUIRE(test_file_open(fa, files[i], &fd) == 0);

		ATF_REQUIRE(test_file_mode(fd, O_RDWR) == 0);
		ATF_REQUIRE(test_file_cap(fd, &rights) == true);
		ATF_REQUIRE(test_file_write(fd) == 0);
		ATF_REQUIRE(test_file_read(fd) == 0);

		/* DISALLOWED */
		ATF_REQUIRE(test_file_lstat(fa, files[i]) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_open(fa, TEST_FILE, NULL) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_cap(fd, &norights) == false);
		ATF_REQUIRE(test_file_realpath(fa, files[i]) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_realpath(fa, TEST_FILE) == ENOTCAPABLE);

		/* CLOSE */
		ATF_REQUIRE(close(fd) == 0);
	}
}
ATF_TC_CLEANUP(fileargs__open_create, tc)
{
	clear_files();
}

ATF_TC_WITH_CLEANUP(fileargs__open_with_casper);
ATF_TC_HEAD(fileargs__open_with_casper, tc) {}
ATF_TC_BODY(fileargs__open_with_casper, tc)
{
	cap_channel_t *capcas;
	cap_rights_t rights;
	fileargs_t *fa;
	size_t i;
	int fd;

	check_capsicum();

	prepare_files(MAX_FILES, true);

	capcas = cap_init();
	ATF_REQUIRE(capcas != NULL);

	cap_rights_init(&rights, CAP_READ);
	fa = fileargs_cinit(capcas, MAX_FILES, files, O_RDONLY, 0, &rights,
	    FA_OPEN);
	ATF_REQUIRE(fa != NULL);

	for (i = 0; i < MAX_FILES; i++) {
		/* ALLOWED */
		ATF_REQUIRE(test_file_open(fa, files[i], &fd) == 0);
		ATF_REQUIRE(test_file_read(fd) == 0);

		/* CLOSE */
		ATF_REQUIRE(close(fd) == 0);
	}
}
ATF_TC_CLEANUP(fileargs__open_with_casper, tc)
{
	clear_files();
}

ATF_TC_WITH_CLEANUP(fileargs__fopen_read);
ATF_TC_HEAD(fileargs__fopen_read, tc) {}
ATF_TC_BODY(fileargs__fopen_read, tc)
{
	cap_rights_t rights, norights;
	fileargs_t *fa;
	size_t i;
	FILE *pfile;
	int fd;

	check_capsicum();

	prepare_files(MAX_FILES, true);

	cap_rights_init(&rights, CAP_READ | CAP_FCNTL);
	cap_rights_init(&norights, CAP_WRITE);
	fa = fileargs_init(MAX_FILES, files, O_RDONLY, 0, &rights,
	    FA_OPEN);
	ATF_REQUIRE(fa != NULL);

	for (i = 0; i < MAX_FILES; i++) {
		/* ALLOWED */
		/* We fopen file twice to check if we can. */
		ATF_REQUIRE(test_file_fopen(fa, files[i], "r", &pfile) == 0);
		ATF_REQUIRE(fclose(pfile) == 0);

		ATF_REQUIRE(test_file_fopen(fa, files[i], "r", &pfile) == 0);
		fd = fileno(pfile);
		ATF_REQUIRE(test_file_mode(fd, O_RDONLY) == 0);
		ATF_REQUIRE(test_file_cap(fd, &rights) == true);
		ATF_REQUIRE(test_file_fread(pfile) == 0);

		/* DISALLOWED */
		ATF_REQUIRE(test_file_lstat(fa, files[i]) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_fopen(fa, TEST_FILE, "r", NULL) ==
		    ENOTCAPABLE);
		ATF_REQUIRE(test_file_cap(fd, &norights) == false);
		ATF_REQUIRE(test_file_fwrite(pfile) == EBADF);
		ATF_REQUIRE(test_file_realpath(fa, files[i]) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_realpath(fa, TEST_FILE) == ENOTCAPABLE);

		/* CLOSE */
		ATF_REQUIRE(fclose(pfile) == 0);
	}
}
ATF_TC_CLEANUP(fileargs__fopen_read, tc)
{
	clear_files();
}

ATF_TC_WITH_CLEANUP(fileargs__fopen_write);
ATF_TC_HEAD(fileargs__fopen_write, tc) {}
ATF_TC_BODY(fileargs__fopen_write, tc)
{
	cap_rights_t rights, norights;
	fileargs_t *fa;
	size_t i;
	FILE *pfile;
	int fd;

	check_capsicum();

	prepare_files(MAX_FILES, true);

	cap_rights_init(&rights, CAP_WRITE | CAP_FCNTL);
	cap_rights_init(&norights, CAP_READ);
	fa = fileargs_init(MAX_FILES, files, O_WRONLY, 0, &rights,
	    FA_OPEN);
	ATF_REQUIRE(fa != NULL);

	for (i = 0; i < MAX_FILES; i++) {
		/* ALLOWED */
		/* We fopen file twice to check if we can. */
		ATF_REQUIRE(test_file_fopen(fa, files[i], "w", &pfile) == 0);
		ATF_REQUIRE(fclose(pfile) == 0);

		ATF_REQUIRE(test_file_fopen(fa, files[i], "w", &pfile) == 0);
		fd = fileno(pfile);
		ATF_REQUIRE(test_file_mode(fd, O_WRONLY) == 0);
		ATF_REQUIRE(test_file_cap(fd, &rights) == true);
		ATF_REQUIRE(test_file_fwrite(pfile) == 0);

		/* DISALLOWED */
		ATF_REQUIRE(test_file_lstat(fa, files[i]) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_fopen(fa, TEST_FILE, "w", NULL) ==
		    ENOTCAPABLE);
		ATF_REQUIRE(test_file_cap(fd, &norights) == false);
		ATF_REQUIRE(test_file_fread(pfile) == EBADF);
		ATF_REQUIRE(test_file_realpath(fa, files[i]) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_realpath(fa, TEST_FILE) == ENOTCAPABLE);

		/* CLOSE */
		ATF_REQUIRE(fclose(pfile) == 0);
	}
}
ATF_TC_CLEANUP(fileargs__fopen_write, tc)
{
	clear_files();
}

ATF_TC_WITH_CLEANUP(fileargs__fopen_create);
ATF_TC_HEAD(fileargs__fopen_create, tc) {}
ATF_TC_BODY(fileargs__fopen_create, tc)
{
	cap_rights_t rights;
	fileargs_t *fa;
	size_t i;
	FILE *pfile;
	int fd;

	check_capsicum();

	prepare_files(MAX_FILES, false);

	cap_rights_init(&rights, CAP_READ | CAP_WRITE | CAP_FCNTL);
	fa = fileargs_init(MAX_FILES, files, O_RDWR | O_CREAT, 0, &rights,
	    FA_OPEN);
	ATF_REQUIRE(fa != NULL);

	for (i = 0; i < MAX_FILES; i++) {
		/* ALLOWED */
		/* We fopen file twice to check if we can. */
		ATF_REQUIRE(test_file_fopen(fa, files[i], "w+", &pfile) == 0);
		fd = fileno(pfile);
		ATF_REQUIRE(test_file_mode(fd, O_RDWR) == 0);
		ATF_REQUIRE(test_file_cap(fd, &rights) == true);
		ATF_REQUIRE(test_file_fwrite(pfile) == 0);
		ATF_REQUIRE(test_file_fread(pfile) == 0);

		/* DISALLOWED */
		ATF_REQUIRE(test_file_lstat(fa, files[i]) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_fopen(fa, TEST_FILE, "w+", NULL) ==
		    ENOTCAPABLE);
		ATF_REQUIRE(test_file_realpath(fa, files[i]) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_realpath(fa, TEST_FILE) == ENOTCAPABLE);

		/* CLOSE */
		ATF_REQUIRE(fclose(pfile) == 0);
	}
}
ATF_TC_CLEANUP(fileargs__fopen_create, tc)
{
	clear_files();
}

ATF_TC_WITH_CLEANUP(fileargs__lstat);
ATF_TC_HEAD(fileargs__lstat, tc) {}
ATF_TC_BODY(fileargs__lstat, tc)
{
	fileargs_t *fa;
	size_t i;
	int fd;

	check_capsicum();

	prepare_files(MAX_FILES, true);

	fa = fileargs_init(MAX_FILES, files, 0, 0, NULL, FA_LSTAT);
	ATF_REQUIRE(fa != NULL);

	for (i = 0; i < MAX_FILES; i++) {
		/* ALLOWED */
		ATF_REQUIRE(test_file_lstat(fa, files[i]) == 0);

		/* DISALLOWED */
		ATF_REQUIRE(test_file_open(fa, files[i], &fd) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_lstat(fa, TEST_FILE) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_open(fa, TEST_FILE, &fd) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_realpath(fa, files[i]) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_realpath(fa, TEST_FILE) == ENOTCAPABLE);
	}
}
ATF_TC_CLEANUP(fileargs__lstat, tc)
{
	clear_files();
}

ATF_TC_WITH_CLEANUP(fileargs__realpath);
ATF_TC_HEAD(fileargs__realpath, tc) {}
ATF_TC_BODY(fileargs__realpath, tc)
{
	fileargs_t *fa;
	size_t i;
	int fd;

	prepare_files(MAX_FILES, true);

	fa = fileargs_init(MAX_FILES, files, 0, 0, NULL, FA_REALPATH);
	ATF_REQUIRE(fa != NULL);

	for (i = 0; i < MAX_FILES; i++) {
		/* ALLOWED */
		ATF_REQUIRE(test_file_realpath(fa, files[i]) == 0);

		/* DISALLOWED */
		ATF_REQUIRE(test_file_open(fa, files[i], &fd) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_lstat(fa, files[i]) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_lstat(fa, TEST_FILE) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_open(fa, TEST_FILE, &fd) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_realpath(fa, TEST_FILE) == ENOTCAPABLE);
	}
}
ATF_TC_CLEANUP(fileargs__realpath, tc)
{
	clear_files();
}

ATF_TC_WITH_CLEANUP(fileargs__open_lstat);
ATF_TC_HEAD(fileargs__open_lstat, tc) {}
ATF_TC_BODY(fileargs__open_lstat, tc)
{
	cap_rights_t rights, norights;
	fileargs_t *fa;
	size_t i;
	int fd;

	check_capsicum();

	prepare_files(MAX_FILES, true);

	cap_rights_init(&rights, CAP_READ | CAP_FCNTL);
	cap_rights_init(&norights, CAP_WRITE);
	fa = fileargs_init(MAX_FILES, files, O_RDONLY, 0, &rights,
	    FA_OPEN | FA_LSTAT);
	ATF_REQUIRE(fa != NULL);

	for (i = 0; i < MAX_FILES; i++) {
		/* ALLOWED */
		/* We open file twice to check if we can. */
		ATF_REQUIRE(test_file_lstat(fa, files[i]) == 0);
		ATF_REQUIRE(test_file_open(fa, files[i], &fd) == 0);
		ATF_REQUIRE(close(fd) == 0);

		ATF_REQUIRE(test_file_open(fa, files[i], &fd) == 0);
		ATF_REQUIRE(test_file_lstat(fa, files[i]) == 0);
		ATF_REQUIRE(test_file_mode(fd, O_RDONLY) == 0);
		ATF_REQUIRE(test_file_cap(fd, &rights) == true);
		ATF_REQUIRE(test_file_read(fd) == 0);

		/* DISALLOWED */
		ATF_REQUIRE(test_file_open(fa, TEST_FILE, NULL) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_cap(fd, &norights) == false);
		ATF_REQUIRE(test_file_write(fd) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_realpath(fa, files[i]) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_realpath(fa, TEST_FILE) == ENOTCAPABLE);

		/* CLOSE */
		ATF_REQUIRE(close(fd) == 0);
	}
}
ATF_TC_CLEANUP(fileargs__open_lstat, tc)
{
	clear_files();
}

ATF_TC_WITH_CLEANUP(fileargs__open_realpath);
ATF_TC_HEAD(fileargs__open_realpath, tc) {}
ATF_TC_BODY(fileargs__open_realpath, tc)
{
	cap_rights_t rights, norights;
	fileargs_t *fa;
	size_t i;
	int fd;

	check_capsicum();

	prepare_files(MAX_FILES, true);

	cap_rights_init(&rights, CAP_READ | CAP_FCNTL);
	cap_rights_init(&norights, CAP_WRITE);
	fa = fileargs_init(MAX_FILES, files, O_RDONLY, 0, &rights,
	    FA_OPEN | FA_REALPATH);
	ATF_REQUIRE(fa != NULL);

	for (i = 0; i < MAX_FILES; i++) {
		/* ALLOWED */
		/* We open file twice to check if we can. */
		ATF_REQUIRE(test_file_realpath(fa, files[i]) == 0);
		ATF_REQUIRE(test_file_open(fa, files[i], &fd) == 0);
		ATF_REQUIRE(close(fd) == 0);

		ATF_REQUIRE(test_file_open(fa, files[i], &fd) == 0);
		ATF_REQUIRE(test_file_realpath(fa, files[i]) == 0);
		ATF_REQUIRE(test_file_mode(fd, O_RDONLY) == 0);
		ATF_REQUIRE(test_file_cap(fd, &rights) == true);
		ATF_REQUIRE(test_file_read(fd) == 0);

		/* DISALLOWED */
		ATF_REQUIRE(test_file_open(fa, TEST_FILE, NULL) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_cap(fd, &norights) == false);
		ATF_REQUIRE(test_file_write(fd) == ENOTCAPABLE);
		ATF_REQUIRE(test_file_lstat(fa, files[i]) == ENOTCAPABLE);

		/* CLOSE */
		ATF_REQUIRE(close(fd) == 0);
	}
}
ATF_TC_CLEANUP(fileargs__open_realpath, tc)
{
	clear_files();
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, fileargs__open_create);
	ATF_TP_ADD_TC(tp, fileargs__open_read);
	ATF_TP_ADD_TC(tp, fileargs__open_write);
	ATF_TP_ADD_TC(tp, fileargs__open_with_casper);

	ATF_TP_ADD_TC(tp, fileargs__fopen_create);
	ATF_TP_ADD_TC(tp, fileargs__fopen_read);
	ATF_TP_ADD_TC(tp, fileargs__fopen_write);

	ATF_TP_ADD_TC(tp, fileargs__lstat);

	ATF_TP_ADD_TC(tp, fileargs__realpath);

	ATF_TP_ADD_TC(tp, fileargs__open_lstat);
	ATF_TP_ADD_TC(tp, fileargs__open_realpath);

	return (atf_no_error());
}
