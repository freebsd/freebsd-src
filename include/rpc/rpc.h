/*	$NetBSD: rpc.h,v 1.13 2000/06/02 22:57:56 fvdl Exp $	*/

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
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
 *
 *	from: @(#)rpc.h 1.9 88/02/08 SMI
 *	from: @(#)rpc.h	2.4 89/07/11 4.0 RPCSRC
 * $FreeBSD$
 */

/*
 * rpc.h, Just includes the billions of rpc header files necessary to
 * do remote procedure calling.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */
#ifndef _RPC_RPC_H
#define _RPC_RPC_H

#include <rpc/types.h>		/* some typedefs */
#include <sys/socket.h>
#include <netinet/in.h>

/* external data representation interfaces */
#include <rpc/xdr.h>		/* generic (de)serializer */

/* Client side only authentication */
#include <rpc/auth.h>		/* generic authenticator (client side) */

/* Client side (mostly) remote procedure call */
#include <rpc/clnt.h>		/* generic rpc stuff */

/* semi-private protocol headers */
#include <rpc/rpc_msg.h>	/* protocol for rpc messages */
#include <rpc/auth_unix.h>	/* protocol for unix style cred */
/*
 *  Uncomment-out the next line if you are building the rpc library with
 *  DES Authentication (see the README file in the secure_rpc/ directory).
 */
#include <rpc/auth_des.h>	/* protocol for des style cred */

/* Server side only remote procedure callee */
#include <rpc/svc.h>		/* service manager and multiplexer */
#include <rpc/svc_auth.h>	/* service side authenticator */

/* Portmapper client, server, and protocol headers */
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>

#ifndef _KERNEL
#include <rpc/rpcb_clnt.h>	/* rpcbind interface functions */
#endif

#include <rpc/rpcent.h>

__BEGIN_DECLS
extern int get_myaddress __P((struct sockaddr_in *));
extern int bindresvport __P((int, struct sockaddr_in *));
extern int registerrpc __P((int, int, int, char *(*) __P((char [UDPMSGSIZE])),
    xdrproc_t, xdrproc_t));
extern int callrpc __P((const char *, int, int, int, xdrproc_t, void *,
    xdrproc_t , void *));
extern int getrpcport __P((char *, int, int, int));

char *taddr2uaddr __P((const struct netconfig *, const struct netbuf *));
struct netbuf *uaddr2taddr __P((const struct netconfig *, const char *));

struct sockaddr;
extern int bindresvport_sa __P((int, struct sockaddr *));
__END_DECLS

/*
 * The following are not exported interfaces, they are for internal library
 * and rpcbind use only. Do not use, they may change without notice.
 */
__BEGIN_DECLS
int __rpc_nconf2fd __P((const struct netconfig *));
int __rpc_nconf2sockinfo __P((const struct netconfig *,
			      struct __rpc_sockinfo *));
int __rpc_fd2sockinfo __P((int, struct __rpc_sockinfo *));
u_int __rpc_get_t_size __P((int, int, int));
__END_DECLS

#endif /* !_RPC_RPC_H */
