/*
 * Copyright (c) Christos Zoulas 2021.
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "file.h"

#ifndef lint
FILE_RCSID("@(#)$File: memtest.c,v 1.6 2022/09/24 20:30:13 christos Exp $")
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <magic.h>

void *
malloc(size_t len)
{
	char buf[512];
	void *(*orig)(size_t) = dlsym(RTLD_NEXT, "malloc");
	void *p = (*orig)(len);
	int l = snprintf(buf, sizeof(buf), "malloc %zu %p\n", len, p);
	write(2, buf, l);
	return p;
}

void
free(void *p)
{
	char buf[512];
	void (*orig)(void *) = dlsym(RTLD_NEXT, "free");
	(*orig)(p);
	int l = snprintf(buf, sizeof(buf), "free %p\n", p);
	write(2, buf, l);
}

void *
calloc(size_t len, size_t nitems)
{
	char buf[512];
	void *(*orig)(size_t, size_t) = dlsym(RTLD_NEXT, "calloc");
	void *p = (*orig)(len, nitems);
	size_t tot = len * nitems;
	int l = snprintf(buf, sizeof(buf), "calloc %zu %p\n", tot, p);
	write(2, buf, l);
	return p;
}
void *
realloc(void *q, size_t len)
{
	char buf[512];
	void *(*orig)(void *, size_t) = dlsym(RTLD_NEXT, "realloc");
	void *p = (*orig)(q, len);
	int l = snprintf(buf, sizeof(buf), "realloc %zu %p\n", len, p);
	write(2, buf, l);
	return p;
}

static void
usage(void)
{
	fprintf(stderr, "Usage: test [-b] <filename>\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	bool buf = false;
	int c;

	while ((c = getopt(argc, argv, "b")) != -1)
		switch (c) {
		case 'b':
			buf = true;
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	magic_t m = magic_open(0);
	if (m == NULL)
		err(EXIT_FAILURE, "magic_open");

	magic_load(m, NULL);

	const char *r;
	if (buf) {
		int fd = open(argv[0], O_RDONLY);
		if (fd == -1)
			err(EXIT_FAILURE, "Cannot open `%s'", argv[0]);

		struct stat st;
		if (fstat(fd, &st) == -1)
			err(EXIT_FAILURE, "Cannot stat `%s'", argv[0]);
		size_t l = (size_t)st.st_size;
		void *p = mmap(NULL, l, PROT_READ, MAP_FILE | MAP_PRIVATE, fd,
		    (off_t)0);
		if (p == MAP_FAILED)
			err(EXIT_FAILURE, "Cannot map `%s'", argv[0]);
		close(fd);
		r = magic_buffer(m, p, l);
		munmap(p, l);
	} else {
		r = magic_file(m, argv[0]);
	}
	magic_close(m);

	printf("%s\n", r ? r : "(null)");

	return 0;
}
