/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Klara, Inc.
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

#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <fcntl.h>
#include <paths.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define PROGNAME	"mktar"

#define SUBDIRNAME	"directory"
#define NORMALFILENAME	"file"
#define SPARSEFILENAME	"sparse_file"
#define HARDLINKNAME	"hard_link"
#define SHORTLINKNAME	"short_link"
#define LONGLINKNAME	"long_link"

static bool opt_g;
static bool opt_v;

static void verbose(const char *fmt, ...)
{
	va_list ap;

	if (!opt_v)
		return;
	fprintf(stderr, "%s: ", PROGNAME);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}

static void
mknormalfile(const char *filename, mode_t mode)
{
	char buf[512];
	ssize_t res;
	int fd;

	if ((fd = open(filename, O_RDWR|O_CREAT|O_EXCL, mode)) < 0)
		err(1, "%s", filename);
	for (unsigned int i = 0; i < sizeof(buf); i++)
		buf[i] = 32 + i % 64;
	res = write(fd, buf, sizeof(buf));
	if (res < 0)
		err(1, "%s", filename);
	if (res != sizeof(buf))
		errx(1, "%s: short write", filename);
	close(fd);
}

static void
mksparsefile(const char *filename, mode_t mode)
{
	char buf[511];
	ssize_t res;
	int fd;

	if ((fd = open(filename, O_RDWR|O_CREAT|O_EXCL, mode)) < 0)
		err(1, "%s", filename);
	for (unsigned int i = 33; i <= 126; i++) {
		memset(buf, i, sizeof(buf));
		if (lseek(fd, 1048576LU * (i - 32), SEEK_SET) < 0)
			err(1, "%s", filename);
		res = write(fd, buf, sizeof(buf));
		if (res < 0)
			err(1, "%s", filename);
		if (res != sizeof(buf))
			errx(1, "%s: short write", filename);
	}
	close(fd);
}

static char *
mklonglinktarget(const char *dirname, const char *filename)
{
	char *piece, *target;

	if (asprintf(&piece, "%1$s/../%1$s/../%1$s/../%1$s/../", dirname) < 0)
		err(1, "asprintf()");
	if (asprintf(&target, "%1$s%1$s%1$s%1$s%1$s%1$s%1$s%1$s%2$s", piece, filename) < 0)
		err(1, "asprintf()");
	free(piece);
	return target;
}

static void
mktar(void)
{
	char *linktarget;

	/* create a subdirectory */
	verbose("mkdir %s", SUBDIRNAME);
	if (mkdir(SUBDIRNAME, 0755) != 0)
		err(1, "%s", SUBDIRNAME);

	/* create a normal file */
	verbose("creating %s", NORMALFILENAME);
	mknormalfile(NORMALFILENAME, 0644);

	/* create a sparse file */
	verbose("creating %s", SPARSEFILENAME);
	mksparsefile(SPARSEFILENAME, 0644);
	chflags(SPARSEFILENAME, UF_NODUMP);

	/* create a hard link */
	verbose("link %s %s", SPARSEFILENAME, HARDLINKNAME);
	if (link(SPARSEFILENAME, HARDLINKNAME) != 0)
		err(1, "%s", HARDLINKNAME);

	/* create a symbolic link with a short target */
	verbose("symlink %s %s", SPARSEFILENAME, SHORTLINKNAME);
	if (symlink(SPARSEFILENAME, SHORTLINKNAME) != 0)
		err(1, "%s", SHORTLINKNAME);

	/* create a symbolic link with a long target */
	linktarget = mklonglinktarget(SUBDIRNAME, SPARSEFILENAME);
	verbose("symlink %s %s", linktarget, LONGLINKNAME);
	if (symlink(linktarget, LONGLINKNAME) != 0)
		err(1, "%s", LONGLINKNAME);
	free(linktarget);
}

static void
usage(void)
{

	fprintf(stderr, "usage: %s [-gv] tarfile\n", PROGNAME);
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	const char *tarfilename;
	char *dirname;
	int opt, wstatus;
	pid_t pid;

	while ((opt = getopt(argc, argv, "gv")) != -1)
		switch (opt) {
		case 'g':
			opt_g = true;
			break;
		case 'v':
			opt_v = true;
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();
	tarfilename = *argv;

	if (asprintf(&dirname, "%s%s.XXXXXXXX", _PATH_TMP, PROGNAME) < 0)
		err(1, "asprintf()");
	if (mkdtemp(dirname) == NULL)
		err(1, "%s", dirname);
	verbose("mkdir %s", dirname);

	/* fork a child to create the files */
	if ((pid = fork()) < 0)
		err(1, "fork()");
	if (pid == 0) {
		verbose("cd %s", dirname);
		if (chdir(dirname) != 0)
			err(1, "%s", dirname);
		verbose("umask 022");
		umask(022);
		mktar();
		verbose("cd -");
		exit(0);
	}
	if (waitpid(pid, &wstatus, 0) < 0)
		err(1, "waitpid()");
	if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0)
		errx(1, "child failed");

	/* fork a child to create the tarball */
	if ((pid = fork()) < 0)
		err(1, "fork()");
	if (pid == 0) {
		verbose("creating tarball");
		execlp(opt_g ? "gtar" : "tar",
		    "tar",
		    "-c",
		    "-f", tarfilename,
		    "-C", dirname,
		    "--posix",
		    "--zstd",
#if 0
		    "--options", "zstd:frame-per-file",
#endif
		    "./" SUBDIRNAME "/../" NORMALFILENAME,
		    "./" SPARSEFILENAME,
		    "./" HARDLINKNAME,
		    "./" SHORTLINKNAME,
		    "./" SUBDIRNAME,
		    "./" LONGLINKNAME,
		    NULL);
		err(1, "execlp()");
	}
	if (waitpid(pid, &wstatus, 0) < 0)
		err(1, "waitpid()");
	if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0)
		errx(1, "child failed");

	/* fork a child to delete everything */
	if ((pid = fork()) < 0)
		err(1, "fork()");
	if (pid == 0) {
		verbose("cd %s", dirname);
		if (chdir(dirname) != 0)
			err(1, "%s", dirname);
		verbose("rm %s", LONGLINKNAME);
		(void)unlink(LONGLINKNAME);
		verbose("rm %s", SHORTLINKNAME);
		(void)unlink(SHORTLINKNAME);
		verbose("rm %s", HARDLINKNAME);
		(void)unlink(HARDLINKNAME);
		verbose("rm %s", SPARSEFILENAME);
		(void)unlink(SPARSEFILENAME);
		verbose("rmdir %s", SUBDIRNAME);
		(void)rmdir(SUBDIRNAME);
		verbose("cd -");
		exit(0);
	}
	if (waitpid(pid, &wstatus, 0) < 0)
		err(1, "waitpid()");
	if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0)
		errx(1, "child failed");
	verbose("rmdir %s", dirname);
	(void)rmdir(dirname);

	exit(0);
}
