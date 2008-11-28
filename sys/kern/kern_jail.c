/*-
 * Copyright (c) 1999 Poul-Henning Kamp. All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_mac.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/sysproto.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/taskqueue.h>
#include <sys/fcntl.h>
#include <sys/jail.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/namei.h>
#include <sys/mount.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/vimage.h>
#include <sys/osd.h>
#include <net/if.h>
#include <netinet/in.h>

#include <security/mac/mac_framework.h>

MALLOC_DEFINE(M_PRISON, "prison", "Prison structures");

SYSCTL_NODE(_security, OID_AUTO, jail, CTLFLAG_RW, 0,
    "Jail rules");

int	jail_set_hostname_allowed = 1;
SYSCTL_INT(_security_jail, OID_AUTO, set_hostname_allowed, CTLFLAG_RW,
    &jail_set_hostname_allowed, 0,
    "Processes in jail can set their hostnames");

int	jail_socket_unixiproute_only = 1;
SYSCTL_INT(_security_jail, OID_AUTO, socket_unixiproute_only, CTLFLAG_RW,
    &jail_socket_unixiproute_only, 0,
    "Processes in jail are limited to creating UNIX/IPv4/route sockets only");

int	jail_sysvipc_allowed = 0;
SYSCTL_INT(_security_jail, OID_AUTO, sysvipc_allowed, CTLFLAG_RW,
    &jail_sysvipc_allowed, 0,
    "Processes in jail can use System V IPC primitives");

static int jail_enforce_statfs = 2;
SYSCTL_INT(_security_jail, OID_AUTO, enforce_statfs, CTLFLAG_RW,
    &jail_enforce_statfs, 0,
    "Processes in jail cannot see all mounted file systems");

int	jail_allow_raw_sockets = 0;
SYSCTL_INT(_security_jail, OID_AUTO, allow_raw_sockets, CTLFLAG_RW,
    &jail_allow_raw_sockets, 0,
    "Prison root can create raw sockets");

int	jail_chflags_allowed = 0;
SYSCTL_INT(_security_jail, OID_AUTO, chflags_allowed, CTLFLAG_RW,
    &jail_chflags_allowed, 0,
    "Processes in jail can alter system file flags");

int	jail_mount_allowed = 0;
SYSCTL_INT(_security_jail, OID_AUTO, mount_allowed, CTLFLAG_RW,
    &jail_mount_allowed, 0,
    "Processes in jail can mount/unmount jail-friendly file systems");

/* allprison, lastprid, and prisoncount are protected by allprison_lock. */
struct	prisonlist allprison;
struct	sx allprison_lock;
int	lastprid = 0;
int	prisoncount = 0;

static void		 init_prison(void *);
static void		 prison_complete(void *context, int pending);
static int		 sysctl_jail_list(SYSCTL_HANDLER_ARGS);

static void
init_prison(void *data __unused)
{

	sx_init(&allprison_lock, "allprison");
	LIST_INIT(&allprison);
}

SYSINIT(prison, SI_SUB_INTRINSIC, SI_ORDER_ANY, init_prison, NULL);

/*
 * struct jail_args {
 *	struct jail *jail;
 * };
 */
