/*
 * Copyright 2016 Chris Torek <torek@ixsystems.com>
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/acl.h>
#include <sys/stat.h>

#include "lib9p.h"
#include "lib9p_impl.h"
#include "genacl.h"
#include "fid.h"
#include "log.h"

typedef int econvertfn(acl_entry_t, struct l9p_ace *);

#ifndef __APPLE__
static struct l9p_acl *l9p_new_acl(uint32_t acetype, uint32_t aceasize);
static struct l9p_acl *l9p_growacl(struct l9p_acl *acl, uint32_t aceasize);
static int l9p_count_aces(acl_t sysacl);
static struct l9p_acl *l9p_sysacl_to_acl(int, acl_t, econvertfn *);
#endif
static bool l9p_ingroup(gid_t tid, gid_t gid, gid_t *gids, size_t ngids);
static int l9p_check_aces(int32_t mask, struct l9p_acl *acl, struct stat *st,
    uid_t uid, gid_t gid, gid_t *gids, size_t ngids);

void
l9p_acl_free(struct l9p_acl *acl)
{

	free(acl);
}

/*
 * Is the given group ID tid (test-id) any of the gid's in agids?
 */
static bool
l9p_ingroup(gid_t tid, gid_t gid, gid_t *gids, size_t ngids)
{
	size_t i;

	if (tid == gid)
		return (true);
	for (i = 0; i < ngids; i++)
		if (tid == gids[i])
			return (true);
	return (false);
}

/* #define ACE_DEBUG */

/*
 * Note that NFSv4 tests are done on a "first match" basis.
 * That is, we check each ACE sequentially until we run out
 * of ACEs, or find something explicitly denied (DENIED!),
 * or have cleared out all our attempt-something bits.  Once
 * we come across an ALLOW entry for the bits we're trying,
 * we clear those from the bits we're still looking for, in
 * the order they appear.
 *
 * The result is either "definitely allowed" (we cleared
 * all the bits), "definitely denied" (we hit a deny with
 * some or all of the bits), or "unspecified".  We
 * represent these three states as +1 (positive = yes = allow),
 * -1 (negative = no = denied), or 0 (no strong answer).
 *
 * For our caller's convenience, if we are called with a
 * mask of 0, we return 0 (no answer).
 */
