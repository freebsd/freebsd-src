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
 *
 * $FreeBSD: src/lib/libarchive/archive_entry.h,v 1.23 2007/07/15 19:10:34 kientzle Exp $
 */

#ifndef ARCHIVE_ENTRY_H_INCLUDED
#define	ARCHIVE_ENTRY_H_INCLUDED

#include <sys/types.h>
#include <stddef.h>  /* for wchar_t */
#include <time.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Description of an archive entry.
 *
 * Basically, a "struct stat" with a few text fields added in.
 *
 * TODO: Add "comment", "charset", and possibly other entries that are
 * supported by "pax interchange" format.  However, GNU, ustar, cpio,
 * and other variants don't support these features, so they're not an
 * excruciatingly high priority right now.
 *
 * TODO: "pax interchange" format allows essentially arbitrary
 * key/value attributes to be attached to any entry.  Supporting
 * such extensions may make this library useful for special
 * applications (e.g., a package manager could attach special
 * package-management attributes to each entry).
 */
struct archive_entry;

/*
 * File-type constants.  These are returned from archive_entry_filetype().
 */
#define	AE_IFMT		0170000
#define	AE_IFREG	0100000
#define	AE_IFLNK	0120000
#define	AE_IFCHR	0020000
#define	AE_IFBLK	0060000
#define	AE_IFDIR	0040000
#define	AE_IFIFO	0010000

/*
 * Basic object manipulation
 */

struct archive_entry	*archive_entry_clear(struct archive_entry *);
/* The 'clone' function does a deep copy; all of the strings are copied too. */
struct archive_entry	*archive_entry_clone(struct archive_entry *);
void			 archive_entry_free(struct archive_entry *);
struct archive_entry	*archive_entry_new(void);

/*
 * Retrieve fields from an archive_entry.
 */

time_t			 archive_entry_atime(struct archive_entry *);
long			 archive_entry_atime_nsec(struct archive_entry *);
time_t			 archive_entry_ctime(struct archive_entry *);
long			 archive_entry_ctime_nsec(struct archive_entry *);
dev_t			 archive_entry_dev(struct archive_entry *);
dev_t			 archive_entry_devmajor(struct archive_entry *);
dev_t			 archive_entry_devminor(struct archive_entry *);
mode_t			 archive_entry_filetype(struct archive_entry *);
void			 archive_entry_fflags(struct archive_entry *,
			     unsigned long *set, unsigned long *clear);
const char		*archive_entry_fflags_text(struct archive_entry *);
gid_t			 archive_entry_gid(struct archive_entry *);
const char		*archive_entry_gname(struct archive_entry *);
const wchar_t		*archive_entry_gname_w(struct archive_entry *);
const char		*archive_entry_hardlink(struct archive_entry *);
const wchar_t		*archive_entry_hardlink_w(struct archive_entry *);
ino_t			 archive_entry_ino(struct archive_entry *);
mode_t			 archive_entry_mode(struct archive_entry *);
time_t			 archive_entry_mtime(struct archive_entry *);
long			 archive_entry_mtime_nsec(struct archive_entry *);
unsigned int		 archive_entry_nlink(struct archive_entry *);
const char		*archive_entry_pathname(struct archive_entry *);
const wchar_t		*archive_entry_pathname_w(struct archive_entry *);
dev_t			 archive_entry_rdev(struct archive_entry *);
dev_t			 archive_entry_rdevmajor(struct archive_entry *);
dev_t			 archive_entry_rdevminor(struct archive_entry *);
int64_t			 archive_entry_size(struct archive_entry *);
const char		*archive_entry_symlink(struct archive_entry *);
const wchar_t		*archive_entry_symlink_w(struct archive_entry *);
uid_t			 archive_entry_uid(struct archive_entry *);
const char		*archive_entry_uname(struct archive_entry *);
const wchar_t		*archive_entry_uname_w(struct archive_entry *);

/*
 * Set fields in an archive_entry.
 *
 * Note that string 'set' functions do not copy the string, only the pointer.
 * In contrast, 'copy' functions do copy the object pointed to.
 */

