/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * Copyright (c) 2008 Joerg Sonnenberger
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

#define	MTREE_HAS_DEVICE	0x0001
#define	MTREE_HAS_FFLAGS	0x0002
#define	MTREE_HAS_GID		0x0004
#define	MTREE_HAS_GNAME		0x0008
#define	MTREE_HAS_MTIME		0x0010
#define	MTREE_HAS_NLINK		0x0020
#define	MTREE_HAS_PERM		0x0040
#define	MTREE_HAS_SIZE		0x0080
#define	MTREE_HAS_TYPE		0x0100
#define	MTREE_HAS_UID		0x0200
#define	MTREE_HAS_UNAME		0x0400

#define	MTREE_HAS_OPTIONAL	0x0800

struct mtree_option {
	struct mtree_option *next;
	char *value;
};

struct mtree_entry {
	struct mtree_entry *next;
	struct mtree_option *options;
	char *name;
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

	struct archive_entry_linkresolver *resolver;

	off_t			 cur_size, cur_offset;
};

static int	cleanup(struct archive_read *);
static int	mtree_bid(struct archive_read *);
static int	parse_file(struct archive_read *, struct archive_entry *,
		    struct mtree *, struct mtree_entry *, int *);
static void	parse_escapes(char *, struct mtree_entry *);
static int	parse_line(struct archive_read *, struct archive_entry *,
		    struct mtree *, struct mtree_entry *, int *);
static int	parse_keyword(struct archive_read *, struct mtree *,
		    struct archive_entry *, struct mtree_option *, int *);
static int	read_data(struct archive_read *a,
		    const void **buff, size_t *size, off_t *offset);
static ssize_t	readline(struct archive_read *, struct mtree *, char **, ssize_t);
static int	skip(struct archive_read *a);
static int	read_header(struct archive_read *,
		    struct archive_entry *);
static int64_t	mtree_atol10(char **);
static int64_t	mtree_atol8(char **);
static int64_t	mtree_atol(char **);

static void
free_options(struct mtree_option *head)
{
	struct mtree_option *next;

	for (; head != NULL; head = next) {
		next = head->next;
		free(head->value);
		free(head);
	}
}

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
		free_options(p->options);
		free(p);
		p = q;
	}
	archive_string_free(&mtree->line);
	archive_string_free(&mtree->current_dir);
	archive_string_free(&mtree->contents_name);
	archive_entry_linkresolver_free(mtree->resolver);

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
 * attributes for each file.  For those entries, only the last line
 * is actually used.  Practically speaking, that means we have
 * to read the entire mtree file into memory up front.
 *
 * The parsing is done in two steps.  First, it is decided if a line
 * changes the global defaults and if it is, processed accordingly.
 * Otherwise, the options of the line are merged with the current
 * global options.
 */
static int
add_option(struct archive_read *a, struct mtree_option **global,
    const char *value, size_t len)
{
	struct mtree_option *option;

	if ((option = malloc(sizeof(*option))) == NULL) {
		archive_set_error(&a->archive, errno, "Can't allocate memory");
		return (ARCHIVE_FATAL);
	}
	if ((option->value = malloc(len + 1)) == NULL) {
		free(option);
		archive_set_error(&a->archive, errno, "Can't allocate memory");
		return (ARCHIVE_FATAL);
	}
	memcpy(option->value, value, len);
	option->value[len] = '\0';
	option->next = *global;
	*global = option;
	return (ARCHIVE_OK);
}

static void
remove_option(struct mtree_option **global, const char *value, size_t len)
{
	struct mtree_option *iter, *last;

	last = NULL;
	for (iter = *global; iter != NULL; last = iter, iter = iter->next) {
		if (strncmp(iter->value, value, len) == 0 &&
		    (iter->value[len] == '\0' ||
		     iter->value[len] == '='))
			break;
	}
	if (iter == NULL)
		return;
	if (last == NULL)
		*global = iter->next;
	else
		last->next = iter->next;

	free(iter->value);
	free(iter);
}

