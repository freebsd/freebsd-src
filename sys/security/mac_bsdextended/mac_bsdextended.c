/*-
 * Copyright (c) 1999, 2000, 2001, 2002 Robert N. M. Watson
 * Copyright (c) 2001, 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by NAI Labs,
 * the Security Research Division of Network Associates, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 * Developed by the TrustedBSD Project.
 * "BSD Extended" MAC policy, allowing the administrator to impose
 * mandatory rules regarding users and some system objects.
 *
 * XXX: Much locking support required here.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/acl.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <net/bpfdesc.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <vm/vm.h>

#include <sys/mac_policy.h>

#include <security/mac_bsdextended/mac_bsdextended.h>

SYSCTL_DECL(_security_mac);

SYSCTL_NODE(_security_mac, OID_AUTO, bsdextended, CTLFLAG_RW, 0,
    "TrustedBSD extended BSD MAC policy controls");

static int	mac_bsdextended_enabled = 1;
SYSCTL_INT(_security_mac_bsdextended, OID_AUTO, enabled, CTLFLAG_RW,
    &mac_bsdextended_enabled, 0, "Enforce extended BSD policy");
TUNABLE_INT("security.mac.bsdextended.enabled", &mac_bsdextended_enabled);

MALLOC_DEFINE(M_MACBSDEXTENDED, "mac_bsdextended", "BSD Extended MAC rule");

#define	MAC_BSDEXTENDED_MAXRULES	250
static struct mac_bsdextended_rule *rules[MAC_BSDEXTENDED_MAXRULES];
static int rule_count = 0;
static int rule_slots = 0;

SYSCTL_INT(_security_mac_bsdextended, OID_AUTO, rule_count, CTLFLAG_RD,
    &rule_count, 0, "Number of defined rules\n");
SYSCTL_INT(_security_mac_bsdextended, OID_AUTO, rule_slots, CTLFLAG_RD,
    &rule_slots, 0, "Number of used rule slots\n");

static int mac_bsdextended_debugging;
SYSCTL_INT(_security_mac_bsdextended, OID_AUTO, debugging, CTLFLAG_RW,
    &mac_bsdextended_debugging, 0, "Enable debugging on failure");

static int
mac_bsdextended_rule_valid(struct mac_bsdextended_rule *rule)
{

	if ((rule->mbr_subject.mbi_flags | MBI_BITS) != MBI_BITS)
		return (EINVAL);

	if ((rule->mbr_object.mbi_flags | MBI_BITS) != MBI_BITS)
		return (EINVAL);

	if ((rule->mbr_mode | VALLPERM) != VALLPERM)
		return (EINVAL);

	return (0);
}

static int
sysctl_rule(SYSCTL_HANDLER_ARGS)
{
	struct mac_bsdextended_rule temprule, *ruleptr;
	u_int namelen;
	int error, index, *name;

	name = (int *)arg1;
	namelen = arg2;

	/* printf("bsdextended sysctl handler (namelen %d)\n", namelen); */

	if (namelen != 1)
		return (EINVAL);

	index = name[0];
	if (index < 0 || index > rule_slots + 1)
		return (ENOENT);
	if (rule_slots >= MAC_BSDEXTENDED_MAXRULES)
		return (ENOENT);

	if (req->oldptr) {
		if (rules[index] == NULL)
			return (ENOENT);

		error = SYSCTL_OUT(req, rules[index], sizeof(*rules[index]));
		if (error)
			return (error);
	}

	if (req->newptr) {
		if (req->newlen == 0) {
			/* printf("deletion\n"); */
			ruleptr = rules[index];
			if (ruleptr == NULL)
				return (ENOENT);
			rule_count--;
			rules[index] = NULL;
			FREE(ruleptr, M_MACBSDEXTENDED);
			return(0);
		}
		error = SYSCTL_IN(req, &temprule, sizeof(temprule));
		if (error)
			return (error);

		error = mac_bsdextended_rule_valid(&temprule);
		if (error)
			return (error);

		if (rules[index] == NULL) {
			/* printf("addition\n"); */
			MALLOC(ruleptr, struct mac_bsdextended_rule *,
			    sizeof(*ruleptr), M_MACBSDEXTENDED, M_WAITOK |
			    M_ZERO);
			*ruleptr = temprule;
			rules[index] = ruleptr;
			if (index+1 > rule_slots)
				rule_slots = index+1;
			rule_count++;
		} else {
			/* printf("replacement\n"); */
			*rules[index] = temprule;
		}
	}

	return (0);
}

