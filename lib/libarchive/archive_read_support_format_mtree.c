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

#include "archive_platform.h"
__FBSDID("$FreeBSD$");

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <stddef.h>
/* #include <stdint.h> */ /* See archive_platform.h */
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_read_private.h"
#include "archive_string.h"

#ifndef O_BINARY
#define	O_BINARY 0
#endif

struct mtree_entry {
	struct mtree_entry *next;
	char *name;
	char *option_start;
	char *option_end;
	char full;
	char used;
};

struct mtree {
	struct archive_string	 line;
	size_t			 buffsize;
	char			*buff;
	off_t			 offset;
	int			 fd;
	int			 filetype;
	int			 archive_format;
	const char		*archive_format_name;
	struct mtree_entry	*entries;
	struct mtree_entry	*this_entry;
	struct archive_string	 current_dir;
	struct archive_string	 contents_name;
};

static int	cleanup(struct archive_read *);
static int	mtree_bid(struct archive_read *);
static void	parse_escapes(char *, struct mtree_entry *);
static int	parse_setting(struct archive_read *, struct mtree *,
		    struct archive_entry *, char *, char *);
static int	read_data(struct archive_read *a,
		    const void **buff, size_t *size, off_t *offset);
static ssize_t	readline(struct archive_read *, struct mtree *, char **, ssize_t);
static int	skip(struct archive_read *a);
static int	read_header(struct archive_read *,
		    struct archive_entry *);
static int64_t	mtree_atol10(char **);
static int64_t	mtree_atol8(char **);

int
archive_read_support_format_mtree(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct mtree *mtree;
	int r;

	mtree = (struct mtree *)malloc(sizeof(*mtree));
	if (mtree == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate mtree data");
		return (ARCHIVE_FATAL);
	}
	memset(mtree, 0, sizeof(*mtree));
	mtree->fd = -1;

	r = __archive_read_register_format(a, mtree,
	    mtree_bid, read_header, read_data, skip, cleanup);

	if (r != ARCHIVE_OK)
		free(mtree);
	return (ARCHIVE_OK);
}

static int
cleanup(struct archive_read *a)
{
	struct mtree *mtree;
	struct mtree_entry *p, *q;

	mtree = (struct mtree *)(a->format->data);
	p = mtree->entries;
	while (p != NULL) {
		q = p->next;
		free(p->name);
		/*
		 * Note: option_start, option_end are pointers into
		 * the block that p->name points to.  So we should
		 * not try to free them!
		 */
		free(p);
		p = q;
	}
	archive_string_free(&mtree->line);
	archive_string_free(&mtree->current_dir);
	archive_string_free(&mtree->contents_name);
	free(mtree->buff);
	free(mtree);
	(a->format->data) = NULL;
	return (ARCHIVE_OK);
}


static int
mtree_bid(struct archive_read *a)
{
	struct mtree *mtree;
	ssize_t bytes_read;
	const void *h;
	const char *signature = "#mtree";
	const char *p;
	int bid;

	mtree = (struct mtree *)(a->format->data);

	/* Now let's look at the actual header and see if it matches. */
	bytes_read = (a->decompressor->read_ahead)(a, &h, strlen(signature));

	if (bytes_read <= 0)
		return (bytes_read);

	p = h;
	bid = 0;
	while (bytes_read > 0 && *signature != '\0') {
		if (*p != *signature)
			return (bid = 0);
		bid += 8;
		p++;
		signature++;
		bytes_read--;
	}
	return (bid);
}

/*
 * The extended mtree format permits multiple lines specifying
 * attributes for each file.  Practically speaking, that means we have
 * to read the entire mtree file into memory up front.
 */
static int
read_mtree(struct archive_read *a, struct mtree *mtree)
{
	ssize_t len;
	char *p;
	struct mtree_entry *mentry;
	struct mtree_entry *last_mentry = NULL;

	mtree->archive_format = ARCHIVE_FORMAT_MTREE_V1;
	mtree->archive_format_name = "mtree";

	for (;;) {
		len = readline(a, mtree, &p, 256);
		if (len == 0) {
			mtree->this_entry = mtree->entries;
			return (ARCHIVE_OK);
		}
		if (len < 0)
			return (len);
		/* Leading whitespace is never significant, ignore it. */
		while (*p == ' ' || *p == '\t') {
			++p;
			--len;
		}
		/* Skip content lines and blank lines. */
		if (*p == '#')
			continue;
		if (*p == '\r' || *p == '\n' || *p == '\0')
			continue;
		mentry = malloc(sizeof(*mentry));
		if (mentry == NULL) {
			archive_set_error(&a->archive, ENOMEM,
			    "Can't allocate memory");
			return (ARCHIVE_FATAL);
		}
		memset(mentry, 0, sizeof(*mentry));
		/* Add this entry to list. */
		if (last_mentry == NULL) {
			last_mentry = mtree->entries = mentry;
		} else {
			last_mentry->next = mentry;
		}
		last_mentry = mentry;

		/* Copy line over onto heap. */
		mentry->name = malloc(len + 1);
		if (mentry->name == NULL) {
			free(mentry);
			archive_set_error(&a->archive, ENOMEM,
			    "Can't allocate memory");
			return (ARCHIVE_FATAL);
		}
		strcpy(mentry->name, p);
		mentry->option_end = mentry->name + len;
		/* Find end of name. */
		p = mentry->name;
		while (*p != ' ' && *p != '\n' && *p != '\0')
			++p;
		*p++ = '\0';
		parse_escapes(mentry->name, mentry);
		/* Find start of options and record it. */
		while (p < mentry->option_end && (*p == ' ' || *p == '\t'))
			++p;
		mentry->option_start = p;
		/* Null terminate each separate option. */
		while (++p < mentry->option_end)
			if (*p == ' ' || *p == '\t' || *p == '\n')
				*p = '\0';
	}
}

