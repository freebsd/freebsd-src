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

#include "archive_platform.h"
__FBSDID("$FreeBSD$");

#include <sys/stat.h>
#include <sys/types.h>
#ifdef HAVE_POSIX_ACL
#include <sys/acl.h>
#endif
#include <sys/time.h>

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef LINUX
#include <ext2fs/ext2_fs.h>
#include <sys/ioctl.h>
#endif

#include "archive.h"
#include "archive_string.h"
#include "archive_entry.h"
#include "archive_private.h"

struct fixup_entry {
	struct fixup_entry	*next;
	mode_t			 mode;
	int64_t			 mtime;
	int64_t			 atime;
	unsigned long		 mtime_nanos;
	unsigned long		 atime_nanos;
	unsigned long		 fflags_set;
	int			 fixup; /* bitmask of what needs fixing */
	char			*name;
};

#define	FIXUP_MODE	1
#define	FIXUP_TIMES	2
#define	FIXUP_FFLAGS	4

struct extract {
	struct archive_string	 mkdirpath;
	struct fixup_entry	*fixup_list;
};

/* Default mode for dirs created automatically. */
#define DEFAULT_DIR_MODE 0777
/*
 * Mode to use for newly-created dirs during extraction; the correct
 * mode will be set at the end of the extraction.
 */
#define SECURE_DIR_MODE 0700

static void	archive_extract_cleanup(struct archive *);
static int	archive_read_extract_block_device(struct archive *,
		    struct archive_entry *, int);
static int	archive_read_extract_char_device(struct archive *,
		    struct archive_entry *, int);
static int	archive_read_extract_device(struct archive *,
		    struct archive_entry *, int flags, mode_t mode);
static int	archive_read_extract_dir(struct archive *,
		    struct archive_entry *, int);
static int	archive_read_extract_fifo(struct archive *,
		    struct archive_entry *, int);
static int	archive_read_extract_hard_link(struct archive *,
		    struct archive_entry *, int);
static int	archive_read_extract_regular(struct archive *,
		    struct archive_entry *, int);
static int	archive_read_extract_symbolic_link(struct archive *,
		    struct archive_entry *, int);
static gid_t	lookup_gid(struct archive *, const char *uname, gid_t);
static uid_t	lookup_uid(struct archive *, const char *uname, uid_t);
static int	mkdirpath(struct archive *, const char *);
static int	mkdirpath_recursive(struct archive *, char *,
		    const struct stat *, mode_t, int);
#ifdef HAVE_POSIX_ACL
static int	set_acl(struct archive *, struct archive_entry *,
		    acl_type_t, int archive_entry_acl_type, const char *tn);
#endif
static int	set_acls(struct archive *, struct archive_entry *);
static int	set_extended_perm(struct archive *, struct archive_entry *,
		    int flags);
static int	set_fflags(struct archive *, const char *name, mode_t mode,
		    unsigned long fflags_set, unsigned long fflags_clear);
static int	set_ownership(struct archive *, struct archive_entry *, int);
static int	set_perm(struct archive *, struct archive_entry *, int mode,
		    int flags);
static int	set_time(struct archive *, struct archive_entry *, int);
static struct fixup_entry *sort_dir_list(struct fixup_entry *p);


/*
 * Extract this entry to disk.
 *
 * TODO: Validate hardlinks.  According to the standards, we're
 * supposed to check each extracted hardlink and squawk if it refers
 * to a file that we didn't restore.  I'm not entirely convinced this
 * is a good idea, but more importantly: Is there any way to validate
 * hardlinks without keeping a complete list of filenames from the
 * entire archive?? Ugh.
 *
 */
