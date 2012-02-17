/*-
 * Copyright (c) 1999 Poul-Henning Kamp.
 * Copyright (c) 2008 Bjoern A. Zeeb.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"
#include "opt_inet.h"
#include "opt_inet6.h"
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
#include <net/if.h>
#include <netinet/in.h>
#ifdef DDB
#include <ddb/ddb.h>
#ifdef INET6
#include <netinet6/in6_var.h>
#endif /* INET6 */
#endif /* DDB */

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
    "Processes in jail are limited to creating UNIX/IP/route sockets only");

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

#ifdef INET
static int	jail_ip4_saddrsel = 1;
SYSCTL_INT(_security_jail, OID_AUTO, ip4_saddrsel, CTLFLAG_RW,
    &jail_ip4_saddrsel, 0,
   "Do (not) use IPv4 source address selection rather than the "
   "primary jail IPv4 address.");
#endif

#ifdef INET6
static int	jail_ip6_saddrsel = 1;
SYSCTL_INT(_security_jail, OID_AUTO, ip6_saddrsel, CTLFLAG_RW,
    &jail_ip6_saddrsel, 0,
   "Do (not) use IPv6 source address selection rather than the "
   "primary jail IPv6 address.");
#endif

int	jail_chflags_allowed = 0;
SYSCTL_INT(_security_jail, OID_AUTO, chflags_allowed, CTLFLAG_RW,
    &jail_chflags_allowed, 0,
    "Processes in jail can alter system file flags");

int	jail_mount_allowed = 0;
SYSCTL_INT(_security_jail, OID_AUTO, mount_allowed, CTLFLAG_RW,
    &jail_mount_allowed, 0,
    "Processes in jail can mount/unmount jail-friendly file systems");

int	jail_max_af_ips = 255;
SYSCTL_INT(_security_jail, OID_AUTO, jail_max_af_ips, CTLFLAG_RW,
    &jail_max_af_ips, 0,
    "Number of IP addresses a jail may have at most per address family");

/* allprison, lastprid, and prisoncount are protected by allprison_lock. */
struct	prisonlist allprison;
struct	sx allprison_lock;
int	lastprid = 0;
int	prisoncount = 0;

/*
 * List of jail services. Protected by allprison_lock.
 */
TAILQ_HEAD(prison_services_head, prison_service);
static struct prison_services_head prison_services =
    TAILQ_HEAD_INITIALIZER(prison_services);
static int prison_service_slots = 0;

struct prison_service {
	prison_create_t ps_create;
	prison_destroy_t ps_destroy;
	int		ps_slotno;
	TAILQ_ENTRY(prison_service) ps_next;
	char	ps_name[0];
};

static void		 init_prison(void *);
static void		 prison_complete(void *context, int pending);
static int		 sysctl_jail_list(SYSCTL_HANDLER_ARGS);
#ifdef INET
static int		_prison_check_ip4(struct prison *, struct in_addr *);
#endif
#ifdef INET6
static int		_prison_check_ip6(struct prison *, struct in6_addr *);
#endif

static void
init_prison(void *data __unused)
{

	sx_init(&allprison_lock, "allprison");
	LIST_INIT(&allprison);
}

SYSINIT(prison, SI_SUB_INTRINSIC, SI_ORDER_ANY, init_prison, NULL);

#ifdef INET
static int
qcmp_v4(const void *ip1, const void *ip2)
{
	in_addr_t iaa, iab;

	/*
	 * We need to compare in HBO here to get the list sorted as expected
	 * by the result of the code.  Sorting NBO addresses gives you
	 * interesting results.  If you do not understand, do not try.
	 */
	iaa = ntohl(((const struct in_addr *)ip1)->s_addr);
	iab = ntohl(((const struct in_addr *)ip2)->s_addr);

	/*
	 * Do not simply return the difference of the two numbers, the int is
	 * not wide enough.
	 */
	if (iaa > iab)
		return (1);
	else if (iaa < iab)
		return (-1);
	else
		return (0);
}
#endif

#ifdef INET6
static int
qcmp_v6(const void *ip1, const void *ip2)
{
	const struct in6_addr *ia6a, *ia6b;
	int i, rc;

	ia6a = (const struct in6_addr *)ip1;
	ia6b = (const struct in6_addr *)ip2;

	rc = 0;
	for (i=0; rc == 0 && i < sizeof(struct in6_addr); i++) {
		if (ia6a->s6_addr[i] > ia6b->s6_addr[i])
			rc = 1;
		else if (ia6a->s6_addr[i] < ia6b->s6_addr[i])
			rc = -1;
	}
	return (rc);
}
#endif

#if defined(INET) || defined(INET6)
static int
prison_check_conflicting_ips(struct prison *p)
{
	struct prison *pr;
	int i;

	sx_assert(&allprison_lock, SX_LOCKED);

	if (p->pr_ip4s == 0 && p->pr_ip6s == 0)
		return (0);

	LIST_FOREACH(pr, &allprison, pr_list) {
		/*
		 * Skip 'dying' prisons to avoid problems when
		 * restarting multi-IP jails.
		 */
		if (pr->pr_state == PRISON_STATE_DYING)
			continue;

		/*
		 * We permit conflicting IPs if there is no
		 * more than 1 IP on eeach jail.
		 * In case there is one duplicate on a jail with
		 * more than one IP stop checking and return error.
		 */
#ifdef INET
		if ((p->pr_ip4s >= 1 && pr->pr_ip4s > 1) ||
		    (p->pr_ip4s > 1 && pr->pr_ip4s >= 1)) {
			for (i = 0; i < p->pr_ip4s; i++) {
				if (_prison_check_ip4(pr, &p->pr_ip4[i]) == 0)
					return (EINVAL);
			}
		}
#endif
#ifdef INET6
		if ((p->pr_ip6s >= 1 && pr->pr_ip6s > 1) ||
		    (p->pr_ip6s > 1 && pr->pr_ip6s >= 1)) {
			for (i = 0; i < p->pr_ip6s; i++) {
				if (_prison_check_ip6(pr, &p->pr_ip6[i]) == 0)
					return (EINVAL);
			}
		}
#endif
	}

	return (0);
}