int
jail(struct thread *td, struct jail_args *uap)
{
	struct nameidata nd;
	struct prison *pr, *tpr;
	struct jail j;
	struct jail_attach_args jaa;
	int vfslocked, error, tryprid;

	error = copyin(uap->jail, &j, sizeof(j));
	if (error)
		return (error);
	if (j.version != 0)
		return (EINVAL);

	pr = malloc(sizeof(*pr), M_PRISON, M_WAITOK | M_ZERO);
	mtx_init(&pr->pr_mtx, "jail mutex", NULL, MTX_DEF);
	pr->pr_ref = 1;
	error = copyinstr(j.path, &pr->pr_path, sizeof(pr->pr_path), 0);
	if (error)
		goto e_killmtx;
	NDINIT(&nd, LOOKUP, MPSAFE | FOLLOW | LOCKLEAF, UIO_SYSSPACE,
	    pr->pr_path, td);
	error = namei(&nd);
	if (error)
		goto e_killmtx;
	vfslocked = NDHASGIANT(&nd);
	pr->pr_root = nd.ni_vp;
	VOP_UNLOCK(nd.ni_vp, 0);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	VFS_UNLOCK_GIANT(vfslocked);
	error = copyinstr(j.hostname, &pr->pr_host, sizeof(pr->pr_host), 0);
	if (error)
		goto e_dropvnref;
	pr->pr_ip = j.ip_number;
	pr->pr_linux = NULL;
	pr->pr_securelevel = securelevel;
	bzero(&pr->pr_osd, sizeof(pr->pr_osd));

	/* Determine next pr_id and add prison to allprison list. */
	sx_xlock(&allprison_lock);
	tryprid = lastprid + 1;
	if (tryprid == JAIL_MAX)
		tryprid = 1;
next:
	LIST_FOREACH(tpr, &allprison, pr_list) {
		if (tpr->pr_id == tryprid) {
			tryprid++;
			if (tryprid == JAIL_MAX) {
				sx_xunlock(&allprison_lock);
				error = EAGAIN;
				goto e_dropvnref;
			}
			goto next;
		}
	}
	pr->pr_id = jaa.jid = lastprid = tryprid;
	LIST_INSERT_HEAD(&allprison, pr, pr_list);
	prisoncount++;
	sx_xunlock(&allprison_lock);

	error = jail_attach(td, &jaa);
	if (error)
		goto e_dropprref;
	mtx_lock(&pr->pr_mtx);
	pr->pr_ref--;
	mtx_unlock(&pr->pr_mtx);
	td->td_retval[0] = jaa.jid;
	return (0);
e_dropprref:
	sx_xlock(&allprison_lock);
	LIST_REMOVE(pr, pr_list);
	prisoncount--;
	sx_xunlock(&allprison_lock);
e_dropvnref:
	vfslocked = VFS_LOCK_GIANT(pr->pr_root->v_mount);
	vrele(pr->pr_root);
	VFS_UNLOCK_GIANT(vfslocked);
e_killmtx:
	mtx_destroy(&pr->pr_mtx);
	free(pr, M_PRISON);
	return (error);
}

/*
 * struct jail_attach_args {
 *	int jid;
 * };
 */
int
jail_attach(struct thread *td, struct jail_attach_args *uap)
{
	struct proc *p;
	struct ucred *newcred, *oldcred;
	struct prison *pr;
	int vfslocked, error;

	/*
	 * XXX: Note that there is a slight race here if two threads
	 * in the same privileged process attempt to attach to two
	 * different jails at the same time.  It is important for
	 * user processes not to do this, or they might end up with
	 * a process root from one prison, but attached to the jail
	 * of another.
	 */
	error = priv_check(td, PRIV_JAIL_ATTACH);
	if (error)
		return (error);

	p = td->td_proc;
	sx_slock(&allprison_lock);
	pr = prison_find(uap->jid);
	if (pr == NULL) {
		sx_sunlock(&allprison_lock);
		return (EINVAL);
	}
	pr->pr_ref++;
	mtx_unlock(&pr->pr_mtx);
	sx_sunlock(&allprison_lock);

	vfslocked = VFS_LOCK_GIANT(pr->pr_root->v_mount);
	vn_lock(pr->pr_root, LK_EXCLUSIVE | LK_RETRY);
	if ((error = change_dir(pr->pr_root, td)) != 0)
		goto e_unlock;
#ifdef MAC
	if ((error = mac_vnode_check_chroot(td->td_ucred, pr->pr_root)))
		goto e_unlock;
#endif
	VOP_UNLOCK(pr->pr_root, 0);
	change_root(pr->pr_root, td);
	VFS_UNLOCK_GIANT(vfslocked);

	newcred = crget();
	PROC_LOCK(p);
	oldcred = p->p_ucred;
	setsugid(p);
	crcopy(newcred, oldcred);
	newcred->cr_prison = pr;
	p->p_ucred = newcred;
	PROC_UNLOCK(p);
	crfree(oldcred);
	return (0);
e_unlock:
	VOP_UNLOCK(pr->pr_root, 0);
	VFS_UNLOCK_GIANT(vfslocked);
	mtx_lock(&pr->pr_mtx);
	pr->pr_ref--;
	mtx_unlock(&pr->pr_mtx);
	return (error);
}

