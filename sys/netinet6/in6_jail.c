/*-
 * Copyright (c) 1999 Poul-Henning Kamp.
 * Copyright (c) 2008 Bjoern A. Zeeb.
 * Copyright (c) 2009 James Gritton.
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/sysproto.h>
#include <sys/malloc.h>
#include <sys/osd.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/taskqueue.h>
#include <sys/fcntl.h>
#include <sys/jail.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/racct.h>
#include <sys/refcount.h>
#include <sys/sx.h>
#include <sys/sysent.h>
#include <sys/namei.h>
#include <sys/mount.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <net/if.h>
#include <net/vnet.h>

#include <netinet/in.h>

static void
prison_bcopy_primary_ip6(const struct prison *pr, struct in6_addr *ia6)
{

	bcopy(prison_ip_get0(pr, PR_INET6), ia6, sizeof(struct in6_addr));
}

int
prison_qcmp_v6(const void *ip1, const void *ip2)
{
	const struct in6_addr *ia6a, *ia6b;
	int i, rc;

	ia6a = (const struct in6_addr *)ip1;
	ia6b = (const struct in6_addr *)ip2;

	rc = 0;
	for (i = 0; rc == 0 && i < sizeof(struct in6_addr); i++) {
		if (ia6a->s6_addr[i] > ia6b->s6_addr[i])
			rc = 1;
		else if (ia6a->s6_addr[i] < ia6b->s6_addr[i])
			rc = -1;
	}
	return (rc);
}

bool
prison_valid_v6(const void *ip)
{
	const struct in6_addr *ia = ip;

	return (!IN6_IS_ADDR_UNSPECIFIED(ia));
}

/*
 * Pass back primary IPv6 address for this jail.
 *
 * If not restricted return success but do not alter the address.  Caller has
 * to make sure to initialize it correctly (e.g. IN6ADDR_ANY_INIT).
 *
 * Returns 0 on success, EAFNOSUPPORT if the jail doesn't allow IPv6.
 */
int
prison_get_ip6(struct ucred *cred, struct in6_addr *ia6)
{
	struct prison *pr;

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia6 != NULL, ("%s: ia6 is NULL", __func__));

	pr = cred->cr_prison;
	if (!(pr->pr_flags & PR_IP6))
		return (0);
	mtx_lock(&pr->pr_mtx);
	if (!(pr->pr_flags & PR_IP6)) {
		mtx_unlock(&pr->pr_mtx);
		return (0);
	}
	if (pr->pr_addrs[PR_INET6] == NULL) {
		mtx_unlock(&pr->pr_mtx);
		return (EAFNOSUPPORT);
	}

	prison_bcopy_primary_ip6(pr, ia6);
	mtx_unlock(&pr->pr_mtx);
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
	struct prison *pr;
	struct in6_addr lia6;
	int error;

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia6 != NULL, ("%s: ia6 is NULL", __func__));

	if (!jailed(cred))
		return (1);

	pr = cred->cr_prison;
	if (pr->pr_flags & PR_IP6_SADDRSEL)
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
 * Return true if pr1 and pr2 have the same IPv6 address restrictions.
 */
int
prison_equal_ip6(struct prison *pr1, struct prison *pr2)
{

	if (pr1 == pr2)
		return (1);

	while (pr1 != &prison0 &&
#ifdef VIMAGE
	       !(pr1->pr_flags & PR_VNET) &&
#endif
	       !(pr1->pr_flags & PR_IP6_USER))
		pr1 = pr1->pr_parent;
	while (pr2 != &prison0 &&
#ifdef VIMAGE
	       !(pr2->pr_flags & PR_VNET) &&
#endif
	       !(pr2->pr_flags & PR_IP6_USER))
		pr2 = pr2->pr_parent;
	return (pr1 == pr2);
}

/*
 * Make sure our (source) address is set to something meaningful to this jail.
 *
 * v6only should be set based on (inp->inp_flags & IN6P_IPV6_V6ONLY != 0)
 * when needed while binding.
 *
 * Returns 0 if jail doesn't restrict IPv6 or if address belongs to jail,
 * EADDRNOTAVAIL if the address doesn't belong, or EAFNOSUPPORT if the jail
 * doesn't allow IPv6.
 */
