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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_ACL_H
#include <sys/acl.h>
#endif

#include "archive_entry.h"
#include "archive_private.h"
#include "archive_read_disk_private.h"
#include "archive_acl_maps.h"

/*
 * Solaris-specific ACL functions and helper functions
 *
 * Exported functions:
 * translate_acl()
 */
static void *
sunacl_get(int cmd, int *aclcnt, int fd, const char *path)
{
	int cnt, cntcmd;
	size_t size;
	void *aclp;

	if (cmd == GETACL) {
		cntcmd = GETACLCNT;
		size = sizeof(aclent_t);
	}
#if ARCHIVE_ACL_SUNOS_NFS4
	else if (cmd == ACE_GETACL) {
		cntcmd = ACE_GETACLCNT;
		size = sizeof(ace_t);
	}
#endif
	else {
		errno = EINVAL;
		*aclcnt = -1;
		return (NULL);
	}

	aclp = NULL;
	cnt = -2;

	while (cnt == -2 || (cnt == -1 && errno == ENOSPC)) {
		if (path != NULL)
			cnt = acl(path, cntcmd, 0, NULL);
		else
			cnt = facl(fd, cntcmd, 0, NULL);

		if (cnt > 0) {
			if (aclp == NULL)
				aclp = malloc(cnt * size);
			else
				aclp = realloc(NULL, cnt * size);
			if (aclp != NULL) {
				if (path != NULL)
					cnt = acl(path, cmd, cnt, aclp);
				else
					cnt = facl(fd, cmd, cnt, aclp);
			}
		} else {
			if (aclp != NULL) {
				free(aclp);
				aclp = NULL;
			}
			break;
		}
	}

	*aclcnt = cnt;
	return (aclp);
}

/*
 * Check if acl is trivial
 * This is a FreeBSD acl_is_trivial_np() implementation for Solaris
 */
