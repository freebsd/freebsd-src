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
	aclent_t	 *aclent;
#if ARCHIVE_ACL_SUNOS_NFS4
	ace_t		 *ace;
#endif
	int		 cmd, e, r;
	void		 *aclp;
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
	case ARCHIVE_ENTRY_ACL_TYPE_POSIX1E:
		cmd = SETACL;
		aclp = malloc(entries * sizeof(aclent_t));
		break;
#if ARCHIVE_ACL_SUNOS_NFS4
	case ARCHIVE_ENTRY_ACL_TYPE_NFS4:
		cmd = ACE_SETACL;
		aclp = malloc(entries * sizeof(ace_t));

		break;
#endif
	default:
		errno = ENOENT;
		archive_set_error(a, errno, "Unsupported ACL type");
		return (ARCHIVE_FAILED);
	}

	if (aclp == NULL) {
		archive_set_error(a, errno,
		    "Can't allocate memory for acl buffer");
		return (ARCHIVE_FAILED);
	}

	e = 0;

	while (archive_acl_next(a, abstract_acl, ae_requested_type, &ae_type,
		   &ae_permset, &ae_tag, &ae_id, &ae_name) == ARCHIVE_OK) {
		aclent = NULL;
#if ARCHIVE_ACL_SUNOS_NFS4
		ace = NULL;
#endif
		if (cmd == SETACL) {
			aclent = &((aclent_t *)aclp)[e];
			aclent->a_id = -1;
			aclent->a_type = 0;
			aclent->a_perm = 0;
		}
#if ARCHIVE_ACL_SUNOS_NFS4
		else {	/* cmd == ACE_SETACL */
			ace = &((ace_t *)aclp)[e];
			ace->a_who = -1;
			ace->a_access_mask = 0;
			ace->a_flags = 0;
		}
#endif	/* ARCHIVE_ACL_SUNOS_NFS4 */

		switch (ae_tag) {
		case ARCHIVE_ENTRY_ACL_USER:
			ae_uid = archive_write_disk_uid(a, ae_name, ae_id);
			if (aclent != NULL) {
				aclent->a_id = ae_uid;
				aclent->a_type |= USER;
			}
#if ARCHIVE_ACL_SUNOS_NFS4
			else {
				ace->a_who = ae_uid;
			}
#endif
			break;
		case ARCHIVE_ENTRY_ACL_GROUP:
			ae_gid = archive_write_disk_gid(a, ae_name, ae_id);
			if (aclent != NULL) {
				aclent->a_id = ae_gid;
				aclent->a_type |= GROUP;
			}
#if ARCHIVE_ACL_SUNOS_NFS4
			else {
				ace->a_who = ae_gid;
				ace->a_flags |= ACE_IDENTIFIER_GROUP;
			}
#endif
			break;
		case ARCHIVE_ENTRY_ACL_USER_OBJ:
			if (aclent != NULL)
				aclent->a_type |= USER_OBJ;
#if ARCHIVE_ACL_SUNOS_NFS4
			else {
				ace->a_flags |= ACE_OWNER;
			}
#endif
			break;
		case ARCHIVE_ENTRY_ACL_GROUP_OBJ:
			if (aclent != NULL)
				aclent->a_type |= GROUP_OBJ;
#if ARCHIVE_ACL_SUNOS_NFS4
			else {
				ace->a_flags |= ACE_GROUP;
				ace->a_flags |= ACE_IDENTIFIER_GROUP;
			}
#endif
			break;
		case ARCHIVE_ENTRY_ACL_MASK:
			if (aclent != NULL)
				aclent->a_type |= CLASS_OBJ;
			break;
		case ARCHIVE_ENTRY_ACL_OTHER:
			if (aclent != NULL)
				aclent->a_type |= OTHER_OBJ;
			break;
#if ARCHIVE_ACL_SUNOS_NFS4
		case ARCHIVE_ENTRY_ACL_EVERYONE:
			if (ace != NULL)
				ace->a_flags |= ACE_EVERYONE;
			break;
#endif
		default:
			archive_set_error(a, ARCHIVE_ERRNO_MISC,
			    "Unsupported ACL tag");
			ret = ARCHIVE_FAILED;
			goto exit_free;
		}

		r = 0;
		switch (ae_type) {
#if ARCHIVE_ACL_SUNOS_NFS4
		case ARCHIVE_ENTRY_ACL_TYPE_ALLOW:
			if (ace != NULL)
				ace->a_type = ACE_ACCESS_ALLOWED_ACE_TYPE;
			else
				r = -1;
			break;
		case ARCHIVE_ENTRY_ACL_TYPE_DENY:
			if (ace != NULL)
				ace->a_type = ACE_ACCESS_DENIED_ACE_TYPE;
			else
				r = -1;
			break;
		case ARCHIVE_ENTRY_ACL_TYPE_AUDIT:
			if (ace != NULL)
				ace->a_type = ACE_SYSTEM_AUDIT_ACE_TYPE;
			else
				r = -1;
			break;
		case ARCHIVE_ENTRY_ACL_TYPE_ALARM:
			if (ace != NULL)
				ace->a_type = ACE_SYSTEM_ALARM_ACE_TYPE;
			else
				r = -1;
			break;
#endif
		case ARCHIVE_ENTRY_ACL_TYPE_ACCESS:
			if (aclent == NULL)
				r = -1;
			break;
		case ARCHIVE_ENTRY_ACL_TYPE_DEFAULT:
			if (aclent != NULL)
				aclent->a_type |= ACL_DEFAULT;
			else
				r = -1;
			break;
		default:
			archive_set_error(a, ARCHIVE_ERRNO_MISC,
			    "Unsupported ACL entry type");
			ret = ARCHIVE_FAILED;
			goto exit_free;
		}

		if (r != 0) {
			errno = EINVAL;
			archive_set_error(a, errno,
			    "Failed to set ACL entry type");
			ret = ARCHIVE_FAILED;
			goto exit_free;
		}

#if ARCHIVE_ACL_SUNOS_NFS4
		if (ae_requested_type == ARCHIVE_ENTRY_ACL_TYPE_NFS4) {
			perm_map_size = acl_nfs4_perm_map_size;
			perm_map = acl_nfs4_perm_map;
		} else {
#endif
			perm_map_size = acl_posix_perm_map_size;
			perm_map = acl_posix_perm_map;
#if ARCHIVE_ACL_SUNOS_NFS4
		}
#endif
		for (i = 0; i < perm_map_size; ++i) {
			if (ae_permset & perm_map[i].a_perm) {
#if ARCHIVE_ACL_SUNOS_NFS4
				if (ae_requested_type ==
				    ARCHIVE_ENTRY_ACL_TYPE_NFS4)
					ace->a_access_mask |=
					    perm_map[i].p_perm;
				else
#endif
					aclent->a_perm |= perm_map[i].p_perm;
			}
		}

#if ARCHIVE_ACL_SUNOS_NFS4
		if (ae_requested_type == ARCHIVE_ENTRY_ACL_TYPE_NFS4) {
			for (i = 0; i < acl_nfs4_flag_map_size; ++i) {
				if (ae_permset & acl_nfs4_flag_map[i].a_perm) {
					ace->a_flags |=
					    acl_nfs4_flag_map[i].p_perm;
				}
			}
		}
#endif
	e++;
	}

	/* Try restoring the ACL through 'fd' if we can. */
	if (fd >= 0) {
		if (facl(fd, cmd, entries, aclp) == 0)
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
	} else if (acl(name, cmd, entries, aclp) != 0) {
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
	free(aclp);
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
		/* Solaris writes POSIX.1e access and default ACLs together */
		ret = set_acl(a, fd, name, abstract_acl,
		    ARCHIVE_ENTRY_ACL_TYPE_POSIX1E, "posix1e");

		/* Simultaneous POSIX.1e and NFSv4 is not supported */
		return (ret);
	}
#if ARCHIVE_ACL_SUNOS_NFS4
	else if ((archive_acl_types(abstract_acl) &
	    ARCHIVE_ENTRY_ACL_TYPE_NFS4) != 0) {
		ret = set_acl(a, fd, name, abstract_acl,
		    ARCHIVE_ENTRY_ACL_TYPE_NFS4, "nfs4");
	}
#endif
	return (ret);
}
