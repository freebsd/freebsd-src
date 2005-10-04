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
#ifdef HAVE_EXT2FS_EXT2_FS_H
#include <ext2fs/ext2_fs.h>	/* for Linux file flags */
#endif
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Obtain suitable wide-character manipulation functions. */
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#else
static size_t wcslen(const wchar_t *s)
{
	const wchar_t *p = s;
	while (*p != L'\0')
		++p;
	return p - s;
}
static wchar_t * wcscpy(wchar_t *s1, const wchar_t *s2)
{
	wchar_t *dest = s1;
	while((*s1 = *s2) != L'\0')
		++s1, ++s2;
	return dest;
}
#define wmemcpy(a,b,i)  (wchar_t *)memcpy((a),(b),(i)*sizeof(wchar_t))
/* Good enough for simple equality testing, but not for sorting. */
#define wmemcmp(a,b,i)  memcmp((a),(b),(i)*sizeof(wchar_t))
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"

#undef max
#define	max(a, b)	((a)>(b)?(a):(b))

/*
 * Handle wide character (i.e., Unicode) and non-wide character
 * strings transparently.
 *
 */

struct aes {
	const char *aes_mbs;
	char *aes_mbs_alloc;
	const wchar_t *aes_wcs;
	wchar_t *aes_wcs_alloc;
};

struct ae_acl {
	struct ae_acl *next;
	int	type;			/* E.g., access or default */
	int	tag;			/* E.g., user/group/other/mask */
	int	permset;		/* r/w/x bits */
	int	id;			/* uid/gid for user/group */
	struct aes name;		/* uname/gname */
};

static void	aes_clean(struct aes *);
static void	aes_copy(struct aes *dest, struct aes *src);
static const char *	aes_get_mbs(struct aes *);
static const wchar_t *	aes_get_wcs(struct aes *);
static void	aes_set_mbs(struct aes *, const char *mbs);
static void	aes_copy_mbs(struct aes *, const char *mbs);
/* static void	aes_set_wcs(struct aes *, const wchar_t *wcs); */
static void	aes_copy_wcs(struct aes *, const wchar_t *wcs);

static char *	 ae_fflagstostr(unsigned long bitset, unsigned long bitclear);
static const wchar_t	*ae_wcstofflags(const wchar_t *stringp,
		    unsigned long *setp, unsigned long *clrp);
static void	append_entry_w(wchar_t **wp, const wchar_t *prefix, int tag,
		    const wchar_t *wname, int perm, int id);
static void	append_id_w(wchar_t **wp, int id);

static int	acl_special(struct archive_entry *entry,
		    int type, int permset, int tag);
static struct ae_acl *acl_new_entry(struct archive_entry *entry,
		    int type, int permset, int tag, int id);
static void	next_field_w(const wchar_t **wp, const wchar_t **start,
		    const wchar_t **end, wchar_t *sep);
static int	prefix_w(const wchar_t *start, const wchar_t *end,
		    const wchar_t *test);


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
	 * Note that ae_stat.st_mode & S_IFMT  can be  0!
	 *
	 * This occurs when the actual file type of the object is not
	 * in the archive.  For example, 'tar' archives store
	 * hardlinks without marking the type of the underlying
	 * object.
	 */
	struct stat ae_stat;

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

	struct ae_acl	*acl_head;
	struct ae_acl	*acl_p;
	int		 acl_state;	/* See acl_next for details. */
	wchar_t		*acl_text_w;
};

static void
aes_clean(struct aes *aes)
{
	if (aes->aes_mbs_alloc) {
		free(aes->aes_mbs_alloc);
		aes->aes_mbs_alloc = NULL;
	}
	if (aes->aes_wcs_alloc) {
		free(aes->aes_wcs_alloc);
		aes->aes_wcs_alloc = NULL;
	}
	memset(aes, 0, sizeof(*aes));
}

static void
aes_copy(struct aes *dest, struct aes *src)
{
	*dest = *src;
	if (src->aes_mbs != NULL) {
		dest->aes_mbs_alloc = strdup(src->aes_mbs);
		dest->aes_mbs = dest->aes_mbs_alloc;
		if (dest->aes_mbs == NULL)
			__archive_errx(1, "No memory for aes_copy()");
	}

	if (src->aes_wcs != NULL) {
		dest->aes_wcs_alloc = malloc((wcslen(src->aes_wcs) + 1)
		    * sizeof(wchar_t));
		dest->aes_wcs = dest->aes_wcs_alloc;
		if (dest->aes_wcs == NULL)
			__archive_errx(1, "No memory for aes_copy()");
		wcscpy(dest->aes_wcs_alloc, src->aes_wcs);
	}
}

static const char *
aes_get_mbs(struct aes *aes)
{
	if (aes->aes_mbs == NULL && aes->aes_wcs != NULL) {
		/*
		 * XXX Need to estimate the number of byte in the
		 * multi-byte form.  Assume that, on average, wcs
		 * chars encode to no more than 3 bytes.  There must
		 * be a better way... XXX
		 */
		int mbs_length = wcslen(aes->aes_wcs) * 3 + 64;
		aes->aes_mbs_alloc = malloc(mbs_length);
		aes->aes_mbs = aes->aes_mbs_alloc;
		if (aes->aes_mbs == NULL)
			__archive_errx(1, "No memory for aes_get_mbs()");
		wcstombs(aes->aes_mbs_alloc, aes->aes_wcs, mbs_length - 1);
		aes->aes_mbs_alloc[mbs_length - 1] = 0;
	}
	return (aes->aes_mbs);
}

static const wchar_t *
aes_get_wcs(struct aes *aes)
{
	if (aes->aes_wcs == NULL && aes->aes_mbs != NULL) {
		/*
		 * No single byte will be more than one wide character,
		 * so this length estimate will always be big enough.
		 */
		int wcs_length = strlen(aes->aes_mbs);
		aes->aes_wcs_alloc
		    = malloc((wcs_length + 1) * sizeof(wchar_t));
		aes->aes_wcs = aes->aes_wcs_alloc;
		if (aes->aes_wcs == NULL)
			__archive_errx(1, "No memory for aes_get_wcs()");
		mbstowcs(aes->aes_wcs_alloc, aes->aes_mbs, wcs_length);
		aes->aes_wcs_alloc[wcs_length] = 0;
	}
	return (aes->aes_wcs);
}