static int
process_global_set(struct archive_read *a,
    struct mtree_option **global, const char *line)
{
	const char *next, *eq;
	size_t len;
	int r;

	line += 4;
	for (;;) {
		next = line + strspn(line, " \t\r\n");
		if (*next == '\0')
			return (ARCHIVE_OK);
		line = next;
		next = line + strcspn(line, " \t\r\n");
		eq = strchr(line, '=');
		if (eq > next)
			len = next - line;
		else
			len = eq - line;

		remove_option(global, line, len);
		r = add_option(a, global, line, next - line);
		if (r != ARCHIVE_OK)
			return (r);
		line = next;
	}
}

static int
process_global_unset(struct archive_read *a,
    struct mtree_option **global, const char *line)
{
	const char *next;
	size_t len;

	line += 6;
	if (strchr(line, '=') != NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "/unset shall not contain `='");
		return ARCHIVE_FATAL;
	}

	for (;;) {
		next = line + strspn(line, " \t\r\n");
		if (*next == '\0')
			return (ARCHIVE_OK);
		line = next;
		len = strcspn(line, " \t\r\n");

		if (len == 3 && strncmp(line, "all", 3) == 0) {
			free_options(*global);
			*global = NULL;
		} else {
			remove_option(global, line, len);
		}

		line += len;
	}
}

static int
process_add_entry(struct archive_read *a, struct mtree *mtree,
    struct mtree_option **global, const char *line,
    struct mtree_entry **last_entry)
{
	struct mtree_entry *entry;
	struct mtree_option *iter;
	const char *next, *eq;
	size_t len;
	int r;

	if ((entry = malloc(sizeof(*entry))) == NULL) {
		archive_set_error(&a->archive, errno, "Can't allocate memory");
		return (ARCHIVE_FATAL);
	}
	entry->next = NULL;
	entry->options = NULL;
	entry->name = NULL;
	entry->used = 0;
	entry->full = 0;

	/* Add this entry to list. */
	if (*last_entry == NULL)
		mtree->entries = entry;
	else
		(*last_entry)->next = entry;
	*last_entry = entry;

	len = strcspn(line, " \t\r\n");
	if ((entry->name = malloc(len + 1)) == NULL) {
		archive_set_error(&a->archive, errno, "Can't allocate memory");
		return (ARCHIVE_FATAL);
	}

	memcpy(entry->name, line, len);
	entry->name[len] = '\0';
	parse_escapes(entry->name, entry);

	line += len;
	for (iter = *global; iter != NULL; iter = iter->next) {
		r = add_option(a, &entry->options, iter->value,
		    strlen(iter->value));
		if (r != ARCHIVE_OK)
			return (r);
	}

	for (;;) {
		next = line + strspn(line, " \t\r\n");
		if (*next == '\0')
			return (ARCHIVE_OK);
		line = next;
		next = line + strcspn(line, " \t\r\n");
		eq = strchr(line, '=');
		if (eq > next)
			len = next - line;
		else
			len = eq - line;

		remove_option(&entry->options, line, len);
		r = add_option(a, &entry->options, line, next - line);
		if (r != ARCHIVE_OK)
			return (r);
		line = next;
	}
}

static int
read_mtree(struct archive_read *a, struct mtree *mtree)
{
	ssize_t len;
	uintmax_t counter;
	char *p;
	struct mtree_option *global;
	struct mtree_entry *last_entry;
	int r;

	mtree->archive_format = ARCHIVE_FORMAT_MTREE;
	mtree->archive_format_name = "mtree";

	global = NULL;
	last_entry = NULL;
	r = ARCHIVE_OK;

	for (counter = 1; ; ++counter) {
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
		if (*p != '/') {
			r = process_add_entry(a, mtree, &global, p,
			    &last_entry);
		} else if (strncmp(p, "/set", 4) == 0) {
			if (p[4] != ' ' && p[4] != '\t')
				break;
			r = process_global_set(a, &global, p);
		} else if (strncmp(p, "/unset", 6) == 0) {
			if (p[6] != ' ' && p[6] != '\t')
				break;
			r = process_global_unset(a, &global, p);
		} else
			break;

		if (r != ARCHIVE_OK)
			return r;
	}

	archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
	    "Can't parse line %ju", counter);
	return ARCHIVE_FATAL;
}

