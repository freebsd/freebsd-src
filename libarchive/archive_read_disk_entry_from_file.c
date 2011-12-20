/*-
 * Copyright (c) 2003-2009 Tim Kientzle
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
__FBSDID("$FreeBSD: head/lib/libarchive/archive_read_disk_entry_from_file.c 201084 2009-12-28 02:14:09Z kientzle $");

#ifdef HAVE_SYS_TYPES_H
/* Mac OSX requires sys/types.h before sys/acl.h. */
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_ACL_H
#include <sys/acl.h>
#endif
#ifdef HAVE_SYS_EXTATTR_H
#include <sys/extattr.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif
#ifdef HAVE_ACL_LIBACL_H
#include <acl/libacl.h>
#endif
#ifdef HAVE_ATTR_XATTR_H
#include <attr/xattr.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_read_disk_private.h"

/*
 * Linux and FreeBSD plug this obvious hole in POSIX.1e in
 * different ways.
 */
#if HAVE_ACL_GET_PERM
#define	ACL_GET_PERM acl_get_perm
#elif HAVE_ACL_GET_PERM_NP
#define	ACL_GET_PERM acl_get_perm_np
#endif

static int setup_acls_posix1e(struct archive_read_disk *,
    struct archive_entry *, int fd);
static int setup_xattrs(struct archive_read_disk *,
    struct archive_entry *, int fd);

int
archive_read_disk_entry_from_file(struct archive *_a,
    struct archive_entry *entry,
    int fd, const struct stat *st)
{
	struct archive_read_disk *a = (struct archive_read_disk *)_a;
	const char *path, *name;
	struct stat s;
	int initial_fd = fd;
	int r, r1;

	archive_clear_error(_a);
	path = archive_entry_sourcepath(entry);
	if (path == NULL)
		path = archive_entry_pathname(entry);

#ifdef EXT2_IOC_GETFLAGS
	/* Linux requires an extra ioctl to pull the flags.  Although
	 * this is an extra step, it has a nice side-effect: We get an
	 * open file descriptor which we can use in the subsequent lookups. */
	if ((S_ISREG(st->st_mode) || S_ISDIR(st->st_mode))) {
		if (fd < 0)
			fd = open(pathname, O_RDONLY | O_NONBLOCK | O_BINARY);
		if (fd >= 0) {
			unsigned long stflags;
			int r = ioctl(fd, EXT2_IOC_GETFLAGS, &stflags);
			if (r == 0 && stflags != 0)
				archive_entry_set_fflags(entry, stflags, 0);
		}
	}
#endif

	if (st == NULL) {
		/* TODO: On Windows, use GetFileInfoByHandle() here.
		 * Using Windows stat() call is badly broken, but
		 * even the stat() wrapper has problems because
		 * 'struct stat' is broken on Windows.
		 */
#if HAVE_FSTAT
		if (fd >= 0) {
			if (fstat(fd, &s) != 0) {
				archive_set_error(&a->archive, errno,
				    "Can't fstat");
				return (ARCHIVE_FAILED);
			}
		} else
#endif
#if HAVE_LSTAT
		if (!a->follow_symlinks) {
			if (lstat(path, &s) != 0) {
				archive_set_error(&a->archive, errno,
				    "Can't lstat %s", path);
				return (ARCHIVE_FAILED);
			}
		} else
#endif
		if (stat(path, &s) != 0) {
			archive_set_error(&a->archive, errno,
			    "Can't lstat %s", path);
			return (ARCHIVE_FAILED);
		}
		st = &s;
	}
	archive_entry_copy_stat(entry, st);

	/* Lookup uname/gname */
	name = archive_read_disk_uname(_a, archive_entry_uid(entry));
	if (name != NULL)
		archive_entry_copy_uname(entry, name);
	name = archive_read_disk_gname(_a, archive_entry_gid(entry));
	if (name != NULL)
		archive_entry_copy_gname(entry, name);

#ifdef HAVE_STRUCT_STAT_ST_FLAGS
	/* On FreeBSD, we get flags for free with the stat. */
	/* TODO: Does this belong in copy_stat()? */
	if (st->st_flags != 0)
		archive_entry_set_fflags(entry, st->st_flags, 0);
#endif

#ifdef HAVE_READLINK
	if (S_ISLNK(st->st_mode)) {
		char linkbuffer[PATH_MAX + 1];
		int lnklen = readlink(path, linkbuffer, PATH_MAX);
		if (lnklen < 0) {
			archive_set_error(&a->archive, errno,
			    "Couldn't read link data");
			return (ARCHIVE_FAILED);
		}
		linkbuffer[lnklen] = 0;
		archive_entry_set_symlink(entry, linkbuffer);
	}
#endif

	r = setup_acls_posix1e(a, entry, fd);
	r1 = setup_xattrs(a, entry, fd);
	if (r1 < r)
		r = r1;
	/* If we opened the file earlier in this function, close it. */
	if (initial_fd != fd)
		close(fd);
	return (r);
}

