/*	$NetBSD: svc_auth.h,v 1.8 2000/06/02 22:57:57 fvdl Exp $	*/

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
 *	from: @(#)svc_auth.h 1.6 86/07/16 SMI
 *	@(#)svc_auth.h	2.1 88/07/29 4.0 RPCSRC
 * $FreeBSD$
 */

/*
 * svc_auth.h, Service side of rpc authentication.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#ifndef _RPC_SVC_AUTH_H
#define _RPC_SVC_AUTH_H

/*
 * Server side authenticator
 */
__BEGIN_DECLS
extern enum auth_stat _authenticate(struct svc_req *, struct rpc_msg *);
#ifdef _KERNEL
extern int svc_auth_reg(int,
    enum auth_stat (*)(struct svc_req *, struct rpc_msg *),
    int (*)(struct svc_req *, struct ucred **, int *));
#else
extern int svc_auth_reg(int, enum auth_stat (*)(struct svc_req *,
                          struct rpc_msg *));
#endif


extern int svc_getcred(struct svc_req *, struct ucred **, int *);
/*
 * struct svc_req *req;                 -- RPC request
 * struct ucred **crp			-- Kernel cred to modify
 * int *flavorp				-- Return RPC auth flavor
 *
 * Retrieve unix creds corresponding to an RPC request, if
 * possible. The auth flavor (AUTH_NONE or AUTH_UNIX) is returned in
 * *flavorp. If the flavor is AUTH_UNIX the caller's ucred pointer
 * will be modified to point at a ucred structure which reflects the
 * values from the request. The caller should call crfree on this
 * pointer.
 *
 * Return's non-zero if credentials were retrieved from the request,
 * otherwise zero.
 */

__END_DECLS

#endif /* !_RPC_SVC_AUTH_H */
