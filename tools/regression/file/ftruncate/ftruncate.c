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
 *
 * $FreeBSD$
 */

/*
 * Very simple regression test.
 *
 * Future tests that might be of interest:
 *
 * - Make sure that files grown via ftruncate() return 0 bytes for data
 *   reads.
 *
 * - Make sure that we can't ftruncate on a read-only descriptor.
 *
 * - Make sure we get EISDIR on a directory.
 */

#include <sys/types.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>

/*
 * Select various potentially interesting sizes at and around power of 2
 * edges.
 */
static off_t sizes[] = {0, 1, 2, 3, 4, 127, 128, 129, 511, 512, 513, 1023,
    1024, 1025, 2047, 2048, 2049, 4095, 4096, 4097, 8191, 8192, 8193, 16383,
    16384, 16385};
static int size_count = sizeof(sizes) / sizeof(off_t);

int
main(int argc, char *argv[])
{
	char path[PATH_MAX];
	int fd, fds[2], i;
	struct stat sb;

	/*
	 * Tests using a writable temporary file: grow and then shrink a file
	 * using ftruncate and various sizes.  Make sure that a negative file
	 * size is rejected.
	 */
	snprintf(path, PATH_MAX, "/tmp/ftruncate.XXXXXXXXXXXXX");
	fd = mkstemp(path);
	if (fd < 0)
		err(-1, "makestemp");
	(void)unlink(path);

	if (ftruncate(fd, -1) == 0)
		errx(-1, "ftruncate(fd, -1) succeeded");
	if (errno != EINVAL)
		err(-1, "ftruncate(fd, -1) returned wrong error");

	for (i = 0; i < size_count; i++) {
		if (ftruncate(fd, sizes[i]) < 0)
			err(-1, "ftruncate(%llu) up", sizes[i]);
		if (fstat(fd, &sb) < 0)
			err(-1, "stat");
		if (sb.st_size != sizes[i])
			errx(-1, "fstat(%llu) returned %llu up", sizes[i],
			    sb.st_size);
	}

	for (i = size_count - 1; i >= 0; i--) {
		if (ftruncate(fd, sizes[i]) < 0)
			err(-1, "ftruncate(%llu) down", sizes[i]);
		if (fstat(fd, &sb) < 0)
			err(-1, "stat");
		if (sb.st_size != sizes[i])
			errx(-1, "fstat(%llu) returned %llu down", sizes[i],
			    sb.st_size);
	}
	close(fd);

	/*
	 * Make sure that ftruncate on sockets doesn't work.
	 */
	fd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		err(-1, "socket(PF_UNIX, SOCK_STREAM, 0)");
	if (ftruncate(fd, 0) == 0)
		errx(-1, "ftruncate(socket) succeeded");
	if (errno != EINVAL)
		err(-1, "ftruncate(socket) returned wrong error");
	close(fd);

	/*
	 * Make sure that ftruncate on pipes doesn't work.
	 */
	if (pipe(fds) < 0)
		err(-1, "pipe");
	if (ftruncate(fds[0], 0) == 0)
		errx(-1, "ftruncate(pipe) succeeded");
	if (errno != EINVAL)
		err(-1, "ftruncate(pipe) returned wrong error");
	close(fds[0]);
	close(fds[1]);

	/*
	 * Make sure that ftruncate on kqueues doesn't work.
	 */
	fd = kqueue();
	if (fd < 0)
		err(-1, "kqueue");
	if (ftruncate(fds[0], 0) == 0)
		errx(-1, "ftruncate(kqueue) succeeded");
	if (errno != EINVAL)
		err(-1, "ftruncate(kqueue) returned wrong error");
	close(fd);

	return (0);
}