#ifdef HAVE_POSIX_ACL
static void setup_acl_posix1e(struct archive_read_disk *a,
    struct archive_entry *entry, acl_t acl, int archive_entry_acl_type);

static int
setup_acls_posix1e(struct archive_read_disk *a,
    struct archive_entry *entry, int fd)
{
	const char	*accpath;
	acl_t		 acl;

	accpath = archive_entry_sourcepath(entry);
	if (accpath == NULL)
		accpath = archive_entry_pathname(entry);

	archive_entry_acl_clear(entry);

	/* Retrieve access ACL from file. */
	if (fd >= 0)
		acl = acl_get_fd(fd);
#if HAVE_ACL_GET_LINK_NP
	else if (!a->follow_symlinks)
		acl = acl_get_link_np(accpath, ACL_TYPE_ACCESS);
#else
	else if ((!a->follow_symlinks)
	    && (archive_entry_filetype(entry) == AE_IFLNK))
		/* We can't get the ACL of a symlink, so we assume it can't
		   have one. */
		acl = NULL;
#endif
	else
		acl = acl_get_file(accpath, ACL_TYPE_ACCESS);
	if (acl != NULL) {
		setup_acl_posix1e(a, entry, acl,
		    ARCHIVE_ENTRY_ACL_TYPE_ACCESS);
		acl_free(acl);
	}

	/* Only directories can have default ACLs. */
	if (S_ISDIR(archive_entry_mode(entry))) {
		acl = acl_get_file(accpath, ACL_TYPE_DEFAULT);
		if (acl != NULL) {
			setup_acl_posix1e(a, entry, acl,
			    ARCHIVE_ENTRY_ACL_TYPE_DEFAULT);
			acl_free(acl);
		}
	}
	return (ARCHIVE_OK);
}

/*
 * Translate POSIX.1e ACL into libarchive internal structure.
 */
static void
setup_acl_posix1e(struct archive_read_disk *a,
    struct archive_entry *entry, acl_t acl, int archive_entry_acl_type)
{
	acl_tag_t	 acl_tag;
	acl_entry_t	 acl_entry;
	acl_permset_t	 acl_permset;
	int		 s, ae_id, ae_tag, ae_perm;
	const char	*ae_name;

	s = acl_get_entry(acl, ACL_FIRST_ENTRY, &acl_entry);
	while (s == 1) {
		ae_id = -1;
		ae_name = NULL;

		acl_get_tag_type(acl_entry, &acl_tag);
		if (acl_tag == ACL_USER) {
			ae_id = (int)*(uid_t *)acl_get_qualifier(acl_entry);
			ae_name = archive_read_disk_uname(&a->archive, ae_id);
			ae_tag = ARCHIVE_ENTRY_ACL_USER;
		} else if (acl_tag == ACL_GROUP) {
			ae_id = (int)*(gid_t *)acl_get_qualifier(acl_entry);
			ae_name = archive_read_disk_gname(&a->archive, ae_id);
			ae_tag = ARCHIVE_ENTRY_ACL_GROUP;
		} else if (acl_tag == ACL_MASK) {
			ae_tag = ARCHIVE_ENTRY_ACL_MASK;
		} else if (acl_tag == ACL_USER_OBJ) {
			ae_tag = ARCHIVE_ENTRY_ACL_USER_OBJ;
		} else if (acl_tag == ACL_GROUP_OBJ) {
			ae_tag = ARCHIVE_ENTRY_ACL_GROUP_OBJ;
		} else if (acl_tag == ACL_OTHER) {
			ae_tag = ARCHIVE_ENTRY_ACL_OTHER;
		} else {
			/* Skip types that libarchive can't support. */
			continue;
		}

		acl_get_permset(acl_entry, &acl_permset);
		ae_perm = 0;
		/*
		 * acl_get_perm() is spelled differently on different
		 * platforms; see above.
		 */
		if (ACL_GET_PERM(acl_permset, ACL_EXECUTE))
			ae_perm |= ARCHIVE_ENTRY_ACL_EXECUTE;
		if (ACL_GET_PERM(acl_permset, ACL_READ))
			ae_perm |= ARCHIVE_ENTRY_ACL_READ;
		if (ACL_GET_PERM(acl_permset, ACL_WRITE))
			ae_perm |= ARCHIVE_ENTRY_ACL_WRITE;

		archive_entry_acl_add_entry(entry,
		    archive_entry_acl_type, ae_perm, ae_tag,
		    ae_id, ae_name);

		s = acl_get_entry(acl, ACL_NEXT_ENTRY, &acl_entry);
	}
}
#else
static int
setup_acls_posix1e(struct archive_read_disk *a,
    struct archive_entry *entry, int fd)
{
	(void)a;      /* UNUSED */
	(void)entry;  /* UNUSED */
	(void)fd;     /* UNUSED */
	return (ARCHIVE_OK);
}
#endif

