/*-
 * Copyright (c) 2003-2008 Tim Kientzle
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
 * $FreeBSD$
 */

#ifndef ARCHIVE_ENTRY_H_INCLUDED
#define	ARCHIVE_ENTRY_H_INCLUDED

/*
 * Note: archive_entry.h is for use outside of libarchive; the
 * configuration headers (config.h, archive_platform.h, etc.) are
 * purely internal.  Do NOT use HAVE_XXX configuration macros to
 * control the behavior of this header!  If you must conditionalize,
 * use predefined compiler and/or platform macros.
 */

#include <sys/types.h>
#include <stddef.h>  /* for wchar_t */
#include <time.h>

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <windows.h>
#endif

/* Get appropriate definitions of standard POSIX-style types. */
/* These should match the types used in 'struct stat' */
#if defined(_WIN32) && !defined(__CYGWIN__)
#define	__LA_INT64_T	__int64
# if defined(__BORLANDC__)
#  define	__LA_UID_T	uid_t
#  define	__LA_GID_T	gid_t
#  define	__LA_DEV_T	dev_t
#  define	__LA_MODE_T	mode_t
# else
#  define	__LA_UID_T	short
#  define	__LA_GID_T	short
#  define	__LA_DEV_T	unsigned int
#  define	__LA_MODE_T	unsigned short
# endif
#else
#include <unistd.h>
#define	__LA_INT64_T	int64_t
#define	__LA_UID_T	uid_t
#define	__LA_GID_T	gid_t
#define	__LA_DEV_T	dev_t
#define	__LA_MODE_T	mode_t
#endif

/*
 * XXX Is this defined for all Windows compilers?  If so, in what
 * header?  It would be nice to remove the __LA_INO_T indirection and
 * just use plain ino_t everywhere.  Likewise for the other types just
 * above.
 */
#define	__LA_INO_T	ino_t


/*
 * On Windows, define LIBARCHIVE_STATIC if you're building or using a
 * .lib.  The default here assumes you're building a DLL.  Only
 * libarchive source should ever define __LIBARCHIVE_BUILD.
 */
#if ((defined __WIN32__) || (defined _WIN32) || defined(__CYGWIN__)) && (!defined LIBARCHIVE_STATIC)
# ifdef __LIBARCHIVE_BUILD
#  ifdef __GNUC__
#   define __LA_DECL	__attribute__((dllexport)) extern
#  else
#   define __LA_DECL	__declspec(dllexport)
#  endif
# else
#  ifdef __GNUC__
#   define __LA_DECL	__attribute__((dllimport)) extern
#  else
#   define __LA_DECL	__declspec(dllimport)
#  endif
# endif
#else
/* Static libraries on all platforms and shared libraries on non-Windows. */
# define __LA_DECL
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Description of an archive entry.
 *
 * You can think of this as "struct stat" with some text fields added in.
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
 * File-type constants.  These are returned from archive_entry_filetype()
 * and passed to archive_entry_set_filetype().
 *
 * These values match S_XXX defines on every platform I've checked,
 * including Windows, AIX, Linux, Solaris, and BSD.  They're
 * (re)defined here because platforms generally don't define the ones
 * they don't support.  For example, Windows doesn't define S_IFLNK or
 * S_IFBLK.  Instead of having a mass of conditional logic and system
 * checks to define any S_XXX values that aren't supported locally,
 * I've just defined a new set of such constants so that
 * libarchive-based applications can manipulate and identify archive
 * entries properly even if the hosting platform can't store them on
 * disk.
 *
 * These values are also used directly within some portable formats,
 * such as cpio.  If you find a platform that varies from these, the
 * correct solution is to leave these alone and translate from these
 * portable values to platform-native values when entries are read from
 * or written to disk.
 */
#define	AE_IFMT		0170000
#define	AE_IFREG	0100000
#define	AE_IFLNK	0120000
#define	AE_IFSOCK	0140000
#define	AE_IFCHR	0020000
#define	AE_IFBLK	0060000
#define	AE_IFDIR	0040000
#define	AE_IFIFO	0010000

/*
 * Basic object manipulation
 */

__LA_DECL struct archive_entry	*archive_entry_clear(struct archive_entry *);
/* The 'clone' function does a deep copy; all of the strings are copied too. */
__LA_DECL struct archive_entry	*archive_entry_clone(struct archive_entry *);
__LA_DECL void			 archive_entry_free(struct archive_entry *);
__LA_DECL struct archive_entry	*archive_entry_new(void);

