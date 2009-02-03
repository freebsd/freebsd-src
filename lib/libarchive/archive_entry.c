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
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef MAJOR_IN_MKDEV
#include <sys/mkdev.h>
#else
#ifdef MAJOR_IN_SYSMACROS
#include <sys/sysmacros.h>
#endif
#endif
#ifdef HAVE_EXT2FS_EXT2_FS_H
#include <ext2fs/ext2_fs.h>	/* for Linux file flags */
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_LINUX_FS_H
#include <linux/fs.h>	/* for Linux file flags */
#endif
#ifdef HAVE_LINUX_EXT2_FS_H
#include <linux/ext2_fs.h>	/* for Linux file flags */
#endif
#include <stddef.h>
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_entry_private.h"

#undef max
#define	max(a, b)	((a)>(b)?(a):(b))

/* Play games to come up with a suitable makedev() definition. */
#ifdef __QNXNTO__
/* QNX.  <sigh> */
#include <sys/netmgr.h>
#define ae_makedev(maj, min) makedev(ND_LOCAL_NODE, (maj), (min))
#elif defined makedev
/* There's a "makedev" macro. */
#define ae_makedev(maj, min) makedev((maj), (min))
#elif defined mkdev || defined _WIN32 || defined __WIN32__
/* Windows. <sigh> */
#define ae_makedev(maj, min) mkdev((maj), (min))
#else
/* There's a "makedev" function. */
#define ae_makedev(maj, min) makedev((maj), (min))
#endif

static void	aes_clean(struct aes *);
static void	aes_copy(struct aes *dest, struct aes *src);
static const char *	aes_get_mbs(struct aes *);
static const wchar_t *	aes_get_wcs(struct aes *);
static int	aes_set_mbs(struct aes *, const char *mbs);
static int	aes_copy_mbs(struct aes *, const char *mbs);
/* static void	aes_set_wcs(struct aes *, const wchar_t *wcs); */
static int	aes_copy_wcs(struct aes *, const wchar_t *wcs);
static int	aes_copy_wcs_len(struct aes *, const wchar_t *wcs, size_t);

static char *	 ae_fflagstostr(unsigned long bitset, unsigned long bitclear);
static const wchar_t	*ae_wcstofflags(const wchar_t *stringp,
		    unsigned long *setp, unsigned long *clrp);
static const char	*ae_strtofflags(const char *stringp,
		    unsigned long *setp, unsigned long *clrp);
static void	append_entry_w(wchar_t **wp, const wchar_t *prefix, int tag,
		    const wchar_t *wname, int perm, int id);
static void	append_id_w(wchar_t **wp, int id);

static int	acl_special(struct archive_entry *entry,
		    int type, int permset, int tag);
static struct ae_acl *acl_new_entry(struct archive_entry *entry,
		    int type, int permset, int tag, int id);
static int	isint_w(const wchar_t *start, const wchar_t *end, int *result);
static void	next_field_w(const wchar_t **wp, const wchar_t **start,
		    const wchar_t **end, wchar_t *sep);
static int	prefix_w(const wchar_t *start, const wchar_t *end,
		    const wchar_t *test);
static void
archive_entry_acl_add_entry_w_len(struct archive_entry *entry, int type,
		    int permset, int tag, int id, const wchar_t *name, size_t);


#ifndef HAVE_WCSCPY
static wchar_t * wcscpy(wchar_t *s1, const wchar_t *s2)
{
	wchar_t *dest = s1;
	while ((*s1 = *s2) != L'\0')
		++s1, ++s2;
	return dest;
}
#endif
#ifndef HAVE_WCSLEN
static size_t wcslen(const wchar_t *s)
{
	const wchar_t *p = s;
	while (*p != L'\0')
		++p;
	return p - s;
}
#endif
#ifndef HAVE_WMEMCMP
/* Good enough for simple equality testing, but not for sorting. */
#define wmemcmp(a,b,i)  memcmp((a), (b), (i) * sizeof(wchar_t))
#endif
#ifndef HAVE_WMEMCPY
#define wmemcpy(a,b,i)  (wchar_t *)memcpy((a), (b), (i) * sizeof(wchar_t))
#endif

static void
aes_clean(struct aes *aes)
{
	if (aes->aes_wcs) {
		free((wchar_t *)(uintptr_t)aes->aes_wcs);
		aes->aes_wcs = NULL;
	}
	archive_string_free(&(aes->aes_mbs));
	archive_string_free(&(aes->aes_utf8));
	aes->aes_set = 0;
}

static void
aes_copy(struct aes *dest, struct aes *src)
{
	wchar_t *wp;

	dest->aes_set = src->aes_set;
	archive_string_copy(&(dest->aes_mbs), &(src->aes_mbs));
	archive_string_copy(&(dest->aes_utf8), &(src->aes_utf8));

	if (src->aes_wcs != NULL) {
		wp = (wchar_t *)malloc((wcslen(src->aes_wcs) + 1)
		    * sizeof(wchar_t));
		if (wp == NULL)
			__archive_errx(1, "No memory for aes_copy()");
		wcscpy(wp, src->aes_wcs);
		dest->aes_wcs = wp;
	}
}