#if HAVE_LISTXATTR && HAVE_LLISTXATTR && HAVE_GETXATTR && HAVE_LGETXATTR

/*
 * Linux extended attribute support.
 *
 * TODO:  By using a stack-allocated buffer for the first
 * call to getxattr(), we might be able to avoid the second
 * call entirely.  We only need the second call if the
 * stack-allocated buffer is too small.  But a modest buffer
 * of 1024 bytes or so will often be big enough.  Same applies
 * to listxattr().
 */


static int
setup_xattr(struct archive_read_disk *a,
    struct archive_entry *entry, const char *name, int fd)
{
	ssize_t size;
	void *value = NULL;
	const char *accpath;

	(void)fd; /* UNUSED */

	accpath = archive_entry_sourcepath(entry);
	if (accpath == NULL)
		accpath = archive_entry_pathname(entry);

	if (!a->follow_symlinks)
		size = lgetxattr(accpath, name, NULL, 0);
	else
		size = getxattr(accpath, name, NULL, 0);

	if (size == -1) {
		archive_set_error(&a->archive, errno,
		    "Couldn't query extended attribute");
		return (ARCHIVE_WARN);
	}

	if (size > 0 && (value = malloc(size)) == NULL) {
		archive_set_error(&a->archive, errno, "Out of memory");
		return (ARCHIVE_FATAL);
	}

	if (!a->follow_symlinks)
		size = lgetxattr(accpath, name, value, size);
	else
		size = getxattr(accpath, name, value, size);

	if (size == -1) {
		archive_set_error(&a->archive, errno,
		    "Couldn't read extended attribute");
		return (ARCHIVE_WARN);
	}

	archive_entry_xattr_add_entry(entry, name, value, size);

	free(value);
	return (ARCHIVE_OK);
}

static int
setup_xattrs(struct archive_read_disk *a,
    struct archive_entry *entry, int fd)
{
	char *list, *p;
	const char *path;
	ssize_t list_size;


	path = archive_entry_sourcepath(entry);
	if (path == NULL)
		path = archive_entry_pathname(entry);

	if (!a->follow_symlinks)
		list_size = llistxattr(path, NULL, 0);
	else
		list_size = listxattr(path, NULL, 0);

	if (list_size == -1) {
		if (errno == ENOTSUP)
			return (ARCHIVE_OK);
		archive_set_error(&a->archive, errno,
			"Couldn't list extended attributes");
		return (ARCHIVE_WARN);
	}

	if (list_size == 0)
		return (ARCHIVE_OK);

	if ((list = malloc(list_size)) == NULL) {
		archive_set_error(&a->archive, errno, "Out of memory");
		return (ARCHIVE_FATAL);
	}

	if (!a->follow_symlinks)
		list_size = llistxattr(path, list, list_size);
	else
		list_size = listxattr(path, list, list_size);

	if (list_size == -1) {
		archive_set_error(&a->archive, errno,
			"Couldn't retrieve extended attributes");
		free(list);
		return (ARCHIVE_WARN);
	}