/*
 * Retrieve fields from an archive_entry.
 *
 * There are a number of implicit conversions among these fields.  For
 * example, if a regular string field is set and you read the _w wide
 * character field, the entry will implicitly convert narrow-to-wide
 * using the current locale.  Similarly, dev values are automatically
 * updated when you write devmajor or devminor and vice versa.
 *
 * In addition, fields can be "set" or "unset."  Unset string fields
 * return NULL, non-string fields have _is_set() functions to test
 * whether they've been set.  You can "unset" a string field by
 * assigning NULL; non-string fields have _unset() functions to
 * unset them.
 *
 * Note: There is one ambiguity in the above; string fields will
 * also return NULL when implicit character set conversions fail.
 * This is usually what you want.
 */
__LA_DECL time_t	 archive_entry_atime(struct archive_entry *);
__LA_DECL long		 archive_entry_atime_nsec(struct archive_entry *);
__LA_DECL int		 archive_entry_atime_is_set(struct archive_entry *);
__LA_DECL time_t	 archive_entry_birthtime(struct archive_entry *);
__LA_DECL long		 archive_entry_birthtime_nsec(struct archive_entry *);
__LA_DECL int		 archive_entry_birthtime_is_set(struct archive_entry *);
__LA_DECL time_t	 archive_entry_ctime(struct archive_entry *);
__LA_DECL long		 archive_entry_ctime_nsec(struct archive_entry *);
__LA_DECL int		 archive_entry_ctime_is_set(struct archive_entry *);
__LA_DECL dev_t		 archive_entry_dev(struct archive_entry *);
__LA_DECL dev_t		 archive_entry_devmajor(struct archive_entry *);
__LA_DECL dev_t		 archive_entry_devminor(struct archive_entry *);
__LA_DECL __LA_MODE_T	 archive_entry_filetype(struct archive_entry *);
__LA_DECL void		 archive_entry_fflags(struct archive_entry *,
			    unsigned long * /* set */,
			    unsigned long * /* clear */);
__LA_DECL const char	*archive_entry_fflags_text(struct archive_entry *);
__LA_DECL __LA_GID_T	 archive_entry_gid(struct archive_entry *);
__LA_DECL const char	*archive_entry_gname(struct archive_entry *);
__LA_DECL const wchar_t	*archive_entry_gname_w(struct archive_entry *);
__LA_DECL const char	*archive_entry_hardlink(struct archive_entry *);
__LA_DECL const wchar_t	*archive_entry_hardlink_w(struct archive_entry *);
__LA_DECL __LA_INO_T	 archive_entry_ino(struct archive_entry *);
__LA_DECL __LA_INT64_T	 archive_entry_ino64(struct archive_entry *);
__LA_DECL __LA_MODE_T	 archive_entry_mode(struct archive_entry *);
__LA_DECL time_t	 archive_entry_mtime(struct archive_entry *);
__LA_DECL long		 archive_entry_mtime_nsec(struct archive_entry *);
__LA_DECL int		 archive_entry_mtime_is_set(struct archive_entry *);
__LA_DECL unsigned int	 archive_entry_nlink(struct archive_entry *);
__LA_DECL const char	*archive_entry_pathname(struct archive_entry *);
__LA_DECL const wchar_t	*archive_entry_pathname_w(struct archive_entry *);
__LA_DECL dev_t		 archive_entry_rdev(struct archive_entry *);
__LA_DECL dev_t		 archive_entry_rdevmajor(struct archive_entry *);
__LA_DECL dev_t		 archive_entry_rdevminor(struct archive_entry *);
__LA_DECL const char	*archive_entry_sourcepath(struct archive_entry *);
__LA_DECL __LA_INT64_T	 archive_entry_size(struct archive_entry *);
__LA_DECL int		 archive_entry_size_is_set(struct archive_entry *);
__LA_DECL const char	*archive_entry_strmode(struct archive_entry *);
__LA_DECL const char	*archive_entry_symlink(struct archive_entry *);
__LA_DECL const wchar_t	*archive_entry_symlink_w(struct archive_entry *);
__LA_DECL __LA_UID_T	 archive_entry_uid(struct archive_entry *);
__LA_DECL const char	*archive_entry_uname(struct archive_entry *);
__LA_DECL const wchar_t	*archive_entry_uname_w(struct archive_entry *);

/*
 * Set fields in an archive_entry.
 *
 * Note that string 'set' functions do not copy the string, only the pointer.
 * In contrast, 'copy' functions do copy the object pointed to.
 *
 * Note: As of libarchive 2.4, 'set' functions do copy the string and
 * are therefore exact synonyms for the 'copy' versions.  The 'copy'
 * names will be retired in libarchive 3.0.
 */

