/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)uipc_domain.c	8.2 (Berkeley) 10/18/93
 * $Id: uipc_domain.c,v 1.12 1995/12/16 02:13:50 bde Exp $
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/systm.h>

/*
 * System initialization
 *
 * Note: domain initialization wants to take place on a per domain basis
 * as a result of traversing a linker set.  Most likely, each domain
 * want to call a registration function rather than being handled here
 * in domaininit().  Probably this will look like:
 *
 * SYSINIT(unique, SI_SUB_PROTO_DOMAI, SI_ORDER_ANY, domain_add, xxx)
 *
 * Where 'xxx' is replaced by the address of a parameter struct to be
 * passed to the doamin_add() function.
 */

static int	x_save_spl;			/* used by kludge*/
static void kludge_splimp __P((void *));
static void kludge_splx __P((void *));
static void domaininit __P((void *));
SYSINIT(splimp, SI_SUB_PROTO_BEGIN, SI_ORDER_FIRST, kludge_splimp, &x_save_spl)
SYSINIT(domain, SI_SUB_PROTO_DOMAIN, SI_ORDER_FIRST, domaininit, NULL)
SYSINIT(splx, SI_SUB_PROTO_END, SI_ORDER_FIRST, kludge_splx, &x_save_spl)

static void	pffasttimo __P((void *));
static void	pfslowtimo __P((void *));

struct domain *domains;

#define	ADDDOMAIN(x)	{ \
	__CONCAT(x,domain.dom_next) = domains; \
	domains = &__CONCAT(x,domain); \
}

extern struct linker_set domain_set;

/* ARGSUSED*/
static void
domaininit(dummy)
	void *dummy;
{
	register struct domain *dp, **dpp;
	register struct protosw *pr;

	/*
	 * NB - local domain is always present.
	 */
	ADDDOMAIN(local);

	for (dpp = (struct domain **)domain_set.ls_items; *dpp; dpp++) {
		(**dpp).dom_next = domains;
		domains = *dpp;
	}

/* - not in our sources
#ifdef ISDN
	ADDDOMAIN(isdn);
#endif
*/

	for (dp = domains; dp; dp = dp->dom_next) {
		if (dp->dom_init)
			(*dp->dom_init)();
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++){
#ifdef PRU_OLDSTYLE
			/* See comments in uipc_socket2.c. */
			if (pr->pr_usrreqs == 0)
				pr->pr_usrreqs = &pru_oldstyle;
			else if(pr->pr_usrreq == 0)
				pr->pr_usrreq = pr_newstyle_usrreq;
#endif
			if (pr->pr_init)
				(*pr->pr_init)();
		}
	}

	if (max_linkhdr < 16)		/* XXX */
		max_linkhdr = 16;
	max_hdr = max_linkhdr + max_protohdr;
	max_datalen = MHLEN - max_hdr;
	timeout(pffasttimo, (void *)0, 1);
	timeout(pfslowtimo, (void *)0, 1);
}


/*
 * The following two operations are kludge code.  Most likely, they should
 * be done as a "domainpreinit()" for the first function and then rolled
 * in as the last act of "domaininit()" for the second.
 *
 * In point of fact, it is questionable why other initialization prior
 * to this does not also take place at splimp by default.
 */
static void
kludge_splimp(udata)
	void *udata;
{
	int	*savesplp = udata;

	*savesplp = splimp();
}

static void
kludge_splx(udata)
	void *udata;
{
	int	*savesplp = udata;

	splx( *savesplp);
}



struct protosw *
pffindtype(int family, int type)
{
	register struct domain *dp;
	register struct protosw *pr;

	for (dp = domains; dp; dp = dp->dom_next)
		if (dp->dom_family == family)
			goto found;
	return (0);
found:
	for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
		if (pr->pr_type && pr->pr_type == type)
			return (pr);
	return (0);
}

struct protosw *
pffindproto(int family, int protocol, int type)
{
	register struct domain *dp;
	register struct protosw *pr;
	struct protosw *maybe = 0;

	if (family == 0)
		return (0);
	for (dp = domains; dp; dp = dp->dom_next)
		if (dp->dom_family == family)
			goto found;
	return (0);
found:
	for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++) {
		if ((pr->pr_protocol == protocol) && (pr->pr_type == type))
			return (pr);

		if (type == SOCK_RAW && pr->pr_type == SOCK_RAW &&
		    pr->pr_protocol == 0 && maybe == (struct protosw *)0)
			maybe = pr;
	}
	return (maybe);
}

void
pfctlinput(cmd, sa)
	int cmd;
	struct sockaddr *sa;
{
	register struct domain *dp;
	register struct protosw *pr;

	for (dp = domains; dp; dp = dp->dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_ctlinput)
				(*pr->pr_ctlinput)(cmd, sa, (void *)0);
}

static void
pfslowtimo(arg)
	void *arg;
{
	register struct domain *dp;
	register struct protosw *pr;

	for (dp = domains; dp; dp = dp->dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_slowtimo)
				(*pr->pr_slowtimo)();
	timeout(pfslowtimo, (void *)0, hz/2);
}

static void
pffasttimo(arg)
	void *arg;
{
	register struct domain *dp;
	register struct protosw *pr;

	for (dp = domains; dp; dp = dp->dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_fasttimo)
				(*pr->pr_fasttimo)();
	timeout(pffasttimo, (void *)0, hz/5);
}