void	archive_entry_set_atime(struct archive_entry *, time_t, long);
void	archive_entry_set_ctime(struct archive_entry *, time_t, long);
void	archive_entry_set_dev(struct archive_entry *, dev_t);
void	archive_entry_set_devmajor(struct archive_entry *, dev_t);
void	archive_entry_set_devminor(struct archive_entry *, dev_t);
void	archive_entry_set_filetype(struct archive_entry *, unsigned int);
void	archive_entry_set_fflags(struct archive_entry *,
	    unsigned long set, unsigned long clear);
/* Returns pointer to start of first invalid token, or NULL if none. */
/* Note that all recognized tokens are processed, regardless. */
const wchar_t *archive_entry_copy_fflags_text_w(struct archive_entry *,
	    const wchar_t *);
void	archive_entry_set_gid(struct archive_entry *, gid_t);
void	archive_entry_set_gname(struct archive_entry *, const char *);
void	archive_entry_copy_gname(struct archive_entry *, const char *);
void	archive_entry_copy_gname_w(struct archive_entry *, const wchar_t *);
void	archive_entry_set_hardlink(struct archive_entry *, const char *);
void	archive_entry_copy_hardlink(struct archive_entry *, const char *);
void	archive_entry_copy_hardlink_w(struct archive_entry *, const wchar_t *);
void	archive_entry_set_ino(struct archive_entry *, unsigned long);
void	archive_entry_set_link(struct archive_entry *, const char *);
void	archive_entry_set_mode(struct archive_entry *, mode_t);
void	archive_entry_set_mtime(struct archive_entry *, time_t, long);
void	archive_entry_set_nlink(struct archive_entry *, unsigned int);
void	archive_entry_set_pathname(struct archive_entry *, const char *);
void	archive_entry_copy_pathname(struct archive_entry *, const char *);
void	archive_entry_copy_pathname_w(struct archive_entry *, const wchar_t *);
void	archive_entry_set_rdev(struct archive_entry *, dev_t);
void	archive_entry_set_rdevmajor(struct archive_entry *, dev_t);
void	archive_entry_set_rdevminor(struct archive_entry *, dev_t);
void	archive_entry_set_size(struct archive_entry *, int64_t);
void	archive_entry_set_symlink(struct archive_entry *, const char *);
void	archive_entry_copy_symlink(struct archive_entry *, const char *);
void	archive_entry_copy_symlink_w(struct archive_entry *, const wchar_t *);
void	archive_entry_set_uid(struct archive_entry *, uid_t);
void	archive_entry_set_uname(struct archive_entry *, const char *);
void	archive_entry_copy_uname(struct archive_entry *, const char *);
void	archive_entry_copy_uname_w(struct archive_entry *, const wchar_t *);

/*
 * Routines to bulk copy fields to/from a platform-native "struct
 * stat."  Libarchive used to just store a struct stat inside of each
 * archive_entry object, but this created issues when trying to
 * manipulate archives on systems different than the ones they were
 * created on.
 *
 * TODO: On Linux, provide both stat32 and stat64 versions of these functions.
 */
const struct stat	*archive_entry_stat(struct archive_entry *);
void	archive_entry_copy_stat(struct archive_entry *, const struct stat *);

/*
 * ACL routines.  This used to simply store and return text-format ACL
 * strings, but that proved insufficient for a number of reasons:
 *   = clients need control over uname/uid and gname/gid mappings
 *   = there are many different ACL text formats
 *   = would like to be able to read/convert archives containing ACLs
 *     on platforms that lack ACL libraries
 */

/*
 * Permission bits mimic POSIX.1e.  Note that I've not followed POSIX.1e's
 * "permset"/"perm" abstract type nonsense.  A permset is just a simple
 * bitmap, following long-standing Unix tradition.
 */
#define	ARCHIVE_ENTRY_ACL_EXECUTE	1
#define	ARCHIVE_ENTRY_ACL_WRITE		2
#define	ARCHIVE_ENTRY_ACL_READ		4

/* We need to be able to specify either or both of these. */
#define	ARCHIVE_ENTRY_ACL_TYPE_ACCESS	256
#define	ARCHIVE_ENTRY_ACL_TYPE_DEFAULT	512