/*
 * Returns a locked prison instance, or NULL on failure.
 */
struct prison *
prison_find(int prid)
{
	struct prison *pr;

	sx_assert(&allprison_lock, SX_LOCKED);
	LIST_FOREACH(pr, &allprison, pr_list) {
		if (pr->pr_id == prid) {
			mtx_lock(&pr->pr_mtx);
			if (pr->pr_ref == 0) {
				mtx_unlock(&pr->pr_mtx);
				break;
			}
			return (pr);
		}
	}
	return (NULL);
}

void
prison_free_locked(struct prison *pr)
{

	mtx_assert(&pr->pr_mtx, MA_OWNED);
	pr->pr_ref--;
	if (pr->pr_ref == 0) {
		mtx_unlock(&pr->pr_mtx);
		TASK_INIT(&pr->pr_task, 0, prison_complete, pr);
		taskqueue_enqueue(taskqueue_thread, &pr->pr_task);
		return;
	}
	mtx_unlock(&pr->pr_mtx);
}

void
prison_free(struct prison *pr)
{

	mtx_lock(&pr->pr_mtx);
	prison_free_locked(pr);
}

static void
prison_complete(void *context, int pending)
{
	struct prison *pr;
	int vfslocked;

	pr = (struct prison *)context;

	sx_xlock(&allprison_lock);
	LIST_REMOVE(pr, pr_list);
	prisoncount--;
	sx_xunlock(&allprison_lock);

	/* Free all OSD associated to this jail. */
	osd_jail_exit(pr);

	vfslocked = VFS_LOCK_GIANT(pr->pr_root->v_mount);
	vrele(pr->pr_root);
	VFS_UNLOCK_GIANT(vfslocked);

	mtx_destroy(&pr->pr_mtx);
	if (pr->pr_linux != NULL)
		free(pr->pr_linux, M_PRISON);
	free(pr, M_PRISON);
}

void
prison_hold_locked(struct prison *pr)
{

	mtx_assert(&pr->pr_mtx, MA_OWNED);
	KASSERT(pr->pr_ref > 0,
	    ("Trying to hold dead prison (id=%d).", pr->pr_id));
	pr->pr_ref++;
}

void
prison_hold(struct prison *pr)
{

	mtx_lock(&pr->pr_mtx);
	prison_hold_locked(pr);
	mtx_unlock(&pr->pr_mtx);
}

u_int32_t
prison_getip(struct ucred *cred)
{

	return (cred->cr_prison->pr_ip);
}

int
prison_ip(struct ucred *cred, int flag, u_int32_t *ip)
{
	u_int32_t tmp;

	if (!jailed(cred))
		return (0);
	if (flag)
		tmp = *ip;
	else
		tmp = ntohl(*ip);
	if (tmp == INADDR_ANY) {
		if (flag)
			*ip = cred->cr_prison->pr_ip;
		else
			*ip = htonl(cred->cr_prison->pr_ip);
		return (0);
	}
	if (tmp == INADDR_LOOPBACK) {
		if (flag)
			*ip = cred->cr_prison->pr_ip;
		else
			*ip = htonl(cred->cr_prison->pr_ip);
		return (0);
	}
	if (cred->cr_prison->pr_ip != tmp)
		return (1);
	return (0);
}

void
prison_remote_ip(struct ucred *cred, int flag, u_int32_t *ip)
{
	u_int32_t tmp;

	if (!jailed(cred))
		return;
	if (flag)
		tmp = *ip;
	else
		tmp = ntohl(*ip);
	if (tmp == INADDR_LOOPBACK) {
		if (flag)
			*ip = cred->cr_prison->pr_ip;
		else
			*ip = htonl(cred->cr_prison->pr_ip);
		return;
	}
	return;
}