static int
sun_acl_is_trivial(void *aclp, int aclcnt, mode_t mode, int is_nfs4,
    int is_dir, int *trivialp)
{
#if ARCHIVE_ACL_SUNOS_NFS4
	int i, p;
	const uint32_t rperm = ACE_READ_DATA;
	const uint32_t wperm = ACE_WRITE_DATA | ACE_APPEND_DATA;
	const uint32_t eperm = ACE_EXECUTE;
	const uint32_t pubset = ACE_READ_ATTRIBUTES | ACE_READ_NAMED_ATTRS |
	    ACE_READ_ACL | ACE_SYNCHRONIZE;
	const uint32_t ownset = pubset | ACE_WRITE_ATTRIBUTES |
	    ACE_WRITE_NAMED_ATTRS | ACE_WRITE_ACL | ACE_WRITE_OWNER;

	ace_t *ace;
	ace_t tace[6];
#endif

	if (aclp == NULL || trivialp == NULL)
		return (-1);

	*trivialp = 0;

	/*
	 * POSIX.1e ACLs marked with ACL_IS_TRIVIAL are compatible with
	 * FreeBSD acl_is_trivial_np(). On Solaris they have 4 entries,
	 * including mask.
	 */
	if (!is_nfs4) {
		if (aclcnt == 4)
			*trivialp = 1;
		return (0);
	}

#if ARCHIVE_ACL_SUNOS_NFS4
	/*
	 * Continue with checking NFSv4 ACLs
	 *
	 * Create list of trivial ace's to be compared
	 */

	/* owner@ allow pre */
	tace[0].a_flags = ACE_OWNER;
	tace[0].a_type = ACE_ACCESS_ALLOWED_ACE_TYPE;
	tace[0].a_access_mask = 0;

	/* owner@ deny */
	tace[1].a_flags = ACE_OWNER;
	tace[1].a_type = ACE_ACCESS_DENIED_ACE_TYPE;
	tace[1].a_access_mask = 0;

	/* group@ deny */
	tace[2].a_flags = ACE_GROUP | ACE_IDENTIFIER_GROUP;
	tace[2].a_type = ACE_ACCESS_DENIED_ACE_TYPE;
	tace[2].a_access_mask = 0;

	/* owner@ allow */
	tace[3].a_flags = ACE_OWNER;
	tace[3].a_type = ACE_ACCESS_ALLOWED_ACE_TYPE;
	tace[3].a_access_mask = ownset;

	/* group@ allow */
	tace[4].a_flags = ACE_GROUP | ACE_IDENTIFIER_GROUP;
	tace[4].a_type = ACE_ACCESS_ALLOWED_ACE_TYPE;
	tace[4].a_access_mask = pubset;

	/* everyone@ allow */
	tace[5].a_flags = ACE_EVERYONE;
	tace[5].a_type = ACE_ACCESS_ALLOWED_ACE_TYPE;
	tace[5].a_access_mask = pubset;

	/* Permissions for everyone@ */
	if (mode & 0004)
		tace[5].a_access_mask |= rperm;
	if (mode & 0002)
		tace[5].a_access_mask |= wperm;
	if (mode & 0001)
		tace[5].a_access_mask |= eperm;

	/* Permissions for group@ */
	if (mode & 0040)
		tace[4].a_access_mask |= rperm;
	else if (mode & 0004)
		tace[2].a_access_mask |= rperm;
	if (mode & 0020)
		tace[4].a_access_mask |= wperm;
	else if (mode & 0002)
		tace[2].a_access_mask |= wperm;
	if (mode & 0010)
		tace[4].a_access_mask |= eperm;
	else if (mode & 0001)
		tace[2].a_access_mask |= eperm;

	/* Permissions for owner@ */
	if (mode & 0400) {
		tace[3].a_access_mask |= rperm;
		if (!(mode & 0040) && (mode & 0004))
			tace[0].a_access_mask |= rperm;
	} else if ((mode & 0040) || (mode & 0004))
		tace[1].a_access_mask |= rperm;
	if (mode & 0200) {
		tace[3].a_access_mask |= wperm;
		if (!(mode & 0020) && (mode & 0002))
			tace[0].a_access_mask |= wperm;
	} else if ((mode & 0020) || (mode & 0002))
		tace[1].a_access_mask |= wperm;
	if (mode & 0100) {
		tace[3].a_access_mask |= eperm;
		if (!(mode & 0010) && (mode & 0001))
			tace[0].a_access_mask |= eperm;
	} else if ((mode & 0010) || (mode & 0001))
		tace[1].a_access_mask |= eperm;

	/* Check if the acl count matches */
	p = 3;
	for (i = 0; i < 3; i++) {
		if (tace[i].a_access_mask != 0)
			p++;
	}
	if (aclcnt != p)
		return (0);

	p = 0;
	for (i = 0; i < 6; i++) {
		if (tace[i].a_access_mask != 0) {
			ace = &((ace_t *)aclp)[p];
			/*
			 * Illumos added ACE_DELETE_CHILD to write perms for
			 * directories. We have to check against that, too.
			 */
			if (ace->a_flags != tace[i].a_flags ||
			    ace->a_type != tace[i].a_type ||
			    (ace->a_access_mask != tace[i].a_access_mask &&
			    (!is_dir || (tace[i].a_access_mask & wperm) == 0 ||
			    ace->a_access_mask !=
			    (tace[i].a_access_mask | ACE_DELETE_CHILD))))
				return (0);
			p++;
		}
	}

	*trivialp = 1;
#else	/* !ARCHIVE_ACL_SUNOS_NFS4 */
	(void)is_dir;	/* UNUSED */
	(void)aclp;	/* UNUSED */
#endif	/* !ARCHIVE_ACL_SUNOS_NFS4 */
	return (0);
}

/*
 * Translate Solaris POSIX.1e and NFSv4 ACLs into libarchive internal ACL
 */