static int
jail_copyin_ips(struct jail *j)
{
#ifdef INET
	struct in_addr  *ip4;
#endif
#ifdef INET6
	struct in6_addr *ip6;
#endif
	int error, i;

	/*
	 * Copy in addresses, check for duplicate addresses and do some
	 * simple 0 and broadcast checks. If users give other bogus addresses
	 * it is their problem.
	 *
	 * IP addresses are all sorted but ip[0] to preserve the primary IP
	 * address as given from userland.  This special IP is used for
	 * unbound outgoing connections as well for "loopback" traffic in case
	 * source address selection cannot find any more fitting address to
	 * connect from.
	 */
#ifdef INET
	ip4 = NULL;
#endif
#ifdef INET6
	ip6 = NULL;
#endif
#ifdef INET
	if (j->ip4s > 0) {
		ip4 = (struct in_addr *)malloc(j->ip4s * sizeof(struct in_addr),
		    M_PRISON, M_WAITOK | M_ZERO);
		error = copyin(j->ip4, ip4, j->ip4s * sizeof(struct in_addr));
		if (error)
			goto e_free_ip;
		/* Sort all but the first IPv4 address. */
		if (j->ip4s > 1)
			qsort((ip4 + 1), j->ip4s - 1,
			    sizeof(struct in_addr), qcmp_v4);

		/*
		 * We do not have to care about byte order for these checks
		 * so we will do them in NBO.
		 */
		for (i=0; i<j->ip4s; i++) {
			if (ip4[i].s_addr == htonl(INADDR_ANY) ||
			    ip4[i].s_addr == htonl(INADDR_BROADCAST)) {
				error = EINVAL;
				goto e_free_ip;
			}
			if ((i+1) < j->ip4s &&
			    (ip4[0].s_addr == ip4[i+1].s_addr ||
			    ip4[i].s_addr == ip4[i+1].s_addr)) {
				error = EINVAL;
				goto e_free_ip;
			}
		}

		j->ip4 = ip4;
	} else
		j->ip4 = NULL;
#endif
#ifdef INET6
	if (j->ip6s > 0) {
		ip6 = (struct in6_addr *)malloc(j->ip6s * sizeof(struct in6_addr),
		    M_PRISON, M_WAITOK | M_ZERO);
		error = copyin(j->ip6, ip6, j->ip6s * sizeof(struct in6_addr));
		if (error)
			goto e_free_ip;
		/* Sort all but the first IPv6 address. */
		if (j->ip6s > 1)
			qsort((ip6 + 1), j->ip6s - 1,
			    sizeof(struct in6_addr), qcmp_v6);
		for (i=0; i<j->ip6s; i++) {
			if (IN6_IS_ADDR_UNSPECIFIED(&ip6[i])) {
				error = EINVAL;
				goto e_free_ip;
			}
			if ((i+1) < j->ip6s &&
			    (IN6_ARE_ADDR_EQUAL(&ip6[0], &ip6[i+1]) ||
			    IN6_ARE_ADDR_EQUAL(&ip6[i], &ip6[i+1]))) {
				error = EINVAL;
				goto e_free_ip;
			}
		}

		j->ip6 = ip6;
	} else
		j->ip6 = NULL;
#endif
	return (0);

e_free_ip:
#ifdef INET6
	free(ip6, M_PRISON);
#endif
#ifdef INET
	free(ip4, M_PRISON);
#endif
	return (error);
}
#endif /* INET || INET6 */

static int
jail_handle_ips(struct jail *j)
{
#if defined(INET) || defined(INET6)
	int error;
#endif

	/*
	 * Finish conversion for older versions, copyin and setup IPs.
	 */
	switch (j->version) {
	case 0:	
	{
#ifdef INET
		/* FreeBSD single IPv4 jails. */
		struct in_addr *ip4;

		if (j->ip4s == INADDR_ANY || j->ip4s == INADDR_BROADCAST)
			return (EINVAL);
		ip4 = (struct in_addr *)malloc(sizeof(struct in_addr),
		    M_PRISON, M_WAITOK | M_ZERO);

		/*
		 * Jail version 0 still used HBO for the IPv4 address.
		 */
		ip4->s_addr = htonl(j->ip4s);
		j->ip4s = 1;
		j->ip4 = ip4;
		break;
#else
		return (EINVAL);
#endif
	}

	case 1:
		/*
		 * Version 1 was used by multi-IPv4 jail implementations
		 * that never made it into the official kernel.
		 * We should never hit this here; jail() should catch it.
		 */
		return (EINVAL);

	case 2:	/* JAIL_API_VERSION */
		/* FreeBSD multi-IPv4/IPv6,noIP jails. */
#if defined(INET) || defined(INET6)
#ifdef INET
		if (j->ip4s > jail_max_af_ips)
			return (EINVAL);
#else
		if (j->ip4s != 0)
			return (EINVAL);
#endif
#ifdef INET6
		if (j->ip6s > jail_max_af_ips)
			return (EINVAL);
#else
		if (j->ip6s != 0)
			return (EINVAL);
#endif
		error = jail_copyin_ips(j);
		if (error)
			return (error);
#endif
		break;

	default:
		/* Sci-Fi jails are not supported, sorry. */
		return (EINVAL);
	}

	return (0);
}