static int
l9p_check_aces(int32_t mask, struct l9p_acl *acl, struct stat *st,
    uid_t uid, gid_t gid, gid_t *gids, size_t ngids)
{
	uint32_t i;
	struct l9p_ace *ace;
#ifdef ACE_DEBUG
	char *acetype, *allowdeny;
	bool show_tid;
#endif
	bool match;
	uid_t tid;

	if (mask == 0)
		return (0);

	for (i = 0; mask != 0 && i < acl->acl_nace; i++) {
		ace = &acl->acl_aces[i];
		switch (ace->ace_type) {
		case L9P_ACET_ACCESS_ALLOWED:
		case L9P_ACET_ACCESS_DENIED:
			break;
		default:
			/* audit, alarm - ignore */
			continue;
		}
#ifdef ACE_DEBUG
		show_tid = false;
#endif
		if (ace->ace_flags & L9P_ACEF_OWNER) {
#ifdef ACE_DEBUG
			acetype = "OWNER@";
#endif
			match = st->st_uid == uid;
		} else if (ace->ace_flags & L9P_ACEF_GROUP) {
#ifdef ACE_DEBUG
			acetype = "GROUP@";
#endif
			match = l9p_ingroup(st->st_gid, gid, gids, ngids);
		} else if (ace->ace_flags & L9P_ACEF_EVERYONE) {
#ifdef ACE_DEBUG
			acetype = "EVERYONE@";
#endif
			match = true;
		} else {
			if (ace->ace_idsize != sizeof(tid))
				continue;
#ifdef ACE_DEBUG
			show_tid = true;
#endif
			memcpy(&tid, &ace->ace_idbytes, sizeof(tid));
			if (ace->ace_flags & L9P_ACEF_IDENTIFIER_GROUP) {
#ifdef ACE_DEBUG
				acetype = "group";
#endif
				match = l9p_ingroup(tid, gid, gids, ngids);
			} else {
#ifdef ACE_DEBUG
				acetype = "user";
#endif
				match = tid == uid;
			}
		}
		/*
		 * If this ACE applies to us, check remaining bits.
		 * If any of those bits also apply, check the type:
		 * DENY means "stop now", ALLOW means allow these bits
		 * and keep checking.
		 */
#ifdef ACE_DEBUG
		allowdeny = ace->ace_type == L9P_ACET_ACCESS_DENIED ?
		    "deny" : "allow";
#endif
		if (match && (ace->ace_mask & (uint32_t)mask) != 0) {
#ifdef ACE_DEBUG
			if (show_tid)
				L9P_LOG(L9P_DEBUG,
				    "ACE: %s %s %d: mask 0x%x ace_mask 0x%x",
				    allowdeny, acetype, (int)tid,
				    (u_int)mask, (u_int)ace->ace_mask);
			else
				L9P_LOG(L9P_DEBUG,
				    "ACE: %s %s: mask 0x%x ace_mask 0x%x",
				    allowdeny, acetype,
				    (u_int)mask, (u_int)ace->ace_mask);
#endif
			if (ace->ace_type == L9P_ACET_ACCESS_DENIED)
				return (-1);
			mask &= ~ace->ace_mask;
#ifdef ACE_DEBUG
			L9P_LOG(L9P_DEBUG, "clear 0x%x: now mask=0x%x",
			    (u_int)ace->ace_mask, (u_int)mask);
#endif
		} else {
#ifdef ACE_DEBUG
			if (show_tid)
				L9P_LOG(L9P_DEBUG,
				    "ACE: SKIP %s %s %d: "
				    "match %d mask 0x%x ace_mask 0x%x",
				    allowdeny, acetype, (int)tid,
				    (int)match, (u_int)mask,
				    (u_int)ace->ace_mask);
			else
				L9P_LOG(L9P_DEBUG,
				    "ACE: SKIP %s %s: "
				    "match %d mask 0x%x ace_mask 0x%x",
				    allowdeny, acetype,
				    (int)match, (u_int)mask,
				    (u_int)ace->ace_mask);
#endif
		}
	}

	/* Return 1 if access definitely granted. */
#ifdef ACE_DEBUG
	L9P_LOG(L9P_DEBUG, "ACE: end of ACEs, mask now 0x%x: %s",
	    mask, mask ? "no-definitive-answer" : "ALLOW");
#endif
	return (mask == 0 ? 1 : 0);
}

/*
 * Test against ACLs.
 *
 * The return value is normally 0 (access allowed) or EPERM
 * (access denied), so it could just be a boolean....
 *
 * For "make new dir in dir" and "remove dir in dir", you must
 * set the mask to test the directory permissions (not ADD_FILE but
 * ADD_SUBDIRECTORY, and DELETE_CHILD).  For "make new file in dir"
 * you must set the opmask to test file ADD_FILE.
 *
 * The L9P_ACE_DELETE flag means "can delete this thing"; it's not
 * clear whether it should override the parent directory's ACL if
 * any.  In our case it does not, but a caller may try
 * L9P_ACE_DELETE_CHILD (separately, on its own) and then a
 * (second, separate) L9P_ACE_DELETE, to make the permissions work
 * as "or" instead of "and".
 *
 * Pass a NULL parent/pstat if they are not applicable, e.g.,
 * for doing operations on an existing file, such as reading or
 * writing data or attributes.  Pass in a null child/cstat if
 * that's not applicable, such as creating a new file/dir.
 *
 * NB: it's probably wise to allow the owner of any file to update
 * the ACLs of that file, but we leave that test to the caller.
 */