static const char *
aes_get_utf8(struct aes *aes)
{
	if (aes->aes_set & AES_SET_UTF8)
		return (aes->aes_utf8.s);
	if ((aes->aes_set & AES_SET_WCS)
	    && archive_strappend_w_utf8(&(aes->aes_utf8), aes->aes_wcs) != NULL) {
		aes->aes_set |= AES_SET_UTF8;
		return (aes->aes_utf8.s);
	}
	return (NULL);
}

static const char *
aes_get_mbs(struct aes *aes)
{
	/* If we already have an MBS form, return that immediately. */
	if (aes->aes_set & AES_SET_MBS)
		return (aes->aes_mbs.s);
	/* If there's a WCS form, try converting with the native locale. */
	if ((aes->aes_set & AES_SET_WCS)
	    && archive_strappend_w_mbs(&(aes->aes_mbs), aes->aes_wcs) != NULL) {
		aes->aes_set |= AES_SET_MBS;
		return (aes->aes_mbs.s);
	}
	/* We'll use UTF-8 for MBS if all else fails. */
	return (aes_get_utf8(aes));
}

static const wchar_t *
aes_get_wcs(struct aes *aes)
{
	wchar_t *w;
	int r;

	/* Return WCS form if we already have it. */
	if (aes->aes_set & AES_SET_WCS)
		return (aes->aes_wcs);

	if (aes->aes_set & AES_SET_MBS) {
		/* Try converting MBS to WCS using native locale. */
		/*
		 * No single byte will be more than one wide character,
		 * so this length estimate will always be big enough.
		 */
		size_t wcs_length = aes->aes_mbs.length;

		w = (wchar_t *)malloc((wcs_length + 1) * sizeof(wchar_t));
		if (w == NULL)
			__archive_errx(1, "No memory for aes_get_wcs()");
		r = mbstowcs(w, aes->aes_mbs.s, wcs_length);
		w[wcs_length] = 0;
		if (r > 0) {
			aes->aes_set |= AES_SET_WCS;
			return (aes->aes_wcs = w);
		}
		free(w);
	}

	if (aes->aes_set & AES_SET_UTF8) {
		/* Try converting UTF8 to WCS. */
		aes->aes_wcs = __archive_string_utf8_w(&(aes->aes_utf8));
		aes->aes_set |= AES_SET_WCS;
		return (aes->aes_wcs);
	}
	return (NULL);
}

static int
aes_set_mbs(struct aes *aes, const char *mbs)
{
	return (aes_copy_mbs(aes, mbs));
}

static int
aes_copy_mbs(struct aes *aes, const char *mbs)
{
	if (mbs == NULL) {
		aes->aes_set = 0;
		return (0);
	}
	aes->aes_set = AES_SET_MBS; /* Only MBS form is set now. */
	archive_strcpy(&(aes->aes_mbs), mbs);
	archive_string_empty(&(aes->aes_utf8));
	if (aes->aes_wcs) {
		free((wchar_t *)(uintptr_t)aes->aes_wcs);
		aes->aes_wcs = NULL;
	}
	return (0);
}

/*
 * The 'update' form tries to proactively update all forms of
 * this string (WCS and MBS) and returns an error if any of
 * them fail.  This is used by the 'pax' handler, for instance,
 * to detect and report character-conversion failures early while
 * still allowing clients to get potentially useful values from
 * the more tolerant lazy conversions.  (get_mbs and get_wcs will
 * strive to give the user something useful, so you can get hopefully
 * usable values even if some of the character conversions are failing.)
 */
static int
aes_update_utf8(struct aes *aes, const char *utf8)
{
	if (utf8 == NULL) {
		aes->aes_set = 0;
		return (1); /* Succeeded in clearing everything. */
	}

	/* Save the UTF8 string. */
	archive_strcpy(&(aes->aes_utf8), utf8);

	/* Empty the mbs and wcs strings. */
	archive_string_empty(&(aes->aes_mbs));
	if (aes->aes_wcs) {
		free((wchar_t *)(uintptr_t)aes->aes_wcs);
		aes->aes_wcs = NULL;
	}

	aes->aes_set = AES_SET_UTF8;	/* Only UTF8 is set now. */

	/* TODO: We should just do a direct UTF-8 to MBS conversion
	 * here.  That would be faster, use less space, and give the
	 * same information.  (If a UTF-8 to MBS conversion succeeds,
	 * then UTF-8->WCS and Unicode->MBS conversions will both
	 * succeed.) */

	/* Try converting UTF8 to WCS, return false on failure. */
	aes->aes_wcs = __archive_string_utf8_w(&(aes->aes_utf8));
	if (aes->aes_wcs == NULL)
		return (0);
	aes->aes_set = AES_SET_UTF8 | AES_SET_WCS; /* Both UTF8 and WCS set. */

	/* Try converting WCS to MBS, return false on failure. */
	if (archive_strappend_w_mbs(&(aes->aes_mbs), aes->aes_wcs) == NULL)
		return (0);
	aes->aes_set = AES_SET_UTF8 | AES_SET_WCS | AES_SET_MBS;

	/* All conversions succeeded. */
	return (1);
}