SYSCTL_NODE(_security_mac_bsdextended, OID_AUTO, rules,
    CTLFLAG_RW, sysctl_rule, "BSD extended MAC rules");

static void
mac_bsdextended_init(struct mac_policy_conf *mpc)
{

	/* Initialize ruleset lock. */
	/* Register dynamic sysctl's for rules. */
}

static void
mac_bsdextended_destroy(struct mac_policy_conf *mpc)
{

	/* Tear down sysctls. */
	/* Destroy ruleset lock. */
}

static int
mac_bsdextended_rulecheck(struct mac_bsdextended_rule *rule,
    struct ucred *cred, uid_t object_uid, gid_t object_gid, int acc_mode)
{
	int match;

	/*
	 * Is there a subject match?
	 */
	if (rule->mbr_subject.mbi_flags & MBI_UID_DEFINED) {
		match =  (rule->mbr_subject.mbi_uid == cred->cr_uid ||
		    rule->mbr_subject.mbi_uid == cred->cr_ruid ||
		    rule->mbr_subject.mbi_uid == cred->cr_svuid);

		if (rule->mbr_subject.mbi_flags & MBI_NEGATED)
			match = !match;

		if (!match)
			return (0);
	}

	if (rule->mbr_subject.mbi_flags & MBI_GID_DEFINED) {
		match = (groupmember(rule->mbr_subject.mbi_gid, cred) ||
		    rule->mbr_subject.mbi_gid == cred->cr_rgid ||
		    rule->mbr_subject.mbi_gid == cred->cr_svgid);

		if (rule->mbr_subject.mbi_flags & MBI_NEGATED)
			match = !match;

		if (!match)
			return (0);
	}

	/*
	 * Is there an object match?
	 */
	if (rule->mbr_object.mbi_flags & MBI_UID_DEFINED) {
		match = (rule->mbr_object.mbi_uid == object_uid);

		if (rule->mbr_object.mbi_flags & MBI_NEGATED)
			match = !match;

		if (!match)
			return (0);
	}

	if (rule->mbr_object.mbi_flags & MBI_GID_DEFINED) {
		match = (rule->mbr_object.mbi_gid == object_gid);

		if (rule->mbr_object.mbi_flags & MBI_NEGATED)
			match = !match;

		if (!match)
			return (0);
	}

	/*
	 * Is the access permitted?
	 */
	if ((rule->mbr_mode & acc_mode) != acc_mode) {
		if (mac_bsdextended_debugging)
			printf("mac_bsdextended: %d:%d request %d on %d:%d"
			    " fails\n", cred->cr_ruid, cred->cr_rgid,
			    acc_mode, object_uid, object_gid);
		return (EACCES);
	}

	return (0);
}

static int
mac_bsdextended_check(struct ucred *cred, uid_t object_uid, gid_t object_gid,
    int acc_mode)
{
	int error, i;

	for (i = 0; i < rule_slots; i++) {
		if (rules[i] == NULL)
			continue;

		error = mac_bsdextended_rulecheck(rules[i], cred, object_uid,
		    object_gid, acc_mode);
		if (error)
			return (error);
	}

	return (0);
}

static int
mac_bsdextended_check_vnode_access(struct ucred *cred, struct vnode *vp,
    struct label *label, int acc_mode)
{
	struct vattr vap;
	int error;

	if (!mac_bsdextended_enabled)
		return (0);

	error = VOP_GETATTR(vp, &vap, cred, curthread);
	if (error)
		return (error);
	return (mac_bsdextended_check(cred, vap.va_uid, vap.va_gid, acc_mode));
}

static int
mac_bsdextended_check_vnode_chdir(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel)
{
	struct vattr vap;
	int error;

	if (!mac_bsdextended_enabled)
		return (0);

	error = VOP_GETATTR(dvp, &vap, cred, curthread);
	if (error)
		return (error);
	return (mac_bsdextended_check(cred, vap.va_uid, vap.va_gid, VEXEC));
}

static int
mac_bsdextended_check_vnode_chroot(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel)
{
	struct vattr vap;
	int error;

	if (!mac_bsdextended_enabled)
		return (0);

	error = VOP_GETATTR(dvp, &vap, cred, curthread);
	if (error)
		return (error);
	return (mac_bsdextended_check(cred, vap.va_uid, vap.va_gid, VEXEC));
}

static int
mac_bsdextended_check_create_vnode(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct componentname *cnp, struct vattr *vap)
{
	struct vattr dvap;
	int error;

	if (!mac_bsdextended_enabled)
		return (0);

	error = VOP_GETATTR(dvp, &dvap, cred, curthread);
	if (error)
		return (error);
	return (mac_bsdextended_check(cred, dvap.va_uid, dvap.va_gid, VWRITE));
}