/*
 * struct jail_args {
 *	struct jail *jail;
 * };
 */
int
jail(struct thread *td, struct jail_args *uap)
{
	uint32_t version;
	int error;
	struct jail j;

	error = copyin(uap->jail, &version, sizeof(uint32_t));
	if (error)
		return (error);

	switch (version) {
	case 0:	
		/* FreeBSD single IPv4 jails. */
	{
		struct jail_v0 j0;

		bzero(&j, sizeof(struct jail));
		error = copyin(uap->jail, &j0, sizeof(struct jail_v0));
		if (error)
			return (error);
		j.version = j0.version;
		j.path = j0.path;
		j.hostname = j0.hostname;
		j.ip4s = j0.ip_number;
		break;
	}

	case 1:
		/*
		 * Version 1 was used by multi-IPv4 jail implementations
		 * that never made it into the official kernel.
		 */
		return (EINVAL);

	case 2:	/* JAIL_API_VERSION */
		/* FreeBSD multi-IPv4/IPv6,noIP jails. */
		error = copyin(uap->jail, &j, sizeof(struct jail));
		if (error)
			return (error);
		break;

	default:
		/* Sci-Fi jails are not supported, sorry. */
		return (EINVAL);
	}
	return (kern_jail(td, &j));
}

int
kern_jail(struct thread *td, struct jail *j)
{
	struct nameidata nd;
	struct prison *pr, *tpr;
	struct prison_service *psrv;
	struct jail_attach_args jaa;
	int vfslocked, error, tryprid;

	KASSERT(j != NULL, ("%s: j is NULL", __func__));

	/* Handle addresses - convert old structs, copyin, check IPs. */
	error = jail_handle_ips(j);
	if (error)
		return (error);

	/* Allocate struct prison and fill it with life. */
	pr = malloc(sizeof(*pr), M_PRISON, M_WAITOK | M_ZERO);
	mtx_init(&pr->pr_mtx, "jail mutex", NULL, MTX_DEF);
	pr->pr_ref = 1;
	error = copyinstr(j->path, &pr->pr_path, sizeof(pr->pr_path), NULL);
	if (error)
		goto e_killmtx;
	NDINIT(&nd, LOOKUP, MPSAFE | FOLLOW | LOCKLEAF, UIO_SYSSPACE,
	    pr->pr_path, td);
	error = namei(&nd);
	if (error)
		goto e_killmtx;
	vfslocked = NDHASGIANT(&nd);
	pr->pr_root = nd.ni_vp;
	VOP_UNLOCK(nd.ni_vp, 0, td);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	VFS_UNLOCK_GIANT(vfslocked);
	error = copyinstr(j->hostname, &pr->pr_host, sizeof(pr->pr_host), NULL);
	if (error)
		goto e_dropvnref;
	if (j->jailname != NULL) {
		error = copyinstr(j->jailname, &pr->pr_name,
		    sizeof(pr->pr_name), NULL);
		if (error)
			goto e_dropvnref;
	}
	if (j->ip4s > 0) {
		pr->pr_ip4 = j->ip4;
		pr->pr_ip4s = j->ip4s;
	}
#ifdef INET6
	if (j->ip6s > 0) {
		pr->pr_ip6 = j->ip6;
		pr->pr_ip6s = j->ip6s;
	}
#endif
	pr->pr_linux = NULL;
	pr->pr_securelevel = securelevel;
	if (prison_service_slots == 0)
		pr->pr_slots = NULL;
	else {
		pr->pr_slots = malloc(sizeof(*pr->pr_slots) * prison_service_slots,
		    M_PRISON, M_ZERO | M_WAITOK);
	}

	/*
	 * Pre-set prison state to ALIVE upon cration.  This is needed so we
	 * can later attach the process to it, etc (avoiding another extra
	 * state for ther process of creation, complicating things).
	 */
	pr->pr_state = PRISON_STATE_ALIVE;

	/* Allocate a dedicated cpuset for each jail. */
	error = cpuset_create_root(td, &pr->pr_cpuset);
	if (error)
		goto e_dropvnref;

	sx_xlock(&allprison_lock);
	/* Make sure we cannot run into problems with ambiguous bind()ings. */
#if defined(INET) || defined(INET6)
	error = prison_check_conflicting_ips(pr);
	if (error) {
		sx_xunlock(&allprison_lock);
		goto e_dropcpuset;
	}
#endif

	/* Determine next pr_id and add prison to allprison list. */
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
				goto e_dropcpuset;
			}
			goto next;
		}
	}
	pr->pr_id = jaa.jid = lastprid = tryprid;
	LIST_INSERT_HEAD(&allprison, pr, pr_list);
	prisoncount++;
	sx_downgrade(&allprison_lock);
	TAILQ_FOREACH(psrv, &prison_services, ps_next) {
		psrv->ps_create(psrv, pr);
	}
	sx_sunlock(&allprison_lock);

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
	sx_downgrade(&allprison_lock);
	TAILQ_FOREACH(psrv, &prison_services, ps_next) {
		psrv->ps_destroy(psrv, pr);
	}
	sx_sunlock(&allprison_lock);
e_dropcpuset:
	cpuset_rel(pr->pr_cpuset);
e_dropvnref:
	if (pr->pr_slots != NULL)
		free(pr->pr_slots, M_PRISON);
	vfslocked = VFS_LOCK_GIANT(pr->pr_root->v_mount);
	vrele(pr->pr_root);
	VFS_UNLOCK_GIANT(vfslocked);