/*
 * Read in the entire mtree file into memory on the first request.
 * Then use the next unused file to satisfy each header request.
 */
static int
read_header(struct archive_read *a, struct archive_entry *entry)
{
	struct mtree *mtree;
	char *p;
	int r, use_next;

	mtree = (struct mtree *)(a->format->data);

	if (mtree->fd >= 0) {
		close(mtree->fd);
		mtree->fd = -1;
	}

	if (mtree->entries == NULL) {
		mtree->resolver = archive_entry_linkresolver_new();
		if (mtree->resolver == NULL)
			return ARCHIVE_FATAL;
		archive_entry_linkresolver_set_strategy(mtree->resolver,
		    ARCHIVE_FORMAT_MTREE);
		r = read_mtree(a, mtree);
		if (r != ARCHIVE_OK)
			return (r);
	}

	a->archive.archive_format = mtree->archive_format;
	a->archive.archive_format_name = mtree->archive_format_name;

	for (;;) {
		if (mtree->this_entry == NULL)
			return (ARCHIVE_EOF);
		if (strcmp(mtree->this_entry->name, "..") == 0) {
			mtree->this_entry->used = 1;
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
		}
		if (!mtree->this_entry->used) {
			use_next = 0;
			r = parse_file(a, entry, mtree, mtree->this_entry, &use_next);
			if (use_next == 0)
				return (r);
		}
		mtree->this_entry = mtree->this_entry->next;
	}
}

/*
 * A single file can have multiple lines contribute specifications.
 * Parse as many lines as necessary, then pull additional information
 * from a backing file on disk as necessary.
 */
static int
parse_file(struct archive_read *a, struct archive_entry *entry,
    struct mtree *mtree, struct mtree_entry *mentry, int *use_next)
{
	const char *path;
	struct stat st_storage, *st;
	struct mtree_entry *mp;
	struct archive_entry *sparse_entry;
	int r = ARCHIVE_OK, r1, parsed_kws, mismatched_type;

	mentry->used = 1;

	/* Initialize reasonable defaults. */
	mtree->filetype = AE_IFREG;
	archive_entry_set_size(entry, 0);

	/* Parse options from this line. */
	parsed_kws = 0;
	r = parse_line(a, entry, mtree, mentry, &parsed_kws);

