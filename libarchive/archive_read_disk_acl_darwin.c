/*-
 * Copyright (c) 2017 Martin Matuska
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

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#if HAVE_MEMBERSHIP_H
#include <membership.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_ACL_H
#define _ACL_PRIVATE /* For debugging */
#include <sys/acl.h>
#endif

#include "archive_entry.h"
#include "archive_private.h"
#include "archive_read_disk_private.h"
#include "archive_acl_maps.h"


/*
 * Darwin-specific ACL functions and helper functions
 *
 * Exported functions:
 * none
 */
static int translate_guid(struct archive *a, acl_entry_t acl_entry,
    int *ae_id, int *ae_tag, const char **ae_name)
{
	void *q;
	uid_t ugid;
	int r, idtype;

	q = acl_get_qualifier(acl_entry);
	if (q == NULL)
		return (1);
	r = mbr_uuid_to_id((const unsigned char *)q, &ugid, &idtype);
	if (r != 0) {
		acl_free(q);
		return (1);
	}
	if (idtype == ID_TYPE_UID) {
		*ae_tag = ARCHIVE_ENTRY_ACL_USER;
		*ae_id = ugid;
		*ae_name = archive_read_disk_uname(a, *ae_id);
	} else if (idtype == ID_TYPE_GID) {
		*ae_tag = ARCHIVE_ENTRY_ACL_GROUP;
		*ae_id = ugid;
		*ae_name = archive_read_disk_gname(a, *ae_id);
	} else
		r = 1;

	acl_free(q);
	return (r);
}

/*
 * Add trivial NFSv4 ACL entries from mode
 */
static void
add_trivial_nfs4_acl(struct archive_entry *entry)
{
	mode_t mode;
	int i;
	const int rperm = ARCHIVE_ENTRY_ACL_READ_DATA;
	const int wperm = ARCHIVE_ENTRY_ACL_WRITE_DATA |
	    ARCHIVE_ENTRY_ACL_APPEND_DATA;
	const int eperm = ARCHIVE_ENTRY_ACL_EXECUTE;
	const int pubset = ARCHIVE_ENTRY_ACL_READ_ATTRIBUTES |
	    ARCHIVE_ENTRY_ACL_READ_NAMED_ATTRS |
	    ARCHIVE_ENTRY_ACL_READ_ACL |
	    ARCHIVE_ENTRY_ACL_SYNCHRONIZE;
	const int ownset = pubset | ARCHIVE_ENTRY_ACL_WRITE_ATTRIBUTES |
	    ARCHIVE_ENTRY_ACL_WRITE_NAMED_ATTRS |
	    ARCHIVE_ENTRY_ACL_WRITE_ACL |
	    ARCHIVE_ENTRY_ACL_WRITE_OWNER;

	struct {
	    const int type;
	    const int tag;
	    int permset;
	} tacl_entry[] = {
	    {ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_USER_OBJ, 0},
	    {ARCHIVE_ENTRY_ACL_TYPE_DENY, ARCHIVE_ENTRY_ACL_USER_OBJ, 0},
	    {ARCHIVE_ENTRY_ACL_TYPE_DENY, ARCHIVE_ENTRY_ACL_GROUP_OBJ, 0},
	    {ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_USER_OBJ, ownset},
	    {ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_GROUP_OBJ, pubset},
	    {ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EVERYONE, pubset}
	};

	mode = archive_entry_mode(entry);

	/* Permissions for everyone@ */
	if (mode & 0004)
		tacl_entry[5].permset |= rperm;
	if (mode & 0002)
		tacl_entry[5].permset |= wperm;
	if (mode & 0001)
		tacl_entry[5].permset |= eperm;

	/* Permissions for group@ */
	if (mode & 0040)
		tacl_entry[4].permset |= rperm;
	else if (mode & 0004)
		tacl_entry[2].permset |= rperm;
	if (mode & 0020)
		tacl_entry[4].permset |= wperm;
	else if (mode & 0002)
		tacl_entry[2].permset |= wperm;
	if (mode & 0010)
		tacl_entry[4].permset |= eperm;
	else if (mode & 0001)
		tacl_entry[2].permset |= eperm;

	/* Permissions for owner@ */
	if (mode & 0400) {
		tacl_entry[3].permset |= rperm;
		if (!(mode & 0040) && (mode & 0004))
			tacl_entry[0].permset |= rperm;
	} else if ((mode & 0040) || (mode & 0004))
		tacl_entry[1].permset |= rperm;
	if (mode & 0200) {
		tacl_entry[3].permset |= wperm;
		if (!(mode & 0020) && (mode & 0002))
			tacl_entry[0].permset |= wperm;
	} else if ((mode & 0020) || (mode & 0002))
		tacl_entry[1].permset |= wperm;
	if (mode & 0100) {
		tacl_entry[3].permset |= eperm;
		if (!(mode & 0010) && (mode & 0001))
			tacl_entry[0].permset |= eperm;
	} else if ((mode & 0010) || (mode & 0001))
		tacl_entry[1].permset |= eperm;

	for (i = 0; i < 6; i++) {
		if (tacl_entry[i].permset != 0) {
			archive_entry_acl_add_entry(entry,
			    tacl_entry[i].type, tacl_entry[i].permset,
			    tacl_entry[i].tag, -1, NULL);
		}
	}

	return;
}

static int
translate_acl(struct archive_read_disk *a,
    struct archive_entry *entry, acl_t acl)
{
	acl_tag_t	 acl_tag;
	acl_flagset_t	 acl_flagset;
	acl_entry_t	 acl_entry;
	acl_permset_t	 acl_permset;
	int		 i, entry_acl_type;
	int		 r, s, ae_id, ae_tag, ae_perm;
	const char	*ae_name;

