/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2023, Juniper Networks, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include "opt_mac.h"

#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mac.h>
#include <sys/namei.h>
#include <sys/priv.h>
#include <sys/imgact.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <security/mac/mac_policy.h>

#include "mac_grantbylabel.h"
#include <security/mac_veriexec/mac_veriexec_internal.h>

#define MAC_GRANTBYLABEL_FULLNAME   "MAC/grantbylabel"

SYSCTL_DECL(_security_mac);
SYSCTL_NODE(_security_mac, OID_AUTO, grantbylabel, CTLFLAG_RW, 0,
    "MAC/grantbylabel policy controls");

#ifdef MAC_DEBUG
static int mac_grantbylabel_debug;

SYSCTL_INT(_security_mac_grantbylabel, OID_AUTO, debug, CTLFLAG_RW,
    &mac_grantbylabel_debug, 0, "Debug mac_grantbylabel");

#define GRANTBYLABEL_DEBUG(n, x) if (mac_grantbylabel_debug >= (n)) printf x

#define	MAC_GRANTBYLABEL_DBG(_lvl, _fmt, ...)				\
	do {								\
		GRANTBYLABEL_DEBUG((_lvl), (MAC_GRANTBYLABEL_FULLNAME ": " \
			_fmt "\n", ##__VA_ARGS__));			\
	} while(0)
#else
#define	MAC_GRANTBYLABEL_DBG(_lvl, _fmt, ...)
#endif


/* label token prefix */
#define GBL_PREFIX "gbl/"

static int mac_grantbylabel_slot;

#define	SLOT(l) \
	mac_label_get((l), mac_grantbylabel_slot)
#define	SLOT_SET(l, v) \
	mac_label_set((l), mac_grantbylabel_slot, (v))


/**
 * @brief parse label into bitmask
 *
 * We are only interested in tokens prefixed by GBL_PREFIX ("gbl/").
 *
 * @return 32bit mask
 */
static gbl_label_t
gbl_parse_label(const char *label)
{
	gbl_label_t gbl;
	char *cp;

	if (!(label && *label))
		return GBL_EMPTY;
	gbl = 0;
	for (cp = strstr(label, GBL_PREFIX); cp; cp = strstr(cp, GBL_PREFIX)) {
		/* check we didn't find "fugbl/" */
		if (cp > label && cp[-1] != ',') {
			cp += sizeof(GBL_PREFIX);
			continue;
		}
		cp += sizeof(GBL_PREFIX) - 1;
		switch (*cp) {
		case 'b':
			if (strncmp(cp, "bind", 4) == 0)
				gbl |= GBL_BIND;
			break;
		case 'd':
			if (strncmp(cp, "daemon", 6) == 0)
				gbl |= (GBL_BIND|GBL_IPC|GBL_NET|GBL_PROC|
				    GBL_SYSCTL|GBL_VACCESS);
			break;
		case 'i':
			if (strncmp(cp, "ipc", 3) == 0)
				gbl |= GBL_IPC;
			break;
		case 'k':
			if (strncmp(cp, "kmem", 4) == 0)
				gbl |= GBL_KMEM;
			break;
		case 'n':
			if (strncmp(cp, "net", 3) == 0)
				gbl |= GBL_NET;
			break;
		case 'p':
			if (strncmp(cp, "proc", 4) == 0)
				gbl |= GBL_PROC;
			break;
		case 'r':
			if (strncmp(cp, "rtsock", 6) == 0)
				gbl |= GBL_RTSOCK;
			break;
		case 's':
			if (strncmp(cp, "sysctl", 6) == 0)
				gbl |= GBL_SYSCTL;
			break;
		case 'v':
			if (strncmp(cp, "vaccess", 7) == 0)
				gbl |= GBL_VACCESS;
			else if (strncmp(cp, "veriexec", 8) == 0)
				gbl |= GBL_VERIEXEC;
			break;
		default:		/* ignore unknown? */
			MAC_GRANTBYLABEL_DBG(1,
			    "ignoring unknown token at %s/%s",
			    GBL_PREFIX, cp);
			break;
		}
	}

	return gbl;
}


/**
 * @brief get the v_label for a vnode
 *
 * Lookup the label if not already set in v_label
 *
 * @return 32bit mask or 0 on error
 */
static gbl_label_t
gbl_get_vlabel(struct vnode *vp, struct ucred *cred)
{
	struct vattr va;
	const char *label;
	gbl_label_t gbl;
	int error;

	gbl = SLOT(vp->v_label);
	if (gbl == 0) {
		error = VOP_GETATTR(vp, &va, cred);
		if (error == 0) {
			label = mac_veriexec_metadata_get_file_label(va.va_fsid,
			    va.va_fileid, va.va_gen, FALSE);
			if (label) {
				MAC_GRANTBYLABEL_DBG(1,
				    "label=%s dev=%ju, file %ju.%lu",
				    label,
				    (uintmax_t)va.va_fsid,
				    (uintmax_t)va.va_fileid,
				    va.va_gen);
				gbl = gbl_parse_label(label);
			} else {
				gbl = GBL_EMPTY;
				MAC_GRANTBYLABEL_DBG(2, "no label dev=%ju, file %ju.%lu",
				    (uintmax_t)va.va_fsid,
				    (uintmax_t)va.va_fileid,
				    va.va_gen);
			}
		}
	}
	return gbl;
}


/**
 * @brief grant priv if warranted
 *
 * If the cred is root, we have nothing to do.
 * Otherwise see if the current process has a label
 * that grants it the requested priv.
 */
static int
mac_grantbylabel_priv_grant(struct ucred *cred, int priv)
{
	gbl_label_t label;
	int rc;

	rc = EPERM;			/* default response */

	if ((curproc->p_flag & (P_KPROC|P_SYSTEM)))
		return rc;		/* not interested */

	switch (priv) {
	case PRIV_PROC_MEM_WRITE:
	case PRIV_KMEM_READ:
	case PRIV_KMEM_WRITE:
		break;
	case PRIV_VERIEXEC_DIRECT:
	case PRIV_VERIEXEC_NOVERIFY:
		/* XXX might want to skip in FIPS mode */
		break;
	default:
		if (cred->cr_uid == 0)
			return rc;	/* not interested */
		break;
	}

	label = (gbl_label_t)(SLOT(curproc->p_textvp->v_label) |
	    SLOT(curproc->p_label));

	/*
	 * We look at the extra privs granted
	 * via process label.
	 */
	switch (priv) {
	case PRIV_IPC_READ:
	case PRIV_IPC_WRITE:
		if (label & GBL_IPC)
			rc = 0;
		break;
	case PRIV_PROC_MEM_WRITE:
	case PRIV_KMEM_READ:
	case PRIV_KMEM_WRITE:
		if (label & GBL_KMEM)
			rc = 0;
		break;
	case PRIV_NETINET_BINDANY:
	case PRIV_NETINET_RESERVEDPORT:	/* socket bind low port */
	case PRIV_NETINET_REUSEPORT:
		if (label & GBL_BIND)
			rc = 0;
		break;
	case PRIV_NETINET_ADDRCTRL6:
	case PRIV_NET_LAGG:
	case PRIV_NET_SETIFFIB:
	case PRIV_NET_SETIFVNET:
	case PRIV_NETINET_SETHDROPTS:
	case PRIV_NET_VXLAN:
	case PRIV_NETINET_GETCRED:
	case PRIV_NETINET_IPSEC:
	case PRIV_NETINET_RAW:
		if (label & GBL_NET)
			rc = 0;
		break;
	case PRIV_NETINET_MROUTE:
	case PRIV_NET_ROUTE:
		if (label & GBL_RTSOCK)
			rc = 0;
		break;
	case PRIV_PROC_LIMIT:
	case PRIV_PROC_SETRLIMIT:
		if (label & GBL_PROC)
			rc = 0;
		break;
	case PRIV_SYSCTL_WRITE:
		if (label & GBL_SYSCTL)
			rc = 0;
		break;
	case PRIV_VFS_READ:
	case PRIV_VFS_WRITE:
		if (label & GBL_KMEM)
			rc = 0;
		/* FALLTHROUGH */
	case PRIV_VFS_ADMIN:
	case PRIV_VFS_BLOCKRESERVE:
	case PRIV_VFS_CHOWN:
	case PRIV_VFS_EXEC:	/* vaccess file and accmode & VEXEC */
	case PRIV_VFS_GENERATION:
	case PRIV_VFS_LOOKUP:	/* vaccess DIR */
		if (label & GBL_VACCESS)
			rc = 0;
		break;
	case PRIV_VERIEXEC_DIRECT:
		/*
		 * We are here because we are attempting to direct exec
		 * something with the 'indirect' flag set.
		 * We need to check parent label for this one.
		 */
		PROC_LOCK(curproc);
		label = (gbl_label_t)SLOT(curproc->p_pptr->p_textvp->v_label);
		if (label & GBL_VERIEXEC) {
			rc = 0;
			/*
			 * Of course the only reason to be running an
			 * interpreter this way is to bypass O_VERIFY
			 * so we can run unsigned script.
			 * We set GBL_VERIEXEC on p_label for
			 * PRIV_VERIEXEC_NOVERIFY below
			 */
			SLOT_SET(curproc->p_label, GBL_VERIEXEC);
		}
		PROC_UNLOCK(curproc);
		break;
	case PRIV_VERIEXEC_NOVERIFY:
		/* we look at p_label! see above */
		label = (gbl_label_t)SLOT(curproc->p_label);
		if (label & GBL_VERIEXEC)
			rc = 0;
		break;
	default:
		break;
	}
	MAC_GRANTBYLABEL_DBG(rc ? 1 : 2,
	    "pid=%d priv=%d, label=%#o rc=%d",
	    curproc->p_pid, priv, label, rc);

	return rc;
}


/*
 * If proc->p_textvp does not yet have a label,
 * fetch file info from mac_veriexec
 * and set label (if any) else set.
 * If there is no label set it to GBL_EMPTY.
 */
static int
mac_grantbylabel_proc_check_resource(struct ucred *cred,
    struct proc *proc)
{
	gbl_label_t gbl;

	if (!SLOT(proc->p_textvp->v_label)) {
		gbl = gbl_get_vlabel(proc->p_textvp, cred);
		if (gbl == 0)
			gbl = GBL_EMPTY;
		SLOT_SET(proc->p_textvp->v_label, gbl);
	}
	return 0;
}

static int
mac_grantbylabel_syscall(struct thread *td, int call, void *arg)
{
	cap_rights_t rights;
	struct mac_grantbylabel_fetch_gbl_args gbl_args;
	struct file *fp;
	struct proc *proc;
	int error;
	int proc_locked;

	switch (call) {
	case MAC_GRANTBYLABEL_FETCH_GBL:
	case MAC_GRANTBYLABEL_FETCH_PID_GBL:
		error = copyin(arg, &gbl_args, sizeof(gbl_args));
		if (error)
			return error;
		gbl_args.gbl = 0;
		break;
	default:
		return EOPNOTSUPP;
		break;
	}
	proc_locked = 0;
	switch (call) {
	case MAC_GRANTBYLABEL_FETCH_GBL:
		error = getvnode(td, gbl_args.u.fd,
		    cap_rights_init(&rights), &fp);
		if (error)
			return (error);

		if (fp->f_type != DTYPE_VNODE) {
		       error = EINVAL;
		       goto cleanup_file;
		}

		vn_lock(fp->f_vnode, LK_SHARED | LK_RETRY);
		gbl_args.gbl = gbl_get_vlabel(fp->f_vnode, td->td_ucred);
		if (gbl_args.gbl == 0)
			error = EOPNOTSUPP;
		else
			error = 0;
		VOP_UNLOCK(fp->f_vnode);
cleanup_file:
		fdrop(fp, td);
		break;
	case MAC_GRANTBYLABEL_FETCH_PID_GBL:
		error = 0;
		if (gbl_args.u.pid == 0
		    || gbl_args.u.pid == curproc->p_pid) {
			proc = curproc;
		} else {
			proc = pfind(gbl_args.u.pid);
			if (proc == NULL)
				return (EINVAL);
			proc_locked = 1;
		}
		gbl_args.gbl = (SLOT(proc->p_textvp->v_label) |
		    SLOT(proc->p_label));
		if (proc_locked)
			PROC_UNLOCK(proc);
		break;
	}
	if (error == 0) {
		error = copyout(&gbl_args, arg, sizeof(gbl_args));
	}
	return error;
}


static void
mac_grantbylabel_proc_init_label(struct label *label)
{

	SLOT_SET(label, 0);		/* not yet set! */
}

static void
mac_grantbylabel_vnode_init_label(struct label *label)
{

	SLOT_SET(label, 0);		/* not yet set! */
}

/**
 * @brief set v_label if needed
 */
static int
mac_grantbylabel_vnode_check_exec(struct ucred *cred __unused,
    struct vnode *vp __unused, struct label *label __unused,
    struct image_params *imgp, struct label *execlabel __unused)
{
	gbl_label_t gbl;

	gbl = SLOT(vp->v_label);
	if (gbl == 0) {
		gbl = gbl_get_vlabel(vp, cred);
		if (gbl == 0)
			gbl = GBL_EMPTY;
		MAC_GRANTBYLABEL_DBG(1, "vnode_check_exec label=%#o", gbl);
		SLOT_SET(vp->v_label, gbl);
	}
	return 0;
}

static void
mac_grantbylabel_copy_label(struct label *src, struct label *dest)
{
	SLOT_SET(dest, SLOT(src));
}

/**
 * @brief if interpreting copy script v_label to proc p_label
 */
static int
mac_grantbylabel_vnode_execve_will_transition(struct ucred *old,
    struct vnode *vp, struct label *vplabel,
    struct label *interpvplabel, struct image_params *imgp,
    struct label *execlabel)
{
	gbl_label_t gbl;

	if (imgp->interpreted) {
		gbl = SLOT(interpvplabel);
		if (gbl) {
			SLOT_SET(imgp->proc->p_label, gbl);
		}
		MAC_GRANTBYLABEL_DBG(1, "execve_will_transition label=%#o", gbl);
	}
	return 0;
}


static struct mac_policy_ops mac_grantbylabel_ops =
{
	.mpo_proc_check_resource = mac_grantbylabel_proc_check_resource,
	.mpo_priv_grant = mac_grantbylabel_priv_grant,
	.mpo_syscall = mac_grantbylabel_syscall,
	.mpo_proc_init_label = mac_grantbylabel_proc_init_label,
	.mpo_vnode_check_exec = mac_grantbylabel_vnode_check_exec,
	.mpo_vnode_copy_label = mac_grantbylabel_copy_label,
	.mpo_vnode_execve_will_transition = mac_grantbylabel_vnode_execve_will_transition,
	.mpo_vnode_init_label = mac_grantbylabel_vnode_init_label,
};

MAC_POLICY_SET(&mac_grantbylabel_ops, mac_grantbylabel,
    MAC_GRANTBYLABEL_FULLNAME,
    MPC_LOADTIME_FLAG_NOTLATE, &mac_grantbylabel_slot);
MODULE_VERSION(mac_grantbylabel, 1);
MODULE_DEPEND(mac_grantbylabel, mac_veriexec, MAC_VERIEXEC_VERSION,
    MAC_VERIEXEC_VERSION, MAC_VERIEXEC_VERSION);