static void
aes_set_mbs(struct aes *aes, const char *mbs)
{
	if (aes->aes_mbs_alloc) {
		free(aes->aes_mbs_alloc);
		aes->aes_mbs_alloc = NULL;
	}
	if (aes->aes_wcs_alloc) {
		free(aes->aes_wcs_alloc);
		aes->aes_wcs_alloc = NULL;
	}
	aes->aes_mbs = mbs;
	aes->aes_wcs = NULL;
}

static void
aes_copy_mbs(struct aes *aes, const char *mbs)
{
	if (aes->aes_mbs_alloc) {
		free(aes->aes_mbs_alloc);
		aes->aes_mbs_alloc = NULL;
	}
	if (aes->aes_wcs_alloc) {
		free(aes->aes_wcs_alloc);
		aes->aes_wcs_alloc = NULL;
	}
	aes->aes_mbs_alloc = malloc((strlen(mbs) + 1) * sizeof(char));
	if (aes->aes_mbs_alloc == NULL)
		__archive_errx(1, "No memory for aes_copy_mbs()");
	strcpy(aes->aes_mbs_alloc, mbs);
	aes->aes_mbs = aes->aes_mbs_alloc;
	aes->aes_wcs = NULL;
}

#if 0
static void
aes_set_wcs(struct aes *aes, const wchar_t *wcs)
{
	if (aes->aes_mbs_alloc) {
		free(aes->aes_mbs_alloc);
		aes->aes_mbs_alloc = NULL;
	}
	if (aes->aes_wcs_alloc) {
		free(aes->aes_wcs_alloc);
		aes->aes_wcs_alloc = NULL;
	}
	aes->aes_mbs = NULL;
	aes->aes_wcs = wcs;
}
#endif

static void
aes_copy_wcs(struct aes *aes, const wchar_t *wcs)
{
	if (aes->aes_mbs_alloc) {
		free(aes->aes_mbs_alloc);
		aes->aes_mbs_alloc = NULL;
	}
	if (aes->aes_wcs_alloc) {
		free(aes->aes_wcs_alloc);
		aes->aes_wcs_alloc = NULL;
	}
	aes->aes_mbs = NULL;
	aes->aes_wcs_alloc = malloc((wcslen(wcs) + 1) * sizeof(wchar_t));
	if (aes->aes_wcs_alloc == NULL)
		__archive_errx(1, "No memory for aes_copy_wcs()");
	wcscpy(aes->aes_wcs_alloc, wcs);
	aes->aes_wcs = aes->aes_wcs_alloc;
}

struct archive_entry *
archive_entry_clear(struct archive_entry *entry)
{
	aes_clean(&entry->ae_fflags_text);
	aes_clean(&entry->ae_gname);
	aes_clean(&entry->ae_hardlink);
	aes_clean(&entry->ae_pathname);
	aes_clean(&entry->ae_symlink);
	aes_clean(&entry->ae_uname);
	archive_entry_acl_clear(entry);
	memset(entry, 0, sizeof(*entry));
	return entry;
}

struct archive_entry *
archive_entry_clone(struct archive_entry *entry)
{
	struct archive_entry *entry2;

	/* Allocate new structure and copy over all of the fields. */
	entry2 = malloc(sizeof(*entry2));
	if (entry2 == NULL)
		return (NULL);
	memset(entry2, 0, sizeof(*entry2));
	entry2->ae_stat = entry->ae_stat;
	entry2->ae_fflags_set = entry->ae_fflags_set;
	entry2->ae_fflags_clear = entry->ae_fflags_clear;

	aes_copy(&entry2->ae_fflags_text, &entry->ae_fflags_text);
	aes_copy(&entry2->ae_gname, &entry->ae_gname);
	aes_copy(&entry2->ae_hardlink, &entry->ae_hardlink);
	aes_copy(&entry2->ae_pathname, &entry->ae_pathname);
	aes_copy(&entry2->ae_symlink, &entry->ae_symlink);
	aes_copy(&entry2->ae_uname, &entry->ae_uname);

	/* XXX TODO: Copy ACL data over as well. XXX */
	return (entry2);
}

void
archive_entry_free(struct archive_entry *entry)
{
	archive_entry_clear(entry);
	free(entry);
}

struct archive_entry *
archive_entry_new(void)
{
	struct archive_entry *entry;

	entry = malloc(sizeof(*entry));
	if (entry == NULL)
		return (NULL);
	memset(entry, 0, sizeof(*entry));
	return (entry);
}

/*
 * Functions for reading fields from an archive_entry.
 */

time_t
archive_entry_atime(struct archive_entry *entry)
{
	return (entry->ae_stat.st_atime);
}

long
archive_entry_atime_nsec(struct archive_entry *entry)
{
	(void)entry; /* entry can be unused here. */
	return (ARCHIVE_STAT_ATIME_NANOS(&entry->ae_stat));
}

time_t
archive_entry_ctime(struct archive_entry *entry)
{
	return (entry->ae_stat.st_ctime);
}

long
archive_entry_ctime_nsec(struct archive_entry *entry)
{
	(void)entry; /* entry can be unused here. */
	return (ARCHIVE_STAT_CTIME_NANOS(&entry->ae_stat));
}

dev_t
archive_entry_dev(struct archive_entry *entry)
{
	return (entry->ae_stat.st_dev);
}

void
archive_entry_fflags(struct archive_entry *entry,
    unsigned long *set, unsigned long *clear)
{
	*set = entry->ae_fflags_set;
	*clear = entry->ae_fflags_clear;
}