static int
read_header(struct archive_read *a, struct archive_entry *entry)
{
	struct stat st;
	struct mtree *mtree;
	struct mtree_entry *mentry, *mentry2;
	char *p, *q;
	int r = ARCHIVE_OK, r1;

	mtree = (struct mtree *)(a->format->data);

	if (mtree->fd >= 0) {
		close(mtree->fd);
		mtree->fd = -1;
	}

	if (mtree->entries == NULL) {
		r = read_mtree(a, mtree);
		if (r != ARCHIVE_OK)
			return (r);
	}

	a->archive.archive_format = mtree->archive_format;
	a->archive.archive_format_name = mtree->archive_format_name;

	for (;;) {
		mentry = mtree->this_entry;
		if (mentry == NULL) {
			mtree->this_entry = NULL;
			return (ARCHIVE_EOF);
		}
		mtree->this_entry = mentry->next;
		if (mentry->used)
			continue;
		mentry->used = 1;
		if (strcmp(mentry->name, "..") == 0) {
			if (archive_strlen(&mtree->current_dir) > 0) {
				/* Roll back current path. */
				p = mtree->current_dir.s
				    + mtree->current_dir.length - 1;
				while (p >= mtree->current_dir.s && *p != '/')
					--p;
				if (p >= mtree->current_dir.s)
					--p;
				mtree->current_dir.length
				    = p - mtree->current_dir.s + 1;
			}
			continue;
		}

		mtree->filetype = AE_IFREG;

		/* Parse options. */
		p = mentry->option_start;
		while (p < mentry->option_end) {
			q = p + strlen(p);
			r1 = parse_setting(a, mtree, entry, p, q);
			if (r1 != ARCHIVE_OK)
				r = r1;
			p = q + 1;
		}

		if (mentry->full) {
			archive_entry_copy_pathname(entry, mentry->name);
			/*
			 * "Full" entries are allowed to have multiple
			 * lines and those lines aren't required to be
			 * adjacent.  We don't support multiple lines
			 * for "relative" entries nor do we make any
			 * attempt to merge data from separate
			 * "relative" and "full" entries.  (Merging
			 * "relative" and "full" entries would require
			 * dealing with pathname canonicalization,
			 * which is a very tricky subject.)
			 */
			mentry2 = mentry->next;
			while (mentry2 != NULL) {
				if (mentry2->full
				    && !mentry2->used
				    && strcmp(mentry->name, mentry2->name) == 0) {
					/*
					 * Add those options as well;
					 * later lines override
					 * earlier ones.
					 */
					p = mentry2->option_start;
					while (p < mentry2->option_end) {
						q = p + strlen(p);
						r1 = parse_setting(a, mtree, entry, p, q);
						if (r1 != ARCHIVE_OK)
							r = r1;
						p = q + 1;
					}
					mentry2->used = 1;
				}
				mentry2 = mentry2->next;
			}
		} else {
			/*
			 * Relative entries require us to construct
			 * the full path and possibly update the
			 * current directory.
			 */
			size_t n = archive_strlen(&mtree->current_dir);
			if (n > 0)
				archive_strcat(&mtree->current_dir, "/");
			archive_strcat(&mtree->current_dir, mentry->name);
			archive_entry_copy_pathname(entry, mtree->current_dir.s);
			if (archive_entry_filetype(entry) != AE_IFDIR)
				mtree->current_dir.length = n;
		}

		/*
		 * Try to open and stat the file to get the real size.
		 * It would be nice to avoid this here so that getting
		 * a listing of an mtree wouldn't require opening
		 * every referenced contents file.  But then we
		 * wouldn't know the actual contents size, so I don't
		 * see a really viable way around this.  (Also, we may
		 * want to someday pull other unspecified info from
		 * the contents file on disk.)
		 */
		if (archive_strlen(&mtree->contents_name) > 0) {
			mtree->fd = open(mtree->contents_name.s,
			    O_RDONLY | O_BINARY);
			if (mtree->fd < 0) {
				archive_set_error(&a->archive, errno,
				    "Can't open content=\"%s\"",
				    mtree->contents_name.s);
				r = ARCHIVE_WARN;
			}
		} else {
			/* If the specified path opens, use it. */
			mtree->fd = open(mtree->current_dir.s,
			    O_RDONLY | O_BINARY);
			/* But don't fail if it's not there. */
		}

		/*
		 * If there is a contents file on disk, use that size;
		 * otherwise leave it as-is (it might have been set from
		 * the mtree size= keyword).
		 */
		if (mtree->fd >= 0) {
			fstat(mtree->fd, &st);
			archive_entry_set_size(entry, st.st_size);
		}

		return r;
	}
}