int
prison_if(struct ucred *cred, struct sockaddr *sa)
{
	struct sockaddr_in *sai;
	int ok;

	sai = (struct sockaddr_in *)sa;
	if ((sai->sin_family != AF_INET) && jail_socket_unixiproute_only)
		ok = 1;
	else if (sai->sin_family != AF_INET)
		ok = 0;
	else if (cred->cr_prison->pr_ip != ntohl(sai->sin_addr.s_addr))
		ok = 1;
	else
		ok = 0;
	return (ok);
}

/*
 * Return 0 if jails permit p1 to frob p2, otherwise ESRCH.
 */
int
prison_check(struct ucred *cred1, struct ucred *cred2)
{

	if (jailed(cred1)) {
		if (!jailed(cred2))
			return (ESRCH);
		if (cred2->cr_prison != cred1->cr_prison)
			return (ESRCH);
	}

	return (0);
}

/*
 * Return 1 if the passed credential is in a jail, otherwise 0.
 */
int
jailed(struct ucred *cred)
{

	return (cred->cr_prison != NULL);
}

/*
 * Return the correct hostname for the passed credential.
 */
void
getcredhostname(struct ucred *cred, char *buf, size_t size)
{
	INIT_VPROCG(cred->cr_vimage->v_procg);

	if (jailed(cred)) {
		mtx_lock(&cred->cr_prison->pr_mtx);
		strlcpy(buf, cred->cr_prison->pr_host, size);
		mtx_unlock(&cred->cr_prison->pr_mtx);
	} else {
		mtx_lock(&hostname_mtx);
		strlcpy(buf, V_hostname, size);
		mtx_unlock(&hostname_mtx);
	}
}

/*
 * Determine whether the subject represented by cred can "see"
 * status of a mount point.
 * Returns: 0 for permitted, ENOENT otherwise.
 * XXX: This function should be called cr_canseemount() and should be
 *      placed in kern_prot.c.
 */
int
prison_canseemount(struct ucred *cred, struct mount *mp)
{
	struct prison *pr;
	struct statfs *sp;
	size_t len;

	if (!jailed(cred) || jail_enforce_statfs == 0)
		return (0);
	pr = cred->cr_prison;
	if (pr->pr_root->v_mount == mp)
		return (0);
	if (jail_enforce_statfs == 2)
		return (ENOENT);
	/*
	 * If jail's chroot directory is set to "/" we should be able to see
	 * all mount-points from inside a jail.
	 * This is ugly check, but this is the only situation when jail's
	 * directory ends with '/'.
	 */
	if (strcmp(pr->pr_path, "/") == 0)
		return (0);
	len = strlen(pr->pr_path);
	sp = &mp->mnt_stat;
	if (strncmp(pr->pr_path, sp->f_mntonname, len) != 0)
		return (ENOENT);
	/*
	 * Be sure that we don't have situation where jail's root directory
	 * is "/some/path" and mount point is "/some/pathpath".
	 */
	if (sp->f_mntonname[len] != '\0' && sp->f_mntonname[len] != '/')
		return (ENOENT);
	return (0);
}

void
prison_enforce_statfs(struct ucred *cred, struct mount *mp, struct statfs *sp)
{
	char jpath[MAXPATHLEN];
	struct prison *pr;
	size_t len;

	if (!jailed(cred) || jail_enforce_statfs == 0)
		return;
	pr = cred->cr_prison;
	if (prison_canseemount(cred, mp) != 0) {
		bzero(sp->f_mntonname, sizeof(sp->f_mntonname));
		strlcpy(sp->f_mntonname, "[restricted]",
		    sizeof(sp->f_mntonname));
		return;
	}
	if (pr->pr_root->v_mount == mp) {
		/*
		 * Clear current buffer data, so we are sure nothing from
		 * the valid path left there.
		 */
		bzero(sp->f_mntonname, sizeof(sp->f_mntonname));
		*sp->f_mntonname = '/';
		return;
	}
	/*
	 * If jail's chroot directory is set to "/" we should be able to see
	 * all mount-points from inside a jail.
	 */
	if (strcmp(pr->pr_path, "/") == 0)
		return;
	len = strlen(pr->pr_path);
	strlcpy(jpath, sp->f_mntonname + len, sizeof(jpath));
	/*
	 * Clear current buffer data, so we are sure nothing from
	 * the valid path left there.
	 */
	bzero(sp->f_mntonname, sizeof(sp->f_mntonname));
	if (*jpath == '\0') {
		/* Should never happen. */
		*sp->f_mntonname = '/';
	} else {
		strlcpy(sp->f_mntonname, jpath, sizeof(sp->f_mntonname));
	}
}