/*
 * Note: if text was provided, this just returns that text.  If you
 * really need the text to be rebuilt in a canonical form, set the
 * text, ask for the bitmaps, then set the bitmaps.  (Setting the
 * bitmaps clears any stored text.)  This design is deliberate: if
 * we're editing archives, we don't want to discard flags just because
 * they aren't supported on the current system.  The bitmap<->text
 * conversions are platform-specific (see below).
 */
const char *
archive_entry_fflags_text(struct archive_entry *entry)
{
	const char *f;
	char *p;

	f = aes_get_mbs(&entry->ae_fflags_text);
	if (f != NULL)
		return (f);

	if (entry->ae_fflags_set == 0  &&  entry->ae_fflags_clear == 0)
		return (NULL);

	p = ae_fflagstostr(entry->ae_fflags_set, entry->ae_fflags_clear);
	if (p == NULL)
		return (NULL);

	aes_copy_mbs(&entry->ae_fflags_text, p);
	free(p);
	f = aes_get_mbs(&entry->ae_fflags_text);
	return (f);
}

gid_t
archive_entry_gid(struct archive_entry *entry)
{
	return (entry->ae_stat.st_gid);
}

const char *
archive_entry_gname(struct archive_entry *entry)
{
	return (aes_get_mbs(&entry->ae_gname));
}

const char *
archive_entry_hardlink(struct archive_entry *entry)
{
	return (aes_get_mbs(&entry->ae_hardlink));
}

ino_t
archive_entry_ino(struct archive_entry *entry)
{
	return (entry->ae_stat.st_ino);
}

mode_t
archive_entry_mode(struct archive_entry *entry)
{
	return (entry->ae_stat.st_mode);
}

time_t
archive_entry_mtime(struct archive_entry *entry)
{
	return (entry->ae_stat.st_mtime);
}

long
archive_entry_mtime_nsec(struct archive_entry *entry)
{
	(void)entry; /* entry can be unused here. */
	return (ARCHIVE_STAT_MTIME_NANOS(&entry->ae_stat));
}

const char *
archive_entry_pathname(struct archive_entry *entry)
{
	return (aes_get_mbs(&entry->ae_pathname));
}

const wchar_t *
archive_entry_pathname_w(struct archive_entry *entry)
{
	return (aes_get_wcs(&entry->ae_pathname));
}

dev_t
archive_entry_rdev(struct archive_entry *entry)
{
	return (entry->ae_stat.st_rdev);
}

dev_t
archive_entry_rdevmajor(struct archive_entry *entry)
{
	return (major(entry->ae_stat.st_rdev));
}

dev_t
archive_entry_rdevminor(struct archive_entry *entry)
{
	return (minor(entry->ae_stat.st_rdev));
}

int64_t
archive_entry_size(struct archive_entry *entry)
{
	return (entry->ae_stat.st_size);
}

const struct stat *
archive_entry_stat(struct archive_entry *entry)
{
	return (&entry->ae_stat);
}

const char *
archive_entry_symlink(struct archive_entry *entry)
{
	return (aes_get_mbs(&entry->ae_symlink));
}

uid_t
archive_entry_uid(struct archive_entry *entry)
{
	return (entry->ae_stat.st_uid);
}

const char *
archive_entry_uname(struct archive_entry *entry)
{
	return (aes_get_mbs(&entry->ae_uname));
}

/*
 * Functions to set archive_entry properties.
 */

/*
 * Note "copy" not "set" here.  The "set" functions that accept a pointer
 * only store the pointer; they don't copy the underlying object.
 */
void
archive_entry_copy_stat(struct archive_entry *entry, const struct stat *st)
{
	entry->ae_stat = *st;
}

void
archive_entry_set_fflags(struct archive_entry *entry,
    unsigned long set, unsigned long clear)
{
	aes_clean(&entry->ae_fflags_text);
	entry->ae_fflags_set = set;
	entry->ae_fflags_clear = clear;
}

const wchar_t *
archive_entry_copy_fflags_text_w(struct archive_entry *entry,
    const wchar_t *flags)
{
	aes_copy_wcs(&entry->ae_fflags_text, flags);
	return (ae_wcstofflags(flags,
		    &entry->ae_fflags_set, &entry->ae_fflags_clear));
}

void
archive_entry_set_gid(struct archive_entry *entry, gid_t g)
{
	entry->ae_stat.st_gid = g;
}

void
archive_entry_set_gname(struct archive_entry *entry, const char *name)
{
	aes_set_mbs(&entry->ae_gname, name);
}

void
archive_entry_copy_gname_w(struct archive_entry *entry, const wchar_t *name)
{
	aes_copy_wcs(&entry->ae_gname, name);
}

void
archive_entry_set_hardlink(struct archive_entry *entry, const char *target)
{
	aes_set_mbs(&entry->ae_hardlink, target);
}

void
archive_entry_copy_hardlink(struct archive_entry *entry, const char *target)
{
	aes_copy_mbs(&entry->ae_hardlink, target);
}

void
archive_entry_copy_hardlink_w(struct archive_entry *entry, const wchar_t *target)
{
	aes_copy_wcs(&entry->ae_hardlink, target);
}

void
archive_entry_set_atime(struct archive_entry *entry, time_t t, long ns)
{
	entry->ae_stat.st_atime = t;
	ARCHIVE_STAT_SET_ATIME_NANOS(&entry->ae_stat, ns);
}

void
archive_entry_set_ctime(struct archive_entry *entry, time_t t, long ns)
{
	entry->ae_stat.st_ctime = t;
	ARCHIVE_STAT_SET_CTIME_NANOS(&entry->ae_stat, ns);
}

/* Set symlink if symlink is already set, else set hardlink. */
void
archive_entry_set_link(struct archive_entry *entry, const char *target)
{
	if (entry->ae_symlink.aes_mbs != NULL ||
	    entry->ae_symlink.aes_wcs != NULL)
		aes_set_mbs(&entry->ae_symlink, target);
	else
		aes_set_mbs(&entry->ae_hardlink, target);
}

void
archive_entry_set_mode(struct archive_entry *entry, mode_t m)
{
	entry->ae_stat.st_mode = m;
}