	if (mentry->full) {
		archive_entry_copy_pathname(entry, mentry->name);
		/*
		 * "Full" entries are allowed to have multiple lines
		 * and those lines aren't required to be adjacent.  We
		 * don't support multiple lines for "relative" entries
		 * nor do we make any attempt to merge data from
		 * separate "relative" and "full" entries.  (Merging
		 * "relative" and "full" entries would require dealing
		 * with pathname canonicalization, which is a very
		 * tricky subject.)
		 */
		for (mp = mentry->next; mp != NULL; mp = mp->next) {
			if (mp->full && !mp->used
			    && strcmp(mentry->name, mp->name) == 0) {
				/* Later lines override earlier ones. */
				mp->used = 1;
				r1 = parse_line(a, entry, mtree, mp,
				    &parsed_kws);
				if (r1 < r)
					r = r1;
			}
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
	 * Try to open and stat the file to get the real size
	 * and other file info.  It would be nice to avoid
	 * this here so that getting a listing of an mtree
	 * wouldn't require opening every referenced contents
	 * file.  But then we wouldn't know the actual
	 * contents size, so I don't see a really viable way
	 * around this.  (Also, we may want to someday pull
	 * other unspecified info from the contents file on
	 * disk.)
	 */
	mtree->fd = -1;
	if (archive_strlen(&mtree->contents_name) > 0)
		path = mtree->contents_name.s;
	else
		path = archive_entry_pathname(entry);

	if (archive_entry_filetype(entry) == AE_IFREG ||
	    archive_entry_filetype(entry) == AE_IFDIR) {
		mtree->fd = open(path,
		    O_RDONLY | O_BINARY);
		if (mtree->fd == -1 &&
		    (errno != ENOENT ||
		     archive_strlen(&mtree->contents_name) > 0)) {
			archive_set_error(&a->archive, errno,
			    "Can't open %s", path);
			r = ARCHIVE_WARN;
		}
	}

	st = &st_storage;
	if (mtree->fd >= 0) {
		if (fstat(mtree->fd, st) == -1) {
			archive_set_error(&a->archive, errno,
			    "Could not fstat %s", path);
			r = ARCHIVE_WARN;
			/* If we can't stat it, don't keep it open. */
			close(mtree->fd);
			mtree->fd = -1;
			st = NULL;
		}
	} else if (lstat(path, st) == -1) {
		st = NULL;
	}

	/*
	 * If there is a contents file on disk, use that size;
	 * otherwise leave it as-is (it might have been set from
	 * the mtree size= keyword).
	 */
	if (st != NULL) {
		mismatched_type = 0;
		if ((st->st_mode & S_IFMT) == S_IFREG &&
		    archive_entry_filetype(entry) != AE_IFREG)
			mismatched_type = 1;
		if ((st->st_mode & S_IFMT) == S_IFLNK &&
		    archive_entry_filetype(entry) != AE_IFLNK)
			mismatched_type = 1;
		if ((st->st_mode & S_IFSOCK) == S_IFSOCK &&
		    archive_entry_filetype(entry) != AE_IFSOCK)
			mismatched_type = 1;
		if ((st->st_mode & S_IFMT) == S_IFCHR &&
		    archive_entry_filetype(entry) != AE_IFCHR)
			mismatched_type = 1;
		if ((st->st_mode & S_IFMT) == S_IFBLK &&
		    archive_entry_filetype(entry) != AE_IFBLK)
			mismatched_type = 1;
		if ((st->st_mode & S_IFMT) == S_IFDIR &&
		    archive_entry_filetype(entry) != AE_IFDIR)
			mismatched_type = 1;
		if ((st->st_mode & S_IFMT) == S_IFIFO &&
		    archive_entry_filetype(entry) != AE_IFIFO)
			mismatched_type = 1;

		if (mismatched_type) {
			if ((parsed_kws & MTREE_HAS_OPTIONAL) == 0) {
				archive_set_error(&a->archive,
				    ARCHIVE_ERRNO_MISC,
				    "mtree specification has different type for %s",
				    archive_entry_pathname(entry));
				r = ARCHIVE_WARN;
			} else {
				*use_next = 1;
			}
			/* Don't hold a non-regular file open. */
			if (mtree->fd >= 0)
				close(mtree->fd);
			mtree->fd = -1;
			st = NULL;
			return r;
		}
	}

	if (st != NULL) {
		if ((parsed_kws & MTREE_HAS_DEVICE) == 0 &&
		    (archive_entry_filetype(entry) == AE_IFCHR ||
		     archive_entry_filetype(entry) == AE_IFBLK))
			archive_entry_set_rdev(entry, st->st_rdev);
		if ((parsed_kws & (MTREE_HAS_GID | MTREE_HAS_GNAME)) == 0)
			archive_entry_set_gid(entry, st->st_gid);
		if ((parsed_kws & (MTREE_HAS_UID | MTREE_HAS_UNAME)) == 0)
			archive_entry_set_uid(entry, st->st_uid);
		if ((parsed_kws & MTREE_HAS_MTIME) == 0) {
#if HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC
			archive_entry_set_mtime(entry, st->st_mtime,
			    st->st_mtimespec.tv_nsec);
#elif HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
			archive_entry_set_mtime(entry, st->st_mtime,
			    st->st_mtim.tv_nsec);
#else
			archive_entry_set_mtime(entry, st->st_mtime, 0);
#endif
		}
		if ((parsed_kws & MTREE_HAS_NLINK) == 0)
			archive_entry_set_nlink(entry, st->st_nlink);
		if ((parsed_kws & MTREE_HAS_PERM) == 0)
			archive_entry_set_perm(entry, st->st_mode);
		if ((parsed_kws & MTREE_HAS_SIZE) == 0)
			archive_entry_set_size(entry, st->st_size);
		archive_entry_set_ino(entry, st->st_ino);
		archive_entry_set_dev(entry, st->st_dev);

		archive_entry_linkify(mtree->resolver, &entry, &sparse_entry);
	} else if (parsed_kws & MTREE_HAS_OPTIONAL) {
		/*
		 * Couldn't open the entry, stat it or the on-disk type
		 * didn't match.  If this entry is optional, just ignore it
		 * and read the next header entry.
		 */
		*use_next = 1;
		return ARCHIVE_OK;
	}

	mtree->cur_size = archive_entry_size(entry);
	mtree->offset = 0;

	return r;
}

/*
 * Each line contains a sequence of keywords.
 */
static int
parse_line(struct archive_read *a, struct archive_entry *entry,
    struct mtree *mtree, struct mtree_entry *mp, int *parsed_kws)
{
	struct mtree_option *iter;
	int r = ARCHIVE_OK, r1;

	for (iter = mp->options; iter != NULL; iter = iter->next) {
		r1 = parse_keyword(a, mtree, entry, iter, parsed_kws);
		if (r1 < r)
			r = r1;
	}
	if ((*parsed_kws & MTREE_HAS_TYPE) == 0) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Missing type keyword in mtree specification");
		return (ARCHIVE_WARN);
	}
	return (r);
}

/*
 * Device entries have one of the following forms:
 * raw dev_t
 * format,major,minor[,subdevice]
 *
 * Just use major and minor, no translation etc is done
 * between formats.
 */
static int
parse_device(struct archive *a, struct archive_entry *entry, char *val)
{
	char *comma1, *comma2;

	comma1 = strchr(val, ',');
	if (comma1 == NULL) {
		archive_entry_set_dev(entry, mtree_atol10(&val));
		return (ARCHIVE_OK);
	}
	++comma1;
	comma2 = strchr(comma1, ',');
	if (comma2 == NULL) {
		archive_set_error(a, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Malformed device attribute");
		return (ARCHIVE_WARN);
	}
	++comma2;
	archive_entry_set_rdevmajor(entry, mtree_atol(&comma1));
	archive_entry_set_rdevminor(entry, mtree_atol(&comma2));
	return (ARCHIVE_OK);
}

/*
 * Parse a single keyword and its value.
 */
static int
parse_keyword(struct archive_read *a, struct mtree *mtree,
    struct archive_entry *entry, struct mtree_option *option, int *parsed_kws)
{
	char *val, *key;

	key = option->value;

	if (*key == '\0')
		return (ARCHIVE_OK);

	if (strcmp(key, "optional") == 0) {
		*parsed_kws |= MTREE_HAS_OPTIONAL;
		return (ARCHIVE_OK);
	}
	if (strcmp(key, "ignore") == 0) {
		/*
		 * The mtree processing is not recursive, so
		 * recursion will only happen for explicitly listed
		 * entries.
		 */
		return (ARCHIVE_OK);
	}

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
		if (strcmp(key, "content") == 0
		    || strcmp(key, "contents") == 0) {
			parse_escapes(val, NULL);
			archive_strcpy(&mtree->contents_name, val);
			break;
		}
		if (strcmp(key, "cksum") == 0)
			break;
	case 'd':
		if (strcmp(key, "device") == 0) {
			*parsed_kws |= MTREE_HAS_DEVICE;
			return parse_device(&a->archive, entry, val);
		}
	case 'f':
		if (strcmp(key, "flags") == 0) {
			*parsed_kws |= MTREE_HAS_FFLAGS;
			archive_entry_copy_fflags_text(entry, val);
			break;
		}
	case 'g':
		if (strcmp(key, "gid") == 0) {
			*parsed_kws |= MTREE_HAS_GID;
			archive_entry_set_gid(entry, mtree_atol10(&val));
			break;
		}
		if (strcmp(key, "gname") == 0) {
			*parsed_kws |= MTREE_HAS_GNAME;
			archive_entry_copy_gname(entry, val);
			break;
		}
	case 'l':
		if (strcmp(key, "link") == 0) {
			archive_entry_copy_symlink(entry, val);
			break;
		}
	case 'm':
		if (strcmp(key, "md5") == 0 || strcmp(key, "md5digest") == 0)
			break;
		if (strcmp(key, "mode") == 0) {
			if (val[0] >= '0' && val[0] <= '9') {
				*parsed_kws |= MTREE_HAS_PERM;
				archive_entry_set_perm(entry,
				    mtree_atol8(&val));
			} else {
				archive_set_error(&a->archive,
				    ARCHIVE_ERRNO_FILE_FORMAT,
				    "Symbolic mode \"%s\" unsupported", val);
				return ARCHIVE_WARN;
			}
			break;
		}
	case 'n':
		if (strcmp(key, "nlink") == 0) {
			*parsed_kws |= MTREE_HAS_NLINK;
			archive_entry_set_nlink(entry, mtree_atol10(&val));
			break;
		}
	case 'r':
		if (strcmp(key, "rmd160") == 0 ||
		    strcmp(key, "rmd160digest") == 0)
			break;
	case 's':
		if (strcmp(key, "sha1") == 0 || strcmp(key, "sha1digest") == 0)
			break;
		if (strcmp(key, "sha256") == 0 ||
		    strcmp(key, "sha256digest") == 0)
			break;
		if (strcmp(key, "sha384") == 0 ||
		    strcmp(key, "sha384digest") == 0)
			break;
		if (strcmp(key, "sha512") == 0 ||
		    strcmp(key, "sha512digest") == 0)
			break;
		if (strcmp(key, "size") == 0) {
			archive_entry_set_size(entry, mtree_atol10(&val));
			break;
		}
	case 't':
		if (strcmp(key, "tags") == 0) {
			/*
			 * Comma delimited list of tags.
			 * Ignore the tags for now, but the interface
			 * should be extended to allow inclusion/exclusion.
			 */
			break;
		}
		if (strcmp(key, "time") == 0) {
			*parsed_kws |= MTREE_HAS_MTIME;
			archive_entry_set_mtime(entry, mtree_atol10(&val), 0);
			break;
		}
		if (strcmp(key, "type") == 0) {
			*parsed_kws |= MTREE_HAS_TYPE;
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
	case 'u':
		if (strcmp(key, "uid") == 0) {
			*parsed_kws |= MTREE_HAS_UID;
			archive_entry_set_uid(entry, mtree_atol10(&val));
			break;
		}
		if (strcmp(key, "uname") == 0) {
			*parsed_kws |= MTREE_HAS_UNAME;
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
	size_t bytes_to_read;
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
		return (ARCHIVE_FATAL);
	}

	*buff = mtree->buff;
	*offset = mtree->offset;
	if ((off_t)mtree->buffsize > mtree->cur_size - mtree->offset)
		bytes_to_read = mtree->cur_size - mtree->offset;
	else
		bytes_to_read = mtree->buffsize;
	bytes_read = read(mtree->fd, mtree->buff, bytes_to_read);
	if (bytes_read < 0) {
		archive_set_error(&a->archive, errno, "Can't read");
		return (ARCHIVE_WARN);
	}
	if (bytes_read == 0) {
		*size = 0;
		return (ARCHIVE_EOF);
	}
	mtree->offset += bytes_read;
	*size = bytes_read;
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
 * Since parsing backslash sequences always makes strings shorter,
 * we can always do this conversion in-place.
 */
static void
parse_escapes(char *src, struct mtree_entry *mentry)
{
	char *dest = src;
	char c;

	/*
	 * The current directory is somewhat special, it should be archived
	 * only once as it will confuse extraction otherwise.
	 */
	if (strcmp(src, ".") == 0)
		mentry->full = 1;

	while (*src != '\0') {
		c = *src++;
		if (c == '/' && mentry != NULL)
			mentry->full = 1;
		if (c == '\\') {
			switch (src[0]) {
			case '0':
				if (src[1] < '0' || src[1] > '7') {
					c = 0;
					++src;
					break;
				}
				/* FALLTHROUGH */
			case '1':
			case '2':
			case '3':
				if (src[1] >= '0' && src[1] <= '7' &&
				    src[2] >= '0' && src[2] <= '7') {
					c = (src[0] - '0') << 6;
					c |= (src[1] - '0') << 3;
					c |= (src[2] - '0');
					src += 3;
				}
				break;
			case 'a':
				c = '\a';
				++src;
				break;
			case 'b':
				c = '\b';
				++src;
				break;
			case 'f':
				c = '\f';
				++src;
				break;
			case 'n':
				c = '\n';
				++src;
				break;
			case 'r':
				c = '\r';
				++src;
				break;
			case 's':
				c = ' ';
				++src;
				break;
			case 't':
				c = '\t';
				++src;
				break;
			case 'v':
				c = '\v';
				++src;
				break;
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
 * Note that this implementation does not (and should not!) obey
 * locale settings; you cannot simply substitute strtol here, since
 * it does obey locale.
 */
static int64_t
mtree_atol16(char **p)
{
	int64_t l, limit, last_digit_limit;
	int base, digit, sign;

	base = 16;
	limit = INT64_MAX / base;
	last_digit_limit = INT64_MAX % base;

	if (**p == '-') {
		sign = -1;
		++(*p);
	} else
		sign = 1;

	l = 0;
	if (**p >= '0' && **p <= '9')
		digit = **p - '0';
	else if (**p >= 'a' && **p <= 'f')
		digit = **p - 'a' + 10;
	else if (**p >= 'A' && **p <= 'F')
		digit = **p - 'A' + 10;
	else
		digit = -1;
	while (digit >= 0 && digit < base) {
		if (l > limit || (l == limit && digit > last_digit_limit)) {
			l = UINT64_MAX; /* Truncate on overflow. */
			break;
		}
		l = (l * base) + digit;
		if (**p >= '0' && **p <= '9')
			digit = **p - '0';
		else if (**p >= 'a' && **p <= 'f')
			digit = **p - 'a' + 10;
		else if (**p >= 'A' && **p <= 'F')
			digit = **p - 'A' + 10;
		else
			digit = -1;
	}
	return (sign < 0) ? -l : l;
}

static int64_t
mtree_atol(char **p)
{
	if (**p != '0')
		return mtree_atol10(p);
	if ((*p)[1] == 'x' || (*p)[1] == 'X') {
		*p += 2;
		return mtree_atol16(p);
	}
	return mtree_atol8(p);
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
	char *u;

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
		/* If we found an unescaped '\n', clean up and return. */
		if (p == NULL)
			continue;
		for (u = mtree->line.s; *u; ++u) {
			if (u[0] == '\n') {
				*start = mtree->line.s;
				return total_size;
			}
			if (u[0] == '#') {
				if (p == NULL)
					break;
				*start = mtree->line.s;
				return total_size;
			}
			if (u[0] != '\\')
				continue;
			if (u[1] == '\\') {
				++u;
				continue;
			}
			if (u[1] == '\n') {
				memmove(u, u + 1,
				    total_size - (u - mtree->line.s) + 1);
				--total_size;
				continue;    
			}
		}
	}
}
