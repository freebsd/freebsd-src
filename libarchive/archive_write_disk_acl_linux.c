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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if HAVE_ACL_LIBACL_H
#include <acl/libacl.h>
#endif
#ifdef HAVE_SYS_ACL_H
#include <sys/acl.h>
#endif
#if HAVE_SYS_RICHACL_H
#include <sys/richacl.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_write_disk_private.h"
#include "archive_acl_maps.h"

#if ARCHIVE_ACL_LIBRICHACL
static int
_richacl_mode_to_mask(short mode)
{
	int mask = 0;

	if (mode & S_IROTH)
		mask |= RICHACE_POSIX_MODE_READ;
	if (mode & S_IWOTH)
		mask |= RICHACE_POSIX_MODE_WRITE;
	if (mode & S_IXOTH)
		mask |= RICHACE_POSIX_MODE_EXEC;

	return (mask);
}

static void
_richacl_mode_to_masks(struct richacl *richacl, __LA_MODE_T mode)
{
	richacl->a_owner_mask = _richacl_mode_to_mask((mode & 0700) >> 6);
	richacl->a_group_mask = _richacl_mode_to_mask((mode & 0070) >> 3);
	richacl->a_other_mask = _richacl_mode_to_mask(mode & 0007);
}
#endif /* ARCHIVE_ACL_LIBRICHACL */

#if ARCHIVE_ACL_LIBRICHACL
static int
set_richacl(struct archive *a, int fd, const char *name,
    struct archive_acl *abstract_acl, __LA_MODE_T mode,
    int ae_requested_type, const char *tname)
{
	int		 ae_type, ae_permset, ae_tag, ae_id;
	uid_t		 ae_uid;
	gid_t		 ae_gid;
	const char	*ae_name;
	int		 entries;
	int		 i;
	int		 ret;
	int		 e = 0;
	struct richacl  *richacl = NULL;
	struct richace  *richace;

	ret = ARCHIVE_OK;
	entries = archive_acl_reset(abstract_acl, ae_requested_type);
	if (entries == 0)
		return (ARCHIVE_OK);

	if (ae_requested_type != ARCHIVE_ENTRY_ACL_TYPE_NFS4) {
		errno = ENOENT;
		archive_set_error(a, errno, "Unsupported ACL type");
		return (ARCHIVE_FAILED);
	}

	richacl = richacl_alloc(entries);
	if (richacl == NULL) {
		archive_set_error(a, errno,
			"Failed to initialize RichACL working storage");
		return (ARCHIVE_FAILED);
	}

	e = 0;

	while (archive_acl_next(a, abstract_acl, ae_requested_type, &ae_type,
		   &ae_permset, &ae_tag, &ae_id, &ae_name) == ARCHIVE_OK) {
		richace = &(richacl->a_entries[e]);

		richace->e_flags = 0;
		richace->e_mask = 0;

		switch (ae_tag) {
		case ARCHIVE_ENTRY_ACL_USER:
			ae_uid = archive_write_disk_uid(a, ae_name, ae_id);
			richace->e_id = ae_uid;
			break;
		case ARCHIVE_ENTRY_ACL_GROUP:
			ae_gid = archive_write_disk_gid(a, ae_name, ae_id);
			richace->e_id = ae_gid;
			richace->e_flags |= RICHACE_IDENTIFIER_GROUP;
			break;
		case ARCHIVE_ENTRY_ACL_USER_OBJ:
			richace->e_flags |= RICHACE_SPECIAL_WHO;
			richace->e_id = RICHACE_OWNER_SPECIAL_ID;
			break;
		case ARCHIVE_ENTRY_ACL_GROUP_OBJ:
			richace->e_flags |= RICHACE_SPECIAL_WHO;
			richace->e_id = RICHACE_GROUP_SPECIAL_ID;
			break;
		case ARCHIVE_ENTRY_ACL_EVERYONE:
			richace->e_flags |= RICHACE_SPECIAL_WHO;
			richace->e_id = RICHACE_EVERYONE_SPECIAL_ID;
			break;
		default:
			archive_set_error(a, ARCHIVE_ERRNO_MISC,
			    "Unsupported ACL tag");
			ret = ARCHIVE_FAILED;
			goto exit_free;
		}

		switch (ae_type) {
			case ARCHIVE_ENTRY_ACL_TYPE_ALLOW:
				richace->e_type =
				    RICHACE_ACCESS_ALLOWED_ACE_TYPE;
				break;
			case ARCHIVE_ENTRY_ACL_TYPE_DENY:
				richace->e_type =
				    RICHACE_ACCESS_DENIED_ACE_TYPE;
				break;
			case ARCHIVE_ENTRY_ACL_TYPE_AUDIT:
			case ARCHIVE_ENTRY_ACL_TYPE_ALARM:
				break;
		default:
			archive_set_error(a, ARCHIVE_ERRNO_MISC,
			    "Unsupported ACL entry type");
			ret = ARCHIVE_FAILED;
			goto exit_free;
		}

		for (i = 0; i < acl_nfs4_perm_map_size; ++i) {
			if (ae_permset & acl_nfs4_perm_map[i].a_perm)
				richace->e_mask |= acl_nfs4_perm_map[i].p_perm;
		}

		for (i = 0; i < acl_nfs4_flag_map_size; ++i) {
			if (ae_permset &
			    acl_nfs4_flag_map[i].a_perm)
				richace->e_flags |= acl_nfs4_flag_map[i].p_perm;
		}
	e++;
	}

	/* Fill RichACL masks */
	_richacl_mode_to_masks(richacl, mode);

	if (fd >= 0) {
		if (richacl_set_fd(fd, richacl) == 0)
			ret = ARCHIVE_OK;
		else {
			if (errno == EOPNOTSUPP) {
				/* Filesystem doesn't support ACLs */
				ret = ARCHIVE_OK;
			} else {
				archive_set_error(a, errno,
				    "Failed to set richacl on fd: %s", tname);
				ret = ARCHIVE_WARN;
			}
		}
	} else if (richacl_set_file(name, richacl) != 0) {
		if (errno == EOPNOTSUPP) {
			/* Filesystem doesn't support ACLs */
			ret = ARCHIVE_OK;
		} else {
			archive_set_error(a, errno, "Failed to set richacl: %s",
			    tname);
			ret = ARCHIVE_WARN;
		}
	}
exit_free:
	richacl_free(richacl);
	return (ret);
}
#endif /* ARCHIVE_ACL_RICHACL */