static int
parse_setting(struct archive_read *a, struct mtree *mtree, struct archive_entry *entry, char *key, char *end)
{
	char *val;


	if (end == key)
		return (ARCHIVE_OK);
	if (*key == '\0')
		return (ARCHIVE_OK);

	val = strchr(key, '=');
	if (val == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Malformed attribute \"%s\" (%d)", key, key[0]);
		return (ARCHIVE_WARN);
	}

	*val = '\0';
	++val;

	switch (key[0]) {
	case 'c':
		if (strcmp(key, "content") == 0) {
			parse_escapes(val, NULL);
			archive_strcpy(&mtree->contents_name, val);
			break;
		}
	case 'g':
		if (strcmp(key, "gid") == 0) {
			archive_entry_set_gid(entry, mtree_atol10(&val));
			break;
		}
		if (strcmp(key, "gname") == 0) {
			archive_entry_copy_gname(entry, val);
			break;
		}
	case 'm':
		if (strcmp(key, "mode") == 0) {
			if (val[0] == '0') {
				archive_entry_set_perm(entry,
				    mtree_atol8(&val));
			} else
				archive_set_error(&a->archive,
				    ARCHIVE_ERRNO_FILE_FORMAT,
				    "Symbolic mode \"%s\" unsupported", val);
			break;
		}
	case 't':
		if (strcmp(key, "type") == 0) {
			switch (val[0]) {
			case 'b':
				if (strcmp(val, "block") == 0) {
					mtree->filetype = AE_IFBLK;
					break;
				}
			case 'c':
				if (strcmp(val, "char") == 0) {
					mtree->filetype = AE_IFCHR;
					break;
				}
			case 'd':
				if (strcmp(val, "dir") == 0) {
					mtree->filetype = AE_IFDIR;
					break;
				}
			case 'f':
				if (strcmp(val, "fifo") == 0) {
					mtree->filetype = AE_IFIFO;
					break;
				}
				if (strcmp(val, "file") == 0) {
					mtree->filetype = AE_IFREG;
					break;
				}
			case 'l':
				if (strcmp(val, "link") == 0) {
					mtree->filetype = AE_IFLNK;
					break;
				}
			default:
				archive_set_error(&a->archive,
				    ARCHIVE_ERRNO_FILE_FORMAT,
				    "Unrecognized file type \"%s\"", val);
				return (ARCHIVE_WARN);
			}
			archive_entry_set_filetype(entry, mtree->filetype);
			break;
		}
		if (strcmp(key, "time") == 0) {
			archive_entry_set_mtime(entry, mtree_atol10(&val), 0);
			break;
		}
	case 'u':
		if (strcmp(key, "uid") == 0) {
			archive_entry_set_uid(entry, mtree_atol10(&val));
			break;
		}
		if (strcmp(key, "uname") == 0) {
			archive_entry_copy_uname(entry, val);
			break;
		}
	default:
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Unrecognized key %s=%s", key, val);
		return (ARCHIVE_WARN);
	}
	return (ARCHIVE_OK);
}

static int
read_data(struct archive_read *a, const void **buff, size_t *size, off_t *offset)
{
	ssize_t bytes_read;
	struct mtree *mtree;

	mtree = (struct mtree *)(a->format->data);
	if (mtree->fd < 0) {
		*buff = NULL;
		*offset = 0;
		*size = 0;
		return (ARCHIVE_EOF);
	}
	if (mtree->buff == NULL) {
		mtree->buffsize = 64 * 1024;
		mtree->buff = malloc(mtree->buffsize);
		if (mtree->buff == NULL) {
			archive_set_error(&a->archive, ENOMEM,
			    "Can't allocate memory");
		}
	}

	*buff = mtree->buff;
	*offset = mtree->offset;
	bytes_read = read(mtree->fd, mtree->buff, mtree->buffsize);
	if (bytes_read < 0) {
		archive_set_error(&a->archive, errno, "Can't read");
		return (ARCHIVE_WARN);
	}
	if (bytes_read == 0) {
		*size = 0;
		return (ARCHIVE_EOF);
	}
	mtree->offset += bytes_read;
	*size = (size_t)bytes_read;
	return (ARCHIVE_OK);
}

