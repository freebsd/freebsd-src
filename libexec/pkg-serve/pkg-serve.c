/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Baptiste Daroussin <bapt@FreeBSD.org>
 */

/*
 * Speaks the same protocol as "pkg ssh" (see pkg-ssh(8)):
 *   -> ok: pkg-serve <version>
 *   <- get <file> <mtime>
 *   -> ok: <size>\n<data>   or   ok: 0\n   or   ko: <error>\n
 *   <- quit
 */

#include <sys/capsicum.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define VERSION "0.1"
#define BUFSZ 32768

static void
usage(void)
{
	fprintf(stderr, "usage: pkg-serve basedir\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	struct stat st;
	cap_rights_t rights;
	char *line = NULL;
	char *file, *age;
	size_t linecap = 0, r, toread;
	ssize_t linelen;
	off_t remaining;
	time_t mtime;
	char *end;
	int fd, ffd;
	char buf[BUFSZ];
	const char *basedir;

	if (argc != 2)
		usage();

	basedir = argv[1];

	if ((fd = open(basedir, O_DIRECTORY | O_RDONLY | O_CLOEXEC)) < 0)
		err(EXIT_FAILURE, "open(%s)", basedir);

	cap_rights_init(&rights, CAP_READ, CAP_FSTATAT, CAP_LOOKUP,
	    CAP_FCNTL);
	if (cap_rights_limit(fd, &rights) < 0 && errno != ENOSYS)
		err(EXIT_FAILURE, "cap_rights_limit");

	if (cap_enter() < 0 && errno != ENOSYS)
		err(EXIT_FAILURE, "cap_enter");

	printf("ok: pkg-serve " VERSION "\n");
	fflush(stdout);

	while ((linelen = getline(&line, &linecap, stdin)) > 0) {
		/* trim newline */
		if (linelen > 0 && line[linelen - 1] == '\n')
			line[--linelen] = '\0';

		if (linelen == 0)
			continue;

		if (strcmp(line, "quit") == 0)
			break;

		if (strncmp(line, "get ", 4) != 0) {
			printf("ko: unknown command '%s'\n", line);
			fflush(stdout);
			continue;
		}

		file = line + 4;

		if (*file == '\0') {
			printf("ko: bad command get, expecting 'get file age'\n");
			fflush(stdout);
			continue;
		}

		/* skip leading slash */
		if (*file == '/')
			file++;

		/* find the age argument */
		age = file;
		while (*age != '\0' && !isspace((unsigned char)*age))
			age++;

		if (*age == '\0') {
			printf("ko: bad command get, expecting 'get file age'\n");
			fflush(stdout);
			continue;
		}

		*age++ = '\0';

		/* skip whitespace */
		while (isspace((unsigned char)*age))
			age++;

		if (*age == '\0') {
			printf("ko: bad command get, expecting 'get file age'\n");
			fflush(stdout);
			continue;
		}

		errno = 0;
		mtime = (time_t)strtoimax(age, &end, 10);
		if (errno != 0 || *end != '\0' || end == age) {
			printf("ko: bad number %s\n", age);
			fflush(stdout);
			continue;
		}

		if (fstatat(fd, file, &st, AT_RESOLVE_BENEATH) == -1) {
			printf("ko: file not found\n");
			fflush(stdout);
			continue;
		}

		if (!S_ISREG(st.st_mode)) {
			printf("ko: not a file\n");
			fflush(stdout);
			continue;
		}

		if (st.st_mtime <= mtime) {
			printf("ok: 0\n");
			fflush(stdout);
			continue;
		}

		if ((ffd = openat(fd, file, O_RDONLY | O_RESOLVE_BENEATH)) == -1) {
			printf("ko: file not found\n");
			fflush(stdout);
			continue;
		}

		printf("ok: %" PRIdMAX "\n", (intmax_t)st.st_size);
		fflush(stdout);

		remaining = st.st_size;
		while (remaining > 0) {
			toread = sizeof(buf);
			if ((off_t)toread > remaining)
				toread = (size_t)remaining;
			r = read(ffd, buf, toread);
			if (r <= 0)
				break;
			if (fwrite(buf, 1, r, stdout) != r)
				break;
			remaining -= r;
		}
		close(ffd);
		if (remaining > 0)
			errx(EXIT_FAILURE, "%s: file truncated during transfer",
			    file);
		fflush(stdout);
	}

	return (EXIT_SUCCESS);
}
