/*-
 * Copyright (c) 1999, 2000, 2001 Robert N. M. Watson
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
/*
 * Support functionality for the POSIX.1e ACL interface
 * These calls are intended only to be called within the library.
 */

#include <sys/types.h>
#include "namespace.h"
#include <sys/acl.h>
#include "un-namespace.h"
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>

#include "acl_support.h"

#define ACL_STRING_PERM_WRITE   'w'
#define ACL_STRING_PERM_READ    'r'
#define ACL_STRING_PERM_EXEC    'x'
#define ACL_STRING_PERM_NONE    '-'

/*
 * _posix1e_acl_entry_compare -- compare two acl_entry structures to
 * determine the order they should appear in.  Used by _posix1e_acl_sort to
 * sort ACL entries into the kernel-desired order -- i.e., the order useful
 * for evaluation and O(n) validity checking.  Beter to have an O(nlogn) sort
 * in userland and an O(n) in kernel than to have both in kernel.
 */
typedef int (*compare)(const void *, const void *);
static int
_posix1e_acl_entry_compare(struct acl_entry *a, struct acl_entry *b)
{
	/*
	 * First, sort between tags -- conveniently defined in the correct
	 * order for verification.
	 */
	if (a->ae_tag < b->ae_tag)
		return (-1);
	if (a->ae_tag > b->ae_tag)
		return (1);

	/*
	 * Next compare uids/gids on appropriate types.
	 */

	if (a->ae_tag == ACL_USER || a->ae_tag == ACL_GROUP) {
		if (a->ae_id < b->ae_id)
			return (-1);
		if (a->ae_id > b->ae_id)
			return (1);

		/* shouldn't be equal, fall through to the invalid case */
	}

	/*
	 * Don't know how to sort multiple entries of the rest--either it's
	 * a bad entry, or there shouldn't be more than one.  Ignore and the
	 * validity checker can get it later.
	 */
	return (0);
}

/*
 * _posix1e_acl_sort -- sort ACL entries in POSIX.1e-formatted ACLs
 * Give the opportunity to fail, although we don't currently have a way
 * to fail.
 */
int
_posix1e_acl_sort(acl_t acl)
{
	struct acl *acl_int;

	acl_int = &acl->ats_acl;

	qsort(&acl_int->acl_entry[0], acl_int->acl_cnt,
	    sizeof(struct acl_entry), (compare) _posix1e_acl_entry_compare);

	return (0);
}

/*
 * acl_posix1e -- in what situations should we acl_sort before submission?
 * We apply posix1e ACL semantics for any ACL of type ACL_TYPE_ACCESS or
 * ACL_TYPE_DEFAULT
 */
int
_posix1e_acl(acl_t acl, acl_type_t type)
{

	return ((type == ACL_TYPE_ACCESS) || (type == ACL_TYPE_DEFAULT));
}

/*
 * _posix1e_acl_check -- given an ACL, check its validity.  This is mirrored
 * from code in sys/kern/kern_acl.c, and if changes are made in one, they
 * should be made in the other also.  This copy of acl_check is made
 * available * in userland for the benefit of processes wanting to check ACLs
 * for validity before submitting them to the kernel, or for performing 
 * in userland file system checking.  Needless to say, the kernel makes
 * the real checks on calls to get/setacl.
 *
 * See the comments in kernel for explanation -- just briefly, it assumes
 * an already sorted ACL, and checks based on that assumption.  The
 * POSIX.1e interface, acl_valid(), will perform the sort before calling
 * this.  Returns 0 on success, EINVAL on failure.
 */