int
prison_local_ip6(struct ucred *cred, struct in6_addr *ia6, int v6only)
{
	struct prison *pr;
	int error;

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia6 != NULL, ("%s: ia6 is NULL", __func__));

	pr = cred->cr_prison;
	if (!(pr->pr_flags & PR_IP6))
		return (0);
	mtx_lock(&pr->pr_mtx);
	if (!(pr->pr_flags & PR_IP6)) {
		mtx_unlock(&pr->pr_mtx);
		return (0);
	}
	if (pr->pr_addrs[PR_INET6] == NULL) {
		mtx_unlock(&pr->pr_mtx);
		return (EAFNOSUPPORT);
	}

	if (IN6_IS_ADDR_UNSPECIFIED(ia6)) {
		/*
		 * In case there is only 1 IPv6 address, and v6only is true,
		 * then bind directly.
		 */
		if (v6only != 0 && prison_ip_cnt(pr, PR_INET6) == 1)
			prison_bcopy_primary_ip6(pr, ia6);
		mtx_unlock(&pr->pr_mtx);
		return (0);
	}

	error = prison_check_ip6_locked(pr, ia6);
	if (error == EADDRNOTAVAIL && IN6_IS_ADDR_LOOPBACK(ia6)) {
		prison_bcopy_primary_ip6(pr, ia6);
		error = 0;
	}

	mtx_unlock(&pr->pr_mtx);
	return (error);
}

/*
 * Rewrite destination address in case we will connect to loopback address.
 *
 * Returns 0 on success, EAFNOSUPPORT if the jail doesn't allow IPv6.
 */
int
prison_remote_ip6(struct ucred *cred, struct in6_addr *ia6)
{
	struct prison *pr;

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia6 != NULL, ("%s: ia6 is NULL", __func__));

	pr = cred->cr_prison;
	if (!(pr->pr_flags & PR_IP6))
		return (0);
	mtx_lock(&pr->pr_mtx);
	if (!(pr->pr_flags & PR_IP6)) {
		mtx_unlock(&pr->pr_mtx);
		return (0);
	}
	if (pr->pr_addrs[PR_INET6] == NULL) {
		mtx_unlock(&pr->pr_mtx);
		return (EAFNOSUPPORT);
	}

	if (IN6_IS_ADDR_LOOPBACK(ia6) &&
            prison_check_ip6_locked(pr, ia6) == EADDRNOTAVAIL) {
		prison_bcopy_primary_ip6(pr, ia6);
		mtx_unlock(&pr->pr_mtx);
		return (0);
	}

	/*
	 * Return success because nothing had to be changed.
	 */
	mtx_unlock(&pr->pr_mtx);
	return (0);
}

/*
 * Check if given address belongs to the jail referenced by cred/prison.
 *
 * Returns 0 if address belongs to jail,
 * EADDRNOTAVAIL if the address doesn't belong to the jail.
 */
int
prison_check_ip6_locked(const struct prison *pr, const struct in6_addr *ia6)
{

	if (!(pr->pr_flags & PR_IP6))
		return (0);

	return (prison_ip_check(pr, PR_INET6, ia6));
}

int
prison_check_ip6(const struct ucred *cred, const struct in6_addr *ia6)
{
	struct prison *pr;
	int error;

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia6 != NULL, ("%s: ia6 is NULL", __func__));

	pr = cred->cr_prison;
	if (!(pr->pr_flags & PR_IP6))
		return (0);
	mtx_lock(&pr->pr_mtx);
	if (!(pr->pr_flags & PR_IP6)) {
		mtx_unlock(&pr->pr_mtx);
		return (0);
	}
	if (pr->pr_addrs[PR_INET6] == NULL) {
		mtx_unlock(&pr->pr_mtx);
		return (EAFNOSUPPORT);
	}

	error = prison_check_ip6_locked(pr, ia6);
	mtx_unlock(&pr->pr_mtx);
	return (error);
}