void
archive_entry_set_mtime(struct archive_entry *entry, time_t m, long ns)
{
	entry->ae_stat.st_mtime = m;
	ARCHIVE_STAT_SET_MTIME_NANOS(&entry->ae_stat, ns);
}

void
archive_entry_set_pathname(struct archive_entry *entry, const char *name)
{
	aes_set_mbs(&entry->ae_pathname, name);
}

void
archive_entry_copy_pathname(struct archive_entry *entry, const char *name)
{
	aes_copy_mbs(&entry->ae_pathname, name);
}

void
archive_entry_copy_pathname_w(struct archive_entry *entry, const wchar_t *name)
{
	aes_copy_wcs(&entry->ae_pathname, name);
}

void
archive_entry_set_rdevmajor(struct archive_entry *entry, dev_t m)
{
	dev_t d;

	d = entry->ae_stat.st_rdev;
	entry->ae_stat.st_rdev = makedev(major(m), minor(d));
}

void
archive_entry_set_rdevminor(struct archive_entry *entry, dev_t m)
{
	dev_t d;

	d = entry->ae_stat.st_rdev;
	entry->ae_stat.st_rdev = makedev(major(d), minor(m));
}

void
archive_entry_set_size(struct archive_entry *entry, int64_t s)
{
	entry->ae_stat.st_size = s;
}

void
archive_entry_set_symlink(struct archive_entry *entry, const char *linkname)
{
	aes_set_mbs(&entry->ae_symlink, linkname);
}

void
archive_entry_copy_symlink_w(struct archive_entry *entry, const wchar_t *linkname)
{
	aes_copy_wcs(&entry->ae_symlink, linkname);
}

void
archive_entry_set_uid(struct archive_entry *entry, uid_t u)
{
	entry->ae_stat.st_uid = u;
}

void
archive_entry_set_uname(struct archive_entry *entry, const char *name)
{
	aes_set_mbs(&entry->ae_uname, name);
}

void
archive_entry_copy_uname_w(struct archive_entry *entry, const wchar_t *name)
{
	aes_copy_wcs(&entry->ae_uname, name);
}

/*
 * ACL management.  The following would, of course, be a lot simpler
 * if: 1) the last draft of POSIX.1e were a really thorough and
 * complete standard that addressed the needs of ACL archiving and 2)
 * everyone followed it faithfully.  Alas, neither is true, so the
 * following is a lot more complex than might seem necessary to the
 * uninitiated.
 */

void
archive_entry_acl_clear(struct archive_entry *entry)
{
	struct ae_acl	*ap;

	while (entry->acl_head != NULL) {
		ap = entry->acl_head->next;
		aes_clean(&entry->acl_head->name);
		free(entry->acl_head);
		entry->acl_head = ap;
	}
	if (entry->acl_text_w != NULL) {
		free(entry->acl_text_w);
		entry->acl_text_w = NULL;
	}
	entry->acl_p = NULL;
	entry->acl_state = 0; /* Not counting. */
}

/*
 * Add a single ACL entry to the internal list of ACL data.
 */
void
archive_entry_acl_add_entry(struct archive_entry *entry,
    int type, int permset, int tag, int id, const char *name)
{
	struct ae_acl *ap;

	if (acl_special(entry, type, permset, tag) == 0)
		return;
	ap = acl_new_entry(entry, type, permset, tag, id);
	if (ap == NULL) {
		/* XXX Error XXX */
		return;
	}
	if (name != NULL  &&  *name != '\0')
		aes_copy_mbs(&ap->name, name);
	else
		aes_clean(&ap->name);
}

/*
 * As above, but with a wide-character name.
 */
void
archive_entry_acl_add_entry_w(struct archive_entry *entry,
    int type, int permset, int tag, int id, const wchar_t *name)
{
	struct ae_acl *ap;

	if (acl_special(entry, type, permset, tag) == 0)
		return;
	ap = acl_new_entry(entry, type, permset, tag, id);
	if (ap == NULL) {
		/* XXX Error XXX */
		return;
	}
	if (name != NULL  &&  *name != L'\0')
		aes_copy_wcs(&ap->name, name);
	else
		aes_clean(&ap->name);
}

/*
 * If this ACL entry is part of the standard POSIX permissions set,
 * store the permissions in the stat structure and return zero.
 */
static int
acl_special(struct archive_entry *entry, int type, int permset, int tag)
{
	if (type == ARCHIVE_ENTRY_ACL_TYPE_ACCESS) {
		switch (tag) {
		case ARCHIVE_ENTRY_ACL_USER_OBJ:
			entry->ae_stat.st_mode &= ~0700;
			entry->ae_stat.st_mode |= (permset & 7) << 6;
			return (0);
		case ARCHIVE_ENTRY_ACL_GROUP_OBJ:
			entry->ae_stat.st_mode &= ~0070;
			entry->ae_stat.st_mode |= (permset & 7) << 3;
			return (0);
		case ARCHIVE_ENTRY_ACL_OTHER:
			entry->ae_stat.st_mode &= ~0007;
			entry->ae_stat.st_mode |= permset & 7;
			return (0);
		}
	}
	return (1);
}

/*
 * Allocate and populate a new ACL entry with everything but the
 * name.
 */
static struct ae_acl *
acl_new_entry(struct archive_entry *entry,
    int type, int permset, int tag, int id)
{
	struct ae_acl *ap;

	if (type != ARCHIVE_ENTRY_ACL_TYPE_ACCESS &&
	    type != ARCHIVE_ENTRY_ACL_TYPE_DEFAULT)
		return (NULL);
	if (entry->acl_text_w != NULL) {
		free(entry->acl_text_w);
		entry->acl_text_w = NULL;
	}

	/* XXX TODO: More sanity-checks on the arguments XXX */

	/* If there's a matching entry already in the list, overwrite it. */
	for (ap = entry->acl_head; ap != NULL; ap = ap->next) {
		if (ap->type == type && ap->tag == tag && ap->id == id) {
			ap->permset = permset;
			return (ap);
		}
	}

	/* Add a new entry to the list. */
	ap = malloc(sizeof(*ap));
	if (ap == NULL)
		return (NULL);
	memset(ap, 0, sizeof(*ap));
	ap->next = entry->acl_head;
	entry->acl_head = ap;
	ap->type = type;
	ap->tag = tag;
	ap->id = id;
	ap->permset = permset;
	return (ap);
}