int l9p_acl_check_access(int32_t opmask, struct l9p_acl_check_args *args)
{
	struct l9p_acl *parent, *child;
	struct stat *pstat, *cstat;
	int32_t pop, cop;
	size_t ngids;
	uid_t uid;
	gid_t gid, *gids;
	int panswer, canswer;

	assert(opmask != 0);
	parent = args->aca_parent;
	pstat = args->aca_pstat;
	child = args->aca_child;
	cstat = args->aca_cstat;
	uid = args->aca_uid;
	gid = args->aca_gid;
	gids = args->aca_groups;
	ngids = args->aca_ngroups;

#ifdef ACE_DEBUG
	L9P_LOG(L9P_DEBUG,
	    "l9p_acl_check_access: opmask=0x%x uid=%ld gid=%ld ngids=%zd",
	    (u_int)opmask, (long)uid, (long)gid, ngids);
#endif
	/*
	 * If caller said "superuser semantics", check that first.
	 * Note that we apply them regardless of ACLs.
	 */
	if (uid == 0 && args->aca_superuser)
		return (0);

	/*
	 * If told to ignore ACLs and use only stat-based permissions,
	 * discard any non-NULL ACL pointers.
	 *
	 * This will need some fancying up when we support POSIX ACLs.
	 */
	if ((args->aca_aclmode & L9P_ACM_NFS_ACL) == 0)
		parent = child = NULL;

	assert(parent == NULL || parent->acl_acetype == L9P_ACLTYPE_NFSv4);
	assert(parent == NULL || pstat != NULL);
	assert(child == NULL || child->acl_acetype == L9P_ACLTYPE_NFSv4);
	assert(child == NULL || cstat != NULL);
	assert(pstat != NULL || cstat != NULL);

	/*
	 * If the operation is UNLINK we should have either both ACLs
	 * or no ACLs, but we won't require that here.
	 *
	 * If a parent ACL is supplied, it's a directory by definition.
	 * Make sure we're allowed to do this there, whatever this is.
	 * If a child ACL is supplied, check it too.  Note that the
	 * DELETE permission only applies in the child though, not
	 * in the parent, and the DELETE_CHILD only applies in the
	 * parent.
	 */
	pop = cop = opmask;
	if (parent != NULL || pstat != NULL) {
		/*
		 * Remove child-only bits from parent op and
		 * parent-only bits from child op.
		 *
		 * L9P_ACE_DELETE is child-only.
		 *
		 * L9P_ACE_DELETE_CHILD is parent-only, and three data
		 * access bits overlap with three directory access bits.
		 * We should have child==NULL && cstat==NULL, so the
		 * three data bits should be redundant, but it's
		 * both trivial and safest to remove them anyway.
		 */
		pop &= ~L9P_ACE_DELETE;
		cop &= ~(L9P_ACE_DELETE_CHILD | L9P_ACE_LIST_DIRECTORY |
		    L9P_ACE_ADD_FILE | L9P_ACE_ADD_SUBDIRECTORY);
	} else {
		/*
		 * Remove child-only bits from parent op.  We need
		 * not bother since we just found we have no parent
		 * and no pstat, and hence won't actually *use* pop.
		 *
		 * pop &= ~(L9P_ACE_READ_DATA | L9P_ACE_WRITE_DATA |
		 *     L9P_ACE_APPEND_DATA);
		 */
	}
	panswer = 0;
	canswer = 0;
	if (parent != NULL)
		panswer = l9p_check_aces(pop, parent, pstat,
		    uid, gid, gids, ngids);
	if (child != NULL)
		canswer = l9p_check_aces(cop, child, cstat,
		    uid, gid, gids, ngids);

	if (panswer || canswer) {
		/*
		 * Got a definitive answer from parent and/or
		 * child ACLs.  We're not quite done yet though.
		 */
		if (opmask == L9P_ACOP_UNLINK) {
			/*
			 * For UNLINK, we can get an allow from child
			 * and deny from parent, or vice versa.  It's
			 * not 100% clear how to handle the two-answer
			 * case.  ZFS says that if either says "allow",
			 * we allow, and if both definitely say "deny",
			 * we deny.  This makes sense, so we do that
			 * here for all cases, even "strict".
			 */
			if (panswer > 0 || canswer > 0)
				return (0);
			if (panswer < 0 && canswer < 0)
				return (EPERM);
			/* non-definitive answer from one! move on */
		} else {
			/*
			 * Have at least one definitive answer, and
			 * should have only one; obey whichever
			 * one it is.
			 */
			if (panswer)
				return (panswer < 0 ? EPERM : 0);
			return (canswer < 0 ? EPERM : 0);
		}
	}

	/*
	 * No definitive answer from ACLs alone.  Check for ZFS style
	 * permissions checking and an "UNLINK" operation under ACLs.
	 * If so, find write-and-execute permission on parent.
	 * Note that WRITE overlaps with ADD_FILE -- that's ZFS's
	 * way of saying "allow write to dir" -- but EXECUTE is
	 * separate from LIST_DIRECTORY, so that's at least a little
	 * bit cleaner.
	 *
	 * Note also that only a definitive yes (both bits are
	 * explicitly allowed) results in granting unlink, and
	 * a definitive no (at least one bit explicitly denied)
	 * results in EPERM.  Only "no answer" moves on.
	 */
	if ((args->aca_aclmode & L9P_ACM_ZFS_ACL) &&
	    opmask == L9P_ACOP_UNLINK && parent != NULL) {
		panswer = l9p_check_aces(L9P_ACE_ADD_FILE | L9P_ACE_EXECUTE,
		    parent, pstat, uid, gid, gids, ngids);
		if (panswer)
			return (panswer < 0 ? EPERM : 0);
	}

	/*
	 * No definitive answer from ACLs.
	 *
	 * Try POSIX style rwx permissions if allowed.  This should
	 * be rare, occurring mainly when caller supplied no ACLs
	 * or set the mode to suppress them.
	 *
	 * The stat to check is the parent's if we don't have a child
	 * (i.e., this is a dir op), or if the DELETE_CHILD bit is set
	 * (i.e., this is an unlink or similar).  Otherwise it's the
	 * child's.
	 */
	if (args->aca_aclmode & L9P_ACM_STAT_MODE) {
		struct stat *st;
		int rwx, bits;

		rwx = l9p_ace_mask_to_rwx(opmask);
		if ((st = cstat) == NULL || (opmask & L9P_ACE_DELETE_CHILD))
			st = pstat;
		if (uid == st->st_uid)
			bits = (st->st_mode >> 6) & 7;
		else if (l9p_ingroup(st->st_gid, gid, gids, ngids))
			bits = (st->st_mode >> 3) & 7;
		else
			bits = st->st_mode & 7;
		/*
		 * If all the desired bits are set, we're OK.
		 */
		if ((rwx & bits) == rwx)
			return (0);
	}

	/* all methods have failed, return EPERM */
	return (EPERM);
}

