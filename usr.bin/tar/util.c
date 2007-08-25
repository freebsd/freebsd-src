/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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
#include <ctype.h>
#include <errno.h>
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
	if (l <= 0)
		return (0);
	buff[l] = 0;

	for (p = buff; *p != '\0'; p++) {
		if (isspace(0xff & (int)*p))
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
		if (mode & S_IXOTH) bp[9] = 't';
		else bp[9] = 'T';
	}
	if (archive_entry_acl_count(entry, ARCHIVE_ENTRY_ACL_TYPE_ACCESS))
		bp[10] = '+';
}


/*
 * Read lines from file and do something with each one.  If option_null
 * is set, lines are terminated with zero bytes; otherwise, they're
 * terminated with newlines.
 *
 * This uses a self-sizing buffer to handle arbitrarily-long lines.
 * If the "process" function returns non-zero for any line, this
 * function will return non-zero after attempting to process all
 * remaining lines.
 */
int
process_lines(struct bsdtar *bsdtar, const char *pathname,
    int (*process)(struct bsdtar *, const char *))
{
	FILE *f;
	char *buff, *buff_end, *line_start, *line_end, *p;
	size_t buff_length, bytes_read, bytes_wanted;
	int separator;
	int ret;

	separator = bsdtar->option_null ? '\0' : '\n';
	ret = 0;

	if (strcmp(pathname, "-") == 0)
		f = stdin;
	else
		f = fopen(pathname, "r");
	if (f == NULL)
		bsdtar_errc(bsdtar, 1, errno, "Couldn't open %s", pathname);
	buff_length = 8192;
	buff = malloc(buff_length);
	if (buff == NULL)
		bsdtar_errc(bsdtar, 1, ENOMEM, "Can't read %s", pathname);
	line_start = line_end = buff_end = buff;
	for (;;) {
		/* Get some more data into the buffer. */
		bytes_wanted = buff + buff_length - buff_end;
		bytes_read = fread(buff_end, 1, bytes_wanted, f);
		buff_end += bytes_read;
		/* Process all complete lines in the buffer. */
		while (line_end < buff_end) {
			if (*line_end == separator) {
				*line_end = '\0';
				if ((*process)(bsdtar, line_start) != 0)
					ret = -1;
				line_start = line_end + 1;
				line_end = line_start;
			} else
				line_end++;
		}
		if (feof(f))
			break;
		if (ferror(f))
			bsdtar_errc(bsdtar, 1, errno,
			    "Can't read %s", pathname);
		if (line_start > buff) {
			/* Move a leftover fractional line to the beginning. */
			memmove(buff, line_start, buff_end - line_start);
			buff_end -= line_start - buff;
			line_end -= line_start - buff;
			line_start = buff;
		} else {
			/* Line is too big; enlarge the buffer. */
			p = realloc(buff, buff_length *= 2);
			if (p == NULL)
				bsdtar_errc(bsdtar, 1, ENOMEM,
				    "Line too long in %s", pathname);
			buff_end = p + (buff_end - buff);
			line_end = p + (line_end - buff);
			line_start = buff = p;
		}
	}
	/* At end-of-file, handle the final line. */
	if (line_end > line_start) {
		*line_end = '\0';
		if ((*process)(bsdtar, line_start) != 0)
			ret = -1;
	}
	free(buff);
	if (f != stdin)
		fclose(f);
	return (ret);
}

/*-
 * The logic here for -C <dir> attempts to avoid
 * chdir() as long as possible.  For example:
 * "-C /foo -C /bar file"          needs chdir("/bar") but not chdir("/foo")
 * "-C /foo -C bar file"           needs chdir("/foo/bar")
 * "-C /foo -C bar /file1"         does not need chdir()
 * "-C /foo -C bar /file1 file2"   needs chdir("/foo/bar") before file2
 *
 * The only correct way to handle this is to record a "pending" chdir
 * request and combine multiple requests intelligently until we
 * need to process a non-absolute file.  set_chdir() adds the new dir
 * to the pending list; do_chdir() actually executes any pending chdir.
 *
 * This way, programs that build tar command lines don't have to worry
 * about -C with non-existent directories; such requests will only
 * fail if the directory must be accessed.
 */