	s = acl_get_entry(acl, ACL_FIRST_ENTRY, &acl_entry);
	if (s == -1) {
		archive_set_error(&a->archive, errno,
		    "Failed to get first ACL entry");
		return (ARCHIVE_WARN);
	}

	while (s == 0) {
		ae_id = -1;
		ae_name = NULL;
		ae_perm = 0;

		if (acl_get_tag_type(acl_entry, &acl_tag) != 0) {
			archive_set_error(&a->archive, errno,
			    "Failed to get ACL tag type");
			return (ARCHIVE_WARN);
		}
		switch (acl_tag) {
		case ACL_EXTENDED_ALLOW:
			entry_acl_type = ARCHIVE_ENTRY_ACL_TYPE_ALLOW;
			r = translate_guid(&a->archive, acl_entry,
			    &ae_id, &ae_tag, &ae_name);
			break;
		case ACL_EXTENDED_DENY:
			entry_acl_type = ARCHIVE_ENTRY_ACL_TYPE_DENY;
			r = translate_guid(&a->archive, acl_entry,
			    &ae_id, &ae_tag, &ae_name);
			break;
		default:
			/* Skip types that libarchive can't support. */
			s = acl_get_entry(acl, ACL_NEXT_ENTRY, &acl_entry);
			continue;
		}

		/* Skip if translate_guid() above failed */
		if (r != 0) {
			s = acl_get_entry(acl, ACL_NEXT_ENTRY, &acl_entry);
			continue;
		}

		/*
		 * Libarchive stores "flag" (NFSv4 inheritance bits)
		 * in the ae_perm bitmap.
		 *
		 * acl_get_flagset_np() fails with non-NFSv4 ACLs
		 */
		if (acl_get_flagset_np(acl_entry, &acl_flagset) != 0) {
			archive_set_error(&a->archive, errno,
			    "Failed to get flagset from a NFSv4 ACL entry");
			return (ARCHIVE_WARN);
		}
		for (i = 0; i < acl_nfs4_flag_map_size; ++i) {
			r = acl_get_flag_np(acl_flagset,
			    acl_nfs4_flag_map[i].p_perm);
			if (r == -1) {
				archive_set_error(&a->archive, errno,
				    "Failed to check flag in a NFSv4 "
				    "ACL flagset");
				return (ARCHIVE_WARN);
			} else if (r)
				ae_perm |= acl_nfs4_flag_map[i].a_perm;
		}

		if (acl_get_permset(acl_entry, &acl_permset) != 0) {
			archive_set_error(&a->archive, errno,
			    "Failed to get ACL permission set");
			return (ARCHIVE_WARN);
		}

		for (i = 0; i < acl_nfs4_perm_map_size; ++i) {
			/*
			 * acl_get_perm() is spelled differently on different
			 * platforms; see above.
			 */
			r = acl_get_perm_np(acl_permset,
			    acl_nfs4_perm_map[i].p_perm);
			if (r == -1) {
				archive_set_error(&a->archive, errno,
				    "Failed to check permission in an ACL "
				    "permission set");
				return (ARCHIVE_WARN);
			} else if (r)
				ae_perm |= acl_nfs4_perm_map[i].a_perm;
		}

#if !HAVE_DECL_ACL_SYNCHRONIZE
		/* On Mac OS X without ACL_SYNCHRONIZE assume it is set */
		ae_perm |= ARCHIVE_ENTRY_ACL_SYNCHRONIZE;
#endif

		archive_entry_acl_add_entry(entry, entry_acl_type,
					    ae_perm, ae_tag,
					    ae_id, ae_name);

		s = acl_get_entry(acl, ACL_NEXT_ENTRY, &acl_entry);
	}
	return (ARCHIVE_OK);
}

int
archive_read_disk_entry_setup_acls(struct archive_read_disk *a,
    struct archive_entry *entry, int *fd)
{
	const char	*accpath;
	acl_t		acl;
	int		r;

	accpath = NULL;

	if (*fd < 0) {
		accpath = archive_read_disk_entry_setup_path(a, entry, fd);
		if (accpath == NULL)
			return (ARCHIVE_WARN);
	}

	archive_entry_acl_clear(entry);

	acl = NULL;

	if (*fd >= 0)
		acl = acl_get_fd_np(*fd, ACL_TYPE_EXTENDED);
	else if (!a->follow_symlinks)
		acl = acl_get_link_np(accpath, ACL_TYPE_EXTENDED);
	else
		acl = acl_get_file(accpath, ACL_TYPE_EXTENDED);

	if (acl != NULL) {
		r = translate_acl(a, entry, acl);
		acl_free(acl);
		acl = NULL;

		if (r != ARCHIVE_OK) {
			archive_set_error(&a->archive, errno,
			    "Couldn't translate NFSv4 ACLs");
		}

		/*
		 * Because Mac OS doesn't support owner@, group@ and everyone@
		 * ACLs we need to add NFSv4 ACLs mirroring the file mode to
		 * the archive entry. Otherwise extraction on non-Mac platforms
		 * would lead to an invalid file mode.
		 */
		if ((archive_entry_acl_types(entry) &
		    ARCHIVE_ENTRY_ACL_TYPE_NFS4) != 0)
			add_trivial_nfs4_acl(entry);

		return (r);
	}
	return (ARCHIVE_OK);
}