	for (p = list; (p - list) < list_size; p += strlen(p) + 1) {
		if (strncmp(p, "system.", 7) == 0 ||
				strncmp(p, "xfsroot.", 8) == 0)
			continue;
		setup_xattr(a, entry, p, fd);
	}

	free(list);
	return (ARCHIVE_OK);
}

#elif HAVE_EXTATTR_GET_FILE && HAVE_EXTATTR_LIST_FILE && \
    HAVE_DECL_EXTATTR_NAMESPACE_USER

/*
 * FreeBSD extattr interface.
 */

/* TODO: Implement this.  Follow the Linux model above, but
 * with FreeBSD-specific system calls, of course.  Be careful
 * to not include the system extattrs that hold ACLs; we handle
 * those separately.
 */
static int
setup_xattr(struct archive_read_disk *a, struct archive_entry *entry,
    int namespace, const char *name, const char *fullname, int fd);

static int
setup_xattr(struct archive_read_disk *a, struct archive_entry *entry,
    int namespace, const char *name, const char *fullname, int fd)
{
	ssize_t size;
	void *value = NULL;
	const char *accpath;

	(void)fd; /* UNUSED */

	accpath = archive_entry_sourcepath(entry);
	if (accpath == NULL)
		accpath = archive_entry_pathname(entry);

	if (!a->follow_symlinks)
		size = extattr_get_link(accpath, namespace, name, NULL, 0);
	else
		size = extattr_get_file(accpath, namespace, name, NULL, 0);

	if (size == -1) {
		archive_set_error(&a->archive, errno,
		    "Couldn't query extended attribute");
		return (ARCHIVE_WARN);
	}

	if (size > 0 && (value = malloc(size)) == NULL) {
		archive_set_error(&a->archive, errno, "Out of memory");
		return (ARCHIVE_FATAL);
	}

	if (!a->follow_symlinks)
		size = extattr_get_link(accpath, namespace, name, value, size);
	else
		size = extattr_get_file(accpath, namespace, name, value, size);

	if (size == -1) {
		archive_set_error(&a->archive, errno,
		    "Couldn't read extended attribute");
		return (ARCHIVE_WARN);
	}

	archive_entry_xattr_add_entry(entry, fullname, value, size);

	free(value);
	return (ARCHIVE_OK);
}

static int
setup_xattrs(struct archive_read_disk *a,
    struct archive_entry *entry, int fd)
{
	char buff[512];
	char *list, *p;
	ssize_t list_size;
	const char *path;
	int namespace = EXTATTR_NAMESPACE_USER;

	path = archive_entry_sourcepath(entry);
	if (path == NULL)
		path = archive_entry_pathname(entry);

	if (!a->follow_symlinks)
		list_size = extattr_list_link(path, namespace, NULL, 0);
	else
		list_size = extattr_list_file(path, namespace, NULL, 0);

	if (list_size == -1 && errno == EOPNOTSUPP)
		return (ARCHIVE_OK);
	if (list_size == -1) {
		archive_set_error(&a->archive, errno,
			"Couldn't list extended attributes");
		return (ARCHIVE_WARN);
	}

	if (list_size == 0)
		return (ARCHIVE_OK);

	if ((list = malloc(list_size)) == NULL) {
		archive_set_error(&a->archive, errno, "Out of memory");
		return (ARCHIVE_FATAL);
	}

	if (!a->follow_symlinks)
		list_size = extattr_list_link(path, namespace, list, list_size);
	else
		list_size = extattr_list_file(path, namespace, list, list_size);

	if (list_size == -1) {
		archive_set_error(&a->archive, errno,
			"Couldn't retrieve extended attributes");
		free(list);
		return (ARCHIVE_WARN);
	}

	p = list;
	while ((p - list) < list_size) {
		size_t len = 255 & (int)*p;
		char *name;

		strcpy(buff, "user.");
		name = buff + strlen(buff);
		memcpy(name, p + 1, len);
		name[len] = '\0';
		setup_xattr(a, entry, namespace, name, buff, fd);
		p += 1 + len;
	}

	free(list);
	return (ARCHIVE_OK);
}

#else

/*
 * Generic (stub) extended attribute support.
 */
static int
setup_xattrs(struct archive_read_disk *a,
    struct archive_entry *entry, int fd)
{
	(void)a;     /* UNUSED */
	(void)entry; /* UNUSED */
	(void)fd;    /* UNUSED */
	return (ARCHIVE_OK);
}

#endif
