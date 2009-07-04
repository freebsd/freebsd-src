/*-
 * Copyright (c) 2008 Edward Tomasz Napierała <trasz@FreeBSD.org>
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
 */

/*
 * ACL support routines specific to NFSv4 access control lists.  These are
 * utility routines for code common across file systems implementing NFSv4
 * ACLs.
 */

#ifdef _KERNEL
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/priv.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/acl.h>
#else
#include <errno.h>
#include <assert.h>
#include <sys/acl.h>
#include <sys/stat.h>
#define KASSERT(a, b) assert(a)
#define CTASSERT(a)
#endif

static int
_acl_entry_matches(struct acl_entry *entry, acl_tag_t tag, acl_perm_t perm,
    acl_entry_type_t entry_type)
{
	if (entry->ae_tag != tag)
		return (0);

	if (entry->ae_id != ACL_UNDEFINED_ID)
		return (0);

	if (entry->ae_perm != perm)
		return (0);

	if (entry->ae_entry_type != entry_type)
		return (0);

	if (entry->ae_flags != 0)
		return (0);

	return (1);
}

static struct acl_entry *
_acl_append(struct acl *aclp, acl_tag_t tag, acl_perm_t perm,
    acl_entry_type_t entry_type)
{
	struct acl_entry *entry;

	KASSERT(aclp->acl_cnt + 1 <= ACL_MAX_ENTRIES,
	    ("aclp->acl_cnt + 1 <= ACL_MAX_ENTRIES"));

	entry = &(aclp->acl_entry[aclp->acl_cnt]);
	aclp->acl_cnt++;

	entry->ae_tag = tag;
	entry->ae_id = ACL_UNDEFINED_ID;
	entry->ae_perm = perm;
	entry->ae_entry_type = entry_type;
	entry->ae_flags = 0;

	return (entry);
}

static struct acl_entry *
_acl_duplicate_entry(struct acl *aclp, int entry_index)
{
	int i;

	KASSERT(aclp->acl_cnt + 1 <= ACL_MAX_ENTRIES,
	    ("aclp->acl_cnt + 1 <= ACL_MAX_ENTRIES"));

	for (i = aclp->acl_cnt; i > entry_index; i--)
		aclp->acl_entry[i] = aclp->acl_entry[i - 1];

	aclp->acl_cnt++;

	return (&(aclp->acl_entry[entry_index + 1]));
}

