/*	$NetBSD: clnt_generic.c,v 1.18 2000/07/06 03:10:34 christos Exp $	*/

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

/* #ident	"@(#)clnt_generic.c	1.20	94/05/03 SMI" */

#if defined(LIBC_SCCS) && !defined(lint)
/*static char *sccsid = "from: @(#)clnt_generic.c 1.4 87/08/11 (C) 1987 SMI";*/
/*static char *sccsid = "from: @(#)clnt_generic.c	2.2 88/08/01 4.0 RPCSRC";*/
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Copyright (c) 1986-1991 by Sun Microsystems Inc. 
 */
#include "namespace.h"
#include "reentrant.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <rpc/rpc.h>
#include <rpc/nettype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "un-namespace.h"
#include "rpc_com.h"

/*
 * Generic client creation with version checking the value of
 * vers_out is set to the highest server supported value
 * vers_low <= vers_out <= vers_high  AND an error results
 * if this can not be done.
 */
CLIENT *
clnt_create_vers(hostname, prog, vers_out, vers_low, vers_high, nettype)
	const char *hostname;
	rpcprog_t prog;
	rpcvers_t *vers_out;
	rpcvers_t vers_low;
	rpcvers_t vers_high;
	const char *nettype;
{
	CLIENT *clnt;
	struct timeval to;
	enum clnt_stat rpc_stat;
	struct rpc_err rpcerr;

	clnt = clnt_create(hostname, prog, vers_high, nettype);
	if (clnt == NULL) {
		return (NULL);
	}
	to.tv_sec = 10;
	to.tv_usec = 0;
	rpc_stat = clnt_call(clnt, NULLPROC, (xdrproc_t) xdr_void,
			(char *) NULL, (xdrproc_t) xdr_void, (char *) NULL, to);
	if (rpc_stat == RPC_SUCCESS) {
		*vers_out = vers_high;
		return (clnt);
	}
	if (rpc_stat == RPC_PROGVERSMISMATCH) {
		unsigned long minvers, maxvers;

		clnt_geterr(clnt, &rpcerr);
		minvers = rpcerr.re_vers.low;
		maxvers = rpcerr.re_vers.high;
		if (maxvers < vers_high)
			vers_high = (rpcvers_t)maxvers;
		if (minvers > vers_low)
			vers_low = (rpcvers_t)minvers;
		if (vers_low > vers_high) {
			goto error;
		}
		CLNT_CONTROL(clnt, CLSET_VERS, (char *)(void *)&vers_high);
		rpc_stat = clnt_call(clnt, NULLPROC, (xdrproc_t) xdr_void,
				(char *) NULL, (xdrproc_t) xdr_void,
				(char *) NULL, to);
		if (rpc_stat == RPC_SUCCESS) {
			*vers_out = vers_high;
			return (clnt);
		}
	}
	clnt_geterr(clnt, &rpcerr);

error:
	rpc_createerr.cf_stat = rpc_stat;
	rpc_createerr.cf_error = rpcerr;
	clnt_destroy(clnt);
	return (NULL);
}

/*
 * Top level client creation routine.
 * Generic client creation: takes (servers name, program-number, nettype) and
 * returns client handle. Default options are set, which the user can
 * change using the rpc equivalent of _ioctl()'s.
 *
 * It tries for all the netids in that particular class of netid until
 * it succeeds.
 * XXX The error message in the case of failure will be the one
 * pertaining to the last create error.
 *
 * It calls clnt_tp_create();
 */
CLIENT *
clnt_create(hostname, prog, vers, nettype)
	const char *hostname;				/* server name */
	rpcprog_t prog;				/* program number */
	rpcvers_t vers;				/* version number */
	const char *nettype;				/* net type */
{
	struct netconfig *nconf;
	CLIENT *clnt = NULL;
	void *handle;
	enum clnt_stat	save_cf_stat = RPC_SUCCESS;
	struct rpc_err	save_cf_error;


	if ((handle = __rpc_setconf(nettype)) == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
		return (NULL);
	}
	rpc_createerr.cf_stat = RPC_SUCCESS;
	while (clnt == NULL) {
		if ((nconf = __rpc_getconf(handle)) == NULL) {
			if (rpc_createerr.cf_stat == RPC_SUCCESS)
				rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
			break;
		}
#ifdef CLNT_DEBUG
		printf("trying netid %s\n", nconf->nc_netid);
#endif
		clnt = clnt_tp_create(hostname, prog, vers, nconf);
		if (clnt)
			break;
		else
			/*
			 *	Since we didn't get a name-to-address
			 *	translation failure here, we remember
			 *	this particular error.  The object of
			 *	this is to enable us to return to the
			 *	caller a more-specific error than the
			 *	unhelpful ``Name to address translation
			 *	failed'' which might well occur if we
			 *	merely returned the last error (because
			 *	the local loopbacks are typically the
			 *	last ones in /etc/netconfig and the most
			 *	likely to be unable to translate a host
			 *	name).
			 */
			if (rpc_createerr.cf_stat != RPC_N2AXLATEFAILURE) {
				save_cf_stat = rpc_createerr.cf_stat;
				save_cf_error = rpc_createerr.cf_error;
			}
	}

	/*
	 *	Attempt to return an error more specific than ``Name to address
	 *	translation failed''
	 */
	if ((rpc_createerr.cf_stat == RPC_N2AXLATEFAILURE) &&
		(save_cf_stat != RPC_SUCCESS)) {
		rpc_createerr.cf_stat = save_cf_stat;
		rpc_createerr.cf_error = save_cf_error;
	}
	__rpc_endconf(handle);
	return (clnt);
}