static int
translate_acl(struct archive_read_disk *a,
    struct archive_entry *entry, void *aclp, int aclcnt,
    int default_entry_acl_type)
{
	int e, i;
	int ae_id, ae_tag, ae_perm;
	int entry_acl_type;
	const char *ae_name;
	aclent_t *aclent;
#if ARCHIVE_ACL_SUNOS_NFS4
	ace_t *ace;
#endif

	if (aclcnt <= 0)
		return (ARCHIVE_OK);

	for (e = 0; e < aclcnt; e++) {
		ae_name = NULL;
		ae_tag = 0;
		ae_perm = 0;

#if ARCHIVE_ACL_SUNOS_NFS4
		if (default_entry_acl_type == ARCHIVE_ENTRY_ACL_TYPE_NFS4) {
			ace = &((ace_t *)aclp)[e];
			ae_id = ace->a_who;

			switch(ace->a_type) {
			case ACE_ACCESS_ALLOWED_ACE_TYPE:
				entry_acl_type = ARCHIVE_ENTRY_ACL_TYPE_ALLOW;
				break;
			case ACE_ACCESS_DENIED_ACE_TYPE:
				entry_acl_type = ARCHIVE_ENTRY_ACL_TYPE_DENY;
				break;
			case ACE_SYSTEM_AUDIT_ACE_TYPE:
				entry_acl_type = ARCHIVE_ENTRY_ACL_TYPE_ACCESS;
				break;
			case ACE_SYSTEM_ALARM_ACE_TYPE:
				entry_acl_type = ARCHIVE_ENTRY_ACL_TYPE_ALARM;
				break;
			default:
				/* Unknown entry type, skip */
				continue;
			}

			if ((ace->a_flags & ACE_OWNER) != 0)
				ae_tag = ARCHIVE_ENTRY_ACL_USER_OBJ;
			else if ((ace->a_flags & ACE_GROUP) != 0)
				ae_tag = ARCHIVE_ENTRY_ACL_GROUP_OBJ;
			else if ((ace->a_flags & ACE_EVERYONE) != 0)
				ae_tag = ARCHIVE_ENTRY_ACL_EVERYONE;
			else if ((ace->a_flags & ACE_IDENTIFIER_GROUP) != 0) {
				ae_tag = ARCHIVE_ENTRY_ACL_GROUP;
				ae_name = archive_read_disk_gname(&a->archive,
				    ae_id);
			} else {
				ae_tag = ARCHIVE_ENTRY_ACL_USER;
				ae_name = archive_read_disk_uname(&a->archive,
				    ae_id);
			}

			for (i = 0; i < acl_nfs4_flag_map_size; ++i) {
				if ((ace->a_flags &
				    acl_nfs4_flag_map[i].p_perm) != 0)
					ae_perm |= acl_nfs4_flag_map[i].a_perm;
			}

			for (i = 0; i < acl_nfs4_perm_map_size; ++i) {
				if ((ace->a_access_mask &
				    acl_nfs4_perm_map[i].p_perm) != 0)
					ae_perm |= acl_nfs4_perm_map[i].a_perm;
			}
		} else
#endif	/* ARCHIVE_ACL_SUNOS_NFS4 */
		if (default_entry_acl_type == ARCHIVE_ENTRY_ACL_TYPE_ACCESS) {
			aclent = &((aclent_t *)aclp)[e];
			if ((aclent->a_type & ACL_DEFAULT) != 0)
				entry_acl_type = ARCHIVE_ENTRY_ACL_TYPE_DEFAULT;
			else
				entry_acl_type = ARCHIVE_ENTRY_ACL_TYPE_ACCESS;
			ae_id = aclent->a_id;

			switch(aclent->a_type) {
			case DEF_USER:
			case USER:
				ae_name = archive_read_disk_uname(&a->archive,
				    ae_id);
				ae_tag = ARCHIVE_ENTRY_ACL_USER;
				break;
			case DEF_GROUP:
			case GROUP:
				ae_name = archive_read_disk_gname(&a->archive,
				    ae_id);
				ae_tag = ARCHIVE_ENTRY_ACL_GROUP;
				break;
			case DEF_CLASS_OBJ:
			case CLASS_OBJ:
				ae_tag = ARCHIVE_ENTRY_ACL_MASK;
				break;
			case DEF_USER_OBJ:
			case USER_OBJ:
				ae_tag = ARCHIVE_ENTRY_ACL_USER_OBJ;
				break;
			case DEF_GROUP_OBJ:
			case GROUP_OBJ:
				ae_tag = ARCHIVE_ENTRY_ACL_GROUP_OBJ;
				break;
			case DEF_OTHER_OBJ:
			case OTHER_OBJ:
				ae_tag = ARCHIVE_ENTRY_ACL_OTHER;
				break;
			default:
				/* Unknown tag type, skip */
				continue;
			}

			for (i = 0; i < acl_posix_perm_map_size; ++i) {
				if ((aclent->a_perm &
				    acl_posix_perm_map[i].p_perm) != 0)
					ae_perm |= acl_posix_perm_map[i].a_perm;
			}
		} else
			return (ARCHIVE_WARN);

		archive_entry_acl_add_entry(entry, entry_acl_type,
		    ae_perm, ae_tag, ae_id, ae_name);
	}
	return (ARCHIVE_OK);
}