void
acl_nfs4_sync_acl_from_mode(struct acl *aclp, mode_t mode, int file_owner_id)
{
	int i, meets, must_append;
	struct acl_entry *entry, *copy, *previous,
	    *a1, *a2, *a3, *a4, *a5, *a6;
	mode_t amode;
	const int READ = 04;
	const int WRITE = 02;
	const int EXEC = 01;

	KASSERT(aclp->acl_cnt >= 0, ("aclp->acl_cnt >= 0"));
	KASSERT(aclp->acl_cnt <= ACL_MAX_ENTRIES,
	    ("aclp->acl_cnt <= ACL_MAX_ENTRIES"));

	/*
	 * NFSv4 Minor Version 1, draft-ietf-nfsv4-minorversion1-03.txt
	 *
	 * 3.16.6.3. Applying a Mode to an Existing ACL
	 */

	/*
	 * 1. For each ACE:
	 */
	for (i = 0; i < aclp->acl_cnt; i++) {
		entry = &(aclp->acl_entry[i]);

		/*
		 * 1.1. If the type is neither ALLOW or DENY - skip.
		 */
		if (entry->ae_entry_type != ACL_ENTRY_TYPE_ALLOW &&
		    entry->ae_entry_type != ACL_ENTRY_TYPE_DENY)
			continue;

		/*
		 * 1.2. If ACL_ENTRY_INHERIT_ONLY is set - skip.
		 */
		if (entry->ae_flags & ACL_ENTRY_INHERIT_ONLY)
			continue;

		/*
		 * 1.3. If ACL_ENTRY_FILE_INHERIT or ACL_ENTRY_DIRECTORY_INHERIT
		 *      are set:
		 */
		if (entry->ae_flags &
		    (ACL_ENTRY_FILE_INHERIT | ACL_ENTRY_DIRECTORY_INHERIT)) {
			/*
			 * 1.3.1. A copy of the current ACE is made, and placed
			 *        in the ACL immediately following the current
			 *        ACE.
			 */
			copy = _acl_duplicate_entry(aclp, i);

			/*
			 * 1.3.2. In the first ACE, the flag
			 *        ACL_ENTRY_INHERIT_ONLY is set.
			 */
			entry->ae_flags |= ACL_ENTRY_INHERIT_ONLY;

			/*
			 * 1.3.3. In the second ACE, the following flags
			 *        are cleared:
			 *        ACL_ENTRY_FILE_INHERIT,
			 *        ACL_ENTRY_DIRECTORY_INHERIT,
			 *        ACL_ENTRY_NO_PROPAGATE_INHERIT.
			 */
			copy->ae_flags &= ~(ACL_ENTRY_FILE_INHERIT |
			    ACL_ENTRY_DIRECTORY_INHERIT |
			    ACL_ENTRY_NO_PROPAGATE_INHERIT);

			/*
			 * The algorithm continues on with the second ACE.
			 */
			i++;
			entry = copy;
		}

		/*
		 * 1.4. If it's owner@, group@ or everyone@ entry, clear
		 *      ACL_READ_DATA, ACL_WRITE_DATA, ACL_APPEND_DATA
		 *      and ACL_EXECUTE.  Continue to the next entry.
		 */
		if (entry->ae_tag == ACL_USER_OBJ ||
		    entry->ae_tag == ACL_GROUP_OBJ ||
		    entry->ae_tag == ACL_EVERYONE) {
			entry->ae_perm &= ~(ACL_READ_DATA | ACL_WRITE_DATA |
			    ACL_APPEND_DATA | ACL_EXECUTE);
			continue;
		}

		/*
		 * 1.5. Otherwise, if the "who" field did not match one
		 *      of OWNER@, GROUP@, EVERYONE@:
		 *
		 * 1.5.1. If the type is ALLOW, check the preceding ACE.
		 *        If it does not meet all of the following criteria:
		 */
		if (entry->ae_entry_type != ACL_ENTRY_TYPE_ALLOW)
			continue;

		meets = 0;
		if (i > 0) {
			meets = 1;
			previous = &(aclp->acl_entry[i - 1]);

			/*
			 * 1.5.1.1. The type field is DENY,
			 */
			if (previous->ae_entry_type != ACL_ENTRY_TYPE_DENY)
				meets = 0;

			/*
			 * 1.5.1.2. The "who" field is the same as the current
			 *          ACE,
			 *
			 * 1.5.1.3. The flag bit ACE4_IDENTIFIER_GROUP
			 *          is the same as it is in the current ACE,
			 *          and no other flag bits are set,
			 */
			if (previous->ae_id != entry->ae_id ||
			    previous->ae_tag != entry->ae_tag)
				meets = 0;

			if (previous->ae_flags)
				meets = 0;

			/*
			 * 1.5.1.4. The mask bits are a subset of the mask bits
			 *          of the current ACE, and are also subset of
			 *          the following: ACL_READ_DATA,
			 *          ACL_WRITE_DATA, ACL_APPEND_DATA, ACL_EXECUTE
			 */
			if (previous->ae_perm & ~(entry->ae_perm))
				meets = 0;

			if (previous->ae_perm & ~(ACL_READ_DATA |
			    ACL_WRITE_DATA | ACL_APPEND_DATA | ACL_EXECUTE))
				meets = 0;
		}

		if (!meets) {
			/*
		 	 * Then the ACE of type DENY, with a who equal
			 * to the current ACE, flag bits equal to
			 * (<current ACE flags> & <ACE_IDENTIFIER_GROUP>)
			 * and no mask bits, is prepended.
			 */
			previous = entry;
			entry = _acl_duplicate_entry(aclp, i);

			/* Adjust counter, as we've just added an entry. */
			i++;

			previous->ae_tag = entry->ae_tag;
			previous->ae_id = entry->ae_id;
			previous->ae_flags = entry->ae_flags;
			previous->ae_perm = 0;
			previous->ae_entry_type = ACL_ENTRY_TYPE_DENY;
		}

		/*
		 * 1.5.2. The following modifications are made to the prepended
		 *        ACE.  The intent is to mask the following ACE
		 *        to disallow ACL_READ_DATA, ACL_WRITE_DATA,
		 *        ACL_APPEND_DATA, or ACL_EXECUTE, based upon the group
		 *        permissions of the new mode.  As a special case,
		 *        if the ACE matches the current owner of the file,
		 *        the owner bits are used, rather than the group bits.
		 *        This is reflected in the algorithm below.
		 */
		amode = mode >> 3;

		/*
		 * If ACE4_IDENTIFIER_GROUP is not set, and the "who" field
		 * in ACE matches the owner of the file, we shift amode three
		 * more bits, in order to have the owner permission bits
		 * placed in the three low order bits of amode.
		 */
		if (entry->ae_tag == ACL_USER && entry->ae_id == file_owner_id)
			amode = amode >> 3;

		if (entry->ae_perm & ACL_READ_DATA) {
			if (amode & READ)
				previous->ae_perm &= ~ACL_READ_DATA;
			else
				previous->ae_perm |= ACL_READ_DATA;
		}

		if (entry->ae_perm & ACL_WRITE_DATA) {
			if (amode & WRITE)
				previous->ae_perm &= ~ACL_WRITE_DATA;
			else
				previous->ae_perm |= ACL_WRITE_DATA;
		}

		if (entry->ae_perm & ACL_APPEND_DATA) {
			if (amode & WRITE)
				previous->ae_perm &= ~ACL_APPEND_DATA;
			else
				previous->ae_perm |= ACL_APPEND_DATA;
		}

		if (entry->ae_perm & ACL_EXECUTE) {
			if (amode & EXEC)
				previous->ae_perm &= ~ACL_EXECUTE;
			else
				previous->ae_perm |= ACL_EXECUTE;
		}

		/*
		 * 1.5.3. If ACE4_IDENTIFIER_GROUP is set in the flags
		 *        of the ALLOW ace:
		 *
		 * XXX: This point is not there in the Falkner's draft.
		 */
		if (entry->ae_tag == ACL_GROUP &&
		    entry->ae_entry_type == ACL_ENTRY_TYPE_ALLOW) {
			mode_t extramode, ownermode;
			extramode = (mode >> 3) & 07;
			ownermode = mode >> 6;
			extramode &= ~ownermode;

			if (extramode) {
				if (extramode & READ) {
					entry->ae_perm &= ~ACL_READ_DATA;
					previous->ae_perm &= ~ACL_READ_DATA;
				}

				if (extramode & WRITE) {
					entry->ae_perm &=
					    ~(ACL_WRITE_DATA | ACL_APPEND_DATA);
					previous->ae_perm &=
					    ~(ACL_WRITE_DATA | ACL_APPEND_DATA);
				}

				if (extramode & EXEC) {
					entry->ae_perm &= ~ACL_EXECUTE;
					previous->ae_perm &= ~ACL_EXECUTE;
				}
			}
		}
	}

	/*
	 * 2. If there at least six ACEs, the final six ACEs are examined.
	 *    If they are not equal to what we want, append six ACEs.
	 */
	must_append = 0;
	if (aclp->acl_cnt < 6) {
		must_append = 1;
	} else {
		a6 = &(aclp->acl_entry[aclp->acl_cnt - 1]);
		a5 = &(aclp->acl_entry[aclp->acl_cnt - 2]);
		a4 = &(aclp->acl_entry[aclp->acl_cnt - 3]);
		a3 = &(aclp->acl_entry[aclp->acl_cnt - 4]);
		a2 = &(aclp->acl_entry[aclp->acl_cnt - 5]);
		a1 = &(aclp->acl_entry[aclp->acl_cnt - 6]);

		if (!_acl_entry_matches(a1, ACL_USER_OBJ, 0,
		    ACL_ENTRY_TYPE_DENY))
			must_append = 1;
		if (!_acl_entry_matches(a2, ACL_USER_OBJ, ACL_WRITE_ACL |
		    ACL_WRITE_OWNER | ACL_WRITE_ATTRIBUTES |
		    ACL_WRITE_NAMED_ATTRS, ACL_ENTRY_TYPE_ALLOW))
			must_append = 1;
		if (!_acl_entry_matches(a3, ACL_GROUP_OBJ, 0,
		    ACL_ENTRY_TYPE_DENY))
			must_append = 1;
		if (!_acl_entry_matches(a4, ACL_GROUP_OBJ, 0,
		    ACL_ENTRY_TYPE_ALLOW))
			must_append = 1;
		if (!_acl_entry_matches(a5, ACL_EVERYONE, ACL_WRITE_ACL |
		    ACL_WRITE_OWNER | ACL_WRITE_ATTRIBUTES |
		    ACL_WRITE_NAMED_ATTRS, ACL_ENTRY_TYPE_DENY))
			must_append = 1;
		if (!_acl_entry_matches(a6, ACL_EVERYONE, ACL_READ_ACL |
		    ACL_READ_ATTRIBUTES | ACL_READ_NAMED_ATTRS |
		    ACL_SYNCHRONIZE, ACL_ENTRY_TYPE_ALLOW))
			must_append = 1;
	}

	if (must_append) {
		KASSERT(aclp->acl_cnt + 6 <= ACL_MAX_ENTRIES,
		    ("aclp->acl_cnt <= ACL_MAX_ENTRIES"));

		a1 = _acl_append(aclp, ACL_USER_OBJ, 0, ACL_ENTRY_TYPE_DENY);
		a2 = _acl_append(aclp, ACL_USER_OBJ, ACL_WRITE_ACL |
		    ACL_WRITE_OWNER | ACL_WRITE_ATTRIBUTES |
		    ACL_WRITE_NAMED_ATTRS, ACL_ENTRY_TYPE_ALLOW);
		a3 = _acl_append(aclp, ACL_GROUP_OBJ, 0, ACL_ENTRY_TYPE_DENY);
		a4 = _acl_append(aclp, ACL_GROUP_OBJ, 0, ACL_ENTRY_TYPE_ALLOW);
		a5 = _acl_append(aclp, ACL_EVERYONE, ACL_WRITE_ACL |
		    ACL_WRITE_OWNER | ACL_WRITE_ATTRIBUTES |
		    ACL_WRITE_NAMED_ATTRS, ACL_ENTRY_TYPE_DENY);
		a6 = _acl_append(aclp, ACL_EVERYONE, ACL_READ_ACL |
		    ACL_READ_ATTRIBUTES | ACL_READ_NAMED_ATTRS |
		    ACL_SYNCHRONIZE, ACL_ENTRY_TYPE_ALLOW);

		KASSERT(a1 != NULL && a2 != NULL && a3 != NULL && a4 != NULL &&
		    a5 != NULL && a6 != NULL, ("couldn't append to ACL."));
	}

	/*
	 * 3. The final six ACEs are adjusted according to the incoming mode.
	 */
	if (mode & S_IRUSR)
		a2->ae_perm |= ACL_READ_DATA;
	else
		a1->ae_perm |= ACL_READ_DATA;
	if (mode & S_IWUSR)
		a2->ae_perm |= (ACL_WRITE_DATA | ACL_APPEND_DATA);
	else
		a1->ae_perm |= (ACL_WRITE_DATA | ACL_APPEND_DATA);
	if (mode & S_IXUSR)
		a2->ae_perm |= ACL_EXECUTE;
	else
		a1->ae_perm |= ACL_EXECUTE;

	if (mode & S_IRGRP)
		a4->ae_perm |= ACL_READ_DATA;
	else
		a3->ae_perm |= ACL_READ_DATA;
	if (mode & S_IWGRP)
		a4->ae_perm |= (ACL_WRITE_DATA | ACL_APPEND_DATA);
	else
		a3->ae_perm |= (ACL_WRITE_DATA | ACL_APPEND_DATA);
	if (mode & S_IXGRP)
		a4->ae_perm |= ACL_EXECUTE;
	else
		a3->ae_perm |= ACL_EXECUTE;

	if (mode & S_IROTH)
		a6->ae_perm |= ACL_READ_DATA;
	else
		a5->ae_perm |= ACL_READ_DATA;
	if (mode & S_IWOTH)
		a6->ae_perm |= (ACL_WRITE_DATA | ACL_APPEND_DATA);
	else
		a5->ae_perm |= (ACL_WRITE_DATA | ACL_APPEND_DATA);
	if (mode & S_IXOTH)
		a6->ae_perm |= ACL_EXECUTE;
	else
		a5->ae_perm |= ACL_EXECUTE;
}