/*
 * Collapse fancy ACL operation mask down to simple Unix bits.
 *
 * Directory operations don't map that well.  However, listing
 * a directory really does require read permission, and adding
 * or deleting files really does require write permission, so
 * this is probably sufficient.
 */
int
l9p_ace_mask_to_rwx(int32_t opmask)
{
	int rwx = 0;

	if (opmask &
	    (L9P_ACE_READ_DATA | L9P_ACE_READ_NAMED_ATTRS |
	     L9P_ACE_READ_ATTRIBUTES | L9P_ACE_READ_ACL))
		rwx |= 4;
	if (opmask &
	    (L9P_ACE_WRITE_DATA | L9P_ACE_APPEND_DATA |
	     L9P_ACE_ADD_FILE | L9P_ACE_ADD_SUBDIRECTORY |
	     L9P_ACE_DELETE | L9P_ACE_DELETE_CHILD |
	     L9P_ACE_WRITE_NAMED_ATTRS | L9P_ACE_WRITE_ATTRIBUTES |
	     L9P_ACE_WRITE_ACL))
		rwx |= 2;
	if (opmask & L9P_ACE_EXECUTE)
		rwx |= 1;
	return (rwx);
}

#ifndef __APPLE__
/*
 * Allocate new ACL holder and ACEs.
 */
static struct l9p_acl *
l9p_new_acl(uint32_t acetype, uint32_t aceasize)
{
	struct l9p_acl *ret;
	size_t asize, size;

	asize = aceasize * sizeof(struct l9p_ace);
	size = sizeof(struct l9p_acl) + asize;
	ret = malloc(size);
	if (ret != NULL) {
		ret->acl_acetype = acetype;
		ret->acl_nace = 0;
		ret->acl_aceasize = aceasize;
	}
	return (ret);
}

/*
 * Expand ACL to accomodate more entries.
 *
 * Currently won't shrink, only grow, so it's a fast no-op until
 * we hit the allocated size.  After that, it's best to grow in
 * big chunks, or this will be O(n**2).
 */
static struct l9p_acl *
l9p_growacl(struct l9p_acl *acl, uint32_t aceasize)
{
	struct l9p_acl *tmp;
	size_t asize, size;

	if (acl->acl_aceasize < aceasize) {
		asize = aceasize * sizeof(struct l9p_ace);
		size = sizeof(struct l9p_acl) + asize;
		tmp = realloc(acl, size);
		if (tmp == NULL)
			free(acl);
		acl = tmp;
	}
	return (acl);
}

