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
 *	$Id: uipc_domain.c,v 1.19 1998/05/15 20:11:29 wollman Exp $
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socketvar.h>
#include <sys/systm.h>
#include <vm/vm_zone.h>

/*
 * System initialization
 *
 * Note: domain initialization wants to take place on a per domain basis
 * as a result of traversing a linker set.  Most likely, each domain
 * want to call a registration function rather than being handled here
 * in domaininit().  Probably this will look like:
 *
 * SYSINIT(unique, SI_SUB_PROTO_DOMAIN, SI_ORDER_ANY, domain_add, xxx)
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

/*
 * Add a new protocol domain to the list of supported domains
 * Note: you cant unload it again because  a socket may be using it.
 * XXX can't fail at this time.
 */
static int
net_init_domain(struct domain *dp)
{
	register struct protosw *pr;
	int	s;

	s = splnet();
	if (dp->dom_init)
		(*dp->dom_init)();
	for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++){
		if (pr->pr_usrreqs == 0)
			panic("domaininit: %ssw[%d] has no usrreqs!",
			      dp->dom_name, 
			      (int)(pr - dp->dom_protosw));
		if (pr->pr_init)
			(*pr->pr_init)();
	}
	/*
	 * update global informatio about maximums
	 */
	max_hdr = max_linkhdr + max_protohdr;
	max_datalen = MHLEN - max_hdr;
	splx(s);
	return (0);
}

/*
 * Add a new protocol domain to the list of supported domains
 * Note: you cant unload it again because  a socket may be using it.
 * XXX can't fail at this time.
 */
int
net_add_domain(struct domain *dp)
{
	int	s, error;

	s = splnet();
	dp->dom_next = domains;
	domains = dp;
	splx(s);
	error = net_init_domain(dp);
	max_hdr = max_linkhdr + max_protohdr;
	max_datalen = MHLEN - max_hdr;
	return (error);
}

extern struct linker_set domain_set;

/* ARGSUSED*/
static void
domaininit(void *dummy)
{
	register struct domain *dp, **dpp;
	/*
	 * Before we do any setup, make sure to initialize the
	 * zone allocator we get struct sockets from.  The obvious
	 * maximum number of sockets is `maxfiles', but it is possible
	 * to have a socket without an open file (e.g., a connection waiting
	 * to be accept(2)ed).  Rather than think up and define a
	 * better value, we just use nmbclusters, since that's what people
	 * are told to increase first when the network runs out of memory.
	 * Perhaps we should have two pools, one of unlimited size
	 * for use during socreate(), and one ZONE_INTERRUPT pool for
	 * use in sonewconn().
	 */
	socket_zone = zinit("socket", sizeof(struct socket), maxsockets,
			    ZONE_INTERRUPT, 0);

	if (max_linkhdr < 16)		/* XXX */
		max_linkhdr = 16;

	/*
	 * NB - local domain is always present.
	 */
	net_add_domain(&localdomain);

	/* 
	 * gather up as many protocols as we have statically linked.
	 * XXX we need to do this because when we ask the routing
	 * protocol to initialise it will want to examine all 
	 * installed protocols. This needs fixing before protocols
	 * that use the standard routing can become modules.
	 */
	for (dpp = (struct domain **)domain_set.ls_items; *dpp; dpp++) {
		(**dpp).dom_next = domains;
		domains = *dpp;
	}

	/*
	 * Now ask them all to init (XXX including the routing domain,
	 * see above)
	 */
	for (dp = domains; dp; dp = dp->dom_next)
		net_init_domain(dp);

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

	splx(*savesplp);
}



struct protosw *
pffindtype(family, type)
	int family;
	int type;
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
pffindproto(family, protocol, type)
	int family;
	int protocol;
	int type;
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
