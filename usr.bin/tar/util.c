/*-
 * Copyright (c) 2003-2004 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bsdtar_platform.h"
__FBSDID("$FreeBSD$");

#include <sys/stat.h>
#include <sys/types.h>  /* Linux doesn't define mode_t, etc. in sys/stat.h. */
#include <archive_entry.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsdtar.h"

static void	bsdtar_vwarnc(struct bsdtar *, int code,
		    const char *fmt, va_list ap);

/*
 * Print a string, taking care with any non-printable characters.
 */

void
safe_fprintf(FILE *f, const char *fmt, ...)
{
	char *buff;
	char *buff_heap;
	int buff_length;
	int length;
	va_list ap;
	char *p;
	unsigned i;
	char buff_stack[256];
	char copy_buff[256];

	/* Use a stack-allocated buffer if we can, for speed and safety. */
	buff_heap = NULL;
	buff_length = sizeof(buff_stack);
	buff = buff_stack;

	va_start(ap, fmt);
	length = vsnprintf(buff, buff_length, fmt, ap);
	va_end(ap);
	/* If the result is too large, allocate a buffer on the heap. */
	if (length >= buff_length) {
		buff_length = length+1;
		buff_heap = malloc(buff_length);
		/* Failsafe: use the truncated string if malloc fails. */
		if (buff_heap != NULL) {
			buff = buff_heap;
			va_start(ap, fmt);
			length = vsnprintf(buff, buff_length, fmt, ap);
			va_end(ap);
		}
	}

	/* Write data, expanding unprintable characters. */
	p = buff;
	i = 0;
	while (*p != '\0') {
		unsigned char c = *p++;

		if (isprint(c) && c != '\\')
			copy_buff[i++] = c;
		else {
			copy_buff[i++] = '\\';
			switch (c) {
			case '\a': copy_buff[i++] = 'a'; break;
			case '\b': copy_buff[i++] = 'b'; break;
			case '\f': copy_buff[i++] = 'f'; break;
			case '\n': copy_buff[i++] = 'n'; break;
#if '\r' != '\n'
			/* On some platforms, \n and \r are the same. */
			case '\r': copy_buff[i++] = 'r'; break;
#endif
			case '\t': copy_buff[i++] = 't'; break;
			case '\v': copy_buff[i++] = 'v'; break;
			case '\\': copy_buff[i++] = '\\'; break;
			default:
				sprintf(copy_buff + i, "%03o", c);
				i += 3;
			}
		}

		/* If our temp buffer is full, dump it and keep going. */
		if (i > (sizeof(copy_buff) - 8)) {
			copy_buff[i++] = '\0';
			fprintf(f, "%s", copy_buff);
			i = 0;
		}
	}
	copy_buff[i++] = '\0';
	fprintf(f, "%s", copy_buff);

	/* If we allocated a heap-based buffer, free it now. */
	if (buff_heap != NULL)
		free(buff_heap);
}

static void
bsdtar_vwarnc(struct bsdtar *bsdtar, int code, const char *fmt, va_list ap)
{
	fprintf(stderr, "%s: ", bsdtar->progname);
	vfprintf(stderr, fmt, ap);
	if (code != 0)
		fprintf(stderr, ": %s", strerror(code));
	fprintf(stderr, "\n");
}

void
bsdtar_warnc(struct bsdtar *bsdtar, int code, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	bsdtar_vwarnc(bsdtar, code, fmt, ap);
	va_end(ap);
}

void
bsdtar_errc(struct bsdtar *bsdtar, int eval, int code, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	bsdtar_vwarnc(bsdtar, code, fmt, ap);
	va_end(ap);
	exit(eval);
}

int
yes(const char *fmt, ...)
{
	char buff[32];
	char *p;
	ssize_t l;

	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, " (y/N)? ");
	fflush(stderr);

	l = read(2, buff, sizeof(buff));
	buff[l] = 0;

	for (p = buff; *p != '\0'; p++) {
		if (isspace(*p))
			continue;
		switch(*p) {
		case 'y': case 'Y':
			return (1);
		case 'n': case 'N':
			return (0);
		default:
			return (0);
		}
	}

	return (0);
}

void
bsdtar_strmode(struct archive_entry *entry, char *bp)
{
	static const char *perms = "?rwxrwxrwx ";
	static const mode_t permbits[] =
	    { S_IRUSR, S_IWUSR, S_IXUSR, S_IRGRP, S_IWGRP, S_IXGRP,
	      S_IROTH, S_IWOTH, S_IXOTH };
	mode_t mode;
	int i;

	/* Fill in a default string, then selectively override. */
	strcpy(bp, perms);

	mode = archive_entry_mode(entry);
	switch (mode & S_IFMT) {
	case S_IFREG:  bp[0] = '-'; break;
	case S_IFBLK:  bp[0] = 'b'; break;
	case S_IFCHR:  bp[0] = 'c'; break;
	case S_IFDIR:  bp[0] = 'd'; break;
	case S_IFLNK:  bp[0] = 'l'; break;
	case S_IFSOCK: bp[0] = 's'; break;
#ifdef S_IFIFO
	case S_IFIFO:  bp[0] = 'p'; break;
#endif
#ifdef S_IFWHT
	case S_IFWHT:  bp[0] = 'w'; break;
#endif
	}

	for (i = 0; i < 9; i++)
		if (!(mode & permbits[i]))
			bp[i+1] = '-';

	if (mode & S_ISUID) {
		if (mode & S_IXUSR) bp[3] = 's';
		else bp[3] = 'S';
	}
	if (mode & S_ISGID) {
		if (mode & S_IXGRP) bp[6] = 's';
		else bp[6] = 'S';
	}
	if (mode & S_ISVTX) {
		if (mode & S_IXGRP) bp[9] = 't';
		else bp[9] = 'T';
	}
	if (archive_entry_acl_count(entry, ARCHIVE_ENTRY_ACL_TYPE_ACCESS))
		bp[10] = '+';
}
