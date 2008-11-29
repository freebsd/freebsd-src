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

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>  /* Linux doesn't define mode_t, etc. in sys/stat.h. */
#endif
#include <ctype.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_WCTYPE_H
#include <wctype.h>
#else
/* If we don't have wctype, we need to hack up some version of iswprint(). */
#define iswprint isprint
#endif

#include "bsdtar.h"

static void	bsdtar_vwarnc(struct bsdtar *, int code,
		    const char *fmt, va_list ap);
static size_t	bsdtar_expand_char(char *, size_t, char);
static const char *strip_components(const char *path, int elements);

/* TODO:  Hack up a version of mbtowc for platforms with no wide
 * character support at all.  I think the following might suffice,
 * but it needs careful testing.
 * #if !HAVE_MBTOWC
 * #define mbtowc(wcp, p, n) ((*wcp = *p), 1)
 * #endif
 */

/*
 * Print a string, taking care with any non-printable characters.
 *
 * Note that we use a stack-allocated buffer to receive the formatted
 * string if we can.  This is partly performance (avoiding a call to
 * malloc()), partly out of expedience (we have to call vsnprintf()
 * before malloc() anyway to find out how big a buffer we need; we may
 * as well point that first call at a small local buffer in case it
 * works), but mostly for safety (so we can use this to print messages
 * about out-of-memory conditions).
 */

void
safe_fprintf(FILE *f, const char *fmt, ...)
{
	char fmtbuff_stack[256]; /* Place to format the printf() string. */
	char outbuff[256]; /* Buffer for outgoing characters. */
	char *fmtbuff_heap; /* If fmtbuff_stack is too small, we use malloc */
	char *fmtbuff;  /* Pointer to fmtbuff_stack or fmtbuff_heap. */
	int fmtbuff_length;
	int length;
	va_list ap;
	const char *p;
	unsigned i;
	wchar_t wc;
	char try_wc;

	/* Use a stack-allocated buffer if we can, for speed and safety. */
	fmtbuff_heap = NULL;
	fmtbuff_length = sizeof(fmtbuff_stack);
	fmtbuff = fmtbuff_stack;

	/* Try formatting into the stack buffer. */
	va_start(ap, fmt);
	length = vsnprintf(fmtbuff, fmtbuff_length, fmt, ap);
	va_end(ap);

	/* If the result was too large, allocate a buffer on the heap. */
	if (length >= fmtbuff_length) {
		fmtbuff_length = length+1;
		fmtbuff_heap = malloc(fmtbuff_length);

		/* Reformat the result into the heap buffer if we can. */
		if (fmtbuff_heap != NULL) {
			fmtbuff = fmtbuff_heap;
			va_start(ap, fmt);
			length = vsnprintf(fmtbuff, fmtbuff_length, fmt, ap);
			va_end(ap);
		} else {
			/* Leave fmtbuff pointing to the truncated
			 * string in fmtbuff_stack. */
			length = sizeof(fmtbuff_stack) - 1;
		}
	}

	/* Note: mbrtowc() has a cleaner API, but mbtowc() seems a bit
	 * more portable, so we use that here instead. */
	mbtowc(NULL, NULL, 0); /* Reset the shift state. */

	/* Write data, expanding unprintable characters. */
	p = fmtbuff;
	i = 0;
	try_wc = 1;
	while (*p != '\0') {
		int n;

		/* Convert to wide char, test if the wide
		 * char is printable in the current locale. */
		if (try_wc && (n = mbtowc(&wc, p, length)) != -1) {
			length -= n;
			if (iswprint(wc) && wc != L'\\') {
				/* Printable, copy the bytes through. */
				while (n-- > 0)
					outbuff[i++] = *p++;
			} else {
				/* Not printable, format the bytes. */
				while (n-- > 0)
					i += bsdtar_expand_char(
					    outbuff, i, *p++);
			}
		} else {
			/* After any conversion failure, don't bother
			 * trying to convert the rest. */
			i += bsdtar_expand_char(outbuff, i, *p++);
			try_wc = 0;
		}

		/* If our output buffer is full, dump it and keep going. */
		if (i > (sizeof(outbuff) - 20)) {
			outbuff[i++] = '\0';
			fprintf(f, "%s", outbuff);
			i = 0;
		}
	}
	outbuff[i++] = '\0';
	fprintf(f, "%s", outbuff);

	/* If we allocated a heap-based formatting buffer, free it now. */
	if (fmtbuff_heap != NULL)
		free(fmtbuff_heap);
}