__LA_DECL void	archive_entry_set_atime(struct archive_entry *, time_t, long);
__LA_DECL void  archive_entry_unset_atime(struct archive_entry *);
#if defined(_WIN32) && !defined(__CYGWIN__)
__LA_DECL void archive_entry_copy_bhfi(struct archive_entry *,
									   BY_HANDLE_FILE_INFORMATION *);
#endif
__LA_DECL void	archive_entry_set_birthtime(struct archive_entry *, time_t, long);
__LA_DECL void  archive_entry_unset_birthtime(struct archive_entry *);
__LA_DECL void	archive_entry_set_ctime(struct archive_entry *, time_t, long);
__LA_DECL void  archive_entry_unset_ctime(struct archive_entry *);
__LA_DECL void	archive_entry_set_dev(struct archive_entry *, dev_t);
__LA_DECL void	archive_entry_set_devmajor(struct archive_entry *, dev_t);
__LA_DECL void	archive_entry_set_devminor(struct archive_entry *, dev_t);
__LA_DECL void	archive_entry_set_filetype(struct archive_entry *, unsigned int);
__LA_DECL void	archive_entry_set_fflags(struct archive_entry *,
	    unsigned long /* set */, unsigned long /* clear */);
/* Returns pointer to start of first invalid token, or NULL if none. */
/* Note that all recognized tokens are processed, regardless. */
__LA_DECL const char *archive_entry_copy_fflags_text(struct archive_entry *,
	    const char *);
__LA_DECL const wchar_t *archive_entry_copy_fflags_text_w(struct archive_entry *,
	    const wchar_t *);
__LA_DECL void	archive_entry_set_gid(struct archive_entry *, __LA_GID_T);
__LA_DECL void	archive_entry_set_gname(struct archive_entry *, const char *);
__LA_DECL void	archive_entry_copy_gname(struct archive_entry *, const char *);
__LA_DECL void	archive_entry_copy_gname_w(struct archive_entry *, const wchar_t *);
__LA_DECL int	archive_entry_update_gname_utf8(struct archive_entry *, const char *);
__LA_DECL void	archive_entry_set_hardlink(struct archive_entry *, const char *);
__LA_DECL void	archive_entry_copy_hardlink(struct archive_entry *, const char *);
__LA_DECL void	archive_entry_copy_hardlink_w(struct archive_entry *, const wchar_t *);
__LA_DECL int	archive_entry_update_hardlink_utf8(struct archive_entry *, const char *);
#if ARCHIVE_VERSION_NUMBER >= 3000000
/* Starting with libarchive 3.0, this will be synonym for ino64. */
__LA_DECL void	archive_entry_set_ino(struct archive_entry *, __LA_INT64_T);
#else
__LA_DECL void	archive_entry_set_ino(struct archive_entry *, unsigned long);
#endif
__LA_DECL void	archive_entry_set_ino64(struct archive_entry *, __LA_INT64_T);
__LA_DECL void	archive_entry_set_link(struct archive_entry *, const char *);
__LA_DECL void	archive_entry_copy_link(struct archive_entry *, const char *);
__LA_DECL void	archive_entry_copy_link_w(struct archive_entry *, const wchar_t *);
__LA_DECL int	archive_entry_update_link_utf8(struct archive_entry *, const char *);
__LA_DECL void	archive_entry_set_mode(struct archive_entry *, __LA_MODE_T);
__LA_DECL void	archive_entry_set_mtime(struct archive_entry *, time_t, long);
__LA_DECL void  archive_entry_unset_mtime(struct archive_entry *);
__LA_DECL void	archive_entry_set_nlink(struct archive_entry *, unsigned int);
__LA_DECL void	archive_entry_set_pathname(struct archive_entry *, const char *);
__LA_DECL void	archive_entry_copy_pathname(struct archive_entry *, const char *);
__LA_DECL void	archive_entry_copy_pathname_w(struct archive_entry *, const wchar_t *);
__LA_DECL int	archive_entry_update_pathname_utf8(struct archive_entry *, const char *);
__LA_DECL void	archive_entry_set_perm(struct archive_entry *, __LA_MODE_T);
__LA_DECL void	archive_entry_set_rdev(struct archive_entry *, dev_t);
__LA_DECL void	archive_entry_set_rdevmajor(struct archive_entry *, dev_t);
__LA_DECL void	archive_entry_set_rdevminor(struct archive_entry *, dev_t);
__LA_DECL void	archive_entry_set_size(struct archive_entry *, __LA_INT64_T);
__LA_DECL void	archive_entry_unset_size(struct archive_entry *);
__LA_DECL void	archive_entry_copy_sourcepath(struct archive_entry *, const char *);
__LA_DECL void	archive_entry_set_symlink(struct archive_entry *, const char *);
__LA_DECL void	archive_entry_copy_symlink(struct archive_entry *, const char *);
__LA_DECL void	archive_entry_copy_symlink_w(struct archive_entry *, const wchar_t *);
__LA_DECL int	archive_entry_update_symlink_utf8(struct archive_entry *, const char *);
__LA_DECL void	archive_entry_set_uid(struct archive_entry *, __LA_UID_T);
__LA_DECL void	archive_entry_set_uname(struct archive_entry *, const char *);
__LA_DECL void	archive_entry_copy_uname(struct archive_entry *, const char *);
__LA_DECL void	archive_entry_copy_uname_w(struct archive_entry *, const wchar_t *);
__LA_DECL int	archive_entry_update_uname_utf8(struct archive_entry *, const char *);
/*
 * Routines to bulk copy fields to/from a platform-native "struct
 * stat."  Libarchive used to just store a struct stat inside of each
 * archive_entry object, but this created issues when trying to
 * manipulate archives on systems different than the ones they were
 * created on.
 *
 * TODO: On Linux, provide both stat32 and stat64 versions of these functions.
 */