static int
aes_copy_wcs(struct aes *aes, const wchar_t *wcs)
{
	return aes_copy_wcs_len(aes, wcs, wcs == NULL ? 0 : wcslen(wcs));
}

static int
aes_copy_wcs_len(struct aes *aes, const wchar_t *wcs, size_t len)
{
	wchar_t *w;

	if (wcs == NULL) {
		aes->aes_set = 0;
		return (0);
	}
	aes->aes_set = AES_SET_WCS; /* Only WCS form set. */
	archive_string_empty(&(aes->aes_mbs));
	archive_string_empty(&(aes->aes_utf8));
	if (aes->aes_wcs) {
		free((wchar_t *)(uintptr_t)aes->aes_wcs);
		aes->aes_wcs = NULL;
	}
	w = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));
	if (w == NULL)
		__archive_errx(1, "No memory for aes_copy_wcs()");
	wmemcpy(w, wcs, len);
	w[len] = L'\0';
	aes->aes_wcs = w;
	return (0);
}

/****************************************************************************
 *
 * Public Interface
 *
 ****************************************************************************/

struct archive_entry *
archive_entry_clear(struct archive_entry *entry)
{
	if (entry == NULL)
		return (NULL);
	aes_clean(&entry->ae_fflags_text);
	aes_clean(&entry->ae_gname);
	aes_clean(&entry->ae_hardlink);
	aes_clean(&entry->ae_pathname);
	aes_clean(&entry->ae_symlink);
	aes_clean(&entry->ae_uname);
	archive_entry_acl_clear(entry);
	archive_entry_xattr_clear(entry);
	free(entry->stat);
	memset(entry, 0, sizeof(*entry));
	return entry;
}

struct archive_entry *
archive_entry_clone(struct archive_entry *entry)
{
	struct archive_entry *entry2;
	struct ae_acl *ap, *ap2;
	struct ae_xattr *xp;

	/* Allocate new structure and copy over all of the fields. */
	entry2 = (struct archive_entry *)malloc(sizeof(*entry2));
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
	entry2->ae_set = entry->ae_set;
	aes_copy(&entry2->ae_uname, &entry->ae_uname);

	/* Copy ACL data over. */
	ap = entry->acl_head;
	while (ap != NULL) {
		ap2 = acl_new_entry(entry2,
		    ap->type, ap->permset, ap->tag, ap->id);
		if (ap2 != NULL)
			aes_copy(&ap2->name, &ap->name);
		ap = ap->next;
	}

	/* Copy xattr data over. */
	xp = entry->xattr_head;
	while (xp != NULL) {
		archive_entry_xattr_add_entry(entry2,
		    xp->name, xp->value, xp->size);
		xp = xp->next;
	}

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

	entry = (struct archive_entry *)malloc(sizeof(*entry));
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
	return (entry->ae_stat.aest_atime);
}

long
archive_entry_atime_nsec(struct archive_entry *entry)
{
	return (entry->ae_stat.aest_atime_nsec);
}

int
archive_entry_atime_is_set(struct archive_entry *entry)
{
	return (entry->ae_set & AE_SET_ATIME);
}

time_t
archive_entry_ctime(struct archive_entry *entry)
{
	return (entry->ae_stat.aest_ctime);
}

int
archive_entry_ctime_is_set(struct archive_entry *entry)
{
	return (entry->ae_set & AE_SET_CTIME);
}

long
archive_entry_ctime_nsec(struct archive_entry *entry)
{
	return (entry->ae_stat.aest_ctime_nsec);
}

dev_t
archive_entry_dev(struct archive_entry *entry)
{
	if (entry->ae_stat.aest_dev_is_broken_down)
		return ae_makedev(entry->ae_stat.aest_devmajor,
		    entry->ae_stat.aest_devminor);
	else
		return (entry->ae_stat.aest_dev);
}

dev_t
archive_entry_devmajor(struct archive_entry *entry)
{
	if (entry->ae_stat.aest_dev_is_broken_down)
		return (entry->ae_stat.aest_devmajor);
	else
		return major(entry->ae_stat.aest_dev);
}

dev_t
archive_entry_devminor(struct archive_entry *entry)
{
	if (entry->ae_stat.aest_dev_is_broken_down)
		return (entry->ae_stat.aest_devminor);
	else
		return minor(entry->ae_stat.aest_dev);
}

mode_t
archive_entry_filetype(struct archive_entry *entry)
{
	return (AE_IFMT & entry->ae_stat.aest_mode);
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
	return (entry->ae_stat.aest_gid);
}

const char *
archive_entry_gname(struct archive_entry *entry)
{
	return (aes_get_mbs(&entry->ae_gname));
}

const wchar_t *
archive_entry_gname_w(struct archive_entry *entry)
{
	return (aes_get_wcs(&entry->ae_gname));
}

const char *
archive_entry_hardlink(struct archive_entry *entry)
{
	if (entry->ae_set & AE_SET_HARDLINK)
		return (aes_get_mbs(&entry->ae_hardlink));
	return (NULL);
}