/*
 * Return a count of entries matching "want_type".
 */
int
archive_entry_acl_count(struct archive_entry *entry, int want_type)
{
	int count;
	struct ae_acl *ap;

	count = 0;
	ap = entry->acl_head;
	while (ap != NULL) {
		if ((ap->type & want_type) != 0)
			count++;
		ap = ap->next;
	}

	if (count > 0 && ((want_type & ARCHIVE_ENTRY_ACL_TYPE_ACCESS) != 0))
		count += 3;
	return (count);
}

/*
 * Prepare for reading entries from the ACL data.  Returns a count
 * of entries matching "want_type", or zero if there are no
 * non-extended ACL entries of that type.
 */
int
archive_entry_acl_reset(struct archive_entry *entry, int want_type)
{
	int count, cutoff;

	count = archive_entry_acl_count(entry, want_type);

	/*
	 * If the only entries are the three standard ones,
	 * then don't return any ACL data.  (In this case,
	 * client can just use chmod(2) to set permissions.)
	 */
	if ((want_type & ARCHIVE_ENTRY_ACL_TYPE_ACCESS) != 0)
		cutoff = 3;
	else
		cutoff = 0;

	if (count > cutoff)
		entry->acl_state = ARCHIVE_ENTRY_ACL_USER_OBJ;
	else
		entry->acl_state = 0;
	entry->acl_p = entry->acl_head;
	return (count);
}

/*
 * Return the next ACL entry in the list.  Fake entries for the
 * standard permissions and include them in the returned list.
 */

int
archive_entry_acl_next(struct archive_entry *entry, int want_type, int *type,
    int *permset, int *tag, int *id, const char **name)
{
	*name = NULL;
	*id = -1;

	/*
	 * The acl_state is either zero (no entries available), -1
	 * (reading from list), or an entry type (retrieve that type
	 * from ae_stat.st_mode).
	 */
	if (entry->acl_state == 0)
		return (ARCHIVE_WARN);

	/* The first three access entries are special. */
	if ((want_type & ARCHIVE_ENTRY_ACL_TYPE_ACCESS) != 0) {
		switch (entry->acl_state) {
		case ARCHIVE_ENTRY_ACL_USER_OBJ:
			*permset = (entry->ae_stat.st_mode >> 6) & 7;
			*type = ARCHIVE_ENTRY_ACL_TYPE_ACCESS;
			*tag = ARCHIVE_ENTRY_ACL_USER_OBJ;
			entry->acl_state = ARCHIVE_ENTRY_ACL_GROUP_OBJ;
			return (ARCHIVE_OK);
		case ARCHIVE_ENTRY_ACL_GROUP_OBJ:
			*permset = (entry->ae_stat.st_mode >> 3) & 7;
			*type = ARCHIVE_ENTRY_ACL_TYPE_ACCESS;
			*tag = ARCHIVE_ENTRY_ACL_GROUP_OBJ;
			entry->acl_state = ARCHIVE_ENTRY_ACL_OTHER;
			return (ARCHIVE_OK);
		case ARCHIVE_ENTRY_ACL_OTHER:
			*permset = entry->ae_stat.st_mode & 7;
			*type = ARCHIVE_ENTRY_ACL_TYPE_ACCESS;
			*tag = ARCHIVE_ENTRY_ACL_OTHER;
			entry->acl_state = -1;
			entry->acl_p = entry->acl_head;
			return (ARCHIVE_OK);
		default:
			break;
		}
	}

	while (entry->acl_p != NULL && (entry->acl_p->type & want_type) == 0)
		entry->acl_p = entry->acl_p->next;
	if (entry->acl_p == NULL) {
		entry->acl_state = 0;
		return (ARCHIVE_WARN);
	}
	*type = entry->acl_p->type;
	*permset = entry->acl_p->permset;
	*tag = entry->acl_p->tag;
	*id = entry->acl_p->id;
	*name = aes_get_mbs(&entry->acl_p->name);
	entry->acl_p = entry->acl_p->next;
	return (ARCHIVE_OK);
}

/*
 * Generate a text version of the ACL.  The flags parameter controls
 * the style of the generated ACL.
 */