__LA_DECL const struct stat	*archive_entry_stat(struct archive_entry *);
__LA_DECL void	archive_entry_copy_stat(struct archive_entry *, const struct stat *);


/*
 * ACL routines.  This used to simply store and return text-format ACL
 * strings, but that proved insufficient for a number of reasons:
 *   = clients need control over uname/uid and gname/gid mappings
 *   = there are many different ACL text formats
 *   = would like to be able to read/convert archives containing ACLs
 *     on platforms that lack ACL libraries
 *
 *  This last point, in particular, forces me to implement a reasonably
 *  complete set of ACL support routines.
 *
 *  TODO: Extend this to support NFSv4/NTFS permissions.  That should
 *  allow full ACL support on Mac OS, in particular, which uses
 *  POSIX.1e-style interfaces to manipulate NFSv4/NTFS permissions.
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
__LA_DECL void	 archive_entry_acl_clear(struct archive_entry *);
__LA_DECL void	 archive_entry_acl_add_entry(struct archive_entry *,
	    int /* type */, int /* permset */, int /* tag */,
	    int /* qual */, const char * /* name */);
__LA_DECL void	 archive_entry_acl_add_entry_w(struct archive_entry *,
	    int /* type */, int /* permset */, int /* tag */,
	    int /* qual */, const wchar_t * /* name */);

/*
 * To retrieve the ACL, first "reset", then repeatedly ask for the
 * "next" entry.  The want_type parameter allows you to request only
 * access entries or only default entries.
 */
__LA_DECL int	 archive_entry_acl_reset(struct archive_entry *, int /* want_type */);
__LA_DECL int	 archive_entry_acl_next(struct archive_entry *, int /* want_type */,
	    int * /* type */, int * /* permset */, int * /* tag */,
	    int * /* qual */, const char ** /* name */);
__LA_DECL int	 archive_entry_acl_next_w(struct archive_entry *, int /* want_type */,
	    int * /* type */, int * /* permset */, int * /* tag */,
	    int * /* qual */, const wchar_t ** /* name */);

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
__LA_DECL const wchar_t	*archive_entry_acl_text_w(struct archive_entry *,
		    int /* flags */);

/* Return a count of entries matching 'want_type' */
__LA_DECL int	 archive_entry_acl_count(struct archive_entry *, int /* want_type */);

/*
 * Private ACL parser.  This is private because it handles some
 * very weird formats that clients should not be messing with.
 * Clients should only deal with their platform-native formats.
 * Because of the need to support many formats cleanly, new arguments
 * are likely to get added on a regular basis.  Clients who try to use
 * this interface are likely to be surprised when it changes.
 *
 * You were warned!
 *
 * TODO: Move this declaration out of the public header and into
 * a private header.  Warnings above are silly.
 */
__LA_DECL int		 __archive_entry_acl_parse_w(struct archive_entry *,
		    const wchar_t *, int /* type */);

/*
 * extended attributes
 */

