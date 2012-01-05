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
 * $FreeBSD$
 */

#ifndef __LIBARCHIVE_BUILD
#error This header is only to be used internally to libarchive.
#endif

#ifndef ARCHIVE_ENTRY_PRIVATE_H_INCLUDED
#define	ARCHIVE_ENTRY_PRIVATE_H_INCLUDED

#include "archive_string.h"

/*
 * Handle wide character (i.e., Unicode) and non-wide character
 * strings transparently.
 */

struct aes {
	struct archive_string aes_mbs;
	struct archive_string aes_utf8;
	const wchar_t *aes_wcs;
	/* Bitmap of which of the above are valid.  Because we're lazy
	 * about malloc-ing and reusing the underlying storage, we
	 * can't rely on NULL pointers to indicate whether a string
	 * has been set. */
	int aes_set;
#define	AES_SET_MBS 1
#define	AES_SET_UTF8 2
#define	AES_SET_WCS 4
};

struct ae_acl {
	struct ae_acl *next;
	int	type;			/* E.g., access or default */
	int	tag;			/* E.g., user/group/other/mask */
	int	permset;		/* r/w/x bits */
	int	id;			/* uid/gid for user/group */
	struct aes name;		/* uname/gname */
};

struct ae_xattr {
	struct ae_xattr *next;

	char	*name;
	void	*value;
	size_t	size;
};

/*
 * Description of an archive entry.
 *
 * Basically, this is a "struct stat" with a few text fields added in.
 *
 * TODO: Add "comment", "charset", and possibly other entries
 * that are supported by "pax interchange" format.  However, GNU, ustar,
 * cpio, and other variants don't support these features, so they're not an
 * excruciatingly high priority right now.
 *
 * TODO: "pax interchange" format allows essentially arbitrary
 * key/value attributes to be attached to any entry.  Supporting
 * such extensions may make this library useful for special
 * applications (e.g., a package manager could attach special
 * package-management attributes to each entry).  There are tricky
 * API issues involved, so this is not going to happen until
 * there's a real demand for it.
 *
 * TODO: Design a good API for handling sparse files.
 */
struct archive_entry {
	/*
	 * Note that ae_stat.st_mode & AE_IFMT  can be  0!
	 *
	 * This occurs when the actual file type of the object is not
	 * in the archive.  For example, 'tar' archives store
	 * hardlinks without marking the type of the underlying
	 * object.
	 */

	/*
	 * Read archive_entry_copy_stat.c for an explanation of why I
	 * don't just use "struct stat" instead of "struct aest" here
	 * and why I have this odd pointer to a separately-allocated
	 * struct stat.
	 */
	void *stat;
	int  stat_valid; /* Set to 0 whenever a field in aest changes. */

	struct aest {
		int64_t		aest_atime;
		uint32_t	aest_atime_nsec;
		int64_t		aest_ctime;
		uint32_t	aest_ctime_nsec;
		int64_t		aest_mtime;
		uint32_t	aest_mtime_nsec;
		int64_t		aest_birthtime;
		uint32_t	aest_birthtime_nsec;
		gid_t		aest_gid;
		int64_t		aest_ino;
		mode_t		aest_mode;
		uint32_t	aest_nlink;
		uint64_t	aest_size;
		uid_t		aest_uid;
		/*
		 * Because converting between device codes and
		 * major/minor values is platform-specific and
		 * inherently a bit risky, we only do that conversion
		 * lazily.  That way, we will do a better job of
		 * preserving information in those cases where no
		 * conversion is actually required.
		 */
		int		aest_dev_is_broken_down;
		dev_t		aest_dev;
		dev_t		aest_devmajor;
		dev_t		aest_devminor;
		int		aest_rdev_is_broken_down;
		dev_t		aest_rdev;
		dev_t		aest_rdevmajor;
		dev_t		aest_rdevminor;
	} ae_stat;

	int ae_set; /* bitmap of fields that are currently set */
#define	AE_SET_HARDLINK	1
#define	AE_SET_SYMLINK	2
#define	AE_SET_ATIME	4
#define	AE_SET_CTIME	8
#define	AE_SET_MTIME	16
#define	AE_SET_BIRTHTIME 32
#define	AE_SET_SIZE	64

	/*
	 * Use aes here so that we get transparent mbs<->wcs conversions.
	 */
	struct aes ae_fflags_text;	/* Text fflags per fflagstostr(3) */
	unsigned long ae_fflags_set;		/* Bitmap fflags */
	unsigned long ae_fflags_clear;
	struct aes ae_gname;		/* Name of owning group */
	struct aes ae_hardlink;	/* Name of target for hardlink */
	struct aes ae_pathname;	/* Name of entry */
	struct aes ae_symlink;		/* symlink contents */
	struct aes ae_uname;		/* Name of owner */

	/* Not used within libarchive; useful for some clients. */
	struct aes ae_sourcepath;	/* Path this entry is sourced from. */

	/* ACL support. */
	struct ae_acl	*acl_head;
	struct ae_acl	*acl_p;
	int		 acl_state;	/* See acl_next for details. */
	wchar_t		*acl_text_w;

	/* extattr support. */
	struct ae_xattr *xattr_head;
	struct ae_xattr *xattr_p;

	/* Miscellaneous. */
	char		 strmode[12];
};


#endif /* ARCHIVE_ENTRY_PRIVATE_H_INCLUDED */