const wchar_t *
archive_entry_acl_text_w(struct archive_entry *entry, int flags)
{
	int count;
	int length;
	const wchar_t *wname;
	const wchar_t *prefix;
	wchar_t separator;
	struct ae_acl *ap;
	int id;
	wchar_t *wp;

	if (entry->acl_text_w != NULL) {
		free (entry->acl_text_w);
		entry->acl_text_w = NULL;
	}

	separator = L',';
	count = 0;
	length = 0;
	ap = entry->acl_head;
	while (ap != NULL) {
		if ((ap->type & flags) != 0) {
			count++;
			if ((flags & ARCHIVE_ENTRY_ACL_STYLE_MARK_DEFAULT) &&
			    (ap->type & ARCHIVE_ENTRY_ACL_TYPE_DEFAULT))
				length += 8; /* "default:" */
			length += 5; /* tag name */
			length += 1; /* colon */
			wname = aes_get_wcs(&ap->name);
			if (wname != NULL)
				length += wcslen(wname);
			length ++; /* colon */
			length += 3; /* rwx */
			length += 1; /* colon */
			length += max(sizeof(uid_t),sizeof(gid_t)) * 3 + 1;
			length ++; /* newline */
		}
		ap = ap->next;
	}

	if (count > 0 && ((flags & ARCHIVE_ENTRY_ACL_TYPE_ACCESS) != 0)) {
		length += 10; /* "user::rwx\n" */
		length += 11; /* "group::rwx\n" */
		length += 11; /* "other::rwx\n" */
	}

	if (count == 0)
		return (NULL);

	/* Now, allocate the string and actually populate it. */
	wp = entry->acl_text_w = malloc(length * sizeof(wchar_t));
	if (wp == NULL)
		__archive_errx(1, "No memory to generate the text version of the ACL");
	count = 0;
	if ((flags & ARCHIVE_ENTRY_ACL_TYPE_ACCESS) != 0) {
		append_entry_w(&wp, NULL, ARCHIVE_ENTRY_ACL_USER_OBJ, NULL,
		    entry->ae_stat.st_mode & 0700, -1);
		*wp++ = ',';
		append_entry_w(&wp, NULL, ARCHIVE_ENTRY_ACL_GROUP_OBJ, NULL,
		    entry->ae_stat.st_mode & 0070, -1);
		*wp++ = ',';
		append_entry_w(&wp, NULL, ARCHIVE_ENTRY_ACL_OTHER, NULL,
		    entry->ae_stat.st_mode & 0007, -1);
		count += 3;

		ap = entry->acl_head;
		while (ap != NULL) {
			if ((ap->type & ARCHIVE_ENTRY_ACL_TYPE_ACCESS) != 0) {
				wname = aes_get_wcs(&ap->name);
				*wp++ = separator;
				if (flags & ARCHIVE_ENTRY_ACL_STYLE_EXTRA_ID)
					id = ap->id;
				else
					id = -1;
				append_entry_w(&wp, NULL, ap->tag, wname,
				    ap->permset, id);
				count++;
			}
			ap = ap->next;
		}
	}


	if ((flags & ARCHIVE_ENTRY_ACL_TYPE_DEFAULT) != 0) {
		if (flags & ARCHIVE_ENTRY_ACL_STYLE_MARK_DEFAULT)
			prefix = L"default:";
		else
			prefix = NULL;
		ap = entry->acl_head;
		count = 0;
		while (ap != NULL) {
			if ((ap->type & ARCHIVE_ENTRY_ACL_TYPE_DEFAULT) != 0) {
				wname = aes_get_wcs(&ap->name);
				if (count > 0)
					*wp++ = separator;
				if (flags & ARCHIVE_ENTRY_ACL_STYLE_EXTRA_ID)
					id = ap->id;
				else
					id = -1;
				append_entry_w(&wp, prefix, ap->tag,
				    wname, ap->permset, id);
				count ++;
			}
			ap = ap->next;
		}
	}

	return (entry->acl_text_w);
}

static void
append_id_w(wchar_t **wp, int id)
{
	if (id > 9)
		append_id_w(wp, id / 10);
	*(*wp)++ = L"0123456789"[id % 10];
}

static void
append_entry_w(wchar_t **wp, const wchar_t *prefix, int tag,
    const wchar_t *wname, int perm, int id)
{
	if (prefix != NULL) {
		wcscpy(*wp, prefix);
		*wp += wcslen(*wp);
	}
	switch (tag) {
	case ARCHIVE_ENTRY_ACL_USER_OBJ:
		wname = NULL;
		id = -1;
		/* FALL THROUGH */
	case ARCHIVE_ENTRY_ACL_USER:
		wcscpy(*wp, L"user");
		break;
	case ARCHIVE_ENTRY_ACL_GROUP_OBJ:
		wname = NULL;
		id = -1;
		/* FALL THROUGH */
	case ARCHIVE_ENTRY_ACL_GROUP:
		wcscpy(*wp, L"group");
		break;
	case ARCHIVE_ENTRY_ACL_MASK:
		wcscpy(*wp, L"mask");
		wname = NULL;
		id = -1;
		break;
	case ARCHIVE_ENTRY_ACL_OTHER:
		wcscpy(*wp, L"other");
		wname = NULL;
		id = -1;
		break;
	}
	*wp += wcslen(*wp);
	*(*wp)++ = L':';
	if (wname != NULL) {
		wcscpy(*wp, wname);
		*wp += wcslen(*wp);
	}
	*(*wp)++ = L':';
	*(*wp)++ = (perm & 0444) ? L'r' : L'-';
	*(*wp)++ = (perm & 0222) ? L'w' : L'-';
	*(*wp)++ = (perm & 0111) ? L'x' : L'-';
	if (id != -1) {
		*(*wp)++ = L':';
		append_id_w(wp, id);
	}
	**wp = L'\0';
}

/*
 * Parse a textual ACL.  This automatically recognizes and supports
 * extensions described above.  The 'type' argument is used to
 * indicate the type that should be used for any entries not
 * explicitly marked as "default:".
 */
