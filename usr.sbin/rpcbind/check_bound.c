/*	$NetBSD: check_bound.c,v 1.2 2000/06/22 08:09:26 fvdl Exp $	*/
/*	$FreeBSD$ */

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */
/*
 * Copyright (c) 1986 - 1991 by Sun Microsystems, Inc.
 */

/* #ident	"@(#)check_bound.c	1.15	93/07/05 SMI" */

#if 0
#ifndef lint
static	char sccsid[] = "@(#)check_bound.c 1.11 89/04/21 Copyr 1989 Sun Micro";
#endif
#endif

/*
 * check_bound.c
 * Checks to see whether the program is still bound to the
 * claimed address and returns the universal merged address
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <rpc/rpc.h>
#include <rpc/svc_dg.h>
#include <stdio.h>
#include <netconfig.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "rpcbind.h"

struct fdlist {
	int fd;
	struct netconfig *nconf;
	struct fdlist *next;
	int check_binding;
};

static struct fdlist *fdhead;	/* Link list of the check fd's */
static struct fdlist *fdtail;
static char *nullstring = "";

static bool_t check_bound(struct fdlist *, char *uaddr);

/*
 * Returns 1 if the given address is bound for the given addr & transport
 * For all error cases, we assume that the address is bound
 * Returns 0 for success.
 */
static bool_t
check_bound(struct fdlist *fdl, char *uaddr)
{
	int fd;
	struct netbuf *na;
	int ans;

	if (fdl->check_binding == FALSE)
		return (TRUE);

	na = uaddr2taddr(fdl->nconf, uaddr);
	if (!na)
		return (TRUE); /* punt, should never happen */

	fd = __rpc_nconf2fd(fdl->nconf);
	if (fd < 0) {
		free(na->buf);
		free(na);
		return (TRUE);
	}

	ans = bind(fd, (struct sockaddr *)na->buf, na->len);

	close(fd);
	free(na->buf);
	free(na);

	return (ans == 0 ? FALSE : TRUE);
}

int
add_bndlist(struct netconfig *nconf, struct netbuf *baddr __unused)
{
	struct fdlist *fdl;
	struct netconfig *newnconf;

	newnconf = getnetconfigent(nconf->nc_netid);
	if (newnconf == NULL)
		return (-1);
	fdl = malloc(sizeof (struct fdlist));
	if (fdl == NULL) {
		freenetconfigent(newnconf);
		syslog(LOG_ERR, "no memory!");
		return (-1);
	}
	fdl->nconf = newnconf;
	fdl->next = NULL;
	if (fdhead == NULL) {
		fdhead = fdl;
		fdtail = fdl;
	} else {
		fdtail->next = fdl;
		fdtail = fdl;
	}
	/* XXX no bound checking for now */
	fdl->check_binding = FALSE;

	return 0;
}

bool_t
is_bound(char *netid, char *uaddr)
{
	struct fdlist *fdl;

	for (fdl = fdhead; fdl; fdl = fdl->next)
		if (strcmp(fdl->nconf->nc_netid, netid) == 0)
			break;
	if (fdl == NULL)
		return (TRUE);
	return (check_bound(fdl, uaddr));
}

/*
 * Returns NULL if there was some system error.
 * Returns "" if the address was not bound, i.e the server crashed.
 * Returns the merged address otherwise.
 */
char *
mergeaddr(SVCXPRT *xprt, char *netid, char *uaddr, char *saddr)
{
	struct fdlist *fdl;
	struct svc_dg_data *dg_data;
	char *c_uaddr, *s_uaddr, *m_uaddr, *allocated_uaddr = NULL;

	for (fdl = fdhead; fdl; fdl = fdl->next)
		if (strcmp(fdl->nconf->nc_netid, netid) == 0)
			break;
	if (fdl == NULL)
		return (NULL);
	if (check_bound(fdl, uaddr) == FALSE)
		/* that server died */
		return (nullstring);
	/*
	 * Try to determine the local address on which the client contacted us,
	 * so we can send a reply from the same address.  If it's unknown, then
	 * try to determine which address the client used, and pick a nearby
	 * local address.
	 *
	 * If saddr is not NULL, the remote client may have included the
	 * address by which it contacted us.  Use that for the "client" uaddr,
	 * otherwise use the info from the SVCXPRT.
	 */
	dg_data = (struct svc_dg_data*)xprt->xp_p2;
	if (dg_data != NULL && dg_data->su_srcaddr.buf != NULL) {
		c_uaddr = taddr2uaddr(fdl->nconf, &dg_data->su_srcaddr);
		allocated_uaddr = c_uaddr;
	}
	else if (saddr != NULL) {
		c_uaddr = saddr;
	} else {
		c_uaddr = taddr2uaddr(fdl->nconf, svc_getrpccaller(xprt));
		allocated_uaddr = c_uaddr;
	}
	if (c_uaddr == NULL) {
		syslog(LOG_ERR, "taddr2uaddr failed for %s",
			fdl->nconf->nc_netid);
		return (NULL);
	}

#ifdef ND_DEBUG
	if (debugging) {
		if (saddr == NULL) {
			fprintf(stderr, "mergeaddr: client uaddr = %s\n",
			    c_uaddr);
		} else {
			fprintf(stderr, "mergeaddr: contact uaddr = %s\n",
			    c_uaddr);
		}
	}
#endif
	s_uaddr = uaddr;
	/*
	 * This is all we should need for IP 4 and 6
	 */
	m_uaddr = addrmerge(svc_getrpccaller(xprt), s_uaddr, c_uaddr, netid);
#ifdef ND_DEBUG
	if (debugging)
		fprintf(stderr, "mergeaddr: uaddr = %s, merged uaddr = %s\n",
				uaddr, m_uaddr);
#endif
	free(allocated_uaddr);
	return (m_uaddr);
}

/*
 * Returns a netconf structure from its internal list.  This
 * structure should not be freed.
 */
struct netconfig *
rpcbind_get_conf(const char *netid)
{
	struct fdlist *fdl;

	for (fdl = fdhead; fdl; fdl = fdl->next)
		if (strcmp(fdl->nconf->nc_netid, netid) == 0)
			break;
	if (fdl == NULL)
		return (NULL);
	return (fdl->nconf);
}