/*
 * Annoyingly, there's no POSIX-standard way to count the number
 * of ACEs in a system ACL other than to walk through them all.
 * This is silly, but at least 2n is still O(n), and the walk is
 * short.  (If the system ACL mysteriously grows, we'll handle
 * that OK via growacl(), too.)
 */
static int
l9p_count_aces(acl_t sysacl)
{
	acl_entry_t entry;
	uint32_t n;
	int id;

	id = ACL_FIRST_ENTRY;
	for (n = 0; acl_get_entry(sysacl, id, &entry) == 1; n++)
		id = ACL_NEXT_ENTRY;

	return ((int)n);
}

/*
 * Create ACL with ACEs from the given acl_t.  We use the given
 * convert function on each ACE.
 */
static struct l9p_acl *
l9p_sysacl_to_acl(int acetype, acl_t sysacl, econvertfn *convert)
{
	struct l9p_acl *acl;
	acl_entry_t entry;
	uint32_t n;
	int error, id;

	acl = l9p_new_acl((uint32_t)acetype, (uint32_t)l9p_count_aces(sysacl));
	if (acl == NULL)
		return (NULL);
	id = ACL_FIRST_ENTRY;
	for (n = 0;;) {
		if (acl_get_entry(sysacl, id, &entry) != 1)
			break;
		acl = l9p_growacl(acl, n + 1);
		if (acl == NULL)
			return (NULL);
		error = (*convert)(entry, &acl->acl_aces[n]);
		id = ACL_NEXT_ENTRY;
		if (error == 0)
			n++;
	}
	acl->acl_nace = n;
	return (acl);
}
#endif

#if defined(HAVE_POSIX_ACLS) && 0 /* not yet */
struct l9p_acl *
l9p_posix_acl_to_acl(acl_t sysacl)
{
}
#endif

