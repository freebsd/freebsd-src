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
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Regression test for piddling details of fifos.
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

static void
makefifo(const char *fifoname, const char *testname)
{

	if (mkfifo(fifoname, 0700) < 0)
		err(-1, "%s: makefifo: mkfifo: %s", testname, fifoname);
}

static void
cleanfifo(const char *fifoname, int fd1, int fd2)
{

	if (fd1 != -1)
		close(fd1);
	if (fd2 != -1)
		close(fd2);
	(void)unlink(fifoname);
}

static int
openfifo(const char *fifoname, const char *testname, int *reader_fdp,
    int *writer_fdp)
{
	int error, fd1, fd2;

	fd1 = open(fifoname, O_RDONLY | O_NONBLOCK);
	if (fd1 < 0)
		return (-1);
	fd2 = open(fifoname, O_WRONLY | O_NONBLOCK);
	if (fd2 < 0) {
		error = errno;
		close(fd1);
		errno = error;
		return (-1);
	}
	*reader_fdp = fd1;
	*writer_fdp = fd2;

	return (0);
}

static void
test_lseek(void)
{
	int reader_fd, writer_fd;

	makefifo("testfifo", __func__);

	if (openfifo("testfifo", __func__, &reader_fd, &writer_fd) < 0) {
		warn("%s: openfifo", __func__);
		cleanfifo("testfifo", -1, -1);
		exit(-1);
	}

	if (lseek(reader_fd, SEEK_CUR, 1) >= 0) {
		warnx("%s: lseek succeeded instead of returning ESPIPE",
		    __func__);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}
	if (errno != ESPIPE) {
		warn("%s: lseek returned instead of ESPIPE", __func__);
		cleanfifo("testfifo", reader_fd, writer_fd);
		exit(-1);
	}

	cleanfifo("testfifo", reader_fd, writer_fd);
}

int
main(int argc, char *argv[])
{

	strcpy(temp_dir, "/tmp/fifo_misc.XXXXXXXXXXX");
	if (mkdtemp(temp_dir) == NULL)
		err(-1, "mkdtemp");
	atexit(atexit_temp_dir);

	if (chdir(temp_dir) < 0)
		err(-1, "chdir %s", temp_dir);

	test_lseek();

	return (0);
}