/*
 * Generic client creation: takes (servers name, program-number, netconf) and
 * returns client handle. Default options are set, which the user can
 * change using the rpc equivalent of _ioctl()'s : clnt_control()
 * It finds out the server address from rpcbind and calls clnt_tli_create()
 */
CLIENT *
clnt_tp_create(hostname, prog, vers, nconf)
	const char *hostname;			/* server name */
	rpcprog_t prog;				/* program number */
	rpcvers_t vers;				/* version number */
	const struct netconfig *nconf;		/* net config struct */
{
	struct netbuf *svcaddr;			/* servers address */
	CLIENT *cl = NULL;			/* client handle */

	if (nconf == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
		return (NULL);
	}

	/*
	 * Get the address of the server
	 */
	if ((svcaddr = __rpcb_findaddr(prog, vers, nconf, hostname,
		&cl)) == NULL) {
		/* appropriate error number is set by rpcbind libraries */
		return (NULL);
	}
	if (cl == NULL) {
		cl = clnt_tli_create(RPC_ANYFD, nconf, svcaddr,
					prog, vers, 0, 0);
	} else {
		/* Reuse the CLIENT handle and change the appropriate fields */
		if (CLNT_CONTROL(cl, CLSET_SVC_ADDR, (void *)svcaddr) == TRUE) {
			if (cl->cl_netid == NULL)
				cl->cl_netid = strdup(nconf->nc_netid);
			if (cl->cl_tp == NULL)
				cl->cl_tp = strdup(nconf->nc_device);
			(void) CLNT_CONTROL(cl, CLSET_PROG, (void *)&prog);
			(void) CLNT_CONTROL(cl, CLSET_VERS, (void *)&vers);
		} else {
			CLNT_DESTROY(cl);
			cl = clnt_tli_create(RPC_ANYFD, nconf, svcaddr,
					prog, vers, 0, 0);
		}
	}
	free(svcaddr->buf);
	free(svcaddr);
	return (cl);
}

/*
 * Generic client creation:  returns client handle.
 * Default options are set, which the user can
 * change using the rpc equivalent of _ioctl()'s : clnt_control().
 * If fd is RPC_ANYFD, it will be opened using nconf.
 * It will be bound if not so.
 * If sizes are 0; appropriate defaults will be chosen.
 */
CLIENT *
clnt_tli_create(fd, nconf, svcaddr, prog, vers, sendsz, recvsz)
	int fd;				/* fd */
	const struct netconfig *nconf;	/* netconfig structure */
	const struct netbuf *svcaddr;	/* servers address */
	rpcprog_t prog;			/* program number */
	rpcvers_t vers;			/* version number */
	u_int sendsz;			/* send size */
	u_int recvsz;			/* recv size */
{
	CLIENT *cl;			/* client handle */
	bool_t madefd = FALSE;		/* whether fd opened here */
	long servtype;
	int one = 1;
	struct __rpc_sockinfo si;

	if (fd == RPC_ANYFD) {
		if (nconf == NULL) {
			rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
			return (NULL);
		}

		fd = __rpc_nconf2fd(nconf);

		if (fd == -1)
			goto err;

		madefd = TRUE;
		servtype = nconf->nc_semantics;
		if (!__rpc_fd2sockinfo(fd, &si))
			goto err;

		bindresvport(fd, NULL);
	} else {
		if (!__rpc_fd2sockinfo(fd, &si))
			goto err;
		servtype = __rpc_socktype2seman(si.si_socktype);
		if (servtype == -1) {
			rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
			return NULL;
		}

	}

	if (si.si_af != ((struct sockaddr *)svcaddr->buf)->sa_family) {
		rpc_createerr.cf_stat = RPC_UNKNOWNHOST;	/* XXX */
		goto err1;
	}

	switch (servtype) {
	case NC_TPI_COTS_ORD:
		cl = clnt_vc_create(fd, svcaddr, prog, vers, sendsz, recvsz);
		if (!nconf || !cl)
			break;
		/* XXX fvdl - is this useful? */
		if (strncmp(nconf->nc_protofmly, "inet", 4) == 0)
			_setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one,
			    sizeof (one));
		break;
	case NC_TPI_CLTS:
		cl = clnt_dg_create(fd, svcaddr, prog, vers, sendsz, recvsz);
		break;
	default:
		goto err;
	}

	if (cl == NULL)
		goto err1; /* borrow errors from clnt_dg/vc creates */
	if (nconf) {
		cl->cl_netid = strdup(nconf->nc_netid);
		cl->cl_tp = strdup(nconf->nc_device);
	} else {
		cl->cl_netid = "";
		cl->cl_tp = "";
	}
	if (madefd) {
		(void) CLNT_CONTROL(cl, CLSET_FD_CLOSE, NULL);
/*		(void) CLNT_CONTROL(cl, CLSET_POP_TIMOD, (char *) NULL);  */
	};

	return (cl);

err:
	rpc_createerr.cf_stat = RPC_SYSTEMERROR;
	rpc_createerr.cf_error.re_errno = errno;
err1:	if (madefd)
		(void)_close(fd);
	return (NULL);
}
