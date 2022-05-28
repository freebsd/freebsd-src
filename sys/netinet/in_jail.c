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

static in_addr_t
prison_primary_ip4(const struct prison *pr)
{

	return (((const struct in_addr *)prison_ip_get0(pr, PR_INET))->s_addr);
}

int
prison_qcmp_v4(const void *ip1, const void *ip2)
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

bool
prison_valid_v4(const void *ip)
{
	in_addr_t ia = ((const struct in_addr *)ip)->s_addr;

	/*
	 * We do not have to care about byte order for these
	 * checks so we will do them in NBO.
	 */
	return (ia != INADDR_ANY && ia != INADDR_BROADCAST);
}

/*
 * Pass back primary IPv4 address of this jail.
 *
 * If not restricted return success but do not alter the address.  Caller has
 * to make sure to initialize it correctly (e.g. INADDR_ANY).
 *
 * Returns 0 on success, EAFNOSUPPORT if the jail doesn't allow IPv4.
 * Address returned in NBO.
 */
int
prison_get_ip4(struct ucred *cred, struct in_addr *ia)
{
	struct prison *pr;

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia != NULL, ("%s: ia is NULL", __func__));

	pr = cred->cr_prison;
	if (!(pr->pr_flags & PR_IP4))
		return (0);
	mtx_lock(&pr->pr_mtx);
	if (!(pr->pr_flags & PR_IP4)) {
		mtx_unlock(&pr->pr_mtx);
		return (0);
	}
	if (pr->pr_addrs[PR_INET] == NULL) {
		mtx_unlock(&pr->pr_mtx);
		return (EAFNOSUPPORT);
	}

	ia->s_addr = prison_primary_ip4(pr);
	mtx_unlock(&pr->pr_mtx);
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
	struct prison *pr;
	struct in_addr lia;
	int error;

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia != NULL, ("%s: ia is NULL", __func__));

	if (!jailed(cred))
		return (1);

	pr = cred->cr_prison;
	if (pr->pr_flags & PR_IP4_SADDRSEL)
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
 * Return true if pr1 and pr2 have the same IPv4 address restrictions.
 */
int
prison_equal_ip4(struct prison *pr1, struct prison *pr2)
{

	if (pr1 == pr2)
		return (1);

	/*
	 * No need to lock since the PR_IP4_USER flag can't be altered for
	 * existing prisons.
	 */
	while (pr1 != &prison0 &&
#ifdef VIMAGE
	       !(pr1->pr_flags & PR_VNET) &&
#endif
	       !(pr1->pr_flags & PR_IP4_USER))
		pr1 = pr1->pr_parent;
	while (pr2 != &prison0 &&
#ifdef VIMAGE
	       !(pr2->pr_flags & PR_VNET) &&
#endif
	       !(pr2->pr_flags & PR_IP4_USER))
		pr2 = pr2->pr_parent;
	return (pr1 == pr2);
}

/*
 * Make sure our (source) address is set to something meaningful to this
 * jail.
 *
 * Returns 0 if jail doesn't restrict IPv4 or if address belongs to jail,
 * EADDRNOTAVAIL if the address doesn't belong, or EAFNOSUPPORT if the jail
 * doesn't allow IPv4.  Address passed in in NBO and returned in NBO.
 */
int
prison_local_ip4(struct ucred *cred, struct in_addr *ia)
{
	struct prison *pr;
	struct in_addr ia0;
	int error;

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia != NULL, ("%s: ia is NULL", __func__));

	pr = cred->cr_prison;
	if (!(pr->pr_flags & PR_IP4))
		return (0);
	mtx_lock(&pr->pr_mtx);
	if (!(pr->pr_flags & PR_IP4)) {
		mtx_unlock(&pr->pr_mtx);
		return (0);
	}
	if (pr->pr_addrs[PR_INET] == NULL) {
		mtx_unlock(&pr->pr_mtx);
		return (EAFNOSUPPORT);
	}

	ia0.s_addr = ntohl(ia->s_addr);

	if (ia0.s_addr == INADDR_ANY) {
		/*
		 * In case there is only 1 IPv4 address, bind directly.
		 */
		if (prison_ip_cnt(pr, PR_INET) == 1)
			ia->s_addr = prison_primary_ip4(pr);
		mtx_unlock(&pr->pr_mtx);
		return (0);
	}

	error = prison_check_ip4_locked(pr, ia);
	if (error == EADDRNOTAVAIL && ia0.s_addr == INADDR_LOOPBACK) {
		ia->s_addr = prison_primary_ip4(pr);
		error = 0;
	}

	mtx_unlock(&pr->pr_mtx);
	return (error);
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
	struct prison *pr;

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia != NULL, ("%s: ia is NULL", __func__));

	pr = cred->cr_prison;
	if (!(pr->pr_flags & PR_IP4))
		return (0);
	mtx_lock(&pr->pr_mtx);
	if (!(pr->pr_flags & PR_IP4)) {
		mtx_unlock(&pr->pr_mtx);
		return (0);
	}
	if (pr->pr_addrs[PR_INET] == NULL) {
		mtx_unlock(&pr->pr_mtx);
		return (EAFNOSUPPORT);
	}

	if (ntohl(ia->s_addr) == INADDR_LOOPBACK &&
	    prison_check_ip4_locked(pr, ia) == EADDRNOTAVAIL) {
		ia->s_addr = prison_primary_ip4(pr);
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
prison_check_ip4_locked(const struct prison *pr, const struct in_addr *ia)
{

	if (!(pr->pr_flags & PR_IP4))
		return (0);

	return (prison_ip_check(pr, PR_INET, ia));
}

int
prison_check_ip4(const struct ucred *cred, const struct in_addr *ia)
{
	struct prison *pr;
	int error;

	KASSERT(cred != NULL, ("%s: cred is NULL", __func__));
	KASSERT(ia != NULL, ("%s: ia is NULL", __func__));

	pr = cred->cr_prison;
	if (!(pr->pr_flags & PR_IP4))
		return (0);
	mtx_lock(&pr->pr_mtx);
	if (!(pr->pr_flags & PR_IP4)) {
		mtx_unlock(&pr->pr_mtx);
		return (0);
	}
	if (pr->pr_addrs[PR_INET] == NULL) {
		mtx_unlock(&pr->pr_mtx);
		return (EAFNOSUPPORT);
	}

	error = prison_check_ip4_locked(pr, ia);
	mtx_unlock(&pr->pr_mtx);
	return (error);
}