void
acl_nfs4_sync_mode_from_acl(mode_t *_mode, const struct acl *aclp)
{
	int i;
	mode_t old_mode = *_mode, mode = 0, seen = 0;
	const struct acl_entry *entry;

	KASSERT(aclp->acl_cnt > 0, ("aclp->acl_cnt > 0"));
	KASSERT(aclp->acl_cnt <= ACL_MAX_ENTRIES,
	    ("aclp->acl_cnt <= ACL_MAX_ENTRIES"));

	/*
	 * NFSv4 Minor Version 1, draft-ietf-nfsv4-minorversion1-03.txt
	 *
	 * 3.16.6.1. Recomputing mode upon SETATTR of ACL
	 */

	for (i = 0; i < aclp->acl_cnt; i++) {
		entry = &(aclp->acl_entry[i]);

		if (entry->ae_entry_type != ACL_ENTRY_TYPE_ALLOW &&
		    entry->ae_entry_type != ACL_ENTRY_TYPE_DENY)
			continue;

		if (entry->ae_flags & ACL_ENTRY_INHERIT_ONLY)
			continue;

		if (entry->ae_tag == ACL_USER_OBJ) {
			if ((entry->ae_perm & ACL_READ_DATA) &&
			    ((seen & S_IRUSR) == 0)) {
				seen |= S_IRUSR;
				if (entry->ae_entry_type == ACL_ENTRY_TYPE_ALLOW)
					mode |= S_IRUSR;
			}
			if ((entry->ae_perm & ACL_WRITE_DATA) &&
			     ((seen & S_IWUSR) == 0)) {
				seen |= S_IWUSR;
				if (entry->ae_entry_type == ACL_ENTRY_TYPE_ALLOW)
					mode |= S_IWUSR;
			}
			if ((entry->ae_perm & ACL_EXECUTE) &&
			    ((seen & S_IXUSR) == 0)) {
				seen |= S_IXUSR;
				if (entry->ae_entry_type == ACL_ENTRY_TYPE_ALLOW)
					mode |= S_IXUSR;
			}
		} else if (entry->ae_tag == ACL_GROUP_OBJ) {
			if ((entry->ae_perm & ACL_READ_DATA) &&
			    ((seen & S_IRGRP) == 0)) {
				seen |= S_IRGRP;
				if (entry->ae_entry_type == ACL_ENTRY_TYPE_ALLOW)
					mode |= S_IRGRP;
			}
			if ((entry->ae_perm & ACL_WRITE_DATA) &&
			    ((seen & S_IWGRP) == 0)) {
				seen |= S_IWGRP;
				if (entry->ae_entry_type == ACL_ENTRY_TYPE_ALLOW)
					mode |= S_IWGRP;
			}
			if ((entry->ae_perm & ACL_EXECUTE) &&
			    ((seen & S_IXGRP) == 0)) {
				seen |= S_IXGRP;
				if (entry->ae_entry_type == ACL_ENTRY_TYPE_ALLOW)
					mode |= S_IXGRP;
			}
		} else if (entry->ae_tag == ACL_EVERYONE) {
			if (entry->ae_perm & ACL_READ_DATA) {
				if ((seen & S_IRUSR) == 0) {
					seen |= S_IRUSR;
					if (entry->ae_entry_type == ACL_ENTRY_TYPE_ALLOW)
						mode |= S_IRUSR;
				}
				if ((seen & S_IRGRP) == 0) {
					seen |= S_IRGRP;
					if (entry->ae_entry_type == ACL_ENTRY_TYPE_ALLOW)
						mode |= S_IRGRP;
				}
				if ((seen & S_IROTH) == 0) {
					seen |= S_IROTH;
					if (entry->ae_entry_type == ACL_ENTRY_TYPE_ALLOW)
						mode |= S_IROTH;
				}
			}
			if (entry->ae_perm & ACL_WRITE_DATA) {
				if ((seen & S_IWUSR) == 0) {
					seen |= S_IWUSR;
					if (entry->ae_entry_type == ACL_ENTRY_TYPE_ALLOW)
						mode |= S_IWUSR;
				}
				if ((seen & S_IWGRP) == 0) {
					seen |= S_IWGRP;
					if (entry->ae_entry_type == ACL_ENTRY_TYPE_ALLOW)
						mode |= S_IWGRP;
				}
				if ((seen & S_IWOTH) == 0) {
					seen |= S_IWOTH;
					if (entry->ae_entry_type == ACL_ENTRY_TYPE_ALLOW)
						mode |= S_IWOTH;
				}
			}
			if (entry->ae_perm & ACL_EXECUTE) {
				if ((seen & S_IXUSR) == 0) {
					seen |= S_IXUSR;
					if (entry->ae_entry_type == ACL_ENTRY_TYPE_ALLOW)
						mode |= S_IXUSR;
				}
				if ((seen & S_IXGRP) == 0) {
					seen |= S_IXGRP;
					if (entry->ae_entry_type == ACL_ENTRY_TYPE_ALLOW)
						mode |= S_IXGRP;
				}
				if ((seen & S_IXOTH) == 0) {
					seen |= S_IXOTH;
					if (entry->ae_entry_type == ACL_ENTRY_TYPE_ALLOW)
						mode |= S_IXOTH;
				}
			}
		}
	}

	*_mode = mode | (old_mode & ACL_PRESERVE_MASK);
}