int
archive_read_extract(struct archive *a, struct archive_entry *entry, int flags)
{
	mode_t mode;
	struct extract *extract;
	int ret;
	int restore_pwd;

	if (a->extract == NULL) {
		a->extract = malloc(sizeof(*a->extract));
		if (a->extract == NULL) {
			archive_set_error(a, ENOMEM, "Can't extract");
			return (ARCHIVE_FATAL);
		}
		a->cleanup_archive_extract = archive_extract_cleanup;
		memset(a->extract, 0, sizeof(*a->extract));
	}
	extract = a->extract;

	restore_pwd = -1;

	if (archive_entry_hardlink(entry) != NULL)
		return (archive_read_extract_hard_link(a, entry, flags));

	/*
	 * TODO: If pathname is longer than PATH_MAX, record starting
	 * directory and move to a suitable intermediate dir, which
	 * might require creating them!
	 */
	if (strlen(archive_entry_pathname(entry)) > PATH_MAX) {
		restore_pwd = open(".", O_RDONLY);
		/* XXX chdir() to a suitable intermediate dir XXX */
		/* XXX Update pathname in 'entry' XXX */
	}

	mode = archive_entry_mode(entry);
	switch (mode & S_IFMT) {
	default:
		/* Fall through, as required by POSIX. */
	case S_IFREG:
		ret =  archive_read_extract_regular(a, entry, flags);
		break;
	case S_IFLNK:	/* Symlink */
		ret =  archive_read_extract_symbolic_link(a, entry, flags);
		break;
	case S_IFCHR:
		ret =  archive_read_extract_char_device(a, entry, flags);
		break;
	case S_IFBLK:
		ret =  archive_read_extract_block_device(a, entry, flags);
		break;
	case S_IFDIR:
		ret =  archive_read_extract_dir(a, entry, flags);
		break;
	case S_IFIFO:
		ret =  archive_read_extract_fifo(a, entry, flags);
		break;
	}

	/* If we changed directory above, restore it here. */
	if (restore_pwd >= 0)
		fchdir(restore_pwd);

	return (ret);
}

/*
 * Cleanup function for archive_extract.  Free name/mode list and
 * restore permissions and dir timestamps.  This must be done last;
 * otherwise, the dir permission might prevent us from restoring a
 * file.  Similarly, the act of restoring a file touches the directory
 * and changes the timestamp on the dir, so we have to touch-up the
 * timestamps at the end as well.  Note that tar/cpio do not require
 * that archives be in a particular order; there is no way to know
 * when the last file has been restored within a directory, so there's
 * no way to optimize the memory usage here by fixing up the directory
 * any earlier than the end-of-archive.
 *
 * XXX TODO: Directory ACLs should be restored here, for the same
 * reason we set directory perms here. XXX
 *
 * Registering this function (rather than calling it explicitly by
 * name from archive_read_finish) reduces static link pollution, since
 * applications that don't use this API won't get this file linked in.
 */
static
void archive_extract_cleanup(struct archive *a)
{
	struct fixup_entry *next, *p;
	struct extract *extract;
	mode_t mask;

	/* Sort dir list so directories are fixed up in depth-first order. */
	extract = a->extract;
	p = sort_dir_list(extract->fixup_list);
	umask(mask = umask(0)); /* Read the current umask. */

	while (p != NULL) {
		if (p->fixup & FIXUP_TIMES) {
			struct timeval times[2];
			times[1].tv_sec = p->mtime;
			times[1].tv_usec = p->mtime_nanos / 1000;
			times[0].tv_sec = p->atime;
			times[0].tv_usec = p->atime_nanos / 1000;
			utimes(p->name, times);
		}
		if (p->fixup & FIXUP_MODE)
			chmod(p->name, p->mode & ~mask);

		if (p->fixup & FIXUP_FFLAGS)
			set_fflags(a, p->name, p->mode, p->fflags_set, 0);

		next = p->next;
		free(p->name);
		free(p);
		p = next;
	}
	extract->fixup_list = NULL;
	archive_string_free(&extract->mkdirpath);
	free(a->extract);
	a->extract = NULL;
}

/*
 * Simple O(n log n) merge sort to order the fixup list.  In
 * particular, we want to restore dir timestamps depth-first.
 */
static struct fixup_entry *
sort_dir_list(struct fixup_entry *p)
{
	struct fixup_entry *a, *b, *t;

	if (p == NULL)
		return (NULL);
	/* A one-item list is already sorted. */
	if (p->next == NULL)
		return (p);

	/* Step 1: split the list. */
	t = p;
	a = p->next->next;
	while (a != NULL) {
		/* Step a twice, t once. */
		a = a->next;
		if (a != NULL)
			a = a->next;
		t = t->next;
	}
	/* Now, t is at the mid-point, so break the list here. */
	b = t->next;
	t->next = NULL;
	a = p;

	/* Step 2: Recursively sort the two sub-lists. */
	a = sort_dir_list(a);
	b = sort_dir_list(b);

	/* Step 3: Merge the returned lists. */
	/* Pick the first element for the merged list. */
	if (strcmp(a->name, b->name) > 0) {
		t = p = a;
		a = a->next;
	} else {
		t = p = b;
		b = b->next;
	}

	/* Always put the later element on the list first. */
	while (a != NULL && b != NULL) {
		if (strcmp(a->name, b->name) > 0) {
			t->next = a;
			a = a->next;
		} else {
			t->next = b;
			b = b->next;
		}
		t = t->next;
	}

	/* Only one list is non-empty, so just splice it on. */
	if (a != NULL)
		t->next = a;
	if (b != NULL)
		t->next = b;

	return (p);
}

