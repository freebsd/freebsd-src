/*-
 * Copyright (c) 1999, 2000, 2001, 2002 Robert N. M. Watson
 * Copyright (c) 2001, 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by Network
 * Associates Laboratories, the Security Research Division of Network
 * Associates, Inc. under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"),
 * as part of the DARPA CHATS research program.
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
			    sizeof(*ruleptr), M_MACBSDEXTENDED,
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

		/*
		 * Since we don't separately handle append, map append to
		 * write.
		 */
		if (acc_mode & VAPPEND) {
			acc_mode &= ~VAPPEND;
			acc_mode |= VWRITE;
		}

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
    struct label *label, struct image_params *imgp,
    struct label *execlabel)
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

static struct mac_policy_ops mac_bsdextended_ops =
{
	.mpo_destroy = mac_bsdextended_destroy,
	.mpo_init = mac_bsdextended_init,
	.mpo_check_vnode_access = mac_bsdextended_check_vnode_access,
	.mpo_check_vnode_chdir = mac_bsdextended_check_vnode_chdir,
	.mpo_check_vnode_chroot = mac_bsdextended_check_vnode_chroot,
	.mpo_check_vnode_create = mac_bsdextended_check_create_vnode,
	.mpo_check_vnode_delete = mac_bsdextended_check_vnode_delete,
	.mpo_check_vnode_deleteacl = mac_bsdextended_check_vnode_deleteacl,
	.mpo_check_vnode_exec = mac_bsdextended_check_vnode_exec,
	.mpo_check_vnode_getacl = mac_bsdextended_check_vnode_getacl,
	.mpo_check_vnode_getextattr = mac_bsdextended_check_vnode_getextattr,
	.mpo_check_vnode_link = mac_bsdextended_check_vnode_link,
	.mpo_check_vnode_lookup = mac_bsdextended_check_vnode_lookup,
	.mpo_check_vnode_open = mac_bsdextended_check_vnode_open,
	.mpo_check_vnode_readdir = mac_bsdextended_check_vnode_readdir,
	.mpo_check_vnode_readlink = mac_bsdextended_check_vnode_readdlink,
	.mpo_check_vnode_rename_from = mac_bsdextended_check_vnode_rename_from,
	.mpo_check_vnode_rename_to = mac_bsdextended_check_vnode_rename_to,
	.mpo_check_vnode_revoke = mac_bsdextended_check_vnode_revoke,
	.mpo_check_vnode_setacl = mac_bsdextended_check_setacl_vnode,
	.mpo_check_vnode_setextattr = mac_bsdextended_check_vnode_setextattr,
	.mpo_check_vnode_setflags = mac_bsdextended_check_vnode_setflags,
	.mpo_check_vnode_setmode = mac_bsdextended_check_vnode_setmode,
	.mpo_check_vnode_setowner = mac_bsdextended_check_vnode_setowner,
	.mpo_check_vnode_setutimes = mac_bsdextended_check_vnode_setutimes,
	.mpo_check_vnode_stat = mac_bsdextended_check_vnode_stat,
};

MAC_POLICY_SET(&mac_bsdextended_ops, trustedbsd_mac_bsdextended,
    "TrustedBSD MAC/BSD Extended", MPC_LOADTIME_FLAG_UNLOADOK, NULL);