int
_posix1e_acl_check(acl_t acl)
{
	struct acl *acl_int;
	struct acl_entry	*entry; 	/* current entry */
	uid_t	obj_uid=-1, obj_gid=-1, highest_uid=0, highest_gid=0;
	int	stage = ACL_USER_OBJ;
	int	i = 0;
	int	count_user_obj=0, count_user=0, count_group_obj=0,
		count_group=0, count_mask=0, count_other=0;

	acl_int = &acl->ats_acl;

	/* printf("_posix1e_acl_check: checking acl with %d entries\n",
	    acl->acl_cnt); */
	while (i < acl_int->acl_cnt) {
		entry = &acl_int->acl_entry[i];

		if ((entry->ae_perm | ACL_PERM_BITS) != ACL_PERM_BITS)
			return (EINVAL);

		switch(entry->ae_tag) {
		case ACL_USER_OBJ:
			/* printf("_posix1e_acl_check: %d: ACL_USER_OBJ\n",
			    i); */
			if (stage > ACL_USER_OBJ)
				return (EINVAL);
			stage = ACL_USER;
			count_user_obj++;
			obj_uid = entry->ae_id;
			break;
	
		case ACL_USER:
			/* printf("_posix1e_acl_check: %d: ACL_USER\n", i); */
			if (stage > ACL_USER)
				return (EINVAL);
			stage = ACL_USER;
			if (entry->ae_id == obj_uid)
				return (EINVAL);
			if (count_user && (entry->ae_id <= highest_uid))
				return (EINVAL);
			highest_uid = entry->ae_id;
			count_user++;
			break;	
	
		case ACL_GROUP_OBJ:
			/* printf("_posix1e_acl_check: %d: ACL_GROUP_OBJ\n",
			    i); */
			if (stage > ACL_GROUP_OBJ)
				return (EINVAL);
			stage = ACL_GROUP;
			count_group_obj++;
			obj_gid = entry->ae_id;
			break;
	
		case ACL_GROUP:
			/* printf("_posix1e_acl_check: %d: ACL_GROUP\n", i); */
			if (stage > ACL_GROUP)
				return (EINVAL);
			stage = ACL_GROUP;
			if (entry->ae_id == obj_gid)
				return (EINVAL);
			if (count_group && (entry->ae_id <= highest_gid))
				return (EINVAL);
			highest_gid = entry->ae_id;
			count_group++;
			break;
			
		case ACL_MASK:
			/* printf("_posix1e_acl_check: %d: ACL_MASK\n", i); */
			if (stage > ACL_MASK)
				return (EINVAL);
			stage = ACL_MASK;
			count_mask++;
			break;
	
		case ACL_OTHER:
			/* printf("_posix1e_acl_check: %d: ACL_OTHER\n", i); */
			if (stage > ACL_OTHER)
				return (EINVAL);
			stage = ACL_OTHER;
			count_other++;
			break;
	
		default:
			/* printf("_posix1e_acl_check: %d: INVALID\n", i); */
			return (EINVAL);
		}
		i++;
	}

	if (count_user_obj != 1)
		return (EINVAL);
	
	if (count_group_obj != 1)
		return (EINVAL);

	if (count_mask != 0 && count_mask != 1)
		return (EINVAL);

	if (count_other != 1)
		return (EINVAL);

	return (0);
}


/*
 * Given a uid/gid, return a username/groupname for the text form of an ACL
 * XXX NOT THREAD SAFE, RELIES ON GETPWUID, GETGRGID
 * XXX USES *PW* AND *GR* WHICH ARE STATEFUL AND THEREFORE THIS ROUTINE
 * MAY HAVE SIDE-EFFECTS
 */
int
_posix1e_acl_id_to_name(acl_tag_t tag, uid_t id, ssize_t buf_len, char *buf)
{
	struct group	*g;
	struct passwd	*p;
	int	i;

	switch(tag) {
	case ACL_USER:
		p = getpwuid(id);
		if (!p)
			i = snprintf(buf, buf_len, "%d", id);
		else
			i = snprintf(buf, buf_len, "%s", p->pw_name);

		if (i < 0 || i >= buf_len) {
			errno = ENOMEM;
			return (-1);
		}
		return (0);

	case ACL_GROUP:
		g = getgrgid(id);
		if (g == NULL) 
			i = snprintf(buf, buf_len, "%d", id);
		else
			i = snprintf(buf, buf_len, "%s", g->gr_name);

		if (i < 0 || i >= buf_len) {
			errno = ENOMEM;
			return (-1);
		}
		return (0);

	default:
		return (EINVAL);
	}
}