/* Skip does nothing except possibly close the contents file. */
static int
skip(struct archive_read *a)
{
	struct mtree *mtree;

	mtree = (struct mtree *)(a->format->data);
	if (mtree->fd >= 0) {
		close(mtree->fd);
		mtree->fd = -1;
	}
	return (ARCHIVE_OK);
}

/*
 * Since parsing octal escapes always makes strings shorter,
 * we can always do this conversion in-place.
 */
static void
parse_escapes(char *src, struct mtree_entry *mentry)
{
	char *dest = src;
	char c;

	while (*src != '\0') {
		c = *src++;
		if (c == '/' && mentry != NULL)
			mentry->full = 1;
		if (c == '\\') {
			if (src[0] >= '0' && src[0] <= '3'
			    && src[1] >= '0' && src[1] <= '7'
			    && src[2] >= '0' && src[2] <= '7') {
				c = (src[0] - '0') << 6;
				c |= (src[1] - '0') << 3;
				c |= (src[2] - '0');
				src += 3;
			}
		}
		*dest++ = c;
	}
	*dest = '\0';
}

/*
 * Note that this implementation does not (and should not!) obey
 * locale settings; you cannot simply substitute strtol here, since
 * it does obey locale.
 */
static int64_t
mtree_atol8(char **p)
{
	int64_t	l, limit, last_digit_limit;
	int digit, base;

	base = 8;
	limit = INT64_MAX / base;
	last_digit_limit = INT64_MAX % base;

	l = 0;
	digit = **p - '0';
	while (digit >= 0 && digit < base) {
		if (l>limit || (l == limit && digit > last_digit_limit)) {
			l = INT64_MAX; /* Truncate on overflow. */
			break;
		}
		l = (l * base) + digit;
		digit = *++(*p) - '0';
	}
	return (l);
}

/*
 * Note that this implementation does not (and should not!) obey
 * locale settings; you cannot simply substitute strtol here, since
 * it does obey locale.
 */
static int64_t
mtree_atol10(char **p)
{
	int64_t l, limit, last_digit_limit;
	int base, digit, sign;

	base = 10;
	limit = INT64_MAX / base;
	last_digit_limit = INT64_MAX % base;

	if (**p == '-') {
		sign = -1;
		++(*p);
	} else
		sign = 1;

	l = 0;
	digit = **p - '0';
	while (digit >= 0 && digit < base) {
		if (l > limit || (l == limit && digit > last_digit_limit)) {
			l = UINT64_MAX; /* Truncate on overflow. */
			break;
		}
		l = (l * base) + digit;
		digit = *++(*p) - '0';
	}
	return (sign < 0) ? -l : l;
}

/*
 * Returns length of line (including trailing newline)
 * or negative on error.  'start' argument is updated to
 * point to first character of line.
 */
static ssize_t
readline(struct archive_read *a, struct mtree *mtree, char **start, ssize_t limit)
{
	ssize_t bytes_read;
	ssize_t total_size = 0;
	const void *t;
	const char *s;
	void *p;

	/* Accumulate line in a line buffer. */
	for (;;) {
		/* Read some more. */
		bytes_read = (a->decompressor->read_ahead)(a, &t, 1);
		if (bytes_read == 0)
			return (0);
		if (bytes_read < 0)
			return (ARCHIVE_FATAL);
		s = t;  /* Start of line? */
		p = memchr(t, '\n', bytes_read);
		/* If we found '\n', trim the read. */
		if (p != NULL) {
			bytes_read = 1 + ((const char *)p) - s;
		}
		if (total_size + bytes_read + 1 > limit) {
			archive_set_error(&a->archive,
			    ARCHIVE_ERRNO_FILE_FORMAT,
			    "Line too long");
			return (ARCHIVE_FATAL);
		}
		if (archive_string_ensure(&mtree->line,
			total_size + bytes_read + 1) == NULL) {
			archive_set_error(&a->archive, ENOMEM,
			    "Can't allocate working buffer");
			return (ARCHIVE_FATAL);
		}
		memcpy(mtree->line.s + total_size, t, bytes_read);
		(a->decompressor->consume)(a, bytes_read);
		total_size += bytes_read;
		/* Null terminate. */
		mtree->line.s[total_size] = '\0';
		/* If we found '\n', clean up and return. */
		if (p != NULL) {
			*start = mtree->line.s;
			return (total_size);
		}
	}
}
