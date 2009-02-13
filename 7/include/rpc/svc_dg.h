/*	$NetBSD: svc_dg.h,v 1.1 2000/06/02 23:11:16 fvdl Exp $	*/
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
 * XXX - this file exists only so that the rpcbind code can pull it in.
 * This should go away. It should only be include by svc_dg.c and
 * rpcb_svc_com.c in the rpcbind code.
 */

/*
 * kept in xprt->xp_p2
 */
struct svc_dg_data {
	/* XXX: optbuf should be the first field, used by ti_opts.c code */
	size_t		su_iosz;		/* size of send.recv buffer */
	u_int32_t	su_xid;			/* transaction id */
	XDR		su_xdrs;			/* XDR handle */
	char		su_verfbody[MAX_AUTH_BYTES];	/* verifier body */
	void		*su_cache;		/* cached data, NULL if none */
};

#define __rpcb_get_dg_xidp(x)	(&((struct svc_dg_data *)(x)->xp_p2)->su_xid)