static int
mac_bsdextended_check_vnode_delete(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct vnode *vp, struct label *label,
    struct componentname *cnp)
{
	struct vattr vap;
	int error;

	if (!mac_bsdextended_enabled)
		return (0);

	error = VOP_GETATTR(dvp, &vap, cred, curthread);
	if (error)
		return (error);
	error = mac_bsdextended_check(cred, vap.va_uid, vap.va_gid, VWRITE);
	if (error)
		return (error);

	error = VOP_GETATTR(vp, &vap, cred, curthread);
	if (error)
		return (error);
	return (mac_bsdextended_check(cred, vap.va_uid, vap.va_gid, VWRITE));
}

static int
mac_bsdextended_check_vnode_deleteacl(struct ucred *cred, struct vnode *vp,
    struct label *label, acl_type_t type)
{
	struct vattr vap;
	int error;

	if (!mac_bsdextended_enabled)
		return (0);

	error = VOP_GETATTR(vp, &vap, cred, curthread);
	if (error) 
		return (error);
	return (mac_bsdextended_check(cred, vap.va_uid, vap.va_gid, VADMIN));
}

static int
mac_bsdextended_check_vnode_exec(struct ucred *cred, struct vnode *vp,
    struct label *label)
{
	struct vattr vap;
	int error;

	if (!mac_bsdextended_enabled)
		return (0);

	error = VOP_GETATTR(vp, &vap, cred, curthread);
	if (error)
		return (error);
	return (mac_bsdextended_check(cred, vap.va_uid, vap.va_gid,
	    VREAD|VEXEC));
}

static int
mac_bsdextended_check_vnode_getacl(struct ucred *cred, struct vnode *vp,
    struct label *label, acl_type_t type)
{
	struct vattr vap;
	int error;

	if (!mac_bsdextended_enabled)
		return (0);

	error = VOP_GETATTR(vp, &vap, cred, curthread);
	if (error)
		return (error);
	return (mac_bsdextended_check(cred, vap.va_uid, vap.va_gid, VSTAT));
}

static int
mac_bsdextended_check_vnode_getextattr(struct ucred *cred, struct vnode *vp,
    struct label *label, int attrnamespace, const char *name, struct uio *uio)
{
	struct vattr vap;
	int error;

	if (!mac_bsdextended_enabled)
		return (0);

	error = VOP_GETATTR(vp, &vap, cred, curthread);
	if (error)
		return (error);
	return (mac_bsdextended_check(cred, vap.va_uid, vap.va_gid, VREAD));
}

static int
mac_bsdextended_check_vnode_link(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct vnode *vp, struct label *label,
    struct componentname *cnp)
{
	struct vattr vap;
	int error;

	if (!mac_bsdextended_enabled)
		return (0);

	error = VOP_GETATTR(dvp, &vap, cred, curthread);
	if (error)
		return (error);
	error = mac_bsdextended_check(cred, vap.va_uid, vap.va_gid, VWRITE);
	if (error)
		return (error);

	error = VOP_GETATTR(vp, &vap, cred, curthread);
	if (error)
		return (error);
	error = mac_bsdextended_check(cred, vap.va_uid, vap.va_gid, VWRITE);
	if (error)
		return (error);
	return (0);
}

static int
mac_bsdextended_check_vnode_lookup(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct componentname *cnp)
{
	struct vattr vap;
	int error;
  
	if (!mac_bsdextended_enabled)
		return (0);
  
	error = VOP_GETATTR(dvp, &vap, cred, curthread);
	if (error)
		return (error);
	return (mac_bsdextended_check(cred, vap.va_uid, vap.va_gid, VEXEC));
}

static int
mac_bsdextended_check_vnode_open(struct ucred *cred, struct vnode *vp,
    struct label *filelabel, int acc_mode)
{
	struct vattr vap;
	int error;

	if (!mac_bsdextended_enabled)
		return (0);

	error = VOP_GETATTR(vp, &vap, cred, curthread);
	if (error)
		return (error);
	return (mac_bsdextended_check(cred, vap.va_uid, vap.va_gid, acc_mode));
}

static int
mac_bsdextended_check_vnode_readdir(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel)
{
	struct vattr vap;
	int error;

	if (!mac_bsdextended_enabled)
		return (0);

	error = VOP_GETATTR(dvp, &vap, cred, curthread);
	if (error)
		return (error);
	return (mac_bsdextended_check(cred, vap.va_uid, vap.va_gid, VREAD));
}