#if defined(HAVE_FREEBSD_ACLS)
int
l9p_frombsdnfs4(acl_entry_t sysace, struct l9p_ace *ace)
{
	acl_tag_t tag;			/* e.g., USER_OBJ, GROUP, etc */
	acl_entry_type_t entry_type;	/* e.g., allow/deny */
	acl_permset_t absdperm;
	acl_flagset_t absdflag;
	acl_perm_t bsdperm;		/* e.g., READ_DATA */
	acl_flag_t bsdflag;		/* e.g., FILE_INHERIT_ACE */
	uint32_t flags, mask;
	int error;
	uid_t uid, *aid;

	error = acl_get_tag_type(sysace, &tag);
	if (error == 0)
		error = acl_get_entry_type_np(sysace, &entry_type);
	if (error == 0)
		error = acl_get_flagset_np(sysace, &absdflag);
	if (error == 0)
		error = acl_get_permset(sysace, &absdperm);
	if (error)
		return (error);

	flags = 0;
	uid = 0;
	aid = NULL;

	/* move user/group/everyone + id-is-group-id into flags */
	switch (tag) {
	case ACL_USER_OBJ:
		flags |= L9P_ACEF_OWNER;
		break;
	case ACL_GROUP_OBJ:
		flags |= L9P_ACEF_GROUP;
		break;
	case ACL_EVERYONE:
		flags |= L9P_ACEF_EVERYONE;
		break;
	case ACL_GROUP:
		flags |= L9P_ACEF_IDENTIFIER_GROUP;
		/* FALLTHROUGH */
	case ACL_USER:
		aid = acl_get_qualifier(sysace); /* ugh, this malloc()s */
		if (aid == NULL)
			return (ENOMEM);
		uid = *(uid_t *)aid;
		free(aid);
		aid = &uid;
		break;
	default:
		return (EINVAL);	/* can't happen */
	}

	switch (entry_type) {

	case ACL_ENTRY_TYPE_ALLOW:
		ace->ace_type = L9P_ACET_ACCESS_ALLOWED;
		break;

	case ACL_ENTRY_TYPE_DENY:
		ace->ace_type = L9P_ACET_ACCESS_DENIED;
		break;

	case ACL_ENTRY_TYPE_AUDIT:
		ace->ace_type = L9P_ACET_SYSTEM_AUDIT;
		break;

	case ACL_ENTRY_TYPE_ALARM:
		ace->ace_type = L9P_ACET_SYSTEM_ALARM;
		break;

	default:
		return (EINVAL);	/* can't happen */
	}

	/* transform remaining BSD flags to internal NFS-y form */
	bsdflag = *absdflag;
	if (bsdflag & ACL_ENTRY_FILE_INHERIT)
		flags |= L9P_ACEF_FILE_INHERIT_ACE;
	if (bsdflag & ACL_ENTRY_DIRECTORY_INHERIT)
		flags |= L9P_ACEF_DIRECTORY_INHERIT_ACE;
	if (bsdflag & ACL_ENTRY_NO_PROPAGATE_INHERIT)
		flags |= L9P_ACEF_NO_PROPAGATE_INHERIT_ACE;
	if (bsdflag & ACL_ENTRY_INHERIT_ONLY)
		flags |= L9P_ACEF_INHERIT_ONLY_ACE;
	if (bsdflag & ACL_ENTRY_SUCCESSFUL_ACCESS)
		flags |= L9P_ACEF_SUCCESSFUL_ACCESS_ACE_FLAG;
	if (bsdflag & ACL_ENTRY_FAILED_ACCESS)
		flags |= L9P_ACEF_FAILED_ACCESS_ACE_FLAG;
	ace->ace_flags = flags;

	/*
	 * Transform BSD permissions to ace_mask.  Note that directory
	 * vs file bits are the same in both sets, so we don't need
	 * to worry about that, at least.
	 *
	 * There seem to be no BSD equivalents for WRITE_RETENTION
	 * and WRITE_RETENTION_HOLD.
	 */
	mask = 0;
	bsdperm = *absdperm;
	if (bsdperm & ACL_READ_DATA)
		mask |= L9P_ACE_READ_DATA;
	if (bsdperm & ACL_WRITE_DATA)
		mask |= L9P_ACE_WRITE_DATA;
	if (bsdperm & ACL_APPEND_DATA)
		mask |= L9P_ACE_APPEND_DATA;
	if (bsdperm & ACL_READ_NAMED_ATTRS)
		mask |= L9P_ACE_READ_NAMED_ATTRS;
	if (bsdperm & ACL_WRITE_NAMED_ATTRS)
		mask |= L9P_ACE_WRITE_NAMED_ATTRS;
	if (bsdperm & ACL_EXECUTE)
		mask |= L9P_ACE_EXECUTE;
	if (bsdperm & ACL_DELETE_CHILD)
		mask |= L9P_ACE_DELETE_CHILD;
	if (bsdperm & ACL_READ_ATTRIBUTES)
		mask |= L9P_ACE_READ_ATTRIBUTES;
	if (bsdperm & ACL_WRITE_ATTRIBUTES)
		mask |= L9P_ACE_WRITE_ATTRIBUTES;
	/* L9P_ACE_WRITE_RETENTION */
	/* L9P_ACE_WRITE_RETENTION_HOLD */
	/* 0x00800 */
	if (bsdperm & ACL_DELETE)
		mask |= L9P_ACE_DELETE;
	if (bsdperm & ACL_READ_ACL)
		mask |= L9P_ACE_READ_ACL;
	if (bsdperm & ACL_WRITE_ACL)
		mask |= L9P_ACE_WRITE_ACL;
	if (bsdperm & ACL_WRITE_OWNER)
		mask |= L9P_ACE_WRITE_OWNER;
	if (bsdperm & ACL_SYNCHRONIZE)
		mask |= L9P_ACE_SYNCHRONIZE;
	ace->ace_mask = mask;

	/* fill in variable-size user or group ID bytes */
	if (aid == NULL)
		ace->ace_idsize = 0;
	else {
		ace->ace_idsize = sizeof(uid);
		memcpy(&ace->ace_idbytes[0], aid, sizeof(uid));
	}

	return (0);
}

struct l9p_acl *
l9p_freebsd_nfsv4acl_to_acl(acl_t sysacl)
{

	return (l9p_sysacl_to_acl(L9P_ACLTYPE_NFSv4, sysacl, l9p_frombsdnfs4));
}
#endif

#if defined(HAVE_DARWIN_ACLS) && 0 /* not yet */
struct l9p_acl *
l9p_darwin_nfsv4acl_to_acl(acl_t sysacl)
{
}
#endif
