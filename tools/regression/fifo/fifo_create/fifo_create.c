/*-
 * Copyright (c) 2005 Robert N. M. Watson
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
 *
 * $FreeBSD$
 */

#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Simple regression test for the creation and destruction of POSIX fifos in
 * the file system name space.  Using a specially created directory, create
 * a fifo in it and check that the following properties are present, as
 * specified in IEEE Std 1003.1, 2004 Edition:
 *
 * - When mkfifo() is called, on success, a fifo is created.
 *
 * - On an error, no fifo is created. (XXX: Not tested)
 *
 * - The mode bits on the fifo are a product of combining the umask and
 *   requested mode.
 *
 * - The fifo's owner will be the processes effective user ID. (XXX: Not
 *   tested)
 *
 * - The fifo's group will be the parent directory's group or the effective
 *   group ID of the process.  For historical reasons, BSD prefers the group
 *   ID of the process, so we will generate an error if it's not that. (XXX:
 *   Not tested)
 *
 * - The st_atime, st_ctime, st_mtime of the fifo will be set appropriately,
 *   and st_ctime and st_mtime on the directory will be updated. (XXX: We
 *   test they are updated, not correct)
 *
 * - EEXIST is returned if the named file already exists.
 *
 * In addition, we check that we can unlink the fifo, and that if we do, it
 * disappears.
 *
 * This test must run as root in order to usefully frob the process
 * credential to test permission parts.
 */

/*
 * All activity occurs within a temporary directory created early in the
 * test.
 */
char	temp_dir[PATH_MAX];

static void __unused
atexit_temp_dir(void)
{

	rmdir(temp_dir);
}

/*
 * Basic creation tests: verify that mkfifo() creates a fifo, that the time
 * stamps on the directory are updated, that if we try twice we get EEXIST,
 * and that we can unlink it.
 */
static void
fifo_create_test(void)
{
	struct stat old_dirsb, dirsb, fifosb;
	char path[PATH_MAX];
	int error;

	/*
	 * Sleep to make sure that the time stamp on the directory will be
	 * updated.
	 */
	sleep(2);

	if (stat(temp_dir, &old_dirsb) < 0)
		err(-1, "basic_create_test: stat: %s", temp_dir);

	snprintf(path, PATH_MAX, "%s/testfifo", temp_dir);

	if (mkfifo(path, 0600) < 0)
		err(-1, "basic_create_test: mkfifo: %s", path);

	if (stat(path, &fifosb) < 0) {
		error = errno;
		(void)unlink(path);
		errno = error;
		err(-1, "basic_create_test: stat: %s", path);
	}

	if (!(S_ISFIFO(fifosb.st_mode))) {
		(void)unlink(path);
		errx(-1, "basic_create_test: mkfifo produced non-fifo");
	}

	if (mkfifo(path, 0600) == 0)
		errx(-1, "basic_create_test: dup mkfifo succeeded");

	if (errno != EEXIST)
		err(-1, "basic_create_test: dup mkfifo unexpected error");

	if (stat(temp_dir, &dirsb) < 0) {
		error = errno;
		(void)unlink(path);
		errno = error;
		err(-1, "basic_create_test: stat: %s", temp_dir);
	}

	if (old_dirsb.st_ctime == dirsb.st_ctime) {
		(void)unlink(path);
		errx(-1, "basic_create_test: old_dirsb.st_ctime == "
		    "dirsb.st_ctime");
	}

	if (old_dirsb.st_mtime == dirsb.st_mtime) {
		(void)unlink(path);
		errx(-1, "basic_create_test: old_dirsb.st_mtime == "
		    "dirsb.st_mtime");
	}

	if (unlink(path) < 0)
		err(-1, "basic_create_test: unlink: %s", path);

	if (stat(path, &fifosb) == 0)
		errx(-1, "basic_create_test: unlink failed to unlink");
	if (errno != ENOENT)
		err(-1, "basic_create_test: unlink unexpected error");
}

/*
 * Having determined that basic create/remove/etc functionality is present
 * for fifos, now make sure that the umask, requested permissions, and
 * resulting mode are handled properly.
 */
static const struct permission_test {
	mode_t	pt_umask;
	mode_t	pt_reqmode;
	mode_t	pt_mode;
} permission_test[] = {
	{0000, 0, S_IFIFO},
	{0000, S_IRWXU, S_IFIFO | S_IRWXU},
	{0000, S_IRWXU | S_IRWXG | S_IRWXO, S_IFIFO | S_IRWXU | S_IRWXG |
	    S_IRWXO },
	{0077, S_IRWXU, S_IFIFO | S_IRWXU},
	{0077, S_IRWXU | S_IRWXG | S_IRWXO, S_IFIFO | S_IRWXU},
};
static const int permission_test_count = sizeof(permission_test) /
    sizeof(struct permission_test);

static void
fifo_permission_test(void)
{
	const struct permission_test *ptp;
	mode_t __unused old_umask;
	char path[PATH_MAX];
	struct stat sb;
	int error, i;

	snprintf(path, PATH_MAX, "%s/testfifo", temp_dir);
	old_umask = umask(0022);
	for (i = 0; i < permission_test_count; i++) {
		ptp = &permission_test[i];

		umask(ptp->pt_umask);
		if (mkfifo(path, ptp->pt_reqmode) < 0)
			err(-1, "fifo_permission_test: %08o %08o %08o\n",
			    ptp->pt_umask, ptp->pt_reqmode, ptp->pt_mode);

		if (stat(path, &sb) < 0) {
			error = errno;
			(void)unlink(path);
			errno = error;
			err(-1, "fifo_permission_test: %s", path);
		}

		if (sb.st_mode != ptp->pt_mode) {
			(void)unlink(path);
			errx(-1, "fifo_permission_test: %08o %08o %08o "
			    "got %08o", ptp->pt_umask, ptp->pt_reqmode,
			    ptp->pt_mode, sb.st_mode);
		}

		if (unlink(path) < 0)
			err(-1, "fifo_permission_test: unlink: %s", path);
	}
	umask(old_umask);
}

int
main(int argc, char *argv[])
{

	if (geteuid() != 0)
		errx(-1, "must be run as root");

	strcpy(temp_dir, "/tmp/fifo_create.XXXXXXXXXXX");
	if (mkdtemp(temp_dir) == NULL)
		err(-1, "mkdtemp");
	atexit(atexit_temp_dir);

	if (chdir(temp_dir) < 0)
		err(-1, "chdir");

	fifo_create_test();

	fifo_permission_test();

	return (0);
}
