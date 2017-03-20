/*-
 * Copyright (c) 2003-2010 Tim Kientzle
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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
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
	int		 acl_type = 0;
	acl_t		 acl;
	acl_entry_t	 acl_entry;
	acl_permset_t	 acl_permset;
#if ARCHIVE_ACL_FREEBSD_NFS4
	acl_flagset_t	 acl_flagset;
	int		 r;
#endif
	int		 ret;
	int		 ae_type, ae_permset, ae_tag, ae_id;
	int		 perm_map_size;
	const acl_perm_map_t	*perm_map;
	uid_t		 ae_uid;
	gid_t		 ae_gid;
	const char	*ae_name;
	int		 entries;
	int		 i;

	ret = ARCHIVE_OK;
	entries = archive_acl_reset(abstract_acl, ae_requested_type);
	if (entries == 0)
		return (ARCHIVE_OK);


	switch (ae_requested_type) {
	case ARCHIVE_ENTRY_ACL_TYPE_ACCESS:
		acl_type = ACL_TYPE_ACCESS;
		break;
	case ARCHIVE_ENTRY_ACL_TYPE_DEFAULT:
		acl_type = ACL_TYPE_DEFAULT;
		break;
#if ARCHIVE_ACL_FREEBSD_NFS4
	case ARCHIVE_ENTRY_ACL_TYPE_NFS4:
		acl_type = ACL_TYPE_NFS4;
		break;
#endif
	default:
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
		if (acl_create_entry(&acl, &acl_entry) != 0) {
			archive_set_error(a, errno,
			    "Failed to create a new ACL entry");
			ret = ARCHIVE_FAILED;
			goto exit_free;
		}
		switch (ae_tag) {
		case ARCHIVE_ENTRY_ACL_USER:
			ae_uid = archive_write_disk_uid(a, ae_name, ae_id);
			acl_set_tag_type(acl_entry, ACL_USER);
			acl_set_qualifier(acl_entry, &ae_uid);
			break;
		case ARCHIVE_ENTRY_ACL_GROUP:
			ae_gid = archive_write_disk_gid(a, ae_name, ae_id);
			acl_set_tag_type(acl_entry, ACL_GROUP);
			acl_set_qualifier(acl_entry, &ae_gid);
			break;
		case ARCHIVE_ENTRY_ACL_USER_OBJ:
			acl_set_tag_type(acl_entry, ACL_USER_OBJ);
			break;
		case ARCHIVE_ENTRY_ACL_GROUP_OBJ:
			acl_set_tag_type(acl_entry, ACL_GROUP_OBJ);
			break;
		case ARCHIVE_ENTRY_ACL_MASK:
			acl_set_tag_type(acl_entry, ACL_MASK);
			break;
		case ARCHIVE_ENTRY_ACL_OTHER:
			acl_set_tag_type(acl_entry, ACL_OTHER);
			break;
#if ARCHIVE_ACL_FREEBSD_NFS4
		case ARCHIVE_ENTRY_ACL_EVERYONE:
			acl_set_tag_type(acl_entry, ACL_EVERYONE);
			break;
#endif
		default:
			archive_set_error(a, ARCHIVE_ERRNO_MISC,
			    "Unsupported ACL tag");
			ret = ARCHIVE_FAILED;
			goto exit_free;
		}

#if ARCHIVE_ACL_FREEBSD_NFS4
		r = 0;
		switch (ae_type) {
		case ARCHIVE_ENTRY_ACL_TYPE_ALLOW:
			r = acl_set_entry_type_np(acl_entry,
			    ACL_ENTRY_TYPE_ALLOW);
			break;
		case ARCHIVE_ENTRY_ACL_TYPE_DENY:
			r = acl_set_entry_type_np(acl_entry,
			    ACL_ENTRY_TYPE_DENY);
			break;
		case ARCHIVE_ENTRY_ACL_TYPE_AUDIT:
			r = acl_set_entry_type_np(acl_entry,
			    ACL_ENTRY_TYPE_AUDIT);
			break;
		case ARCHIVE_ENTRY_ACL_TYPE_ALARM:
			r = acl_set_entry_type_np(acl_entry,
			    ACL_ENTRY_TYPE_ALARM);
			break;
		case ARCHIVE_ENTRY_ACL_TYPE_ACCESS:
		case ARCHIVE_ENTRY_ACL_TYPE_DEFAULT:
			// These don't translate directly into the system ACL.
			break;
		default:
			archive_set_error(a, ARCHIVE_ERRNO_MISC,
			    "Unsupported ACL entry type");
			ret = ARCHIVE_FAILED;
			goto exit_free;
		}

		if (r != 0) {
			archive_set_error(a, errno,
			    "Failed to set ACL entry type");
			ret = ARCHIVE_FAILED;
			goto exit_free;
		}
#endif

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
#if ARCHIVE_ACL_FREEBSD_NFS4
		if (ae_requested_type == ARCHIVE_ENTRY_ACL_TYPE_NFS4) {
			perm_map_size = acl_nfs4_perm_map_size;
			perm_map = acl_nfs4_perm_map;
		} else {
#endif
			perm_map_size = acl_posix_perm_map_size;
			perm_map = acl_posix_perm_map;
#if ARCHIVE_ACL_FREEBSD_NFS4
		}
#endif

		for (i = 0; i < perm_map_size; ++i) {
			if (ae_permset & perm_map[i].a_perm) {
				if (acl_add_perm(acl_permset,
				    perm_map[i].p_perm) != 0) {
					archive_set_error(a, errno,
					    "Failed to add ACL permission");
					ret = ARCHIVE_FAILED;
					goto exit_free;
				}
			}
		}

#if ARCHIVE_ACL_FREEBSD_NFS4
		if (ae_requested_type == ARCHIVE_ENTRY_ACL_TYPE_NFS4) {
			/*
			 * acl_get_flagset_np() fails with non-NFSv4 ACLs
			 */
			if (acl_get_flagset_np(acl_entry, &acl_flagset) != 0) {
				archive_set_error(a, errno,
				    "Failed to get flagset from an NFSv4 "
				    "ACL entry");
				ret = ARCHIVE_FAILED;
				goto exit_free;
			}
			if (acl_clear_flags_np(acl_flagset) != 0) {
				archive_set_error(a, errno,
				    "Failed to clear flags from an NFSv4 "
				    "ACL flagset");
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
#endif
	}

	/* Try restoring the ACL through 'fd' if we can. */
	if (fd >= 0) {
		if (acl_set_fd_np(fd, acl, acl_type) == 0)
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
	}