#if ARCHIVE_ACL_LIBACL
static int
set_acl(struct archive *a, int fd, const char *name,
    struct archive_acl *abstract_acl,
    int ae_requested_type, const char *tname)
{
	int		 acl_type = 0;
	int		 ae_type, ae_permset, ae_tag, ae_id;
	uid_t		 ae_uid;
	gid_t		 ae_gid;
	const char	*ae_name;
	int		 entries;
	int		 i;
	int		 ret;
	acl_t		 acl = NULL;
	acl_entry_t	 acl_entry;
	acl_permset_t	 acl_permset;

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

		for (i = 0; i < acl_posix_perm_map_size; ++i) {
			if (ae_permset & acl_posix_perm_map[i].a_perm) {
				if (acl_add_perm(acl_permset,
				    acl_posix_perm_map[i].p_perm) != 0) {
					archive_set_error(a, errno,
					    "Failed to add ACL permission");
					ret = ARCHIVE_FAILED;
					goto exit_free;
				}
			}
		}

	}

	if (fd >= 0 && ae_requested_type == ARCHIVE_ENTRY_ACL_TYPE_ACCESS) {
		if (acl_set_fd(fd, acl) == 0)
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
	} else if (acl_set_file(name, acl_type, acl) != 0) {
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
#endif /* ARCHIVE_ACL_LIBACL */

int
archive_write_disk_set_acls(struct archive *a, int fd, const char *name,
    struct archive_acl *abstract_acl, __LA_MODE_T mode)
{
	int		ret = ARCHIVE_OK;

#if !ARCHIVE_ACL_LIBRICHACL
	(void)mode;	/* UNUSED */
#endif

#if ARCHIVE_ACL_LIBRICHACL
	if ((archive_acl_types(abstract_acl)
	    & ARCHIVE_ENTRY_ACL_TYPE_NFS4) != 0) {
		ret = set_richacl(a, fd, name, abstract_acl, mode,
		    ARCHIVE_ENTRY_ACL_TYPE_NFS4, "nfs4");
	}
#if ARCHIVE_ACL_LIBACL
	else
#endif
#endif	/* ARCHIVE_ACL_LIBRICHACL */
#if ARCHIVE_ACL_LIBACL
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
	}
#endif	/* ARCHIVE_ACL_LIBACL */
	return (ret);
}
