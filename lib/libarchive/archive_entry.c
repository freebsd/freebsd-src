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
#ifdef HAVE_DMALLOC
#include <dmalloc.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "archive_entry.h"

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

void		aes_clean(struct aes *);
void		aes_copy(struct aes *dest, struct aes *src);
const char *	aes_get_mbs(struct aes *);
const wchar_t *	aes_get_wcs(struct aes *);
void		aes_set_mbs(struct aes *, const char *mbs);
void		aes_set_wcs(struct aes *, const wchar_t *wcs);
void		aes_copy_wcs(struct aes *, const wchar_t *wcs);

/*
 * Description of an archive entry.
 *
 * Basically, this is a "struct stat" with a few text fields added in.
 *
 * TODO: Add "comment", "charset", "acl", and possibly other entries
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
	 * This occurs when the actual file type of the underlying object is
	 * not in the archive.  For example, 'tar' archives store hardlinks
	 * without marking the type of the underlying object.
	 */
	struct stat ae_stat;

	/* I'm not happy with having this format-particular data here. */
	int ae_tartype;

	/*
	 * Use aes here so that we get transparent mbs<->wcs conversions.
	 */
	struct aes ae_acl;		/* ACL text */
	struct aes ae_acl_default;	/* default ACL */
	struct aes ae_fflags;		/* Text fflags per fflagstostr(3) */
	struct aes ae_gname;		/* Name of owning group */
	struct aes ae_hardlink;	/* Name of target for hardlink */
	struct aes ae_pathname;	/* Name of entry */
	struct aes ae_symlink;		/* symlink contents */
	struct aes ae_uname;		/* Name of owner */
};

void
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

void
aes_copy(struct aes *dest, struct aes *src)
{
	*dest = *src;
	if (src->aes_mbs_alloc != NULL) {
		dest->aes_mbs_alloc = strdup(src->aes_mbs_alloc);
		dest->aes_mbs = dest->aes_mbs_alloc;
	}

	if (src->aes_wcs_alloc != NULL) {
		dest->aes_wcs_alloc = malloc((wcslen(src->aes_wcs_alloc) + 1)
		    * sizeof(wchar_t));
		dest->aes_wcs = dest->aes_wcs_alloc;
		wcscpy(dest->aes_wcs_alloc, src->aes_wcs);
	}
}

const char *
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
		wcstombs(aes->aes_mbs_alloc, aes->aes_wcs, mbs_length - 1);
		aes->aes_mbs_alloc[mbs_length - 1] = 0;
	}
	return (aes->aes_mbs);
}

const wchar_t *
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
		mbstowcs(aes->aes_wcs_alloc, aes->aes_mbs, wcs_length);
		aes->aes_wcs_alloc[wcs_length] = 0;
	}
	return (aes->aes_wcs);
}

void
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

void
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

void
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
	wcscpy(aes->aes_wcs_alloc, wcs);
	aes->aes_wcs = aes->aes_wcs_alloc;
}

struct archive_entry *
archive_entry_clear(struct archive_entry *entry)
{
	aes_clean(&entry->ae_acl);
	aes_clean(&entry->ae_acl_default);
	aes_clean(&entry->ae_fflags);
	aes_clean(&entry->ae_gname);
	aes_clean(&entry->ae_hardlink);
	aes_clean(&entry->ae_pathname);
	aes_clean(&entry->ae_symlink);
	aes_clean(&entry->ae_uname);
	memset(entry, 0, sizeof(*entry));
	entry->ae_tartype = -1;
	return entry;
}