__LA_DECL void	 archive_entry_xattr_clear(struct archive_entry *);
__LA_DECL void	 archive_entry_xattr_add_entry(struct archive_entry *,
	    const char * /* name */, const void * /* value */,
	    size_t /* size */);

/*
 * To retrieve the xattr list, first "reset", then repeatedly ask for the
 * "next" entry.
 */

__LA_DECL int	archive_entry_xattr_count(struct archive_entry *);
__LA_DECL int	archive_entry_xattr_reset(struct archive_entry *);
__LA_DECL int	archive_entry_xattr_next(struct archive_entry *,
	    const char ** /* name */, const void ** /* value */, size_t *);

/*
 * Utility to match up hardlinks.
 *
 * The 'struct archive_entry_linkresolver' is a cache of archive entries
 * for files with multiple links.  Here's how to use it:
 *   1. Create a lookup object with archive_entry_linkresolver_new()
 *   2. Tell it the archive format you're using.
 *   3. Hand each archive_entry to archive_entry_linkify().
 *      That function will return 0, 1, or 2 entries that should
 *      be written.
 *   4. Call archive_entry_linkify(resolver, NULL) until
 *      no more entries are returned.
 *   5. Call archive_entry_link_resolver_free(resolver) to free resources.
 *
 * The entries returned have their hardlink and size fields updated
 * appropriately.  If an entry is passed in that does not refer to
 * a file with multiple links, it is returned unchanged.  The intention
 * is that you should be able to simply filter all entries through
 * this machine.
 *
 * To make things more efficient, be sure that each entry has a valid
 * nlinks value.  The hardlink cache uses this to track when all links
 * have been found.  If the nlinks value is zero, it will keep every
 * name in the cache indefinitely, which can use a lot of memory.
 *
 * Note that archive_entry_size() is reset to zero if the file
 * body should not be written to the archive.  Pay attention!
 */
struct archive_entry_linkresolver;

/*
 * There are three different strategies for marking hardlinks.
 * The descriptions below name them after the best-known
 * formats that rely on each strategy:
 *
 * "Old cpio" is the simplest, it always returns any entry unmodified.
 *    As far as I know, only cpio formats use this.  Old cpio archives
 *    store every link with the full body; the onus is on the dearchiver
 *    to detect and properly link the files as they are restored.
 * "tar" is also pretty simple; it caches a copy the first time it sees
 *    any link.  Subsequent appearances are modified to be hardlink
 *    references to the first one without any body.  Used by all tar
 *    formats, although the newest tar formats permit the "old cpio" strategy
 *    as well.  This strategy is very simple for the dearchiver,
 *    and reasonably straightforward for the archiver.
 * "new cpio" is trickier.  It stores the body only with the last
 *    occurrence.  The complication is that we might not
 *    see every link to a particular file in a single session, so
 *    there's no easy way to know when we've seen the last occurrence.
 *    The solution here is to queue one link until we see the next.
 *    At the end of the session, you can enumerate any remaining
 *    entries by calling archive_entry_linkify(NULL) and store those
 *    bodies.  If you have a file with three links l1, l2, and l3,
 *    you'll get the following behavior if you see all three links:
 *           linkify(l1) => NULL   (the resolver stores l1 internally)
 *           linkify(l2) => l1     (resolver stores l2, you write l1)
 *           linkify(l3) => l2, l3 (all links seen, you can write both).
 *    If you only see l1 and l2, you'll get this behavior:
 *           linkify(l1) => NULL
 *           linkify(l2) => l1
 *           linkify(NULL) => l2   (at end, you retrieve remaining links)
 *    As the name suggests, this strategy is used by newer cpio variants.
 *    It's noticably more complex for the archiver, slightly more complex
 *    for the dearchiver than the tar strategy, but makes it straightforward
 *    to restore a file using any link by simply continuing to scan until
 *    you see a link that is stored with a body.  In contrast, the tar
 *    strategy requires you to rescan the archive from the beginning to
 *    correctly extract an arbitrary link.
 */

__LA_DECL struct archive_entry_linkresolver *archive_entry_linkresolver_new(void);
__LA_DECL void archive_entry_linkresolver_set_strategy(
	struct archive_entry_linkresolver *, int /* format_code */);
__LA_DECL void archive_entry_linkresolver_free(struct archive_entry_linkresolver *);
__LA_DECL void archive_entry_linkify(struct archive_entry_linkresolver *,
    struct archive_entry **, struct archive_entry **);

#ifdef __cplusplus
}
#endif

/* This is meaningless outside of this header. */
#undef __LA_DECL

#endif /* !ARCHIVE_ENTRY_H_INCLUDED */