e_killmtx:
	mtx_destroy(&pr->pr_mtx);
	free(pr, M_PRISON);
#ifdef INET6
	free(j->ip6, M_PRISON);
#endif
#ifdef INET
	free(j->ip4, M_PRISON);
#endif
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

	/*
	 * Do not allow a process to attach to a prison that is not
	 * considered to be "ALIVE".
	 */
	if (pr->pr_state != PRISON_STATE_ALIVE) {
		mtx_unlock(&pr->pr_mtx);
		sx_sunlock(&allprison_lock);
		return (EINVAL);
	}
	pr->pr_ref++;
	mtx_unlock(&pr->pr_mtx);
	sx_sunlock(&allprison_lock);

	/*
	 * Reparent the newly attached process to this jail.
	 */
	error = cpuset_setproc_update_set(p, pr->pr_cpuset);
	if (error)
		goto e_unref;

	vfslocked = VFS_LOCK_GIANT(pr->pr_root->v_mount);
	vn_lock(pr->pr_root, LK_EXCLUSIVE | LK_RETRY, td);
	if ((error = change_dir(pr->pr_root, td)) != 0)
		goto e_unlock;
#ifdef MAC
	if ((error = mac_check_vnode_chroot(td->td_ucred, pr->pr_root)))
		goto e_unlock;
#endif
	VOP_UNLOCK(pr->pr_root, 0, td);
	change_root(pr->pr_root, td);
	VFS_UNLOCK_GIANT(vfslocked);

	newcred = crget();
	PROC_LOCK(p);
	oldcred = p->p_ucred;
	setsugid(p);
	crcopy(newcred, oldcred);
	newcred->cr_prison = pr;
	p->p_ucred = newcred;
	prison_proc_hold(pr);
	PROC_UNLOCK(p);
	crfree(oldcred);
	return (0);
e_unlock:
	VOP_UNLOCK(pr->pr_root, 0, td);
	VFS_UNLOCK_GIANT(vfslocked);
e_unref:
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
	struct prison_service *psrv;
	struct prison *pr;
	int vfslocked;

	pr = (struct prison *)context;

	sx_xlock(&allprison_lock);
	LIST_REMOVE(pr, pr_list);
	prisoncount--;
	sx_downgrade(&allprison_lock);
	TAILQ_FOREACH(psrv, &prison_services, ps_next) {
		psrv->ps_destroy(psrv, pr);
	}
	sx_sunlock(&allprison_lock);

	cpuset_rel(pr->pr_cpuset);

	if (pr->pr_slots != NULL)
		free(pr->pr_slots, M_PRISON);

	vfslocked = VFS_LOCK_GIANT(pr->pr_root->v_mount);
	vrele(pr->pr_root);
	VFS_UNLOCK_GIANT(vfslocked);

	mtx_destroy(&pr->pr_mtx);
	free(pr->pr_linux, M_PRISON);
#ifdef INET6
	free(pr->pr_ip6, M_PRISON);
#endif
#ifdef INET
	free(pr->pr_ip4, M_PRISON);
#endif
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

void
prison_proc_hold(struct prison *pr)
{

	mtx_lock(&pr->pr_mtx);
	KASSERT(pr->pr_state == PRISON_STATE_ALIVE,
	    ("Cannot add a process to a non-alive prison (id=%d).", pr->pr_id));
	pr->pr_nprocs++;
	mtx_unlock(&pr->pr_mtx);
}

void
prison_proc_free(struct prison *pr)
{

	mtx_lock(&pr->pr_mtx);
	KASSERT(pr->pr_state == PRISON_STATE_ALIVE && pr->pr_nprocs > 0,
	    ("Trying to kill a process in a dead prison (id=%d).", pr->pr_id));
	pr->pr_nprocs--;
	if (pr->pr_nprocs == 0)
		pr->pr_state = PRISON_STATE_DYING;
	mtx_unlock(&pr->pr_mtx);
}


#ifdef INET
/*
 * Pass back primary IPv4 address of this jail.
 *
 * If not jailed return success but do not alter the address.  Caller has to
 * make sure to intialize it correctly (e.g. INADDR_ANY).
 *
 * Returns 0 on success, EAFNOSUPPORT if the jail doesn't allow IPv4.
 * Address returned in NBO.
 */
int
prison_get_ip4(struct ucred *cred, struct in_addr *ia)
{

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia != NULL, ("%s: ia is NULL", __func__));

	if (!jailed(cred))
		/* Do not change address passed in. */
		return (0);

	if (cred->cr_prison->pr_ip4 == NULL)
		return (EAFNOSUPPORT);

	ia->s_addr = cred->cr_prison->pr_ip4[0].s_addr;
	return (0);
}

/*
 * Return 1 if we should do proper source address selection or are not jailed.
 * We will return 0 if we should bypass source address selection in favour
 * of the primary jail IPv4 address. Only in this case *ia will be updated and
 * returned in NBO.
 * Return EAFNOSUPPORT, in case this jail does not allow IPv4.
 */
int
prison_saddrsel_ip4(struct ucred *cred, struct in_addr *ia)
{
	struct in_addr lia;
	int error;

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia != NULL, ("%s: ia is NULL", __func__));

	if (!jailed(cred))
		return (1);

	if (jail_ip4_saddrsel != 0)
		return (1);

	lia.s_addr = INADDR_ANY;
	error = prison_get_ip4(cred, &lia);
	if (error)
		return (error);
	if (lia.s_addr == INADDR_ANY)
		return (1);

	ia->s_addr = lia.s_addr;
	return (0);
}