/* Tag values mimic POSIX.1e */
#define	ARCHIVE_ENTRY_ACL_USER		10001	/* Specified user. */
#define	ARCHIVE_ENTRY_ACL_USER_OBJ 	10002	/* User who owns the file. */
#define	ARCHIVE_ENTRY_ACL_GROUP		10003	/* Specified group. */
#define	ARCHIVE_ENTRY_ACL_GROUP_OBJ	10004	/* Group who owns the file. */
#define	ARCHIVE_ENTRY_ACL_MASK		10005	/* Modify group access. */
#define	ARCHIVE_ENTRY_ACL_OTHER		10006	/* Public. */

/*
 * Set the ACL by clearing it and adding entries one at a time.
 * Unlike the POSIX.1e ACL routines, you must specify the type
 * (access/default) for each entry.  Internally, the ACL data is just
 * a soup of entries.  API calls here allow you to retrieve just the
 * entries of interest.  This design (which goes against the spirit of
 * POSIX.1e) is useful for handling archive formats that combine
 * default and access information in a single ACL list.
 */
void	 archive_entry_acl_clear(struct archive_entry *);
void	 archive_entry_acl_add_entry(struct archive_entry *,
	     int type, int permset, int tag, int qual, const char *name);
void	 archive_entry_acl_add_entry_w(struct archive_entry *,
	     int type, int permset, int tag, int qual, const wchar_t *name);

/*
 * To retrieve the ACL, first "reset", then repeatedly ask for the
 * "next" entry.  The want_type parameter allows you to request only
 * access entries or only default entries.
 */
int	 archive_entry_acl_reset(struct archive_entry *, int want_type);
int	 archive_entry_acl_next(struct archive_entry *, int want_type,
	     int *type, int *permset, int *tag, int *qual, const char **name);
int	 archive_entry_acl_next_w(struct archive_entry *, int want_type,
	     int *type, int *permset, int *tag, int *qual,
	     const wchar_t **name);

/*
 * Construct a text-format ACL.  The flags argument is a bitmask that
 * can include any of the following:
 *
 * ARCHIVE_ENTRY_ACL_TYPE_ACCESS - Include access entries.
 * ARCHIVE_ENTRY_ACL_TYPE_DEFAULT - Include default entries.
 * ARCHIVE_ENTRY_ACL_STYLE_EXTRA_ID - Include extra numeric ID field in
 *    each ACL entry.  (As used by 'star'.)
 * ARCHIVE_ENTRY_ACL_STYLE_MARK_DEFAULT - Include "default:" before each
 *    default ACL entry.
 */
#define	ARCHIVE_ENTRY_ACL_STYLE_EXTRA_ID	1024
#define	ARCHIVE_ENTRY_ACL_STYLE_MARK_DEFAULT	2048
const wchar_t	*archive_entry_acl_text_w(struct archive_entry *, int flags);

/* Return a count of entries matching 'want_type' */
int	 archive_entry_acl_count(struct archive_entry *, int want_type);

/*
 * Private ACL parser.  This is private because it handles some
 * very weird formats that clients should not be messing with.
 * Clients should only deal with their platform-native formats.
 * Because of the need to support many formats cleanly, new arguments
 * are likely to get added on a regular basis.  Clients who try to use
 * this interface are likely to be surprised when it changes.
 *
 * You were warned!
 */
int		 __archive_entry_acl_parse_w(struct archive_entry *,
		     const wchar_t *, int type);


#ifdef __cplusplus
}
#endif

/*
 * extended attributes
 */

void	 archive_entry_xattr_clear(struct archive_entry *);
void	 archive_entry_xattr_add_entry(struct archive_entry *,
	     const char *name, const void *value, size_t size);

/*
 * To retrieve the xattr list, first "reset", then repeatedly ask for the
 * "next" entry.
 */

int	archive_entry_xattr_count(struct archive_entry *);
int	archive_entry_xattr_reset(struct archive_entry *);
int	archive_entry_xattr_next(struct archive_entry *,
	     const char **name, const void **value, size_t *);


#endif /* !ARCHIVE_ENTRY_H_INCLUDED */
