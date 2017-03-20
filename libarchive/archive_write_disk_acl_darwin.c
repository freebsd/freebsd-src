/*-
 * Copyright (c) 2017 Martin Matuska
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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_MEMBERSHIP_H
#include <membership.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SYS_ACL_H
#define _ACL_PRIVATE /* For debugging */
#include <sys/acl.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_write_disk_private.h"
#include "archive_acl_maps.h"

static int
set_acl(struct archive *a, int fd, const char *name,
    struct archive_acl *abstract_acl,
    int ae_requested_type, const char *tname)
{
	acl_t		 acl;
	acl_entry_t	 acl_entry;
	acl_permset_t	 acl_permset;
	acl_flagset_t	 acl_flagset;
	int		 ret;
	int		 ae_type, ae_permset, ae_tag, ae_id;
	uuid_t		 ae_uuid;
	uid_t		 ae_uid;
	gid_t		 ae_gid;
	const char	*ae_name;
	int		 entries;
	int		 i;

	ret = ARCHIVE_OK;
	entries = archive_acl_reset(abstract_acl, ae_requested_type);
	if (entries == 0)
		return (ARCHIVE_OK);

	if (ae_requested_type != ARCHIVE_ENTRY_ACL_TYPE_NFS4) {
		errno = ENOENT;
		archive_set_error(a, errno, "Unsupported ACL type");
		return (ARCHIVE_FAILED);
	}

	acl = acl_init(entries);
	if (acl == (acl_t)NULL) {
		archive_set_error(a, errno,
		    "Failed to initialize ACL working storage");
		return (ARCHIVE_FAILED);
	}

	while (archive_acl_next(a, abstract_acl, ae_requested_type, &ae_type,
		   &ae_permset, &ae_tag, &ae_id, &ae_name) == ARCHIVE_OK) {
		/*
		 * Mac OS doesn't support NFSv4 ACLs for
		 * owner@, group@ and everyone@.
		 * We skip any of these ACLs found.
		 */
		if (ae_tag == ARCHIVE_ENTRY_ACL_USER_OBJ ||
		    ae_tag == ARCHIVE_ENTRY_ACL_GROUP_OBJ ||
		    ae_tag == ARCHIVE_ENTRY_ACL_EVERYONE)
			continue;

		if (acl_create_entry(&acl, &acl_entry) != 0) {
			archive_set_error(a, errno,
			    "Failed to create a new ACL entry");
			ret = ARCHIVE_FAILED;
			goto exit_free;
		}

		switch (ae_type) {
		case ARCHIVE_ENTRY_ACL_TYPE_ALLOW:
			acl_set_tag_type(acl_entry, ACL_EXTENDED_ALLOW);
			break;
		case ARCHIVE_ENTRY_ACL_TYPE_DENY:
			acl_set_tag_type(acl_entry, ACL_EXTENDED_DENY);
			break;
		default:
			/* We don't support any other types on MacOS */
			continue;
		}

		switch (ae_tag) {
		case ARCHIVE_ENTRY_ACL_USER:
			ae_uid = archive_write_disk_uid(a, ae_name, ae_id);
			if (mbr_uid_to_uuid(ae_uid, ae_uuid) != 0)
				continue;
			if (acl_set_qualifier(acl_entry, &ae_uuid) != 0)
				continue;
			break;
		case ARCHIVE_ENTRY_ACL_GROUP:
			ae_gid = archive_write_disk_gid(a, ae_name, ae_id);
			if (mbr_gid_to_uuid(ae_gid, ae_uuid) != 0)
				continue;
			if (acl_set_qualifier(acl_entry, &ae_uuid) != 0)
				continue;
			break;
		default:
			archive_set_error(a, ARCHIVE_ERRNO_MISC,
			    "Unsupported ACL tag");
			ret = ARCHIVE_FAILED;
			goto exit_free;
		}

		if (acl_get_permset(acl_entry, &acl_permset) != 0) {
			archive_set_error(a, errno,
			    "Failed to get ACL permission set");
			ret = ARCHIVE_FAILED;
			goto exit_free;
		}
		if (acl_clear_perms(acl_permset) != 0) {
			archive_set_error(a, errno,
			    "Failed to clear ACL permissions");
			ret = ARCHIVE_FAILED;
			goto exit_free;
		}

		for (i = 0; i < acl_nfs4_perm_map_size; ++i) {
			if (ae_permset & acl_nfs4_perm_map[i].a_perm) {
				if (acl_add_perm(acl_permset,
				    acl_nfs4_perm_map[i].p_perm) != 0) {
					archive_set_error(a, errno,
					    "Failed to add ACL permission");
					ret = ARCHIVE_FAILED;
					goto exit_free;
				}
			}
		}

		/*
		 * acl_get_flagset_np() fails with non-NFSv4 ACLs
		 */
		if (acl_get_flagset_np(acl_entry, &acl_flagset) != 0) {
			archive_set_error(a, errno,
			    "Failed to get flagset from an NFSv4 ACL entry");
			ret = ARCHIVE_FAILED;
			goto exit_free;
		}
		if (acl_clear_flags_np(acl_flagset) != 0) {
			archive_set_error(a, errno,
			    "Failed to clear flags from an NFSv4 ACL flagset");
			ret = ARCHIVE_FAILED;
			goto exit_free;
		}

		for (i = 0; i < acl_nfs4_flag_map_size; ++i) {
			if (ae_permset & acl_nfs4_flag_map[i].a_perm) {
				if (acl_add_flag_np(acl_flagset,
				    acl_nfs4_flag_map[i].p_perm) != 0) {
					archive_set_error(a, errno,
					    "Failed to add flag to "
					    "NFSv4 ACL flagset");
					ret = ARCHIVE_FAILED;
					goto exit_free;
				}
			}
		}
	}

	if (fd >= 0) {
		if (acl_set_fd_np(fd, acl, ACL_TYPE_EXTENDED) == 0)
			ret = ARCHIVE_OK;
		else {
			if (errno == EOPNOTSUPP) {
				/* Filesystem doesn't support ACLs */
				ret = ARCHIVE_OK;
			} else {
				archive_set_error(a, errno,
				    "Failed to set acl on fd: %s", tname);
				ret = ARCHIVE_WARN;
			}
		}
	} else if (acl_set_link_np(name, ACL_TYPE_EXTENDED, acl) != 0) {
		if (errno == EOPNOTSUPP) {
			/* Filesystem doesn't support ACLs */
			ret = ARCHIVE_OK;
		} else {
			archive_set_error(a, errno, "Failed to set acl: %s",
			    tname);
			ret = ARCHIVE_WARN;
		}
	}
exit_free:
	acl_free(acl);
	return (ret);
}

int
archive_write_disk_set_acls(struct archive *a, int fd, const char *name,
    struct archive_acl *abstract_acl, __LA_MODE_T mode)
{
	int		ret = ARCHIVE_OK;

	(void)mode;	/* UNUSED */

	if ((archive_acl_types(abstract_acl) &
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4) != 0) {
		ret = set_acl(a, fd, name, abstract_acl,
		    ARCHIVE_ENTRY_ACL_TYPE_NFS4, "nfs4");
	}
	return (ret);
}