static int
archive_read_extract_regular(struct archive *a, struct archive_entry *entry,
    int flags)
{
	const char *name;
	mode_t mode;
	int fd, r;

	name = archive_entry_pathname(entry);
	mode = archive_entry_mode(entry);
	r = ARCHIVE_OK;

	/*
	 * If we're not supposed to overwrite pre-existing files,
	 * use O_EXCL.  Otherwise, use O_TRUNC.
	 */
	if (flags & (ARCHIVE_EXTRACT_UNLINK | ARCHIVE_EXTRACT_NO_OVERWRITE))
		fd = open(name, O_WRONLY | O_CREAT | O_EXCL, mode);
	else
		fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, mode);

	/* Try removing a pre-existing file. */
	if (fd < 0 && !(flags & ARCHIVE_EXTRACT_NO_OVERWRITE)) {
		unlink(name);
		fd = open(name, O_WRONLY | O_CREAT | O_EXCL, mode);
	}

	/* Might be a non-existent parent dir; try fixing that. */
	if (fd < 0) {
		mkdirpath(a, name);
		fd = open(name, O_WRONLY | O_CREAT | O_EXCL, mode);
	}
	if (fd < 0) {
		archive_set_error(a, errno, "Can't open '%s'", name);
		return (ARCHIVE_WARN);
	}
	r = archive_read_data_into_fd(a, fd);
	set_ownership(a, entry, flags);
	set_time(a, entry, flags);
	/* Always reset permissions for regular files. */
	set_perm(a, entry, archive_entry_mode(entry),
	    flags | ARCHIVE_EXTRACT_PERM);
	set_extended_perm(a, entry, flags);
	close(fd);
	return (r);
}

static int
archive_read_extract_dir(struct archive *a, struct archive_entry *entry,
    int flags)
{
	struct extract *extract;
	const struct stat *st;
	char *p;
	int ret, ret2;
	size_t len;

	extract = a->extract;
	st = archive_entry_stat(entry);

	/* Copy path to mutable storage. */
	archive_strcpy(&(extract->mkdirpath), archive_entry_pathname(entry));
	p = extract->mkdirpath.s;
	len = strlen(p);
	if (len > 2 && p[len - 1] == '.' && p[len - 2] == '/')
		p[--len] = '\0'; /* Remove trailing "/." */
	if (len > 2 && p[len - 1] == '/')
		p[--len] = '\0'; /* Remove trailing "/" */
	/* Recursively try to build the path. */
	if (mkdirpath_recursive(a, p, st, st->st_mode, flags))
		return (ARCHIVE_WARN);
	set_ownership(a, entry, flags);
	ret = set_perm(a, entry, 0700, flags);
	ret2 = set_extended_perm(a, entry, flags);
	return (err_combine(ret, ret2));
}


/*
 * Convenience form.
 */
static int
mkdirpath(struct archive *a, const char *path)
{
	struct extract *extract;
	char *p;

	extract = a->extract;

	/* Copy path to mutable storage. */
	archive_strcpy(&(extract->mkdirpath), path);
	p = extract->mkdirpath.s;
	p = strrchr(extract->mkdirpath.s, '/');
	if (p == NULL)
		return (ARCHIVE_OK);
	*p = '\0';

	/* Recursively try to build the path. */
	if (mkdirpath_recursive(a, extract->mkdirpath.s,
	    NULL, DEFAULT_DIR_MODE, 0))
		return (ARCHIVE_WARN);
	return (ARCHIVE_OK);
}

/*
 * Returns 0 if it successfully created necessary directories.
 * Otherwise, returns ARCHIVE_WARN.
 */
