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
#include <stdlib.h>
#include <string.h>

#include "archive_entry.h"

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
	 * Note: If you add any more string fields, update
	 * archive_entry_clone accordingly.
	 */
	const char *ae_acl;		/* ACL text */
	const char *ae_acl_default;	/* default ACL */
	const char *ae_fflags;		/* Text fflags per fflagstostr(3) */
	const char *ae_gname;		/* Name of owning group */
	const char *ae_hardlink;	/* Name of target for hardlink */
	const char *ae_pathname;	/* Name of entry */
	const char *ae_symlink;		/* symlink contents */
	const char *ae_uname;		/* Name of owner */

	char buff[1]; /* MUST BE AT END OF STRUCT!!! */
};

struct archive_entry *
archive_entry_clear(struct archive_entry *entry)
{
	memset(entry, 0, sizeof(*entry));
	entry->ae_tartype = -1;
	return entry;
}

struct archive_entry *
archive_entry_clone(struct archive_entry *entry)
{
	int size;
	struct archive_entry *entry2;
	char *p;

	size = sizeof(*entry2);
	if (entry->ae_acl)
		size += strlen(entry->ae_acl) + 1;
	if (entry->ae_acl_default)
		size += strlen(entry->ae_acl_default) + 1;
	if (entry->ae_fflags)
		size += strlen(entry->ae_fflags) + 1;
	if (entry->ae_gname)
		size += strlen(entry->ae_gname) + 1;
	if (entry->ae_hardlink)
		size += strlen(entry->ae_hardlink) + 1;
	if (entry->ae_pathname)
		size += strlen(entry->ae_pathname) + 1;
	if (entry->ae_symlink)
		size += strlen(entry->ae_symlink) + 1;
	if (entry->ae_uname)
		size += strlen(entry->ae_uname) + 1;

	entry2 = malloc(size);
	*entry2 = *entry;

	/* Copy all of the strings from the original. */
	p = entry2->buff;

	if (entry->ae_acl) {
		entry2->ae_acl = p;
		strcpy(p, entry->ae_acl);
		p += strlen(p) + 1;
	}

	if (entry->ae_acl_default) {
		entry2->ae_acl_default = p;
		strcpy(p, entry->ae_acl_default);
		p += strlen(p) + 1;
	}

	if (entry->ae_fflags) {
		entry2->ae_fflags = p;
		strcpy(p, entry->ae_fflags);
		p += strlen(p) + 1;
	}

	if (entry->ae_gname) {
		entry2->ae_gname = p;
		strcpy(p, entry->ae_gname);
		p += strlen(p) + 1;
	}

	if (entry->ae_hardlink) {
		entry2->ae_hardlink = p;
		strcpy(p, entry->ae_hardlink);
		p += strlen(p) + 1;
	}

	if (entry->ae_pathname) {
		entry2->ae_pathname = p;
		strcpy(p, entry->ae_pathname);
		p += strlen(p) + 1;
	}

	if (entry->ae_symlink) {
		entry2->ae_symlink = p;
		strcpy(p, entry->ae_symlink);
		p += strlen(p) + 1;
	}

	if (entry->ae_uname) {
		entry2->ae_uname = p;
		strcpy(p, entry->ae_uname);
		p += strlen(p) + 1;
	}

	return (entry2);
}

struct archive_entry *
archive_entry_dup(struct archive_entry *entry)
{
	struct archive_entry *entry2;

	entry2 = malloc(sizeof(*entry2));
	*entry2 = *entry;
	return (entry2);
}

void
archive_entry_free(struct archive_entry *entry)
{
	free(entry);
}

struct archive_entry *
archive_entry_new(void)
{
	struct archive_entry *entry;

	entry = malloc(sizeof(*entry));
	if(entry == NULL)
		return (NULL);
	archive_entry_clear(entry);
	return (entry);
}


/*
 * Functions for reading fields from an archive_entry.
 */

const char *
archive_entry_acl(struct archive_entry *entry)
{
	return (entry->ae_acl);
}


const char *
archive_entry_acl_default(struct archive_entry *entry)
{
	return (entry->ae_acl_default);
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
	return (entry->ae_fflags);
}

const char *
archive_entry_gname(struct archive_entry *entry)
{
	return (entry->ae_gname);
}

const char *
archive_entry_hardlink(struct archive_entry *entry)
{
	return (entry->ae_hardlink);
}

mode_t
archive_entry_mode(struct archive_entry *entry)
{
	return (entry->ae_stat.st_mode);
}

const char *
archive_entry_pathname(struct archive_entry *entry)
{
	return (entry->ae_pathname);
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
	return (entry->ae_symlink);
}

int
archive_entry_tartype(struct archive_entry *entry)
{
	return (entry->ae_tartype);
}

const char *
archive_entry_uname(struct archive_entry *entry)
{
	return (entry->ae_uname);
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
	entry->ae_acl = acl;
}


void
archive_entry_set_acl_default(struct archive_entry *entry, const char *acl)
{
	entry->ae_acl_default = acl;
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
	entry->ae_fflags = flags;
}

void
archive_entry_set_gid(struct archive_entry *entry, gid_t g)
{
	entry->ae_stat.st_gid = g;
}

void
archive_entry_set_gname(struct archive_entry *entry, const char *name)
{
	entry->ae_gname = name;
}

void
archive_entry_set_hardlink(struct archive_entry *entry, const char *target)
{
	entry->ae_hardlink = target;
}

void
archive_entry_set_mode(struct archive_entry *entry, mode_t m)
{
	entry->ae_stat.st_mode = m;
}

void
archive_entry_set_pathname(struct archive_entry *entry, const char *name)
{
	entry->ae_pathname = name;
}

void
archive_entry_set_size(struct archive_entry *entry, int64_t s)
{
	entry->ae_stat.st_size = s;
}

void
archive_entry_set_symlink(struct archive_entry *entry, const char *link)
{
	entry->ae_symlink = link;
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
	entry->ae_uname = name;
}