const wchar_t *
archive_entry_hardlink_w(struct archive_entry *entry)
{
	if (entry->ae_set & AE_SET_HARDLINK)
		return (aes_get_wcs(&entry->ae_hardlink));
	return (NULL);
}

ino_t
archive_entry_ino(struct archive_entry *entry)
{
	return (entry->ae_stat.aest_ino);
}

mode_t
archive_entry_mode(struct archive_entry *entry)
{
	return (entry->ae_stat.aest_mode);
}

time_t
archive_entry_mtime(struct archive_entry *entry)
{
	return (entry->ae_stat.aest_mtime);
}

long
archive_entry_mtime_nsec(struct archive_entry *entry)
{
	return (entry->ae_stat.aest_mtime_nsec);
}

int
archive_entry_mtime_is_set(struct archive_entry *entry)
{
	return (entry->ae_set & AE_SET_MTIME);
}

unsigned int
archive_entry_nlink(struct archive_entry *entry)
{
	return (entry->ae_stat.aest_nlink);
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
	if (entry->ae_stat.aest_rdev_is_broken_down)
		return ae_makedev(entry->ae_stat.aest_rdevmajor,
		    entry->ae_stat.aest_rdevminor);
	else
		return (entry->ae_stat.aest_rdev);
}

dev_t
archive_entry_rdevmajor(struct archive_entry *entry)
{
	if (entry->ae_stat.aest_rdev_is_broken_down)
		return (entry->ae_stat.aest_rdevmajor);
	else
		return major(entry->ae_stat.aest_rdev);
}

dev_t
archive_entry_rdevminor(struct archive_entry *entry)
{
	if (entry->ae_stat.aest_rdev_is_broken_down)
		return (entry->ae_stat.aest_rdevminor);
	else
		return minor(entry->ae_stat.aest_rdev);
}

int64_t
archive_entry_size(struct archive_entry *entry)
{
	return (entry->ae_stat.aest_size);
}

int
archive_entry_size_is_set(struct archive_entry *entry)
{
	return (entry->ae_set & AE_SET_SIZE);
}

const char *
archive_entry_sourcepath(struct archive_entry *entry)
{
	return (aes_get_mbs(&entry->ae_sourcepath));
}

const char *
archive_entry_symlink(struct archive_entry *entry)
{
	if (entry->ae_set & AE_SET_SYMLINK)
		return (aes_get_mbs(&entry->ae_symlink));
	return (NULL);
}

const wchar_t *
archive_entry_symlink_w(struct archive_entry *entry)
{
	if (entry->ae_set & AE_SET_SYMLINK)
		return (aes_get_wcs(&entry->ae_symlink));
	return (NULL);
}

uid_t
archive_entry_uid(struct archive_entry *entry)
{
	return (entry->ae_stat.aest_uid);
}

const char *
archive_entry_uname(struct archive_entry *entry)
{
	return (aes_get_mbs(&entry->ae_uname));
}

const wchar_t *
archive_entry_uname_w(struct archive_entry *entry)
{
	return (aes_get_wcs(&entry->ae_uname));
}

/*
 * Functions to set archive_entry properties.
 */

void
archive_entry_set_filetype(struct archive_entry *entry, unsigned int type)
{
	entry->stat_valid = 0;
	entry->ae_stat.aest_mode &= ~AE_IFMT;
	entry->ae_stat.aest_mode |= AE_IFMT & type;
}

void
archive_entry_set_fflags(struct archive_entry *entry,
    unsigned long set, unsigned long clear)
{
	aes_clean(&entry->ae_fflags_text);
	entry->ae_fflags_set = set;
	entry->ae_fflags_clear = clear;
}

const char *
archive_entry_copy_fflags_text(struct archive_entry *entry,
    const char *flags)
{
	aes_copy_mbs(&entry->ae_fflags_text, flags);
	return (ae_strtofflags(flags,
		    &entry->ae_fflags_set, &entry->ae_fflags_clear));
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
	entry->stat_valid = 0;
	entry->ae_stat.aest_gid = g;
}

void
archive_entry_set_gname(struct archive_entry *entry, const char *name)
{
	aes_set_mbs(&entry->ae_gname, name);
}

void
archive_entry_copy_gname(struct archive_entry *entry, const char *name)
{
	aes_copy_mbs(&entry->ae_gname, name);
}

void
archive_entry_copy_gname_w(struct archive_entry *entry, const wchar_t *name)
{
	aes_copy_wcs(&entry->ae_gname, name);
}

int
archive_entry_update_gname_utf8(struct archive_entry *entry, const char *name)
{
	return (aes_update_utf8(&entry->ae_gname, name));
}

void
archive_entry_set_ino(struct archive_entry *entry, unsigned long ino)
{
	entry->stat_valid = 0;
	entry->ae_stat.aest_ino = ino;
}

void
archive_entry_set_hardlink(struct archive_entry *entry, const char *target)
{
	aes_set_mbs(&entry->ae_hardlink, target);
	if (target != NULL)
		entry->ae_set |= AE_SET_HARDLINK;
	else
		entry->ae_set &= ~AE_SET_HARDLINK;
}