/*
 * Render an arbitrary sequence of bytes into printable ASCII characters.
 */
static size_t
bsdtar_expand_char(char *buff, size_t offset, char c)
{
	size_t i = offset;

	if (isprint(c) && c != '\\')
		buff[i++] = c;
	else {
		buff[i++] = '\\';
		switch (c) {
		case '\a': buff[i++] = 'a'; break;
		case '\b': buff[i++] = 'b'; break;
		case '\f': buff[i++] = 'f'; break;
		case '\n': buff[i++] = 'n'; break;
#if '\r' != '\n'
		/* On some platforms, \n and \r are the same. */
		case '\r': buff[i++] = 'r'; break;
#endif
		case '\t': buff[i++] = 't'; break;
		case '\v': buff[i++] = 'v'; break;
		case '\\': buff[i++] = '\\'; break;
		default:
			sprintf(buff + i, "%03o", 0xFF & (int)c);
			i += 3;
		}
	}

	return (i - offset);
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

	l = read(2, buff, sizeof(buff) - 1);
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
	size_t buff_length, new_buff_length, bytes_read, bytes_wanted;
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
			new_buff_length = buff_length * 2;
			if (new_buff_length <= buff_length)
				bsdtar_errc(bsdtar, 1, ENOMEM,
				    "Line too long in %s", pathname);
			buff_length = new_buff_length;
			p = realloc(buff, buff_length);
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

const char *
strip_components(const char *path, int elements)
{
	const char *p = path;

	while (elements > 0) {
		switch (*p++) {
		case '/':
			elements--;
			path = p;
			break;
		case '\0':
			/* Path is too short, skip it. */
			return (NULL);
		}
	}

	while (*path == '/')
	       ++path;
	if (*path == '\0')
	       return (NULL);

	return (path);
}

/*
 * Handle --strip-components and any future path-rewriting options.
 * Returns non-zero if the pathname should not be extracted.
 *
 * TODO: Support pax-style regex path rewrites.
 */
int
edit_pathname(struct bsdtar *bsdtar, struct archive_entry *entry)
{
	const char *name = archive_entry_pathname(entry);
#if HAVE_REGEX_H
	char *subst_name;
#endif
	int r;

#if HAVE_REGEX_H
	r = apply_substitution(bsdtar, name, &subst_name, 0);
	if (r == -1) {
		bsdtar_warnc(bsdtar, 0, "Invalid substituion, skipping entry");
		return 1;
	}
	if (r == 1) {
		archive_entry_copy_pathname(entry, subst_name);
		if (*subst_name == '\0') {
			free(subst_name);
			return -1;
		} else
			free(subst_name);
		name = archive_entry_pathname(entry);
	}

	if (archive_entry_hardlink(entry)) {
		r = apply_substitution(bsdtar, archive_entry_hardlink(entry), &subst_name, 1);
		if (r == -1) {
			bsdtar_warnc(bsdtar, 0, "Invalid substituion, skipping entry");
			return 1;
		}
		if (r == 1) {
			archive_entry_copy_hardlink(entry, subst_name);
			free(subst_name);
		}
	}
	if (archive_entry_symlink(entry) != NULL) {
		r = apply_substitution(bsdtar, archive_entry_symlink(entry), &subst_name, 1);
		if (r == -1) {
			bsdtar_warnc(bsdtar, 0, "Invalid substituion, skipping entry");
			return 1;
		}
		if (r == 1) {
			archive_entry_copy_symlink(entry, subst_name);
			free(subst_name);
		}
	}
#endif

	/* Strip leading dir names as per --strip-components option. */
	if (bsdtar->strip_components > 0) {
		const char *linkname = archive_entry_hardlink(entry);

		name = strip_components(name, bsdtar->strip_components);
		if (name == NULL)
			return (1);

		if (linkname != NULL) {
			linkname = strip_components(linkname,
			    bsdtar->strip_components);
			if (linkname == NULL)
				return (1);
			archive_entry_copy_hardlink(entry, linkname);
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