/*
 * Given a username/groupname from a text form of an ACL, return the uid/gid
 * XXX NOT THREAD SAFE, RELIES ON GETPWNAM, GETGRNAM
 * XXX USES *PW* AND *GR* WHICH ARE STATEFUL AND THEREFORE THIS ROUTINE
 * MAY HAVE SIDE-EFFECTS
 *
 * XXX currently doesn't deal correctly with a numeric uid being passed
 * instead of a username.  What is correct behavior here?  Check chown.
 */
int
_posix1e_acl_name_to_id(acl_tag_t tag, char *name, uid_t *id)
{
	struct group	*g;
	struct passwd	*p;
	unsigned long	l;
	char 		*endp;

	switch(tag) {
	case ACL_USER:
		p = getpwnam(name);
		if (p == NULL) {
			l = strtoul(name, &endp, 0);
			if (*endp != '\0' || l != (unsigned long)(uid_t)l) {
				errno = EINVAL;
				return (-1);
			}
			*id = (uid_t)l;
			return (0);
		}
		*id = p->pw_uid;
		return (0);

	case ACL_GROUP:
		g = getgrnam(name);
		if (g == NULL) {
			l = strtoul(name, &endp, 0);
			if (*endp != '\0' || l != (unsigned long)(gid_t)l) {
				errno = EINVAL;
				return (-1);
			}
			*id = (gid_t)l;
			return (0);
		}
		*id = g->gr_gid;
		return (0);

	default:
		return (EINVAL);
	}
}


/*
 * Given a right-shifted permission (i.e., direct ACL_PERM_* mask), fill
 * in a string describing the permissions.
 */
int
_posix1e_acl_perm_to_string(acl_perm_t perm, ssize_t buf_len, char *buf)
{

	if (buf_len < _POSIX1E_ACL_STRING_PERM_MAXSIZE + 1) {
		errno = ENOMEM;
		return (-1);
	}

	if ((perm | ACL_PERM_BITS) != ACL_PERM_BITS) {
		errno = EINVAL;
		return (-1);
	}

	buf[3] = 0;	/* null terminate */

	if (perm & ACL_READ)
		buf[0] = ACL_STRING_PERM_READ;
	else
		buf[0] = ACL_STRING_PERM_NONE;

	if (perm & ACL_WRITE)
		buf[1] = ACL_STRING_PERM_WRITE;
	else
		buf[1] = ACL_STRING_PERM_NONE;

	if (perm & ACL_EXECUTE)
		buf[2] = ACL_STRING_PERM_EXEC;
	else
		buf[2] = ACL_STRING_PERM_NONE;

	return (0);
}

/*
 * given a string, return a permission describing it
 */
int
_posix1e_acl_string_to_perm(char *string, acl_perm_t *perm)
{
	acl_perm_t	myperm = ACL_PERM_NONE;
	char	*ch;

	ch = string;
	while (*ch) {
		switch(*ch) {
		case ACL_STRING_PERM_READ:
			myperm |= ACL_READ;
			break;
		case ACL_STRING_PERM_WRITE:
			myperm |= ACL_WRITE;
			break;
		case ACL_STRING_PERM_EXEC:
			myperm |= ACL_EXECUTE;
			break;
		case ACL_STRING_PERM_NONE:
			break;
		default:
			return (EINVAL);
		}
		ch++;
	}

	*perm = myperm;
	return (0);
}

/*
 * Add an ACL entry without doing much checking, et al
 */
int
_posix1e_acl_add_entry(acl_t acl, acl_tag_t tag, uid_t id, acl_perm_t perm)
{
	struct acl		*acl_int;
	struct acl_entry	*e;

	acl_int = &acl->ats_acl;

	if (acl_int->acl_cnt >= ACL_MAX_ENTRIES) {
		errno = ENOMEM;
		return (-1);
	}

	e = &(acl_int->acl_entry[acl_int->acl_cnt]);
	e->ae_perm = perm;
	e->ae_tag = tag;
	e->ae_id = id;
	acl_int->acl_cnt++;

	return (0);
}