int
archive_read_disk_entry_setup_acls(struct archive_read_disk *a,
    struct archive_entry *entry, int *fd)
{
	const char	*accpath;
	void		*aclp;
	int		aclcnt;
	int		r;

	accpath = NULL;

	if (*fd < 0) {
		accpath = archive_read_disk_entry_setup_path(a, entry, fd);
		if (accpath == NULL)
			return (ARCHIVE_WARN);
	}

	archive_entry_acl_clear(entry);

	aclp = NULL;

#if ARCHIVE_ACL_SUNOS_NFS4
	if (*fd >= 0)
		aclp = sunacl_get(ACE_GETACL, &aclcnt, *fd, NULL);
	else if ((!a->follow_symlinks)
	    && (archive_entry_filetype(entry) == AE_IFLNK))
		/* We can't get the ACL of a symlink, so we assume it can't
		   have one. */
		aclp = NULL;
	else
		aclp = sunacl_get(ACE_GETACL, &aclcnt, 0, accpath);

	if (aclp != NULL && sun_acl_is_trivial(aclp, aclcnt,
	    archive_entry_mode(entry), 1, S_ISDIR(archive_entry_mode(entry)),
	    &r) == 0 && r == 1) {
		free(aclp);
		aclp = NULL;
		return (ARCHIVE_OK);
	}

	if (aclp != NULL) {
		r = translate_acl(a, entry, aclp, aclcnt,
		    ARCHIVE_ENTRY_ACL_TYPE_NFS4);
		free(aclp);
		aclp = NULL;

		if (r != ARCHIVE_OK) {
			archive_set_error(&a->archive, errno,
			    "Couldn't translate NFSv4 ACLs");
		}
		return (r);
	}
#endif	/* ARCHIVE_ACL_SUNOS_NFS4 */

	/* Retrieve POSIX.1e ACLs from file. */
	if (*fd >= 0)
		aclp = sunacl_get(GETACL, &aclcnt, *fd, NULL);
	else if ((!a->follow_symlinks)
	    && (archive_entry_filetype(entry) == AE_IFLNK))
		/* We can't get the ACL of a symlink, so we assume it can't
		   have one. */
		aclp = NULL;
	else
		aclp = sunacl_get(GETACL, &aclcnt, 0, accpath);

	/* Ignore "trivial" ACLs that just mirror the file mode. */
	if (aclp != NULL && sun_acl_is_trivial(aclp, aclcnt,
	    archive_entry_mode(entry), 0, S_ISDIR(archive_entry_mode(entry)),
	    &r) == 0 && r == 1) {
		free(aclp);
		aclp = NULL;
	}

	if (aclp != NULL)
	{
		r = translate_acl(a, entry, aclp, aclcnt,
		    ARCHIVE_ENTRY_ACL_TYPE_ACCESS);
		free(aclp);
		aclp = NULL;

		if (r != ARCHIVE_OK) {
			archive_set_error(&a->archive, errno,
			    "Couldn't translate access ACLs");
			return (r);
		}
	}

	return (ARCHIVE_OK);
}