void
archive_entry_copy_hardlink(struct archive_entry *entry, const char *target)
{
	aes_copy_mbs(&entry->ae_hardlink, target);
	if (target != NULL)
		entry->ae_set |= AE_SET_HARDLINK;
	else
		entry->ae_set &= ~AE_SET_HARDLINK;
}

void
archive_entry_copy_hardlink_w(struct archive_entry *entry, const wchar_t *target)
{
	aes_copy_wcs(&entry->ae_hardlink, target);
	if (target != NULL)
		entry->ae_set |= AE_SET_HARDLINK;
	else
		entry->ae_set &= ~AE_SET_HARDLINK;
}

void
archive_entry_set_atime(struct archive_entry *entry, time_t t, long ns)
{
	entry->stat_valid = 0;
	entry->ae_set |= AE_SET_ATIME;
	entry->ae_stat.aest_atime = t;
	entry->ae_stat.aest_atime_nsec = ns;
}

void
archive_entry_unset_atime(struct archive_entry *entry)
{
	archive_entry_set_atime(entry, 0, 0);
	entry->ae_set &= ~AE_SET_ATIME;
}

void
archive_entry_set_ctime(struct archive_entry *entry, time_t t, long ns)
{
	entry->stat_valid = 0;
	entry->ae_set |= AE_SET_CTIME;
	entry->ae_stat.aest_ctime = t;
	entry->ae_stat.aest_ctime_nsec = ns;
}

void
archive_entry_unset_ctime(struct archive_entry *entry)
{
	archive_entry_set_ctime(entry, 0, 0);
	entry->ae_set &= ~AE_SET_CTIME;
}

void
archive_entry_set_dev(struct archive_entry *entry, dev_t d)
{
	entry->stat_valid = 0;
	entry->ae_stat.aest_dev_is_broken_down = 0;
	entry->ae_stat.aest_dev = d;
}

void
archive_entry_set_devmajor(struct archive_entry *entry, dev_t m)
{
	entry->stat_valid = 0;
	entry->ae_stat.aest_dev_is_broken_down = 1;
	entry->ae_stat.aest_devmajor = m;
}

void
archive_entry_set_devminor(struct archive_entry *entry, dev_t m)
{
	entry->stat_valid = 0;
	entry->ae_stat.aest_dev_is_broken_down = 1;
	entry->ae_stat.aest_devminor = m;
}

/* Set symlink if symlink is already set, else set hardlink. */
void
archive_entry_set_link(struct archive_entry *entry, const char *target)
{
	if (entry->ae_set & AE_SET_SYMLINK)
		aes_set_mbs(&entry->ae_symlink, target);
	else
		aes_set_mbs(&entry->ae_hardlink, target);
}

/* Set symlink if symlink is already set, else set hardlink. */
void
archive_entry_copy_link(struct archive_entry *entry, const char *target)
{
	if (entry->ae_set & AE_SET_SYMLINK)
		aes_copy_mbs(&entry->ae_symlink, target);
	else
		aes_copy_mbs(&entry->ae_hardlink, target);
}

/* Set symlink if symlink is already set, else set hardlink. */
void
archive_entry_copy_link_w(struct archive_entry *entry, const wchar_t *target)
{
	if (entry->ae_set & AE_SET_SYMLINK)
		aes_copy_wcs(&entry->ae_symlink, target);
	else
		aes_copy_wcs(&entry->ae_hardlink, target);
}

int
archive_entry_update_link_utf8(struct archive_entry *entry, const char *target)
{
	if (entry->ae_set & AE_SET_SYMLINK)
		return (aes_update_utf8(&entry->ae_symlink, target));
	else
		return (aes_update_utf8(&entry->ae_hardlink, target));
}

void
archive_entry_set_mode(struct archive_entry *entry, mode_t m)
{
	entry->stat_valid = 0;
	entry->ae_stat.aest_mode = m;
}

void
archive_entry_set_mtime(struct archive_entry *entry, time_t m, long ns)
{
	entry->stat_valid = 0;
	entry->ae_set |= AE_SET_MTIME;
	entry->ae_stat.aest_mtime = m;
	entry->ae_stat.aest_mtime_nsec = ns;
}

void
archive_entry_unset_mtime(struct archive_entry *entry)
{
	archive_entry_set_mtime(entry, 0, 0);
	entry->ae_set &= ~AE_SET_MTIME;
}