static int
mkdirpath_recursive(struct archive *a, char *path, const struct stat *st,
    mode_t mode, int flags)
{
	struct stat sb;
	struct extract *extract;
	struct fixup_entry *le;
	char *p;
	mode_t writable_mode = SECURE_DIR_MODE;
	int r;

	extract = a->extract;

	if (path[0] == '.' && path[1] == 0)
		return (ARCHIVE_OK);

	if (mode != writable_mode ||
	    (st != NULL && (flags & ARCHIVE_EXTRACT_TIME))) {
		/* Add this dir to the fixup list. */
		le = malloc(sizeof(struct fixup_entry));
		le->fixup = 0;
		le->next = extract->fixup_list;
		extract->fixup_list = le;
		le->name = strdup(path);

		if (mode != writable_mode) {
			le->mode = mode;
			le->fixup |= FIXUP_MODE;
			mode = writable_mode;
		}
		if (flags & ARCHIVE_EXTRACT_TIME) {
			le->mtime = st->st_mtime;
			le->mtime_nanos = ARCHIVE_STAT_MTIME_NANOS(st);
			le->atime = st->st_atime;
			le->atime_nanos = ARCHIVE_STAT_ATIME_NANOS(st);
			le->fixup |= FIXUP_TIMES;
		}
	}

	if (mkdir(path, mode) == 0)
		return (ARCHIVE_OK);
	/*
	 * Do "unlink first" after.  The preceding syscall will always
	 * fail if something already exists, so we save a little time
	 * in the common case by not trying to unlink until we know
	 * something is there.
	 */
	if ((flags & ARCHIVE_EXTRACT_UNLINK))
		unlink(path);
	/*
	 * Yes, this should be stat() and not lstat().  Using lstat()
	 * here loses the ability to extract through symlinks.  If
	 * clients don't want to extract through symlinks, they should
	 * specify ARCHIVE_EXTRACT_UNLINK.
	 */
	if (stat(path, &sb) == 0) {
		/* Already exists! */
		if (S_ISDIR(sb.st_mode))
			return (ARCHIVE_OK);
		if ((flags & ARCHIVE_EXTRACT_NO_OVERWRITE)) {
			archive_set_error(a, EEXIST,
			    "Can't create directory '%s'", path);
			return (ARCHIVE_WARN);
		}
		/* Not a dir: remove it and create a directory. */
		if (unlink(path) == 0 &&
		    mkdir(path, mode) == 0)
			return (ARCHIVE_OK);
	} else if (errno != ENOENT) {
		/* Stat failed? */
		archive_set_error(a, errno, "Can't test directory '%s'", path);
		return (ARCHIVE_WARN);
	}

	/* Doesn't exist: try creating parent dir. */
	p = strrchr(path, '/');
	if (p != NULL) {
		*p = '\0';	/* Terminate path name. */
		r = mkdirpath_recursive(a, path, NULL, DEFAULT_DIR_MODE, 0);
		*p = '/';	/* Restore the '/' we just overwrote. */
		if (r != ARCHIVE_OK)
			return (r);
		if (mkdir(path, mode) == 0)
			return (ARCHIVE_OK);
	}
	archive_set_error(a, errno, "Failed to create dir '%s'", path);
	return (ARCHIVE_WARN);
}

static int
archive_read_extract_hard_link(struct archive *a, struct archive_entry *entry,
    int flags)
{
	int r;
	const char *pathname;
	const char *linkname;

	pathname = archive_entry_pathname(entry);
	linkname = archive_entry_hardlink(entry);

	/* Just remove any pre-existing file with this name. */
	if (!(flags & ARCHIVE_EXTRACT_NO_OVERWRITE))
		unlink(pathname);

	r = link(linkname, pathname);

	if (r != 0) {
		/* Might be a non-existent parent dir; try fixing that. */
		mkdirpath(a, pathname);
		r = link(linkname, pathname);
	}

	if (r != 0) {
		/* XXX Better error message here XXX */
		archive_set_error(a, errno,
		    "Can't restore hardlink to '%s'", linkname);
		return (ARCHIVE_WARN);
	}

	/* Set ownership, time, permission information. */
	set_ownership(a, entry, flags);
	set_time(a, entry, flags);
	set_perm(a, entry, archive_entry_stat(entry)->st_mode, flags);
	set_extended_perm(a, entry, flags);

	return (ARCHIVE_OK);
}

