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
 *
 * $FreeBSD$
 */

#ifndef ARCHIVE_ENTRY_H_INCLUDED
#define	ARCHIVE_ENTRY_H_INCLUDED

#include <unistd.h>
#include <wchar.h>

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
 *
 * TODO: Design a good API for handling sparse files.
 */
struct archive_entry;

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

const char		*archive_entry_acl(struct archive_entry *);
const char		*archive_entry_acl_default(struct archive_entry *);
dev_t			 archive_entry_devmajor(struct archive_entry *);
dev_t			 archive_entry_devminor(struct archive_entry *);
const char		*archive_entry_fflags(struct archive_entry *);
const char		*archive_entry_gname(struct archive_entry *);
const char		*archive_entry_hardlink(struct archive_entry *);
mode_t			 archive_entry_mode(struct archive_entry *);
const char		*archive_entry_pathname(struct archive_entry *);
const wchar_t		*archive_entry_pathname_w(struct archive_entry *);
int64_t			 archive_entry_size(struct archive_entry *);
const struct stat	*archive_entry_stat(struct archive_entry *);
const char		*archive_entry_symlink(struct archive_entry *);
int			 archive_entry_tartype(struct archive_entry *);
const char		*archive_entry_uname(struct archive_entry *);

/*
 * Set fields in an archive_entry.
 *
 * Note that string 'set' functions do not copy the string, only the pointer.
 * In contrast, 'copy_stat' does copy the full structure.
 */

void	archive_entry_copy_stat(struct archive_entry *, const struct stat *);
void	archive_entry_set_acl(struct archive_entry *, const char *);
void	archive_entry_copy_acl_w(struct archive_entry *, const wchar_t *);
void	archive_entry_set_acl_default(struct archive_entry *, const char *);
void	archive_entry_copy_acl_default_w(struct archive_entry *, const wchar_t *);
void	archive_entry_set_fflags(struct archive_entry *, const char *);
void	archive_entry_copy_fflags_w(struct archive_entry *, const wchar_t *);
void	archive_entry_set_devmajor(struct archive_entry *, dev_t);
void	archive_entry_set_devminor(struct archive_entry *, dev_t);
void	archive_entry_set_gid(struct archive_entry *, gid_t);
void	archive_entry_set_gname(struct archive_entry *, const char *);
void	archive_entry_copy_gname_w(struct archive_entry *, const wchar_t *);
void	archive_entry_set_hardlink(struct archive_entry *, const char *);
void	archive_entry_copy_hardlink_w(struct archive_entry *, const wchar_t *);
void	archive_entry_set_mode(struct archive_entry *, mode_t);
void	archive_entry_set_pathname(struct archive_entry *, const char *);
void	archive_entry_copy_pathname_w(struct archive_entry *, const wchar_t *);
void	archive_entry_set_size(struct archive_entry *, int64_t);
void	archive_entry_set_symlink(struct archive_entry *, const char *);
void	archive_entry_copy_symlink_w(struct archive_entry *, const wchar_t *);
void	archive_entry_set_tartype(struct archive_entry *, char);
void	archive_entry_set_uid(struct archive_entry *, uid_t);
void	archive_entry_set_uname(struct archive_entry *, const char *);
void	archive_entry_copy_uname_w(struct archive_entry *, const wchar_t *);

#endif /* !ARCHIVE_ENTRY_H_INCLUDED */