void
archive_entry_set_nlink(struct archive_entry *entry, unsigned int nlink)
{
	entry->stat_valid = 0;
	entry->ae_stat.aest_nlink = nlink;
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

int
archive_entry_update_pathname_utf8(struct archive_entry *entry, const char *name)
{
	return (aes_update_utf8(&entry->ae_pathname, name));
}

void
archive_entry_set_perm(struct archive_entry *entry, mode_t p)
{
	entry->stat_valid = 0;
	entry->ae_stat.aest_mode &= AE_IFMT;
	entry->ae_stat.aest_mode |= ~AE_IFMT & p;
}

void
archive_entry_set_rdev(struct archive_entry *entry, dev_t m)
{
	entry->stat_valid = 0;
	entry->ae_stat.aest_rdev = m;
	entry->ae_stat.aest_rdev_is_broken_down = 0;
}

void
archive_entry_set_rdevmajor(struct archive_entry *entry, dev_t m)
{
	entry->stat_valid = 0;
	entry->ae_stat.aest_rdev_is_broken_down = 1;
	entry->ae_stat.aest_rdevmajor = m;
}

void
archive_entry_set_rdevminor(struct archive_entry *entry, dev_t m)
{
	entry->stat_valid = 0;
	entry->ae_stat.aest_rdev_is_broken_down = 1;
	entry->ae_stat.aest_rdevminor = m;
}

void
archive_entry_set_size(struct archive_entry *entry, int64_t s)
{
	entry->stat_valid = 0;
	entry->ae_stat.aest_size = s;
	entry->ae_set |= AE_SET_SIZE;
}

void
archive_entry_unset_size(struct archive_entry *entry)
{
	archive_entry_set_size(entry, 0);
	entry->ae_set &= ~AE_SET_SIZE;
}

void
archive_entry_copy_sourcepath(struct archive_entry *entry, const char *path)
{
	aes_set_mbs(&entry->ae_sourcepath, path);
}

void
archive_entry_set_symlink(struct archive_entry *entry, const char *linkname)
{
	aes_set_mbs(&entry->ae_symlink, linkname);
	if (linkname != NULL)
		entry->ae_set |= AE_SET_SYMLINK;
	else
		entry->ae_set &= ~AE_SET_SYMLINK;
}

void
archive_entry_copy_symlink(struct archive_entry *entry, const char *linkname)
{
	aes_copy_mbs(&entry->ae_symlink, linkname);
	if (linkname != NULL)
		entry->ae_set |= AE_SET_SYMLINK;
	else
		entry->ae_set &= ~AE_SET_SYMLINK;
}

void
archive_entry_copy_symlink_w(struct archive_entry *entry, const wchar_t *linkname)
{
	aes_copy_wcs(&entry->ae_symlink, linkname);
	if (linkname != NULL)
		entry->ae_set |= AE_SET_SYMLINK;
	else
		entry->ae_set &= ~AE_SET_SYMLINK;
}

void
archive_entry_set_uid(struct archive_entry *entry, uid_t u)
{
	entry->stat_valid = 0;
	entry->ae_stat.aest_uid = u;
}

void
archive_entry_set_uname(struct archive_entry *entry, const char *name)
{
	aes_set_mbs(&entry->ae_uname, name);
}

void
archive_entry_copy_uname(struct archive_entry *entry, const char *name)
{
	aes_copy_mbs(&entry->ae_uname, name);
}

void
archive_entry_copy_uname_w(struct archive_entry *entry, const wchar_t *name)
{
	aes_copy_wcs(&entry->ae_uname, name);
}

int
archive_entry_update_uname_utf8(struct archive_entry *entry, const char *name)
{
	return (aes_update_utf8(&entry->ae_uname, name));
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
	archive_entry_acl_add_entry_w_len(entry, type, permset, tag, id, name, wcslen(name));
}

void
archive_entry_acl_add_entry_w_len(struct archive_entry *entry,
    int type, int permset, int tag, int id, const wchar_t *name, size_t len)
{
	struct ae_acl *ap;

	if (acl_special(entry, type, permset, tag) == 0)
		return;
	ap = acl_new_entry(entry, type, permset, tag, id);
	if (ap == NULL) {
		/* XXX Error XXX */
		return;
	}
	if (name != NULL  &&  *name != L'\0' && len > 0)
		aes_copy_wcs_len(&ap->name, name, len);
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
			entry->ae_stat.aest_mode &= ~0700;
			entry->ae_stat.aest_mode |= (permset & 7) << 6;
			return (0);
		case ARCHIVE_ENTRY_ACL_GROUP_OBJ:
			entry->ae_stat.aest_mode &= ~0070;
			entry->ae_stat.aest_mode |= (permset & 7) << 3;
			return (0);
		case ARCHIVE_ENTRY_ACL_OTHER:
			entry->ae_stat.aest_mode &= ~0007;
			entry->ae_stat.aest_mode |= permset & 7;
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
	ap = (struct ae_acl *)malloc(sizeof(*ap));
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
	 * from ae_stat.aest_mode).
	 */
	if (entry->acl_state == 0)
		return (ARCHIVE_WARN);

	/* The first three access entries are special. */
	if ((want_type & ARCHIVE_ENTRY_ACL_TYPE_ACCESS) != 0) {
		switch (entry->acl_state) {
		case ARCHIVE_ENTRY_ACL_USER_OBJ:
			*permset = (entry->ae_stat.aest_mode >> 6) & 7;
			*type = ARCHIVE_ENTRY_ACL_TYPE_ACCESS;
			*tag = ARCHIVE_ENTRY_ACL_USER_OBJ;
			entry->acl_state = ARCHIVE_ENTRY_ACL_GROUP_OBJ;
			return (ARCHIVE_OK);
		case ARCHIVE_ENTRY_ACL_GROUP_OBJ:
			*permset = (entry->ae_stat.aest_mode >> 3) & 7;
			*type = ARCHIVE_ENTRY_ACL_TYPE_ACCESS;
			*tag = ARCHIVE_ENTRY_ACL_GROUP_OBJ;
			entry->acl_state = ARCHIVE_ENTRY_ACL_OTHER;
			return (ARCHIVE_OK);
		case ARCHIVE_ENTRY_ACL_OTHER:
			*permset = entry->ae_stat.aest_mode & 7;
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
		*type = 0;
		*permset = 0;
		*tag = 0;
		*id = -1;
		*name = NULL;
		return (ARCHIVE_EOF); /* End of ACL entries. */
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
	size_t length;
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
			else
				length += sizeof(uid_t) * 3 + 1;
			length ++; /* colon */
			length += 3; /* rwx */
			length += 1; /* colon */
			length += max(sizeof(uid_t), sizeof(gid_t)) * 3 + 1;
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
	wp = entry->acl_text_w = (wchar_t *)malloc(length * sizeof(wchar_t));
	if (wp == NULL)
		__archive_errx(1, "No memory to generate the text version of the ACL");
	count = 0;
	if ((flags & ARCHIVE_ENTRY_ACL_TYPE_ACCESS) != 0) {
		append_entry_w(&wp, NULL, ARCHIVE_ENTRY_ACL_USER_OBJ, NULL,
		    entry->ae_stat.aest_mode & 0700, -1);
		*wp++ = ',';
		append_entry_w(&wp, NULL, ARCHIVE_ENTRY_ACL_GROUP_OBJ, NULL,
		    entry->ae_stat.aest_mode & 0070, -1);
		*wp++ = ',';
		append_entry_w(&wp, NULL, ARCHIVE_ENTRY_ACL_OTHER, NULL,
		    entry->ae_stat.aest_mode & 0007, -1);
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
	if (id < 0)
		id = 0;
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
		/* FALLTHROUGH */
	case ARCHIVE_ENTRY_ACL_USER:
		wcscpy(*wp, L"user");
		break;
	case ARCHIVE_ENTRY_ACL_GROUP_OBJ:
		wname = NULL;
		id = -1;
		/* FALLTHROUGH */
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
	} else if (tag == ARCHIVE_ENTRY_ACL_USER
	    || tag == ARCHIVE_ENTRY_ACL_GROUP) {
		append_id_w(wp, id);
		id = -1;
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
	struct {
		const wchar_t *start;
		const wchar_t *end;
	} field[4];

	int fields;
	int type, tag, permset, id;
	const wchar_t *p;
	wchar_t sep;

	while (text != NULL  &&  *text != L'\0') {
		/*
		 * Parse the fields out of the next entry,
		 * advance 'text' to start of next entry.
		 */
		fields = 0;
		do {
			const wchar_t *start, *end;
			next_field_w(&text, &start, &end, &sep);
			if (fields < 4) {
				field[fields].start = start;
				field[fields].end = end;
			}
			++fields;
		} while (sep == L':');

		if (fields < 3)
			return (ARCHIVE_WARN);

		/* Check for a numeric ID in field 1 or 3. */
		id = -1;
		isint_w(field[1].start, field[1].end, &id);
		/* Field 3 is optional. */
		if (id == -1 && fields > 3)
			isint_w(field[3].start, field[3].end, &id);

		/* Parse the permissions from field 2. */
		permset = 0;
		p = field[2].start;
		while (p < field[2].end) {
			switch (*p++) {
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
				return (ARCHIVE_WARN);
			}
		}

		/*
		 * Solaris extension:  "defaultuser::rwx" is the
		 * default ACL corresponding to "user::rwx", etc.
		 */
		if (field[0].end-field[0].start > 7
		    && wmemcmp(field[0].start, L"default", 7) == 0) {
			type = ARCHIVE_ENTRY_ACL_TYPE_DEFAULT;
			field[0].start += 7;
		} else
			type = default_type;

		if (prefix_w(field[0].start, field[0].end, L"user")) {
			if (id != -1 || field[1].start < field[1].end)
				tag = ARCHIVE_ENTRY_ACL_USER;
			else
				tag = ARCHIVE_ENTRY_ACL_USER_OBJ;
		} else if (prefix_w(field[0].start, field[0].end, L"group")) {
			if (id != -1 || field[1].start < field[1].end)
				tag = ARCHIVE_ENTRY_ACL_GROUP;
			else
				tag = ARCHIVE_ENTRY_ACL_GROUP_OBJ;
		} else if (prefix_w(field[0].start, field[0].end, L"other")) {
			if (id != -1 || field[1].start < field[1].end)
				return (ARCHIVE_WARN);
			tag = ARCHIVE_ENTRY_ACL_OTHER;
		} else if (prefix_w(field[0].start, field[0].end, L"mask")) {
			if (id != -1 || field[1].start < field[1].end)
				return (ARCHIVE_WARN);
			tag = ARCHIVE_ENTRY_ACL_MASK;
		} else
			return (ARCHIVE_WARN);

		/* Add entry to the internal list. */
		archive_entry_acl_add_entry_w_len(entry, type, permset,
		    tag, id, field[1].start, field[1].end - field[1].start);
	}
	return (ARCHIVE_OK);
}

/*
 * extended attribute handling
 */

void
archive_entry_xattr_clear(struct archive_entry *entry)
{
	struct ae_xattr	*xp;

	while (entry->xattr_head != NULL) {
		xp = entry->xattr_head->next;
		free(entry->xattr_head->name);
		free(entry->xattr_head->value);
		free(entry->xattr_head);
		entry->xattr_head = xp;
	}

	entry->xattr_head = NULL;
}

void
archive_entry_xattr_add_entry(struct archive_entry *entry,
	const char *name, const void *value, size_t size)
{
	struct ae_xattr	*xp;

	for (xp = entry->xattr_head; xp != NULL; xp = xp->next)
		;

	if ((xp = (struct ae_xattr *)malloc(sizeof(struct ae_xattr))) == NULL)
		/* XXX Error XXX */
		return;

	xp->name = strdup(name);
	if ((xp->value = malloc(size)) != NULL) {
		memcpy(xp->value, value, size);
		xp->size = size;
	} else
		xp->size = 0;

	xp->next = entry->xattr_head;
	entry->xattr_head = xp;
}


/*
 * returns number of the extended attribute entries
 */
int
archive_entry_xattr_count(struct archive_entry *entry)
{
	struct ae_xattr *xp;
	int count = 0;

	for (xp = entry->xattr_head; xp != NULL; xp = xp->next)
		count++;

	return count;
}

int
archive_entry_xattr_reset(struct archive_entry * entry)
{
	entry->xattr_p = entry->xattr_head;

	return archive_entry_xattr_count(entry);
}

int
archive_entry_xattr_next(struct archive_entry * entry,
	const char **name, const void **value, size_t *size)
{
	if (entry->xattr_p) {
		*name = entry->xattr_p->name;
		*value = entry->xattr_p->value;
		*size = entry->xattr_p->size;

		entry->xattr_p = entry->xattr_p->next;

		return (ARCHIVE_OK);
	} else {
		*name = NULL;
		*value = NULL;
		*size = (size_t)0;
		return (ARCHIVE_WARN);
	}
}

/*
 * end of xattr handling
 */

/*
 * Parse a string to a positive decimal integer.  Returns true if
 * the string is non-empty and consists only of decimal digits,
 * false otherwise.
 */
static int
isint_w(const wchar_t *start, const wchar_t *end, int *result)
{
	int n = 0;
	if (start >= end)
		return (0);
	while (start < end) {
		if (*start < '0' || *start > '9')
			return (0);
		if (n > (INT_MAX / 10))
			n = INT_MAX;
		else {
			n *= 10;
			n += *start - '0';
		}
		start++;
	}
	*result = n;
	return (1);
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

/*
 * Return true if the characters [start...end) are a prefix of 'test'.
 * This makes it easy to handle the obvious abbreviations: 'u' for 'user', etc.
 */
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
static char *
ae_fflagstostr(unsigned long bitset, unsigned long bitclear)
{
	char *string, *dp;
	const char *sp;
	unsigned long bits;
	struct flag *flag;
	size_t	length;

	bits = bitset | bitclear;
	length = 0;
	for (flag = flags; flag->name != NULL; flag++)
		if (bits & (flag->set | flag->clear)) {
			length += strlen(flag->name) + 1;
			bits &= ~(flag->set | flag->clear);
		}

	if (length == 0)
		return (NULL);
	string = (char *)malloc(length);
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
 * strtofflags --
 *	Take string of arguments and return file flags.  This
 *	version works a little differently than strtofflags(3).
 *	In particular, it always tests every token, skipping any
 *	unrecognized tokens.  It returns a pointer to the first
 *	unrecognized token, or NULL if every token was recognized.
 *	This version is also const-correct and does not modify the
 *	provided string.
 */
static const char *
ae_strtofflags(const char *s, unsigned long *setp, unsigned long *clrp)
{
	const char *start, *end;
	struct flag *flag;
	unsigned long set, clear;
	const char *failed;

	set = clear = 0;
	start = s;
	failed = NULL;
	/* Find start of first token. */
	while (*start == '\t'  ||  *start == ' '  ||  *start == ',')
		start++;
	while (*start != '\0') {
		/* Locate end of token. */
		end = start;
		while (*end != '\0'  &&  *end != '\t'  &&
		    *end != ' '  &&  *end != ',')
			end++;
		for (flag = flags; flag->name != NULL; flag++) {
			if (memcmp(start, flag->name, end - start) == 0) {
				/* Matched "noXXXX", so reverse the sense. */
				clear |= flag->set;
				set |= flag->clear;
				break;
			} else if (memcmp(start, flag->name + 2, end - start)
			    == 0) {
				/* Matched "XXXX", so don't reverse. */
				set |= flag->set;
				clear |= flag->clear;
				break;
			}
		}
		/* Ignore unknown flag names. */
		if (flag->name == NULL  &&  failed == NULL)
			failed = start;

		/* Find start of next token. */
		start = end;
		while (*start == '\t'  ||  *start == ' '  ||  *start == ',')
			start++;

	}

	if (setp)
		*setp = set;
	if (clrp)
		*clrp = clear;

	/* Return location of first failure. */
	return (failed);
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
static const wchar_t *
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