void
set_chdir(struct bsdtar *bsdtar, const char *newdir)
{
	if (newdir[0] == '/') {
		/* The -C /foo -C /bar case; dump first one. */
		free(bsdtar->pending_chdir);
		bsdtar->pending_chdir = NULL;
	}
	if (bsdtar->pending_chdir == NULL)
		/* Easy case: no previously-saved dir. */
		bsdtar->pending_chdir = strdup(newdir);
	else {
		/* The -C /foo -C bar case; concatenate */
		char *old_pending = bsdtar->pending_chdir;
		size_t old_len = strlen(old_pending);
		bsdtar->pending_chdir = malloc(old_len + strlen(newdir) + 2);
		if (old_pending[old_len - 1] == '/')
			old_pending[old_len - 1] = '\0';
		if (bsdtar->pending_chdir != NULL)
			sprintf(bsdtar->pending_chdir, "%s/%s",
			    old_pending, newdir);
		free(old_pending);
	}
	if (bsdtar->pending_chdir == NULL)
		bsdtar_errc(bsdtar, 1, errno, "No memory");
}

void
do_chdir(struct bsdtar *bsdtar)
{
	if (bsdtar->pending_chdir == NULL)
		return;

	if (chdir(bsdtar->pending_chdir) != 0) {
		bsdtar_errc(bsdtar, 1, 0, "could not chdir to '%s'\n",
		    bsdtar->pending_chdir);
	}
	free(bsdtar->pending_chdir);
	bsdtar->pending_chdir = NULL;
}

/*
 * Handle --strip-components and any future path-rewriting options.
 * Returns non-zero if the pathname should not be extracted.
 */
int
edit_pathname(struct bsdtar *bsdtar, struct archive_entry *entry)
{
	const char *name = archive_entry_pathname(entry);

	/* Strip leading dir names as per --strip-components option. */
	if (bsdtar->strip_components > 0) {
		int r = bsdtar->strip_components;
		const char *p = name;

		while (r > 0) {
			switch (*p++) {
			case '/':
				r--;
				name = p;
				break;
			case '\0':
				/* Path is too short, skip it. */
				return (1);
			}
		}
	}

	/* Strip redundant leading '/' characters. */
	while (name[0] == '/' && name[1] == '/')
		name++;

	/* Strip leading '/' unless user has asked us not to. */
	if (name[0] == '/' && !bsdtar->option_absolute_paths) {
		/* Generate a warning the first time this happens. */
		if (!bsdtar->warned_lead_slash) {
			bsdtar_warnc(bsdtar, 0,
			    "Removing leading '/' from member names");
			bsdtar->warned_lead_slash = 1;
		}
		name++;
		/* Special case: Stripping leading '/' from "/" yields ".". */
		if (*name == '\0')
			name = ".";
	}

	/* Safely replace name in archive_entry. */
	if (name != archive_entry_pathname(entry)) {
		char *q = strdup(name);
		archive_entry_copy_pathname(entry, q);
		free(q);
	}
	return (0);
}

/*
 * Like strcmp(), but try to be a little more aware of the fact that
 * we're comparing two paths.  Right now, it just handles leading
 * "./" and trailing '/' specially, so that "a/b/" == "./a/b"
 *
 * TODO: Make this better, so that "./a//b/./c/" == "a/b/c"
 * TODO: After this works, push it down into libarchive.
 * TODO: Publish the path normalization routines in libarchive so
 * that bsdtar can normalize paths and use fast strcmp() instead
 * of this.
 */

int
pathcmp(const char *a, const char *b)
{
	/* Skip leading './' */
	if (a[0] == '.' && a[1] == '/' && a[2] != '\0')
		a += 2;
	if (b[0] == '.' && b[1] == '/' && b[2] != '\0')
		b += 2;
	/* Find the first difference, or return (0) if none. */
	while (*a == *b) {
		if (*a == '\0')
			return (0);
		a++;
		b++;
	}
	/*
	 * If one ends in '/' and the other one doesn't,
	 * they're the same.
	 */
	if (a[0] == '/' && a[1] == '\0' && b[0] == '\0')
		return (0);
	if (a[0] == '\0' && b[0] == '/' && b[1] == '\0')
		return (0);
	/* They're really different, return the correct sign. */
	return (*(const unsigned char *)a - *(const unsigned char *)b);
}