#if HAVE_ACL_SET_LINK_NP
	else if (acl_set_link_np(name, acl_type, acl) != 0)
#else
	/* FreeBSD older than 8.0 */
	else if (acl_set_file(name, acl_type, acl) != 0)
#endif
	{
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

	if ((archive_acl_types(abstract_acl)
	    & ARCHIVE_ENTRY_ACL_TYPE_POSIX1E) != 0) {
		if ((archive_acl_types(abstract_acl)
		    & ARCHIVE_ENTRY_ACL_TYPE_ACCESS) != 0) {
			ret = set_acl(a, fd, name, abstract_acl,
			    ARCHIVE_ENTRY_ACL_TYPE_ACCESS, "access");
			if (ret != ARCHIVE_OK)
				return (ret);
		}
		if ((archive_acl_types(abstract_acl)
		    & ARCHIVE_ENTRY_ACL_TYPE_DEFAULT) != 0)
			ret = set_acl(a, fd, name, abstract_acl,
			    ARCHIVE_ENTRY_ACL_TYPE_DEFAULT, "default");

		/* Simultaneous POSIX.1e and NFSv4 is not supported */
		return (ret);
	}
#if ARCHIVE_ACL_FREEBSD_NFS4
	else if ((archive_acl_types(abstract_acl) &
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4) != 0) {
		ret = set_acl(a, fd, name, abstract_acl,
		    ARCHIVE_ENTRY_ACL_TYPE_NFS4, "nfs4");
	}
#endif
	return (ret);
}