/*
 * Make sure our (source) address is set to something meaningful to this
 * jail.
 *
 * Returns 0 if not jailed or if address belongs to jail, EADDRNOTAVAIL if
 * the address doesn't belong, or EAFNOSUPPORT if the jail doesn't allow IPv4.
 * Address passed in in NBO and returned in NBO.
 */
int
prison_local_ip4(struct ucred *cred, struct in_addr *ia)
{
	struct in_addr ia0;

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia != NULL, ("%s: ia is NULL", __func__));

	if (!jailed(cred))
		return (0);
	if (cred->cr_prison->pr_ip4 == NULL)
		return (EAFNOSUPPORT);

	ia0.s_addr = ntohl(ia->s_addr);
	if (ia0.s_addr == INADDR_LOOPBACK) {
		ia->s_addr = cred->cr_prison->pr_ip4[0].s_addr;
		return (0);
	}

	if (ia0.s_addr == INADDR_ANY) {
		/*
		 * In case there is only 1 IPv4 address, bind directly.
		 */
		if (cred->cr_prison->pr_ip4s == 1)
			ia->s_addr = cred->cr_prison->pr_ip4[0].s_addr;
		return (0);
	}

	return (_prison_check_ip4(cred->cr_prison, ia));
}

/*
 * Rewrite destination address in case we will connect to loopback address.
 *
 * Returns 0 on success, EAFNOSUPPORT if the jail doesn't allow IPv4.
 * Address passed in in NBO and returned in NBO.
 */
int
prison_remote_ip4(struct ucred *cred, struct in_addr *ia)
{

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia != NULL, ("%s: ia is NULL", __func__));

	if (!jailed(cred))
		return (0);
	if (cred->cr_prison->pr_ip4 == NULL)
		return (EAFNOSUPPORT);

	if (ntohl(ia->s_addr) == INADDR_LOOPBACK) {
		ia->s_addr = cred->cr_prison->pr_ip4[0].s_addr;
		return (0);
	}

	/*
	 * Return success because nothing had to be changed.
	 */
	return (0);
}

/*
 * Check if given address belongs to the jail referenced by cred/prison.
 *
 * Returns 0 if not jailed or if address belongs to jail, EADDRNOTAVAIL if
 * the address doesn't belong, or EAFNOSUPPORT if the jail doesn't allow IPv4.
 * Address passed in in NBO.
 */
static int
_prison_check_ip4(struct prison *pr, struct in_addr *ia)
{
	int i, a, z, d;

	/*
	 * Check the primary IP.
	 */
	if (pr->pr_ip4[0].s_addr == ia->s_addr)
		return (0);

	/*
	 * All the other IPs are sorted so we can do a binary search.
	 */
	a = 0;
	z = pr->pr_ip4s - 2;
	while (a <= z) {
		i = (a + z) / 2;
		d = qcmp_v4(&pr->pr_ip4[i+1], ia);
		if (d > 0)
			z = i - 1;
		else if (d < 0)
			a = i + 1;
		else
			return (0);
	}

	return (EADDRNOTAVAIL);
}

int
prison_check_ip4(struct ucred *cred, struct in_addr *ia)
{

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia != NULL, ("%s: ia is NULL", __func__));

	if (!jailed(cred))
		return (0);
	if (cred->cr_prison->pr_ip4 == NULL)
		return (EAFNOSUPPORT);

	return (_prison_check_ip4(cred->cr_prison, ia));
}
#endif

#ifdef INET6
/*
 * Pass back primary IPv6 address for this jail.
 *
 * If not jailed return success but do not alter the address.  Caller has to
 * make sure to intialize it correctly (e.g. IN6ADDR_ANY_INIT).
 *
 * Returns 0 on success, EAFNOSUPPORT if the jail doesn't allow IPv6.
 */
int
prison_get_ip6(struct ucred *cred, struct in6_addr *ia6)
{

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia6 != NULL, ("%s: ia6 is NULL", __func__));

	if (!jailed(cred))
		return (0);
	if (cred->cr_prison->pr_ip6 == NULL)
		return (EAFNOSUPPORT);

	bcopy(&cred->cr_prison->pr_ip6[0], ia6, sizeof(struct in6_addr));
	return (0);
}

/*
 * Return 1 if we should do proper source address selection or are not jailed.
 * We will return 0 if we should bypass source address selection in favour
 * of the primary jail IPv6 address. Only in this case *ia will be updated and
 * returned in NBO.
 * Return EAFNOSUPPORT, in case this jail does not allow IPv6.
 */
int
prison_saddrsel_ip6(struct ucred *cred, struct in6_addr *ia6)
{
	struct in6_addr lia6;
	int error;

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia6 != NULL, ("%s: ia6 is NULL", __func__));

	if (!jailed(cred))
		return (1);

	if (jail_ip6_saddrsel != 0)
		return (1);

	lia6 = in6addr_any;
	error = prison_get_ip6(cred, &lia6);
	if (error)
		return (error);
	if (IN6_IS_ADDR_UNSPECIFIED(&lia6))
		return (1);

	bcopy(&lia6, ia6, sizeof(struct in6_addr));
	return (0);
}

/*
 * Make sure our (source) address is set to something meaningful to this jail.
 *
 * v6only should be set based on (inp->inp_flags & IN6P_IPV6_V6ONLY != 0)
 * when needed while binding.
 *
 * Returns 0 if not jailed or if address belongs to jail, EADDRNOTAVAIL if
 * the address doesn't belong, or EAFNOSUPPORT if the jail doesn't allow IPv6.
 */