static int
mac_bsdextended_check_vnode_readdlink(struct ucred *cred, struct vnode *vp,
    struct label *label)
{
	struct vattr vap;
	int error;

	if (!mac_bsdextended_enabled)
		return (0);

	error = VOP_GETATTR(vp, &vap, cred, curthread);
	if (error)
		return (error);
	return (mac_bsdextended_check(cred, vap.va_uid, vap.va_gid, VREAD));
}

static int
mac_bsdextended_check_vnode_rename_from(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct vnode *vp, struct label *label,
    struct componentname *cnp)
{
	struct vattr vap;
	int error;

	if (!mac_bsdextended_enabled)
		return (0);

	error = VOP_GETATTR(dvp, &vap, cred, curthread);
	if (error)
		return (error);
	error = mac_bsdextended_check(cred, vap.va_uid, vap.va_gid, VWRITE);
	if (error)
		return (error);
	error = VOP_GETATTR(vp, &vap, cred, curthread);
	if (error)
		return (error);
	error = mac_bsdextended_check(cred, vap.va_uid, vap.va_gid, VWRITE);

	return (error);
}

static int
mac_bsdextended_check_vnode_rename_to(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct vnode *vp, struct label *label, int samedir,
    struct componentname *cnp)
{
	struct vattr vap;
	int error;

	if (!mac_bsdextended_enabled)
		return (0);

	error = VOP_GETATTR(dvp, &vap, cred, curthread);
	if (error)
		return (error);
	error = mac_bsdextended_check(cred, vap.va_uid, vap.va_gid, VWRITE);
	if (error)
		return (error);

	if (vp != NULL) {
		error = VOP_GETATTR(vp, &vap, cred, curthread);
		if (error)
			return (error);
		error = mac_bsdextended_check(cred, vap.va_uid, vap.va_gid,
		    VWRITE);
	}

	return (error);
}

static int
mac_bsdextended_check_vnode_revoke(struct ucred *cred, struct vnode *vp,
    struct label *label)
{
	struct vattr vap;
	int error;

	if (!mac_bsdextended_enabled)
		return (0);

	error = VOP_GETATTR(vp, &vap, cred, curthread);
	if (error)
		return (error);
	return (mac_bsdextended_check(cred, vap.va_uid, vap.va_gid, VADMIN));
}

static int
mac_bsdextended_check_setacl_vnode(struct ucred *cred, struct vnode *vp,
    struct label *label, acl_type_t type, struct acl *acl)
{
	struct vattr vap;
	int error;

	if (!mac_bsdextended_enabled)
		return (0);

	error = VOP_GETATTR(vp, &vap, cred, curthread);
	if (error)
		return (error);
	return (mac_bsdextended_check(cred, vap.va_uid, vap.va_gid, VADMIN));
}

static int
mac_bsdextended_check_vnode_setextattr(struct ucred *cred, struct vnode *vp,
    struct label *label, int attrnamespace, const char *name, struct uio *uio)
{
	struct vattr vap;
	int error;

	if (!mac_bsdextended_enabled)
		return (0);

	error = VOP_GETATTR(vp, &vap, cred, curthread);
	if (error)
		return (error);
	return (mac_bsdextended_check(cred, vap.va_uid, vap.va_gid, VWRITE));
}

static int
mac_bsdextended_check_vnode_setflags(struct ucred *cred, struct vnode *vp,
    struct label *label, u_long flags)
{
	struct vattr vap;
	int error;

	if (!mac_bsdextended_enabled)
		return (0);

	error = VOP_GETATTR(vp, &vap, cred, curthread);
	if (error)
		return (error);
	return (mac_bsdextended_check(cred, vap.va_uid, vap.va_gid, VADMIN));
}

static int
mac_bsdextended_check_vnode_setmode(struct ucred *cred, struct vnode *vp,
    struct label *label, mode_t mode)
{
	struct vattr vap;
	int error;

	if (!mac_bsdextended_enabled)
		return (0);

	error = VOP_GETATTR(vp, &vap, cred, curthread);
	if (error)
		return (error);
	return (mac_bsdextended_check(cred, vap.va_uid, vap.va_gid, VADMIN));
}

static int
mac_bsdextended_check_vnode_setowner(struct ucred *cred, struct vnode *vp,
    struct label *label, uid_t uid, gid_t gid)
{
	struct vattr vap;
	int error;

	if (!mac_bsdextended_enabled)
		return (0);

	error = VOP_GETATTR(vp, &vap, cred, curthread);
	if (error)
		return (error);
	return (mac_bsdextended_check(cred, vap.va_uid, vap.va_gid, VADMIN));
}