int
__archive_entry_acl_parse_w(struct archive_entry *entry,
    const wchar_t *text, int default_type)
{
	int type, tag, permset, id;
	const wchar_t *start, *end;
	const wchar_t *name_start, *name_end;
	wchar_t sep;
	wchar_t *namebuff;
	int namebuff_length;

	name_start = name_end = NULL;
	namebuff = NULL;
	namebuff_length = 0;

	while (text != NULL  &&  *text != L'\0') {
		next_field_w(&text, &start, &end, &sep);
		if (sep != L':')
			goto fail;

		/*
		 * Solaris extension:  "defaultuser::rwx" is the
		 * default ACL corresponding to "user::rwx", etc.
		 */
		if (end-start > 7  && wmemcmp(start, L"default", 7) == 0) {
			type = ARCHIVE_ENTRY_ACL_TYPE_DEFAULT;
			start += 7;
		} else
			type = default_type;

		if (prefix_w(start, end, L"user")) {
			next_field_w(&text, &start, &end, &sep);
			if (sep != L':')
				goto fail;
			if (end > start) {
				tag = ARCHIVE_ENTRY_ACL_USER;
				name_start = start;
				name_end = end;
			} else
				tag = ARCHIVE_ENTRY_ACL_USER_OBJ;
		} else if (prefix_w(start, end, L"group")) {
			next_field_w(&text, &start, &end, &sep);
			if (sep != L':')
				goto fail;
			if (end > start) {
				tag = ARCHIVE_ENTRY_ACL_GROUP;
				name_start = start;
				name_end = end;
			} else
				tag = ARCHIVE_ENTRY_ACL_GROUP_OBJ;
		} else if (prefix_w(start, end, L"other")) {
			next_field_w(&text, &start, &end, &sep);
			if (sep != L':')
				goto fail;
			if (end > start)
				goto fail;
			tag = ARCHIVE_ENTRY_ACL_OTHER;
		} else if (prefix_w(start, end, L"mask")) {
			next_field_w(&text, &start, &end, &sep);
			if (sep != L':')
				goto fail;
			if (end > start)
				goto fail;
			tag = ARCHIVE_ENTRY_ACL_MASK;
		} else
			goto fail;

		next_field_w(&text, &start, &end, &sep);
		permset = 0;
		while (start < end) {
			switch (*start++) {
			case 'r': case 'R':
				permset |= ARCHIVE_ENTRY_ACL_READ;
				break;
			case 'w': case 'W':
				permset |= ARCHIVE_ENTRY_ACL_WRITE;
				break;
			case 'x': case 'X':
				permset |= ARCHIVE_ENTRY_ACL_EXECUTE;
				break;
			case '-':
				break;
			default:
				goto fail;
			}
		}

		/*
		 * Support star-compatible numeric UID/GID extension.
		 * This extension adds a ":" followed by the numeric
		 * ID so that "group:groupname:rwx", for example,
		 * becomes "group:groupname:rwx:999", where 999 is the
		 * numeric GID.  This extension makes it possible, for
		 * example, to correctly restore ACLs on a system that
		 * might have a damaged passwd file or be disconnected
		 * from a central NIS server.  This extension is compatible
		 * with POSIX.1e draft 17.
		 */
		if (sep == L':' && (tag == ARCHIVE_ENTRY_ACL_USER ||
		    tag == ARCHIVE_ENTRY_ACL_GROUP)) {
			next_field_w(&text, &start, &end, &sep);

			id = 0;
			while (start < end  && *start >= '0' && *start <= '9') {
				if (id > (INT_MAX / 10))
					id = INT_MAX;
				else {
					id *= 10;
					id += *start - '0';
					start++;
				}
			}
		} else
			id = -1; /* No id specified. */

		/* Skip any additional entries. */
		while (sep == L':') {
			next_field_w(&text, &start, &end, &sep);
		}

		/* Add entry to the internal list. */
		if (name_end == name_start) {
			archive_entry_acl_add_entry_w(entry, type, permset,
			    tag, id, NULL);
		} else {
			if (namebuff_length <= name_end - name_start) {
				if (namebuff != NULL)
					free(namebuff);
				namebuff_length = name_end - name_start + 256;
				namebuff =
				    malloc(namebuff_length * sizeof(wchar_t));
				if (namebuff == NULL)
					goto fail;
			}
			wmemcpy(namebuff, name_start, name_end - name_start);
			namebuff[name_end - name_start] = L'\0';
			archive_entry_acl_add_entry_w(entry, type,
			    permset, tag, id, namebuff);
		}
	}
	if (namebuff != NULL)
		free(namebuff);
	return (ARCHIVE_OK);

fail:
	if (namebuff != NULL)
		free(namebuff);
	return (ARCHIVE_WARN);
}

/*
 * Match "[:whitespace:]*(.*)[:whitespace:]*[:,\n]".  *wp is updated
 * to point to just after the separator.  *start points to the first
 * character of the matched text and *end just after the last
 * character of the matched identifier.  In particular *end - *start
 * is the length of the field body, not including leading or trailing
 * whitespace.
 */
static void
next_field_w(const wchar_t **wp, const wchar_t **start,
    const wchar_t **end, wchar_t *sep)
{
	/* Skip leading whitespace to find start of field. */
	while (**wp == L' ' || **wp == L'\t' || **wp == L'\n') {
		(*wp)++;
	}
	*start = *wp;

	/* Scan for the separator. */
	while (**wp != L'\0' && **wp != L',' && **wp != L':' &&
	    **wp != L'\n') {
		(*wp)++;
	}
	*sep = **wp;

	/* Trim trailing whitespace to locate end of field. */
	*end = *wp - 1;
	while (**end == L' ' || **end == L'\t' || **end == L'\n') {
		(*end)--;
	}
	(*end)++;

	/* Adjust scanner location. */
	if (**wp != L'\0')
		(*wp)++;
}

static int
prefix_w(const wchar_t *start, const wchar_t *end, const wchar_t *test)
{
	if (start == end)
		return (0);

	if (*start++ != *test++)
		return (0);

	while (start < end  &&  *start++ == *test++)
		;

	if (start < end)
		return (0);

	return (1);
}


/*
 * Following code is modified from UC Berkeley sources, and
 * is subject to the following copyright notice.
 */

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