int
prison_local_ip6(struct ucred *cred, struct in6_addr *ia6, int v6only)
{

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia6 != NULL, ("%s: ia6 is NULL", __func__));

	if (!jailed(cred))
		return (0);
	if (cred->cr_prison->pr_ip6 == NULL)
		return (EAFNOSUPPORT);

	if (IN6_IS_ADDR_LOOPBACK(ia6)) {
		bcopy(&cred->cr_prison->pr_ip6[0], ia6,
		    sizeof(struct in6_addr));
		return (0);
	}

	if (IN6_IS_ADDR_UNSPECIFIED(ia6)) {
		/*
		 * In case there is only 1 IPv6 address, and v6only is true,
		 * then bind directly.
		 */
		if (v6only != 0 && cred->cr_prison->pr_ip6s == 1)
			bcopy(&cred->cr_prison->pr_ip6[0], ia6,
			    sizeof(struct in6_addr));
		return (0);
	}

	return (_prison_check_ip6(cred->cr_prison, ia6));
}

/*
 * Rewrite destination address in case we will connect to loopback address.
 *
 * Returns 0 on success, EAFNOSUPPORT if the jail doesn't allow IPv6.
 */
int
prison_remote_ip6(struct ucred *cred, struct in6_addr *ia6)
{

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia6 != NULL, ("%s: ia6 is NULL", __func__));

	if (!jailed(cred))
		return (0);
	if (cred->cr_prison->pr_ip6 == NULL)
		return (EAFNOSUPPORT);

	if (IN6_IS_ADDR_LOOPBACK(ia6)) {
		bcopy(&cred->cr_prison->pr_ip6[0], ia6,
		    sizeof(struct in6_addr));
		return (0);
	}

	/*
	 * Return success because nothing had to be changed.
	 */
	return (0);
}

/*
 * Check if given address belongs to the jail referenced by cred/prison.
 *
 * Returns 0 if not jailed or if address belongs to jail, EADDRNOTAVAIL if
 * the address doesn't belong, or EAFNOSUPPORT if the jail doesn't allow IPv6.
 */
static int
_prison_check_ip6(struct prison *pr, struct in6_addr *ia6)
{
	int i, a, z, d;

	/*
	 * Check the primary IP.
	 */
	if (IN6_ARE_ADDR_EQUAL(&pr->pr_ip6[0], ia6))
		return (0);

	/*
	 * All the other IPs are sorted so we can do a binary search.
	 */
	a = 0;
	z = pr->pr_ip6s - 2;
	while (a <= z) {
		i = (a + z) / 2;
		d = qcmp_v6(&pr->pr_ip6[i+1], ia6);
		if (d > 0)
			z = i - 1;
		else if (d < 0)
			a = i + 1;
		else
			return (0);
	}

	return (EADDRNOTAVAIL);
}

int
prison_check_ip6(struct ucred *cred, struct in6_addr *ia6)
{

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia6 != NULL, ("%s: ia6 is NULL", __func__));

	if (!jailed(cred))
		return (0);
	if (cred->cr_prison->pr_ip6 == NULL)
		return (EAFNOSUPPORT);

	return (_prison_check_ip6(cred->cr_prison, ia6));
}
#endif

/*
 * Check if a jail supports the given address family.
 *
 * Returns 0 if not jailed or the address family is supported, EAFNOSUPPORT
 * if not.
 */
int
prison_check_af(struct ucred *cred, int af)
{
	int error;

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));


	if (!jailed(cred))
		return (0);

	error = 0;
	switch (af)
	{
#ifdef INET
	case AF_INET:
		if (cred->cr_prison->pr_ip4 == NULL)
			error = EAFNOSUPPORT;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		if (cred->cr_prison->pr_ip6 == NULL)
			error = EAFNOSUPPORT;
		break;
#endif
	case AF_LOCAL:
	case AF_ROUTE:
		break;
	default:
		if (jail_socket_unixiproute_only)
			error = EAFNOSUPPORT;
	}
	return (error);
}

/*
 * Check if given address belongs to the jail referenced by cred (wrapper to
 * prison_check_ip[46]).
 *
 * Returns 0 if not jailed or if address belongs to jail, EADDRNOTAVAIL if
 * the address doesn't belong, or EAFNOSUPPORT if the jail doesn't allow
 * the address family.  IPv4 Address passed in in NBO.
 */