static int
mac_bsdextended_check_vnode_setutimes(struct ucred *cred, struct vnode *vp,
    struct label *label, struct timespec atime, struct timespec utime)
{
	struct vattr vap;
	int error;

	if (!mac_bsdextended_enabled)
		return (0);

	error = VOP_GETATTR(vp, &vap, cred, curthread);
	if (error)
		return (error);
	return (mac_bsdextended_check(cred, vap.va_uid, vap.va_gid, VADMIN));
}

static int
mac_bsdextended_check_vnode_stat(struct ucred *active_cred,
    struct ucred *file_cred, struct vnode *vp, struct label *label)
{
	struct vattr vap;
	int error;

	if (!mac_bsdextended_enabled)
		return (0);

	error = VOP_GETATTR(vp, &vap, active_cred, curthread);
	if (error)
		return (error);
	return (mac_bsdextended_check(active_cred, vap.va_uid, vap.va_gid,
	    VSTAT));
}

static struct mac_policy_op_entry mac_bsdextended_ops[] =
{
	{ MAC_DESTROY,
	    (macop_t)mac_bsdextended_destroy },
	{ MAC_INIT,
	    (macop_t)mac_bsdextended_init },
	{ MAC_CHECK_VNODE_ACCESS,
	    (macop_t)mac_bsdextended_check_vnode_access },
	{ MAC_CHECK_VNODE_CHDIR,
	    (macop_t)mac_bsdextended_check_vnode_chdir },
	{ MAC_CHECK_VNODE_CHROOT,
	    (macop_t)mac_bsdextended_check_vnode_chroot },
	{ MAC_CHECK_VNODE_CREATE,
	    (macop_t)mac_bsdextended_check_create_vnode },
	{ MAC_CHECK_VNODE_DELETE,
	    (macop_t)mac_bsdextended_check_vnode_delete },
	{ MAC_CHECK_VNODE_DELETEACL,
	    (macop_t)mac_bsdextended_check_vnode_deleteacl },
	{ MAC_CHECK_VNODE_EXEC,
	    (macop_t)mac_bsdextended_check_vnode_exec },
	{ MAC_CHECK_VNODE_GETACL,
	    (macop_t)mac_bsdextended_check_vnode_getacl },
	{ MAC_CHECK_VNODE_GETEXTATTR,
	    (macop_t)mac_bsdextended_check_vnode_getextattr },
	{ MAC_CHECK_VNODE_LINK,
	    (macop_t)mac_bsdextended_check_vnode_link },
	{ MAC_CHECK_VNODE_LOOKUP,
	    (macop_t)mac_bsdextended_check_vnode_lookup },
	{ MAC_CHECK_VNODE_OPEN,
	    (macop_t)mac_bsdextended_check_vnode_open },
	{ MAC_CHECK_VNODE_READDIR,
	    (macop_t)mac_bsdextended_check_vnode_readdir },
	{ MAC_CHECK_VNODE_READLINK,
	    (macop_t)mac_bsdextended_check_vnode_readdlink },
	{ MAC_CHECK_VNODE_RENAME_FROM,
	    (macop_t)mac_bsdextended_check_vnode_rename_from },
	{ MAC_CHECK_VNODE_RENAME_TO,
	    (macop_t)mac_bsdextended_check_vnode_rename_to },
	{ MAC_CHECK_VNODE_REVOKE,
	    (macop_t)mac_bsdextended_check_vnode_revoke },
	{ MAC_CHECK_VNODE_SETACL,
	    (macop_t)mac_bsdextended_check_setacl_vnode },
	{ MAC_CHECK_VNODE_SETEXTATTR,
	    (macop_t)mac_bsdextended_check_vnode_setextattr },
	{ MAC_CHECK_VNODE_SETFLAGS,
	    (macop_t)mac_bsdextended_check_vnode_setflags },
	{ MAC_CHECK_VNODE_SETMODE,
	    (macop_t)mac_bsdextended_check_vnode_setmode },
	{ MAC_CHECK_VNODE_SETOWNER,
	    (macop_t)mac_bsdextended_check_vnode_setowner },
	{ MAC_CHECK_VNODE_SETUTIMES,
	    (macop_t)mac_bsdextended_check_vnode_setutimes },
	{ MAC_CHECK_VNODE_STAT,
	    (macop_t)mac_bsdextended_check_vnode_stat },
	{ MAC_OP_LAST, NULL }
};

MAC_POLICY_SET(mac_bsdextended_ops, trustedbsd_mac_bsdextended,
    "TrustedBSD MAC/BSD Extended", MPC_LOADTIME_FLAG_UNLOADOK, NULL);