static struct flag {
	const char	*name;
	const wchar_t	*wname;
	unsigned long	 set;
	unsigned long	 clear;
} flags[] = {
	/* Preferred (shorter) names per flag first, all prefixed by "no" */
#ifdef SF_APPEND
	{ "nosappnd",	L"nosappnd",		SF_APPEND,	0 },
	{ "nosappend",	L"nosappend",		SF_APPEND,	0 },
#endif
#ifdef  EXT2_APPEND_FL				/* 'a' */
	{ "nosappnd",	L"nosappnd",		EXT2_APPEND_FL,	0 },
	{ "nosappend",	L"nosappend",		EXT2_APPEND_FL,	0 },
#endif
#ifdef SF_ARCHIVED
	{ "noarch",	L"noarch",		SF_ARCHIVED,	0 },
	{ "noarchived",	L"noarchived",       	SF_ARCHIVED,	0 },
#endif
#ifdef SF_IMMUTABLE
	{ "noschg",	L"noschg",		SF_IMMUTABLE,	0 },
	{ "noschange",	L"noschange",		SF_IMMUTABLE,	0 },
	{ "nosimmutable",	L"nosimmutable",	SF_IMMUTABLE,	0 },
#endif
#ifdef EXT2_IMMUTABLE_FL			/* 'i' */
	{ "noschg",	L"noschg",		EXT2_IMMUTABLE_FL,	0 },
	{ "noschange",	L"noschange",		EXT2_IMMUTABLE_FL,	0 },
	{ "nosimmutable",	L"nosimmutable",	EXT2_IMMUTABLE_FL,	0 },
#endif
#ifdef SF_NOUNLINK
	{ "nosunlnk",	L"nosunlnk",		SF_NOUNLINK,	0 },
	{ "nosunlink",	L"nosunlink",		SF_NOUNLINK,	0 },
#endif
#ifdef SF_SNAPSHOT
	{ "nosnapshot",	L"nosnapshot",	SF_SNAPSHOT,	0 },
#endif
#ifdef UF_APPEND
	{ "nouappnd",	L"nouappnd",		UF_APPEND,	0 },
	{ "nouappend",	L"nouappend",		UF_APPEND,	0 },
#endif
#ifdef UF_IMMUTABLE
	{ "nouchg",	L"nouchg",		UF_IMMUTABLE,	0 },
	{ "nouchange",	L"nouchange",		UF_IMMUTABLE,	0 },
	{ "nouimmutable",	L"nouimmutable",	UF_IMMUTABLE,	0 },
#endif
#ifdef UF_NODUMP
	{ "nodump",	L"nodump",		0,		UF_NODUMP},
#endif
#ifdef EXT2_NODUMP_FL				/* 'd' */
	{ "nodump",	L"nodump",		0,		EXT2_NODUMP_FL},
#endif
#ifdef UF_OPAQUE
	{ "noopaque",	L"noopaque",		UF_OPAQUE,	0 },
#endif
#ifdef UF_NOUNLINK
	{ "nouunlnk",	L"nouunlnk",		UF_NOUNLINK,	0 },
	{ "nouunlink",	L"nouunlink",		UF_NOUNLINK,	0 },
#endif
#ifdef EXT2_COMPR_FL				/* 'c' */
        { "nocompress",	L"nocompress",       	EXT2_COMPR_FL,	0 },
#endif

#ifdef EXT2_NOATIME_FL				/* 'A' */
        { "noatime",	L"noatime",		0,		EXT2_NOATIME_FL},
#endif
	{ NULL,		NULL,			0,		0 }
};

/*
 * fflagstostr --
 *	Convert file flags to a comma-separated string.  If no flags
 *	are set, return the empty string.
 */
char *
ae_fflagstostr(unsigned long bitset, unsigned long bitclear)
{
	char *string, *dp;
	const char *sp;
	unsigned long bits;
	struct flag *flag;
	int	length;

	bits = bitset | bitclear;
	length = 0;
	for (flag = flags; flag->name != NULL; flag++)
		if (bits & (flag->set | flag->clear)) {
			length += strlen(flag->name) + 1;
			bits &= ~(flag->set | flag->clear);
		}

	if (length == 0)
		return (NULL);
	string = malloc(length);
	if (string == NULL)
		return (NULL);

	dp = string;
	for (flag = flags; flag->name != NULL; flag++) {
		if (bitset & flag->set || bitclear & flag->clear) {
			sp = flag->name + 2;
		} else if (bitset & flag->clear  ||  bitclear & flag->set) {
			sp = flag->name;
		} else
			continue;
		bitset &= ~(flag->set | flag->clear);
		bitclear &= ~(flag->set | flag->clear);
		if (dp > string)
			*dp++ = ',';
		while ((*dp++ = *sp++) != '\0')
			;
		dp--;
	}

	*dp = '\0';
	return (string);
}

/*
 * wcstofflags --
 *	Take string of arguments and return file flags.  This
 *	version works a little differently than strtofflags(3).
 *	In particular, it always tests every token, skipping any
 *	unrecognized tokens.  It returns a pointer to the first
 *	unrecognized token, or NULL if every token was recognized.
 *	This version is also const-correct and does not modify the
 *	provided string.
 */
const wchar_t *
ae_wcstofflags(const wchar_t *s, unsigned long *setp, unsigned long *clrp)
{
	const wchar_t *start, *end;
	struct flag *flag;
	unsigned long set, clear;
	const wchar_t *failed;

	set = clear = 0;
	start = s;
	failed = NULL;
	/* Find start of first token. */
	while (*start == L'\t'  ||  *start == L' '  ||  *start == L',')
		start++;
	while (*start != L'\0') {
		/* Locate end of token. */
		end = start;
		while (*end != L'\0'  &&  *end != L'\t'  &&
		    *end != L' '  &&  *end != L',')
			end++;
		for (flag = flags; flag->wname != NULL; flag++) {
			if (wmemcmp(start, flag->wname, end - start) == 0) {
				/* Matched "noXXXX", so reverse the sense. */
				clear |= flag->set;
				set |= flag->clear;
				break;
			} else if (wmemcmp(start, flag->wname + 2, end - start)
			    == 0) {
				/* Matched "XXXX", so don't reverse. */
				set |= flag->set;
				clear |= flag->clear;
				break;
			}
		}
		/* Ignore unknown flag names. */
		if (flag->wname == NULL  &&  failed == NULL)
			failed = start;

		/* Find start of next token. */
		start = end;
		while (*start == L'\t'  ||  *start == L' '  ||  *start == L',')
			start++;

	}

	if (setp)
		*setp = set;
	if (clrp)
		*clrp = clear;

	/* Return location of first failure. */
	return (failed);
}


#ifdef TEST
#include <stdio.h>
int
main(int argc, char **argv)
{
	struct archive_entry *entry = archive_entry_new();
	unsigned long set, clear;
	const wchar_t *remainder;

	remainder = archive_entry_copy_fflags_text_w(entry, L"nosappnd dump archive,,,,,,,");
	archive_entry_fflags(entry, &set, &clear);

	wprintf(L"set=0x%lX clear=0x%lX remainder='%ls'\n", set, clear, remainder);

	wprintf(L"new flags='%s'\n", archive_entry_fflags_text(entry));
	return (0);
}
#endif