static int
archive_read_extract_symbolic_link(struct archive *a,
    struct archive_entry *entry, int flags)
{
	int r;
	const char *pathname;
	const char *linkname;

	pathname = archive_entry_pathname(entry);
	linkname = archive_entry_symlink(entry);

	/* Just remove any pre-existing file with this name. */
	if (!(flags & ARCHIVE_EXTRACT_NO_OVERWRITE))
		unlink(pathname);

	r = symlink(linkname, pathname);

	if (r != 0) {
		/* Might be a non-existent parent dir; try fixing that. */
		mkdirpath(a, pathname);
		r = symlink(linkname, pathname);
	}

	if (r != 0) {
		/* XXX Better error message here XXX */
		archive_set_error(a, errno,
		    "Can't restore symlink to '%s'", linkname);
		return (ARCHIVE_WARN);
	}

	/* Set ownership, time, permission information. */
	set_ownership(a, entry, flags);
	set_time(a, entry, flags);
	set_perm(a, entry, archive_entry_stat(entry)->st_mode, flags);
	set_extended_perm(a, entry, flags);

	return (ARCHIVE_OK);
}

static int
archive_read_extract_device(struct archive *a, struct archive_entry *entry,
    int flags, mode_t mode)
{
	int r;

	/* Just remove any pre-existing file with this name. */
	if (!(flags & ARCHIVE_EXTRACT_NO_OVERWRITE))
		unlink(archive_entry_pathname(entry));

	r = mknod(archive_entry_pathname(entry), mode,
	    archive_entry_stat(entry)->st_rdev);

	/* Might be a non-existent parent dir; try fixing that. */
	if (r != 0 && errno == ENOENT) {
		mkdirpath(a, archive_entry_pathname(entry));
		r = mknod(archive_entry_pathname(entry), mode,
		    archive_entry_stat(entry)->st_rdev);
	}

	if (r != 0) {
		archive_set_error(a, errno, "Can't recreate device node");
		return (ARCHIVE_WARN);
	}

	/* Set ownership, time, permission information. */
	set_ownership(a, entry, flags);
	set_time(a, entry, flags);
	set_perm(a, entry, archive_entry_stat(entry)->st_mode, flags);
	set_extended_perm(a, entry, flags);

	return (ARCHIVE_OK);
}

static int
archive_read_extract_char_device(struct archive *a,
    struct archive_entry *entry, int flags)
{
	mode_t mode;

	mode = (archive_entry_stat(entry)->st_mode & ~S_IFMT) | S_IFCHR;
	return (archive_read_extract_device(a, entry, flags, mode));
}

static int
archive_read_extract_block_device(struct archive *a,
    struct archive_entry *entry, int flags)
{
	mode_t mode;

	mode = (archive_entry_stat(entry)->st_mode & ~S_IFMT) | S_IFBLK;
	return (archive_read_extract_device(a, entry, flags, mode));
}

static int
archive_read_extract_fifo(struct archive *a,
    struct archive_entry *entry, int flags)
{
	int r;

	/* Just remove any pre-existing file with this name. */
	if (!(flags & ARCHIVE_EXTRACT_NO_OVERWRITE))
		unlink(archive_entry_pathname(entry));

	r = mkfifo(archive_entry_pathname(entry),
	    archive_entry_stat(entry)->st_mode);

	/* Might be a non-existent parent dir; try fixing that. */
	if (r != 0 && errno == ENOENT) {
		mkdirpath(a, archive_entry_pathname(entry));
		r = mkfifo(archive_entry_pathname(entry),
		    archive_entry_stat(entry)->st_mode);
	}

	if (r != 0) {
		archive_set_error(a, errno, "Can't restore fifo");
		return (ARCHIVE_WARN);
	}

	/* Set ownership, time, permission information. */
	set_ownership(a, entry, flags);
	set_time(a, entry, flags);
	/* Done by mkfifo. */
	/* set_perm(a, entry, archive_entry_stat(entry)->st_mode, flags); */
	set_extended_perm(a, entry, flags);

	return (ARCHIVE_OK);
}

/*
 * Returns 0 if UID/GID successfully restored; ARCHIVE_WARN otherwise.
 */