/*
 * Check with permission for a specific privilege is granted within jail.  We
 * have a specific list of accepted privileges; the rest are denied.
 */
int
prison_priv_check(struct ucred *cred, int priv)
{

	if (!jailed(cred))
		return (0);

	switch (priv) {

		/*
		 * Allow ktrace privileges for root in jail.
		 */
	case PRIV_KTRACE:

#if 0
		/*
		 * Allow jailed processes to configure audit identity and
		 * submit audit records (login, etc).  In the future we may
		 * want to further refine the relationship between audit and
		 * jail.
		 */
	case PRIV_AUDIT_GETAUDIT:
	case PRIV_AUDIT_SETAUDIT:
	case PRIV_AUDIT_SUBMIT:
#endif

		/*
		 * Allow jailed processes to manipulate process UNIX
		 * credentials in any way they see fit.
		 */
	case PRIV_CRED_SETUID:
	case PRIV_CRED_SETEUID:
	case PRIV_CRED_SETGID:
	case PRIV_CRED_SETEGID:
	case PRIV_CRED_SETGROUPS:
	case PRIV_CRED_SETREUID:
	case PRIV_CRED_SETREGID:
	case PRIV_CRED_SETRESUID:
	case PRIV_CRED_SETRESGID:

		/*
		 * Jail implements visibility constraints already, so allow
		 * jailed root to override uid/gid-based constraints.
		 */
	case PRIV_SEEOTHERGIDS:
	case PRIV_SEEOTHERUIDS:

		/*
		 * Jail implements inter-process debugging limits already, so
		 * allow jailed root various debugging privileges.
		 */
	case PRIV_DEBUG_DIFFCRED:
	case PRIV_DEBUG_SUGID:
	case PRIV_DEBUG_UNPRIV:

		/*
		 * Allow jail to set various resource limits and login
		 * properties, and for now, exceed process resource limits.
		 */
	case PRIV_PROC_LIMIT:
	case PRIV_PROC_SETLOGIN:
	case PRIV_PROC_SETRLIMIT:

		/*
		 * System V and POSIX IPC privileges are granted in jail.
		 */
	case PRIV_IPC_READ:
	case PRIV_IPC_WRITE:
	case PRIV_IPC_ADMIN:
	case PRIV_IPC_MSGSIZE:
	case PRIV_MQ_ADMIN:

		/*
		 * Jail implements its own inter-process limits, so allow
		 * root processes in jail to change scheduling on other
		 * processes in the same jail.  Likewise for signalling.
		 */
	case PRIV_SCHED_DIFFCRED:
	case PRIV_SIGNAL_DIFFCRED:
	case PRIV_SIGNAL_SUGID:

		/*
		 * Allow jailed processes to write to sysctls marked as jail
		 * writable.
		 */
	case PRIV_SYSCTL_WRITEJAIL:

		/*
		 * Allow root in jail to manage a variety of quota
		 * properties.  These should likely be conditional on a
		 * configuration option.
		 */
	case PRIV_VFS_GETQUOTA:
	case PRIV_VFS_SETQUOTA:

		/*
		 * Since Jail relies on chroot() to implement file system
		 * protections, grant many VFS privileges to root in jail.
		 * Be careful to exclude mount-related and NFS-related
		 * privileges.
		 */
	case PRIV_VFS_READ:
	case PRIV_VFS_WRITE:
	case PRIV_VFS_ADMIN:
	case PRIV_VFS_EXEC:
	case PRIV_VFS_LOOKUP:
	case PRIV_VFS_BLOCKRESERVE:	/* XXXRW: Slightly surprising. */
	case PRIV_VFS_CHFLAGS_DEV:
	case PRIV_VFS_CHOWN:
	case PRIV_VFS_CHROOT:
	case PRIV_VFS_RETAINSUGID:
	case PRIV_VFS_FCHROOT:
	case PRIV_VFS_LINK:
	case PRIV_VFS_SETGID:
	case PRIV_VFS_STAT:
	case PRIV_VFS_STICKYFILE:
		return (0);

		/*
		 * Depending on the global setting, allow privilege of
		 * setting system flags.
		 */
	case PRIV_VFS_SYSFLAGS:
		if (jail_chflags_allowed)
			return (0);
		else
			return (EPERM);

		/*
		 * Depending on the global setting, allow privilege of
		 * mounting/unmounting file systems.
		 */
	case PRIV_VFS_MOUNT:
	case PRIV_VFS_UNMOUNT:
	case PRIV_VFS_MOUNT_NONUSER:
	case PRIV_VFS_MOUNT_OWNER:
		if (jail_mount_allowed)
			return (0);
		else
			return (EPERM);

		/*
		 * Allow jailed root to bind reserved ports and reuse in-use
		 * ports.
		 */
	case PRIV_NETINET_RESERVEDPORT:
	case PRIV_NETINET_REUSEPORT:
		return (0);

		/*
		 * Allow jailed root to set certian IPv4/6 (option) headers.
		 */
	case PRIV_NETINET_SETHDROPTS:
		return (0);

		/*
		 * Conditionally allow creating raw sockets in jail.
		 */
	case PRIV_NETINET_RAW:
		if (jail_allow_raw_sockets)
			return (0);
		else
			return (EPERM);

		/*
		 * Since jail implements its own visibility limits on netstat
		 * sysctls, allow getcred.  This allows identd to work in
		 * jail.
		 */
	case PRIV_NETINET_GETCRED:
		return (0);

	default:
		/*
		 * In all remaining cases, deny the privilege request.  This
		 * includes almost all network privileges, many system
		 * configuration privileges.
		 */
		return (EPERM);
	}
}

