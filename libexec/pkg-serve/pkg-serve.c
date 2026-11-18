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
#define MAXLINE 4096

static void
usage(void)
{
	fprintf(stderr, "usage: pkg-serve basedir\n");
	exit(EXIT_FAILURE);
}

static ssize_t
readcmd(char *buf, size_t size, FILE *fp)
{
	size_t len;
	int c;

	len = 0;
	while ((c = fgetc(fp)) != EOF && c != '\n') {
		if (len + 1 >= size) {
			/* Line too long: discard the rest of it. */
			while ((c = fgetc(fp)) != EOF && c != '\n')
				;
			buf[0] = '\0';
			return (-2);
		}
		buf[len++] = (char)c;
	}

	if (c == EOF && len == 0)
		return (-1);

	buf[len] = '\0';
	return ((ssize_t)len);
}

int
main(int argc, char *argv[])
{
	struct stat st;
	cap_rights_t rights, ffdrights;
	char line[MAXLINE];
	char *file, *age;
	size_t toread;
	ssize_t r, linelen;
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

	cap_rights_init(&rights, CAP_READ, CAP_FSTAT, CAP_LOOKUP);
	if (cap_rights_limit(fd, &rights) < 0 && errno != ENOSYS)
		err(EXIT_FAILURE, "cap_rights_limit");

	if (cap_enter() < 0 && errno != ENOSYS)
		err(EXIT_FAILURE, "cap_enter");

	printf("ok: pkg-serve " VERSION "\n");
	fflush(stdout);

	while ((linelen = readcmd(line, sizeof(line), stdin)) != -1) {
		if (linelen == -2) {
			printf("ko: command too long\n");
			fflush(stdout);
			continue;
		}

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

		ffd = openat(fd, file,
		    O_RDONLY | O_NONBLOCK | O_RESOLVE_BENEATH | O_CLOEXEC);
		if (ffd == -1) {
			printf("ko: file not found\n");
			fflush(stdout);
			continue;
		}

		if (fstat(ffd, &st) == -1) {
			close(ffd);
			printf("ko: file not found\n");
			fflush(stdout);
			continue;
		}

		if (!S_ISREG(st.st_mode)) {
			close(ffd);
			printf("ko: file not found\n");
			fflush(stdout);
			continue;
		}

		if (st.st_mtime <= mtime) {
			close(ffd);
			printf("ok: 0\n");
			fflush(stdout);
			continue;
		}

		/* Only reading is needed from now on. */
		cap_rights_init(&ffdrights, CAP_READ);
		if (cap_rights_limit(ffd, &ffdrights) < 0 && errno != ENOSYS) {
			close(ffd);
			err(EXIT_FAILURE, "cap_rights_limit");
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
			if (fwrite(buf, 1, (size_t)r, stdout) != (size_t)r)
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