struct archive_entry *
archive_entry_clone(struct archive_entry *entry)
{
	struct archive_entry *entry2;

	/* Allocate new structure and copy over all of the fields. */
	entry2 = malloc(sizeof(*entry2));
	entry2->ae_stat = entry->ae_stat;
	entry2->ae_tartype = entry->ae_tartype;

	aes_copy(&entry2->ae_acl ,&entry->ae_acl);
	aes_copy(&entry2->ae_acl_default ,&entry->ae_acl_default);
	aes_copy(&entry2->ae_fflags ,&entry->ae_fflags);
	aes_copy(&entry2->ae_gname ,&entry->ae_gname);
	aes_copy(&entry2->ae_hardlink ,&entry->ae_hardlink);
	aes_copy(&entry2->ae_pathname, &entry->ae_pathname);
	aes_copy(&entry2->ae_symlink ,&entry->ae_symlink);
	aes_copy(&entry2->ae_uname ,&entry->ae_uname);

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
	if(entry == NULL)
		return (NULL);
	memset(entry, 0, sizeof(*entry));
	entry->ae_tartype = -1;
	return (entry);
}

/*
 * Functions for reading fields from an archive_entry.
 */

const char *
archive_entry_acl(struct archive_entry *entry)
{
	return (aes_get_mbs(&entry->ae_acl));
}


const char *
archive_entry_acl_default(struct archive_entry *entry)
{
	return (aes_get_mbs(&entry->ae_acl_default));
}

dev_t
archive_entry_devmajor(struct archive_entry *entry)
{
	return (major(entry->ae_stat.st_rdev));
}


dev_t
archive_entry_devminor(struct archive_entry *entry)
{
	return (minor(entry->ae_stat.st_rdev));
}

const char *
archive_entry_fflags(struct archive_entry *entry)
{
	return (aes_get_mbs(&entry->ae_fflags));
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

mode_t
archive_entry_mode(struct archive_entry *entry)
{
	return (entry->ae_stat.st_mode);
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

int
archive_entry_tartype(struct archive_entry *entry)
{
	return (entry->ae_tartype);
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
archive_entry_set_acl(struct archive_entry *entry, const char *acl)
{
	aes_set_mbs(&entry->ae_acl, acl);
}

void
archive_entry_copy_acl_w(struct archive_entry *entry, const wchar_t *acl)
{
	aes_copy_wcs(&entry->ae_acl, acl);
}

void
archive_entry_set_acl_default(struct archive_entry *entry, const char *acl)
{
	aes_set_mbs(&entry->ae_acl_default, acl);
}

void
archive_entry_copy_acl_default_w(struct archive_entry *entry, const wchar_t *acl)
{
	aes_copy_wcs(&entry->ae_acl_default, acl);
}

void
archive_entry_set_devmajor(struct archive_entry *entry, dev_t m)
{
	dev_t d;

	d = entry->ae_stat.st_rdev;
	entry->ae_stat.st_rdev = makedev(m, minor(d));
}

void
archive_entry_set_devminor(struct archive_entry *entry, dev_t m)
{
	dev_t d;

	d = entry->ae_stat.st_rdev;
	entry->ae_stat.st_rdev = makedev( major(d), m);
}

void
archive_entry_set_fflags(struct archive_entry *entry, const char *flags)
{
	aes_set_mbs(&entry->ae_fflags, flags);
}

void
archive_entry_copy_fflags_w(struct archive_entry *entry, const wchar_t *flags)
{
	aes_copy_wcs(&entry->ae_fflags, flags);
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
archive_entry_copy_hardlink_w(struct archive_entry *entry, const wchar_t *target)
{
	aes_copy_wcs(&entry->ae_hardlink, target);
}

void
archive_entry_set_mode(struct archive_entry *entry, mode_t m)
{
	entry->ae_stat.st_mode = m;
}

void
archive_entry_set_pathname(struct archive_entry *entry, const char *name)
{
	aes_set_mbs(&entry->ae_pathname, name);
}

void
archive_entry_copy_pathname_w(struct archive_entry *entry, const wchar_t *name)
{
	aes_copy_wcs(&entry->ae_pathname, name);
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
archive_entry_set_tartype(struct archive_entry *entry, char t)
{
	entry->ae_tartype = t;
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

#if TEST
int
main(int argc, char **argv)
{
	struct aes aes;

	memset(&aes, 0, sizeof(aes));
	aes_clean(&aes);
	aes_set_mbs(&aes, "»»»abc");
	wprintf("%S\n", L"abcdef");
	wprintf("%S\n",aes_get_wcs(&aes));
	return (0);
}
#endif