static int
sysctl_jail_list(SYSCTL_HANDLER_ARGS)
{
	struct xprison *xp, *sxp;
	struct prison *pr;
	int count, error;

	if (jailed(req->td->td_ucred))
		return (0);

	sx_slock(&allprison_lock);
	if ((count = prisoncount) == 0) {
		sx_sunlock(&allprison_lock);
		return (0);
	}

	sxp = xp = malloc(sizeof(*xp) * count, M_TEMP, M_WAITOK | M_ZERO);

	LIST_FOREACH(pr, &allprison, pr_list) {
		xp->pr_version = XPRISON_VERSION;
		xp->pr_id = pr->pr_id;
		xp->pr_ip = pr->pr_ip;
		strlcpy(xp->pr_path, pr->pr_path, sizeof(xp->pr_path));
		mtx_lock(&pr->pr_mtx);
		strlcpy(xp->pr_host, pr->pr_host, sizeof(xp->pr_host));
		mtx_unlock(&pr->pr_mtx);
		xp++;
	}
	sx_sunlock(&allprison_lock);

	error = SYSCTL_OUT(req, sxp, sizeof(*sxp) * count);
	free(sxp, M_TEMP);
	return (error);
}

SYSCTL_OID(_security_jail, OID_AUTO, list, CTLTYPE_STRUCT | CTLFLAG_RD,
    NULL, 0, sysctl_jail_list, "S", "List of active jails");

static int
sysctl_jail_jailed(SYSCTL_HANDLER_ARGS)
{
	int error, injail;

	injail = jailed(req->td->td_ucred);
	error = SYSCTL_OUT(req, &injail, sizeof(injail));

	return (error);
}
SYSCTL_PROC(_security_jail, OID_AUTO, jailed, CTLTYPE_INT | CTLFLAG_RD,
    NULL, 0, sysctl_jail_jailed, "I", "Process in jail?");