static int
set_ownership(struct archive *a, struct archive_entry *entry, int flags)
{
	uid_t uid;
	gid_t gid;

	/* Not changed. */
	if ((flags & ARCHIVE_EXTRACT_OWNER) == 0)
		return (ARCHIVE_WARN);

	uid = lookup_uid(a, archive_entry_uname(entry),
	    archive_entry_stat(entry)->st_uid);
	gid = lookup_gid(a, archive_entry_gname(entry),
	    archive_entry_stat(entry)->st_gid);

	/*
	 * Root can change owner/group; owner can change group;
	 * otherwise, bail out now.
	 */
	if (a->user_uid != 0  &&  a->user_uid != uid) {
		/* XXXX archive_set_error( XXXX ) ; XXX */
		return (ARCHIVE_WARN);
	}

	if (lchown(archive_entry_pathname(entry), uid, gid)) {
		archive_set_error(a, errno,
		    "Can't set user=%d/group=%d for %s", uid, gid,
		    archive_entry_pathname(entry));
		return (ARCHIVE_WARN);
	}
	return (ARCHIVE_OK);
}

static int
set_time(struct archive *a, struct archive_entry *entry, int flags)
{
	const struct stat *st;
	struct timeval times[2];

	(void)a; /* UNUSED */
	st = archive_entry_stat(entry);

	if ((flags & ARCHIVE_EXTRACT_TIME) == 0)
		return (ARCHIVE_OK);

	times[1].tv_sec = st->st_mtime;
	times[1].tv_usec = ARCHIVE_STAT_MTIME_NANOS(st) / 1000;

	times[0].tv_sec = st->st_atime;
	times[0].tv_usec = ARCHIVE_STAT_ATIME_NANOS(st) / 1000;

#ifdef HAVE_LUTIMES
	if (lutimes(archive_entry_pathname(entry), times) != 0) {
#else
	if ((archive_entry_mode(entry) & S_IFMT) != S_IFLNK &&
	    utimes(archive_entry_pathname(entry), times) != 0) {
#endif
		archive_set_error(a, errno, "Can't update time for %s",
		    archive_entry_pathname(entry));
		return (ARCHIVE_WARN);
	}

	/*
	 * Note: POSIX does not provide a portable way to restore ctime.
	 * (Apart from resetting the system clock, which is distasteful.)
	 * So, any restoration of ctime will necessarily be OS-specific.
	 */

	/* XXX TODO: Can FreeBSD restore ctime? XXX */

	return (ARCHIVE_OK);
}

static int
set_perm(struct archive *a, struct archive_entry *entry, int mode, int flags)
{
	const char *name;

	if ((flags & ARCHIVE_EXTRACT_PERM) == 0)
		return (ARCHIVE_OK);

	name = archive_entry_pathname(entry);
#ifdef HAVE_LCHMOD
	if (lchmod(name, mode) != 0) {
#else
	if ((archive_entry_mode(entry) & S_IFMT) != S_IFLNK &&
	    chmod(name, mode) != 0) {
#endif
		archive_set_error(a, errno, "Can't set permissions");
		return (ARCHIVE_WARN);
	}
	return (0);
}

static int
set_extended_perm(struct archive *a, struct archive_entry *entry, int flags)
{
	struct fixup_entry *le;
	struct extract	*extract;
	unsigned long	 set, clear;
	int		 ret, ret2;
	int		 critical_flags;

	extract = a->extract;

	/*
	 * Make 'critical_flags' hold all file flags that can't be
	 * immediately restored.  For example, on BSD systems,
	 * SF_IMMUTABLE prevents hardlinks from being created, so
	 * should not be set until after any hardlinks are created.  To
	 * preserve some semblance of portability, this uses #ifdef
	 * extensively.  Ugly, but it works.
	 *
	 * Yes, Virginia, this does create a barn-door-sized security
	 * race.  If you see any way to avoid it, please let me know.
	 * People restoring critical file systems should be wary of
	 * other programs that might try to muck with files as they're
	 * being restored.
	 */
	/* Hopefully, the compiler will optimize this mess into a constant. */
	critical_flags = 0;
#ifdef SF_IMMUTABLE
	critical_flags |= SF_IMMUTABLE;
#endif
#ifdef UF_IMMUTABLE
	critical_flags |= UF_IMMUTABLE;
#endif
#ifdef SF_APPEND
	critical_flags |= SF_APPEND;
#endif
#ifdef UF_APPEND
	critical_flags |= UF_APPEND;
#endif
#ifdef EXT2_APPEND_FL
	critical_flags |= EXT2_APPEND_FL;
#endif
#ifdef EXT2_IMMUTABLE_FL
	critical_flags |= EXT2_IMMUTABLE_FL;
#endif

	if ((flags & ARCHIVE_EXTRACT_PERM) == 0)
		return (ARCHIVE_OK);

	archive_entry_fflags(entry, &set, &clear);

	/*
	 * The first test encourages the compiler to eliminate all of
	 * this if it's not necessary.
	 */
	if ((critical_flags != 0)  &&  (set & critical_flags)) {
		le = malloc(sizeof(struct fixup_entry));
		le->fixup = FIXUP_FFLAGS;
		le->next = extract->fixup_list;
		extract->fixup_list = le;
		le->name = strdup(archive_entry_pathname(entry));
		le->mode = archive_entry_mode(entry);
		le->fflags_set = set;
		ret = ARCHIVE_OK;
	} else
		ret = set_fflags(a, archive_entry_pathname(entry),
		    archive_entry_mode(entry), set, clear);

	ret2 = set_acls(a, entry);

	return (err_combine(ret,ret2));
}

static int
set_fflags(struct archive *a, const char *name, mode_t mode,
    unsigned long set, unsigned long clear)
{
	int		 ret;
	struct stat	 st;
#ifdef LINUX
	int		 fd;
	int		 err;
	unsigned long newflags, oldflags;
#endif

	ret = ARCHIVE_OK;
	if (set == 0  && clear == 0)
		return (ret);

#ifdef HAVE_CHFLAGS
	(void)mode; /* UNUSED */
	/*
	 * XXX Is the stat here really necessary?  Or can I just use
	 * the 'set' flags directly?  In particular, I'm not sure
	 * about the correct approach if we're overwriting an existing
	 * file that already has flags on it. XXX
	 */
	if (stat(name, &st) == 0) {
		st.st_flags &= ~clear;
		st.st_flags |= set;
		if (chflags(name, st.st_flags) != 0) {
			archive_set_error(a, errno,
			    "Failed to set file flags");
			ret = ARCHIVE_WARN;
		}
	}
#endif
	/* Linux has flags too, but no chflags syscall */
#ifdef LINUX
	/*
	 * Linux has no define for the flags that are only settable
	 * by the root user...
	 */
#define	SF_MASK                 (EXT2_IMMUTABLE_FL|EXT2_APPEND_FL)

	/*
	 * XXX As above, this would be way simpler if we didn't have
	 * to read the current flags from disk. XXX
	 */
	if ((S_ISREG(mode) || S_ISDIR(mode)) &&
	    ((fd = open(name, O_RDONLY|O_NONBLOCK)) >= 0)) {
		err = 1;
		if (fd >= 0 && (ioctl(fd, EXT2_IOC_GETFLAGS, &oldflags) >= 0)) {
			newflags = (oldflags & ~clear) | set;
			if (ioctl(fd, EXT2_IOC_SETFLAGS, &newflags) >= 0) {
				err = 0;
			} else if (errno == EPERM) {
				if (ioctl(fd, EXT2_IOC_GETFLAGS, &oldflags) >= 0) {
					newflags &= ~SF_MASK;
					oldflags &= SF_MASK;
					newflags |= oldflags;
					if (ioctl(fd, EXT2_IOC_SETFLAGS, &newflags) >= 0)
						err = 0;
				}
			}
		}
		close(fd);
		if (err) {
			archive_set_error(a, errno,
			    "Failed to set file flags");
			ret = ARCHIVE_WARN;
		}
	}
#endif

	return (ret);
}

#ifndef HAVE_POSIX_ACL
/* Default empty function body to satisfy mainline code. */
static int
set_acls(struct archive *a, struct archive_entry *entry)
{
	(void)a;
	(void)entry;

	return (ARCHIVE_OK);
}

#else

/*
 * XXX TODO: What about ACL types other than ACCESS and DEFAULT?
 */
static int
set_acls(struct archive *a, struct archive_entry *entry)
{
	int		 ret;

	ret = set_acl(a, entry, ACL_TYPE_ACCESS,
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS, "access");
	if (ret != ARCHIVE_OK)
		return (ret);
	ret = set_acl(a, entry, ACL_TYPE_DEFAULT,
	    ARCHIVE_ENTRY_ACL_TYPE_DEFAULT, "default");
	return (ret);
}


static int
set_acl(struct archive *a, struct archive_entry *entry, acl_type_t acl_type,
    int ae_requested_type, const char *typename)
{
	acl_t		 acl;
	acl_entry_t	 acl_entry;
	acl_permset_t	 acl_permset;
	int		 ret;
	int		 ae_type, ae_permset, ae_tag, ae_id;
	uid_t		 ae_uid;
	gid_t		 ae_gid;
	const char	*ae_name;
	int		 entries;
	const char	*name;

	ret = ARCHIVE_OK;
	entries = archive_entry_acl_reset(entry, ae_requested_type);
	if (entries == 0)
		return (ARCHIVE_OK);
	acl = acl_init(entries);
	while (archive_entry_acl_next(entry, ae_requested_type, &ae_type,
		   &ae_permset, &ae_tag, &ae_id, &ae_name) == ARCHIVE_OK) {
		acl_create_entry(&acl, &acl_entry);

		switch (ae_tag) {
		case ARCHIVE_ENTRY_ACL_USER:
			acl_set_tag_type(acl_entry, ACL_USER);
			ae_uid = lookup_uid(a, ae_name, ae_id);
			acl_set_qualifier(acl_entry, &ae_uid);
			break;
		case ARCHIVE_ENTRY_ACL_GROUP:
			acl_set_tag_type(acl_entry, ACL_GROUP);
			ae_gid = lookup_gid(a, ae_name, ae_id);
			acl_set_qualifier(acl_entry, &ae_gid);
			break;
		case ARCHIVE_ENTRY_ACL_USER_OBJ:
			acl_set_tag_type(acl_entry, ACL_USER_OBJ);
			break;
		case ARCHIVE_ENTRY_ACL_GROUP_OBJ:
			acl_set_tag_type(acl_entry, ACL_GROUP_OBJ);
			break;
		case ARCHIVE_ENTRY_ACL_MASK:
			acl_set_tag_type(acl_entry, ACL_MASK);
			break;
		case ARCHIVE_ENTRY_ACL_OTHER:
			acl_set_tag_type(acl_entry, ACL_OTHER);
			break;
		default:
			/* XXX */
			break;
		}

		acl_get_permset(acl_entry, &acl_permset);
		acl_clear_perms(acl_permset);
		if (ae_permset & ARCHIVE_ENTRY_ACL_EXECUTE)
			acl_add_perm(acl_permset, ACL_EXECUTE);
		if (ae_permset & ARCHIVE_ENTRY_ACL_WRITE)
			acl_add_perm(acl_permset, ACL_WRITE);
		if (ae_permset & ARCHIVE_ENTRY_ACL_READ)
			acl_add_perm(acl_permset, ACL_READ);
	}

	name = archive_entry_pathname(entry);

	if (acl_set_file(name, acl_type, acl) != 0) {
		archive_set_error(a, errno, "Failed to set %s acl", typename);
		ret = ARCHIVE_WARN;
	}
	acl_free(acl);
	return (ret);
}
#endif

/*
 * XXX The following gid/uid lookups can be a performance bottleneck.
 * Some form of caching would probably be very effective, though
 * I have concerns about staleness.
 */
static gid_t
lookup_gid(struct archive *a, const char *gname, gid_t gid)
{
	struct group	*grent;

	(void)a; /* UNUSED */

	/* Look up gid from gname. */
	if (gname != NULL  &&  *gname != '\0') {
		grent = getgrnam(gname);
		if (grent != NULL)
			gid = grent->gr_gid;
	}
	return (gid);
}

static uid_t
lookup_uid(struct archive *a, const char *uname, uid_t uid)
{
	struct passwd	*pwent;

	(void)a; /* UNUSED */

	/* Look up uid from uname. */
	if (uname != NULL  &&  *uname != '\0') {
		pwent = getpwnam(uname);
		if (pwent != NULL)
			uid = pwent->pw_uid;
	}
	return (uid);
}

void
archive_read_extract_set_progress_callback(struct archive *a,
    void (*progress_func)(void *), void *user_data)
{
	a->extract_progress = progress_func;
	a->extract_progress_user_data = user_data;
}
