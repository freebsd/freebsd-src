/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/sysproto.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/jail.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <netinet/in.h>

MALLOC_DEFINE(M_PRISON, "prison", "Prison structures");

SYSCTL_DECL(_security);
SYSCTL_NODE(_security, OID_AUTO, jail, CTLFLAG_RW, 0,
    "Jail rules");

mp_fixme("these variables need a lock")

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

/*
 * MPSAFE
 */
int
jail(td, uap)
	struct thread *td;
	struct jail_args /* {
		struct jail *jail;
	} */ *uap;
{
	struct proc *p = td->td_proc;
	int error;
	struct prison *pr;
	struct jail j;
	struct chroot_args ca;
	struct ucred *newcred = NULL, *oldcred;

	error = copyin(uap->jail, &j, sizeof j);
	if (error)
		return (error);
	if (j.version != 0)
		return (EINVAL);

	MALLOC(pr, struct prison *, sizeof *pr , M_PRISON, M_WAITOK | M_ZERO);
	mtx_init(&pr->pr_mtx, "jail mutex", NULL, MTX_DEF);
	pr->pr_securelevel = securelevel;
	error = copyinstr(j.hostname, &pr->pr_host, sizeof pr->pr_host, 0);
	if (error)
		goto bail;
	ca.path = j.path;
	mtx_lock(&Giant);
	error = chroot(td, &ca);
	mtx_unlock(&Giant);
	if (error)
		goto bail;
	newcred = crget();
	pr->pr_ip = j.ip_number;
	PROC_LOCK(p);
	/* Implicitly fail if already in jail.  */
	error = suser_cred(p->p_ucred, 0);
	if (error)
		goto badcred;
	oldcred = p->p_ucred;
	crcopy(newcred, oldcred);
	p->p_ucred = newcred;
	p->p_ucred->cr_prison = pr;
	pr->pr_ref = 1;
	PROC_UNLOCK(p);
	crfree(oldcred);
	return (0);
badcred:
	PROC_UNLOCK(p);
	crfree(newcred);
bail:
	mtx_destroy(&pr->pr_mtx);
	FREE(pr, M_PRISON);
	return (error);
}

void
prison_free(struct prison *pr)
{

	mtx_lock(&pr->pr_mtx);
	pr->pr_ref--;
	if (pr->pr_ref == 0) {
		mtx_unlock(&pr->pr_mtx);
		mtx_destroy(&pr->pr_mtx);
		if (pr->pr_linux != NULL)
			FREE(pr->pr_linux, M_PRISON);
		FREE(pr, M_PRISON);
		return;
	}
	mtx_unlock(&pr->pr_mtx);
}

void
prison_hold(struct prison *pr)
{

	mtx_lock(&pr->pr_mtx);
	pr->pr_ref++;
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
	struct sockaddr_in *sai = (struct sockaddr_in*) sa;
	int ok;

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
prison_check(cred1, cred2)
	struct ucred *cred1, *cred2;
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
jailed(cred)
	struct ucred *cred;
{

	return (cred->cr_prison != NULL);
}

/*
 * Return the correct hostname for the passed credential.
 */
void
getcredhostname(cred, buf, size)
	struct ucred *cred;
	char *buf;
	size_t size;
{

	if (jailed(cred)) {
		mtx_lock(&cred->cr_prison->pr_mtx);
		strlcpy(buf, cred->cr_prison->pr_host, size);
		mtx_unlock(&cred->cr_prison->pr_mtx);
	}
	else
		strlcpy(buf, hostname, size);
}
