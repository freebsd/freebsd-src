/*	$NetBSD: rpc_com.h,v 1.3 2000/12/10 04:10:08 christos Exp $	*/

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
 *
 * $FreeBSD: src/sys/rpc/rpc_com.h,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 */
/*
 * Copyright (c) 1986 - 1991 by Sun Microsystems, Inc.
 */

/*
 * rpc_com.h, Common definitions for both the server and client side.
 * All for the topmost layer of rpc
 *
 * In Sun's tirpc distribution, this was installed as <rpc/rpc_com.h>,
 * but as it contains only non-exported interfaces, it was moved here.
 */

#ifndef _RPC_RPCCOM_H
#define	_RPC_RPCCOM_H

#include <sys/cdefs.h>

/* #pragma ident	"@(#)rpc_com.h	1.11	93/07/05 SMI" */

/*
 * The max size of the transport, if the size cannot be determined
 * by other means.
 */
#define	RPC_MAXDATASIZE 9000
#define	RPC_MAXADDRSIZE 1024

#ifdef _KERNEL

#define __RPC_GETXID(now) ((uint32_t)curproc->p_pid ^ (uint32_t)(now)->tv_sec ^ \
    (uint32_t)(now)->tv_usec)

#else

#define __RPC_GETXID(now) ((uint32_t)getpid() ^ (uint32_t)(now)->tv_sec ^ \
    (uint32_t)(now)->tv_usec)

#endif

__BEGIN_DECLS
#ifndef _KERNEL
extern u_int __rpc_get_a_size(int);
extern int __rpc_dtbsize(void);
extern struct netconfig * __rpcgettp(int);
extern  int  __rpc_get_default_domain(char **);

char *__rpc_taddr2uaddr_af(int, const struct netbuf *);
struct netbuf *__rpc_uaddr2taddr_af(int, const char *);
int __rpc_fixup_addr(struct netbuf *, const struct netbuf *);
int __rpc_sockinfo2netid(struct __rpc_sockinfo *, const char **);
int __rpc_seman2socktype(int);
int __rpc_socktype2seman(int);
void *rpc_nullproc(CLIENT *);
int __rpc_sockisbound(int);

struct netbuf *__rpcb_findaddr_timed(rpcprog_t, rpcvers_t,
    const struct netconfig *, const char *host, CLIENT **clpp,
    struct timeval *tp);

bool_t __rpc_control(int,void *);

char *_get_next_token(char *, int);

bool_t __svc_clean_idle(fd_set *, int, bool_t);
bool_t __xdrrec_setnonblock(XDR *, int);
bool_t __xdrrec_getrec(XDR *, enum xprt_stat *, bool_t);
void __xprt_unregister_unlocked(SVCXPRT *);

SVCXPRT **__svc_xports;
int __svc_maxrec;

#else

#define SUN_LEN(su) \
	(sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))

extern u_int __rpc_get_a_size(int);
extern char *__rpc_taddr2uaddr_af(int, const struct netbuf *);
extern struct netbuf *__rpc_uaddr2taddr_af(int, const char *);
extern int __rpc_seman2socktype(int);
extern int __rpc_socktype2seman(int);
extern int __rpc_sockisbound(struct socket*);
extern const char *__rpc_inet_ntop(int af, const void * __restrict src,
    char * __restrict dst, socklen_t size);
extern int __rpc_inet_pton(int af, const char * __restrict src,
    void * __restrict dst);

struct xucred;
struct __rpc_xdr;
bool_t xdr_authunix_parms(struct __rpc_xdr *xdrs, uint32_t *time, struct xucred *cred);
#endif

__END_DECLS

#endif /* _RPC_RPCCOM_H */