int
prison_if(struct ucred *cred, struct sockaddr *sa)
{
#ifdef INET
	struct sockaddr_in *sai;
#endif
#ifdef INET6
	struct sockaddr_in6 *sai6;
#endif
	int error;

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(sa != NULL, ("%s: sa is NULL", __func__));

	error = 0;
	switch (sa->sa_family)
	{
#ifdef INET
	case AF_INET:
		sai = (struct sockaddr_in *)sa;
		error = prison_check_ip4(cred, &sai->sin_addr);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		sai6 = (struct sockaddr_in6 *)sa;
		error = prison_check_ip6(cred, &sai6->sin6_addr);
		break;
#endif
	default:
		if (jailed(cred) && jail_socket_unixiproute_only)
			error = EAFNOSUPPORT;
	}
	return (error);
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

	if (jailed(cred)) {
		mtx_lock(&cred->cr_prison->pr_mtx);
		strlcpy(buf, cred->cr_prison->pr_host, size);
		mtx_unlock(&cred->cr_prison->pr_mtx);
	} else
		strlcpy(buf, hostname, size);
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
	case PRIV_SCHED_CPUSET:
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

/*
 * Register jail service. Provides 'create' and 'destroy' methods.
 * 'create' method will be called for every existing jail and all
 * jails in the future as they beeing created.
 * 'destroy' method will be called for every jail going away and
 * for all existing jails at the time of service deregistration.
 */
struct prison_service *
prison_service_register(const char *name, prison_create_t create,
    prison_destroy_t destroy)
{
	struct prison_service *psrv, *psrv2;
	struct prison *pr;
	int reallocate = 1, slotno = 0;
	void **slots, **oldslots;

	psrv = malloc(sizeof(*psrv) + strlen(name) + 1, M_PRISON,
	    M_WAITOK | M_ZERO);
	psrv->ps_create = create;
	psrv->ps_destroy = destroy;
	strcpy(psrv->ps_name, name);
	/*
	 * Grab the allprison_lock here, so we won't miss any jail
	 * creation/destruction.
	 */
	sx_xlock(&allprison_lock);
#ifdef INVARIANTS
	/*
	 * Verify if service is not already registered.
	 */
	TAILQ_FOREACH(psrv2, &prison_services, ps_next) {
		KASSERT(strcmp(psrv2->ps_name, name) != 0,
		    ("jail service %s already registered", name));
	}
#endif
	/*
	 * Find free slot. When there is no existing free slot available,
	 * allocate one at the end.
	 */
	TAILQ_FOREACH(psrv2, &prison_services, ps_next) {
		if (psrv2->ps_slotno != slotno) {
			KASSERT(slotno < psrv2->ps_slotno,
			    ("Invalid slotno (slotno=%d >= ps_slotno=%d",
			    slotno, psrv2->ps_slotno));
			/* We found free slot. */
			reallocate = 0;
			break;
		}
		slotno++;
	}
	psrv->ps_slotno = slotno;
	/*
	 * Keep the list sorted by slot number.
	 */
	if (psrv2 != NULL) {
		KASSERT(reallocate == 0, ("psrv2 != NULL && reallocate != 0"));
		TAILQ_INSERT_BEFORE(psrv2, psrv, ps_next);
	} else {
		KASSERT(reallocate == 1, ("psrv2 == NULL && reallocate == 0"));
		TAILQ_INSERT_TAIL(&prison_services, psrv, ps_next);
	}
	prison_service_slots++;
	sx_downgrade(&allprison_lock);
	/*
	 * Allocate memory for new slot if we didn't found empty one.
	 * Do not use realloc(9), because pr_slots is protected with a mutex,
	 * so we can't sleep.
	 */
	LIST_FOREACH(pr, &allprison, pr_list) {
		if (reallocate) {
			/* First allocate memory with M_WAITOK. */
			slots = malloc(sizeof(*slots) * prison_service_slots,
			    M_PRISON, M_WAITOK);
			/* Now grab the mutex and replace pr_slots. */
			mtx_lock(&pr->pr_mtx);
			oldslots = pr->pr_slots;
			if (psrv->ps_slotno > 0) {
				bcopy(oldslots, slots,
				    sizeof(*slots) * (prison_service_slots - 1));
			}
			slots[psrv->ps_slotno] = NULL;
			pr->pr_slots = slots;
			mtx_unlock(&pr->pr_mtx);
			if (oldslots != NULL)
				free(oldslots, M_PRISON);
		}
		/*
		 * Call 'create' method for each existing jail.
		 */
		psrv->ps_create(psrv, pr);
	}
	sx_sunlock(&allprison_lock);

	return (psrv);
}

void
prison_service_deregister(struct prison_service *psrv)
{
	struct prison *pr;
	void **slots, **oldslots;
	int last = 0;

	sx_xlock(&allprison_lock);
	if (TAILQ_LAST(&prison_services, prison_services_head) == psrv)
		last = 1;
	TAILQ_REMOVE(&prison_services, psrv, ps_next);
	prison_service_slots--;
	sx_downgrade(&allprison_lock);
	LIST_FOREACH(pr, &allprison, pr_list) {
		/*
		 * Call 'destroy' method for every currently existing jail.
		 */
		psrv->ps_destroy(psrv, pr);
		/*
		 * If this is the last slot, free the memory allocated for it.
		 */
		if (last) {
			if (prison_service_slots == 0)
				slots = NULL;
			else {
				slots = malloc(sizeof(*slots) * prison_service_slots,
				    M_PRISON, M_WAITOK);
			}
			mtx_lock(&pr->pr_mtx);
			oldslots = pr->pr_slots;
			/*
			 * We require setting slot to NULL after freeing it,
			 * this way we can check for memory leaks here.
			 */
			KASSERT(oldslots[psrv->ps_slotno] == NULL,
			    ("Slot %d (service %s, jailid=%d) still contains data?",
			     psrv->ps_slotno, psrv->ps_name, pr->pr_id));
			if (psrv->ps_slotno > 0) {
				bcopy(oldslots, slots,
				    sizeof(*slots) * prison_service_slots);
			}
			pr->pr_slots = slots;
			mtx_unlock(&pr->pr_mtx);
			KASSERT(oldslots != NULL, ("oldslots == NULL"));
			free(oldslots, M_PRISON);
		}
	}
	sx_sunlock(&allprison_lock);
	free(psrv, M_PRISON);
}

/*
 * Function sets data for the given jail in slot assigned for the given
 * jail service.
 */
void
prison_service_data_set(struct prison_service *psrv, struct prison *pr,
    void *data)
{

	mtx_assert(&pr->pr_mtx, MA_OWNED);
	pr->pr_slots[psrv->ps_slotno] = data;
}

/*
 * Function clears slots assigned for the given jail service in the given
 * prison structure and returns current slot data.
 */
void *
prison_service_data_del(struct prison_service *psrv, struct prison *pr)
{
	void *data;

	mtx_assert(&pr->pr_mtx, MA_OWNED);
	data = pr->pr_slots[psrv->ps_slotno];
	pr->pr_slots[psrv->ps_slotno] = NULL;
	return (data);
}

/*
 * Function returns current data from the slot assigned to the given jail
 * service for the given jail.
 */
void *
prison_service_data_get(struct prison_service *psrv, struct prison *pr)
{

	mtx_assert(&pr->pr_mtx, MA_OWNED);
	return (pr->pr_slots[psrv->ps_slotno]);
}

static int
sysctl_jail_list(SYSCTL_HANDLER_ARGS)
{
	struct xprison *xp, *sxp;
	struct prison *pr;
	char *p;
	size_t len;
	int count, error;

	if (jailed(req->td->td_ucred))
		return (0);

	sx_slock(&allprison_lock);
	if ((count = prisoncount) == 0) {
		sx_sunlock(&allprison_lock);
		return (0);
	}

	len = sizeof(*xp) * count;
	LIST_FOREACH(pr, &allprison, pr_list) {
#ifdef INET
		len += pr->pr_ip4s * sizeof(struct in_addr);
#endif
#ifdef INET6
		len += pr->pr_ip6s * sizeof(struct in6_addr);
#endif
	}

	sxp = xp = malloc(len, M_TEMP, M_WAITOK | M_ZERO);

	LIST_FOREACH(pr, &allprison, pr_list) {
		xp->pr_version = XPRISON_VERSION;
		xp->pr_id = pr->pr_id;
		xp->pr_state = pr->pr_state;
		xp->pr_cpusetid = pr->pr_cpuset->cs_id;
		strlcpy(xp->pr_path, pr->pr_path, sizeof(xp->pr_path));
		mtx_lock(&pr->pr_mtx);
		strlcpy(xp->pr_host, pr->pr_host, sizeof(xp->pr_host));
		strlcpy(xp->pr_name, pr->pr_name, sizeof(xp->pr_name));
		mtx_unlock(&pr->pr_mtx);
#ifdef INET
		xp->pr_ip4s = pr->pr_ip4s;
#endif
#ifdef INET6
		xp->pr_ip6s = pr->pr_ip6s;
#endif
		p = (char *)(xp + 1);
#ifdef INET
		if (pr->pr_ip4s > 0) {
			bcopy(pr->pr_ip4, (struct in_addr *)p,
			    pr->pr_ip4s * sizeof(struct in_addr));
			p += (pr->pr_ip4s * sizeof(struct in_addr));
		}
#endif
#ifdef INET6
		if (pr->pr_ip6s > 0) {
			bcopy(pr->pr_ip6, (struct in6_addr *)p,
			    pr->pr_ip6s * sizeof(struct in6_addr));
			p += (pr->pr_ip6s * sizeof(struct in6_addr));
		}
#endif
		xp = (struct xprison *)p;
	}
	sx_sunlock(&allprison_lock);

	error = SYSCTL_OUT(req, sxp, len);
	free(sxp, M_TEMP);
	return (error);
}

SYSCTL_OID(_security_jail, OID_AUTO, list,
    CTLTYPE_STRUCT | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_jail_list, "S", "List of active jails");

static int
sysctl_jail_jailed(SYSCTL_HANDLER_ARGS)
{
	int error, injail;

	injail = jailed(req->td->td_ucred);
	error = SYSCTL_OUT(req, &injail, sizeof(injail));

	return (error);
}
SYSCTL_PROC(_security_jail, OID_AUTO, jailed,
    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_jail_jailed, "I", "Process in jail?");

#ifdef DDB
DB_SHOW_COMMAND(jails, db_show_jails)
{
	struct prison *pr;
#ifdef INET
	struct in_addr ia;
#endif
#ifdef INET6
	char ip6buf[INET6_ADDRSTRLEN];
#endif
	const char *state;
#if defined(INET) || defined(INET6)
	int i;
#endif

	db_printf(
	    "   JID  pr_ref  pr_nprocs  pr_ip4s  pr_ip6s\n");
	db_printf(
	    "        Hostname                      Path\n");
	db_printf(
	    "        Name                          State\n");
	db_printf(
	    "        Cpusetid\n");
	db_printf(
	    "        IP Address(es)\n");
	LIST_FOREACH(pr, &allprison, pr_list) {
		db_printf("%6d  %6d  %9d  %7d  %7d\n",
		    pr->pr_id, pr->pr_ref, pr->pr_nprocs,
		    pr->pr_ip4s, pr->pr_ip6s);
		db_printf("%6s  %-29.29s %.74s\n",
		    "", pr->pr_host, pr->pr_path);
		if (pr->pr_state < 0 || pr->pr_state >= (int)((sizeof(
		    prison_states) / sizeof(struct prison_state))))
			state = "(bogus)";
		else
			state = prison_states[pr->pr_state].state_name;
		db_printf("%6s  %-29.29s %.74s\n",
		    "", (pr->pr_name[0] != '\0') ? pr->pr_name : "", state);
		db_printf("%6s  %-6d\n",
		    "", pr->pr_cpuset->cs_id);
#ifdef INET
		for (i=0; i < pr->pr_ip4s; i++) {
			ia.s_addr = pr->pr_ip4[i].s_addr;
			db_printf("%6s  %s\n", "", inet_ntoa(ia));
		}
#endif
#ifdef INET6
		for (i=0; i < pr->pr_ip6s; i++)
			db_printf("%6s  %s\n",
			    "", ip6_sprintf(ip6buf, &pr->pr_ip6[i]));
#endif /* INET6 */
		if (db_pager_quit)
			break;
	}
}
#endif /* DDB */
